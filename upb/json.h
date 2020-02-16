/*
** string <-> string transcoding of JSON <-> protobuf binary.
**
*/

#ifndef UPB_JSON_H_
#define UPB_JSON_H_

#include "upb/def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UPB_JSON_IGNORE_UNKNOWN 1

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       const upb_symtab* any_msgs, int options, int max_depth,
                       upb_alloc* alloc, size_t* outlen, upb_status* s);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* UPB_JSON_H */
