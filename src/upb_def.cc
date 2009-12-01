/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2008-2009 Joshua Haberman.  See LICENSE for details.
 */

#include "upb_def.h"
#include "descriptor.h"

namespace upb {

/* FieldDef *******************************************************************/

FieldDef::FieldDef(struct google_protobuf_FieldDescriptorProto *fd)
    : type_(fd->type),
      label_(fd->label),
      number_(fd->number),
      name_(upb_strdup(fd->name)) {
  if(fd->set_flags.has.type_name) {
    def_.reset(new UnresolvedDef(fd->type_name));
  }
}

// Callback for sorting fields.
static int compare_fields(const void *e1, const void *e2) {
  const google_protobuf_FieldDescriptorProto *fd1 = *(void**)e1;
  const google_protobuf_FieldDescriptorProto *fd2 = *(void**)e2;
  // Required fields go before non-required.
  bool req1 = fd1->label == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REQUIRED;
  bool req2 = fd2->label == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REQUIRED;
  if(req1 != req2) {
    return req2 - req1;
  } else {
    // Within required and non-required field lists, list in number order.
    // TODO: consider ordering by data size to reduce padding.
    return fd1->number - fd2->number;
  }
}

void FieldDef::SortFds(google_protobuf_FieldDescriptorProto **fds, size_t num) {
  qsort(fds, num, sizeof(*fds), compare_fields);
}

// Callback for sorting fields.
static int compare_fields2(const void *e1, const void *e2) {
  const struct upb_fielddef *f1 = e1;
  const struct upb_fielddef *f2 = e2;
  // Required fields go before non-required.
  bool req1 = f1->label == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REQUIRED;
  bool req2 = f2->label == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REQUIRED;
  if(req1 != req2) {
    return req2 - req1;
  } else {
    // Within required and non-required field lists, list in number order.
    // TODO: consider ordering by data size to reduce padding.
    return f1->number - f2->number;
  }
}

void FieldDef::Sort(struct upb_fielddef *defs, size_t num) {
  qsort(defs, num, sizeof(*defs), compare_fields2);
}

/* MsgDef *********************************************************************/

MsgDef::MsgDef(FieldDef* fields, int num_fields, struct upb_string *fqname)
    : Def(fqname),
      num_fields_(num_fields),
      set_flags_bytes_(div_round_up(num_fields, 8)),
      num_required_fields_(0),  // Incremented in the loop.
      size_(set_flags_bytes_),  // Incremented in the loop.
      fields_(fields),
      fields_by_num_(num_fields),
      fields_by_name_(num_fields) {
  size_t max_align = 0;
  for(int i = 0; i < num_fields; i++) {
    FieldDef* f = &fields_[i];
    struct upb_type_info *type_info = &upb_type_info[f->type];

    // General alignment rules are: each member must be at an address that is a
    // multiple of that type's alignment.  Also, the size of the structure as
    // a whole must be a multiple of the greatest alignment of any member. */
    f->field_index = i;
    f->byte_offset = ALIGN_UP(m->size, type_info->align);
    size_ = f->byte_offset + type_info->size;
    max_align = UPB_MAX(max_align, type_info->align);
    if(f->label == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REQUIRED) {
      // We currently rely on the fact that required fields are always sorted
      // to occur before non-required fields.
      num_required_fields_++;
    }

    // Insert into the tables.  Note that f->ref will be uninitialized, even in
    // the tables' copies of *f, which is why we must update them separately
    // in upb_msg_setref() below.
    fields_by_num_.Insert(f->number, f);
    fields_by_name_.Insert(f->name, f);
  }

  if(max_align > 0) size_ = ALIGN_UP(size_, max_align);
}

virtual MsgDef::~MsgDef() {
  // TODO
  for (unsigned int i = 0; i < m->num_fields; i++)
    upb_fielddef_uninit(&m->fields[i]);
}

void MsgDef::Resolve(FieldDef* f, Def* def) {
  f->SetDef(def);
}

/* EnumDef ********************************************************************/

EnumDef::EnumDef(struct google_protobuf_EnumDescriptorProto *ed,
                 struct upb_string *fqname)
    : Def(fqname),
      num_values_(ed->set_flags.has.value ? ed->value->len : 0),
      nametoint_(num_values_),
      inttoname_(num_values) {
  for(int i = 0; i < num_values; i++) {
    google_protobuf_EnumValueDescriptorProto *value = ed->value->elements[i];
    struct upb_string* str = upb_strdup(value->name);
    nametoint_.Insert(str, value->number);
    inttoname_.Insert(value->number, str);
  }
  return e;
}

virtual EnumDef::~EnumDef() {}

/* SymbolTable::Table *********************************************************/

// The actual symbol table.  We keep this separate because SymbolTable keeps a
// private internal symbol table in addition to its public one.
class SymbolTable::Table {
  Table() : symtab_(16) {}

