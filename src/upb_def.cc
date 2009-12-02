/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2008-2009 Joshua Haberman.  See LICENSE for details.
 */

#include "upb_def.h"
#include "upb_msg.h"
#include "descriptor.h"

namespace upb {

/* FieldDef *******************************************************************/

FieldDef::FieldDef(struct google_protobuf_FieldDescriptorProto *fd)
    : type_(fd->type),
      label_(fd->label),
      number_(fd->number),
      name_(upb_strdup(fd->name)) {
  if(fd->set_flags.has.type_name) {
    subdef_.reset(new UnresolvedDef(fd->type_name));
  }
}

// Callback for sorting fields.
static int compare_fields(upb_label_t label1, upb_field_number_t num1,
                          upb_label_t label2, upb_field_number_t num2) {
  // Required fields go before non-required.
  bool req1 = label1 == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REQUIRED;
  bool req2 = label2 == GOOGLE_PROTOBUF_FIELDDESCRIPTORPROTO_LABEL_REQUIRED;
  if(req1 != req2) {
    return req2 - req1;
  } else {
    // Within required and non-required field lists, list in number order.
    // TODO: consider ordering by data size to reduce padding.
    return num1 - num2;
  }
}

static int compare_desc(const void *e1, const void *e2) {
  const google_protobuf_FieldDescriptorProto *fd1 = *(google_protobuf_FieldDescriptorProto**)e1;
  const google_protobuf_FieldDescriptorProto *fd2 = *(google_protobuf_FieldDescriptorProto**)e2;
  return compare_fields(fd1->label, fd1->number, fd2->label, fd2->number);
}

static int compare_defs(const void *e1, const void *e2) {
  const struct FieldDef *f1 = *(FieldDef**)e1;
  const struct FieldDef *f2 = *(FieldDef**)e2;
  return compare_fields(f1->label(), f1->number(), f2->label(), f2->number());
}


void FieldDef::SortFds(google_protobuf_FieldDescriptorProto **fds, size_t num) {
  qsort(fds, num, sizeof(*fds), compare_desc);
}

void FieldDef::Sort(FieldDef** defs, size_t num) {
  qsort(defs, num, sizeof(*defs), compare_defs);
}

/* MsgDef *********************************************************************/

MsgDef::MsgDef(FieldDef** fields, int num_fields, struct upb_string *fqname)
    : Def(fqname),
      num_fields_(num_fields),
      set_flags_bytes_(DivRoundUp(num_fields, 8)),
      num_required_fields_(0),  // Incremented in the loop.
      size_(set_flags_bytes_),  // Incremented in the loop.
      fields_(fields),
      fields_by_num_(num_fields),
      fields_by_name_(num_fields) {
  size_t max_align = 0;
  for(int i = 0; i < num_fields; i++) {
    FieldDef* f = fields_[i];
    struct upb_type_info *type_info = &upb_type_info[f->type()];

    // General alignment rules are: each member must be at an address that is a
    // multiple of that type's alignment.  Also, the size of the structure as
    // a whole must be a multiple of the greatest alignment of any member. */
    f->field_index_ = i;
    f->byte_offset_ = ALIGN_UP(size_, type_info->align);
    size_ = f->byte_offset_ + type_info->size;
    max_align = UPB_MAX(max_align, type_info->align);
    if(f->label() == UPB_LABEL(REQUIRED)) {
      // We currently rely on the fact that required fields are always sorted
      // to occur before non-required fields.
      num_required_fields_++;
    }

    // Insert into the tables.  Note that f->ref will be uninitialized, even in
    // the tables' copies of *f, which is why we must update them separately
    // in upb_msg_setref() below.
    fields_by_num_.Insert(f->number(), f);
    fields_by_name_.Insert(f->name(), f);
  }
  if(max_align > 0) size_ = ALIGN_UP(size_, max_align);
}

MsgDef::~MsgDef() {}

/* EnumDef ********************************************************************/

EnumDef::EnumDef(struct google_protobuf_EnumDescriptorProto *ed,
                 struct upb_string *fqname)
    : Def(fqname),
      num_values_(ed->set_flags.has.value ? ed->value->len : 0),
      nametoint_(num_values_),
      inttoname_(num_values_) {
  for(int i = 0; i < num_values_; i++) {
    google_protobuf_EnumValueDescriptorProto *value = ed->value->elements[i];
    struct upb_string* str = upb_strdup(value->name);
    nametoint_.Insert(str, value->number);
    inttoname_.Insert(value->number, str);
  }
}

EnumDef::~EnumDef() {}

/* SymbolTable::Table *********************************************************/

// The actual symbol table.  We keep this separate because SymbolTable keeps a
// private internal symbol table in addition to its public one.
class SymbolTable::Table {
 public:
  Table() : table_(16) {}

  ~Table() {
    for (TableType::Entry *e = table_.Begin(); e; e = table_.Next(e)) {
      upb_string_unref(e->key());
      e->value()->Unref();
    }
  }

