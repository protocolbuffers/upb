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

#include <ctype.h>
#include <errno.h>

#include "upb/mini_table.h"
#include "upb/reflection/def_builder_internal.h"
#include "upb/reflection/def_pool.h"
#include "upb/reflection/def_type.h"
#include "upb/reflection/desc_state_internal.h"
#include "upb/reflection/enum_def_internal.h"
#include "upb/reflection/enum_value_def_internal.h"
#include "upb/reflection/extension_range_internal.h"
#include "upb/reflection/field_def_internal.h"
#include "upb/reflection/file_def_internal.h"
#include "upb/reflection/message_def_internal.h"
#include "upb/reflection/oneof_def_internal.h"

// Must be last.
#include "upb/port_def.inc"

#define UPB_FIELD_TYPE_UNSPECIFIED 0

typedef struct {
  size_t len;
  char str[1];  // Null-terminated string data follows.
} str_t;

struct upb_FieldDef {
  const google_protobuf_FieldOptions* opts;
  const upb_FileDef* file;
  const upb_MessageDef* msgdef;
  const char* full_name;
  const char* json_name;
  union {
    int64_t sint;
    uint64_t uint;
    double dbl;
    float flt;
    bool boolean;
    str_t* str;
  } defaultval;
  union {
    const upb_OneofDef* oneof;
    const upb_MessageDef* extension_scope;
  } scope;
  union {
    const upb_MessageDef* msgdef;
    const upb_EnumDef* enumdef;
    const google_protobuf_FieldDescriptorProto* unresolved;
  } sub;
  uint32_t number_;
  uint16_t index_;
  uint16_t layout_index;  // Index into msgdef->layout->fields or file->exts
  bool has_default;
  bool is_extension_;
  bool is_packed_;
  bool proto3_optional_;
  bool has_json_name_;
  upb_FieldType type_;
  upb_Label label_;
#if UINTPTR_MAX == 0xffffffff
  uint32_t padding;  // Increase size to a multiple of 8.
#endif
};

upb_FieldDef* _upb_FieldDef_At(const upb_FieldDef* f, int i) {
  return (upb_FieldDef*)&f[i];
}

const google_protobuf_FieldOptions* upb_FieldDef_Options(const upb_FieldDef* f) {
  return f->opts;
}

bool upb_FieldDef_HasOptions(const upb_FieldDef* f) {
  return f->opts != (void*)kUpbDefOptDefault;
}

const char* upb_FieldDef_FullName(const upb_FieldDef* f) {
  return f->full_name;
}

