
#include "upb/json.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "upb/upb.h"

#include "upb/port_def.inc"

#define CHK(x) if (UPB_UNLIKELY(!(x))) return 0

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

static bool decode_varint(const char** ptr, const char* limit, uint64_t* val) {
  uint8_t byte;
  int bitpos = 0;
  const char* p = *ptr;
  *val = 0;

  do {
    CHK(bitpos < 70 && p < limit);
    byte = *p;
    *val |= (uint64_t)(byte & 0x7F) << bitpos;
    p++;
    bitpos += 7;
  } while (byte & 0x80);

  *ptr = p;
  return true;
}

/* Output Buffer **************************************************************/

typedef struct {
  char* ptr;
  char* buf;
  char* end;
  upb_alloc* alloc;
} outbuf;

UPB_NOINLINE static bool realloc_buf(size_t bytes, outbuf* out) {
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
  return true;
}

static bool reserve_bytes(size_t bytes, outbuf* out) {
  size_t have = out->end - out->ptr;
  return (have >= bytes) ? true : realloc_buf(bytes, out);
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

static bool put_varint(uint64_t val, outbuf* out) {
  CHK(reserve_bytes(10, out));
  out->ptr += encode_varint(val, out->ptr);
  return true;
}

static size_t buf_ofs(outbuf* out) { return out->ptr - out->buf; }

static bool insert_length(size_t ofs, outbuf* out) {
  size_t len = buf_ofs(out) - ofs;
  char varint[10];
  size_t varint_len = encode_varint(len, varint);

  /* Shift data right to make space for the varint. */
  CHK(reserve_bytes(varint_len, out));
  memmove(out->ptr + varint_len, out->ptr, len);
  memcpy(out->ptr, varint, varint_len);
  return true;
}

/* Generic JSON parser ********************************************************/

typedef struct {
  const char* ptr;
  const char* end;
  outbuf out;
  int depth;
  upb_status* status;
} jsonparser;

enum {
  kEnd = 0,
  kObject,
  kArray,
  kNumber,
  kAliasedString,
  kString,
  kTrue,
  kFalse,
  kNull,
};

static bool parse_json_object(const char* ptr, jsonparser* state);
static bool parse_json_array(const char* ptr, jsonparser* state);

// Input buffer.

static bool is_whitespace(char ch) {
  switch (ch) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
      return true;
    default:
      return false;
  }
}

static bool is_eof(const char* ptr, jsonparser* state) {
  return (ptr == state->end);
}

static const char* skip_whitespace(const char* ptr, jsonparser* state) {
  while (!is_eof(ptr, state) && is_whitespace(*ptr)) {
    ptr++;
  }
  return is_eof(ptr, state) ? NULL : ptr;
}

static bool has_n_bytes(const char* ptr, size_t n, jsonparser* state) {
  return state->end - ptr >= n;
}

static const char* parse_char(const char* ptr, char ch, jsonparser* state) {
  CHK(skip_whitespace(ptr, state));
  return *ptr == ch ? ptr + 1 : NULL;
}

static UPB_FORCEINLINE const char* parse_lit(const char* ptr, const char* lit,
                                             jsonparser* state) {
  size_t len = strlen(lit);
  CHK(has_n_bytes(ptr, len, state));
  CHK(memcmp(ptr, lit, len) == 0);
  return ptr + len;
}

static bool peek_char(const char* ptr, jsonparser* state, char* ch) {
  CHK(skip_whitespace(ptr, state));
  *ch = *ptr;
  return true;
}

static bool peek_char_nows(const char* ptr, jsonparser* state, char* ch) {
  CHK(!is_eof(ptr, state));
  *ch = *ptr;
  return true;
}

static const char* consume_char(const char* ptr, jsonparser* state, char* ch) {
  CHK(skip_whitespace(ptr, state));
  *ch = *ptr;
  return ptr + 1;
}

