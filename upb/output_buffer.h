
#ifndef UPB_OUTPUT_BUFFER_H_
#define UPB_OUTPUT_BUFFER_H_

#include "upb/port_def.inc"

#define CHK(x)              \
  if (UPB_UNLIKELY(!(x))) { \
    return 0;               \
  }

typedef struct {
  upb_alloc* alloc;
  char* ptr;
  char* buf;
  char* end;
} outbuf;

UPB_NOINLINE static char* realloc_buf(size_t bytes, outbuf* out) {
  size_t old = out->end - out->buf;
  size_t ptr = out->ptr - out->buf;
  size_t need = ptr + bytes;
  size_t n = UPB_MAX(old, 128);
  static const size_t max = LONG_MIN;

  CHK(need > ptr);  /* ptr + bytes didn't overflow. */
  CHK(need < max);  /* we can exceed by doubling n. */

  while (n < need) {
    n *= 2;
  }

  out->buf = upb_realloc(out->alloc, out->buf, old, n);
  CHK(out->buf);

  out->ptr = out->buf + ptr;
  out->end = out->buf + n;
  return out->ptr;
}

UPB_INLINE char* reserve_bytes(size_t bytes, outbuf* out) {
  size_t have = out->end - out->ptr;
  return (have >= bytes) ? out->ptr : realloc_buf(bytes, out);
}

UPB_INLINE bool write_str(const void* str, size_t n, outbuf* out) {
  CHK(reserve_bytes(n, out));
  memcpy(out->ptr, str, n);
  out->ptr += n;
  return true;
}

UPB_INLINE bool write_strz(const void* str, outbuf* out) {
  return write_str(str, strlen(str), out);
}

UPB_INLINE bool write_char(char ch, outbuf* out) {
  CHK(reserve_bytes(1, out));
  *out->ptr = ch;
  out->ptr++;
  return true;
}

static size_t buf_ofs(outbuf* out) { return out->ptr - out->buf; }

#undef CHK

#include "upb/port_undef.inc"

#endif  /* UPB_OUTPUT_BUFFER_H_ */
