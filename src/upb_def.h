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
#include "upb_misc.h"
#include "upb_table.h"

struct google_protobuf_DescriptorProto;
struct google_protobuf_EnumDescriptorProto;
struct google_protobuf_FieldDescriptorProto;
struct google_protobuf_FileDescriptorSet;

namespace upb {

class MsgDef;
class EnumDef;
class UnresolvedDef;

class Def : public RefCounted {
 public:
  enum Type {
    kMessage,
    kEnum,
    kUnresolved,
  };

  upb_string* fqname() { return fqname_.get(); }
  Type type() { return type_; }

  MsgDef* DowncastMsgDef();
  UnresolvedDef* DowncastUnresolvedDef();

 protected:
  Def(struct upb_string *fqname) : fqname_(fqname) {}
  virtual ~Def() {}  // Refcounted.

 private:
  StringRef fqname_;
  Type type_;
  DISALLOW_COPY_AND_ASSIGN(Def);
};

// A FieldDef describes a single field in a message.  It isn't a full Def in
// the sense that it derives from Def.  It cannot stand on its own; it is
// either a field of a MsgDef or contained inside a ExtensionDef.
class FieldDef {
 public:
  upb_field_type_t type() const { return type_; }
  upb_label_t label() const { return label_; }
  upb_field_number_t number() const { return number_; }
  upb_string* name() { return name_.get(); }
  size_t byte_offset() { return byte_offset_; }
  int set_bit() { return field_index_; }

  bool IsSubMsg() const {
    return type_ == UPB_TYPE(GROUP) || type_ == UPB_TYPE(MESSAGE);
  }
  bool IsString() const {
    return type_ == UPB_TYPE(STRING) || type_ == UPB_TYPE(BYTES);
  }
  bool IsArray() const { return label_ == UPB_LABEL(REPEATED); }

  // Does the type of this field imply that it should contain an associated def?
  bool HasSubDef() const { return IsSubMsg() || type_ == UPB_TYPE(ENUM); }
  bool IsMM() const { return IsArray() || IsString() || IsSubMsg(); }
  bool ElemIsMM() { return IsString() || IsSubMsg(); }

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

  Def* subdef() { return subdef_.get(); }

 private:
  friend class MsgDef;
  friend class SymbolTable;
  explicit FieldDef(struct google_protobuf_FieldDescriptorProto *fd);
  virtual ~FieldDef() {}  // Refcounted.

  void ResetSubDef(Def* subdef) { subdef_.reset(subdef); }

  // Sort the given fielddefs in-place, according to what we think is an optimal
  // ordering of fields.  This can change from upb release to upb release.
  static void Sort(FieldDef** defs, size_t num);
  static void SortFds(struct google_protobuf_FieldDescriptorProto **fds,
                      size_t num);

  const upb_field_type_t type_;
  const upb_label_t label_;
  const upb_field_number_t number_;
  const StringRef name_;

  // These are set only when this fielddef is part of a msgdef.
  uint32_t byte_offset_;     // Where in a upb_msg to find the data.
  uint16_t field_index_;     // Indicates set bit.

  // For the case of an enum or a submessage, points to the def for that type.
  ScopedRef<Def> subdef_;
};


// Structure that describes a single .proto message type.
class MsgDef : public Def {
 public:
  // TODO: These should be private with Message friended.
  size_t size() { return size_; }
  int num_required_fields() { return num_required_fields_; }
  int set_flags_bytes() { return set_flags_bytes_; }

  // Looks up a field by name or number.  While these are written to be as fast
  // as possible, it will still be faster to cache the results of this lookup if
  // possible.  These return NULL if no such field is found.  Ownership is
  // retained by the MsgDef, and these are only guaranteed to live for as long
  // as the MsgDef does.
  FieldDef* FieldByNum(uint32_t num) { return fields_by_num_.LookupVal(num); }
  FieldDef* FieldByName(upb_string *name) {
    return fields_by_name_.LookupVal(name);
  }

  class FieldIterator {
   public:
    FieldDef* Get();
    bool Done();
    void Next();
  };
  FieldIterator* Begin();

 private:
  friend class SymbolTable;
  MsgDef(FieldDef** fields, int num_fields, upb_string *fqname);
  virtual ~MsgDef();  // Refcounted.

  struct upb_msg *default_msg_;   // Message with all default values set.
  uint32_t num_fields_;
  uint32_t set_flags_bytes_;
  uint32_t num_required_fields_;
  size_t size_;
  scoped_array<FieldDef*> fields_;

  // The num->field and name->field maps in upb_msgdef allow fast lookup of fields
  // by number or name.  These lookups are in the critical path of parsing and
  // field lookup, so they must be as fast as possible.
  typedef IntTable<FieldDef*> FieldsByNum;
  typedef StrTable<FieldDef*> FieldsByName;
  FieldsByNum::Type fields_by_num_;
  FieldsByName::Type fields_by_name_;
  DISALLOW_COPY_AND_ASSIGN(MsgDef);
};


class EnumDef : public Def {
 public:

 private:
  friend class SymbolTable;
  EnumDef(struct google_protobuf_EnumDescriptorProto *ed,
          struct upb_string *fqname);
  virtual ~EnumDef();  // Refcounted.

  int num_values_;
  typedef StrTable<uint32_t> NameToInt;
  typedef IntTable<struct upb_string*> IntToName;
  NameToInt::Type nametoint_;
  IntToName::Type inttoname_;
  DISALLOW_COPY_AND_ASSIGN(EnumDef);
};


// This is a placeholder definition that contains only the name of the type
// that should eventually be referenced.  Once symbols are resolved, this
// definition is replaced with a real definition.
class UnresolvedDef : public Def {
 public:

  upb_string* name() { return name_.get(); }

 private:
  friend class FieldDef;
  explicit UnresolvedDef(struct upb_string *name);
  StringRef name_;  // Not fully-qualified.
  DISALLOW_COPY_AND_ASSIGN(UnresolvedDef);
};


// A SymbolTable is where Defs live.  It is empty when first constructed.
// Clients add definitions to the context by supplying unserialized or
// serialized descriptors (as defined in descriptor.proto).
class SymbolTable : public RefCounted {
 public:
  SymbolTable();
  ~SymbolTable();

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
  Def* ResolveAndRef(struct upb_string *base, struct upb_string *symbol);

  // Finds an entry in the symbol table with this exact name.  Returns NULL if
  // no such symbol name has been defined.  The caller owns one ref to the
  // returned def.
  Def* LookupAndRef(struct upb_string *sym);

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
  ReaderWriterLock lock_;
  ScopedRef<MsgDef> fds_msgdef_;  // In psymtab, ptr here for convenience.

  typedef StrTable<Def*> TableType;
  Def* RefAndReturnDef(TableType::Entry* e);

  // Our symbol tables; we own refs to the defs therein.
  class Table;
  scoped_ptr<Table> symtab_;     // The context's symbol table.
  scoped_ptr<Table> psymtab_;    // Private symbols, for internal use.
  DISALLOW_COPY_AND_ASSIGN(SymbolTable);
};

}  // namespace upb

#endif  /* UPB_DEF_H_ */
