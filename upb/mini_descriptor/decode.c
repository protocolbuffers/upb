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

#include "upb/mini_descriptor/decode.h"

#include <inttypes.h>
#include <stdlib.h>

#include "upb/base/string_view.h"
#include "upb/mem/arena.h"
#include "upb/mini_descriptor/internal/base92.h"
#include "upb/mini_descriptor/internal/decoder.h"
#include "upb/mini_descriptor/internal/modifiers.h"
#include "upb/mini_descriptor/internal/wire_constants.h"

// Must be last.
#include "upb/port/def.inc"

typedef struct {
  upb_MdDecoder base;
  upb_MiniTable* table;
  upb_MiniTableField* fields;
  upb_MiniTablePlatform platform;
  upb_Arena* arena;
  uint16_t hasbit_count;
  uint16_t oneof_count;
  uint16_t required_count;
} upb_MtDecoder;

static bool upb_MtDecoder_FieldIsPackable(upb_MiniTableField* field) {
  return (field->mode & kUpb_FieldMode_Array) &&
         upb_FieldType_IsPackable(field->UPB_PRIVATE(descriptortype));
}

typedef struct {
  uint16_t submsg_count;
  uint16_t subenum_count;
} upb_SubCounts;

static void upb_MiniTable_SetTypeAndSub(upb_MiniTableField* field,
                                        upb_FieldType type,
                                        upb_SubCounts* sub_counts,
                                        uint64_t msg_modifiers,
                                        bool is_proto3_enum) {
  if (is_proto3_enum) {
    UPB_ASSERT(type == kUpb_FieldType_Enum);
    type = kUpb_FieldType_Int32;
    field->mode |= kUpb_LabelFlags_IsAlternate;
  } else if (type == kUpb_FieldType_String &&
             !(msg_modifiers & kUpb_MessageModifier_ValidateUtf8)) {
    type = kUpb_FieldType_Bytes;
    field->mode |= kUpb_LabelFlags_IsAlternate;
  }

  field->UPB_PRIVATE(descriptortype) = type;

  if (upb_MtDecoder_FieldIsPackable(field) &&
      (msg_modifiers & kUpb_MessageModifier_DefaultIsPacked)) {
    field->mode |= kUpb_LabelFlags_IsPacked;
  }

  if (type == kUpb_FieldType_Message || type == kUpb_FieldType_Group) {
    field->UPB_PRIVATE(submsg_index) = sub_counts->submsg_count++;
  } else if (type == kUpb_FieldType_Enum) {
    // We will need to update this later once we know the total number of
    // submsg fields.
    field->UPB_PRIVATE(submsg_index) = sub_counts->subenum_count++;
  } else {
    field->UPB_PRIVATE(submsg_index) = kUpb_NoSub;
  }

  field->offset = _upb_MiniTableField_GetRep(field);
}

static const char kUpb_EncodedToType[] = {
    [kUpb_EncodedType_Double] = kUpb_FieldType_Double,
    [kUpb_EncodedType_Float] = kUpb_FieldType_Float,
    [kUpb_EncodedType_Int64] = kUpb_FieldType_Int64,
    [kUpb_EncodedType_UInt64] = kUpb_FieldType_UInt64,
    [kUpb_EncodedType_Int32] = kUpb_FieldType_Int32,
    [kUpb_EncodedType_Fixed64] = kUpb_FieldType_Fixed64,
    [kUpb_EncodedType_Fixed32] = kUpb_FieldType_Fixed32,
    [kUpb_EncodedType_Bool] = kUpb_FieldType_Bool,
    [kUpb_EncodedType_String] = kUpb_FieldType_String,
    [kUpb_EncodedType_Group] = kUpb_FieldType_Group,
    [kUpb_EncodedType_Message] = kUpb_FieldType_Message,
    [kUpb_EncodedType_Bytes] = kUpb_FieldType_Bytes,
    [kUpb_EncodedType_UInt32] = kUpb_FieldType_UInt32,
    [kUpb_EncodedType_OpenEnum] = kUpb_FieldType_Enum,
    [kUpb_EncodedType_SFixed32] = kUpb_FieldType_SFixed32,
    [kUpb_EncodedType_SFixed64] = kUpb_FieldType_SFixed64,
    [kUpb_EncodedType_SInt32] = kUpb_FieldType_SInt32,
    [kUpb_EncodedType_SInt64] = kUpb_FieldType_SInt64,
    [kUpb_EncodedType_ClosedEnum] = kUpb_FieldType_Enum,
};

