
#undef NDEBUG  /* ensure tests always assert. */
#include "upb_table.h"
#include "test_util.h"
#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <ext/hash_map>
#include <sys/resource.h>
#include <iostream>

using std::string;
using std::vector;
using upb::StrTable;
using upb::IntTable;

double get_usertime()
{
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_utime.tv_sec + (usage.ru_utime.tv_usec/1000000.0);
}

struct upb_string *get_upbstring(const string& key) {
  struct upb_string *str = upb_string_new();
  upb_string_resize(str, key.size());
  memcpy(str->ptr, key.c_str(), key.size());
  return str;
}

/* num_entries must be a power of 2. */
void test_strtable(const vector<string>& keys, uint32_t num_to_insert)
{
  /* Initialize structures. */
  std::map<string, int32_t> m;
  std::set<string> all;
  StrTable<int32_t>::Table table(num_to_insert);
  for(size_t i = 0; i < num_to_insert; i++) {
    const string& key = keys[i];
    all.insert(key);
    table.Insert(get_upbstring(key), key[0]);
    m[key] = key[0];
  }

  /* Test correctness. */
  for(uint32_t i = 0; i < keys.size(); i++) {
    const string& key = keys[i];
    struct upb_string *str = get_upbstring(key);
    StrTable<int32_t>::Entry* e = table.Lookup(str);
    if(m.find(key) != m.end()) { /* Assume map implementation is correct. */
      assert(e);
      assert(upb_streql(e->key(), get_upbstring(key)));
      assert(e->value() == key[0]);
      assert(m[key] == key[0]);
    } else {
      assert(e == NULL);
    }
  }

  for(StrTable<int32_t>::Entry* e = table.Begin(); e; e = table.Next(e)) {
    string tmp(e->key()->ptr, e->key()->byte_len);
    std::set<string>::iterator i = all.find(tmp);
    assert(i != all.end());
    all.erase(i);
  }
  assert(all.empty());
}

/* num_entries must be a power of 2. */
void test_inttable(int32_t *keys, size_t num_entries)
{
  /* Initialize structures. */
  uint32_t largest_key = 0;
  std::map<uint32_t, uint32_t> m;
  __gnu_cxx::hash_map<uint32_t, uint32_t> hm;
  IntTable<uint32_t>::Table table(num_entries);
  for(size_t i = 0; i < num_entries; i++) {
    int32_t key = keys[i];
    largest_key = UPB_MAX((int32_t)largest_key, key);
    table.Insert(key, key * 2);
    m[key] = key*2;
    hm[key] = key*2;
  }

  /* Test correctness. */
  for(uint32_t i = 1; i <= largest_key; i++) {
    IntTable<uint32_t>::Entry* e = table.Lookup(i);
    if(m.find(i) != m.end()) { /* Assume map implementation is correct. */
      assert(e);
      assert(e->key() == i);
      assert(e->value() == i*2);
      assert(m[i] == i*2);
      assert(hm[i] == i*2);
    } else {
      assert(e == NULL);
    }
  }

  /* Test performance. We only test lookups for keys that are known to exist. */
  uintptr_t x = 0;
  const unsigned int iterations = 0xFFFFFF;
  const int32_t mask = num_entries - 1;
  printf("Measuring sequential loop overhead...");
  fflush(stdout);
  double before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[i & mask];
    x += key;
  }
  double seq_overhead = get_usertime() - before;
  printf("%0.3f seconds for %d iterations\n", seq_overhead, iterations);

  printf("Measuring random loop overhead...");
  rand();
  fflush(stdout);
  before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[rand() & mask];
    x += key;
  }
  double rand_overhead = get_usertime() - before;
  printf("%0.3f seconds for %d iterations\n", rand_overhead, iterations);

  printf("upb_table(seq): ");
  fflush(stdout);
  before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[i & mask];
    IntTable<uint32_t>::Entry* e = table.Lookup(key);
    x += (uintptr_t)e;
  }
  double total = get_usertime() - before;
  double without_overhead = total - seq_overhead;
  printf("%0.3f seconds (%0.3f - %0.3f overhead) for %d iterations.  %s/s\n", without_overhead, total, seq_overhead, iterations, eng(iterations/without_overhead, 3, false));

  printf("upb_table(rand): ");
  fflush(stdout);
  before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[rand() & mask];
    IntTable<uint32_t>::Entry* e = table.Lookup(key);
    x += (uintptr_t)e;
  }
  total = get_usertime() - before;
  without_overhead = total - rand_overhead;
  printf("%0.3f seconds (%0.3f - %0.3f overhead) for %d iterations.  %s/s\n", without_overhead, total, rand_overhead, iterations, eng(iterations/without_overhead, 3, false));

  printf("map(seq): ");
  fflush(stdout);
  before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[i & mask];
    x += m[key];
  }
  total = get_usertime() - before;
  without_overhead = total - seq_overhead;
  printf("%0.3f seconds (%0.3f - %0.3f overhead) for %d iterations.  %s/s\n", without_overhead, total, seq_overhead, iterations, eng(iterations/without_overhead, 3, false));

  printf("map(rand): ");
  fflush(stdout);
  before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[rand() & mask];
    x += m[key];
  }
  total = get_usertime() - before;
  without_overhead = total - rand_overhead;
  printf("%0.3f seconds (%0.3f - %0.3f overhead) for %d iterations.  %s/s\n", without_overhead, total, rand_overhead, iterations, eng(iterations/without_overhead, 3, false));

  printf("hash_map(seq): ");
  fflush(stdout);
  before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[i & mask];
    x += hm[key];
  }
  total = get_usertime() - before;
  without_overhead = total - seq_overhead;
  printf("%0.3f seconds (%0.3f - %0.3f overhead) for %d iterations.  %s/s\n", without_overhead, total, seq_overhead, iterations, eng(iterations/without_overhead, 3, false));

  printf("hash_map(rand): ");
  fflush(stdout);
  before = get_usertime();
  for(unsigned int i = 0; i < iterations; i++) {
    int32_t key = keys[rand() & mask];
    x += hm[key];
  }
  total = get_usertime() - before;
  without_overhead = total - rand_overhead;
  printf("%0.3f seconds (%0.3f - %0.3f overhead) for %d iterations.  %s/s\n\n", without_overhead, total, rand_overhead, iterations, eng(iterations/without_overhead, 3, false));
}