  void AddFileDescriptor(const Table& existing_defs,
                         google_protobuf_FileDescriptorProto *fd, bool sort,
                         struct upb_status *status) {
    struct upb_string *pkg;
    if (fd->set_flags.has.package)
      pkg = fd->package;
    else
      pkg = upb_string_new();

    if(fd->set_flags.has.message_type)
      for(unsigned int i = 0; i < fd->message_type->len; i++)
        InsertMessage(fd->message_type->elements[i], pkg, sort, status);

    if(fd->set_flags.has.enum_type)
      for(unsigned int i = 0; i < fd->enum_type->len; i++)
        InsertEnum(fd->enum_type->elements[i], pkg, status);

    if(!upb_ok(status)) return;

    // TODO: handle extensions and services.

    // Attempt to resolve all references.
    for (TableType::Entry *e = table_.Begin(); e; e = table_.Next(e)) {
      if (existing_defs.Contains(e->value()->fqname())) {
        upb_seterr(status, UPB_STATUS_ERROR,
                   "attempted to redefine symbol '" UPB_STRFMT "'",
                   UPB_STRARG(e->value()->fqname()));
        return;
      }

      // Only need to resolve references for fields in messages.
      MsgDef* m = e->value()->DowncastMsgDef();
      if (!m) continue;
      upb_string* base = m->fqname();

      for (MsgDef::FieldIterator* iter = m->Begin(); !iter->Done(); iter->Next()) {
        FieldDef* f = iter->Get();
        if (!f->HasSubDef()) continue; // No resolving necessary.
        Def::Type expected_type = f->IsSubMsg() ? Def::kMessage : Def::kEnum;
        UnresolvedDef* u_def = f->subdef()->DowncastUnresolvedDef();
        assert(u_def);
        upb_string* name = u_def->name();

        TableType::Entry* e = existing_defs.Resolve(base, name);
        if (!e) e = Resolve(base, name);
        if (!e) {
          upb_seterr(status, UPB_STATUS_ERROR,
                     "could not resolve symbol '" UPB_STRFMT "'"
                     " in context '" UPB_STRFMT "'",
                     UPB_STRARG(name), UPB_STRARG(base));
          return;
        } else if (expected_type != e->value()->type()) {
          upb_seterr(status, UPB_STATUS_ERROR,
                     "symbol '" UPB_STRFMT "' referenced from context "
                     "'" UPB_STRFMT "' did not have expected type.",
                     UPB_STRARG(name), UPB_STRARG(base));
          return;
        }
        f->ResetSubDef(e->value());
      }
    }
  }

  TableType::Entry* Resolve(upb_string* base, upb_string* symbol) const {
    if(base->byte_len + symbol->byte_len + 1 >= UPB_SYMBOL_MAXLEN ||
       symbol->byte_len == 0) return NULL;

    if(symbol->ptr[0] == UPB_SYMBOL_SEPARATOR) {
      // Symbols starting with '.' are absolute, so we do a single lookup.
      struct upb_string sym_str;
      sym_str.ptr = symbol->ptr+1;
      sym_str.byte_len = symbol->byte_len-1;
      return table_.Lookup(&sym_str);
    } else {
      // Remove components from base until we find an entry or run out.
      char sym[UPB_SYMBOL_MAXLEN+1];
      struct upb_string sym_str;
      sym_str.ptr = sym;
      int baselen = base->byte_len;
      while(1) {
        // sym_str = base[0...base_len] + UPB_SYMBOL_SEPARATOR + symbol
        memcpy(sym, base->ptr, baselen);
        sym[baselen] = UPB_SYMBOL_SEPARATOR;
        memcpy(sym + baselen + 1, symbol->ptr, symbol->byte_len);
        sym_str.byte_len = baselen + symbol->byte_len + 1;

        TableType::Entry* e = table_.Lookup(&sym_str);
        if (e) return e;
        if (baselen == 0) return NULL;  // No more scopes to try.

        baselen = memrchr(base->ptr, UPB_SYMBOL_SEPARATOR, baselen);
      }
    }
  }

  TableType::Entry* Lookup(upb_string* sym) const { return table_.Lookup(sym); }

  bool Contains(upb_string* fqname) const {
    return table_.Lookup(fqname) != NULL;
  }

  void InsertFrom(const Table& t) { (void)t; }

 private:
  upb_string* TryDefine(bool name_defined, upb_string* name, upb_string* base,
                       struct upb_status* status) {
    if(!name_defined) {
      upb_seterr(status, UPB_STATUS_ERROR,
                 "enum in context '" UPB_STRFMT "' does not have a name",
                 UPB_STRARG(base));
      return NULL;
    }
    StringRef fqname(Join(base, name), StringRef::kNew);
    if(Contains(fqname.get())) {
      upb_seterr(status, UPB_STATUS_ERROR,
                 "attempted to redefine symbol '" UPB_STRFMT "'",
                 UPB_STRARG(fqname));
      return NULL;
    }
    return fqname.release();
  }