  ~Table() {
    struct upb_symtab_entry *e = upb_strtable_begin(t);
    for(; e; e = upb_strtable_next(t, &e->e)) {
      upb_def_unref(e->def);
      upb_string_unref(e->e.key);
    }
    upb_strtable_free(t);
  }

  void AddFileDescriptor(const Table& existingdefs,
                         google_protobuf_FileDescriptorProto *fd, bool sort,
                         struct upb_status *status) {
    struct upb_string pkg = {.byte_len=0};
    if(fd->set_flags.has.package) pkg = *fd->package;

    if(fd->set_flags.has.message_type)
      for(unsigned int i = 0; i < fd->message_type->len; i++)
        InsertMessage(fd->message_type->elements[i], &pkg, sort, status);

    if(fd->set_flags.has.enum_type)
      for(unsigned int i = 0; i < fd->enum_type->len; i++)
        InsertEnum(fd->enum_type->elements[i], &pkg, status);

    if(!upb_ok(status)) return;

    // TODO: handle extensions and services.

    // Attempt to resolve all references.
    struct upb_symtab_entry *e;
    for(e = upb_strtable_begin(addto); e; e = upb_strtable_next(addto, &e->e)) {
      if(existingdefs.Lookup(e->e.key)) {  // TODO: leaks a ref?
        upb_seterr(status, UPB_STATUS_ERROR,
                   "attempted to redefine symbol '" UPB_STRFMT "'",
                   UPB_STRARG(e->e.key));
        return;
      }
      if(e->def->type() == UPB_DEF_MESSAGE) {
        MsgDef *m = upb_downcast_msgdef(e->def);
        for(unsigned int i = 0; i < m->num_fields; i++) {
          struct upb_fielddef *f = &m->fields[i];
          if(!upb_issubmsg(f) && f->type != UPB_TYPENUM(ENUM)) {
            // No resolving necessary.
            continue;
          }
          struct upb_def *def;
          struct upb_string *name = upb_downcast_unresolveddef(f->def)->name;
          if(upb_issubmsg(f))
            def = Resolve2(existingdefs, addto, e->e.key, name, UPB_DEF_MESSAGE);
          else if(f->type == UPB_TYPENUM(ENUM))
            def = Resolve2(existingdefs, addto, e->e.key, name, UPB_DEF_ENUM);
          if(!def) {
            upb_seterr(status, UPB_STATUS_ERROR,
                       "could not resolve symbol '" UPB_STRFMT "'"
                       " in context '" UPB_STRFMT "'",
                       UPB_STRARG(name), UPB_STRARG(e->e.key));
            return;
          }
          m->Resolve(f, def);
        }
      }
    }
  }