static void upb_MiniTable_SetField(upb_MtDecoder* d, uint8_t ch,
                                   upb_MiniTableField* field,
                                   uint64_t msg_modifiers,
                                   upb_SubCounts* sub_counts) {
  static const char kUpb_EncodedToFieldRep[] = {
      [kUpb_EncodedType_Double] = kUpb_FieldRep_8Byte,
      [kUpb_EncodedType_Float] = kUpb_FieldRep_4Byte,
      [kUpb_EncodedType_Int64] = kUpb_FieldRep_8Byte,
      [kUpb_EncodedType_UInt64] = kUpb_FieldRep_8Byte,
      [kUpb_EncodedType_Int32] = kUpb_FieldRep_4Byte,
      [kUpb_EncodedType_Fixed64] = kUpb_FieldRep_8Byte,
      [kUpb_EncodedType_Fixed32] = kUpb_FieldRep_4Byte,
      [kUpb_EncodedType_Bool] = kUpb_FieldRep_1Byte,
      [kUpb_EncodedType_String] = kUpb_FieldRep_StringView,
      [kUpb_EncodedType_Bytes] = kUpb_FieldRep_StringView,
      [kUpb_EncodedType_UInt32] = kUpb_FieldRep_4Byte,
      [kUpb_EncodedType_OpenEnum] = kUpb_FieldRep_4Byte,
      [kUpb_EncodedType_SFixed32] = kUpb_FieldRep_4Byte,
      [kUpb_EncodedType_SFixed64] = kUpb_FieldRep_8Byte,
      [kUpb_EncodedType_SInt32] = kUpb_FieldRep_4Byte,
      [kUpb_EncodedType_SInt64] = kUpb_FieldRep_8Byte,
      [kUpb_EncodedType_ClosedEnum] = kUpb_FieldRep_4Byte,
  };

  char pointer_rep = d->platform == kUpb_MiniTablePlatform_32Bit
                         ? kUpb_FieldRep_4Byte
                         : kUpb_FieldRep_8Byte;

  int8_t type = _upb_FromBase92(ch);
  if (ch >= _upb_ToBase92(kUpb_EncodedType_RepeatedBase)) {
    type -= kUpb_EncodedType_RepeatedBase;
    field->mode = kUpb_FieldMode_Array;
    field->mode |= pointer_rep << kUpb_FieldRep_Shift;
    field->presence = kUpb_PresenceFlags_ImplicitPresence;
  } else {
    field->mode = kUpb_FieldMode_Scalar;
    field->presence = kUpb_PresenceFlags_ExplicitPresence;
    if (type == kUpb_EncodedType_Group || type == kUpb_EncodedType_Message) {
      field->mode |= pointer_rep << kUpb_FieldRep_Shift;
    } else if ((unsigned long)type >= sizeof(kUpb_EncodedToFieldRep)) {
      upb_MdDecoder_ErrorJmp(&d->base, "Invalid field type: %d", (int)type);
    } else {
      field->mode |= kUpb_EncodedToFieldRep[type] << kUpb_FieldRep_Shift;
    }
  }
  if ((unsigned long)type >= sizeof(kUpb_EncodedToType)) {
    upb_MdDecoder_ErrorJmp(&d->base, "Invalid field type: %d", (int)type);
  }
  upb_MiniTable_SetTypeAndSub(field, kUpb_EncodedToType[type], sub_counts,
                              msg_modifiers, type == kUpb_EncodedType_OpenEnum);
}