static const char* consume_char_nows(const char* ptr, jsonparser* state, char* ch) {
  CHK(!is_eof(ptr, state));
  *ch = *ptr;
  return ptr + 1;
}

static const char* skip_digits(const char* ptr, jsonparser* state) {
  const char* start = ptr;

  while (true) {
    char ch;
    CHK(peek_char_nows(ptr, state, &ch));
    if (ch < '0' || ch > '9') {
      break;
    }
    ptr++;
  }

  /* We must consume at least one digit. */
  return ptr == start ? NULL : ptr;
}

// Generic JSON parser.

static bool parse_hex_digit(char ch, uint32_t* digit) {
  *digit <<= 4;

  if (ch >= '0' && ch <= '9') {
    *digit |= (ch - '0');
  } else if (ch >= 'a' && ch <= 'f') {
    *digit |= ((ch - 'a') + 10);
  } else if (ch >= 'A' && ch <= 'F') {
    *digit |= ((ch - 'A') + 10);
  } else {
    return false;
  }

  return true;
}

static bool write_utf8_codepoint(uint32_t cp, jsonparser* state) {
  char utf8[3]; /* support \u0000 -- \uFFFF -- need only three bytes. */

  if (cp <= 0x7F) {
    return write_char(cp, &state->out);
  } else if (cp <= 0x07FF) {
    utf8[0] = ((cp >> 6) & 0x1F) | 0xC0;
    utf8[1] = ((cp >> 0) & 0x3F) | 0x80;
    return write_str(utf8, 2, &state->out);
  } else /* cp <= 0xFFFF */ {
    utf8[0] = ((cp >> 12) & 0x0F) | 0xE0;
    utf8[1] = ((cp >> 6) & 0x3F) | 0x80;
    utf8[2] = ((cp >> 0) & 0x3F) | 0x80;
    return write_str(utf8, 3, &state->out);
  }

  /* TODO(haberman): Handle high surrogates: if codepoint is a high surrogate
   * we have to wait for the next escape to get the full code point). */
}

static const char* parse_escape(const char* ptr, jsonparser* state) {
  char ch;

  CHK(ptr = consume_char_nows(ptr, state, &ch));

  switch (ch) {
    case '"':
      CHK(write_char('"', &state->out));
      break;
    case '\\':
      CHK(write_char('\\', &state->out));
      break;
    case '/':
      CHK(write_char('/', &state->out));
      break;
    case 'b':
      CHK(write_char('\b', &state->out));
      break;
    case 'n':
      CHK(write_char('\n', &state->out));
      break;
    case 'r':
      CHK(write_char('\r', &state->out));
      break;
    case 't':
      CHK(write_char('\t', &state->out));
      break;
    case 'u': {
      uint32_t codepoint = 0;
      CHK(has_n_bytes(ptr, 4, state));
      CHK(parse_hex_digit(ptr[0], &codepoint));
      CHK(parse_hex_digit(ptr[1], &codepoint));
      CHK(parse_hex_digit(ptr[2], &codepoint));
      CHK(parse_hex_digit(ptr[3], &codepoint));
      CHK(write_utf8_codepoint(codepoint, state));
      ptr += 4;
      break;
    }
    default:
      return NULL;
  }

  return ptr;
}

static const char* parse_json_string(const char* ptr, jsonparser* state) {
  const char* span_start = ptr;
  size_t ofs = buf_ofs(&state->out);

  while (true) {
    char ch;

    CHK(ptr = consume_char_nows(ptr, state, &ch));

    /* TODO: validate UTF-8. */

    switch (ch) {
      case '"':
        goto done;
      case '\\':
        CHK(write_str(span_start, ptr - span_start, &state->out));
        CHK(ptr = parse_escape(ptr, state));
        span_start = ptr;
        break;
      default:
        CHK((unsigned char)ch >= 0x20);
        break;
    }
  }

done:
  CHK(write_str(span_start, ptr - span_start, &state->out));
  CHK(insert_length(ofs, &state->out));

  return ptr;
}

