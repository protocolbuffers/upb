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

#include "upb/mini_table_accessors.h"

#include "upb/mini_table.h"
#include "upb/msg_internal.h"

// Must be last.
#include "upb/port_def.inc"

size_t upb_MiniTable_get_field_size(const upb_MiniTable_Field* f) {
  static unsigned char sizes[] = {
      0,                      /* 0 */
      8,                      /* kUpb_FieldType_Double */
      4,                      /* kUpb_FieldType_Float */
      8,                      /* kUpb_FieldType_Int64 */
      8,                      /* kUpb_FieldType_UInt64 */
      4,                      /* kUpb_FieldType_Int32 */
      8,                      /* kUpb_FieldType_Fixed64 */
      4,                      /* kUpb_FieldType_Fixed32 */
      1,                      /* kUpb_FieldType_Bool */
      sizeof(upb_StringView), /* kUpb_FieldType_String */
      sizeof(void*),          /* kUpb_FieldType_Group */
      sizeof(void*),          /* kUpb_FieldType_Message */
      sizeof(upb_StringView), /* kUpb_FieldType_Bytes */
      4,                      /* kUpb_FieldType_UInt32 */
      4,                      /* kUpb_FieldType_Enum */
      4,                      /* kUpb_FieldType_SFixed32 */
      8,                      /* kUpb_FieldType_SFixed64 */
      4,                      /* kUpb_FieldType_SInt32 */
      8,                      /* kUpb_FieldType_SInt64 */
  };
  return upb_IsRepeatedOrMap(f) ? sizeof(void*) : sizes[f->descriptortype];
}

bool upb_MiniTable_HasField(const upb_Message* msg,
                            const upb_MiniTable_Field* field) {
  if (_upb_MiniTable_Field_InOneOf(field)) {
    return _upb_getoneofcase_field(msg, field) == field->number;
  } else if (field->presence > 0) {
    return _upb_hasbit_field(msg, field);
  } else {
    UPB_ASSERT(field->descriptortype == kUpb_FieldType_Message ||
               field->descriptortype == kUpb_FieldType_Group);
    return upb_MiniTable_GetMessage(msg, field) != NULL;
  }
}

void upb_MiniTable_ClearField(upb_Message* msg,
                              const upb_MiniTable_Field* field) {
  char* mem = UPB_PTR_AT(msg, field->offset, char);
  if (field->presence > 0) {
    _upb_clearhas_field(msg, field);
  } else if (_upb_MiniTable_Field_InOneOf(field)) {
    uint32_t* oneof_case = _upb_oneofcase_field(msg, field);
    if (*oneof_case != field->number) return;
    *oneof_case = 0;
  }
  memset(mem, 0, upb_MiniTable_get_field_size(field));
}

static const char _upb_CTypeo_sizelg2[12] = {
    0,
    0,              /* kUpb_CType_Bool */
    2,              /* kUpb_CType_Float */
    2,              /* kUpb_CType_Int32 */
    2,              /* kUpb_CType_UInt32 */
    2,              /* kUpb_CType_Enum */
    UPB_SIZE(2, 3), /* kUpb_CType_Message */
    3,              /* kUpb_CType_Double */
    3,              /* kUpb_CType_Int64 */
    3,              /* kUpb_CType_UInt64 */
    UPB_SIZE(3, 4), /* kUpb_CType_String */
    UPB_SIZE(3, 4), /* kUpb_CType_Bytes */
};

upb_CType upb_MiniTable_Field_CType(const upb_MiniTable_Field* f) {
  switch (f->descriptortype) {
    case kUpb_FieldType_Double:
      return kUpb_CType_Double;
    case kUpb_FieldType_Float:
      return kUpb_CType_Float;
    case kUpb_FieldType_Int64:
    case kUpb_FieldType_SInt64:
    case kUpb_FieldType_SFixed64:
      return kUpb_CType_Int64;
    case kUpb_FieldType_Int32:
    case kUpb_FieldType_SFixed32:
    case kUpb_FieldType_SInt32:
      return kUpb_CType_Int32;
    case kUpb_FieldType_UInt64:
    case kUpb_FieldType_Fixed64:
      return kUpb_CType_UInt64;
    case kUpb_FieldType_UInt32:
    case kUpb_FieldType_Fixed32:
      return kUpb_CType_UInt32;
    case kUpb_FieldType_Enum:
      return kUpb_CType_Enum;
    case kUpb_FieldType_Bool:
      return kUpb_CType_Bool;
    case kUpb_FieldType_String:
      return kUpb_CType_String;
    case kUpb_FieldType_Bytes:
      return kUpb_CType_Bytes;
    case kUpb_FieldType_Group:
    case kUpb_FieldType_Message:
      return kUpb_CType_Message;
  }
  UPB_UNREACHABLE();
}

// Resizes storage for repeated fields.
upb_RepeatedFieldArray* upb_MiniTable_ResizeArray(
    upb_Message* msg, const upb_MiniTable_Field* field, size_t new_size,
    upb_Arena* arena) {
  upb_Array* array_val = *UPB_PTR_AT(msg, field->offset, upb_Array*);
  if (!array_val) {
    array_val = _upb_Array_New(
        arena, new_size, _upb_CTypeo_sizelg2[upb_MiniTable_Field_CType(field)]);
    if (!array_val) {
      return NULL;
    }
    *UPB_PTR_AT(msg, field->offset, upb_Array*) = array_val;
  } else {
    if (!_upb_Array_Resize(array_val, new_size, arena)) {
      return NULL;
    }
  }
  return (upb_RepeatedFieldArray*)array_val;
}

void upb_MiniTable_SetArrayValue(upb_RepeatedFieldArray* array, size_t index,
                                 const upb_FieldValue* value) {
  upb_Array* arr = (upb_Array*)array;
  char* data = _upb_array_ptr(arr);
  int lg2 = arr->data & 7;
  UPB_ASSERT(index < arr->len);
  memcpy(data + (index << lg2), value, 1 << lg2);
}

upb_FieldValue upb_MiniTable_GetArrayValue(const upb_RepeatedFieldArray* array,
                                           size_t index) {
  upb_FieldValue ret;
  upb_Array* arr = (upb_Array*)array;
  const char* data = (const char*)_upb_array_constptr(arr);
  int lg2 = arr->data & 7;
  UPB_ASSERT(index < arr->len);
  memcpy(&ret, data + (index << lg2), 1 << lg2);
  return ret;
}

upb_MutableFieldValue upb_MiniTable_GetMutableArrayValue(
    const upb_RepeatedFieldArray* array, size_t index) {
  upb_MutableFieldValue ret;
  upb_Array* arr = (upb_Array*)array;
  char* data = (char*)_upb_array_ptr(arr);
  int lg2 = arr->data & 7;
  UPB_ASSERT(index < arr->len);
  memcpy(&ret, data + (index << lg2), 1 << lg2);
  return ret;
}