static void upb_MtDecoder_ModifyField(upb_MtDecoder* d,
                                      uint32_t message_modifiers,
                                      uint32_t field_modifiers,
                                      upb_MiniTableField* field) {
  if (field_modifiers & kUpb_EncodedFieldModifier_FlipPacked) {
    if (!upb_MtDecoder_FieldIsPackable(field)) {
      upb_MdDecoder_ErrorJmp(&d->base,
                             "Cannot flip packed on unpackable field %" PRIu32,
                             field->number);
    }
    field->mode ^= kUpb_LabelFlags_IsPacked;
  }

  bool singular = field_modifiers & kUpb_EncodedFieldModifier_IsProto3Singular;
  bool required = field_modifiers & kUpb_EncodedFieldModifier_IsRequired;

  // Validate.
  if ((singular || required) &&
      field->presence != kUpb_PresenceFlags_ExplicitPresence) {
    upb_MdDecoder_ErrorJmp(&d->base,
                           "Invalid modifier(s) for repeated field %" PRIu32,
                           field->number);
  }
  if (singular && required) {
    upb_MdDecoder_ErrorJmp(
        &d->base, "Field %" PRIu32 " cannot be both singular and required",
        field->number);
  }

  if (singular) field->presence = kUpb_PresenceFlags_ImplicitPresence;
  if (required) {
    field->presence = kUpb_PresenceFlags_RequiredPresence;
  }
}

size_t upb_MtDecoder_SizeOfRep(upb_FieldRep rep,
                               upb_MiniTablePlatform platform) {
  static const uint8_t kRepToSize32[] = {
      [kUpb_FieldRep_1Byte] = 1,
      [kUpb_FieldRep_4Byte] = 4,
      [kUpb_FieldRep_StringView] = 8,
      [kUpb_FieldRep_8Byte] = 8,
  };
  static const uint8_t kRepToSize64[] = {
      [kUpb_FieldRep_1Byte] = 1,
      [kUpb_FieldRep_4Byte] = 4,
      [kUpb_FieldRep_StringView] = 16,
      [kUpb_FieldRep_8Byte] = 8,
  };
  UPB_ASSERT(sizeof(upb_StringView) ==
             UPB_SIZE(kRepToSize32, kRepToSize64)[kUpb_FieldRep_StringView]);
  return platform == kUpb_MiniTablePlatform_32Bit ? kRepToSize32[rep]
                                                  : kRepToSize64[rep];
}

size_t upb_MtDecoder_AlignOfRep(upb_FieldRep rep,
                                upb_MiniTablePlatform platform) {
  static const uint8_t kRepToAlign32[] = {
      [kUpb_FieldRep_1Byte] = 1,
      [kUpb_FieldRep_4Byte] = 4,
      [kUpb_FieldRep_StringView] = 4,
      [kUpb_FieldRep_8Byte] = 8,
  };
  static const uint8_t kRepToAlign64[] = {
      [kUpb_FieldRep_1Byte] = 1,
      [kUpb_FieldRep_4Byte] = 4,
      [kUpb_FieldRep_StringView] = 8,
      [kUpb_FieldRep_8Byte] = 8,
  };
  UPB_ASSERT(UPB_ALIGN_OF(upb_StringView) ==
             UPB_SIZE(kRepToAlign32, kRepToAlign64)[kUpb_FieldRep_StringView]);
  return platform == kUpb_MiniTablePlatform_32Bit ? kRepToAlign32[rep]
                                                  : kRepToAlign64[rep];
}