static const char* parse_json_number(const char* ptr, jsonparser* state) {
  const char* start = ptr;
  char ch;
  char* end;
  double d;

  CHK(ptr = consume_char_nows(ptr, state, &ch));

  if (ch == '-') {
    CHK(ptr = consume_char_nows(ptr, state, &ch));
  }

  if (ch != '0') {
    CHK(ptr = skip_digits(ptr, state));
  }

  if (is_eof(ptr, state)) return ptr;

  if (*ptr == '.') {
    ptr++;
    CHK(ptr = skip_digits(ptr, state));
  }

  if (is_eof(ptr, state)) return ptr;

  if (*ptr == 'e' || *ptr == 'E') {
    ptr++;
    CHK(!is_eof(ptr, state));

    if (*ptr == '+' || *ptr == '-') {
      ptr++;
    }

    CHK(ptr = skip_digits(ptr, state));
  }

  errno = 0;
  d = strtod(start, &end);

  CHK(errno == 0 && end == ptr);
  CHK(write_char(kNumber, &state->out));
  CHK(write_str(&d, sizeof(d), &state->out));

  return ptr;
}

static const char* parse_json_value(const char* ptr, jsonparser* state) {
  char ch;

  CHK(--state->depth != 0);
  CHK(peek_char(ptr, state, &ch));

  switch (ch) {
    case '{':
      CHK(ptr = parse_json_object(ptr, state));
      break;
    case '[':
      CHK(ptr = parse_json_array(ptr, state));
      break;
    case '"':
      CHK(ptr = parse_json_string(ptr, state));
      break;
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      CHK(ptr = parse_json_number(ptr, state));
      break;
    case 't':
      CHK(ptr = parse_lit(ptr, "true", state));
      CHK(write_char(kTrue, &state->out));
    case 'f':
      CHK(ptr = parse_lit(ptr, "false", state));
      CHK(write_char(kFalse, &state->out));
      break;
    case 'n':
      CHK(ptr = parse_lit(ptr, "null", state));
      CHK(write_char(kNull, &state->out));
      break;
    default:
      return NULL;
  }

  state->depth++;
  return ptr;
}


static const char* parse_json_array(const char* ptr, jsonparser* state) {
  char ch;

  CHK(ptr = parse_char(ptr, '[', state));
  CHK(write_char(kArray, &state->out));
  CHK(peek_char(ptr, state, &ch));

  if (*ptr == ']') {
    CHK(write_char(kEnd, &state->out));
    return ptr + 1;
  }

  while (true) {
    char ch;

    CHK(ptr = parse_json_value(ptr, state));
    CHK(ptr = consume_char(ptr, state, &ch));

    switch (ch) {
      case ',':
        break;
      case ']':
        CHK(write_char(kEnd, &state->out));
        return ptr;
      default:
        return NULL;
    }
  }

  UPB_UNREACHABLE();
}

const char* parse_json_object(const char* ptr, jsonparser* state) {
  char ch;

  CHK(ptr = parse_char(ptr, '{', state));
  CHK(write_char(kObject, &state->out));
  CHK(peek_char(ptr, state, &ch));

  if (*ptr == '}') {
    CHK(write_char(kEnd, &state->out));
    return ptr + 1;
  }

  while (true) {
    char ch;

    CHK(ptr = parse_char(ptr, '"', state));
    CHK(ptr = parse_json_string(ptr, state));
    CHK(ptr = parse_char(ptr, ':', state));
    CHK(ptr = parse_json_value(ptr, state));
    CHK(ptr = consume_char(ptr, state, &ch));

    switch (ch) {
      case ',':
        break;
      case '}':
        CHK(write_char(kEnd, &state->out));
        return ptr;
      default:
        return NULL;
    }
  }

  UPB_UNREACHABLE();
}