  Def* Resolve(struct upb_string *base, struct upb_string *symbol) {
    if(base->byte_len + symbol->byte_len + 1 >= UPB_SYMBOL_MAXLEN ||
       symbol->byte_len == 0) return NULL;

    if(symbol->ptr[0] == UPB_SYMBOL_SEPARATOR) {
      // Symbols starting with '.' are absolute, so we do a single lookup.
      struct upb_string sym_str = {.ptr = symbol->ptr+1,
                                   .byte_len = symbol->byte_len-1};
      return upb_strtable_lookup(t, &sym_str);
    } else {
      // Remove components from base until we find an entry or run out.
      char sym[UPB_SYMBOL_MAXLEN+1];
      struct upb_string sym_str = {.ptr = sym};
      int baselen = base->byte_len;
      while(1) {
        // sym_str = base[0...base_len] + UPB_SYMBOL_SEPARATOR + symbol
        memcpy(sym, base->ptr, baselen);
        sym[baselen] = UPB_SYMBOL_SEPARATOR;
        memcpy(sym + baselen + 1, symbol->ptr, symbol->byte_len);
        sym_str.byte_len = baselen + symbol->byte_len + 1;

        struct upb_symtab_entry *e = upb_strtable_lookup(t, &sym_str);
        if (e) return e;
        else if(baselen == 0) return NULL;  // No more scopes to try.

        baselen = my_memrchr(base->ptr, UPB_SYMBOL_SEPARATOR, baselen);
      }
    }
  }

  // As Resolve(), but tries to resolve the symbol in two different tables, in
  // order.
  static Def* Resolve2(Table *t1, Table *t2, struct upb_string *base,
                       struct upb_string *sym, enum DefType expected_type) {
    struct upb_symtab_entry *e = t1->Resolve(base, sym);
    if(e == NULL) e = t2->Resolve(base, sym);
    if(e && e->def->type == expected_type) return e->def;
    return NULL;
  }

 private:
  void InsertEnum(google_protobuf_EnumDescriptorProto *ed,
                  struct upb_string *base, struct upb_status *status) {
    if(!ed->set_flags.has.name) {
      upb_seterr(status, UPB_STATUS_ERROR,
                 "enum in context '" UPB_STRFMT "' does not have a name",
                 UPB_STRARG(base));
      return;
    }

    StringRef fqname = Join(base, ed->name);
    if(Lookup(fqname.get())) {  // TODO: leaks a ref?
      upb_seterr(status, UPB_STATUS_ERROR,
                 "attempted to redefine symbol '" UPB_STRFMT "'",
                 UPB_STRARG(fqname));
      return;
    }

    struct upb_symtab_entry e;
    e.e.key = fqname.get();  // Donating our ref to the table.
    e.def = new EnumDef(ed, fqname);
    upb_strtable_insert(t, &e.e);
  }

  void InsertMessage(google_protobuf_DescriptorProto *d,
                     struct upb_string *base, bool sort,
                     struct upb_status *status) {
    if(!d->set_flags.has.name) {
      upb_seterr(status, UPB_STATUS_ERROR,
                 "message in context '" UPB_STRFMT "' does not have a name",
                 UPB_STRARG(base));
      return;
    }

    /* We own this and must free it on destruct. */
    StringRef fqname = Join(base, d->name);
    if(Lookup(fqname)) {  // TODO: leaks a ref?
      upb_seterr(status, UPB_STATUS_ERROR,
                 "attempted to redefine symbol '" UPB_STRFMT "'",
                 UPB_STRARG(fqname));
      return;
    }

    // TODO
    struct upb_symtab_entry e;
    e.e.key = fqname;  // Donating our ref to the table.
    struct upb_fielddef *fielddefs = malloc(sizeof(*fielddefs) * d->field->len);
    for (unsigned int i = 0; i < d->field->len; i++) {
      google_protobuf_FieldDescriptorProto *fd = d->field->elements[i];
      upb_fielddef_init(&fielddefs[i], fd);
    }
    if(sort) upb_fielddef_sort(fielddefs, d->field->len);
    e.def = (struct upb_def*)upb_msgdef_new(fielddefs, d->field->len, fqname);
    upb_strtable_insert(t, &e.e);

