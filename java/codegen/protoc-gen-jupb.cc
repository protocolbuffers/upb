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

/*

Usage example: 

protoc.exe
    --plugin=protoc-gen-jupb=protoc-gen-jupb.exe
    --jupb_out=java_dispatcher_package_name=java.com.facebook.foo.bar,java_dispatcher_name=rule_name:$OUT
    -I${SRCDIR}
    SRC1 SRC2 ...

This generates 
1. An outer Java class with 1x inner class per message
2. JNI bindings (1x Java and 1x .c file)

*/

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/compiler/plugin.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/wire_format.h"
#include "upb/mini_table.hpp"
#include "upb/upb.hpp"
#include "upbc/common.h"
#include "upbc/protoc-gen-upb.h"

// Must be last.
#include "upb/port_def.inc"

namespace jupbc {
namespace {

namespace protoc = ::google::protobuf::compiler;
namespace protobuf = ::google::protobuf;
using namespace upbc;

std::string ToJavaIdent(absl::string_view str) {
  return absl::StrReplaceAll(str, {{".", "_"}, {"/", "_"}, {"-", "_"}});
}

std::string ToJavaClassName(absl::string_view str) {
  std::string result = ToJavaIdent(str);
  result[0] = toupper(result[0]);
  return result;
}

std::string GeneratedJavaPackageName(
    const std::string& java_package_name,
    const std::string& java_name) {
  return absl::StrCat(
      "com.facebook.jupbgen.", ToJavaIdent(java_package_name), ".", java_name);
}

std::string DispatcherJavaFqName(
    const std::string& java_package_name,
    const std::string& java_name) {
  return GeneratedJavaPackageName(java_package_name, java_name) + "." +
      "MsginitGetter";
}

std::string WrapperClassName(const google::protobuf::FileDescriptor* file) {
  return ToJavaClassName(StripExtension(file->name()));
}

std::string FullMesageNameToJavaName(const protobuf::Descriptor* message) {
  absl::string_view msg_name = message->full_name();
  absl::ConsumePrefix(&msg_name, message->file()->package());
  return ToJavaIdent(msg_name.substr(1));
}

std::string JavaType(const protobuf::FieldDescriptor* field) {
  switch (field->cpp_type()) {
    // case protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
    //   // std::string maybe_struct =
    //   //     field->file() != field->message_type()->file() ? "struct " : "";
    //   // return maybe_struct + MessageName(field->message_type()) + "*";
    //   return "Object";
    // }
    case protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return "boolean";
    case protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return "float";
    case protobuf::FieldDescriptor::CPPTYPE_INT32:
    case protobuf::FieldDescriptor::CPPTYPE_ENUM:
      return "int";
    case protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return "int";
    case protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return "double";
    case protobuf::FieldDescriptor::CPPTYPE_INT64:
      return "long";
    case protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return "long";
    case protobuf::FieldDescriptor::CPPTYPE_STRING:
      return "String";
    default:
      fprintf(stderr, "Unexpected type");
      abort();
  }
}

std::string FieldDefault(const protobuf::FieldDescriptor* field) {
  switch (field->cpp_type()) {
    case protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return "null";
    case protobuf::FieldDescriptor::CPPTYPE_STRING:
      return absl::StrCat(
          "\"", absl::CEscape(field->default_value_string()), "\"");
    case protobuf::FieldDescriptor::CPPTYPE_INT32:
      return absl::StrCat(field->default_value_int32());
    case protobuf::FieldDescriptor::CPPTYPE_INT64:
      return absl::StrCat(field->default_value_uint64(), "L");
    case protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return absl::StrCat(field->default_value_uint32());
    case protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return absl::StrCat(field->default_value_uint64(), "L");
    case protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return absl::StrCat(field->default_value_float(), "f");
    case protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return absl::StrCat(field->default_value_double());
    case protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return field->default_value_bool() ? "true" : "false";
    case protobuf::FieldDescriptor::CPPTYPE_ENUM:
      // Use a number instead of a symbolic name so that we don't require
      // this enum's header to be included.
      return absl::StrCat(field->default_value_enum()->number());
  }
  ABSL_ASSERT(false);
  return "XXX";
}

void GenerateScalarGetters(
    const protobuf::FieldDescriptor* field,
    const FileLayout& layout,
    Output& output) {
  if (field->cpp_type() == protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
    // TODO
    return;
  }

  if (HasNonZeroDefault(field)) {
    output(
        R"cc(
          public $0 get_$1() {
            return has_$1() ? Messages.UPB_PTR_AT_$0(msg + $2) : $3;
          }
        )cc",
        JavaType(field),
        field->name(),
        layout.GetFieldOffset(field),
        FieldDefault(field));
  } else {
    output(
        R"cc(
          public $0 get_$1() {
            return Messages.UPB_PTR_AT_$0(msg + $2);
          }
        )cc",
        JavaType(field),
        field->name(),
        layout.GetFieldOffset(field));
  }
}