  void InsertEnum(google_protobuf_EnumDescriptorProto *ed,
                  struct upb_string *base, struct upb_status *status) {
    StringRef fqname(TryDefine(ed->set_flags.has.name, ed->name, base, status), StringRef::kNew);
    if (!fqname.get()) return;
    table_.Insert(fqname.release(), new EnumDef(ed, fqname.get()));
  }

  void InsertMessage(google_protobuf_DescriptorProto *d,
                     struct upb_string *base, bool sort,
                     struct upb_status *status) {
    StringRef fqname(TryDefine(d->set_flags.has.name, d->name, base, status));

    int num_fields = d->set_flags.has.field ? d->field->len : 0;
    FieldDef** fielddefs = new FieldDef*[num_fields];
    for (int i = 0; i < num_fields; i++)
      fielddefs[i] = new FieldDef(d->field->elements[i]);
    if(sort) FieldDef::Sort(fielddefs, d->field->len);

    table_.Insert(fqname.release(), new MsgDef(fielddefs, num_fields, fqname.get()));

    // Add nested messages and enums.
    if(d->set_flags.has.nested_type)
      for(unsigned int i = 0; i < d->nested_type->len; i++)
        InsertMessage(d->nested_type->elements[i], fqname.get(), sort, status);

    if(d->set_flags.has.enum_type)
      for(unsigned int i = 0; i < d->enum_type->len; i++)
        InsertEnum(d->enum_type->elements[i], fqname.get(), status);
  }

  // Joins strings together by the symbol separator, for example:
  //   join("Foo.Bar", "Baz") -> "Foo.Bar.Baz"
  //   join("", "Baz") -> "Baz"
  // Caller owns a reference to the returned string.
  struct upb_string *Join(struct upb_string *base, struct upb_string *name) {
    size_t len = base->byte_len + name->byte_len;
    if(base->byte_len > 0) len++;  // For the separator.
    struct upb_string *joined = upb_string_new();
    upb_string_resize(joined, len);
    if(base->byte_len > 0) {
      // nested_base = base + '.' +  d->name
      memcpy(joined->ptr, base->ptr, base->byte_len);
      joined->ptr[base->byte_len] = UPB_SYMBOL_SEPARATOR;
      memcpy(&joined->ptr[base->byte_len+1], name->ptr, name->byte_len);
    } else {
      memcpy(joined->ptr, name->ptr, name->byte_len);
    }
    return joined;
  }

  TableType::Table table_;
};

/* SymbolTable ****************************************************************/

SymbolTable::SymbolTable() : symtab_(new Table), psymtab_(new Table) {
  // Add all the types in descriptor.proto to the private table so we can parse
  // descriptors.
  google_protobuf_FileDescriptorProto *fd =
      upb_file_descriptor_set->file->elements[0]; // We know there is only 1.
  struct upb_status status = UPB_STATUS_INIT;
  psymtab_->AddFileDescriptor(*symtab_, fd, false, &status);
  if(!upb_ok(&status)) {
    // Bootstrapping failed; upb is buggy or corrupt.
    fprintf(stderr, "Failed to initialize upb: %s.\n", status.msg);
    assert(false);
  }
  struct upb_string* name = upb_strdupc("google.protobuf.FileDescriptorSet");
  Def* def = LookupAndRef(name);
  assert(def);
  fds_msgdef_.reset(def->DowncastMsgDef());
  def->Unref();
  upb_string_unref(name);
}

SymbolTable::~SymbolTable() {}

Def* SymbolTable::LookupAndRef(struct upb_string *sym) {
  ReaderMutexLock l(&lock_);
  RefAndReturnDef(symtab_->Lookup(sym));
}

Def *SymbolTable::ResolveAndRef(struct upb_string *base, struct upb_string *symbol) {
  ReaderMutexLock l(&lock_);
  RefAndReturnDef(symtab_->Resolve(base, symbol));
}

Def *SymbolTable::RefAndReturnDef(TableType::Entry* e) {
  if (e) {
    Def* def = e->value();
    def->Ref();
    return def;
  } else {
    return NULL;
  }
}

void SymbolTable::AddFileDescriptorSet(google_protobuf_FileDescriptorSet *fds,
                                       struct upb_status *status) {
  if(fds->set_flags.has.file) {
    // Insert new symbols into a temporary table until we have verified that
    // the descriptor is valid.
    struct Table tmp;
    WriterMutexLock l(&lock_);
    for(uint32_t i = 0; i < fds->file->len; i++) {
      tmp.AddFileDescriptor(*symtab_, fds->file->elements[i], true, status);
      if(!upb_ok(status)) return;
    }
    // All FileDescriptors were added successfully, add to main table.
    symtab_->InsertFrom(tmp);
  }
}

void SymbolTable::ParseFileDescriptorSet(struct upb_string *fds_str,
                                         struct upb_status *status) {
  struct upb_msg *fds = upb_msg_new(fds_msgdef_);
  upb_msg_parsestr(fds, fds_str->ptr, fds_str->byte_len, status);
  if(!upb_ok(status)) return;
  AddFileDescriptorSet((google_protobuf_FileDescriptorSet*)fds, status);
}

}  // namespace upb