    /* Add nested messages and enums. */
    if(d->set_flags.has.nested_type)
      for(unsigned int i = 0; i < d->nested_type->len; i++)
        insert_message(t, d->nested_type->elements[i], fqname, sort, status);

    if(d->set_flags.has.enum_type)
      for(unsigned int i = 0; i < d->enum_type->len; i++)
        insert_enum(t, d->enum_type->elements[i], fqname, status);
  }

  // Joins strings together by the symbol separator, for example:
  //   join("Foo.Bar", "Baz") -> "Foo.Bar.Baz"
  //   join("", "Baz") -> "Baz"
  // Caller owns a reference to the returned string.
  struct upb_string *Join(struct upb_string *base, struct upb_string *name) {
    size_t len = base->byte_len + name->byte_len;
    if(base->byte_len > 0) len++;  /* For the separator. */
    struct upb_string *joined = upb_string_new();
    upb_string_resize(joined, len);
    if(base->byte_len > 0) {
      /* nested_base = base + '.' +  d->name */
      memcpy(joined->ptr, base->ptr, base->byte_len);
      joined->ptr[base->byte_len] = UPB_SYMBOL_SEPARATOR;
      memcpy(&joined->ptr[base->byte_len+1], name->ptr, name->byte_len);
    } else {
      memcpy(joined->ptr, name->ptr, name->byte_len);
    }
    return joined;
  }

  struct Entry {
    struct upb_strtable_entry e;
    struct upb_def *def;
  };
};

/* SymbolTable ****************************************************************/

SymbolTable::SymbolTable() : symtab_(new Table), psymtab_(new Table) {
  // Add all the types in descriptor.proto so we can parse descriptors.
  google_protobuf_FileDescriptorProto *fd =
      upb_file_descriptor_set->file->elements[0]; /* We know there is only 1. */
  struct upb_status status = UPB_STATUS_INIT;
  psymtab_->AddFileDescriptor(symtab_, fd, false, &status);
  if(!upb_ok(&status)) {
    // Bootstrapping failed; upb is buggy or corrupt.
    fprintf(stderr, "Failed to initialize upb: %s.\n", status.msg);
    assert(false);
  }
  struct upb_string name = UPB_STRLIT("google.protobuf.FileDescriptorSet");
  struct upb_symtab_entry *e = upb_strtable_lookup(&c->psymtab, &name);
  assert(e);
  fds_msgdef_ = upb_downcast_msgdef(e->def);
  return c;
}

SymbolTable::~SymbolTable() {}

Def* SymbolTable::Lookup(struct upb_string *sym) {
  ReaderMutexLock l(&lock_);
  struct upb_symtab_entry *e = symtab_.Lookup(sym);
  return e ? e->def : NULL;
}

Def *SymbolTable::Resolve(struct upb_string *base, struct upb_string *symbol) {
  ReaderMutexLock l(&lock_);
  struct upb_symtab_entry *e = symtab_.Resolve(base, symbol);
  return e ? e->def : NULL;
}

void SymbolTable::AddFileDescriptorSet(google_protobuf_FileDescriptorSet *fds,
                                       struct upb_status *status) {
  if(fds->set_flags.has.file) {
    // Insert new symbols into a temporary table until we have verified that
    // the descriptor is valid.
    struct Table tmp;
    WriterMutexLock l(&lock_);
    for(uint32_t i = 0; i < fds->file->len; i++) {
      tmp.AddFileDescriptor(symtab_, fds->file->elements[i], true, status);
      if(!upb_ok(status)) return;
    }
    // All FileDescriptors were added successfully, add to main table.
    symtab_.InsertFrom(tmp);
  }
}

void SymbolTable::ParseFileDescriptorSet(struct upb_string *fds_str,
                                         struct upb_status *status)
{
  struct upb_msg *fds = upb_msg_new(fds_msgdef_);
  upb_msg_parsestr(fds, fds_str->ptr, fds_str->byte_len, status);
  if(!upb_ok(status)) return;
  AddFileDescriptorSet((google_protobuf_FileDescriptorSet*)fds, status);
}
