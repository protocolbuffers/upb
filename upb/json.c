
#include "upb/json.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "upb/upb.h"

#include "upb/port_def.inc"

#define CHK(x)                                                                \
  if (UPB_UNLIKELY(!(x))) {                                                   \
    if (upb_ok(parser->status)) {                                             \
      upb_status_seterrf(parser->status, "CHK failed on line: %d", __LINE__); \
    }                                                                         \
    return 0;                                                                 \
  }

#define CHK2(x)             \
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

static uint32_t zzencode_32(int32_t n) { return (n << 1) ^ (n >> 31); }
static uint64_t zzencode_64(int64_t n) { return (n << 1) ^ (n >> 63); }

/* Output Buffer **************************************************************/

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

  CHK2(need > ptr);  /* ptr + bytes didn't overflow. */
  CHK2(need < max);  /* we can exceed by doubling n. */

  while (n < need) {
    n *= 2;
  }

  out->buf = upb_realloc(out->alloc, out->buf, old, n);
  CHK2(out->buf);

  out->ptr = out->buf + ptr;
  out->end = out->buf + n;
  return out->ptr;
}

static char* reserve_bytes(size_t bytes, outbuf* out) {
  size_t have = out->end - out->ptr;
  return (have >= bytes) ? out->ptr : realloc_buf(bytes, out);
}

static bool write_str(const void* str, size_t n, outbuf* out) {
  CHK2(reserve_bytes(n, out));
  memcpy(out->ptr, str, n);
  out->ptr += n;
  return true;
}

static bool write_char(char ch, outbuf* out) {
  CHK2(reserve_bytes(1, out));
  *out->ptr = ch;
  out->ptr++;
  return true;
}

static bool write_varint(uint64_t val, outbuf* out) {
  CHK2(reserve_bytes(10, out));
  out->ptr += encode_varint(val, out->ptr);
  return true;
}

static bool write_known_tag(int wire_type, int fieldnum, outbuf* out) {
  return write_varint(wire_type | (fieldnum << 3), out);
}

static size_t buf_ofs(outbuf* out) { return out->ptr - out->buf; }