void GenerateGetters(
    const protobuf::FieldDescriptor* field,
    const FileLayout& layout,
    Output& output) {
  // if (field->is_map()) {
  //   GenerateMapGetters(field, layout, msg_name, output);
  // } else if (field->containing_type()->options().map_entry()) {
  //   GenerateMapEntryGetters(field, msg_name, output);
  // } else if (field->is_repeated()) {
  //   GenerateRepeatedGetters(field, layout, msg_name, output);
  // } else if (field->real_containing_oneof()) {
  //   GenerateOneofGetters(field, layout, msg_name, output);
  // } else {
  GenerateScalarGetters(field, layout, output);
  // }
}

void GenerateNonRepeatedSetters(
    const protobuf::FieldDescriptor* field,
    const FileLayout& layout,
    Output& output) {
  if (field == field->containing_type()->map_key()) {
    // Key cannot be mutated.
    return;
  }

  if (field->cpp_type() == protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
    // TODO
    return;
  }

  // The common function signature for all setters.  Varying
  // implementations follow.
  output("public void set_$0($1 value) {\n", field->name(), JavaType(field));

  // if (field == field->containing_type()->map_value()) {
  //   output(
  //       "  _upb_msg_map_set_value(msg, &value, $0);\n"
  //       "}\n",
  //       field->cpp_type() == protobuf::FieldDescriptor::CPPTYPE_STRING
  //           ? "0"
  //           : "sizeof(" + CType(field) + ")");
  // } else if (field->real_containing_oneof()) {
  //   output(
  //       "  UPB_WRITE_ONEOF(msg, $0, $1, value, $2, $3);\n"
  //       "}\n",
  //       CType(field),
  //       layout.GetFieldOffset(field),
  //       layout.GetOneofCaseOffset(field->real_containing_oneof()),
  //       field->number());
  // } else {
  if (layout.HasHasbit(field)) {
    output("  Messages._upb_sethas(msg, $0);\n", layout.GetHasbitIndex(field));
  }
  output(
      "  Messages.UPB_PTR_AT_$0($2 msg + $1, value);\n"
      "}\n",
      JavaType(field),
      layout.GetFieldOffset(field),
      field->cpp_type() == protobuf::FieldDescriptor::CPPTYPE_STRING
          ? "arena.pointer, "
          : "");
  // }

  // Message fields also have a Msg_mutable_foo() accessor that will create
  // the sub-message if it doesn't already exist.
  // if (field->cpp_type() == protobuf::FieldDescriptor::CPPTYPE_MESSAGE &&
  //     !field->containing_type()->options().map_entry()) {
  //   output(
  //       R"cc(
  //         UPB_INLINE struct $0* $1_mutable_$2($1* msg, upb_Arena* arena) {
  //           struct $0* sub = (struct $0*)$1_$2(msg);
  //           if (sub == NULL) {
  //             sub = (struct $0*)_upb_Message_New(&$3, arena);
  //             if (!sub) return NULL;
  //             $1_set_$2(msg, sub);
  //           }
  //           return sub;
  //         }
  //       )cc",
  //       MessageName(field->message_type()),
  //       msg_name,
  //       field->name(),
  //       MessageInit(field->message_type()));
  // }
}

void GenerateSetters(
    const protobuf::FieldDescriptor* field,
    const FileLayout& layout,
    Output& output) {
  // if (field->is_map()) {
  //   GenerateMapSetters(field, layout, msg_name, output);
  // } else if (field->is_repeated()) {
  //   GenerateRepeatedSetters(field, layout, msg_name, output);
  // } else {
  GenerateNonRepeatedSetters(field, layout, output);
  // }
}

