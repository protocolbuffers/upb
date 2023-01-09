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

#include "upb/wire/eps_copy_input_stream.h"

#include "upb/wire/reader.h"

// Must be last.
#include "upb/port/def.inc"

// We frequently reference this constant in this file.
#define kSlopBytes kUpb_EpsCopyInputStream_SlopBytes

#if 0
static const char* _upb_EpsCopyInputStream_NoOpCallback(
    upb_EpsCopyInputStream* e, const char* old_end, const char* new_start) {
  return new_start;
}

const char* _upb_EpsCopyInputStream_IsDoneFallbackNoCallback(
    upb_EpsCopyInputStream* e, const char* ptr, int overrun) {
  return _upb_EpsCopyInputStream_IsDoneFallbackInline(
      e, ptr, overrun, _upb_EpsCopyInputStream_NoOpCallback);
}

const char* _upb_EpsCopyInputStream_CopyFallback(upb_EpsCopyInputStream* e,
                                                 const char* ptr, char* to,
                                                 int size) {
  // Data is split across multiple buffers.  First copy data to end.
  const char* end = to + size;
  size_t avail = e->limit_ptr - ptr + kSlopBytes;
  memcpy(to, ptr, avail);
  to += avail;
  int advance = size - (e->limit_ptr - ptr);
  UPB_ASSERT(size >= avail);
  UPB_ASSERT(e->next_chunk == NULL || e->next_chunk_size >= kSlopBytes);

  DBG("  - _EpsCopyInputStream_CopyFallback(), avail=%zu\n", avail);

  ptr = e->next_chunk ? e->next_chunk + kSlopBytes : NULL;
  avail = ptr ? e->next_chunk_size - kSlopBytes : 0;
  e->next_chunk = NULL;

  while (to < end) {
    if (avail == 0 && !_upb_EpsCopyInputStream_NextBufferRaw(e, &ptr, &avail)) {
      return NULL;
    }
    size_t copy = UPB_MIN(avail, end - to);
    memcpy(to, ptr, copy);
    DBG("    - copy chunk=%zu\n", copy);
    to += copy;
    ptr += copy;
    avail -= copy;
  }

  e->limit -= advance;
  ptr = _upb_EpsCopyInputStream_ResetBuffer(e, ptr, avail);
  e->limit_ptr = e->end + UPB_MIN(0, e->limit);
  DBG("  - EpsCopyInputStream_CopyFallback(), returning=%p, limit=%d, "
      "advance=%d\n",
      ptr, e->limit, advance);
  return ptr;
}

static char* _upb_EpsCopyInputStream_AppendNextChunk(upb_EpsCopyInputStream* e,
                                                     char* ptr) {
  UPB_ASSERT(e->next_chunk);
  UPB_ASSERT(e->next_chunk_size < kSlopBytes);
  DBG("  - AppendNextChunk, data=%.*s, size=%zu\n", (int)e->next_chunk_size,
      e->next_chunk, e->next_chunk_size);
  upb_EpsCopyInputStream_SmallMemcpy(ptr, e->next_chunk, e->next_chunk_size);
  e->next_chunk = NULL;
  return ptr + e->next_chunk_size;
}

UPB_NOINLINE
const char* _upb_EpsCopyInputStream_FillPatchBufferSlow(
    upb_EpsCopyInputStream* e, int have) {
  char* start = e->patch + have;
  char* ptr = start;
  char* end = e->patch + kSlopBytes;
  DBG("- FillPatchBuffer(have=%d), limit=%d\n", have, e->limit);
  while (1) {
    ptr = _upb_EpsCopyInputStream_AppendNextChunk(e, ptr);
    if (ptr > end) {
      // We have enough data to provide kSlopBytes of overflow, so we are done.
      //
      // We could theoretically keep filling as long as there is more space in
      // the patch buffer. However if we get a big chunk in the second half of
      // the patch buffer, we would no longer be able to provide the invariant
      // that e->next_chunk corresponds to the same stream position as e->end,
      // which would complicate the logic and probably penalize the speed of the
      // more common case where stream buffers are >kSlopBytes bytes long.
      e->end = ptr - kSlopBytes;
      e->limit -= e->end - start;
      DBG("  - We found enough small buffers, ptr - e->patch = %d, e->end - "
          "start=%d, e->end - e->patch=%d\n",
          (int)(ptr - e->patch), (int)(e->end - start),
          (int)(e->end - e->patch));
      break;
    }
    if (!_upb_EpsCopyInputStream_NextBuffer(e)) {
      // We are at end-of-stream, so all of the remaining slop bytes will have
      // indeterminate value.
      e->end = ptr;
      if (!_upb_EpsCopyInputStream_SetEndOfStream(e, NULL, NULL)) return NULL;
      DBG("  - End of stream, leaving remaining bytes undefined\n");
      break;
    }
    if (e->next_chunk_size >= kSlopBytes) {
      // We got a big buffer (enough to satisfy all the slop and then some) so
      // we are done, and moreover we can leave e->next_chunk for the next time
      // we need more data.
      DBG("  - Oh we are set, buffer size=%zu\n", e->next_chunk_size);
      memcpy(ptr, e->next_chunk, kSlopBytes);
      e->end = ptr;
      e->limit -= ptr - start;
      break;
    }
  }
  e->limit_ptr = e->end + UPB_MIN(e->limit, 0);
  e->aliasing_delta = kUpb_EpsCopyInputStream_CannotAlias;
  DBG("- Finishing FillPatchBuffer(), available data=%d\n",
      (int)(e->end - e->patch));
  return e->patch;
}

#undef kSlopBytes

#endif
