/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009 Joshua Haberman.  See LICENSE for details.
 *
 * Provides definitions of .proto constructs:
 * - upb_msgdef: describes a "message" construct.
 * - upb_fielddef: describes a message field.
 * - upb_enumdef: describes an enum.
 * (TODO: descriptions of extensions and services).
 *
 * Defs should be obtained from a upb_context object; the APIs for creating
 * them directly are internal-only.
 *
 * Defs are immutable and reference-counted.  Contexts reference any defs
 * that are the currently in their symbol table.  If an extension is loaded
 * that adds a field to an existing message, a new msgdef is constructed that
 * includes the new field and the old msgdef is unref'd.  The old msgdef will
 * still be ref'd by message (if any) that were constructed with that msgdef.
 *
 * This file contains routines for creating and manipulating the definitions
 * themselves.  To create and manipulate actual messages, see upb_msg.h.
 */

#ifndef UPB_DEF_H_
#define UPB_DEF_H_

#include "upb_atomic.h"
#include "upb_table.h"

struct google_protobuf_DescriptorProto;
struct google_protobuf_EnumDescriptorProto;
struct google_protobuf_FieldDescriptorProto;
struct google_protobuf_FileDescriptorSet;

namespace upb {

class Def : public RefCounted {
  Def(struct upb_string *fqname) : fqname_(fqname) {}

 private:
  virtual ~Def() {}  // Refcounted.
  StringRef fqname_;
  DISALLOW_COPY_AND_ASSIGN(Def);
};

// A FieldDef describes a single field in a message.  It isn't a full Def in
// the sense that it derives from Def.  It cannot stand on its own; it is
// either a field of a MsgDef or contained inside a ExtensionDef.
class FieldDef {
 public:
  explicit FieldDef(struct google_protobuf_FieldDescriptorProto *fd);

  bool IsSubMsg() { return upb_issubmsgtype(f->type); }
  bool IsString() { return upb_isstringtype(f->type); }
  bool IsArray() {
    return f->label == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REPEATED;
  }
  // Does the type of this field imply that it should contain an associated def?
  bool HasDef() { return upb_issubmsg(f) || f->type == UPB_TYPENUM(ENUM); }
  bool IsMM() { return upb_isarray(f) || upb_isstring(f) || upb_issubmsg(f); }
  bool ElemIsMM() { return upb_isstring(f) || upb_issubmsg(f); }

  /* Defined iff IsMM(). */
  upb_mm_ptrtype PtrType() {
    if(IsArray()) return UPB_MM_ARR_REF;
    else if(IsString()) return UPB_MM_STR_REF;
    else if(IsSubMsg()) return UPB_MM_MSG_REF;
    else return -1;
  }

  /* Defined iff ElemIsMM(). */
  upb_mm_ptrtype ElemPtrType() {
    if(IsString()) return UPB_MM_STR_REF;
    else if(IsSubMsg()) return UPB_MM_MSG_REF;
    else return -1;
  }

 private:
  virtual ~FieldDef() {}  // Refcounted.
  // Sort the given fielddefs in-place, according to what we think is an optimal
  // ordering of fields.  This can change from upb release to upb release.
  static void Sort(struct upb_fielddef *defs, size_t num);
  static void SortFds(struct google_protobuf_FieldDescriptorProto **fds,
                      size_t num);

  upb_field_type_t type_;
  upb_label_t label_;
  upb_field_number_t number_;
  StringRef name_;

  // These are set only when this fielddef is part of a msgdef.
  uint32_t byte_offset_;     // Where in a upb_msg to find the data.
  uint16_t field_index_;     // Indicates set bit.

  // For the case of an enum or a submessage, points to the def for that type.
  ScopedRef<Def> def_;
}


// Structure that describes a single .proto message type.
class MsgDef : public Def {
  MsgDef(Field *fields, int num_fields, struct upb_string *fqname);

  // Looks up a field by name or number.  While these are written to be as fast
  // as possible, it will still be faster to cache the results of this lookup if
  // possible.  These return NULL if no such field is found.  Ownership is
  // retained by the MsgDef, and these are only guaranteed to live for as long
  // as the MsgDef does.
  FieldDef *FieldByNum(uint32_t num) { return fields_by_num_.Lookup(num); }
  FieldDef *FieldByName(struct upb_string *name) {
    return fields_by_num_.Lookup(name);
  }