upb_CType upb_FieldDef_CType(const upb_FieldDef* f) {
  switch (f->type_) {
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

upb_FieldType upb_FieldDef_Type(const upb_FieldDef* f) { return f->type_; }

uint32_t upb_FieldDef_Index(const upb_FieldDef* f) { return f->index_; }

upb_Label upb_FieldDef_Label(const upb_FieldDef* f) { return f->label_; }

uint32_t upb_FieldDef_Number(const upb_FieldDef* f) { return f->number_; }

bool upb_FieldDef_IsExtension(const upb_FieldDef* f) {
  return f->is_extension_;
}

bool upb_FieldDef_IsPacked(const upb_FieldDef* f) { return f->is_packed_; }

const char* upb_FieldDef_Name(const upb_FieldDef* f) {
  return _upb_DefBuilder_FullToShort(f->full_name);
}

const char* upb_FieldDef_JsonName(const upb_FieldDef* f) {
  return f->json_name;
}

bool upb_FieldDef_HasJsonName(const upb_FieldDef* f) {
  return f->has_json_name_;
}

const upb_FileDef* upb_FieldDef_File(const upb_FieldDef* f) { return f->file; }

const upb_MessageDef* upb_FieldDef_ContainingType(const upb_FieldDef* f) {
  return f->msgdef;
}

const upb_MessageDef* upb_FieldDef_ExtensionScope(const upb_FieldDef* f) {
  return f->is_extension_ ? f->scope.extension_scope : NULL;
}

const upb_OneofDef* upb_FieldDef_ContainingOneof(const upb_FieldDef* f) {
  return f->is_extension_ ? NULL : f->scope.oneof;
}

const upb_OneofDef* upb_FieldDef_RealContainingOneof(const upb_FieldDef* f) {
  const upb_OneofDef* oneof = upb_FieldDef_ContainingOneof(f);
  if (!oneof || upb_OneofDef_IsSynthetic(oneof)) return NULL;
  return oneof;
}

upb_MessageValue upb_FieldDef_Default(const upb_FieldDef* f) {
  UPB_ASSERT(!upb_FieldDef_IsSubMessage(f));
  upb_MessageValue ret;

  switch (upb_FieldDef_CType(f)) {
    case kUpb_CType_Bool:
      return (upb_MessageValue){.bool_val = f->defaultval.boolean};
    case kUpb_CType_Int64:
      return (upb_MessageValue){.int64_val = f->defaultval.sint};
    case kUpb_CType_UInt64:
      return (upb_MessageValue){.uint64_val = f->defaultval.uint};
    case kUpb_CType_Enum:
    case kUpb_CType_Int32:
      return (upb_MessageValue){.int32_val = (int32_t)f->defaultval.sint};
    case kUpb_CType_UInt32:
      return (upb_MessageValue){.uint32_val = (uint32_t)f->defaultval.uint};
    case kUpb_CType_Float:
      return (upb_MessageValue){.float_val = f->defaultval.flt};
    case kUpb_CType_Double:
      return (upb_MessageValue){.double_val = f->defaultval.dbl};
    case kUpb_CType_String:
    case kUpb_CType_Bytes: {
      str_t* str = f->defaultval.str;
      if (str) {
        return (upb_MessageValue){
            .str_val = (upb_StringView){.data = str->str, .size = str->len}};
      } else {
        return (upb_MessageValue){
            .str_val = (upb_StringView){.data = NULL, .size = 0}};
      }
    }
    default:
      UPB_UNREACHABLE();
  }

  return ret;
}

const upb_MessageDef* upb_FieldDef_MessageSubDef(const upb_FieldDef* f) {
  return upb_FieldDef_CType(f) == kUpb_CType_Message ? f->sub.msgdef : NULL;
}

const upb_EnumDef* upb_FieldDef_EnumSubDef(const upb_FieldDef* f) {
  return upb_FieldDef_CType(f) == kUpb_CType_Enum ? f->sub.enumdef : NULL;
}

const upb_MiniTable_Field* upb_FieldDef_MiniTable(const upb_FieldDef* f) {
  UPB_ASSERT(!upb_FieldDef_IsExtension(f));
  const upb_MiniTable* layout = upb_MessageDef_MiniTable(f->msgdef);
  return &layout->fields[f->layout_index];
}

const upb_MiniTable_Extension* _upb_FieldDef_ExtensionMiniTable(
    const upb_FieldDef* f) {
  UPB_ASSERT(upb_FieldDef_IsExtension(f));
  const upb_FileDef* file = upb_FieldDef_File(f);
  return _upb_FileDef_ExtensionMiniTable(file, f->layout_index);
}

bool _upb_FieldDef_IsClosedEnum(const upb_FieldDef* f) {
  if (UPB_TREAT_PROTO2_ENUMS_LIKE_PROTO3) return false;
  if (f->type_ != kUpb_FieldType_Enum) return false;

  // TODO: Maybe make is_proto2 a bool at creation?
  const upb_FileDef* file = upb_EnumDef_File(f->sub.enumdef);
  return upb_FileDef_Syntax(file) == kUpb_Syntax_Proto2;
}

bool _upb_FieldDef_IsProto3Optional(const upb_FieldDef* f) {
  return f->proto3_optional_;
}

int _upb_FieldDef_LayoutIndex(const upb_FieldDef* f) { return f->layout_index; }

uint64_t _upb_FieldDef_Modifiers(const upb_FieldDef* f) {
  uint64_t out = f->is_packed_ ? kUpb_FieldModifier_IsPacked : 0;

  switch (f->label_) {
    case kUpb_Label_Optional:
      if (!upb_FieldDef_HasPresence(f)) {
        out |= kUpb_FieldModifier_IsProto3Singular;
      }
      break;
    case kUpb_Label_Repeated:
      out |= kUpb_FieldModifier_IsRepeated;
      break;
    case kUpb_Label_Required:
      out |= kUpb_FieldModifier_IsRequired;
      break;
  }

  if (_upb_FieldDef_IsClosedEnum(f)) {
    out |= kUpb_FieldModifier_IsClosedEnum;
  }
  return out;
}

bool upb_FieldDef_HasDefault(const upb_FieldDef* f) { return f->has_default; }

bool upb_FieldDef_HasPresence(const upb_FieldDef* f) {
  if (upb_FieldDef_IsRepeated(f)) return false;
  const upb_FileDef* file = upb_FieldDef_File(f);
  return upb_FieldDef_IsSubMessage(f) || upb_FieldDef_ContainingOneof(f) ||
         upb_FileDef_Syntax(file) == kUpb_Syntax_Proto2;
}

bool upb_FieldDef_HasSubDef(const upb_FieldDef* f) {
  return upb_FieldDef_IsSubMessage(f) ||
         upb_FieldDef_CType(f) == kUpb_CType_Enum;
}

bool upb_FieldDef_IsMap(const upb_FieldDef* f) {
  return upb_FieldDef_IsRepeated(f) && upb_FieldDef_IsSubMessage(f) &&
         upb_MessageDef_IsMapEntry(upb_FieldDef_MessageSubDef(f));
}

bool upb_FieldDef_IsOptional(const upb_FieldDef* f) {
  return upb_FieldDef_Label(f) == kUpb_Label_Optional;
}

bool upb_FieldDef_IsPrimitive(const upb_FieldDef* f) {
  return !upb_FieldDef_IsString(f) && !upb_FieldDef_IsSubMessage(f);
}

bool upb_FieldDef_IsRepeated(const upb_FieldDef* f) {
  return upb_FieldDef_Label(f) == kUpb_Label_Repeated;
}

bool upb_FieldDef_IsRequired(const upb_FieldDef* f) {
  return upb_FieldDef_Label(f) == kUpb_Label_Required;
}

bool upb_FieldDef_IsString(const upb_FieldDef* f) {
  return upb_FieldDef_CType(f) == kUpb_CType_String ||
         upb_FieldDef_CType(f) == kUpb_CType_Bytes;
}

bool upb_FieldDef_IsSubMessage(const upb_FieldDef* f) {
  return upb_FieldDef_CType(f) == kUpb_CType_Message;
}

static bool between(int32_t x, int32_t low, int32_t high) {
  return x >= low && x <= high;
}

bool upb_FieldDef_checklabel(int32_t label) { return between(label, 1, 3); }
bool upb_FieldDef_checktype(int32_t type) { return between(type, 1, 11); }
bool upb_FieldDef_checkintfmt(int32_t fmt) { return between(fmt, 1, 3); }

bool upb_FieldDef_checkdescriptortype(int32_t type) {
  return between(type, 1, 18);
}

static bool streql2(const char* a, size_t n, const char* b) {
  return n == strlen(b) && memcmp(a, b, n) == 0;
}

// Implement the transformation as described in the spec:
//   1. upper case all letters after an underscore.
//   2. remove all underscores.
static char* make_json_name(const char* name, size_t size, upb_Arena* a) {
  char* out = upb_Arena_Malloc(a, size + 1);  // +1 is to add a trailing '\0'
  if (out == NULL) return NULL;

  bool ucase_next = false;
  char* des = out;
  for (size_t i = 0; i < size; i++) {
    if (name[i] == '_') {
      ucase_next = true;
    } else {
      *des++ = ucase_next ? toupper(name[i]) : name[i];
      ucase_next = false;
    }
  }
  *des++ = '\0';
  return out;
}

static str_t* newstr(upb_DefBuilder* ctx, const char* data, size_t len) {
  str_t* ret = _upb_DefBuilder_Alloc(ctx, sizeof(*ret) + len);
  if (!ret) _upb_DefBuilder_OomErr(ctx);
  ret->len = len;
  if (len) memcpy(ret->str, data, len);
  ret->str[len] = '\0';
  return ret;
}

static str_t* unescape(upb_DefBuilder* ctx, const upb_FieldDef* f,
                       const char* data, size_t len) {
  // Size here is an upper bound; escape sequences could ultimately shrink it.
  str_t* ret = _upb_DefBuilder_Alloc(ctx, sizeof(*ret) + len);
  char* dst = &ret->str[0];
  const char* src = data;
  const char* end = data + len;

  while (src < end) {
    if (*src == '\\') {
      src++;
      *dst++ = _upb_DefBuilder_ParseEscape(ctx, f, &src, end);
    } else {
      *dst++ = *src++;
    }
  }

  ret->len = dst - &ret->str[0];
  return ret;
}

static void parse_default(upb_DefBuilder* ctx, const char* str, size_t len,
                          upb_FieldDef* f) {
  char* end;
  char nullz[64];
  errno = 0;

  switch (upb_FieldDef_CType(f)) {
    case kUpb_CType_Int32:
    case kUpb_CType_Int64:
    case kUpb_CType_UInt32:
    case kUpb_CType_UInt64:
    case kUpb_CType_Double:
    case kUpb_CType_Float:
      /* Standard C number parsing functions expect null-terminated strings. */
      if (len >= sizeof(nullz) - 1) {
        _upb_DefBuilder_Errf(ctx, "Default too long: %.*s", (int)len, str);
      }
      memcpy(nullz, str, len);
      nullz[len] = '\0';
      str = nullz;
      break;
    default:
      break;
  }

  switch (upb_FieldDef_CType(f)) {
    case kUpb_CType_Int32: {
      long val = strtol(str, &end, 0);
      if (val > INT32_MAX || val < INT32_MIN || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.sint = val;
      break;
    }
    case kUpb_CType_Enum: {
      const upb_EnumDef* e = f->sub.enumdef;
      const upb_EnumValueDef* ev =
          upb_EnumDef_FindValueByNameWithSize(e, str, len);
      if (!ev) {
        goto invalid;
      }
      f->defaultval.sint = upb_EnumValueDef_Number(ev);
      break;
    }
    case kUpb_CType_Int64: {
      long long val = strtoll(str, &end, 0);
      if (val > INT64_MAX || val < INT64_MIN || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.sint = val;
      break;
    }
    case kUpb_CType_UInt32: {
      unsigned long val = strtoul(str, &end, 0);
      if (val > UINT32_MAX || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.uint = val;
      break;
    }
    case kUpb_CType_UInt64: {
      unsigned long long val = strtoull(str, &end, 0);
      if (val > UINT64_MAX || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.uint = val;
      break;
    }
    case kUpb_CType_Double: {
      double val = strtod(str, &end);
      if (errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.dbl = val;
      break;
    }
    case kUpb_CType_Float: {
      float val = strtof(str, &end);
      if (errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.flt = val;
      break;
    }
    case kUpb_CType_Bool: {
      if (streql2(str, len, "false")) {
        f->defaultval.boolean = false;
      } else if (streql2(str, len, "true")) {
        f->defaultval.boolean = true;
      } else {
        goto invalid;
      }
      break;
    }
    case kUpb_CType_String:
      f->defaultval.str = newstr(ctx, str, len);
      break;
    case kUpb_CType_Bytes:
      f->defaultval.str = unescape(ctx, f, str, len);
      break;
    case kUpb_CType_Message:
      /* Should not have a default value. */
      _upb_DefBuilder_Errf(ctx, "Message should not have a default (%s)",
                           upb_FieldDef_FullName(f));
  }

  return;

invalid:
  _upb_DefBuilder_Errf(ctx, "Invalid default '%.*s' for field %s of type %d",
                       (int)len, str, upb_FieldDef_FullName(f),
                       (int)upb_FieldDef_Type(f));
}

static void set_default_default(upb_DefBuilder* ctx, upb_FieldDef* f) {
  switch (upb_FieldDef_CType(f)) {
    case kUpb_CType_Int32:
    case kUpb_CType_Int64:
      f->defaultval.sint = 0;
      break;
    case kUpb_CType_UInt64:
    case kUpb_CType_UInt32:
      f->defaultval.uint = 0;
      break;
    case kUpb_CType_Double:
    case kUpb_CType_Float:
      f->defaultval.dbl = 0;
      break;
    case kUpb_CType_String:
    case kUpb_CType_Bytes:
      f->defaultval.str = newstr(ctx, NULL, 0);
      break;
    case kUpb_CType_Bool:
      f->defaultval.boolean = false;
      break;
    case kUpb_CType_Enum: {
      const upb_EnumValueDef* v = upb_EnumDef_Value(f->sub.enumdef, 0);
      f->defaultval.sint = upb_EnumValueDef_Number(v);
    }
    case kUpb_CType_Message:
      break;
  }
}

static void _upb_FieldDef_Create(upb_DefBuilder* ctx, const char* prefix,
                                 const google_protobuf_FieldDescriptorProto* field_proto,
                                 upb_MessageDef* m, upb_FieldDef* f) {
  // Must happen before _upb_DefBuilder_Add()
  f->file = _upb_DefBuilder_File(ctx);

  if (!google_protobuf_FieldDescriptorProto_has_name(field_proto)) {
    _upb_DefBuilder_Errf(ctx, "field has no name");
  }

  const upb_StringView name = google_protobuf_FieldDescriptorProto_name(field_proto);
  _upb_DefBuilder_CheckIdentNotFull(ctx, name);

  f->has_json_name_ = google_protobuf_FieldDescriptorProto_has_json_name(field_proto);
  if (f->has_json_name_) {
    const upb_StringView sv =
        google_protobuf_FieldDescriptorProto_json_name(field_proto);
    f->json_name = upb_strdup2(sv.data, sv.size, ctx->arena);
  } else {
    f->json_name = make_json_name(name.data, name.size, ctx->arena);
  }
  if (!f->json_name) _upb_DefBuilder_OomErr(ctx);

  f->full_name = _upb_DefBuilder_MakeFullName(ctx, prefix, name);
  f->label_ = (int)google_protobuf_FieldDescriptorProto_label(field_proto);
  f->number_ = google_protobuf_FieldDescriptorProto_number(field_proto);
  f->proto3_optional_ =
      google_protobuf_FieldDescriptorProto_proto3_optional(field_proto);
  f->msgdef = m;
  f->scope.oneof = NULL;

  const bool has_type = google_protobuf_FieldDescriptorProto_has_type(field_proto);
  const bool has_type_name =
      google_protobuf_FieldDescriptorProto_has_type_name(field_proto);

  f->type_ = (int)google_protobuf_FieldDescriptorProto_type(field_proto);

  if (has_type) {
    switch (f->type_) {
      case kUpb_FieldType_Message:
      case kUpb_FieldType_Group:
      case kUpb_FieldType_Enum:
        if (!has_type_name) {
          _upb_DefBuilder_Errf(ctx, "field of type %d requires type name (%s)",
                               (int)f->type_, f->full_name);
        }
        break;
      default:
        if (has_type_name) {
          _upb_DefBuilder_Errf(
              ctx, "invalid type for field with type_name set (%s, %d)",
              f->full_name, (int)f->type_);
        }
    }
  } else if (has_type_name) {
    f->type_ =
        UPB_FIELD_TYPE_UNSPECIFIED;  // We'll fill this in in resolve_fielddef()
  }

  if (f->type_ < kUpb_FieldType_Double || f->type_ > kUpb_FieldType_SInt64) {
    _upb_DefBuilder_Errf(ctx, "invalid type for field %s (%d)", f->full_name,
                         f->type_);
  }

  if (f->label_ < kUpb_Label_Optional || f->label_ > kUpb_Label_Repeated) {
    _upb_DefBuilder_Errf(ctx, "invalid label for field %s (%d)", f->full_name,
                         f->label_);
  }

  /* We can't resolve the subdef or (in the case of extensions) the containing
   * message yet, because it may not have been defined yet.  We stash a pointer
   * to the field_proto until later when we can properly resolve it. */
  f->sub.unresolved = field_proto;

  if (f->label_ == kUpb_Label_Required &&
      upb_FileDef_Syntax(f->file) == kUpb_Syntax_Proto3) {
    _upb_DefBuilder_Errf(ctx, "proto3 fields cannot be required (%s)",
                         f->full_name);
  }

  if (google_protobuf_FieldDescriptorProto_has_oneof_index(field_proto)) {
    uint32_t oneof_index = google_protobuf_FieldDescriptorProto_oneof_index(field_proto);

    if (upb_FieldDef_Label(f) != kUpb_Label_Optional) {
      _upb_DefBuilder_Errf(ctx, "fields in oneof must have OPTIONAL label (%s)",
                           f->full_name);
    }

    if (!m) {
      _upb_DefBuilder_Errf(ctx, "oneof field (%s) has no containing msg",
                           f->full_name);
    }

    if (oneof_index >= upb_MessageDef_OneofCount(m)) {
      _upb_DefBuilder_Errf(ctx, "oneof_index out of range (%s)", f->full_name);
    }

    upb_OneofDef* oneof = (upb_OneofDef*)upb_MessageDef_Oneof(m, oneof_index);
    f->scope.oneof = oneof;

    bool ok = _upb_OneofDef_Insert(oneof, f, name.data, name.size, ctx->arena);
    if (!ok) _upb_DefBuilder_OomErr(ctx);
  }

  UPB_DEF_SET_OPTIONS(f->opts, FieldDescriptorProto, FieldOptions, field_proto);

  if (google_protobuf_FieldOptions_has_packed(f->opts)) {
    f->is_packed_ = google_protobuf_FieldOptions_packed(f->opts);
  } else {
    // Repeated fields default to packed for proto3 only.
    f->is_packed_ = upb_FieldDef_IsPrimitive(f) &&
                    f->label_ == kUpb_Label_Repeated &&
                    upb_FileDef_Syntax(f->file) == kUpb_Syntax_Proto3;
  }
}

static void _upb_FieldDef_CreateExt(
    upb_DefBuilder* ctx, const char* prefix,
    const google_protobuf_FieldDescriptorProto* field_proto, upb_MessageDef* m,
    upb_FieldDef* f) {
  _upb_FieldDef_Create(ctx, prefix, field_proto, m, f);
  f->is_extension_ = true;

  if (google_protobuf_FieldDescriptorProto_has_oneof_index(field_proto)) {
    _upb_DefBuilder_Errf(ctx, "oneof_index provided for extension field (%s)",
                         f->full_name);
  }

  f->scope.extension_scope = m;
  _upb_DefBuilder_Add(ctx, f->full_name, _upb_DefType_Pack(f, UPB_DEFTYPE_EXT));
  f->layout_index = ctx->ext_count++;

  if (ctx->layout) {
    UPB_ASSERT(_upb_FieldDef_ExtensionMiniTable(f)->field.number == f->number_);
  }
}

static void _upb_FieldDef_CreateNotExt(
    upb_DefBuilder* ctx, const char* prefix,
    const google_protobuf_FieldDescriptorProto* field_proto, upb_MessageDef* m,
    upb_FieldDef* f) {
  _upb_FieldDef_Create(ctx, prefix, field_proto, m, f);
  f->is_extension_ = false;

  if (!google_protobuf_FieldDescriptorProto_has_oneof_index(field_proto)) {
    if (f->proto3_optional_) {
      _upb_DefBuilder_Errf(
          ctx,
          "non-extension field (%s) with proto3_optional was not in a oneof",
          f->full_name);
    }
  }

  _upb_MessageDef_InsertField(ctx, m, f);

  if (!ctx->layout) return;

  const upb_MiniTable* mt = upb_MessageDef_MiniTable(m);
  const upb_MiniTable_Field* fields = mt->fields;
  for (int i = 0; i < mt->field_count; i++) {
    if (fields[i].number == f->number_) {
      f->layout_index = i;
      return;
    }
  }

  UPB_ASSERT(false);  // It should be impossible to reach this point.
}

upb_FieldDef* _upb_FieldDefs_New(
    upb_DefBuilder* ctx, int n,
    const google_protobuf_FieldDescriptorProto* const* protos, const char* prefix,
    upb_MessageDef* m, bool* is_sorted) {
  _upb_DefType_CheckPadding(sizeof(upb_FieldDef));
  upb_FieldDef* defs =
      (upb_FieldDef*)_upb_DefBuilder_Alloc(ctx, sizeof(upb_FieldDef) * n);

  // If we are creating extensions then is_sorted will be NULL.
  // If we are not creating extensions then is_sorted will be non-NULL.
  if (is_sorted) {
    uint32_t previous = 0;
    for (int i = 0; i < n; i++) {
      upb_FieldDef* f = &defs[i];

      _upb_FieldDef_CreateNotExt(ctx, prefix, protos[i], m, f);
      f->index_ = i;
      if (!ctx->layout) f->layout_index = i;

      const uint32_t current = f->number_;
      if (previous > current) *is_sorted = false;
      previous = current;
    }
  } else {
    for (int i = 0; i < n; i++) {
      upb_FieldDef* f = &defs[i];

      _upb_FieldDef_CreateExt(ctx, prefix, protos[i], m, f);
      f->index_ = i;
    }
  }

  return defs;
}

static void resolve_subdef(upb_DefBuilder* ctx, const char* prefix,
                           upb_FieldDef* f) {
  const google_protobuf_FieldDescriptorProto* field_proto = f->sub.unresolved;
  upb_StringView name =
      google_protobuf_FieldDescriptorProto_type_name(field_proto);
  bool has_name =
      google_protobuf_FieldDescriptorProto_has_type_name(field_proto);
  switch ((int)f->type_) {
    case UPB_FIELD_TYPE_UNSPECIFIED: {
      // Type was not specified and must be inferred.
      UPB_ASSERT(has_name);
      upb_deftype_t type;
      const void* def =
          _upb_DefBuilder_ResolveAny(ctx, f->full_name, prefix, name, &type);
      switch (type) {
        case UPB_DEFTYPE_ENUM:
          f->sub.enumdef = def;
          f->type_ = kUpb_FieldType_Enum;
          break;
        case UPB_DEFTYPE_MSG:
          f->sub.msgdef = def;
          f->type_ = kUpb_FieldType_Message;  // It appears there is no way of
                                              // this being a group.
          break;
        default:
          _upb_DefBuilder_Errf(ctx, "Couldn't resolve type name for field %s",
                               f->full_name);
      }
    }
    case kUpb_FieldType_Message:
    case kUpb_FieldType_Group:
      UPB_ASSERT(has_name);
      f->sub.msgdef = _upb_DefBuilder_Resolve(ctx, f->full_name, prefix, name,
                                              UPB_DEFTYPE_MSG);
      break;
    case kUpb_FieldType_Enum:
      UPB_ASSERT(has_name);
      f->sub.enumdef = _upb_DefBuilder_Resolve(ctx, f->full_name, prefix, name,
                                               UPB_DEFTYPE_ENUM);
      break;
    default:
      // No resolution necessary.
      break;
  }
}

static int _upb_FieldDef_Compare(const void* p1, const void* p2) {
  const uint32_t v1 = (*(upb_FieldDef**)p1)->number_;
  const uint32_t v2 = (*(upb_FieldDef**)p2)->number_;
  return (v1 < v2) ? -1 : (v1 > v2);
}

const upb_FieldDef** _upb_FieldDefs_Sorted(const upb_FieldDef* f, int n,
                                           upb_Arena* a) {
  // TODO: Try to replace this arena alloc with a persistent scratch buffer.
  upb_FieldDef** out = (upb_FieldDef**)upb_Arena_Malloc(a, n * sizeof(void*));
  if (!out) return NULL;

  for (int i = 0; i < n; i++) {
    out[i] = (upb_FieldDef*)&f[i];
  }
  qsort(out, n, sizeof(void*), _upb_FieldDef_Compare);

  for (int i = 0; i < n; i++) {
    out[i]->layout_index = i;
  }
  return (const upb_FieldDef**)out;
}

bool upb_FieldDef_MiniDescriptorEncode(const upb_FieldDef* f, upb_Arena* a,
                                       upb_StringView* out) {
  UPB_ASSERT(f->is_extension_);

  upb_DescState s;
  _upb_DescState_Init(&s);

  if (!_upb_DescState_Grow(&s, a)) return false;
  s.ptr = upb_MtDataEncoder_StartMessage(&s.e, s.ptr, 0);

  const int number = upb_FieldDef_Number(f);
  const uint64_t modifiers = _upb_FieldDef_Modifiers(f);

  if (!_upb_DescState_Grow(&s, a)) return false;
  s.ptr = upb_MtDataEncoder_PutField(&s.e, s.ptr, f->type_, number, modifiers);

  if (!_upb_DescState_Grow(&s, a)) return false;
  *s.ptr = '\0';

  out->data = s.buf;
  out->size = s.ptr - s.buf;
  return true;
}

static void resolve_extension(upb_DefBuilder* ctx, const char* prefix,
                              upb_FieldDef* f,
                              const google_protobuf_FieldDescriptorProto* field_proto) {
  if (!google_protobuf_FieldDescriptorProto_has_extendee(field_proto)) {
    _upb_DefBuilder_Errf(ctx, "extension for field '%s' had no extendee",
                         f->full_name);
  }

  upb_StringView name = google_protobuf_FieldDescriptorProto_extendee(field_proto);
  const upb_MessageDef* m =
      _upb_DefBuilder_Resolve(ctx, f->full_name, prefix, name, UPB_DEFTYPE_MSG);
  f->msgdef = m;

  if (!_upb_MessageDef_IsValidExtensionNumber(m, f->number_)) {
    _upb_DefBuilder_Errf(
        ctx,
        "field number %u in extension %s has no extension range in message %s",
        (unsigned)f->number_, f->full_name, upb_MessageDef_FullName(m));
  }

  const upb_MiniTable_Extension* ext = _upb_FieldDef_ExtensionMiniTable(f);

  if (ctx->layout) {
    UPB_ASSERT(upb_FieldDef_Number(f) == ext->field.number);
  } else {
    upb_StringView desc;
    if (!upb_FieldDef_MiniDescriptorEncode(f, ctx->tmp_arena, &desc)) {
      _upb_DefBuilder_OomErr(ctx);
    }

    upb_MiniTable_Extension* mut_ext = (upb_MiniTable_Extension*)ext;
    upb_MiniTable_Sub sub = {NULL};
    if (upb_FieldDef_IsSubMessage(f)) {
      sub.submsg = upb_MessageDef_MiniTable(f->sub.msgdef);
    } else if (_upb_FieldDef_IsClosedEnum(f)) {
      sub.subenum = _upb_EnumDef_MiniTable(f->sub.enumdef);
    }
    bool ok2 = upb_MiniTable_BuildExtension(desc.data, desc.size, mut_ext,
                                            upb_MessageDef_MiniTable(m), sub,
                                            ctx->status);
    if (!ok2) _upb_DefBuilder_Errf(ctx, "Could not build extension mini table");
  }

  bool ok = _upb_DefPool_InsertExt(ctx->symtab, ext, f);
  if (!ok) _upb_DefBuilder_OomErr(ctx);
}

static void resolve_default(upb_DefBuilder* ctx, upb_FieldDef* f,
                            const google_protobuf_FieldDescriptorProto* field_proto) {
  // Have to delay resolving of the default value until now because of the enum
  // case, since enum defaults are specified with a label.
  if (google_protobuf_FieldDescriptorProto_has_default_value(field_proto)) {
    upb_StringView defaultval =
        google_protobuf_FieldDescriptorProto_default_value(field_proto);

    if (upb_FileDef_Syntax(f->file) == kUpb_Syntax_Proto3) {
      _upb_DefBuilder_Errf(ctx,
                           "proto3 fields cannot have explicit defaults (%s)",
                           f->full_name);
    }

    if (upb_FieldDef_IsSubMessage(f)) {
      _upb_DefBuilder_Errf(ctx,
                           "message fields cannot have explicit defaults (%s)",
                           f->full_name);
    }

    parse_default(ctx, defaultval.data, defaultval.size, f);
    f->has_default = true;
  } else {
    set_default_default(ctx, f);
    f->has_default = false;
  }
}

void _upb_FieldDef_Resolve(upb_DefBuilder* ctx, const char* prefix,
                           upb_FieldDef* f) {
  // We have to stash this away since resolve_subdef() may overwrite it.
  const google_protobuf_FieldDescriptorProto* field_proto = f->sub.unresolved;

  resolve_subdef(ctx, prefix, f);
  resolve_default(ctx, f, field_proto);

  if (f->is_extension_) {
    resolve_extension(ctx, prefix, f, field_proto);
  }
}