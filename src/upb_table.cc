/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009 Joshua Haberman.  See LICENSE for details.
 */

#include "upb_table.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace upb {

static const double MAX_LOAD = 0.85;

TableBase::TableBase(uint32_t expected_num_entries)
    : count_(0),
      size_lg2_(ceil(log2(expected_num_entries / MAX_LOAD))),
      mask_(size() - 1) {}

TableBase::~TableBase() {}

TableBase::HashVal TableBase::GetEmptyBucket() const {
  // TODO: does it matter that this is biased towards the front of the table?
  for (uint32_t i = 0; i < size(); i++)
    if (GetEntryForBucket(i)->is_empty) return i;
  assert(false);
  return 0;
}

void TableBase::DoInsert(const Entry& e) {
  assert(VirtualLookup(e) == NULL);
  count_++;
  HashVal bucket = GetBucket(e);
  Entry* table_e = GetEntryForBucket(bucket);
  if(!table_e->is_empty) {  // Collision.
    HashVal collider_bucket = GetBucket(*table_e);
    if(bucket == collider_bucket) {
      // Existing element is in its main posisiton.  Find an empty slot to
      // place our new element and append it to this key's chain.
      HashVal empty_bucket = GetEmptyBucket();
      while (!table_e->end_of_chain)
        table_e = GetEntryForBucket(table_e->next_bucket);
      table_e->next_bucket = empty_bucket;
      table_e->end_of_chain = false;
      table_e = GetEntryForBucket(empty_bucket);
    } else {
      // Existing element is not in its main position.  Move it to an empty
      // slot and put our element in its main position.
      HashVal empty_bucket = GetEmptyBucket();
      CopyEntry(*table_e, GetEntryForBucket(empty_bucket));  // copies next.
      Entry* collider_chain_e = GetEntryForBucket(collider_bucket);
      while(1) {
        assert(!collider_chain_e->is_empty);
        assert(!collider_chain_e->end_of_chain);
        if(collider_chain_e->next_bucket == bucket) {
          collider_chain_e->next_bucket = empty_bucket;
          break;
        }
        collider_chain_e = GetEntryForBucket(collider_chain_e->next_bucket);
      }
      // table_e remains set to our mainpos.
    }
  }
  CopyEntry(e, table_e);
  table_e->end_of_chain = true;
  assert(VirtualLookup(e) == table_e);
}

void TableBase::InsertBase(const Entry& e) {
  if((double)(count_ + 1) / size() > MAX_LOAD) {
    // Need to resize.  New table of double the size, add old elements to it.
    scoped_ptr<TableBase> new_table(NewOfSameType(count_ * 2));
    for (unsigned int i = 0; i < size(); i++) {
      Entry *e = GetEntryForBucket(i);
      if (!e->is_empty) new_table->InsertBase(*e);
    }
    Swap(new_table.get());
  }
  DoInsert(e);
}

template<class C>
void SwapValues(C a, C b) {
  C tmp = a;
  a = b;
  b = tmp;
}

void TableBase::Swap(TableBase* other) {
  SwapValues(count_, other->count_);
  SwapValues(size_lg2_, other->size_lg2_);
  SwapValues(mask_, other->mask_);
  SwapDerived(other);
}

#ifdef UPB_UNALIGNED_READS_OK
//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby (released as public domain).
// Reformatted and C99-ified by Joshua Haberman.
// Note - This code makes a few assumptions about how your machine behaves -
//   1. We can read a 4-byte value from any address without crashing
//   2. sizeof(int) == 4 (in upb this limitation is removed by using uint32_t)
// And it has a few limitations -
//   1. It will not work incrementally.
//   2. It will not produce the same results on little-endian and big-endian
//      machines.
uint32_t MurmurHash2(const void *key, size_t len, uint32_t seed) {
  // 'm' and 'r' are mixing constants generated offline.
  // They're not really 'magic', they just happen to work well.
  const uint32_t m = 0x5bd1e995;
  const int32_t r = 24;

  // Initialize the hash to a 'random' value
  uint32_t h = seed ^ len;

  // Mix 4 bytes at a time into the hash
  const uint8_t * data = (const uint8_t *)key;
  while(len >= 4) {
    uint32_t k = *(uint32_t *)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  // Handle the last few bytes of the input array
  switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
  };

  // Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.
  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
}

#else // !UPB_UNALIGNED_READS_OK

//-----------------------------------------------------------------------------
// MurmurHashAligned2, by Austin Appleby
// Same algorithm as MurmurHash2, but only does aligned reads - should be safer
// on certain platforms.
// Performance will be lower than MurmurHash2

#define MIX(h,k,m) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }

uint32_t MurmurHash2(const void * key, size_t len, uint32_t seed) {
  const uint32_t m = 0x5bd1e995;
  const int32_t r = 24;
  const uint8_t * data = (const uint8_t *)key;
  uint32_t h = seed ^ len;
  uint8_t align = (uintptr_t)data & 3;

  if(align && (len >= 4)) {
    // Pre-load the temp registers
    uint32_t t = 0, d = 0;

    switch(align) {
      case 1: t |= data[2] << 16;
      case 2: t |= data[1] << 8;
      case 3: t |= data[0];
    }

    t <<= (8 * align);

    data += 4-align;
    len -= 4-align;

    int32_t sl = 8 * (4-align);
    int32_t sr = 8 * align;

    // Mix
    while(len >= 4) {
      d = *(uint32_t *)data;
      t = (t >> sr) | (d << sl);

      uint32_t k = t;
      MIX(h,k,m);
      t = d;
      data += 4;
      len -= 4;
    }

    // Handle leftover data in temp registers
    d = 0;
    if(len >= align) {
      switch(align) {
        case 3: d |= data[2] << 16;
        case 2: d |= data[1] << 8;
        case 1: d |= data[0];
      }

      uint32_t k = (t >> sr) | (d << sl);
      MIX(h,k,m);
      data += align;
      len -= align;

      //----------
      // Handle tail bytes
      switch(len) {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0]; h *= m;
      };
    } else {
      switch(len) {
        case 3: d |= data[2] << 16;
        case 2: d |= data[1] << 8;
        case 1: d |= data[0];
        case 0: h ^= (t >> sr) | (d << sl); h *= m;
      }
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
  } else {
    while(len >= 4) {
      uint32_t k = *(uint32_t *)data;
      MIX(h,k,m);
      data += 4;
      len -= 4;
    }

    //----------
    // Handle tail bytes
    switch(len) {
      case 3: h ^= data[2] << 16;
      case 2: h ^= data[1] << 8;
      case 1: h ^= data[0]; h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
  }
}
#undef MIX

#endif // UPB_UNALIGNED_READS_OK

}  // namespace upb