static bool insert_fixed_len(size_t ofs, outbuf* out) {
  size_t len = buf_ofs(out) - ofs;
  int intlen = len;
  char* ptr = out->buf + ofs;

  CHK2(len <= INT_MAX);
  CHK2(reserve_bytes(4, out));
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

  CHK2(len <= INT_MAX);
  CHK2(reserve_bytes(varint_len, out));
  ptr = out->buf + ofs;
  memmove(ptr + varint_len, ptr, len);
  memcpy(ptr, varint, varint_len);
  out->ptr += varint_len;
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

static bool parse_json_object(jsonparser* parser);
static bool parse_json_array(jsonparser* parser);

/* Input buffer. */

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

static bool is_eof(jsonparser* parser) {
  return parser->ptr == parser->end;
}

static bool skip_whitespace(jsonparser* parser) {
  while (!is_eof(parser) && is_whitespace(*parser->ptr)) {
    parser->ptr++;
  }
  return !is_eof(parser);
}

static bool has_n_bytes(size_t n, jsonparser* parser) {
  return parser->end - parser->ptr >= n;
}

static bool parse_char(char ch, jsonparser* parser) {
  CHK(!is_eof(parser));
  CHK(*parser->ptr == ch);
  parser->ptr++;
  return true;
}

static bool parse_char_skipws(char ch, jsonparser* parser) {
  CHK(skip_whitespace(parser));
  CHK(*parser->ptr == ch);
  parser->ptr++;
  return true;
}

static UPB_FORCEINLINE bool parse_lit(const char* lit, jsonparser* parser) {
  size_t len = strlen(lit);
  CHK(has_n_bytes(len, parser));
  CHK(memcmp(parser->ptr, lit, len) == 0);
  parser->ptr += len;
  return true;
}

/* NULL is not allowed in JSON text, so we use 0 as failure. */
static char peek_char_skipws(jsonparser* parser) {
  CHK(skip_whitespace(parser));
  return *parser->ptr;
}

static char peek_char(jsonparser* parser) {
  CHK(!is_eof(parser));
  return *parser->ptr;
}

static char consume_char_skipws(jsonparser* parser) {
  char ch = peek_char_skipws(parser);
  CHK(ch);
  parser->ptr++;
  return ch;
}

static char consume_char(jsonparser* parser) {
  char ch = peek_char(parser);
  CHK(ch);
  parser->ptr++;
  return ch;
}

static bool skip_digits(jsonparser* parser) {
  const char* start = parser->ptr;

  while (true) {
    char ch = peek_char(parser);
    if (ch < '0' || ch > '9') {
      break;
    }
    parser->ptr++;
  }

  /* We must consume at least one digit. */
  return parser->ptr != start;
}

/* Generic JSON parser. */

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

static bool write_utf8_codepoint(uint32_t cp, jsonparser* parser) {
  char utf8[3]; /* support \u0000 -- \uFFFF -- need only three bytes. */

  if (cp <= 0x7F) {
    return write_char(cp, &parser->out);
  } else if (cp <= 0x07FF) {
    utf8[0] = ((cp >> 6) & 0x1F) | 0xC0;
    utf8[1] = ((cp >> 0) & 0x3F) | 0x80;
    return write_str(utf8, 2, &parser->out);
  } else /* cp <= 0xFFFF */ {
    utf8[0] = ((cp >> 12) & 0x0F) | 0xE0;
    utf8[1] = ((cp >> 6) & 0x3F) | 0x80;
    utf8[2] = ((cp >> 0) & 0x3F) | 0x80;
    return write_str(utf8, 3, &parser->out);
  }

  /* TODO(haberman): Handle high surrogates: if codepoint is a high surrogate
   * we have to wait for the next escape to get the full code point). */
}

static bool parse_escape(jsonparser* parser) {
  CHK(parse_char('\\', parser));

  switch (consume_char(parser)) {
    case '"':
      CHK(write_char('"', &parser->out));
      break;
    case '\\':
      CHK(write_char('\\', &parser->out));
      break;
    case '/':
      CHK(write_char('/', &parser->out));
      break;
    case 'b':
      CHK(write_char('\b', &parser->out));
      break;
    case 'f':
      CHK(write_char('\f', &parser->out));
      break;
    case 'n':
      CHK(write_char('\n', &parser->out));
      break;
    case 'r':
      CHK(write_char('\r', &parser->out));
      break;
    case 't':
      CHK(write_char('\t', &parser->out));
      break;
    case 'u': {
      uint32_t codepoint = 0;
      CHK(has_n_bytes(4, parser));
      CHK(parse_hex_digit(parser->ptr[0], &codepoint));
      CHK(parse_hex_digit(parser->ptr[1], &codepoint));
      CHK(parse_hex_digit(parser->ptr[2], &codepoint));
      CHK(parse_hex_digit(parser->ptr[3], &codepoint));
      CHK(write_utf8_codepoint(codepoint, parser));
      parser->ptr += 4;
      break;
    }
    default:
      return false;
  }

  return true;
}

static bool parse_json_string(jsonparser* parser) {
  const char* span_start;
  size_t ofs;

  CHK(parse_char_skipws('"', parser));
  CHK(write_char(kString, &parser->out));
  span_start = parser->ptr;
  ofs = buf_ofs(&parser->out);

  while (true) {
    /* TODO: validate UTF-8. */
    switch (peek_char(parser)) {
      case '"':
        goto done;
      case '\\':
        CHK(write_str(span_start, parser->ptr - span_start, &parser->out));
        CHK(parse_escape(parser));
        span_start = parser->ptr;
        break;
      case 0:
        return false;
      default:
        CHK((unsigned char)*parser->ptr >= 0x20);
        parser->ptr++;
        break;
    }
  }

done:
  CHK(write_str(span_start, parser->ptr - span_start, &parser->out));
  parser->ptr++;
  CHK(insert_fixed_len(ofs, &parser->out));

  return true;
}

static bool parse_json_number(jsonparser* parser) {
  const char* start = parser->ptr;
  char* end;
  double d;

  /* No need to check return. */
  parse_char('-', parser);

  if (!parse_char('0', parser)) {
    CHK(skip_digits(parser));
  }

  if (is_eof(parser)) goto parse;

  if (parse_char('.', parser)) {
    CHK(skip_digits(parser));
  }

  if (is_eof(parser)) goto parse;

  if (*parser->ptr == 'e' || *parser->ptr == 'E') {
    parser->ptr++;
    CHK(!is_eof(parser));

    if (*parser->ptr == '+' || *parser->ptr == '-') {
      parser->ptr++;
    }

    CHK(skip_digits(parser));
  }

parse:
  errno = 0;
  d = strtod(start, &end);

  CHK(errno == 0 && end == parser->ptr);
  CHK(write_char(kNumber, &parser->out));
  CHK(write_str(&d, sizeof(d), &parser->out));

  return true;
}

static bool parse_json_value(jsonparser* parser) {
  CHK(--parser->depth != 0);

  switch (peek_char_skipws(parser)) {
    case '{':
      CHK(parse_json_object(parser));
      break;
    case '[':
      CHK(parse_json_array(parser));
      break;
    case '"':
      CHK(parse_json_string(parser));
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
      CHK(parse_json_number(parser));
      break;
    case 't':
      CHK(parse_lit("true", parser));
      CHK(write_char(kTrue, &parser->out));
      break;
    case 'f':
      CHK(parse_lit("false", parser));
      CHK(write_char(kFalse, &parser->out));
      break;
    case 'n':
      CHK(parse_lit("null", parser));
      CHK(write_char(kNull, &parser->out));
      break;
    default:
      return false;
  }

  parser->depth++;
  return true;
}


static bool parse_json_array(jsonparser* parser) {
  CHK(parse_char_skipws('[', parser));
  CHK(write_char(kArray, &parser->out));

  if (parse_char_skipws(']', parser)) {
    return write_char(kEnd, &parser->out);
  }

  while (true) {
    CHK(parse_json_value(parser));
    switch (consume_char_skipws(parser)) {
      case ',':
        break;
      case ']':
        CHK(write_char(kEnd, &parser->out));
        return true;
      default:
        return false;
    }
  }

  UPB_UNREACHABLE();
}

static bool parse_json_object(jsonparser* parser) {
  CHK(parse_char_skipws('{', parser));
  CHK(write_char(kObject, &parser->out));

  if (parse_char_skipws('}', parser)) {
    return write_char(kEnd, &parser->out);
  }

  while (true) {
    CHK(parse_json_string(parser));
    CHK(parse_char_skipws(':', parser));
    CHK(parse_json_value(parser));
    switch (consume_char_skipws(parser)) {
      case ',':
        break;
      case '}':
        CHK(write_char(kEnd, &parser->out));
        return true;
      default:
        return false;
    }
  }

  UPB_UNREACHABLE();
}

/* Schema-aware JSON -> Protobuf translation **********************************/

/* This stage converts the generic JSON representation of stage 1 to serialized
 * protobuf binary format, according to a given schema.
 *
 * In this stage we don't need to bounds-check ptr when we are inside any kind
 * of nesting (object, array) because we know everything is balanced and
 * properly terminated.
 */

struct upb_jsonparser {
  const char* ptr;
  const char* end;
  outbuf out;
  const upb_symtab* any_msgs;
  upb_status *status;
  int options;
};

static bool convert_json_object(const upb_msgdef* m, upb_jsonparser* parser);

static bool is_proto3(const upb_msgdef* m) {
  return upb_filedef_syntax(upb_msgdef_file(m)) == UPB_SYNTAX_PROTO3;
}

static bool is_eof2(upb_jsonparser* parser) {
  return parser->ptr == parser->end;
}

static char peek_char2(upb_jsonparser* parser) {
  CHK(!is_eof2(parser));
  return *parser->ptr;
}

static char consume_char2(upb_jsonparser* parser) {
  char ch = peek_char2(parser);
  CHK(ch);
  parser->ptr++;
  return ch;
}

static bool parse_char2(char ch, upb_jsonparser* parser) {
  return consume_char2(parser) == ch;
}

static upb_strview read_str(upb_jsonparser* parser) {
  int len;
  UPB_ASSERT(*(parser->ptr - 1) == kString);
  memcpy(&len, parser->ptr, sizeof(int));
  parser->ptr += sizeof(int);
  return upb_strview_make(parser->ptr, len);
}

static double read_num(upb_jsonparser* parser) {
  double d;
  UPB_ASSERT(*(parser->ptr - 1) == kNumber);
  memcpy(&d, parser->ptr, sizeof(d));
  parser->ptr += sizeof(d);
  return d;
}

static bool write_tag(const upb_fielddef *f, upb_jsonparser* parser) {
  return true;
}

/* Base64 decoding. */

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

static char* decode_padding(const char* in, char* out, upb_jsonparser* parser) {
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

static bool base64_decode(const upb_fielddef* f, upb_jsonparser* parser) {
  upb_strview str = read_str(parser);
  size_t ofs = buf_ofs(&parser->out);
  const char* in = str.data;
  const char *limit = str.data + str.size;
  /* This is a conservative estimate, assuming no padding. */
  char* out = reserve_bytes(str.size / 4 * 3, &parser->out);

  if ((str.size % 4) != 0) {
    upb_status_seterrf(parser->status,
                       "Base64 input for bytes field not a multiple of 4: %s",
                       upb_fielddef_name(f));
  }

  for (; in < limit; in += 4, out += 3) {
    uint32_t val;

    val = b64tab(in[0]) << 18 | b64tab(in[1]) << 12 | b64tab(in[2]) << 6 |
          b64tab(in[3]);

    /* Returns true if any of the characters returned -1. */
    if (UPB_UNLIKELY(val & 0x80000000)) {
      if (nonbase64(in[0]) || nonbase64(in[1]) || nonbase64(in[2]) ||
          nonbase64(in[3])) {
        upb_status_seterrf(parser->status,
                           "Non-base64 characters in bytes field: %s",
                           upb_fielddef_name(f));
        return false;
      }

      if (in != limit - 4 || (out = decode_padding(in, out, parser)) == NULL) {
        upb_status_seterrf(parser->status,
                           "Incorrect base64 padding for field: %s (%.*s)",
                           upb_fielddef_name(f), 4, in);
        return false;
      }
    }

    out[0] = val >> 16;
    out[1] = (val >> 8) & 0xff;
    out[2] = val & 0xff;
  }

  parser->out.ptr = out;
  insert_varint_len(ofs, &parser->out);
  return true;
}

#define READ_INT_BODY                                          \
  switch (consume_char2(parser)) {                             \
    case kNumber:                                              \
      *out = read_num(parser);                                 \
      return true;                                             \
    case kString: {                                            \
      upb_strview str = read_str(parser);                      \
      const char* ptr = str.data;                              \
      const char* end = str.data + str.size;                   \
      bool neg = false;                                        \
                                                               \
      CHK(ptr != end);                                         \
                                                               \
      if (*ptr == '-') {                                       \
        neg = true;                                            \
        ptr++;                                                 \
        CHK(ptr != end);                                       \
      }                                                        \
                                                               \
      while (ptr != end) {                                     \
        CHK(*ptr >= '0' && *ptr <= '9');                       \
        CHK(!__builtin_mul_overflow(val, 10, &val));           \
        if (neg) {                                             \
          CHK(!__builtin_sub_overflow(val, *ptr - '0', &val)); \
        } else {                                               \
          CHK(!__builtin_add_overflow(val, *ptr - '0', &val)); \
        }                                                      \
        ptr++;                                                 \
      }                                                        \
                                                               \
      *out = val;                                              \
      return ptr == end;                                       \
    }                                                          \
    default:                                                   \
      return false;                                            \
  }                                                            \
  UPB_UNREACHABLE();

static bool read_int64(upb_jsonparser* parser, int64_t* out) {
  int64_t val = 0;
  READ_INT_BODY
}

static bool read_uint64(upb_jsonparser* parser, uint64_t* out) {
  uint64_t val = 0;
  READ_INT_BODY
}

#undef CONVERT_INT_BODY

static bool read_double(const upb_fielddef* f, double* d,
                        upb_jsonparser* parser) {
  /* C89 does not have macros for NAN or INFINITY. */
  static const double nan = 0.0 / 0.0;
  static const double inf = 1.0 / 0.0;

  switch (consume_char2(parser)) {
    case kNumber:
      *d = read_num(parser);
      return true;
    case kString: {
      upb_strview str = read_str(parser);
      if (upb_strview_eql(str, upb_strview_makez("NaN"))) {
        *d = nan;
      } else if (upb_strview_eql(str, upb_strview_makez("Infinity"))) {
        *d = inf;
      } else if (upb_strview_eql(str, upb_strview_makez("-Infinity"))) {
        *d = -inf;
      } else {
        return false;
      }
      return true;
    }
    default:
      return false;
  }
}

static bool convert_wellknown_value(upb_jsonparser* parser) {
  switch (consume_char2(parser)) {
    case kNull:
      CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 1, &parser->out));
      CHK(write_varint(0, &parser->out));
      return true;
    case kNumber: {
      double d = read_num(parser);
      CHK(write_known_tag(UPB_WIRE_TYPE_64BIT, 2, &parser->out));
      CHK(write_str(&d, 8, &parser->out));
      return true;
    }
    case kString: {
      upb_strview str = read_str(parser);
      CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 3, &parser->out));
      CHK(write_varint(str.size, &parser->out));
      CHK(write_str(str.data, str.size, &parser->out));
      return true;
    }
    case kTrue:
      CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 4, &parser->out));
      CHK(write_varint(1, &parser->out));
      return true;
    case kFalse:
      CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 4, &parser->out));
      CHK(write_varint(0, &parser->out));
      return true;
    case kObject:
    case kArray:
    default:
      return false;
  }
}

