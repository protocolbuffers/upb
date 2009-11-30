/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009 Joshua Haberman.  See LICENSE for details.
 *
 * This file defines two hash tables (keyed by integer and string) that are
 * templated on the type of value being stored.  This value must be no bigger
 * than a pointer.  The tables are optimized for:
 *  1. fast lookup
 *  2. small code size (template instantiations do not generate extra code).
 *
 * In particular, we are willing to trade insert performance for both of these.
 *
 * The table uses internal chaining with Brent's variation (inspired by the
 * Lua implementation of hash tables).  The hash function for strings is
 * Austin Appleby's "MurmurHash."
 */

#ifndef UPB_TABLE_H_
#define UPB_TABLE_H_

#include <assert.h>
#include "upb_misc.h"
#include "upb_string.h"

namespace upb {

// The base class.  Cannot be used directly, but implemented as a base so we
// can share code for insertion instead of duplicating between the two table types.
class Table {
 public:
  uint32_t count() const { return count_; }
  uint32_t size() const { return 1 << size_lg2_; }

 protected:
  typedef uint32_t HashValue;
  Table(uint32_t expected_num_entries);
  virtual ~Table();

  struct Entry {
    Entry() : is_empty(true), end_of_chain(true) {}
    Entry(void* k, void* v) : is_empty(false), key(k), value(v) {}
    int next_bucket:30;  // Internal chaining.
    int end_of_chain:1;     // Always set true if is_empty.
    int is_empty:1;
    void* key;  // Integer or ptr to string, depending on table type.
    void* value; // User-defined.
    // Copy and assign explicitly allowed.
  };

  // User-intended functions that derived classes wrapped type-safely.
  void InsertBase(const Entry& entry);
  Entry* BeginBase();
  Entry* NextBase(Entry *entry);
  uint32_t GetBucket(const Entry& entry) { return Hash(entry) & mask_; }

  // Having access to these as virtual methods lets us write a generic Insert
  // routine.
  virtual HashValue Hash(const Entry& entry) = 0;

  // Overridden in derived classes; for assertion-checking only.
  virtual Entry* Lookup(const Entry& e1) { return NULL; }

  Entry* buckets() { return buckets_.get(); }
  uint32_t mask() { return mask_; }

 private:
  uint32_t count_;       /* How many elements are currently in the table? */
  const uint8_t size_lg2_;     /* The table is 2^size_lg2 in size. */
  const uint32_t mask_;
  scoped_array<Entry> buckets_;
  DISALLOW_COPY_AND_ASSIGN(Table);
};

// A generic hash lookup function that is templated on the hash function and
// equality comparison.  We expect this to be inlined, and it must be very fast.
template<class E>
E* Lookup(E::Key key, E* buckets, uint32_t mask) {
  uint32_t bucket = E::Hash(key) & mask_;
  while (1) {
    E* e = buckets[bucket];
    // For an empty entry the key will be kInvalidKey, so this will always
    // return false.
    if(e->KeyEquals(key)) return entry;
    if(e->end_of_chain()) return NULL;
    bucket = e->next_bucketnum();
  }
}

template<class C>
class IntTable {
 public:
  IntTable(uint32_t expected_num_entries) : Table(expected_num_entries) {}
  virtual ~IntTable();

  void Insert(const Entry& entry) { InsertBase(entry); }
  Entry* Begin() { BeginBase(); }
  Entry* Next(Entry *entry) { NextBase(entry); }
  Entry* Lookup(Entry::Key key) { return Lookup(key, buckets(), mask()); }

  class Entry : public Table::Entry {
    typedef intptr_t Key;
    Entry(Key key, C value) : Table::Entry(key, value) {}
    Key key() const { return (Key)key_; }
    C value() const { return (C)value_; }
    HashValue Hash(Key key) const { return (uint32_t)key; }
    EqualsKey(Key key) const { return (Key)key_ == key; }
  };

 private:
  virtual HashValue Hash(const Table::Entry& entry) {
    return Entry::Hash((Entry)entry.key());
  }
#ifndef NDEBUG
  // This will cause the Lookup code to get emitted once per instantiation, and
  // we only use this for an assertion check in the base class.
  virtual Table::Entry* Lookup(const Table::Entry& e1) {
    return Lookup((Entry)e1.key());
  }
#endif
  DISALLOW_COPY_AND_ASSIGN(IntTable);
};

extern uint32_t MurmurHash2(const void *key, size_t len, uint32_t seed);

template<class C>
class StrTable {
 public:
  IntTable(uint32_t expected_num_entries) : Table(expected_num_entries) {}
  virtual ~IntTable();

  void Insert(const Entry& entry) { InsertBase(entry); }
  Entry* Begin() { BeginBase(); }
  Entry* Next(Entry *entry) { NextBase(entry); }
  Entry* Lookup(Entry::Key key) { return Lookup(key, entries_, mask_); }

  class Entry : public Table::Entry {
    typedef struct upb_string* Key;
    Key key() { return (Key)key_; }
    C value() { return (C)value_; }
    uint32_t Hash(Key key) {
      Key key = (Key)key_;
      return MurmurHash2(key->ptr, key->len);
    }
    EqualsKey(Key key) { return upb_streql((Key)key_, key); }
  };
#ifndef NDEBUG
  // This will cause the Lookup code to get emitted once per instantiation, and
  // we only use this for an assertion check in the base class.
  virtual Table::Entry* Lookup(const Table::Entry& e1) {
    return Lookup((Entry)e1.key());
  }
#endif
  DISALLOW_COPY_AND_ASSIGN(StrTable);
};

}  // namespace upb

#endif  /* UPB_TABLE_H_ */
