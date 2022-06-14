/*
 * Copyright (c) 2009-2022, Google LLC
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

#ifndef UPB_WIREDECODE_H_
#define UPB_WIREDECODE_H_

#include <inttypes.h>
#include <string.h>

#include "upb/upb.h"

// Must be last.
#include "upb/port_def.inc"

#ifdef __cplusplus
extern "C" {
#endif

UPB_INLINE uint32_t upb_TagField(uint32_t tag) { return tag >> 3; }

UPB_INLINE upb_WireType upb_TagType(uint32_t tag) {
  return (upb_WireType)(tag & 7);
}

UPB_INLINE const char* upb_WireDecode_32Bit(const char* ptr, const char* end,
                                            uint32_t* val) {
  if (UPB_UNLIKELY(end - ptr < 4)) return NULL;
  if (val != NULL) memcpy(val, ptr, 4);
  return ptr + 4;
}

UPB_INLINE const char* upb_WireDecode_64Bit(const char* ptr, const char* end,
                                            uint64_t* val) {
  if (UPB_UNLIKELY(end - ptr < 8)) return NULL;
  if (val != NULL) memcpy(val, ptr, 8);
  return ptr + 8;
}

UPB_INLINE const char* upb_WireDecode_Skip(const char* ptr, const char* end,
                                           int count) {
  if (UPB_UNLIKELY(end - ptr < count)) return NULL;
  return ptr + count;
}

UPB_INLINE const char* upb_WireDecode_Varint(const char* ptr, const char* end,
                                             uint64_t* val) {
  uint64_t emit = 0;
  for (int shift = 0; shift < 64 && ptr < end; shift += 7) {
    uint64_t byte = (uint8_t)*ptr++;
    emit |= (byte & 0x7f) << shift;
    if (UPB_LIKELY(!(byte & 0x80))) {  // Most varints are just a single byte.
      if (val != NULL) *val = emit;
      return ptr;
    }
  }
  return NULL;
}

// A tag is a varint that must fit within 32 bits after decoding.
UPB_INLINE const char* upb_WireDecode_Tag(const char* ptr, const char* end,
                                          uint32_t* val) {
  uint64_t emit;
  const char* next = upb_WireDecode_Varint(ptr, end, &emit);
  if (UPB_UNLIKELY(!next || next - ptr > 5 || emit > UINT32_MAX)) {
    return NULL;  // Malformed.
  }
  if (val != NULL) *val = (uint32_t)emit;
  return next;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "upb/port_undef.inc"

#endif /* UPB_WIREDECODE_H_ */
