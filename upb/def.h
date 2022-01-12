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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Google LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Defs are upb's internal representation of the constructs that can appear
 * in a .proto file:
 *
 * - upb_MessageDef: describes a "message" construct.
 * - upb_FieldDef: describes a message field.
 * - upb_filedef: describes a .proto file and its defs.
 * - upb_EnumDef: describes an enum.
 * - upb_OneofDef: describes a oneof.
 *
 * TODO: definitions of services.
 */

#ifndef UPB_DEF_H_
#define UPB_DEF_H_

#include "upb/upb.h"
#include "upb/table_internal.h"
#include "google/protobuf/descriptor.upb.h"

/* Must be last. */
#include "upb/port_def.inc"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

struct upb_EnumDef;
typedef struct upb_EnumDef upb_EnumDef;
struct upb_EnumValueDef;
typedef struct upb_EnumValueDef upb_EnumValueDef;
struct upb_ExtensionRange;
typedef struct upb_ExtensionRange upb_ExtensionRange;
struct upb_FieldDef;
typedef struct upb_FieldDef upb_FieldDef;
struct upb_filedef;
typedef struct upb_filedef upb_filedef;
struct upb_methoddef;
typedef struct upb_methoddef upb_methoddef;
struct upb_MessageDef;
typedef struct upb_MessageDef upb_MessageDef;
struct upb_OneofDef;
typedef struct upb_OneofDef upb_OneofDef;
struct upb_servicedef;
typedef struct upb_servicedef upb_servicedef;
struct upb_streamdef;
typedef struct upb_streamdef upb_streamdef;
struct upb_symtab;
typedef struct upb_symtab upb_symtab;

typedef enum {
  UPB_SYNTAX_PROTO2 = 2,
  UPB_SYNTAX_PROTO3 = 3
} upb_syntax_t;

/* All the different kind of well known type messages. For simplicity of check,
 * number wrappers and string wrappers are grouped together. Make sure the
 * order and merber of these groups are not changed.
 */
typedef enum {
  UPB_WELLKNOWN_UNSPECIFIED,
  UPB_WELLKNOWN_ANY,
  UPB_WELLKNOWN_FIELDMASK,
  UPB_WELLKNOWN_DURATION,
  UPB_WELLKNOWN_TIMESTAMP,
  /* number wrappers */
  UPB_WELLKNOWN_DOUBLEVALUE,
  UPB_WELLKNOWN_FLOATVALUE,
  UPB_WELLKNOWN_INT64VALUE,
  UPB_WELLKNOWN_UINT64VALUE,
  UPB_WELLKNOWN_INT32VALUE,
  UPB_WELLKNOWN_UINT32VALUE,
  /* string wrappers */
  UPB_WELLKNOWN_STRINGVALUE,
  UPB_WELLKNOWN_BYTESVALUE,
  UPB_WELLKNOWN_BOOLVALUE,
  UPB_WELLKNOWN_VALUE,
  UPB_WELLKNOWN_LISTVALUE,
  UPB_WELLKNOWN_STRUCT
} upb_wellknowntype_t;

/* upb_FieldDef ***************************************************************/

/* Maximum field number allowed for FieldDefs.  This is an inherent limit of the
 * protobuf wire format. */
#define UPB_MAX_FIELDNUMBER ((1 << 29) - 1)

