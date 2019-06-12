#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "third_party/upb/google/protobuf/descriptor.upb.h"
#include "third_party/upb/upb/def.h"
#include "third_party/upb/upb/msg.h"
#include "third_party/upb/upb/upb.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  upb_strview strview =
      upb_strview_make(reinterpret_cast<const char*>(data), size);
  upb::Arena arena;
  google_protobuf_FileDescriptorProto_parsenew(strview, arena.ptr());
  return 0;
}