static bool convert_wellknown_listvalue(upb_jsonparser* parser) {
  CHK(parse_char2(kArray, parser));

  while (true) {
    size_t ofs;
    if (parse_char2(kEnd, parser)) {
      return true;
    }

    /* repeated Value values = 1; */
    CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 1, &parser->out));
    ofs = buf_ofs(&parser->out);
    CHK(convert_wellknown_value(parser));
    CHK(insert_varint_len(ofs, &parser->out));
  }
}

static bool convert_wellknown_struct_entry(upb_jsonparser* parser) {
  upb_strview key;
  size_t value_ofs;

  CHK(parse_char2(kString, parser));

  key = read_str(parser);
  CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 1, &parser->out));
  CHK(write_varint(key.size, &parser->out));
  CHK(write_str(key.data, key.size, &parser->out));

  CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 2, &parser->out));
  value_ofs = buf_ofs(&parser->out);
  CHK(convert_wellknown_value(parser));
  CHK(insert_varint_len(value_ofs, &parser->out));

  return true;
}

static bool convert_wellknown_struct(upb_jsonparser* parser) {
  CHK(parse_char2(kObject, parser));

  while (true) {
    size_t entry_ofs;
    if (parse_char2(kEnd, parser)) {
      return true;
    }

    /* map<string, Value> fields = 1; */
    CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 1, &parser->out));
    entry_ofs = buf_ofs(&parser->out);
    CHK(convert_wellknown_struct_entry(parser));
    CHK(insert_varint_len(entry_ofs, &parser->out));
  }
}