static const char* upb_MtDecoder_DecodeOneofField(
    upb_MtDecoder* d, const char* ptr, char first_ch,
    upb_MiniTableField** oneof_f) {
  uint32_t field_num;
  ptr = upb_MdDecoder_DecodeBase92Varint(
      &d->base, ptr, first_ch, kUpb_EncodedValue_MinOneofField,
      kUpb_EncodedValue_MaxOneofField, &field_num);
  upb_MiniTableField* f =
      (void*)upb_MiniTable_FindFieldByNumber(d->table, field_num);

  if (!f) {
    upb_MdDecoder_ErrorJmp(&d->base,
                           "Couldn't add field number %" PRIu32
                           " to oneof, no such field number.",
                           field_num);
  }
  if (f->presence != kUpb_PresenceFlags_ExplicitPresence) {
    upb_MdDecoder_ErrorJmp(
        &d->base,
        "Cannot add repeated, required, or singular field %" PRIu32
        " to oneof.",
        field_num);
  }

  if (*oneof_f == NULL) {
    f->presence = (f - d->table->fields) | kUpb_PresenceFlags_IsOneof;
    d->oneof_count++;
    *oneof_f = f;
  } else {
    // Oneof storage must be large enough to accommodate the largest member.
    f->presence = (*oneof_f)->presence;
    (*oneof_f)->offset = UPB_MAX((*oneof_f)->offset, f->offset);
  }

  return ptr;
}

static const char* upb_MtDecoder_DecodeOneofs(upb_MtDecoder* d,
                                              const char* ptr) {
  upb_MiniTableField* f = NULL;
  while (ptr < d->base.end) {
    char ch = *ptr++;
    if (ch == kUpb_EncodedValue_FieldSeparator) {
      // Field separator, no action needed.
    } else if (ch == kUpb_EncodedValue_OneofSeparator) {
      f = NULL;
    } else {
      ptr = upb_MtDecoder_DecodeOneofField(d, ptr, ch, &f);
    }
  }

  return ptr;
}

static const char* upb_MtDecoder_ParseModifier(upb_MtDecoder* d,
                                               const char* ptr, char first_ch,
                                               upb_MiniTableField* last_field,
                                               uint64_t* msg_modifiers) {
  uint32_t mod;
  ptr = upb_MdDecoder_DecodeBase92Varint(&d->base, ptr, first_ch,
                                         kUpb_EncodedValue_MinModifier,
                                         kUpb_EncodedValue_MaxModifier, &mod);
  if (last_field) {
    upb_MtDecoder_ModifyField(d, *msg_modifiers, mod, last_field);
  } else {
    if (!d->table) {
      upb_MdDecoder_ErrorJmp(&d->base,
                             "Extensions cannot have message modifiers");
    }
    *msg_modifiers = mod;
  }

  return ptr;
}

static void upb_MtDecoder_AllocateSubs(upb_MtDecoder* d,
                                       upb_SubCounts sub_counts) {
  uint32_t total_count = sub_counts.submsg_count + sub_counts.subenum_count;
  size_t subs_bytes = sizeof(*d->table->subs) * total_count;
  upb_MiniTableSub* subs = upb_Arena_Malloc(d->arena, subs_bytes);
  upb_MdDecoder_CheckOutOfMemory(&d->base, subs);
  uint32_t i = 0;
  for (; i < sub_counts.submsg_count; i++) {
    subs[i].submsg = &_kUpb_MiniTable_Empty;
  }
  if (sub_counts.subenum_count) {
    upb_MiniTableField* f = d->fields;
    upb_MiniTableField* end_f = f + d->table->field_count;
    for (; f < end_f; f++) {
      if (f->UPB_PRIVATE(descriptortype) == kUpb_FieldType_Enum) {
        f->UPB_PRIVATE(submsg_index) += sub_counts.submsg_count;
      }
    }
    for (; i < sub_counts.submsg_count + sub_counts.subenum_count; i++) {
      subs[i].subenum = NULL;
    }
  }
  d->table->subs = subs;
}

