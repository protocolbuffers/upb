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

#include "third_party/upb/protos_generator/gen_messages.h"

#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/descriptor.h"
#include "third_party/upb/protos_generator/gen_utils.h"
#include "third_party/upb/protos_generator/output.h"

namespace protos_generator {

namespace protobuf = ::google::protobuf;

void WriteModelAccessDeclaration(const protobuf::Descriptor* descriptor,
                                 Output& output);
void WriteModelPublicDeclaration(const protobuf::Descriptor* descriptor,
                                 Output& output);
void WriteModelProxyDeclaration(const protobuf::Descriptor* descriptor,
                                Output& output);
void WriteModelCProxyDeclaration(const protobuf::Descriptor* descriptor,
                                 Output& output);
void WriteInternalForwardDeclarationsInHeader(
    const protobuf::Descriptor* message, Output& output);

// Writes message class declarations into .upb.proto.h.
//
// For each proto Foo, FooAccess and FooProxy/FooCProxy are generated
// that are exposed to users as Foo , Ptr<Foo> and Ptr<const Foo>.
void WriteMessageClassDeclarations(const protobuf::Descriptor* descriptor,
                                   Output& output) {
  if (IsMapEntryMessage(descriptor)) {
    // Skip map entry generation. Low level accessors for maps are
    // generated that don't require a separate map type.
    return;
  }

  output("namespace internal {\n");
  WriteModelAccessDeclaration(descriptor, output);
  output("\n");
  WriteInternalForwardDeclarationsInHeader(descriptor, output);
  output("\n");
  output("}  // namespace internal\n");
  WriteModelPublicDeclaration(descriptor, output);
  output("namespace internal {\n");
  WriteModelProxyDeclaration(descriptor, output);
  WriteModelCProxyDeclaration(descriptor, output);
  output("}  // namespace internal\n");
}

void WriteModelAccessDeclaration(const protobuf::Descriptor* descriptor,
                                 Output& output) {
  // TODO(b/237399867) : Write class ModelAccess

  output(
      R"cc(
        class $0Access {
         private:
          $0Access() {}
          friend class $1;
          friend class $0Proxy;
          friend class $0CProxy;
        };
      )cc",
      ClassName(descriptor), QualifiedClassName(descriptor));
}

void WriteModelPublicDeclaration(const protobuf::Descriptor* descriptor,
                                 Output& output) {
  // TODO(b/237399867) : Write class Model : private internal::ModelAccess

  output(
      R"cc(
        class $0 final : private internal::$0Access {
         public:
          $0() {}
        };
      )cc",
      ClassName(descriptor));
}

void WriteModelProxyDeclaration(const protobuf::Descriptor* descriptor,
                                Output& output) {
  // TODO(b/237399867) : Write class ModelProxy : private internal::ModelAccess
}
void WriteModelCProxyDeclaration(const protobuf::Descriptor* descriptor,
                                 Output& output) {
  // TODO(b/237399867) : Write class ModelCProxy : private internal::ModelAccess
}

void WriteMessageImplementation(const protobuf::Descriptor* descriptor,
                                Output& output) {
  // TODO(b/237399867) : Add non-inline ModelAccess/Proxy/CProxy members.
}

void WriteInternalForwardDeclarationsInHeader(
    const protobuf::Descriptor* message, Output& output) {
  // Write declaration for internal re-usable default_instance without
  // leaking implementation.
  output(
      R"cc(
        struct $0DefaultTypeInternal;
        extern $0DefaultTypeInternal _$0_default_instance_;
      )cc",
      ClassName(message));
}

}  // namespace protos_generator