const google_protobuf_FieldOptions *upb_FieldDef_Options(const upb_FieldDef *f);
bool upb_FieldDef_HasOptions(const upb_FieldDef *f);
const char *upb_FieldDef_FullName(const upb_FieldDef *f);
upb_fieldtype_t upb_FieldDef_CType(const upb_FieldDef *f);
upb_descriptortype_t upb_FieldDef_Type(const upb_FieldDef *f);
upb_label_t upb_FieldDef_Label(const upb_FieldDef *f);
uint32_t upb_FieldDef_Number(const upb_FieldDef *f);
const char *upb_FieldDef_Name(const upb_FieldDef *f);
const char *upb_FieldDef_JsonName(const upb_FieldDef *f);
bool upb_FieldDef_HasJsonName(const upb_FieldDef *f);
bool upb_FieldDef_IsExtension(const upb_FieldDef *f);
bool upb_FieldDef_IsPacked(const upb_FieldDef *f);
const upb_filedef *upb_FieldDef_File(const upb_FieldDef *f);
const upb_MessageDef *upb_FieldDef_ContainingType(const upb_FieldDef *f);
const upb_MessageDef *upb_FieldDef_ExtensionScope(const upb_FieldDef *f);
const upb_OneofDef *upb_FieldDef_ContainingOneof(const upb_FieldDef *f);
const upb_OneofDef *upb_FieldDef_RealContainingOneof(const upb_FieldDef *f);
uint32_t upb_FieldDef_Index(const upb_FieldDef *f);
bool upb_FieldDef_IsSubMessage(const upb_FieldDef *f);
bool upb_FieldDef_IsString(const upb_FieldDef *f);
bool upb_FieldDef_IsRepeated(const upb_FieldDef *f);
bool upb_FieldDef_IsPrimitive(const upb_FieldDef *f);
bool upb_FieldDef_IsMap(const upb_FieldDef *f);
bool upb_FieldDef_HasDefault(const upb_FieldDef *f);
bool upb_FieldDef_HasSubDef(const upb_FieldDef *f);
bool upb_FieldDef_HasPresence(const upb_FieldDef *f);
const upb_MessageDef *upb_FieldDef_MessageSubDef(const upb_FieldDef *f);
const upb_EnumDef *upb_FieldDef_EnumSubDef(const upb_FieldDef *f);
const upb_msglayout_field *upb_FieldDef_Layout(const upb_FieldDef *f);
const upb_msglayout_ext *_upb_FieldDef_ExtensionLayout(const upb_FieldDef *f);
bool _upb_FieldDef_IsProto3Optional(const upb_FieldDef *f);

/* upb_OneofDef ***************************************************************/

const google_protobuf_OneofOptions *upb_OneofDef_Options(const upb_OneofDef *o);
bool upb_OneofDef_HasOptions(const upb_OneofDef *o);
const char *upb_OneofDef_Name(const upb_OneofDef *o);
const upb_MessageDef *upb_OneofDef_ContainingType(const upb_OneofDef *o);
uint32_t upb_OneofDef_Index(const upb_OneofDef *o);
bool upb_OneofDef_IsSynthetic(const upb_OneofDef *o);
int upb_OneofDef_FieldCount(const upb_OneofDef *o);
const upb_FieldDef *upb_OneofDef_Field(const upb_OneofDef *o, int i);

/* Oneof lookups:
 * - ntof:  look up a field by name.
 * - ntofz: look up a field by name (as a null-terminated string).
 * - itof:  look up a field by number. */
const upb_FieldDef *upb_OneofDef_LookupNameWithSize(const upb_OneofDef *o,
                                      const char *name, size_t length);
UPB_INLINE const upb_FieldDef *upb_OneofDef_LookupName(const upb_OneofDef *o,
                                                  const char *name) {
  return upb_OneofDef_LookupNameWithSize(o, name, strlen(name));
}
const upb_FieldDef *upb_OneofDef_LookupNumber(const upb_OneofDef *o, uint32_t num);

/* upb_MessageDef *****************************************************************/

/* Well-known field tag numbers for map-entry messages. */
#define UPB_MAPENTRY_KEY   1
#define UPB_MAPENTRY_VALUE 2

/* Well-known field tag numbers for Any messages. */
#define UPB_ANY_TYPE 1
#define UPB_ANY_VALUE 2

/* Well-known field tag numbers for timestamp messages. */
#define UPB_DURATION_SECONDS 1
#define UPB_DURATION_NANOS 2

/* Well-known field tag numbers for duration messages. */
#define UPB_TIMESTAMP_SECONDS 1
#define UPB_TIMESTAMP_NANOS 2

