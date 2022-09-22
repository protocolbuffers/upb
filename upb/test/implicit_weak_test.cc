
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "upb/base/descriptor_constants.h"
#include "upb/test/implicit_weak_test.upb.h"
#include "upb/upb.hpp"

// Implicit weak only works on Android/iOS, and only in opt mode.
#if defined(COMPILATION_MODE_OPT) && defined(__ANDROID__)

namespace {

const upb_MiniTable* GetSub(const upb_MiniTable* table, uint32_t field_number) {
  const upb_MiniTableField* field =
      upb_MiniTable_FindFieldByNumber(table, field_number);
  ABSL_CHECK(field);
  ABSL_CHECK_EQ(upb_MiniTableField_CType(field), kUpb_CType_Message);
  upb_MiniTableSub sub = table->subs[field->submsg_index];
  ABSL_CHECK(sub.submsg);
  return *sub.submsg;
}

const upb_MiniTable* GetSubForMap(const upb_MiniTable* table,
                                  uint32_t field_number) {
  const upb_MiniTable* entry = GetSub(table, field_number);
  ABSL_CHECK(entry);
  return GetSub(entry, 2);
}

#ifdef UPB_REFERENCE_FILE

// Referencing the file will pull in all messages from the file, even if
// nothing references them through fields.

TEST(ReferencedThroughFileTest, RegularField) {
  const upb_MiniTableFile* volatile file_ref =
      &third_party_upb_upb_test_implicit_weak_test_proto_upb_file_layout;
  (void)file_ref;
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 1));
}

TEST(ReferencedThroughFileTest, RepeatedNotReferenced) {
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 15));
}

TEST(ReferencedThroughFileTest, MapNotReferenced) {
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 16));
}

#else  // UPB_REFERENCE_FILE

TEST(NotReferencedTest, RegularField) {
  EXPECT_EQ(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 1));
}

TEST(NotReferencedTest, RepeatedNotReferenced) {
  EXPECT_EQ(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 15));
}

TEST(NotReferencedTest, MapNotReferenced) {
  EXPECT_EQ(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 16));
}

#endif  // UPB_REFERENCE_FILE

TEST(ImplicitWeakTest, GetNonRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_get_non_repeated(msg);
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 2));
}

TEST(ImplicitWeakTest, SetNonRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_SetNonRepeated* sub = upb_test_SetNonRepeated_new(arena.ptr());
  upb_test_TopLevelMessage_set_set_non_repeated(msg, sub);
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 3));
}

TEST(ImplicitWeakTest, HasNonRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_has_has_non_repeated(msg);
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 4));
}

TEST(ImplicitWeakTest, ClearNonRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_clear_clear_non_repeated(msg);
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 5));
}

TEST(ImplicitWeakTest, GetRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  size_t size;
  upb_test_TopLevelMessage_get_repeated(msg, &size);
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 6));
}

TEST(ImplicitWeakTest, AddRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_add_add_repeated(msg, arena.ptr());
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 7));
}

TEST(ImplicitWeakTest, MutableRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  size_t size;
  upb_test_TopLevelMessage_mutable_mutable_repeated(msg, &size);
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 8));
}

TEST(ImplicitWeakTest, ResizeRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_resize_resize_repeated(msg, 0, arena.ptr());
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 9));
}

TEST(ImplicitWeakTest, ClearRepeated) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_clear_clear_repeated(msg);
  EXPECT_NE(nullptr, GetSub(&upb_test_TopLevelMessage_msg_init, 10));
}

TEST(ImplicitWeakTest, GetMap) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_GetMap* val;
  upb_test_TopLevelMessage_get_map_get(msg, 0, &val);
  EXPECT_NE(nullptr, GetSubForMap(&upb_test_TopLevelMessage_msg_init, 11));
}

TEST(ImplicitWeakTest, MapSize) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_size_map_size(msg);
  EXPECT_NE(nullptr, GetSubForMap(&upb_test_TopLevelMessage_msg_init, 12));
}

TEST(ImplicitWeakTest, ClearMap) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  upb_test_TopLevelMessage_clear_clear_map(msg);
  EXPECT_NE(nullptr, GetSubForMap(&upb_test_TopLevelMessage_msg_init, 13));
}

TEST(ImplicitWeakTest, NextMap) {
  upb::Arena arena;
  upb_test_TopLevelMessage* msg = upb_test_TopLevelMessage_new(arena.ptr());
  size_t iter = 0;
  upb_test_TopLevelMessage_next_map_next(msg, &iter);
  EXPECT_NE(nullptr, GetSubForMap(&upb_test_TopLevelMessage_msg_init, 14));
}

}  // namespace

#endif  // COMPILATION_MODE_OPT && __ANDROID__