/* Schema-aware JSON -> Protobuf translation **********************************/

/* This stage converts the generic JSON representation of stage 1 to serialized
 * protobuf binary format, according to a given schema.
 *
 * In this stage we have a single buffer we are both reading and writing from.
 * If our write head (outptr) runs into our read head (ptr), we have to resize.
 *
 * out.buf     out.ptr->    ptr->  out.end
 *    |           |          |        |
 *    V           V          V        V
 *    |-------------------------------|
 *
 * In this stage we don't need to bounds-check ptr when we are inside any kind
 * of nesting (object, array) because we know everything is balanced and
 * properly terminated.
 */

struct upb_jsonparser {
  outbuf out;
  const upb_msgdef *m;
  const upb_symtab* any_msgs;
  upb_status *status;
  int options;
};

static const char* convert_json_object(const char* ptr, const upb_msgdef* m,
                                       upb_jsonparser* parser);

static bool is_proto3(const upb_msgdef* m) {
  return upb_filedef_syntax(upb_msgdef_file(m)) == UPB_SYNTAX_PROTO3;
}

UPB_NOINLINE static const char* realloc_buf2(size_t bytes, const char* ptr,
                                             outbuf* out) {
  /* We need to preserve n remaining bytes of data from the existing buffer and
   * copy them to the end of the new buffer. */
  size_t n = out->end - ptr;
  size_t oldsize = out->end - out->buf;
  size_t newsize;
  char* newptr;

  CHK(realloc_buf(bytes, out));
  newsize = out->end - out->buf;
  newptr = out->buf + newsize - n;
  memmove(newptr, out->buf + oldsize - n, n);
  return newptr;
}

static bool is_eof2(const char* ptr, upb_jsonparser* parser) {
  return ptr == parser->out.end;
}

static const char* parse_char2(const char* ptr, char ch,
                               upb_jsonparser* parser) {
  CHK(!is_eof2(ptr, parser));
  return *ptr == ch ? ptr + 1 : NULL;
}

static const char* read_string_len(const char* ptr, size_t* len) {
  return NULL;
}

static const char* parse_str(const char* ptr, const char** str, size_t* len) {
  return NULL;
}

static const char* parse_number(const char* ptr, double* d) {
  memcpy(d, ptr, sizeof(*d));
  return ptr + sizeof(d);
}

static const char* reserve_bytes2(size_t bytes, const char* ptr,
                                  upb_jsonparser* parser) {
  size_t have = ptr - parser->out.ptr;
  if (have < bytes) {
    CHK(ptr = realloc_buf2(bytes, ptr, &parser->out));
  }
  return ptr;
}

static bool write_tag(const upb_fielddef *f, upb_jsonparser* parser) {
  return true;
}

// Base64 decoding.

static const signed char b64table[] = {
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      62/*+*/, -1,      -1,      -1,      63/*/ */,
  52/*0*/, 53/*1*/, 54/*2*/, 55/*3*/, 56/*4*/, 57/*5*/, 58/*6*/, 59/*7*/,
  60/*8*/, 61/*9*/, -1,      -1,      -1,      -1,      -1,      -1,
  -1,       0/*A*/,  1/*B*/,  2/*C*/,  3/*D*/,  4/*E*/,  5/*F*/,  6/*G*/,
  07/*H*/,  8/*I*/,  9/*J*/, 10/*K*/, 11/*L*/, 12/*M*/, 13/*N*/, 14/*O*/,
  15/*P*/, 16/*Q*/, 17/*R*/, 18/*S*/, 19/*T*/, 20/*U*/, 21/*V*/, 22/*W*/,
  23/*X*/, 24/*Y*/, 25/*Z*/, -1,      -1,      -1,      -1,      -1,
  -1,      26/*a*/, 27/*b*/, 28/*c*/, 29/*d*/, 30/*e*/, 31/*f*/, 32/*g*/,
  33/*h*/, 34/*i*/, 35/*j*/, 36/*k*/, 37/*l*/, 38/*m*/, 39/*n*/, 40/*o*/,
  41/*p*/, 42/*q*/, 43/*r*/, 44/*s*/, 45/*t*/, 46/*u*/, 47/*v*/, 48/*w*/,
  49/*x*/, 50/*y*/, 51/*z*/, -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1
};

