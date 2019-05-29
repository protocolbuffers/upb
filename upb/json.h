/*
** string <-> string transcoding of JSON <-> protobuf binary.
**
*/

#ifndef UPB_JSON_H_
#define UPB_JSON_H_

#include "upb/def.h"

struct upb_jsonparser;
typedef struct upb_jsonparser upb_jsonparser;

#define UPB_JSON_IGNORE_UNKNOWN 1

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       const upb_symtab* any_msgs, int options, int max_depth,
                       upb_alloc* alloc, size_t* outlen, upb_status* s);

char* upb_binarytojson(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen);

/* For internal use / testing only. *******************************************/

enum {
  /* 0 is reserved for errors. */
  kEnd = 1,
  kObject,
  kArray,
  kNumber,  /* kNumber <8 BYTE DOUBLE> */
  kString,  /* kString <4 BYTE LENGTH> <STRING DATA> */
  kTrue,
  kFalse,
  kNull
};

char* _parse_json_stage1(const char* buf, size_t len, int max_depth,
                         upb_alloc* alloc, size_t* outlen, upb_status* s);

#endif  /* UPB_JSON_H */
