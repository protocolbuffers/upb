
#include "upb/json.h"

#include "upb/upb.h"

static bool is_proto3(const upb_msgdef* m) {
  return upb_filedef_syntax(upb_msgdef_file(m)) == UPB_SYNTAX_PROTO3;
}

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen) {
  char* ret;

  if (!is_proto3(m)) return NULL;

  ret = upb_malloc(alloc, 1);
  ret[0] = 0;
  *outlen = 1;

  return ret;
}

char* upb_binarytojson(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen) {
  char* ret;

  if (!is_proto3(m)) return NULL;

  ret = upb_malloc(alloc, 1);
  ret[0] = 0;
  *outlen = 1;

  return ret;
}
