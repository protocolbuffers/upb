/*
** string <-> string transcoding of JSON <-> protobuf binary.
**
*/

#ifndef UPB_JSON_H_
#define UPB_JSON_H_

#include "upb/def.h"

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen);

char* upb_binarytojson(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen);

#endif  /* UPB_JSON_H */
