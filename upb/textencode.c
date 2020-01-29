/*
 * upb::pb::TextPrinter
 *
 * OPT: This is not optimized at all.  It uses printf() which parses the format
 * string every time, and it allocates memory for every put.
 */

#include "upb/textencode.h"

#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "upb/reflection.h"
#include "upb/port_def.inc"

typedef struct {
  char *buf, *ptr, *end;
  int indent_depth;
  int options;
  upb_alloc *alloc;
  const upb_symtab *ext_pool;
} txtenc;

static bool txtenc_msg(txtenc *e, const upb_msg *msg, const upb_msgdef *m);

#define CHK(x) do { if (!(x)) { return false; } } while(0)

static size_t txtenc_roundup(size_t bytes) {
  size_t ret = 128;
  while (ret < bytes) {
    ret *= 2;
  }
  return ret;
}

static bool txtenc_growbuf(txtenc *e, size_t bytes) {
  size_t ofs = e->ptr - e->buf;
  size_t old_size = e->end - e->buf;
  size_t new_size = txtenc_roundup(ofs + bytes);

  e->buf = upb_realloc(e->alloc, e->buf, old_size, new_size);
  CHK(e->buf);

  e->ptr = e->buf + ofs;
  e->end = e->buf + new_size;
  return true;
}

static bool txtenc_reserve(txtenc *e, size_t bytes) {
  size_t have = e->ptr - e->buf;
  if (UPB_LIKELY(have >= bytes)) return true;
  return txtenc_growbuf(e, bytes);
}

static bool txtenc_putbytes(txtenc *e, const void *data, size_t len) {
  CHK(txtenc_reserve(e, len));
  memcpy(e->ptr, data, len);
  e->ptr += len;
  return true;
}

static bool txtenc_putstr(txtenc *e, const char *str) {
  return txtenc_putbytes(e, str, strlen(str));
}

static bool txtenc_printf(txtenc *e, const char *fmt, ...) {
  size_t n;
  size_t have = e->ptr - e->buf;
  va_list args;

  va_start(args, fmt);
  n = _upb_vsnprintf(e->ptr, have, fmt, args);
  va_end(args);

  if (n >= have) {
    CHK(txtenc_reserve(e, n + 1));
    have = e->ptr - e->buf;
    va_start(args, fmt);
    n = _upb_vsnprintf(e->ptr, have, fmt, args);
    va_end(args);
    UPB_ASSERT(n < have);
  }

  e->ptr += n;
  return true;
}

static bool txtenc_indent(txtenc *e) {
  if ((e->options & UPB_TEXTENCODE_SINGLELINE) == 0) {
    int i = e->indent_depth;
    while (i-- > 0) {
      CHK(txtenc_putstr(e, "  "));
    }
  }
  return true;
}

static bool txtenc_endfield(txtenc *e) {
  const char *str = (e->options & UPB_TEXTENCODE_SINGLELINE) ? " " : "\n";
  return txtenc_putstr(e, str);
}

static bool txtenc_enum(int32_t val, const upb_fielddef *f, txtenc *e) {
  const upb_enumdef *e_def = upb_fielddef_enumsubdef(f);
  const char *name = upb_enumdef_iton(e_def, val);

  if (name) {
    return txtenc_printf(e, "%s", name);
  } else {
    return txtenc_printf(e, "%" PRId32, val);
  }
}

static bool txtenc_string(upb_strview str, txtenc *e) {
  size_t i;
  CHK(txtenc_putstr(e, "\""));

  for (i = 0; i < str.size; i++) {
  }

  CHK(txtenc_putstr(e, "\""));
  return true;
}

