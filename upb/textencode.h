
#ifndef UPB_TEXTENCODE_H_
#define UPB_TEXTENCODE_H_

#include "upb/def.h"

enum {
  /* When set, prints everything on a single line. */
  UPB_TXTENC_SINGLELINE = 1,

  /* When set, unknown fields are not printed. */
  UPB_TXTENC_SKIPUNKNOWN = 2,
};

char *upb_textencode(const upb_msg *msg, const upb_msgdef *m,
                     const upb_symtab *symtab, upb_arena *arena, int options,
                     size_t *size);

#endif  /* UPB_TEXTENCODE_H_ */
