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

#include "third_party/upb/protos_generator/gen_utils.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "upbc/keywords.h"

namespace protos_generator {

namespace protobuf = ::google::protobuf;

std::string DotsToColons(const std::string& name) {
  return StringReplace(name, ".", "::", true);
}

std::string Namespace(const std::string& package) {
  if (package.empty()) return "";
  return "::" + DotsToColons(package);
}

// Return the qualified C++ name for a file level symbol.
std::string QualifiedFileLevelSymbol(const protobuf::FileDescriptor* file,
                                     const std::string& name) {
  if (file->package().empty()) {
    return absl::StrCat("::", name);
  }
  // Append ::protos postfix to package name.
  return absl::StrCat(Namespace(file->package()), "::protos::", name);
}

std::string ClassName(const protobuf::Descriptor* descriptor) {
  const protobuf::Descriptor* parent = descriptor->containing_type();
  std::string res;
  if (parent) res += ClassName(parent) + "_";
  absl::StrAppend(&res, descriptor->name());
  return ::upbc::ResolveKeywordConflict(res);
}

std::string QualifiedClassName(const protobuf::Descriptor* descriptor) {
  return QualifiedFileLevelSymbol(descriptor->file(), ClassName(descriptor));
}

std::string CppSourceFilename(const google::protobuf::FileDescriptor* file) {
  return StripExtension(file->name()) + ".upb.proto.cc";
}

std::string UpbCFilename(const google::protobuf::FileDescriptor* file) {
  return StripExtension(file->name()) + ".upb.h";
}

std::string ForwardingHeaderFilename(const google::protobuf::FileDescriptor* file) {
  return StripExtension(file->name()) + ".upb.fwd.h";
}

std::string CppHeaderFilename(const google::protobuf::FileDescriptor* file) {
  return StripExtension(file->name()) + ".upb.proto.h";
}

std::string NamespaceFromPackageName(absl::string_view package_name) {
  return absl::StrCat(absl::StrReplaceAll(package_name, {{".", "::"}}),
                      "::protos");
}

void WriteStartNamespace(const protobuf::FileDescriptor* file, Output& output) {
  // Skip namespace generation if package name is not specified.
  if (file->package().empty()) {
    return;
  }

  output("namespace $0 {\n\n", NamespaceFromPackageName(file->package()));
}

void WriteEndNamespace(const protobuf::FileDescriptor* file, Output& output) {
  if (file->package().empty()) {
    return;
  }
  output("} //  namespace $0\n\n", NamespaceFromPackageName(file->package()));
}

}  // namespace protos_generator
