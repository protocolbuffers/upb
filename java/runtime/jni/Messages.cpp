/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name Meta nor the 
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "Messages.h"
#include <jni.h>
#include <stdint.h>
#include <upb/msg_internal.h>
#include <upb/upb.h>
#include <array>
#include <cstring>
#include "upb/decode.h"
#include "upb/encode.h"
#include "upb/upb.h"

// MUST be last included header.
#include "upb/port_def.inc"

namespace {
jstring _Nullable utf8ToJString(JNIEnv* env, const char* utf8) {
  if (!utf8) {
    return NULL;
  }

  jstring result = NULL;

  size_t utf8Length;
  size_t modifiedUtf8StringLength;
  uint8_t* modifiedUtf8String;

  utf8ToModifiedUtf8(
      (uint8_t*)utf8,
      &utf8Length,
      &modifiedUtf8String,
      &modifiedUtf8StringLength);

  if (modifiedUtf8String == NULL) {
    result = env->NewStringUTF((char*)utf8);
  } else {
    result = env->NewStringUTF((char*)modifiedUtf8String);
    free(modifiedUtf8String);
  }

  assertNoPendingJniException(env);
  return result;
}

} // namespace

namespace Messages {

jlong _upb_Message_New(
    JNIEnv* env,
    jobject /* thisz */,
    jlong minitablesPointer,
    jint fileIndex,
    jint msgIndex,
    jlong arenaPointer) {
  return (jlong)::_upb_Message_New(
      (((const upb_MiniTable_File**)minitablesPointer)[fileIndex])
          ->msgs[msgIndex],
      (upb_Arena*)arenaPointer);
}

jstring _Nullable get_UPB_PTR_AT_String_internal(
    JNIEnv* env,
    jobject /* thisz */,
    jlong stringViewPointer) {
  upb_StringView* str = ((upb_StringView*)(stringViewPointer));
  char* null_terminated = new char[str->size + 1];
  memcpy(null_terminated, str->data, str->size);
  null_terminated[str->size] = 0;
  return utf8ToJString(env, null_terminated);
}

void set_UPB_PTR_AT_String_internal(
    JNIEnv* env,
    jobject /* thisz */,
    jlong arenaPointer,
    jlong stringViewPointer,
    jbyteArray utf8String) {
  jint len = env->GetArrayLength(utf8String);
  upb_StringView* str = ((upb_StringView*)(stringViewPointer));
  void* dest = upb_Arena_Malloc((upb_Arena*)arenaPointer, len);
  ;
  if (dest == NULL) {
    // TODO: throw
  }
  str->data = (const char*)dest;
  str->size = len;

  void* data = env->GetPrimitiveArrayCritical(utf8String, NULL);
  ::memcpy(dest, data, len);
  env->ReleasePrimitiveArrayCritical(utf8String, data, JNI_ABORT);
}

jboolean getIs64(JNIEnv* env, jobject /* thisz */) {
#if UINTPTR_MAX == 0xffffffff
  return false;
#else
  return true;
#endif
}

void _upb_sethas(
    JNIEnv* env,
    jobject /* thisz */,
    jlong messagePointer,
    jint index) {
  ::_upb_sethas((const upb_Message*)messagePointer, index);
}

void upb_Decode(
    JNIEnv* env,
    jobject /* thisz */,
    jbyteArray buf,
    jlong msgPointer,
    jlong minitablesPointer,
    jint fileIndex,
    jint msgIndex,
    jlong arenaPointer) {
  jint len = env->GetArrayLength(buf);
  void* data = env->GetPrimitiveArrayCritical(buf, NULL);

  // upb_DecodeStatus result =
  ::upb_Decode(
      (const char*)data,
      len,
      (upb_Message*)msgPointer,
      (((const upb_MiniTable_File**)minitablesPointer)[fileIndex])
          ->msgs[msgIndex],
      nullptr /* extension registry */,
      0 /* options */,
      (upb_Arena*)arenaPointer);

  env->ReleasePrimitiveArrayCritical(buf, data, JNI_ABORT);

  // TODO: handle result
}

jbyteArray _Nullable upb_Encode(
    JNIEnv* env,
    jobject /* thisz */,
    jlong msgPointer,
    jlong minitablesPointer,
    jint fileIndex,
    jint msgIndex,
    jlong arenaPointer) {
  size_t size;
  char* result = ::upb_Encode(
      (upb_Message*)msgPointer,
      (((const upb_MiniTable_File**)minitablesPointer)[fileIndex])
          ->msgs[msgIndex],
      0 /* options */,
      (upb_Arena*)arenaPointer,
      &size);

  if (!result || size == 0) {
    return NULL;
  }

  jbyteArray ret = env->NewByteArray(size);
  if (ret == NULL) {
    return NULL; //  out of memory error thrown
  }
  env->SetByteArrayRegion(ret, 0, size, (jbyte*)result);
  return ret;
}

namespace {
auto constexpr kClassName = "com/facebook/upb/runtime/Messages";

std::array<JNINativeMethod, 7> methods = {{
    {"_upb_Message_New", "(JIIJ)J", (void*)Messages::_upb_Message_New},
    {"UPB_PTR_AT_String_internal",
     "(J)Ljava/lang/String;",
     (void*)Messages::get_UPB_PTR_AT_String_internal},
    {"UPB_PTR_AT_String_internal",
     "(JJ[B)V",
     (void*)Messages::set_UPB_PTR_AT_String_internal},
    {"getIs64", "()Z", (void*)Messages::getIs64},
    {"_upb_sethas", "(JI)V", (void*)Messages::_upb_sethas},
    {"upb_Decode", "([BJJIIJ)V", (void*)Messages::upb_Decode},
    {"upb_Encode", "(JJIIJ)[B", (void*)Messages::upb_Encode},
}};

} // namespace

void registerNatives(JNIEnv* env) {
  jclass clazz = env->FindClass(kClassName);
  assertNoPendingJniException(env);
  env->RegisterNatives(clazz, methods, methods.size());
  assertNoPendingJniException(env);
}

} // namespace Messages

#include "upb/port_undef.inc"