const google_protobuf_MessageOptions *upb_MessageDef_Options(const upb_MessageDef *m);
bool upb_MessageDef_HasOptions(const upb_MessageDef *m);
const char *upb_MessageDef_FullName(const upb_MessageDef *m);
const upb_filedef *upb_MessageDef_File(const upb_MessageDef *m);
const upb_MessageDef *upb_MessageDef_ContainingType(const upb_MessageDef *m);
const char *upb_MessageDef_Name(const upb_MessageDef *m);
upb_syntax_t upb_MessageDef_Syntax(const upb_MessageDef *m);
upb_wellknowntype_t upb_MessageDef_WellKnownType(const upb_MessageDef *m);
bool upb_MessageDef_iswrapper(const upb_MessageDef *m);
bool upb_MessageDef_isnumberwrapper(const upb_MessageDef *m);
int upb_MessageDef_ExtensionRangeCount(const upb_MessageDef *m);
int upb_MessageDef_FieldCount(const upb_MessageDef *m);
int upb_MessageDef_OneofCount(const upb_MessageDef *m);
const upb_ExtensionRange *upb_MessageDef_ExtensionRange(const upb_MessageDef *m, int i);
const upb_FieldDef *upb_MessageDef_Field(const upb_MessageDef *m, int i);
const upb_OneofDef *upb_MessageDef_Oneof(const upb_MessageDef *m, int i);
const upb_FieldDef *upb_MessageDef_FindFieldByNumberWithSize(const upb_MessageDef *m, uint32_t i);
const upb_FieldDef *upb_MessageDef_FindFieldByNameWithSize(const upb_MessageDef *m, const char *name,
                                    size_t len);
const upb_OneofDef *upb_MessageDef_FindOneofByNameWithSize(const upb_MessageDef *m, const char *name,
                                    size_t len);
const upb_msglayout *upb_MessageDef_Layout(const upb_MessageDef *m);

UPB_INLINE const upb_OneofDef *upb_MessageDef_FindOneofByName(const upb_MessageDef *m,
                                               const char *name) {
  return upb_MessageDef_FindOneofByNameWithSize(m, name, strlen(name));
}

UPB_INLINE const upb_FieldDef *upb_MessageDef_FindFieldByName(const upb_MessageDef *m,
                                                const char *name) {
  return upb_MessageDef_FindFieldByNameWithSize(m, name, strlen(name));
}

UPB_INLINE bool upb_MessageDef_IsMapEntry(const upb_MessageDef *m) {
  return google_protobuf_MessageOptions_map_entry(upb_MessageDef_Options(m));
}

/* Nested entities. */
int upb_MessageDef_NestedMessageCount(const upb_MessageDef *m);
int upb_MessageDef_NestedEnumCount(const upb_MessageDef *m);
int upb_MessageDef_NestedExtensionCount(const upb_MessageDef *m);
const upb_MessageDef *upb_MessageDef_NestedMessage(const upb_MessageDef *m, int i);
const upb_EnumDef *upb_MessageDef_NestedEnum(const upb_MessageDef *m, int i);
const upb_FieldDef *upb_MessageDef_NestedExtension(const upb_MessageDef *m, int i);

/* Lookup of either field or oneof by name.  Returns whether either was found.
 * If the return is true, then the found def will be set, and the non-found
 * one set to NULL. */
bool upb_MessageDef_FindByNameWithSize(const upb_MessageDef *m, const char *name, size_t len,
                           const upb_FieldDef **f, const upb_OneofDef **o);

UPB_INLINE bool upb_MessageDef_FindByName(const upb_MessageDef *m, const char *name,
                                       const upb_FieldDef **f,
                                       const upb_OneofDef **o) {
  return upb_MessageDef_FindByNameWithSize(m, name, strlen(name), f, o);
}

/* Returns a field by either JSON name or regular proto name. */
const upb_FieldDef *upb_MessageDef_FindByJsonNameWithSize(const upb_MessageDef *m,
                                              const char *name, size_t len);
UPB_INLINE const upb_FieldDef* upb_MessageDef_FindByJsonName(
    const upb_MessageDef* m, const char* name) {
  return upb_MessageDef_FindByJsonNameWithSize(m, name, strlen(name));
}

/* upb_ExtensionRange ***************************************************************/

const google_protobuf_ExtensionRangeOptions *upb_ExtensionRange_Options(
    const upb_ExtensionRange *r);