static bool convert_timestamp(upb_jsonparser* parser) {
  return true;
}

static bool convert_duration(upb_jsonparser* parser) {
  return true;
}

static bool convert_fieldmask(upb_jsonparser* parser) {
  return true;
}

static bool convert_any(upb_jsonparser* parser) {
  return true;
}

static bool convert_json_value(const upb_fielddef* f, upb_jsonparser* parser) {
  if (*parser->ptr == kNull) {
    parser->ptr++;
    return true;
  }

  CHK(write_tag(f, parser));

  switch (upb_fielddef_type(f)) {
    case UPB_TYPE_BOOL:
      switch (consume_char2(parser)) {
        case kFalse:
          CHK(write_varint(0, &parser->out));
          return true;
        case kTrue:
          CHK(write_varint(1, &parser->out));
          return true;
        default:
          /* Should we accept 0/nonzero as true/false? */
          return false;
      }
    case UPB_TYPE_FLOAT:
    case UPB_TYPE_DOUBLE: {
      double d;
      CHK(read_double(f, &d, parser));
      if (upb_fielddef_type(f) == UPB_TYPE_FLOAT) {
        float flt = d;
        return write_str(&flt, 4, &parser->out);
      } else {
        return write_str(&d, 8, &parser->out);
      }
      return true;
    }
    case UPB_TYPE_UINT32: {
      uint64_t u64;
      uint32_t val;
      CHK(read_uint64(parser, &u64) && u64 <= UINT32_MAX);
      val = u64;
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_FIXED32:
          return write_str(&val, 4, &parser->out);
        case UPB_DESCRIPTOR_TYPE_UINT32:
          return write_varint(val, &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_UINT64: {
      uint64_t val;
      CHK(read_uint64(parser, &val));
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_FIXED64:
          return write_str(&val, 8, &parser->out);
        case UPB_DESCRIPTOR_TYPE_UINT64:
          return write_varint(val, &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_INT32: {
      int64_t i64;
      int32_t val;
    int32_val:
      CHK(read_int64(parser, &i64) && i64 <= INT32_MAX && i64 >= INT32_MIN);
      val = i64;
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_SFIXED32:
          return write_str(&val, 4, &parser->out);
        case UPB_DESCRIPTOR_TYPE_INT32:
        case UPB_DESCRIPTOR_TYPE_ENUM:
          return write_varint(val, &parser->out);
        case UPB_DESCRIPTOR_TYPE_SINT32:
          return write_varint(zzencode_32(val), &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_INT64: {
      int64_t val;
      CHK(read_int64(parser, &val));
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_SFIXED32:
          return write_str(&val, 8, &parser->out);
        case UPB_DESCRIPTOR_TYPE_INT32:
        case UPB_DESCRIPTOR_TYPE_ENUM:
          return write_varint(val, &parser->out);
        case UPB_DESCRIPTOR_TYPE_SINT32:
          return write_varint(zzencode_64(val), &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_STRING: {
      upb_strview str;
      CHK(consume_char2(parser) == kString);
      str = read_str(parser);
      CHK(write_varint(str.size, &parser->out));
      CHK(write_str(str.data, str.size, &parser->out));
      return true;
    }
    case UPB_TYPE_BYTES:
      CHK(consume_char2(parser) == kString);
      return base64_decode(f, parser);
    case UPB_TYPE_ENUM:
      if (parse_char2(kString, parser)) {
        upb_strview str = read_str(parser);
        const upb_enumdef* e = upb_fielddef_enumsubdef(f);
        int32_t num;
        CHK(upb_enumdef_ntoi(e, str.data, str.size, &num));
        CHK(write_varint(num, &parser->out));
        return true;
      }
      goto int32_val;
    case UPB_TYPE_MESSAGE: {
      const upb_msgdef* m = upb_fielddef_msgsubdef(f);
      size_t ofs = buf_ofs(&parser->out);
      switch (upb_msgdef_wellknowntype(m)) {
        case UPB_WELLKNOWN_UNSPECIFIED:
          CHK(convert_json_object(m, parser));
          if (upb_fielddef_descriptortype(f) == UPB_DESCRIPTOR_TYPE_GROUP) {
            return write_known_tag(UPB_WIRE_TYPE_END_GROUP,
                                   upb_fielddef_number(f), &parser->out);
          }
          break;
        case UPB_WELLKNOWN_STRINGVALUE:
        case UPB_WELLKNOWN_BYTESVALUE:
        case UPB_WELLKNOWN_DOUBLEVALUE:
        case UPB_WELLKNOWN_FLOATVALUE:
        case UPB_WELLKNOWN_INT64VALUE:
        case UPB_WELLKNOWN_UINT64VALUE:
        case UPB_WELLKNOWN_UINT32VALUE:
        case UPB_WELLKNOWN_INT32VALUE:
        case UPB_WELLKNOWN_BOOLVALUE:
          CHK(convert_json_value(upb_msgdef_itof(m, 1), parser));
          break;
        case UPB_WELLKNOWN_FIELDMASK:
          CHK(convert_fieldmask(parser));
          break;
        case UPB_WELLKNOWN_DURATION:
          CHK(convert_duration(parser));
          break;
        case UPB_WELLKNOWN_TIMESTAMP:
          CHK(convert_timestamp(parser));
          break;
        case UPB_WELLKNOWN_ANY:
          CHK(convert_any(parser));
          break;
        case UPB_WELLKNOWN_VALUE:
          CHK(convert_wellknown_value(parser));
          break;
        case UPB_WELLKNOWN_LISTVALUE:
          CHK(convert_wellknown_listvalue(parser));
          break;
        case UPB_WELLKNOWN_STRUCT:
          CHK(convert_wellknown_struct(parser));
          break;
      }
      CHK(insert_varint_len(ofs, &parser->out));
    }
  }

  UPB_UNREACHABLE();
}

static void skip_json_value(upb_jsonparser* parser) {
  int depth = 0;

  do {
    switch (*parser->ptr) {
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
      case kString:
        parser->ptr++;
        read_str(parser);
        break;
      case kNumber:
        read_num(parser);
    }
  } while (depth > 0);
}

static bool convert_json_array(const upb_fielddef* f, upb_jsonparser* parser) {
  CHK(parse_char2(kArray, parser));

  while (true) {
    switch (peek_char2(parser)) {
      case kEnd:
        parser->ptr++;
        return true;
      default:
        CHK(convert_json_value(f, parser));
        break;
    }
  }

  UPB_UNREACHABLE();
}

static bool convert_json_map(const upb_fielddef* f, upb_jsonparser* parser) {
  const upb_msgdef* entry = upb_fielddef_msgsubdef(f);
  const upb_fielddef* key = upb_msgdef_itof(entry, UPB_MAPENTRY_KEY);
  const upb_fielddef* value = upb_msgdef_itof(entry, UPB_MAPENTRY_VALUE);
  size_t ofs;

  CHK(parse_char2(kObject, parser));
  CHK(write_tag(f, parser));
  ofs = buf_ofs(&parser->out);

  while (true) {
    if (peek_char2(parser) == kEnd) {
      parser->ptr++;
      break;
    }

    CHK(convert_json_value(key, parser));
    CHK(convert_json_value(value, parser));
  }

  CHK(insert_varint_len(ofs, &parser->out));
  return true;
}

static bool convert_json_object(const upb_msgdef* m, upb_jsonparser* parser) {
  upb_strview name;
  const upb_fielddef* f;
  size_t ofs = buf_ofs(&parser->out);

  CHK(parse_char2(kObject, parser));

  while (true) {
    if (peek_char2(parser) == kEnd) {
      parser->ptr++;
      break;
    }

    parse_char2(kString, parser);
    name = read_str(parser);
    f = upb_msgdef_ntof(m, name.data, name.size);

    if (!f) {
      if (parser->options & UPB_JSON_IGNORE_UNKNOWN) {
        skip_json_value(parser);
      } else {
        upb_status_seterrf(parser->status,
                           "Unknown field %.*s when parsing message %s",
                           name.size, name.data, upb_msgdef_fullname(m));
        return NULL;
      }
    }

    if (upb_fielddef_isseq(f)) {
      CHK(convert_json_array(f, parser));
    } else if (upb_fielddef_ismap(f)) {
      CHK(convert_json_map(f, parser));
    } else {
      CHK(convert_json_value(f, parser));
    }
  }

  CHK(insert_varint_len(ofs, &parser->out));
  return true;
}

char* _parse_json_stage1(const char* buf, size_t len, int max_depth,
                         upb_alloc* alloc, size_t* outlen, upb_status* s) {
  jsonparser parser;

  parser.ptr = buf;
  parser.end = buf + len;
  parser.depth = max_depth;
  parser.status = s;
  parser.out.alloc = alloc;
  parser.out.buf = NULL;
  parser.out.ptr = NULL;
  parser.out.end = NULL;

  if (parse_json_value(&parser) && !skip_whitespace(&parser)) {
    *outlen = parser.out.ptr - parser.out.buf;
    return parser.out.buf;
  } else {
    upb_free(alloc, parser.out.buf);
    return NULL;
  }
}

static char* parse_json_stage2(const char* buf, size_t len, const upb_msgdef* m,
                               const upb_symtab* any_msgs, int options,
                               upb_alloc* alloc, size_t* outlen,
                               upb_status* s) {
  upb_jsonparser parser;

  parser.ptr = buf;
  parser.end = buf + len;
  parser.any_msgs = any_msgs;
  parser.status = s;
  parser.options = options;
  parser.out.alloc = alloc;
  parser.out.buf = NULL;
  parser.out.ptr = NULL;
  parser.out.end = NULL;

  /* TODO: should we support various well-known types at the top-level, or
   * does the top-level need to be a regular message? */
  if (convert_json_object(m, &parser) && parser.ptr == parser.end) {
    *outlen = parser.out.ptr - parser.out.buf;
    return parser.out.buf;
  } else {
    upb_free(alloc, parser.out.buf);
    return NULL;
  }
}

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       const upb_symtab* any_msgs, int options, int max_depth,
                       upb_alloc* alloc, size_t* outlen, upb_status *s) {
  char* stage1;
  char* stage2;
  size_t stage1_len;

  CHK2(is_proto3(m));
  CHK2(stage1 = _parse_json_stage1(buf, len, max_depth, alloc, &stage1_len, s));

  stage2 = parse_json_stage2(stage1, stage1_len, m, any_msgs, options, alloc,
                             outlen, s);
  upb_free(alloc, stage1);

  return stage2;
}
