/*
 * Copyright (c) 2009-2021, Google LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Google LLC nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Google LLC BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "upb/util/compare.h"

#include <setjmp.h>
#include <stdbool.h>

#include "upb/reflection.h"
#include "upb/map.h"

// Must be last.
#include "upb/port_def.inc"

struct upb_UnknownFields;
typedef struct upb_UnknownFields upb_UnknownFields;

typedef struct {
  uint32_t tag;
  union {
    uint64_t varint;
    uint64_t uint64;
    uint32_t uint32;
    upb_StringView delimited;
    upb_UnknownFields* group;
  } data;
} upb_UnknownField;

struct upb_UnknownFields {
  size_t size;
  size_t capacity;
  upb_UnknownField* fields;
};

typedef struct {
  const char* end;
  upb_Arena* arena;
  upb_UnknownField* tmp;
  size_t tmp_size;
  int depth;
  jmp_buf err;
} upb_UnknownField_Context;

static void upb_UnknownFields_Grow(upb_UnknownField_Context* ctx,
                                   upb_UnknownField** base,
                                   upb_UnknownField** ptr,
                                   upb_UnknownField** end) {
  size_t old = (*ptr - *base);
  size_t new = UPB_MAX(4, old * 2);

  *base = upb_Arena_Realloc(ctx->arena, *base, old * sizeof(**base),
                            new * sizeof(**base));
  if (!*base) UPB_LONGJMP(ctx->err, kUpb_UnknownCompareResult_OutOfMemory);

  *ptr = *base + old;
  *end = *base + new;
}

static const char* upb_UnknownFields_ParseVarint(const char* ptr,
                                                 const char* limit,
                                                 uint64_t* val) {
  uint8_t byte;
  int bitpos = 0;
  *val = 0;

  do {
    // Unknown field data must be valid.
    UPB_ASSERT(bitpos < 70 && ptr < limit);
    byte = *ptr;
    *val |= (uint64_t)(byte & 0x7F) << bitpos;
    ptr++;
    bitpos += 7;
  } while (byte & 0x80);

  return ptr;
}

// We have to implement our own sort here, since qsort() is not an in-order
// sort. Here we use merge sort, the simplest in-order sort.
static void upb_UnknownFields_Merge(upb_UnknownField* arr, size_t start,
                                    size_t mid, size_t end,
                                    upb_UnknownField* tmp) {
  memcpy(tmp, &arr[start], (end - start) * sizeof(*tmp));

  upb_UnknownField* ptr1 = tmp;
  upb_UnknownField* end1 = &tmp[mid - start];
  upb_UnknownField* ptr2 = &tmp[mid - start];
  upb_UnknownField* end2 = &tmp[end - start];
  upb_UnknownField* out = &arr[start];

  while (ptr1 < end1 && ptr2 < end2) {
    if (ptr1->tag <= ptr2->tag) {
      *out++ = *ptr1++;
    } else {
      *out++ = *ptr2++;
    }
  }

  if (ptr1 < end1) {
    memcpy(out, ptr1, (end1 - ptr1) * sizeof(*out));
  } else if (ptr2 < end2) {
    memcpy(out, ptr1, (end2 - ptr2) * sizeof(*out));
  }
}

static void upb_UnknownFields_SortRecursive(upb_UnknownField* arr, size_t start,
                                            size_t end, upb_UnknownField* tmp) {
  if (end - start > 1) {
    size_t mid = start + ((end - start) / 2);
    upb_UnknownFields_SortRecursive(arr, start, mid, tmp);
    upb_UnknownFields_SortRecursive(arr, mid, end, tmp);
    upb_UnknownFields_Merge(arr, start, mid, end, tmp);
  }
}

static void upb_UnknownFields_Sort(upb_UnknownField_Context* ctx,
                                   upb_UnknownFields* fields) {
  if (ctx->tmp_size < fields->size) {
    ctx->tmp_size = UPB_MAX(8, ctx->tmp_size);
    while (ctx->tmp_size < fields->size) ctx->tmp_size *= 2;
    ctx->tmp = realloc(ctx->tmp, ctx->tmp_size * sizeof(*ctx->tmp));
  }
  upb_UnknownFields_SortRecursive(fields->fields, 0, fields->size, ctx->tmp);
}

static upb_UnknownFields* upb_UnknownFields_DoBuild(
    upb_UnknownField_Context* ctx, const char** buf) {
  upb_UnknownField* arr_base = NULL;
  upb_UnknownField* arr_ptr = NULL;
  upb_UnknownField* arr_end = NULL;
  const char* ptr = *buf;
  uint32_t last_tag = 0;
  bool sorted = true;
  while (ptr < ctx->end) {
    uint64_t tag;
    ptr = upb_UnknownFields_ParseVarint(ptr, ctx->end, &tag);
    UPB_ASSERT(tag <= UINT32_MAX);
    int wire_type = tag & 7;
    if (wire_type == kUpb_WireType_EndGroup) break;
    if (tag < last_tag) sorted = false;
    last_tag = tag;

    if (arr_ptr == arr_end) {
      upb_UnknownFields_Grow(ctx, &arr_base, &arr_ptr, &arr_end);
    }
    upb_UnknownField* field = arr_ptr;
    field->tag = tag;
    arr_ptr++;

    switch (wire_type) {
      case kUpb_WireType_Varint:
        ptr = upb_UnknownFields_ParseVarint(ptr, ctx->end, &field->data.varint);
        break;
      case kUpb_WireType_64Bit:
        UPB_ASSERT(ctx->end - ptr >= 8);
        memcpy(&field->data.uint64, ptr, 8);
        ptr += 8;
        break;
      case kUpb_WireType_32Bit:
        UPB_ASSERT(ctx->end - ptr >= 4);
        memcpy(&field->data.uint32, ptr, 4);
        ptr += 4;
        break;
      case kUpb_WireType_Delimited: {
        uint64_t size;
        ptr = upb_UnknownFields_ParseVarint(ptr, ctx->end, &size);
        UPB_ASSERT(ctx->end - ptr >= size);
        field->data.delimited.data = ptr;
        field->data.delimited.size = size;
        ptr += size;
        break;
      }
      case kUpb_WireType_StartGroup:
        if (--ctx->depth == 0) {
          UPB_LONGJMP(ctx->err, kUpb_UnknownCompareResult_MaxDepthExceeded);
        }
        field->data.group = upb_UnknownFields_DoBuild(ctx, &ptr);
        ctx->depth++;
        break;
      default:
        UPB_UNREACHABLE();
    }
  }

  *buf = ptr;
  upb_UnknownFields* ret = upb_Arena_Malloc(ctx->arena, sizeof(*ret));
  if (!ret) UPB_LONGJMP(ctx->err, kUpb_UnknownCompareResult_OutOfMemory);
  ret->fields = arr_base;
  ret->size = arr_ptr - arr_base;
  ret->capacity = arr_end - arr_base;
  if (!sorted) {
    upb_UnknownFields_Sort(ctx, ret);
  }
  return ret;
}

// Builds a upb_UnknownFields data structure from the binary data in buf.
static upb_UnknownFields* upb_UnknownFields_Build(upb_UnknownField_Context* ctx,
                                                  const char* buf,
                                                  size_t size) {
  ctx->end = buf + size;
  upb_UnknownFields* fields = upb_UnknownFields_DoBuild(ctx, &buf);
  UPB_ASSERT(buf == ctx->end);
  return fields;
}

// Compares two sorted upb_UnknwonFields structures for equality.
static bool upb_UnknownFields_IsEqual(const upb_UnknownFields* uf1,
                                      const upb_UnknownFields* uf2) {
  if (uf1->size != uf2->size) return false;
  for (size_t i = 0, n = uf1->size; i < n; i++) {
    upb_UnknownField* f1 = &uf1->fields[i];
    upb_UnknownField* f2 = &uf2->fields[i];
    if (f1->tag != f2->tag) return false;
    int wire_type = f1->tag & 7;
    switch (wire_type) {
      case kUpb_WireType_Varint:
        if (f1->data.varint != f2->data.varint) return false;
        break;
      case kUpb_WireType_64Bit:
        if (f1->data.uint64 != f2->data.uint64) return false;
        break;
      case kUpb_WireType_32Bit:
        if (f1->data.uint32 != f2->data.uint32) return false;
        break;
      case kUpb_WireType_Delimited:
        if (!upb_StringView_IsEqual(f1->data.delimited, f2->data.delimited)) {
          return false;
        }
        break;
      case kUpb_WireType_StartGroup:
        if (!upb_UnknownFields_IsEqual(f1->data.group, f2->data.group)) {
          return false;
        }
        break;
      default:
        UPB_UNREACHABLE();
    }
  }
  return true;
}

upb_UnknownCompareResult upb_Message_UnknownFieldsAreEqual(const char* buf1,
                                                           size_t size1,
                                                           const char* buf2,
                                                           size_t size2,
                                                           int max_depth) {
  if (size1 == 0 && size2 == 0) return kUpb_UnknownCompareResult_Equal;
  if (size1 == 0 || size2 == 0) return kUpb_UnknownCompareResult_NotEqual;
  if (memcmp(buf1, buf2, size1) == 0) return kUpb_UnknownCompareResult_Equal;

  upb_UnknownField_Context ctx = {
      .arena = upb_Arena_New(),
      .depth = max_depth,
      .tmp = NULL,
      .tmp_size = 0,
  };

  if (!ctx.arena) return kUpb_UnknownCompareResult_OutOfMemory;

  int ret = UPB_SETJMP(ctx.err);

  if (UPB_LIKELY(ret == 0)) {
    // First build both unknown fields into a sorted data structure (similar
    // to the UnknownFieldSet in C++).
    upb_UnknownFields* uf1 = upb_UnknownFields_Build(&ctx, buf1, size1);
    upb_UnknownFields* uf2 = upb_UnknownFields_Build(&ctx, buf2, size2);

    // Now perform the equality check on the sorted structures.
    if (upb_UnknownFields_IsEqual(uf1, uf2)) {
      ret = kUpb_UnknownCompareResult_Equal;
    } else {
      ret = kUpb_UnknownCompareResult_NotEqual;
    }
  }

  upb_Arena_Free(ctx.arena);
  free(ctx.tmp);
  return ret;
}

bool upb_MessageValue_IsEqual(upb_MessageValue val1, upb_MessageValue val2,
                              const upb_FieldDef* f) {
  switch (upb_FieldDef_CType(f)) {
    case kUpb_CType_Bool:
      return val1.bool_val == val2.bool_val;
    case kUpb_CType_Int32:
    case kUpb_CType_UInt32:
    case kUpb_CType_Enum:
      return val1.int32_val == val2.int32_val;
    case kUpb_CType_Int64:
    case kUpb_CType_UInt64:
      return val1.int64_val == val2.int64_val;
    case kUpb_CType_Float:
      return val1.float_val == val2.float_val;
    case kUpb_CType_Double:
      return val1.double_val == val2.double_val;
    case kUpb_CType_String:
    case kUpb_CType_Bytes:
      return val1.str_val.size == val2.str_val.size &&
             memcmp(val1.str_val.data, val2.str_val.data, val1.str_val.size) ==
                 0;
    case kUpb_CType_Message:
      return upb_Message_IsEqual(val1.msg_val, val2.msg_val,
                                 upb_FieldDef_MessageSubDef(f));
    default:
      return false;
  }
}

bool upb_Map_IsEqual(const upb_Map* map1, const upb_Map* map2,
                     const upb_FieldDef* f) {
  assert(upb_FieldDef_IsMap(f));
  if (map1 == map2) return true;

  size_t size1 = map1 ? upb_Map_Size(map1) : 0;
  size_t size2 = map2 ? upb_Map_Size(map2) : 0;
  if (size1 != size2) return false;
  if (size1 == 0) return true;

  const upb_MessageDef* entry_m = upb_FieldDef_MessageSubDef(f);
  const upb_FieldDef* val_f = upb_MessageDef_Field(entry_m, 1);
  size_t iter = kUpb_Map_Begin;

  while (upb_MapIterator_Next(map1, &iter)) {
    upb_MessageValue key = upb_MapIterator_Key(map1, iter);
    upb_MessageValue val1 = upb_MapIterator_Value(map1, iter);
    upb_MessageValue val2;
    if (!upb_Map_Get(map2, key, &val2)) return false;
    if (!upb_MessageValue_IsEqual(val1, val2, val_f)) return false;
  }

  return true;
}

static bool upb_ArrayElem_IsEqual(const upb_Array* arr1, const upb_Array* arr2,
                                  size_t i, const upb_FieldDef* f) {
  assert(i < upb_Array_Size(arr1));
  assert(i < upb_Array_Size(arr2));
  upb_MessageValue val1 = upb_Array_Get(arr1, i);
  upb_MessageValue val2 = upb_Array_Get(arr2, i);
  return upb_MessageValue_IsEqual(val1, val2, f);
}

bool upb_Array_IsEqual(const upb_Array* arr1, const upb_Array* arr2,
                       const upb_FieldDef* f) {
  assert(upb_FieldDef_IsRepeated(f) && !upb_FieldDef_IsMap(f));
  if (arr1 == arr2) return true;

  size_t n1 = arr1 ? upb_Array_Size(arr1) : 0;
  size_t n2 = arr2 ? upb_Array_Size(arr2) : 0;
  if (n1 != n2) return false;

  // Half the length rounded down.  Important: the empty list rounds to 0.
  size_t half = n1 / 2;

  // Search from the ends-in.  We expect differences to more quickly manifest
  // at the ends than in the middle.  If the length is odd we will miss the
  // middle element.
  for (size_t i = 0; i < half; i++) {
    if (!upb_ArrayElem_IsEqual(arr1, arr2, i, f)) return false;
    if (!upb_ArrayElem_IsEqual(arr1, arr2, n1 - 1 - i, f)) return false;
  }

  // For an odd-lengthed list, pick up the middle element.
  if (n1 & 1) {
    if (!upb_ArrayElem_IsEqual(arr1, arr2, half, f)) return false;
  }

  return true;
}

bool upb_Message_IsEqual(const upb_Message* msg1, const upb_Message* msg2,
                         const upb_MessageDef* m) {
  if (msg1 == msg2) return true;
  if (upb_Message_ExtensionCount(msg1) != upb_Message_ExtensionCount(msg2))
    return false;

  // Compare messages field-by-field.  This is slightly tricky, because while
  // we can iterate over normal fields in a predictable order, the extension
  // order is unpredictable and may be different between msg1 and msg2.
  // So we use the following strategy:
  //   1. Iterate over all msg1 fields (including extensions).
  //   2. For non-extension fields, we find the corresponding field by simply
  //      using upb_Message_Next(msg2).  If the two messages have the same set
  //      of fields, this will yield the same field.
  //   3. For extension fields, we have to actually search for the corresponding
  //      field, which we do with upb_Message_Get(msg2, ext_f1).
  //   4. Once iteration over msg1 is complete, we call upb_Message_Next(msg2)
  //   one
  //      final time to verify that we have visited all of msg2's regular fields
  //      (we pass NULL for ext_dict so that iteration will *not* return
  //      extensions).
  //
  // We don't need to visit all of msg2's extensions, because we verified up
  // front that both messages have the same number of extensions.
  const upb_DefPool* symtab = upb_FileDef_Pool(upb_MessageDef_File(m));
  const upb_FieldDef *f1, *f2;
  upb_MessageValue val1, val2;
  size_t iter1 = kUpb_Message_Begin;
  size_t iter2 = kUpb_Message_Begin;
  while (upb_Message_Next(msg1, m, symtab, &f1, &val1, &iter1)) {
    if (upb_FieldDef_IsExtension(f1)) {
      val2 = upb_Message_Get(msg2, f1);
    } else {
      if (!upb_Message_Next(msg2, m, NULL, &f2, &val2, &iter2) || f1 != f2) {
        return false;
      }
    }

    if (upb_FieldDef_IsMap(f1)) {
      if (!upb_Map_IsEqual(val1.map_val, val2.map_val, f1)) return false;
    } else if (upb_FieldDef_IsRepeated(f1)) {
      if (!upb_Array_IsEqual(val1.array_val, val2.array_val, f1)) {
        return false;
      }
    } else {
      if (!upb_MessageValue_IsEqual(val1, val2, f1)) return false;
    }
  }

  if (upb_Message_Next(msg2, m, NULL, &f2, &val2, &iter2)) return false;

  size_t usize1, usize2;
  const char* uf1 = upb_Message_GetUnknown(msg1, &usize1);
  const char* uf2 = upb_Message_GetUnknown(msg2, &usize2);
  // 100 is arbitrary, we're trying to prevent stack overflow but it's not
  // obvious how deep we should allow here.
  return upb_Message_UnknownFieldsAreEqual(uf1, usize1, uf2, usize2, 100) ==
         kUpb_UnknownCompareResult_Equal;
}