/* Sign-extend to 32 bits to elide multiple error checks into one. */
static int32_t b64tab(unsigned char ch) { return b64table[ch]; }

static bool nonbase64(unsigned char ch) {
  return b64table[ch] == -1 && ch != '=';
}

static char* decode_padding(const char* in, char* out) {
  if (in[2] == '=') {
    /* "XX==" => 1 binary byte */
    uint32_t val;

    CHK(in[0] != '=' && in[1] != '=' && in[3] == '=');
    val = b64tab(in[0]) << 18 | b64tab(in[1]) << 12;
    UPB_ASSERT(!(val & 0x80000000));

    out[0] = val >> 16;
    return out + 1;
  } else {
    /* "XXX=" => 2 binary bytes */
    uint32_t val;

    CHK(in[0] != '=' && in[1] != '=' && in[2] != '=');
    val = b64tab(in[0]) << 18 | b64tab(in[1]) << 12 | b64tab(in[2]) << 6;

    out[0] = val >> 16;
    out[1] = (val >> 8) & 0xff;
    return out + 2;
  }
}

static bool base64_decode(const char* ptr, const upb_fielddef* f,
                          upb_jsonparser* state) {
  size_t len;
  size_t ofs = buf_ofs(&state->out);
  const char* in;
  const char *limit;
  char* out;

  CHK(ptr = read_string_len(ptr, &len));

  if ((len % 4) != 0) {
    upb_status_seterrf(state->status,
                       "Base64 input for bytes field not a multiple of 4: %s",
                       upb_fielddef_name(f));
  }

  /* This is a conservative estimate, assuming no padding. */
  CHK(ptr = reserve_bytes2(len / 4 * 3, ptr, state));

  in = ptr;
  limit = in + len;
  out = state->out.ptr;

  for (; in < limit; in += 4, out += 3) {
    uint32_t val;

    val = b64tab(in[0]) << 18 | b64tab(in[1]) << 12 | b64tab(in[2]) << 6 |
          b64tab(in[3]);

    /* Returns true if any of the characters returned -1. */
    if (UPB_UNLIKELY(val & 0x80000000)) {
      if (nonbase64(in[0]) || nonbase64(in[1]) || nonbase64(in[2]) ||
          nonbase64(in[3])) {
        upb_status_seterrf(state->status,
                           "Non-base64 characters in bytes field: %s",
                           upb_fielddef_name(f));
        return false;
      }

      if (in != limit - 4 || (out = decode_padding(in, out)) == NULL) {
        upb_status_seterrf(state->status,
                           "Incorrect base64 padding for field: %s (%.*s)",
                           upb_fielddef_name(f), 4, in);
        return false;
      }
    }

    out[0] = val >> 16;
    out[1] = (val >> 8) & 0xff;
    out[2] = val & 0xff;
  }

  state->out.ptr = out;
  insert_length(ofs, &state->out);
  return true;
}

#if 0

static bool convert_enum(size_t ofs, const upb_enumdef *e, upb_jsonparser* state) {
  int32_t num;
  const char* str = outptr(ofs, state);
  size_t size = state->outptr - str;
  CHK(upb_enumdef_ntoi(e, str, size, &num));
  pop_output(ofs, state);
  CHK(write_varint(num, state));
  return true;
}
#endif

