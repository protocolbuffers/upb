/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009 Joshua Haberman.  See LICENSE for details.
 *
 * This file defines two hash tables (keyed by integer and string) that are
 * templated on the type of value being stored.  The tables are optimized for:
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
#include <stdint.h>
#include "upb_misc.h"
#include "upb_string.h"

namespace upb {

extern uint32_t MurmurHash2(const void *key, size_t len, uint32_t seed);

// The base class.  Cannot be used directly, but implemented as a base so that
// bulky-but-non-time-critical code (mainly insert) can be emitted only once,
// instead of being re-instantiated for every derived class.
class TableBase {
 public:
  virtual ~TableBase();

  uint32_t count() const { return count_; }
  uint32_t size() const { return 1 << size_lg2_; }

 protected:
  typedef uint32_t HashVal;
  TableBase(uint32_t expected_num_entries);

  struct Entry {
    Entry() : is_empty(true), end_of_chain(true) {}
    unsigned int next_bucket:30;  // Internal chaining.
    int is_empty:1;
    int end_of_chain:1;  // Always set true if is_empty.
    // Copy and assign explicitly allowed.
  };

  void InsertBase(const Entry& entry);
  HashVal mask() const { return mask_; }

 private:
  void DoInsert(const Entry& e);
  HashVal GetEmptyBucket() const;
  void Swap(TableBase* other);
  uint32_t GetBucket(const Entry& entry) const { return Hash(entry) & mask_; }

  // Having access to these as virtual methods lets us write a generic Insert
  // routine.
  virtual HashVal Hash(const Entry& entry) const = 0;
  virtual Entry* GetEntryForBucket(uint32_t bucket) const = 0;
  virtual void CopyEntry(const Entry& src, Entry* dst) const = 0;
  virtual void SwapDerived(TableBase* other) = 0;
  virtual TableBase* NewOfSameType(int num) const = 0;

#ifndef NDEBUG
  // Overridden in derived classes; for assertion-checking only.
  virtual Entry* VirtualLookup(const Entry& e1) const { (void)e1; return NULL; }
#endif

  uint32_t count_;       /* How many elements are currently in the table? */
  const uint8_t size_lg2_;     /* The table is 2^size_lg2 in size. */
  const uint32_t mask_;
  DISALLOW_COPY_AND_ASSIGN(TableBase);
};

// A Table that is templated on the entry type.  The Entry object is used as
// the storage for each entry, as well as supplying implementations of hashing,
// and comparing.
//
// Insertions are handled by the base class.  We implement virtual functions
// that are necessary for this to happen.  It is safe to cast entries to our
// specific entry type, because the base class only obtains entries from us
// (which are always of the right type).
template<class E>
class Table : public TableBase {
 public:
  typedef E Entry;
  Table(uint32_t expected_num_entries) : TableBase(expected_num_entries) {
    buckets_.reset(new E[size()]);
  }
  virtual ~Table() {}

  // Inserts the given key and value.  Duplicate insertions are not allowed;
  // to modify an existing value, look it up and modify the returned entry.
  void Insert(typename E::Key key, typename E::Val value) {
    InsertBase(E(key, value));
  }

  // Lookup a value by key.  Returns the entry if found, otherwise NULL.  The
  // entry's value may be modified as desired.
  E* Lookup(typename E::Key key) const {
    uint32_t bucket = E::Hash(key) & mask();
    while (1) {
      E* e = &buckets_[bucket];
      // For an empty entry the key will be kInvalidKey, so this will always
      // return false.
      if(e->EqualsKey(key)) return e;
      if(e->end_of_chain) return NULL;
      bucket = e->next_bucket;
    }
  }

  typename E::Val LookupVal(typename E::Key key) const {
    E* e = Lookup(key);
    return e ? e->value() : typename E::Val();
  }

  // Iteration over the table, as in:
  //   for (Table<Foo>::Entry* e = table.Begin(); e; e = table.Next(e)) {
  //     // ...
  //   }
  virtual E* Begin() const { return Next(buckets_.get() - 1); }
  virtual E* Next(E *entry) const {
    do {
      if (++entry == &buckets_[size()]) return NULL;
    } while (entry->is_empty);
    return entry;
  }

 private:
  scoped_array<E> buckets_;

  virtual TableBase::HashVal Hash(const TableBase::Entry& entry) const {
    return E::Hash(static_cast<const E&>(entry).key());
  }
  virtual Entry* GetEntryForBucket(uint32_t bucket) const {
    return &buckets_[bucket];
  }
  virtual void CopyEntry(const TableBase::Entry& src,
                         TableBase::Entry* dst) const {
    *static_cast<E*>(dst) = static_cast<const E&>(src);
  }
  virtual TableBase* NewOfSameType(int num) const { return new Table<E>(num); }
  virtual void SwapDerived(TableBase* other) {
    buckets_.swap(static_cast<Table<E>*>(other)->buckets_);
  }
#ifndef NDEBUG
  // This will cause the Lookup code to get emitted once per instantiation, and
  // we only use this for an assertion check in the base class.
  virtual TableBase::Entry* VirtualLookup(const TableBase::Entry& entry) const {
    return Lookup(static_cast<const E&>(entry).key());
  }
#endif
  DISALLOW_COPY_AND_ASSIGN(Table);
};

// An Entry class that can be used as base classes for other entries.  This
// supplies useful functionality like storing a key and value.
template<class K, class Val>
class TableEntry : public TableBase::Entry {
 public:
  typedef K Key;
  TableEntry() : TableBase::Entry(), key_(0) {}
  TableEntry(Key k, Val v) : TableBase::Entry(), key_(k), value_(v) {
    is_empty = false;
  }
  Key key() const { return key_; }  // Key not settable!
  Val value() const { return value_; }
  void set_value(Val val) { value_ = val; }

 private:
  Key key_;
  Val value_;
};


// Entry classes for int32 and string.  These define a hashing scheme.
template<class V>
class IntTableEntry : public TableEntry<uint32_t, V> {
 public:
  typedef uint32_t Key;
  typedef V Val;
  IntTableEntry() : TableEntry<Key, Val>() {}
  IntTableEntry(Key key, Val value) : TableEntry<Key, Val>(key, value) {}

  // Hash function and equality operation.
  static TableBase::HashVal Hash(Key key) { return key; }  // Identity.
  bool EqualsKey(Key k) const { return k == TableEntry<Key, Val>::key(); }
};

template<class V>
class StrTableEntry : public TableEntry<upb_string*, V> {
 public:
  typedef upb_string* Key;
  typedef V Val;
  StrTableEntry() : TableEntry<Key, Val>() {}
  StrTableEntry(Key key, Val value) : TableEntry<Key, Val>(key, value) {}

  // Hash function and equality operation.
  static TableBase::HashVal Hash(Key k) {
    return MurmurHash2(k->ptr, k->byte_len, 0);
  }
  bool EqualsKey(Key key) { return upb_streql(key, TableEntry<Key, V>::key()); }
};

// Convenience definitions for defining IntTables and StrTables templated only
// on the value type, eg.
//   IntTable<Foo*>::Type  // maps integers to Foo*.
template<class Val>
struct IntTable {
  typedef IntTableEntry<Val> Entry;
  typedef Table<Entry> Type;
};

template<class Val>
struct StrTable {
  typedef StrTableEntry<Val> Entry;
  typedef Table<Entry> Type;
};

}  // namespace upb

#endif  /* UPB_TABLE_H_ */