static const char* upb_MtDecoder_Parse(upb_MtDecoder* d, const char* ptr,
                                       size_t len, void* fields,
                                       size_t field_size, uint16_t* field_count,
                                       upb_SubCounts* sub_counts) {
  uint64_t msg_modifiers = 0;
  uint32_t last_field_number = 0;
  upb_MiniTableField* last_field = NULL;
  bool need_dense_below = d->table != NULL;

  d->base.end = UPB_PTRADD(ptr, len);

  while (ptr < d->base.end) {
    char ch = *ptr++;
    if (ch <= kUpb_EncodedValue_MaxField) {
      if (!d->table && last_field) {
        // For extensions, consume only a single field and then return.
        return --ptr;
      }
      upb_MiniTableField* field = fields;
      *field_count += 1;
      fields = (char*)fields + field_size;
      field->number = ++last_field_number;
      last_field = field;
      upb_MiniTable_SetField(d, ch, field, msg_modifiers, sub_counts);
    } else if (kUpb_EncodedValue_MinModifier <= ch &&
               ch <= kUpb_EncodedValue_MaxModifier) {
      ptr = upb_MtDecoder_ParseModifier(d, ptr, ch, last_field, &msg_modifiers);
      if (msg_modifiers & kUpb_MessageModifier_IsExtendable) {
        d->table->ext |= kUpb_ExtMode_Extendable;
      }
    } else if (ch == kUpb_EncodedValue_End) {
      if (!d->table) {
        upb_MdDecoder_ErrorJmp(&d->base, "Extensions cannot have oneofs.");
      }
      ptr = upb_MtDecoder_DecodeOneofs(d, ptr);
    } else if (kUpb_EncodedValue_MinSkip <= ch &&
               ch <= kUpb_EncodedValue_MaxSkip) {
      if (need_dense_below) {
        d->table->dense_below = d->table->field_count;
        need_dense_below = false;
      }
      uint32_t skip;
      ptr = upb_MdDecoder_DecodeBase92Varint(&d->base, ptr, ch,
                                             kUpb_EncodedValue_MinSkip,
                                             kUpb_EncodedValue_MaxSkip, &skip);
      last_field_number += skip;
      last_field_number--;  // Next field seen will increment.
    } else {
      upb_MdDecoder_ErrorJmp(&d->base, "Invalid char: %c", ch);
    }
  }

  if (need_dense_below) {
    d->table->dense_below = d->table->field_count;
  }

  return ptr;
}

static void upb_MtDecoder_ParseMessage(upb_MtDecoder* d, const char* data,
                                       size_t len) {
  // Buffer length is an upper bound on the number of fields. We will return
  // what we don't use.
  d->fields = upb_Arena_Malloc(d->arena, sizeof(*d->fields) * len);
  upb_MdDecoder_CheckOutOfMemory(&d->base, d->fields);

  upb_SubCounts sub_counts = {0, 0};
  d->table->field_count = 0;
  d->table->fields = d->fields;
  upb_MtDecoder_Parse(d, data, len, d->fields, sizeof(*d->fields),
                      &d->table->field_count, &sub_counts);

  upb_Arena_ShrinkLast(d->arena, d->fields, sizeof(*d->fields) * len,
                       sizeof(*d->fields) * d->table->field_count);
  d->table->fields = d->fields;
  upb_MtDecoder_AllocateSubs(d, sub_counts);
}

static int upb_MtDecoder_FieldRank(const upb_MiniTableField* f) {
  bool not_required = f->presence != kUpb_PresenceFlags_RequiredPresence;
  // Required fields go first, then non-required.  Sort by number, ascending.
  return ((int)not_required << 29) | f->number;
}

static int upb_MtDecoder_CompareFields(const void* a, const void* b) {
  return upb_MtDecoder_FieldRank(a) < upb_MtDecoder_FieldRank(b) ? -1 : 1;
}

static void upb_MtDecoder_SortFields(upb_MiniTable* mt) {
  qsort((void*)mt->fields, mt->field_count, sizeof(*mt->fields),
        upb_MtDecoder_CompareFields);
}

static size_t upb_MiniTable_DivideRoundUp(size_t n, size_t d) {
  return (n + d - 1) / d;
}