#define READ_INT_BODY                                      \
  const char* ptr = state->outptr;                         \
  const char* end = state->outend;                         \
  bool neg = false;                                        \
                                                           \
  CHK(ptr != end);                                         \
                                                           \
  if (*ptr == '-') {                                       \
    neg = true;                                            \
    ptr++;                                                 \
  }                                                        \
                                                           \
  while (ptr != end) {                                     \
    CHK(!__builtin_mul_overflow(val, 10, &val));           \
    if (neg) {                                             \
      CHK(!__builtin_sub_overflow(val, *ptr - '0', &val)); \
    } else {                                               \
      CHK(!__builtin_add_overflow(val, *ptr - '0', &val)); \
    }                                                      \
    ptr++;                                                 \
  }                                                        \
                                                           \
  return true;

static bool convert_sfixed32(size_t ofs, upb_jsonparser* state) {
  int32_t val = 0;
  READ_INT_BODY
}

static bool convert_sfixed64(size_t ofs, upb_jsonparser* state) {
  int64_t val = 0;
  READ_INT_BODY
}

static bool convert_fixed32(size_t ofs, upb_jsonparser* state) {
  uint32_t val = 0;
  READ_INT_BODY
}

static bool convert_fixed64(size_t ofs, upb_jsonparser* state) {
  uint64_t val = 0;
  READ_INT_BODY
}

#undef CONVERT_INT_BODY


static const char* convert_bool(const char* ptr, const upb_fielddef* f,
                                upb_jsonparser* state) {
  switch (*ptr) {
    case kFalse:
      CHK(put_varint(0, &state->out));
      return ptr + 1;
    case kTrue:
      CHK(put_varint(1, &state->out));
      return ptr + 1;
    default:
      /* Should we accept 0/nonzero as true/false? */
      return NULL;
  }
}

static const char* read_double(const char* ptr, const upb_fielddef* f,
                               double* d, upb_jsonparser* state) {
  switch (*ptr) {
    case kNumber:
      return read_number(ptr, d);
    case kString: {
      upb_strview str;
      CHK(ptr = parse_str(ptr, &str.data, &str.size));
      if (upb_strview_eql(str, upb_strview_makez("NaN"))) {
        *d = NAN;
      } else if (upb_strview_eql(str, upb_strview_makez("Infinity"))) {
        *d = INFINITY;
      } else if (upb_strview_eql(str, upb_strview_makez("-Infinity"))) {
        *d = -INFINITY;
      } else {
        return NULL;
      }
      return ptr;
    }
    default:
      return NULL;
  }
}

static const char* convert_double(const char* ptr, const upb_fielddef* f,
                                  upb_jsonparser* state) {
  double d;
  CHK(ptr = read_double(ptr, f, &d, state));
  CHK(write_str(&d, 8, &state->out));
  return ptr;
}

static const char* convert_float(const char* ptr, const upb_fielddef* f,
                                 upb_jsonparser* state) {
  double d;
  float flt;
  CHK(ptr = read_double(ptr, f, &d, state));
  flt = d;
  CHK(write_str(&f, 4, &state->out));
  return ptr;
}

static const char* convert_string(const char* ptr, const upb_fielddef* f,
                                  upb_jsonparser* state) {
  const char* str;
  size_t len;

  CHK(ptr = parse_str(ptr, &str, &len));
  CHK(put_varint(len, &state->out));
  CHK(write_str(str, len, &state->out));
  return ptr;
}