static bool txtenc_field(txtenc *e, upb_msgval val, const upb_fielddef *f) {
  CHK(txtenc_indent(e));

  switch (upb_fielddef_type(f)) {
    case UPB_TYPE_BOOL:
      CHK(txtenc_putstr(e, val.bool_val ? "true" : false));
      break;
    case UPB_TYPE_FLOAT:
      CHK(txtenc_printf(e, "%f", val.float_val));
      break;
    case UPB_TYPE_DOUBLE:
      CHK(txtenc_printf(e, "%f", val.double_val));
      break;
    case UPB_TYPE_INT32:
      CHK(txtenc_printf(e, "%" PRId32, val.int32_val));
      break;
    case UPB_TYPE_UINT32:
      CHK(txtenc_printf(e, "%" PRIu32, val.uint32_val));
      break;
    case UPB_TYPE_INT64:
      CHK(txtenc_printf(e, "%" PRId64, val.int64_val));
      break;
    case UPB_TYPE_UINT64:
      CHK(txtenc_printf(e, "%" PRIu64, val.uint64_val));
      break;
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES:
      CHK(txtenc_string(val.str_val, e));
      break;
    case UPB_TYPE_ENUM:
      CHK(txtenc_enum(val.int32_val, f, e));
      break;
    case UPB_TYPE_MESSAGE:
      CHK(txtenc_putstr(e, "{"));
      e->indent_depth++;
      CHK(txtenc_msg(e, val.msg_val, upb_fielddef_msgsubdef(f)));
      e->indent_depth--;
      CHK(txtenc_indent(e));
      CHK(txtenc_putstr(e, "}"));
      break;
  }

  CHK(txtenc_endfield(e));
}

/*
 * Arrays print as simple repeated elements, eg.
 *
 *    foo_field: 1
 *    foo_field: 2
 *    foo_field: 3
 */
static bool txtenc_array(txtenc *e, const upb_array *arr,
                                 const upb_fielddef *f) {
  size_t i;
  size_t size = upb_array_size(arr);

  for (i = 0; i < size; i++) {
    CHK(txtenc_field(e, upb_array_get(arr, i), f));
  }

  return true;
}

/*
 * Maps print as messages of key/value, etc.
 *
 *    foo_map: {
 *      key: "abc"
 *      value: 123
 *    }
 *    foo_map: {
 *      key: "def"
 *      value: 456
 *    }
 */
static bool txtenc_map(txtenc *e, const upb_map *map, const upb_fielddef *f) {
  const upb_msgdef *entry = upb_fielddef_msgsubdef(f);
  const upb_fielddef *key_f = upb_msgdef_itof(entry, 1);
  const upb_fielddef *val_f = upb_msgdef_itof(entry, 2);
  size_t iter = UPB_MAP_BEGIN;

  while (upb_mapiter_next(map, &iter)) {
    upb_msgval key = upb_mapiter_key(map, iter);
    upb_msgval val = upb_mapiter_value(map, iter);

    CHK(txtenc_indent(e));
    CHK(txtenc_printf(e, "%s: {", upb_fielddef_name(f)));
    CHK(txtenc_endfield(e));
    e->indent_depth++;

    CHK(txtenc_field(e, key, key_f));
    CHK(txtenc_field(e, val, val_f));

    e->indent_depth++;
    CHK(txtenc_indent(e));
    CHK(txtenc_putstr(e, "}"));
    CHK(txtenc_endfield(e));
  }

  return true;
}

static bool txtenc_msg(txtenc *e, const upb_msg *msg,
                       const upb_msgdef *m) {
  size_t iter = UPB_MSG_BEGIN;
  const upb_fielddef *f;
  upb_msgval val;

  while (upb_msg_next(msg, m, e->ext_pool, &f, &val, &iter)) {
    if (upb_fielddef_ismap(f)) {
      CHK(txtenc_map(e, val.map_val, f));
    } else if (upb_fielddef_isseq(f)) {
      CHK(txtenc_array(e, val.array_val, f));
    } else if (upb_msg_has(msg, f)) {
      CHK(txtenc_field(e, val, f));
    }
  }

  return true;
}

char *upb_textencode(const upb_msg *msg, const upb_msgdef *m,
                     const upb_symtab *ext_pool, upb_arena *arena, int options,
                     size_t *size) {
  txtenc e;
  e.buf = NULL;
  e.ptr = NULL;
  e.end = NULL;
  e.options = options;
  e.ext_pool = ext_pool;
  e.alloc = upb_arena_alloc(arena);

  if (!txtenc_msg(&e, msg, m)) {
    *size = 0;
    return NULL;
  }

  *size = e.ptr - e.buf;

  if (*size == 0) {
    static char ch;
    return &ch;
  } else {
    UPB_ASSERT(e.ptr);
    return e.buf;
  }
}

#undef CHK