static void upb_MtDecoder_AssignHasbits(upb_MiniTable* mt) {
  upb_MtDecoder_SortFields(mt);

  int n = mt->field_count;
  int hasbit_bytes = upb_MiniTable_DivideRoundUp(n, 8);
  int oneof_case = UPB_ALIGN_UP(hasbit_bytes, 4);

  for (int i = 0; i < n; i++) {
    upb_MiniTableField* field = (upb_MiniTableField*)&mt->fields[i];
    if (field->presence & kUpb_PresenceFlags_IsOneof) {
      uint16_t masked_presence =
          field->presence & ~(uint16_t)kUpb_PresenceFlags_IsOneof;
      if (masked_presence == i) {
        // Primary oneof field.
        field->presence = oneof_case | kUpb_PresenceFlags_IsOneof;
        oneof_case += 4;
      } else {
        // Secondary oneof field.
        upb_MiniTableField* primary = (void*)&mt->fields[masked_presence];
        field->offset = primary->offset;  // Copy previously-assigned offset.
        field->presence = primary->presence;
      }
    } else {
      // Field with hasbit.  Preserve presence flags.
      field->presence |= i;
    }
  }
}

static void upb_MtDecoder_AssignOffsetsForRep(upb_MtDecoder* d,
                                              upb_FieldRep rep) {
  upb_MiniTable* mt = d->table;

  size_t field_size = upb_MtDecoder_SizeOfRep(rep, d->platform);
  size_t field_align = upb_MtDecoder_AlignOfRep(rep, d->platform);
  size_t msg_size = UPB_ALIGN_UP(mt->size, field_align);

  int n = mt->field_count;
  for (int i = 0; i < n; i++) {
    // OPT: consider making branchless
    upb_MiniTableField* field = (void*)&mt->fields[i];
    UPB_ASSERT(field->offset >= rep);
    if (field->offset != rep) continue;
    uint16_t masked_presence =
        field->presence & ~(uint16_t)kUpb_PresenceFlags_IsOneof;
    if (masked_presence != i) {
      // This is a oneof field that is not in the primary slot.
      continue;
    }
    // Non-oneof or oneof in primary slot: we need to place.
    field->offset = msg_size;
    msg_size += field_size;
  }

  const size_t max = UINT16_MAX;

  if (msg_size > max) {
    upb_MdDecoder_ErrorJmp(
        &d->base, "Message size exceeded maximum size of %zu bytes", max);
  }

  mt->size = msg_size;
}

static void upb_MtDecoder_AssignOffsets(upb_MtDecoder* d) {
  upb_MiniTable* mt = d->table;

  // Account for hasbits and oneof cases, which come before all other data.
  mt->size =
      upb_MiniTable_DivideRoundUp(mt->field_count, 8) + (d->oneof_count * 2);

  for (int i = 0; i <= kUpb_FieldRep_Max; i++) {
    upb_MtDecoder_AssignOffsetsForRep(d, i);
  }

  // Note: The fasttable parser (supported on 64-bit only) depends on overall
  // size being a multiple of 8 in order to satisfy UPB_MALLOC_ALIGN, which is
  // also 8.
  //
  // On 32-bit we could potentially make this smaller, but there is no
  // compelling reason to optimize this right now.
}

static void upb_MtDecoder_ValidateEntryField(upb_MtDecoder* d,
                                             const upb_MiniTableField* f,
                                             uint32_t expected_num) {
  const char* name = expected_num == 1 ? "key" : "val";
  if (f->number != expected_num) {
    upb_MdDecoder_ErrorJmp(&d->base,
                           "map %s did not have expected number (%d vs %d)",
                           name, expected_num, (int)f->number);
  }

  if (upb_IsRepeatedOrMap(f)) {
    upb_MdDecoder_ErrorJmp(
        &d->base, "map %s cannot be repeated or map, or be in oneof", name);
  }

  uint32_t not_ok_types;
  if (expected_num == 1) {
    not_ok_types = (1 << kUpb_FieldType_Float) | (1 << kUpb_FieldType_Double) |
                   (1 << kUpb_FieldType_Message) | (1 << kUpb_FieldType_Group) |
                   (1 << kUpb_FieldType_Bytes) | (1 << kUpb_FieldType_Enum);
  } else {
    not_ok_types = 1 << kUpb_FieldType_Group;
  }

  if ((1 << upb_MiniTableField_Type(f)) & not_ok_types) {
    upb_MdDecoder_ErrorJmp(&d->base, "map %s cannot have type %d", name,
                           (int)f->UPB_PRIVATE(descriptortype));
  }
}

