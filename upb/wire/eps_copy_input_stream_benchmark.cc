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

#include <benchmark/benchmark.h>
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/parse_context.h"
#include "upb/io/chunked_input_stream.h"
#include "upb/upb.hpp"
#include "upb/wire/eps_copy_input_stream.h"

static void BM_Upb_EmptyInit(benchmark::State& state) {
  for (auto _ : state) {
    upb_EpsCopyInputStream stream;
    const char* ptr =
        upb_EpsCopyInputStream_Init(&stream, NULL, 0, NULL, false);
    benchmark::DoNotOptimize(ptr);
  }
}
BENCHMARK(BM_Upb_EmptyInit);

static void BM_Cpp_EmptyInit(benchmark::State& state) {
  for (auto _ : state) {
    google::protobuf::internal::EpsCopyInputStream stream(false);
    const char* ptr = stream.InitFrom(absl::string_view(NULL, 0));
    benchmark::DoNotOptimize(ptr);
  }
}
BENCHMARK(BM_Cpp_EmptyInit);

static void BM_Upb_FlatString(benchmark::State& state) {
  std::string data(state.range(0), ' ');
  for (auto _ : state) {
    upb_EpsCopyInputStream stream;
    const char* ptr = upb_EpsCopyInputStream_Init(&stream, data.data(),
                                                  data.size(), NULL, false);
    upb_EpsCopyInputStream_IsDone(&stream, &ptr);
    ptr += (data.size() - 5);
    upb_EpsCopyInputStream_IsDone(&stream, &ptr);
    benchmark::DoNotOptimize(ptr);
  }
}
BENCHMARK(BM_Upb_FlatString)->Arg(5)->Arg(64);

static void BM_Cpp_FlatString(benchmark::State& state) {
  std::string data(state.range(0), ' ');
  for (auto _ : state) {
    google::protobuf::internal::EpsCopyInputStream stream(false);
    const char* ptr = stream.InitFrom(data);
    stream.DoneWithCheck(&ptr, INT_MIN);
    ptr += (data.size() - 5);
    stream.DoneWithCheck(&ptr, INT_MIN);
    benchmark::DoNotOptimize(ptr);
  }
}
BENCHMARK(BM_Cpp_FlatString)->Arg(5)->Arg(64);

static void BM_Upb_ChunkedString(benchmark::State& state) {
  std::string data(256, ' ');
  int increment = state.range(1);
  int items = 0;
  for (auto _ : state) {
    upb::InlinedArena<256> arena;
    upb_ZeroCopyInputStream* zcis = upb_ChunkedInputStream_New(
        data.data(), data.size(), state.range(0), arena.ptr());
    upb_EpsCopyInputStream stream;
    const char* ptr =
        upb_EpsCopyInputStream_Init(&stream, NULL, 0, zcis, false);
    while (!upb_EpsCopyInputStream_IsDone(&stream, &ptr)) {
      ptr = stream.end + increment;
      items++;
    }
    benchmark::DoNotOptimize(ptr);
  }
  state.SetItemsProcessed(items);
}
BENCHMARK(BM_Upb_ChunkedString)->ArgPair(1, 8)->ArgPair(32, 3);

static void BM_Cpp_ChunkedString(benchmark::State& state) {
  std::string data(256, 'x');
  int increment = state.range(1);
  int items = 0;
  for (auto _ : state) {
    google::protobuf::io::ArrayInputStream zcis(data.data(), data.size(), state.range(0));
    google::protobuf::internal::EpsCopyInputStream stream(false);
    const char* ptr = stream.InitFrom(&zcis);
    while (!stream.DoneWithCheck(&ptr, INT_MIN)) {
      ptr = stream.buffer_end_ + increment;
      items++;
    }
    benchmark::DoNotOptimize(ptr);
  }
  state.SetItemsProcessed(items);
}
BENCHMARK(BM_Cpp_ChunkedString)->ArgPair(1, 8)->ArgPair(32, 3);

static void BM_Upb_CopyString(benchmark::State& state) {
  std::string data(256, ' ');
  std::string string_buf(state.range(1), ' ');
  int items = 0;
  for (auto _ : state) {
    upb::InlinedArena<256> arena;
    upb_ZeroCopyInputStream* zcis = upb_ChunkedInputStream_New(
        data.data(), data.size(), state.range(0), arena.ptr());
    upb_EpsCopyInputStream stream;
    const char* ptr =
        upb_EpsCopyInputStream_Init(&stream, NULL, 0, zcis, false);
    while (!upb_EpsCopyInputStream_IsDone(&stream, &ptr)) {
      ptr = upb_EpsCopyInputStream_Copy(&stream, ptr, string_buf.data(),
                                        string_buf.size());
      items++;
    }
    benchmark::DoNotOptimize(ptr);
  }
  state.SetItemsProcessed(items);
}
BENCHMARK(BM_Upb_CopyString)->ArgPair(1, 8)->ArgPair(32, 3);

static void BM_Cpp_CopyString(benchmark::State& state) {
  std::string data(256, 'x');
  std::string string_buf(state.range(1), ' ');
  int items = 0;
  for (auto _ : state) {
    google::protobuf::io::ArrayInputStream zcis(data.data(), data.size(), state.range(0));
    google::protobuf::internal::EpsCopyInputStream stream(false);
    const char* ptr = stream.InitFrom(&zcis);
    while (!stream.DoneWithCheck(&ptr, INT_MIN)) {
      ptr = stream.ReadString(ptr, string_buf.size(), &string_buf);
      items++;
    }
    benchmark::DoNotOptimize(ptr);
  }
  state.SetItemsProcessed(items);
}
BENCHMARK(BM_Cpp_CopyString)->ArgPair(1, 8)->ArgPair(32, 3);