bool upb_ExtensionRange_HasOptions(const upb_ExtensionRange *r);
int32_t upb_ExtensionRange_Start(const upb_ExtensionRange *r);
int32_t upb_ExtensionRange_End(const upb_ExtensionRange *r);

/* upb_EnumDef ****************************************************************/

const google_protobuf_EnumOptions *upb_EnumDef_Options(const upb_EnumDef *e);
bool upb_EnumDef_HasOptions(const upb_EnumDef *e);
const char *upb_EnumDef_FullName(const upb_EnumDef *e);
const char *upb_EnumDef_Name(const upb_EnumDef *e);
const upb_filedef *upb_EnumDef_File(const upb_EnumDef *e);
const upb_MessageDef *upb_EnumDef_ContainingType(const upb_EnumDef *e);
int32_t upb_EnumDef_Default(const upb_EnumDef *e);
int upb_EnumDef_ValueCount(const upb_EnumDef *e);
const upb_EnumValueDef *upb_EnumDef_Value(const upb_EnumDef *e, int i);

const upb_EnumValueDef *upb_EnumDef_FindValueByNameWithSize(const upb_EnumDef *e,
                                             const char *name, size_t len);
const upb_EnumValueDef *upb_EnumDef_FindValueByNumber(const upb_EnumDef *e, int32_t num);
bool upb_EnumDef_CheckNumber(const upb_EnumDef *e, int32_t num);

// Convenience wrapper.
UPB_INLINE const upb_EnumValueDef *upb_EnumDef_FindValueByName(const upb_EnumDef *e,
                                                         const char *name) {
  return upb_EnumDef_FindValueByNameWithSize(e, name, strlen(name));
}

/* upb_EnumValueDef *************************************************************/

const google_protobuf_EnumValueOptions *upb_EnumValueDef_Options(
    const upb_EnumValueDef *e);
bool upb_EnumValueDef_HasOptions(const upb_EnumValueDef *e);
const char *upb_EnumValueDef_FullName(const upb_EnumValueDef *e);
const char *upb_EnumValueDef_Name(const upb_EnumValueDef *e);
int32_t upb_EnumValueDef_Number(const upb_EnumValueDef *e);
uint32_t upb_EnumValueDef_Index(const upb_EnumValueDef *e);
const upb_EnumDef *upb_EnumValueDef_Enum(const upb_EnumValueDef *e);

/* upb_filedef ****************************************************************/

const google_protobuf_FileOptions *upb_filedef_options(const upb_filedef *f);
bool upb_filedef_hasoptions(const upb_filedef *f);
const char *upb_filedef_name(const upb_filedef *f);
const char *upb_filedef_package(const upb_filedef *f);
const char *upb_filedef_phpprefix(const upb_filedef *f);
const char *upb_filedef_phpnamespace(const upb_filedef *f);
upb_syntax_t upb_filedef_syntax(const upb_filedef *f);
int upb_filedef_depcount(const upb_filedef *f);
int upb_filedef_publicdepcount(const upb_filedef *f);
int upb_filedef_weakdepcount(const upb_filedef *f);
int upb_filedef_toplvlmsgcount(const upb_filedef *f);
int upb_filedef_toplvlenumcount(const upb_filedef *f);
int upb_filedef_toplvlextcount(const upb_filedef *f);
int upb_filedef_servicecount(const upb_filedef *f);
const upb_filedef *upb_filedef_dep(const upb_filedef *f, int i);
const upb_filedef *upb_filedef_publicdep(const upb_filedef *f, int i);
const upb_filedef *upb_filedef_weakdep(const upb_filedef *f, int i);
const upb_MessageDef *upb_filedef_toplvlmsg(const upb_filedef *f, int i);
const upb_EnumDef *upb_filedef_toplvlenum(const upb_filedef *f, int i);
const upb_FieldDef *upb_filedef_toplvlext(const upb_filedef *f, int i);
const upb_servicedef *upb_filedef_service(const upb_filedef *f, int i);
const upb_symtab *upb_filedef_symtab(const upb_filedef *f);
const int32_t *_upb_filedef_publicdepnums(const upb_filedef *f);
const int32_t *_upb_filedef_weakdepnums(const upb_filedef *f);

/* upb_methoddef **************************************************************/