static const char* convert_json_value(const char* ptr, const upb_fielddef* f,
                                      upb_jsonparser* state) {

  if (*ptr == kNull) {
    return ptr + 1;
  }

  CHK(write_tag(f, state));

  switch (upb_fielddef_type(f)) {
    case UPB_TYPE_BOOL:
      return convert_bool(ptr, f, state);
    case UPB_TYPE_FLOAT:
      return convert_float(ptr, f, state);
    case UPB_TYPE_DOUBLE:
      return convert_double(ptr, f, state);
    case UPB_TYPE_UINT32:
    case UPB_TYPE_INT32:
      CHK(ptr = convert_int32(ptr, f, state));
      break;
    case UPB_TYPE_INT64:
    case UPB_TYPE_UINT64:
      break;
    case UPB_TYPE_STRING:
      return convert_string(ptr, f, state);
    case UPB_TYPE_BYTES:
      return base64_decode(ptr, f, state);
      return ptr;
    case UPB_TYPE_ENUM:
      if (*ptr == kString) {
        const char* str;
        size_t len;
        const upb_enumdef* e = upb_fielddef_enumsubdef(f);
        int32_t num;
        CHK(ptr = parse_str(ptr, &str, &len));
        CHK(upb_enumdef_ntoi(e, str, len, &num));
        CHK(put_varint(num, &state->out));
        return ptr;
      }
      /* Fallthrough. */
    case UPB_TYPE_MESSAGE: {
      const upb_msgdef* m = upb_fielddef_msgsubdef(f);
      size_t ofs = buf_ofs(&state->out);
      switch (upb_msgdef_wellknowntype(m)) {
        case UPB_WELLKNOWN_UNSPECIFIED:
          return convert_json_object(ptr, m, state);
        case UPB_WELLKNOWN_STRINGVALUE:
          CHK(insert_length(ofs, state));
          CHK(put_knowntag(1, UPB_WIRE_TYPE_DELIMITED, state));
          break;
        case UPB_WELLKNOWN_BYTESVALUE:
          CHK(base64_decode(ofs, f, state));
          CHK(insert_length(ofs, state));
          CHK(write_knowntag(1, UPB_WIRE_TYPE_DELIMITED, state));
          break;
        case UPB_WELLKNOWN_DOUBLEVALUE:
          CHK(write_knowntag(1, UPB_WIRE_TYPE_64BIT, state));
          CHK(ptr = convert_double(ptr, f, state));
          CHK(insert_length(ofs, state));
          break;
        case UPB_WELLKNOWN_FLOATVALUE:
          CHK(write_knowntag(1, UPB_WIRE_TYPE_64BIT, state));
          CHK(ptr = convert_float(ptr, f, state));
          CHK(insert_length(ofs, state));
          break;
        case UPB_WELLKNOWN_INT64VALUE:
          CHK(convert_int64(ofs, state));
          CHK(write_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_UINT64VALUE:
          CHK(convert_uint64(ofs, state));
          CHK(write_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_UINT32VALUE:
          CHK(convert_uint32(ofs, state));
          CHK(write_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_INT32VALUE:
          CHK(convert_int32(ofs, state));
          CHK(write_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_FIELDMASK:
          CHK(parse_fieldmask(ofs, state));
          break;
        case UPB_WELLKNOWN_DURATION:
          CHK(parse_duration(ofs, state));
          break;
        case UPB_WELLKNOWN_TIMESTAMP:
          CHK(parse_timestamp(ofs, state));
          break;
        case UPB_WELLKNOWN_BOOLVALUE:
          /* Should we accept 0/nonzero as true/false? */
          return NULL;
        case UPB_WELLKNOWN_ANY:
        case UPB_WELLKNOWN_VALUE:
        case UPB_WELLKNOWN_LISTVALUE:
        case UPB_WELLKNOWN_STRUCT:
          return NULL;
      }
    }
  }

  UPB_UNREACHABLE();
}

const char* skip_json_value(const char* ptr) {
  int depth = 0;

  do {
    switch (*ptr) {
      case kObject:
      case kArray:
        depth++;
        break;
      case kEnd:
        depth--;
        break;
      case kTrue:
      case kFalse:
      case kNull:
        break;
      case kString: {
        const char* name;
        size_t len;
        ptr = parse_str(ptr, &name, &len);
        break;
      }
      case kNumber: {
        double d;
        ptr = parse_number(ptr, &d);
      }
    }
  } while (depth > 0);

  return ptr;
}

const char* convert_json_array(const char* ptr, const upb_fielddef* f,
                               upb_jsonparser* parser) {
  CHK(ptr = parse_char2(ptr, kArray, parser));

  while (true) {
    if (*ptr == kEnd) {
      return ptr + 1;
    }

    CHK(ptr = convert_json_value(ptr, f, parser));
  }

  UPB_UNREACHABLE();
}

static const char* convert_json_map(const char* ptr, const upb_fielddef* f,
                                    upb_jsonparser* parser) {
  const upb_msgdef* entry = upb_fielddef_msgsubdef(f);
  const upb_fielddef* key = upb_msgdef_itof(entry, UPB_MAPENTRY_KEY);
  const upb_fielddef* value = upb_msgdef_itof(entry, UPB_MAPENTRY_VALUE);
  size_t ofs;

  CHK(ptr = parse_char2(ptr, kObject, parser));
  CHK(write_tag(f, parser));
  ofs = buf_ofs(&parser->out);

  while (true) {
    if (*ptr == kEnd) {
      ptr++;
      break;
    }

    CHK(ptr = convert_json_value(ptr, key, parser));
    CHK(ptr = convert_json_value(ptr, value, parser));
  }

  CHK(insert_length(ofs, &parser->out));
  return ptr;
}

static const char* convert_json_object(const char* ptr, const upb_msgdef* m,
                                       upb_jsonparser* parser) {
  const char* name;
  size_t len;
  const upb_fielddef* f;
  size_t ofs = buf_ofs(&parser->out);

  CHK(ptr = parse_char2(ptr, kObject, parser));

  while (true) {
    if (*ptr == kEnd) {
      ptr++;
      break;
    }

    ptr = parse_str(ptr, &name, &len);
    f = upb_msgdef_ntof(m, name, len);

    if (!f) {
      if (parser->options & UPB_JSON_IGNORE_UNKNOWN) {
        ptr = skip_json_value(ptr);
      } else {
        upb_status_seterrf(parser->status,
                           "Unknown field %.*s when parsing message %s", len,
                           name, upb_msgdef_fullname(m));
        return NULL;
      }
    }

    if (upb_fielddef_isseq(f)) {
      CHK(ptr = convert_json_array(ptr, f, parser));
    } else if (upb_fielddef_ismap(f)) {
      CHK(ptr = convert_json_map(ptr, f, parser));
    } else {
      CHK(ptr = convert_json_value(ptr, f, parser));
    }
  }

  CHK(insert_length(ofs, &parser->out));
  return ptr;
}

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       const upb_symtab* any_msgs, int options, int max_depth,
                       upb_alloc* alloc, size_t* outlen, upb_status *s) {
  /* Stage 1: parse JSON generically. */
  jsonparser generic_parser = {
      buf + len, {NULL, NULL, NULL, alloc}, max_depth, s};

  CHK(is_proto3(m));
  CHK(buf = parse_json_object(buf, &generic_parser));
  CHK(skip_whitespace(buf, &generic_parser) == NULL);

  {
    /* Stage 2: convert generic JSON to protobuf. */
    outbuf* out = &generic_parser.out;
    size_t written = buf_ofs(out);
    const char* ptr = out->end - written;
    upb_jsonparser parser = {*out, m, any_msgs, s, options};

    /* Move stage one output to the end of the buffer. */
    memmove((void*)ptr, out->buf, written);

    /* TODO: should we support various well-known types at the top-level, or
     * does the top-level need to be a regular message? */
    CHK(ptr = convert_json_object(ptr, m, &parser));
    CHK(ptr == parser.out.end);

    *outlen = parser.out.ptr - parser.out.buf;
    return parser.out.buf;
  }
}

#if 0
char* upb_binarytojson(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen) {
  char* ret;

  if (!is_proto3(m)) return NULL;

  ret = upb_malloc(alloc, 1);
  ret[0] = 0;
  *outlen = 1;

  return ret;
}
#endif