static void upb_MtDecoder_ParseMap(upb_MtDecoder* d, const char* data,
                                   size_t len) {
  upb_MtDecoder_ParseMessage(d, data, len);

  if (UPB_UNLIKELY(d->table->field_count != 2)) {
    upb_MdDecoder_ErrorJmp(&d->base, "%hu fields in map",
                           d->table->field_count);
    UPB_UNREACHABLE();
  }

  if (d->oneof_count != 0) {
    upb_MdDecoder_ErrorJmp(&d->base, "Map entry cannot have oneof");
  }

  upb_MtDecoder_ValidateEntryField(d, &d->table->fields[0], 1);
  upb_MtDecoder_ValidateEntryField(d, &d->table->fields[1], 2);

  // Map entries have a pre-determined layout, regardless of types.
  // NOTE: sync with mini_table/message_internal.h.
  const size_t kv_size = d->platform == kUpb_MiniTablePlatform_32Bit ? 8 : 16;
  const size_t hasbit_size = 8;
  d->fields[0].offset = hasbit_size;
  d->fields[1].offset = hasbit_size + kv_size;
  d->table->size = UPB_ALIGN_UP(hasbit_size + kv_size + kv_size, 8);

  upb_MtDecoder_AssignHasbits(d->table);

  // Map entries have a special bit set to signal it's a map entry, used in
  // upb_MiniTable_SetSubMessage() below.
  d->table->ext |= kUpb_ExtMode_IsMapEntry;
}

static void upb_MtDecoder_ParseMessageSet(upb_MtDecoder* d, const char* data,
                                          size_t len) {
  if (len > 0) {
    upb_MdDecoder_ErrorJmp(&d->base, "Invalid message set encode length: %zu",
                           len);
  }

  upb_MiniTable* ret = d->table;
  ret->size = 0;
  ret->field_count = 0;
  ret->ext = kUpb_ExtMode_IsMessageSet;
  ret->dense_below = 0;
  ret->table_mask = -1;
  ret->required_count = 0;
}

static upb_MiniTable* upb_MtDecoder_DoBuildMiniTable(upb_MtDecoder* decoder,
                                                     const char* data,
                                                     size_t len) {
  upb_MdDecoder_CheckOutOfMemory(&decoder->base, decoder->table);

  decoder->table->size = 0;
  decoder->table->field_count = 0;
  decoder->table->ext = kUpb_ExtMode_NonExtendable;
  decoder->table->dense_below = 0;
  decoder->table->table_mask = -1;
  decoder->table->required_count = 0;

  // Strip off and verify the version tag.
  if (!len--) {
    upb_MdDecoder_ErrorJmp(&decoder->base, "Empty MiniDescriptor: no version");
  }

  const char vers = *data++;

  switch (vers) {
    case kUpb_EncodedVersion_MapV1:
      upb_MtDecoder_ParseMap(decoder, data, len);
      break;

    case kUpb_EncodedVersion_MessageV1:
      upb_MtDecoder_ParseMessage(decoder, data, len);
      upb_MtDecoder_AssignOffsets(decoder);
      upb_MtDecoder_AssignHasbits(decoder->table);
      break;

    case kUpb_EncodedVersion_MessageSetV1:
      upb_MtDecoder_ParseMessageSet(decoder, data, len);
      break;

    default:
      upb_MdDecoder_ErrorJmp(&decoder->base, "Invalid message version: %c",
                             vers);
  }

  return decoder->table;
}