const google_protobuf_MethodOptions *upb_methoddef_options(
    const upb_methoddef *m);
bool upb_methoddef_hasoptions(const upb_methoddef *m);
const char *upb_methoddef_fullname(const upb_methoddef *m);
const char *upb_methoddef_name(const upb_methoddef *m);
const upb_servicedef *upb_methoddef_service(const upb_methoddef *m);
const upb_MessageDef *upb_methoddef_inputtype(const upb_methoddef *m);
const upb_MessageDef *upb_methoddef_outputtype(const upb_methoddef *m);
bool upb_methoddef_clientstreaming(const upb_methoddef *m);
bool upb_methoddef_serverstreaming(const upb_methoddef *m);

/* upb_servicedef *************************************************************/

const google_protobuf_ServiceOptions *upb_servicedef_options(
    const upb_servicedef *s);
bool upb_servicedef_hasoptions(const upb_servicedef *s);
const char *upb_servicedef_fullname(const upb_servicedef *s);
const char *upb_servicedef_name(const upb_servicedef *s);
int upb_servicedef_index(const upb_servicedef *s);
const upb_filedef *upb_servicedef_file(const upb_servicedef *s);
int upb_servicedef_methodcount(const upb_servicedef *s);
const upb_methoddef *upb_servicedef_method(const upb_servicedef *s, int i);
const upb_methoddef *upb_servicedef_lookupmethod(const upb_servicedef *s,
                                                 const char *name);

/* upb_symtab *****************************************************************/

upb_symtab *upb_symtab_new(void);
void upb_symtab_free(upb_symtab* s);
const upb_MessageDef *upb_symtab_lookupmsg(const upb_symtab *s, const char *sym);
const upb_MessageDef *upb_symtab_lookupmsg2(
    const upb_symtab *s, const char *sym, size_t len);
const upb_EnumDef *upb_symtab_lookupenum(const upb_symtab *s, const char *sym);
const upb_EnumValueDef *upb_symtab_lookupenumval(const upb_symtab *s,
                                               const char *sym);
const upb_FieldDef *upb_symtab_lookupext(const upb_symtab *s, const char *sym);
const upb_FieldDef *upb_symtab_lookupext2(const upb_symtab *s, const char *sym,
                                         size_t len);
const upb_filedef *upb_symtab_lookupfile(const upb_symtab *s, const char *name);
const upb_servicedef *upb_symtab_lookupservice(const upb_symtab *s,
                                               const char *name);
const upb_filedef *upb_symtab_lookupfileforsym(const upb_symtab *s,
                                               const char *name);
const upb_filedef *upb_symtab_lookupfile2(
    const upb_symtab *s, const char *name, size_t len);
const upb_filedef *upb_symtab_addfile(
    upb_symtab *s, const google_protobuf_FileDescriptorProto *file,
    upb_status *status);
size_t _upb_symtab_bytesloaded(const upb_symtab *s);
upb_arena *_upb_symtab_arena(const upb_symtab *s);
const upb_FieldDef *_upb_symtab_lookupextfield(const upb_symtab *s,
                                               const upb_msglayout_ext *ext);
const upb_FieldDef *upb_symtab_lookupextbynum(const upb_symtab *s,
                                              const upb_MessageDef *m,
                                              int32_t fieldnum);
const upb_extreg *upb_symtab_extreg(const upb_symtab *s);
const upb_FieldDef **upb_symtab_getallexts(const upb_symtab *s,
                                           const upb_MessageDef *m, size_t *count);

/* For generated code only: loads a generated descriptor. */
typedef struct upb_def_init {
  struct upb_def_init **deps;     /* Dependencies of this file. */
  const upb_msglayout_file *layout;
  const char *filename;
  upb_strview descriptor;         /* Serialized descriptor. */
} upb_def_init;

bool _upb_symtab_loaddefinit(upb_symtab *s, const upb_def_init *init);
void _upb_symtab_allownameconflicts(upb_symtab *s);

#include "upb/port_undef.inc"

#ifdef __cplusplus
}  /* extern "C" */
#endif  /* __cplusplus */

#endif /* UPB_DEF_H_ */