void WriteMessage(
    const protobuf::Descriptor* message,
    const FileLayout& layout,
    Output& output,
    size_t file_index,
    size_t message_index) {
  std::string msg_java_name = FullMesageNameToJavaName(message);
  std::string fields_array_ref = "NULL";
  std::string submsgs_array_ref = "NULL";
  std::string subenums_array_ref = "NULL";
  const upb_MiniTable* mt_32 = layout.GetMiniTable32(message);
  const upb_MiniTable* mt_64 = layout.GetMiniTable64(message);
  std::vector<std::string> subs;

  output("  public static class $0 extends UpbMessage {\n", msg_java_name);
  output("\n");
  output("    public $0(Arena arena) {\n", msg_java_name);
  output("      super(arena, ", msg_java_name);
  output(
      "      Messages._upb_Message_New(MsginitGetter.minitables(), $0, $1, arena.pointer));\n",
      file_index,
      message_index);
  output("    }\n", msg_java_name);

  // Write fields
  for (auto field : FieldNumberOrder(message)) {
    // GenerateHazzer(field, layout, msg_name, output);
    // if (field->is_repeated()) {
    //   GenerateRepeatedClear(field, layout, msg_name, output);
    // } else {
    //   GenerateClear(field, layout, msg_name, output);
    // }
    GenerateGetters(field, layout, output);
  }

  output("\n");

  for (auto field : FieldNumberOrder(message)) {
    GenerateSetters(field, layout, output);
  }

  output("    public void decode(byte[] buf) {\n");
  output(
      "      Messages.upb_Decode(buf, msg, MsginitGetter.minitables(), $0, $1, arena.pointer);\n",
      file_index,
      message_index);
  output("    }\n");

  output("    public byte[] encode() {\n");
  output(
      "      return Messages.upb_Encode(msg, MsginitGetter.minitables(), $0, $1, arena.pointer);\n",
      file_index,
      message_index);
  output("    }\n");

  output("\n");

  output("  }\n");
}

void WriteJavaFile(
    const FileLayout& layout,
    Output& output,
    const std::string& dispatcherJavaFqName,
    size_t file_index) {
  const protobuf::FileDescriptor* file = layout.descriptor();
  EmitFileWarning(file, output);
  output("package com.facebook.jupbgen.$0;\n\n", ToJavaIdent(file->package()));
  output("import $0;\n", dispatcherJavaFqName);
  output("import java.lang.String;\n");
  output("import com.facebook.upb.runtime.Arena;\n");
  output("import com.facebook.upb.runtime.Messages;\n");
  output("import com.facebook.upb.runtime.UpbMessage;\n");
  output("import static com.facebook.upb.runtime.Messages.UPB_SIZE;\n");
  output("\n");
  output("public class $0 {\n", WrapperClassName(file));

  std::vector<const protobuf::Descriptor*> file_messages = SortedMessages(file);

  if (file_messages.empty()) {
    return;
  }

  for (size_t msg_index = 0; msg_index < file_messages.size(); msg_index++) {
    WriteMessage(
        file_messages[msg_index], layout, output, file_index, msg_index);
  }

  output("}\n");
}

void GenerateJavaDispatcher(
    protoc::GeneratorContext* context,
    const std::string& java_package_name,
    const std::string& java_name) {
  std::unique_ptr<protobuf::io::ZeroCopyOutputStream> java_dispatcher_stream(
      context->Open(java_name + "/MsginitGetter.java"));
  Output output(java_dispatcher_stream.get());

  output(
      "package $0;\n", GeneratedJavaPackageName(java_package_name, java_name));
  output("\n");
  output("import com.facebook.soloader.SoLoader;");
  output("\n");
  output("public class MsginitGetter {\n");
  output("  static {\n");
  output(
      // https://github.com/facebook/SoLoader/blob/90084463c550b537501524f00153575d010b2f21/java/com/facebook/soloader/SoLoader.java#L73
      "    SoLoader.loadLibrary(\"$0\");\n",
      ToJavaIdent(java_package_name + "__" + java_name));
  output("  }\n");
  output("    private static long minitablesPointer;\n");
  output("    public static long minitables() {\n");
  output(
      "      if (minitablesPointer == 0) minitablesPointer = getMessageMinitablesPointer_internal();\n"
      "      return minitablesPointer;\n");
  output("    }\n");
  output(
      "  private static native long getMessageMinitablesPointer_internal();\n");
  output("}\n");
}

void WriteJni(Output& output, const std::string& dispatcherJavaFqName) {
  output(
      R"cc(
        static jlong getMessageMinitablesPointer_internal(JNIEnv* env, jclass clazz) {
          return (jlong)file_MiniTables;
        }

        static JNINativeMethod methods[] = {
            {"getMessageMinitablesPointer_internal",
             "()J",
             (void*)getMessageMinitablesPointer_internal},
        };

        JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* _Nullable reserved) {
          JNIEnv* env;

          if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
            return JNI_ERR;
          }

          jclass clazz = (jclass)(*env)->FindClass(env, "$0");
          if (clazz == NULL) {
            return JNI_ERR;
          }

          int rc = (*env)->RegisterNatives(
              env,
              clazz,
              methods,
              sizeof(methods) / sizeof(methods[0]));
          if (rc != JNI_OK) {
            return JNI_ERR;
          }

          return JNI_VERSION_1_6;
        }
      )cc",
      absl::StrReplaceAll(dispatcherJavaFqName, {{".", "/"}}));
}