static upb_MiniTable* upb_MtDecoder_BuildMiniTable(upb_MtDecoder* const decoder,
                                                   const char* const data,
                                                   const size_t len) {
  if (UPB_SETJMP(decoder->base.err) != 0) return NULL;
  return upb_MtDecoder_DoBuildMiniTable(decoder, data, len);
}

upb_MiniTable* _upb_MiniTable_Build(const char* data, size_t len,
                                    upb_MiniTablePlatform platform,
                                    upb_Arena* arena, upb_Status* status) {
  upb_MtDecoder decoder = {
      .base = {.status = status},
      .platform = platform,
      .arena = arena,
      .table = upb_Arena_Malloc(arena, sizeof(*decoder.table)),
      .hasbit_count = 0,
      .oneof_count = 0,
  };

  return upb_MtDecoder_BuildMiniTable(&decoder, data, len);
}

static const char* upb_MtDecoder_DoBuildMiniTableExtension(
    upb_MtDecoder* decoder, const char* data, size_t len,
    upb_MiniTableExtension* ext, const upb_MiniTable* extendee,
    upb_MiniTableSub sub) {
  // If the string is non-empty then it must begin with a version tag.
  if (len) {
    if (*data != kUpb_EncodedVersion_ExtensionV1) {
      upb_MdDecoder_ErrorJmp(&decoder->base, "Invalid ext version: %c", *data);
    }
    data++;
    len--;
  }

  uint16_t count = 0;
  upb_SubCounts sub_counts = {0, 0};
  const char* ret = upb_MtDecoder_Parse(decoder, data, len, ext, sizeof(*ext),
                                        &count, &sub_counts);
  if (!ret || count != 1) return NULL;

  upb_MiniTableField* f = &ext->field;

  f->mode |= kUpb_LabelFlags_IsExtension;
  f->offset = 0;
  f->presence = 0;

  if (extendee->ext & kUpb_ExtMode_IsMessageSet) {
    // Extensions of MessageSet must be messages.
    if (!upb_IsSubMessage(f)) return NULL;

    // Extensions of MessageSet must be non-repeating.
    if ((f->mode & kUpb_FieldMode_Mask) == kUpb_FieldMode_Array) return NULL;
  }

  ext->extendee = extendee;
  ext->sub = sub;

  return ret;
}

static const char* upb_MtDecoder_BuildMiniTableExtension(
    upb_MtDecoder* const decoder, const char* const data, const size_t len,
    upb_MiniTableExtension* const ext, const upb_MiniTable* const extendee,
    const upb_MiniTableSub sub) {
  if (UPB_SETJMP(decoder->base.err) != 0) return NULL;
  return upb_MtDecoder_DoBuildMiniTableExtension(decoder, data, len, ext,
                                                 extendee, sub);
}

const char* _upb_MiniTableExtension_Init(const char* data, size_t len,
                                         upb_MiniTableExtension* ext,
                                         const upb_MiniTable* extendee,
                                         upb_MiniTableSub sub,
                                         upb_MiniTablePlatform platform,
                                         upb_Status* status) {
  upb_MtDecoder decoder = {
      .base = {.status = status},
      .arena = NULL,
      .table = NULL,
      .platform = platform,
  };

  return upb_MtDecoder_BuildMiniTableExtension(&decoder, data, len, ext,
                                               extendee, sub);
}

upb_MiniTableExtension* _upb_MiniTableExtension_Build(
    const char* data, size_t len, const upb_MiniTable* extendee,
    upb_MiniTableSub sub, upb_MiniTablePlatform platform, upb_Arena* arena,
    upb_Status* status) {
  upb_MiniTableExtension* ext =
      upb_Arena_Malloc(arena, sizeof(upb_MiniTableExtension));
  if (UPB_UNLIKELY(!ext)) return NULL;

  const char* ptr = _upb_MiniTableExtension_Init(data, len, ext, extendee, sub,
                                                 platform, status);
  if (UPB_UNLIKELY(!ptr)) return NULL;

  return ext;
}
