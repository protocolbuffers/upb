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

// Miscellaneous functions for processing ASCII strings.

#ifndef UPB_IO_ASCII_H_
#define UPB_IO_ASCII_H_

// Must be last.
#include "upb/port_def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Given a char, interpret it as a numeric digit and return its value.
// This supports any number base <= 36.
// Represents integer values of digits.
// Uses 36 to indicate an invalid character since we support bases up to 36.
int upb_Ascii_DigitValue(char digit);

char upb_Ascii_TranslateEscape(char c);

UPB_INLINE bool upb_Ascii_IsLetter(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_');
}

UPB_INLINE bool upb_Ascii_IsDigit(char c) { return '0' <= c && c <= '9'; }

UPB_INLINE bool upb_Ascii_IsOctalDigit(char c) { return '0' <= c && c <= '7'; }

UPB_INLINE bool upb_Ascii_IsHexDigit(char c) {
  return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
         ('A' <= c && c <= 'F');
}

UPB_INLINE bool upb_Ascii_IsAlphanumeric(char c) {
  return upb_Ascii_IsLetter(c) || upb_Ascii_IsDigit(c);
}

UPB_INLINE bool upb_Ascii_IsWhitespaceNoNewline(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

UPB_INLINE bool upb_Ascii_IsWhitespace(char c) {
  return c == '\n' || upb_Ascii_IsWhitespaceNoNewline(c);
}

UPB_INLINE bool upb_Ascii_IsEscape(char c) {
  return c == 'a' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' ||
         c == 'v' || c == '\\' || c == '?' || c == '\'' || c == '\"';
}

UPB_INLINE bool upb_Ascii_IsUnprintable(char c) { return '\0' < c && c < ' '; }

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "upb/port_undef.inc"

#endif /* UPB_IO_ASCII_H_ */