class Generator : public protoc::CodeGenerator {
  ~Generator() override {}
  bool Generate(
      const protobuf::FileDescriptor* file,
      const std::string& parameter,
      protoc::GeneratorContext* context,
      std::string* error) const override {
    return false;
  }
  bool Generate2(
      const protobuf::FileDescriptor* file,
      const std::string& parameter,
      protoc::GeneratorContext* context,
      std::string* error,
      const std::string& dispatcherJavaFqName,
      Output& msginit_dispatcher_output,
      size_t message_index) const;
  bool GenerateAll(
      const std::vector<const protobuf::FileDescriptor*>& files,
      const std::string& parameter,
      protoc::GeneratorContext* context,
      std::string* error) const override;
  uint64_t GetSupportedFeatures() const override {
    return FEATURE_PROTO3_OPTIONAL;
  }
};

bool Generator::GenerateAll(
    const std::vector<const protobuf::FileDescriptor*>& files,
    const std::string& parameter,
    protoc::GeneratorContext* context,
    std::string* error) const {
  std::unique_ptr<protobuf::io::ZeroCopyOutputStream>
      msginit_dispatcher_output_stream(context->Open("minitable_dispatcher.c"));
  Output msginit_dispatcher_output(msginit_dispatcher_output_stream.get());

  msginit_dispatcher_output(
      "/* This file was generated by jupbc (the jupb compiler) from the input\n"
      " * Do not edit -- your changes will be discarded when the file is\n"
      " * regenerated. */\n\n");
  msginit_dispatcher_output("#include <jni.h>\n");
  for (size_t i = 0; i < files.size(); i++) {
    msginit_dispatcher_output("#include \"$0\"\n", HeaderFilename(files[i]));
  }

  msginit_dispatcher_output(
      "static const upb_MiniTable_File *file_MiniTables[$0] = {\n",
      files.size());

  std::string java_dispatcher_package_name;
  std::string java_dispatcher_name;
  std::vector<std::pair<std::string, std::string>> params;
  google::protobuf::compiler::ParseGeneratorParameter(parameter, &params);
  for (const auto& pair : params) {
    if (pair.first == "java_dispatcher_package_name") {
      java_dispatcher_package_name = pair.second;
    } else if (pair.first == "java_dispatcher_name") {
      java_dispatcher_name = pair.second;
    } else {
      *error = "Unknown parameter: " + pair.first;
      return false;
    }
  }

  if (java_dispatcher_package_name.empty()) {
    *error = "Param java_dispatcher_package_name must be provided";
    return false;
  }
  if (java_dispatcher_name.empty()) {
    *error = "Param java_dispatcher_name must be provided";
    return false;
  }

  GenerateJavaDispatcher(
      context, java_dispatcher_package_name, java_dispatcher_name);

  std::string dispatcherJavaFqName =
      DispatcherJavaFqName(java_dispatcher_package_name, java_dispatcher_name);

  // Default implementation is just to call the per file method, and prefix
  // any error string with the file to provide context.
  bool succeeded = true;
  for (size_t i = 0; i < files.size(); i++) {
    const protobuf::FileDescriptor* file = files[i];
    succeeded = Generate2(
        file,
        parameter,
        context,
        error,
        dispatcherJavaFqName,
        msginit_dispatcher_output,
        i);
    if (!succeeded && error && error->empty()) {
      *error =
          "Code generator returned false but provided no error "
          "description.";
    }
    if (error && !error->empty()) {
      *error = file->name() + ": " + *error;
      break;
    }
    if (!succeeded) {
      break;
    }
  }

  msginit_dispatcher_output("};\n\n");
  WriteJni(msginit_dispatcher_output, dispatcherJavaFqName);
  return succeeded;
}

bool Generator::Generate2(
    const protobuf::FileDescriptor* file,
    const std::string& parameter,
    protoc::GeneratorContext* context,
    std::string* error,
    const std::string& dispatcherJavaFqName,
    Output& msginit_dispatcher_output,
    size_t file_index) const {
  FileLayout layout(file);

  if (file->package().empty()) {
    *error = "Must specify package";
    return false;
  }

  std::unique_ptr<protobuf::io::ZeroCopyOutputStream> java_stream(
      context->Open(WrapperClassName(file) + ".java"));
  Output java(java_stream.get());
  WriteJavaFile(layout, java, dispatcherJavaFqName, file_index);

  msginit_dispatcher_output("  &$0,\n", FileLayoutName(file));

  return true;
}

} // namespace
} // namespace jupbc

int main(int argc, char** argv) {
  std::unique_ptr<google::protobuf::compiler::CodeGenerator> generator(
      new jupbc::Generator());
  return google::protobuf::compiler::PluginMain(argc, argv, generator.get());
}