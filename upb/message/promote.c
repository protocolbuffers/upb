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

#include "upb/message/promote.h"

#include "upb/collections/array.h"
#include "upb/collections/array_internal.h"
#include "upb/collections/map.h"
#include "upb/message/accessors.h"
#include "upb/message/message.h"
#include "upb/mini_table/field_internal.h"
#include "upb/wire/common.h"
#include "upb/wire/decode.h"
#include "upb/wire/encode.h"
#include "upb/wire/eps_copy_input_stream.h"
#include "upb/wire/reader.h"

// Must be last.
#include "upb/port/def.inc"

// Parses unknown data by merging into existing base_message or creating a
// new message usingg mini_table.
static upb_UnknownToMessageRet upb_MiniTable_ParseUnknownMessage(
    const char* unknown_data, size_t unknown_size,
    const upb_MiniTable* mini_table, upb_Message* base_message,
    int decode_options, upb_Arena* arena) {
  upb_UnknownToMessageRet ret;
  ret.message =
      base_message ? base_message : _upb_Message_New(mini_table, arena);
  if (!ret.message) {
    ret.status = kUpb_UnknownToMessage_OutOfMemory;
    return ret;
  }
  // Decode sub message using unknown field contents.
  const char* data = unknown_data;
  uint32_t tag;
  uint64_t message_len = 0;
  data = upb_WireReader_ReadTag(data, &tag);
  data = upb_WireReader_ReadVarint(data, &message_len);
  upb_DecodeStatus status = upb_Decode(data, message_len, ret.message,
                                       mini_table, NULL, decode_options, arena);
  if (status == kUpb_DecodeStatus_OutOfMemory) {
    ret.status = kUpb_UnknownToMessage_OutOfMemory;
  } else if (status == kUpb_DecodeStatus_Ok) {
    ret.status = kUpb_UnknownToMessage_Ok;
  } else {
    ret.status = kUpb_UnknownToMessage_ParseError;
  }
  return ret;
}

upb_GetExtension_Status upb_MiniTable_GetOrPromoteExtension(
    upb_Message* msg, const upb_MiniTableExtension* ext_table,
    int decode_options, upb_Arena* arena,
    const upb_Message_Extension** extension) {
  UPB_ASSERT(upb_MiniTableField_CType(&ext_table->field) == kUpb_CType_Message);
  *extension = _upb_Message_Getext(msg, ext_table);
  if (*extension) {
    return kUpb_GetExtension_Ok;
  }

  // Check unknown fields, if available promote.
  int field_number = ext_table->field.number;
  upb_FindUnknownRet result = upb_MiniTable_FindUnknown(
      msg, field_number, kUpb_WireFormat_DefaultDepthLimit);
  if (result.status != kUpb_FindUnknown_Ok) {
    return kUpb_GetExtension_NotPresent;
  }
  size_t len;
  size_t ofs = result.ptr - upb_Message_GetUnknown(msg, &len);
  // Decode and promote from unknown.
  const upb_MiniTable* extension_table = ext_table->sub.submsg;
  upb_UnknownToMessageRet parse_result = upb_MiniTable_ParseUnknownMessage(
      result.ptr, result.len, extension_table,
      /* base_message= */ NULL, decode_options, arena);
  switch (parse_result.status) {
    case kUpb_UnknownToMessage_OutOfMemory:
      return kUpb_GetExtension_OutOfMemory;
    case kUpb_UnknownToMessage_ParseError:
      return kUpb_GetExtension_ParseError;
    case kUpb_UnknownToMessage_NotFound:
      return kUpb_GetExtension_NotPresent;
    case kUpb_UnknownToMessage_Ok:
      break;
  }
  upb_Message* extension_msg = parse_result.message;
  // Add to extensions.
  upb_Message_Extension* ext =
      _upb_Message_GetOrCreateExtension(msg, ext_table, arena);
  if (!ext) {
    return kUpb_GetExtension_OutOfMemory;
  }
  memcpy(&ext->data, &extension_msg, sizeof(extension_msg));
  *extension = ext;
  const char* delete_ptr = upb_Message_GetUnknown(msg, &len) + ofs;
  upb_Message_DeleteUnknown(msg, delete_ptr, result.len);
  return kUpb_GetExtension_Ok;
}

static upb_FindUnknownRet upb_FindUnknownRet_ParseError(void) {
  return (upb_FindUnknownRet){.status = kUpb_FindUnknown_ParseError};
}