 private:
  friend class SymbolTable;
  virtual ~MsgDef();  // Refcounted.

  // The SymbolTable uses this function to resolve the "ref" field in the given
  // FieldDef.  Since messages can refer to each other in mutually-recursive
  // ways, this step must be separated from initialization.
  void Resolve(FieldDef* f, Def* def);

  struct upb_msg *default_msg_;   // Message with all default values set.
  size_t size_;
  uint32_t num_fields_;
  uint32_t set_flags_bytes_;
  uint32_t num_required_fields_;
  scoped_array<FieldDef> fields_;

  // The num->field and name->field maps in upb_msgdef allow fast lookup of fields
  // by number or name.  These lookups are in the critical path of parsing and
  // field lookup, so they must be as fast as possible.
  IntTable<FieldDef*> fields_by_num_;
  StrTable<FieldDef*> fields_by_name_;
  DISALLOW_COPY_AND_ASSIGN(MsgDef);
};


class EnumDef : public Def {
  EnumDef(struct google_protobuf_EnumDescriptorProto *ed,
          struct upb_string *fqname);

 private:
  virtual ~EnumDef();  // Refcounted.

  StrTable<uint32_t> nametoint_;
  IntTable<struct upb_string*> inttoname_;
  DISALLOW_COPY_AND_ASSIGN(EnumDef);
};


// This is a placeholder definition that contains only the name of the type
// that should eventually be referenced.  Once symbols are resolved, this
// definition is replaced with a real definition.
class UnresolvedDef {
  explicit UnresolvedDef(struct upb_string *name);
 private:
  StringRef name_;  // Not fully-qualified.
  DISALLOW_COPY_AND_ASSIGN(UnresolvedDef);
};


// A SymbolTable is where Defs live.  It is empty when first constructed.
// Clients add definitions to the context by supplying unserialized or
// serialized descriptors (as defined in descriptor.proto).
class SymbolTable : public RefCounted {
 public:
  SymbolTable();

  // Resolves the given symbol using the rules described in descriptor.proto,
  // namely:
  //
  //    If the name starts with a '.', it is fully-qualified.  Otherwise, C++-like
  //    scoping rules are used to find the type (i.e. first the nested types
  //    within this message are searched, then within the parent, on up to the
  //    root namespace).
  //
  // Returns NULL if no such symbol has been defined.  The caller owns one ref
  // to the returned def.
  Def* Resolve(struct upb_string *base, struct upb_string *symbol);

  // Finds an entry in the symbol table with this exact name.  Returns NULL if
  // no such symbol name has been defined.  The caller owns one ref to the
  // returned def.
  Def* Lookup(struct upb_string *sym);

  // Accepts a visitor and calls the appropriate method for each symbol.  This
  // is performed with the table's internal lock held, so the visitor must not
  // block or perform any long-running operation.  If the client wants to keep
  // a reference to any of the defs, it must ref them before the calback
  // returns.
  void Accept(DefVisitor* visitor);

  // Adds the definitions in the given file descriptor to this context.  All
  // types that are referenced from fd must have previously been defined (or be
  // defined in fd).  fd may not attempt to define any names that are already
  // defined in this context.  Caller retains ownership of fd.  status indicates
  // whether the operation was successful or not, and the error message (if any).
  void AddFileDescriptorSet(struct google_protobuf_FileDescriptorSet *fds,
                            struct upb_status *status);
  // Like the above, but also parses the FileDescriptorSet from fds.
  void ParseFileDescriptorSet(struct upb_string *fds, struct upb_status *status);

 private:
  ReaderWriterLock lock_;          // Protects all members except the refcount.
  ScopedRef<MsgDef> fds_msgdef_;  // In psymtab, ptr here for convenience.

  // Our symbol tables; we own refs to the defs therein.
  class Table;
  scoped_ptr<Table> symtab_;     // The context's symbol table.
  scoped_ptr<Table> psymtab_;    // Private symbols, for internal use.
  DISALLOW_COPY_AND_ASSIGN(SymbolTable);
};

}  // namespace upb

#endif  /* UPB_DEF_H_ */
