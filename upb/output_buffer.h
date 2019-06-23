
#ifndef UPB_OUTPUT_BUFFER_H_
#define UPB_OUTPUT_BUFFER_H_

#include "upb/port_def.inc"

#define CHK(x)              \
  if (UPB_UNLIKELY(!(x))) { \
    return 0;               \
  }

static size_t encode_varint(uint64_t val, char *buf) {
  size_t i;
  if (val < 128) { buf[0] = val; return 1; }
  i = 0;
  while (val) {
    uint8_t byte = val & 0x7fU;
    val >>= 7;
    if (val) byte |= 0x80U;
    buf[i++] = byte;
  }
  return i;
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

static char* reserve_bytes(size_t bytes, outbuf* out) {
  size_t have = out->end - out->ptr;
  return (have >= bytes) ? out->ptr : realloc_buf(bytes, out);
}

static bool write_str(const void* str, size_t n, outbuf* out) {
  CHK(reserve_bytes(n, out));
  memcpy(out->ptr, str, n);
  out->ptr += n;
  return true;
}

static bool write_char(char ch, outbuf* out) {
  CHK(reserve_bytes(1, out));
  *out->ptr = ch;
  out->ptr++;
  return true;
}

static bool write_varint(uint64_t val, outbuf* out) {
  CHK(reserve_bytes(10, out));
  out->ptr += encode_varint(val, out->ptr);
  return true;
}

static bool write_known_tag(uint8_t wire_type, uint32_t fieldnum, outbuf* out) {
  UPB_ASSERT(wire_type <= 5 && wire_type >= 0);
  return write_varint(wire_type | (fieldnum << 3), out);
}

static bool write_string_field(uint32_t fieldnum, const char* buf,
                               unsigned size, outbuf* out) {
  CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, fieldnum, out));
  CHK(write_varint(size, out));
  return write_str(buf, size, out);
}

static size_t buf_ofs(outbuf* out) { return out->ptr - out->buf; }

static bool insert_fixed_len(size_t ofs, outbuf* out) {
  size_t len = buf_ofs(out) - ofs;
  int intlen = len;
  char* ptr = out->buf + ofs;

  CHK(len <= INT_MAX);
  CHK(reserve_bytes(4, out));
  ptr = out->buf + ofs;
  memmove(ptr + 4, ptr, len);
  memcpy(ptr, &intlen, 4);
  out->ptr += 4;
  return true;
}

static bool insert_varint_len(size_t ofs, outbuf* out) {
  size_t len = buf_ofs(out) - ofs;
  char varint[10];
  size_t varint_len = encode_varint(len, varint);
  char* ptr = out->buf + ofs;

  CHK(len <= INT_MAX);
  CHK(reserve_bytes(varint_len, out));
  ptr = out->buf + ofs;
  memmove(ptr + varint_len, ptr, len);
  memcpy(ptr, varint, varint_len);
  out->ptr += varint_len;
  return true;
}

#undef CHK

#include "upb/port_undef.inc"

#endif  /* UPB_OUTPUT_BUFFER_H_ */