upb_FindUnknownRet upb_MiniTable_FindUnknown(const upb_Message* msg,
                                             uint32_t field_number,
                                             int depth_limit) {
  size_t size;
  upb_FindUnknownRet ret;

  const char* ptr = upb_Message_GetUnknown(msg, &size);
  upb_EpsCopyInputStream stream;
  upb_EpsCopyInputStream_Init(&stream, &ptr, size, true);

  while (!upb_EpsCopyInputStream_IsDone(&stream, &ptr)) {
    uint32_t tag;
    const char* unknown_begin = ptr;
    ptr = upb_WireReader_ReadTag(ptr, &tag);
    if (!ptr) return upb_FindUnknownRet_ParseError();
    if (field_number == upb_WireReader_GetFieldNumber(tag)) {
      ret.status = kUpb_FindUnknown_Ok;
      ret.ptr = upb_EpsCopyInputStream_GetAliasedPtr(&stream, unknown_begin);
      ptr = _upb_WireReader_SkipValue(ptr, tag, depth_limit, &stream);
      // Because we know that the input is a flat buffer, it is safe to perform
      // pointer arithmetic on aliased pointers.
      ret.len = upb_EpsCopyInputStream_GetAliasedPtr(&stream, ptr) - ret.ptr;
      return ret;
    }

    ptr = _upb_WireReader_SkipValue(ptr, tag, depth_limit, &stream);
    if (!ptr) return upb_FindUnknownRet_ParseError();
  }
  ret.status = kUpb_FindUnknown_NotPresent;
  ret.ptr = NULL;
  ret.len = 0;
  return ret;
}

static upb_DecodeStatus upb_Message_PromoteOne(upb_TaggedMessagePtr* tagged,
                                               const upb_MiniTable* mini_table,
                                               int decode_options,
                                               upb_Arena* arena) {
  upb_Message* empty = _upb_TaggedMessagePtr_GetEmptyMessage(*tagged);
  size_t unknown_size;
  const char* unknown_data = upb_Message_GetUnknown(empty, &unknown_size);
  upb_Message* promoted = upb_Message_New(mini_table, arena);
  if (!promoted) return kUpb_DecodeStatus_OutOfMemory;
  upb_DecodeStatus status = upb_Decode(unknown_data, unknown_size, promoted,
                                       mini_table, NULL, decode_options, arena);
  if (status == kUpb_DecodeStatus_Ok) {
    *tagged = _upb_TaggedMessagePtr_Pack(promoted, false);
  }
  return status;
}

upb_DecodeStatus upb_Message_PromoteMessage(upb_Message* parent,
                                            const upb_MiniTable* mini_table,
                                            const upb_MiniTableField* field,
                                            int decode_options,
                                            upb_Arena* arena,
                                            upb_Message** promoted) {
  const upb_MiniTable* sub_table =
      upb_MiniTable_GetSubMessageTable(mini_table, field);
  UPB_ASSERT(sub_table);
  upb_TaggedMessagePtr tagged =
      upb_Message_GetTaggedMessagePtr(parent, field, NULL);
  upb_DecodeStatus ret =
      upb_Message_PromoteOne(&tagged, sub_table, decode_options, arena);
  if (ret == kUpb_DecodeStatus_Ok) {
    *promoted = upb_TaggedMessagePtr_GetNonEmptyMessage(tagged);
    upb_Message_SetMessage(parent, mini_table, field, *promoted);
  }
  return ret;
}

upb_DecodeStatus upb_Array_PromoteMessages(upb_Array* arr,
                                           const upb_MiniTable* mini_table,
                                           int decode_options,
                                           upb_Arena* arena) {
  void** data = _upb_array_ptr(arr);
  size_t size = arr->size;
  for (size_t i = 0; i < size; i++) {
    upb_TaggedMessagePtr tagged;
    memcpy(&tagged, &data[i], sizeof(tagged));
    if (!upb_TaggedMessagePtr_IsEmpty(tagged)) continue;
    upb_DecodeStatus status =
        upb_Message_PromoteOne(&tagged, mini_table, decode_options, arena);
    if (status != kUpb_DecodeStatus_Ok) return status;
    memcpy(&data[i], &tagged, sizeof(tagged));
  }
  return kUpb_DecodeStatus_Ok;
}

upb_DecodeStatus upb_Map_PromoteMessages(upb_Map* map,
                                         const upb_MiniTable* mini_table,
                                         int decode_options, upb_Arena* arena) {
  size_t iter = kUpb_Map_Begin;
  upb_MessageValue key, val;
  while (upb_Map_Next(map, &key, &val, &iter)) {
    if (!upb_TaggedMessagePtr_IsEmpty(val.tagged_msg_val)) continue;
    upb_DecodeStatus status = upb_Message_PromoteOne(
        &val.tagged_msg_val, mini_table, decode_options, arena);
    if (status != kUpb_DecodeStatus_Ok) return status;
    upb_Map_SetEntryValue(map, iter, val);
  }
  return kUpb_DecodeStatus_Ok;
}