int32_t *get_contiguous_keys(int32_t num)
{
  int32_t *buf = new int32_t[num];
  for(int32_t i = 0; i < num; i++)
    buf[i] = i+1;
  return buf;
}

int main()
{
  vector<string> keys;
  keys.push_back("google.protobuf.FileDescriptorSet");
  keys.push_back("google.protobuf.FileDescriptorProto");
  keys.push_back("google.protobuf.DescriptorProto");
  keys.push_back("google.protobuf.DescriptorProto.ExtensionRange");
  keys.push_back("google.protobuf.FieldDescriptorProto");
  keys.push_back("google.protobuf.EnumDescriptorProto");
  keys.push_back("google.protobuf.EnumValueDescriptorProto");
  keys.push_back("google.protobuf.ServiceDescriptorProto");
  keys.push_back("google.protobuf.MethodDescriptorProto");
  keys.push_back("google.protobuf.FileOptions");
  keys.push_back("google.protobuf.MessageOptions");
  keys.push_back("google.protobuf.FieldOptions");
  keys.push_back("google.protobuf.EnumOptions");
  keys.push_back("google.protobuf.EnumValueOptions");
  keys.push_back("google.protobuf.ServiceOptions");
  keys.push_back("google.protobuf.MethodOptions");
  keys.push_back("google.protobuf.UninterpretedOption");
  keys.push_back("google.protobuf.UninterpretedOption.NamePart");

  test_strtable(keys, 18);

  int32_t *keys1 = get_contiguous_keys(8);
  printf("Contiguous 1-8 ====\n");
  test_inttable(keys1, 8);
  delete[] keys1;

  int32_t *keys2 = get_contiguous_keys(64);
  printf("Contiguous 1-64 ====\n");
  test_inttable(keys2, 64);
  delete[] keys2;

  int32_t *keys3 = get_contiguous_keys(512);
  printf("Contiguous 1-512 ====\n");
  test_inttable(keys3, 512);
  delete[] keys3;

  int32_t *keys4 = new int32_t[64];
  for(int32_t i = 0; i < 64; i++) {
    if(i < 32)
      keys4[i] = i+1;
    else
      keys4[i] = 10101+i;
  }
  printf("1-32 and 10133-10164 ====\n");
  test_inttable(keys4, 64);
  delete[] keys4;
}
