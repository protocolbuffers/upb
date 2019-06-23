
#include "upb/json.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <execinfo.h>
#include <unistd.h>

#include "upb/upb.h"

#include "upb/port_def.inc"

#include <stdio.h>
/* Maps descriptor type -> wire type.  */
static const uint8_t upb_desctype_to_wiretype[] = {
  UPB_WIRE_TYPE_END_GROUP,      /* ENDGROUP */
  UPB_WIRE_TYPE_64BIT,          /* DOUBLE */
  UPB_WIRE_TYPE_32BIT,          /* FLOAT */
  UPB_WIRE_TYPE_VARINT,         /* INT64 */
  UPB_WIRE_TYPE_VARINT,         /* UINT64 */
  UPB_WIRE_TYPE_VARINT,         /* INT32 */
  UPB_WIRE_TYPE_64BIT,          /* FIXED64 */
  UPB_WIRE_TYPE_32BIT,          /* FIXED32 */
  UPB_WIRE_TYPE_VARINT,         /* BOOL */
  UPB_WIRE_TYPE_DELIMITED,      /* STRING */
  UPB_WIRE_TYPE_START_GROUP,    /* GROUP */
  UPB_WIRE_TYPE_DELIMITED,      /* MESSAGE */
  UPB_WIRE_TYPE_DELIMITED,      /* BYTES */
  UPB_WIRE_TYPE_VARINT,         /* UINT32 */
  UPB_WIRE_TYPE_VARINT,         /* ENUM */
  UPB_WIRE_TYPE_32BIT,          /* SFIXED32 */
  UPB_WIRE_TYPE_64BIT,          /* SFIXED64 */
  UPB_WIRE_TYPE_VARINT,         /* SINT32 */
  UPB_WIRE_TYPE_VARINT,         /* SINT64 */
};

#define CHK(x)                                                             \
  if (UPB_UNLIKELY(!(x))) {                                                \
    if (upb_ok(parser->status)) {                                          \
      upb_status_seterrf(parser->status, "CHK failed on: %s:%d", __FILE__, \
                         __LINE__);                                        \
    }                                                                      \
    return 0;                                                              \
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

static bool write_known_tag(uint8_t wire_type, uint32_t fieldnum, outbuf* out) {
  UPB_ASSERT(wire_type <= 5 && wire_type >= 0);
  return write_varint(wire_type | (fieldnum << 3), out);
}

static bool write_string_field(uint32_t fieldnum, const char* buf,
                               unsigned size, outbuf* out) {
  CHK2(write_known_tag(UPB_WIRE_TYPE_DELIMITED, fieldnum, out));
  CHK2(write_varint(size, out));
  return write_str(buf, size, out);
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
  CHK2(*parser->ptr == ch);
  parser->ptr++;
  return true;
}

static bool parse_char_skipws(char ch, jsonparser* parser) {
  CHK(skip_whitespace(parser));
  CHK2(*parser->ptr == ch);
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

static bool parse_hex_digit(char ch, int* digit) {
  if (ch >= '0' && ch <= '9') {
    *digit = (ch - '0');
  } else if (ch >= 'a' && ch <= 'f') {
    *digit = ((ch - 'a') + 10);
  } else if (ch >= 'A' && ch <= 'F') {
    *digit = ((ch - 'A') + 10);
  } else {
    return false;
  }

  return true;
}

static bool parse_codepoint(uint32_t* cp, jsonparser* parser) {
  int i, val = 0;
  CHK(has_n_bytes(4, parser));
  for (i = 0; i < 4; i++) {
    int digit;
    CHK(parse_hex_digit(*parser->ptr++, &digit));
    val <<= 4;
    val |= digit;
  }
  *cp = val;
  return true;
}

static bool write_utf8_codepoint(uint32_t cp, jsonparser* parser) {
  char utf8[4];
  int n;

  if (cp <= 0x7F) {
    return write_char(cp, &parser->out);
  } else if (cp <= 0x07FF) {
    utf8[0] = ((cp >> 6) & 0x1F) | 0xC0;
    utf8[1] = ((cp >> 0) & 0x3F) | 0x80;
    n = 2;
  } else if (cp <= 0xFFFF) {
    utf8[0] = ((cp >> 12) & 0x0F) | 0xE0;
    utf8[1] = ((cp >> 6) & 0x3F) | 0x80;
    utf8[2] = ((cp >> 0) & 0x3F) | 0x80;
    n = 3;
  } else if (cp < 0x10FFFF) {
    utf8[0] = ((cp >> 18) & 0x07) | 0xF0;
    utf8[1] = ((cp >> 12) & 0x3f) | 0x80;
    utf8[2] = ((cp >> 6) & 0x3f) | 0x80;
    utf8[3] = ((cp >> 0) & 0x3f) | 0x80;
    n = 4;
  } else {
    return false;
  }

  return write_str(utf8, n, &parser->out);
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
      uint32_t cp;
      CHK(parse_codepoint(&cp, parser));
      if (cp >= 0xd800 && cp <= 0xdbff) {
        /* Surrogate pair: two 16-bit codepoints become a 32-bit codepoint. */
        uint32_t high = cp;
        uint32_t low;
        CHK(parse_lit("\\u", parser));
        CHK(parse_codepoint(&low, parser));
        CHK(low >= 0xdc00 && low <= 0xdfff);
        cp = (high & 0x3ff) << 10;
        cp |= (low & 0x3ff);
        cp += 0x10000;
      }
      CHK(write_utf8_codepoint(cp, parser));
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

  /* Currently the min/max-val conformance tests fail if we check this.  Does
   * this mean the conformance tests are wrong or strtod() is wrong, or
   * something else?  Investigate further. */
  /* CHK(errno == 0); */
  CHK(end == parser->ptr);
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
static bool convert_json_field(const upb_msgdef* m, upb_jsonparser* parser);
static bool convert_json_value(const upb_fielddef* f, upb_jsonparser* parser);
static bool convert_wellknown_value(upb_jsonparser* parser);
static bool convert_wellknown(const upb_msgdef* m, upb_jsonparser* parser);

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

static bool try_parse_char2(char want_ch, upb_jsonparser* parser) {
  char ch = peek_char2(parser);
  CHK(ch);
  CHK2(ch == want_ch);
  parser->ptr++;
  return true;
}

static bool parse_char2(char want_ch, upb_jsonparser* parser) {
  return consume_char2(parser) == want_ch;
}

static upb_strview read_str(upb_jsonparser* parser) {
  const char* str;
  int len;
  UPB_ASSERT(*(parser->ptr - 1) == kString);
  memcpy(&len, parser->ptr, sizeof(int));
  parser->ptr += sizeof(int);
  str = parser->ptr;
  parser->ptr += len;
  return upb_strview_make(str, len);
}

static const char* read_str2(upb_jsonparser* parser, const char** end) {
  upb_strview str = read_str(parser);
  *end = str.data + str.size;
  return str.data;
}

static double read_num(upb_jsonparser* parser) {
  double d;
  UPB_ASSERT(*(parser->ptr - 1) == kNumber);
  memcpy(&d, parser->ptr, sizeof(d));
  parser->ptr += sizeof(d);
  return d;
}

static bool write_tag(const upb_fielddef *f, upb_jsonparser* parser) {
  int wire_type = upb_desctype_to_wiretype[upb_fielddef_descriptortype(f)];
  int fieldnum = upb_fielddef_number(f);
  return write_known_tag(wire_type, fieldnum, &parser->out);
}

/* Base64 decoding. */

/* Table includes the normal base64 chars plus the URL-safe variant. */
static const signed char b64table[] = {
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      62/*+*/, -1,      62/*-*/, -1,      63/*/ */,
  52/*0*/, 53/*1*/, 54/*2*/, 55/*3*/, 56/*4*/, 57/*5*/, 58/*6*/, 59/*7*/,
  60/*8*/, 61/*9*/, -1,      -1,      -1,      -1,      -1,      -1,
  -1,       0/*A*/,  1/*B*/,  2/*C*/,  3/*D*/,  4/*E*/,  5/*F*/,  6/*G*/,
  07/*H*/,  8/*I*/,  9/*J*/, 10/*K*/, 11/*L*/, 12/*M*/, 13/*N*/, 14/*O*/,
  15/*P*/, 16/*Q*/, 17/*R*/, 18/*S*/, 19/*T*/, 20/*U*/, 21/*V*/, 22/*W*/,
  23/*X*/, 24/*Y*/, 25/*Z*/, -1,      -1,      -1,      -1,      63/*_*/,
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

/* Handles either padded ("XX==") or unpadded ("XX") trailing characters. */
static bool decode_partialb64(const char* ptr, int n, upb_jsonparser* parser) {
  int32_t val;
  int outbytes;
  char buf[2];

  switch (n) {
    case 2:
      val = b64tab(ptr[0]) << 18 | b64tab(ptr[1]) << 12;
      buf[0] = val >> 16;
      outbytes = 1;
      break;
    case 3:
      val = b64tab(ptr[0]) << 18 | b64tab(ptr[1]) << 12 | b64tab(ptr[2]) << 6;
      buf[0] = val >> 16;
      buf[1] = (val >> 8) & 0xff;
      outbytes = 2;
      break;
    default:
      return false;
  }

  CHK(val >= 0);  /* No non-base64 chars (or padding) encountered. */
  return write_str(buf, outbytes, &parser->out);
}

static bool decode_padding(const char* ptr, upb_jsonparser* parser) {
  if (ptr[3] == '=') {
    if (ptr[2] == '=') {
      return decode_partialb64(ptr, 2, parser); /* "XX==" */
    }
    return decode_partialb64(ptr, 3, parser); /* "XXX=" */
  }
  return false;
}

static bool handle_nonb64(const char* ptr, const char* end,
                          const upb_fielddef* f, upb_jsonparser* parser) {
  if (nonbase64(ptr[0]) || nonbase64(ptr[1]) || nonbase64(ptr[2]) ||
      nonbase64(ptr[3])) {
    upb_status_seterrf(parser->status,
                       "Non-base64 characters in bytes field: %s",
                       upb_fielddef_name(f));
    return false;
  }

  if (ptr != end - 4 || !decode_padding(ptr, parser)) {
    upb_status_seterrf(parser->status,
                       "Incorrect base64 padding for field: %s (%.*s)",
                       upb_fielddef_name(f), (int)(end - ptr), ptr);
    return false;
  }

  return true;
}

static bool base64_decode(const upb_fielddef* f, upb_jsonparser* parser) {
  upb_strview str = read_str(parser);
  size_t ofs = buf_ofs(&parser->out);
  const char* ptr = str.data;
  const char *end = str.data + str.size;

  /* The output will be only 3/4 the size of the input, but we over-reserve a
   * bit to keep the calculation simple and safe. */
  reserve_bytes(str.size, &parser->out);

  for (; end - ptr >= 4; ptr += 4) {
    int32_t val;
    char* out = parser->out.ptr;

    val = b64tab(ptr[0]) << 18 | b64tab(ptr[1]) << 12 | b64tab(ptr[2]) << 6 |
          b64tab(ptr[3]);

    if (val < 0) {
      CHK(handle_nonb64(ptr, end, f, parser));
      goto done;
    }

    out[0] = val >> 16;
    out[1] = (val >> 8) & 0xff;
    out[2] = val & 0xff;
    parser->out.ptr = out + 3;
  }

  /* Permissively allow non-padded ending. */
  if (end - ptr > 0) {
    CHK(decode_partialb64(ptr, end - ptr, parser));
  }

done:
  insert_varint_len(ofs, &parser->out);
  return true;
}

static const char* read_u64(const char* p, uint64_t* val) {
  uint64_t x = 0;
  while (true) {
    unsigned c = *p - '0';
    if (c >= 10) break;
    p++;
    if (x > ULONG_MAX / 10 || x * 10 > ULONG_MAX - c) {
      return NULL;  /* Overflow. */
    }
    x *= 10;
    x += c;
  }
  *val = x;
  return p;
}

static const char* read_i64(const char* ptr, int64_t* val) {
  bool neg = false;
  uint64_t u64;
  if (*ptr == '-') {
    ptr++;
    neg = true;
  }
  CHK2(ptr = read_u64(ptr, &u64));
  CHK2(u64 <= INT64_MAX + neg);  /* Overflow. */
  *val = neg ? -u64 : u64;
  return ptr;
}

static bool read_sint(const upb_fielddef* f, int64_t limit, int64_t* i64,
                      upb_jsonparser* parser) {
  switch (consume_char2(parser)) {
    case kNumber: {
      double d = read_num(parser);
      *i64 = d;
      if (*i64 != d || *i64 > limit || *i64 < -limit - 1) {
        upb_status_seterrf(
            parser->status,
            "JSON number %f for field %s is out of range or not an integer",
            d, upb_fielddef_jsonname(f));
        return false;
      }
      return true;
    }
    case kString: {
      const char* end;
      const char* start = read_str2(parser, &end);
      const char* ptr = start;
      ptr = read_i64(ptr, i64);
      if (ptr != end) {
        upb_status_seterrf(parser->status,
                           "Malformed number '%.*s' for field %s\n",
                           (int)(end - start), start, upb_fielddef_jsonname(f));
        return false;
      }
      if (*i64 > limit || *i64 < -limit - 1) {
        upb_status_seterrf(parser->status,
                           "Integer out of range for field %s\n",
                           upb_fielddef_jsonname(f));
        return false;
      }
      return true;
    }
    default:
      upb_status_seterrf(parser->status,
                         "Expected number or string for number field %s",
                         upb_fielddef_name(f));
      return false;
  }
  UPB_UNREACHABLE();
}

static bool read_uint(const upb_fielddef* f, uint64_t limit, uint64_t* u64,
                      upb_jsonparser* parser) {
  switch (consume_char2(parser)) {
    case kNumber: {
      double d = read_num(parser);
      *u64 = d;
      if (*u64 != d || *u64 > limit) {
        upb_status_seterrf(
            parser->status,
            "JSON number %f for field %s is out of range or not an integer",
            d, upb_fielddef_jsonname(f));
        return false;
      }
      return true;
    }
    case kString: {
      const char* end;
      const char* start = read_str2(parser, &end);
      const char* ptr = start;
      ptr = read_u64(ptr, u64);
      if (ptr != end) {
        upb_status_seterrf(parser->status,
                           "Malformed number '%.*s' for field %s\n",
                           (int)(end - start), start, upb_fielddef_jsonname(f));
        return false;
      }
      if (*u64 > limit) {
        upb_status_seterrf(parser->status,
                           "Integer out of range for field %s\n",
                           upb_fielddef_jsonname(f));
        return false;
      }
      return true;
    }
    default:
      upb_status_seterrf(parser->status,
                         "Expected number or string for number field %s",
                         upb_fielddef_name(f));
      return false;
  }
  UPB_UNREACHABLE();
}

static void skip_json_value(upb_jsonparser* parser) {
  int depth = 0;

  do {
    switch (consume_char2(parser)) {
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
        read_str(parser);
        break;
      case kNumber:
        read_num(parser);
    }
  } while (depth > 0);
}

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
        char *end;
        errno = 0;
        *d = strtod(str.data, &end);
        CHK(errno == 0);
        CHK(end == str.data + str.size);
      }
      return true;
    }
    default:
      upb_status_seterrf(parser->status,
                         "Expected number or string for number field %s",
                         upb_fielddef_name(f));
      return false;
  }
}

static bool convert_wellknown_listvalue(upb_jsonparser* parser) {
  CHK(parse_char2(kArray, parser));

  while (true) {
    size_t ofs;
    if (try_parse_char2(kEnd, parser)) {
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

  /* map<string, Value> fields = 1; */
  CHK(parse_char2(kString, parser));
  key = read_str(parser);
  CHK(write_string_field(1, key.data, key.size, &parser->out));

  CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 2, &parser->out));
  value_ofs = buf_ofs(&parser->out);
  CHK(convert_wellknown_value(parser));
  CHK(insert_varint_len(value_ofs, &parser->out));

  return true;
}

static bool convert_wellknown_struct(upb_jsonparser* parser) {
  CHK(parse_char2(kObject, parser));

  while (!try_parse_char2(kEnd, parser)) {
    size_t entry_ofs;
    /* map<string, Value> fields = 1; */
    CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 1, &parser->out));
    entry_ofs = buf_ofs(&parser->out);
    CHK(convert_wellknown_struct_entry(parser));
    CHK(insert_varint_len(entry_ofs, &parser->out));
  }
  return true;
}

static bool convert_wellknown_value(upb_jsonparser* parser) {
  switch (consume_char2(parser)) {
    case kNull:
      /* NullValue null_value = 1; */
      CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 1, &parser->out));
      CHK(write_varint(0, &parser->out));
      return true;
    case kNumber: {
      /* double number_value = 2; */
      double d = read_num(parser);
      CHK(write_known_tag(UPB_WIRE_TYPE_64BIT, 2, &parser->out));
      CHK(write_str(&d, 8, &parser->out));
      return true;
    }
    case kString: {
      /* string string_value = 3; */
      upb_strview str = read_str(parser);
      CHK(write_string_field(3, str.data, str.size, &parser->out));
      return true;
    }
    case kTrue:
      /* bool bool_value = 4; */
      CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 4, &parser->out));
      CHK(write_varint(1, &parser->out));
      return true;
    case kFalse:
      CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 4, &parser->out));
      CHK(write_varint(0, &parser->out));
      return true;
    case kObject: {
      /* Struct struct_value = 5; */
      size_t ofs;
      CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 5, &parser->out));
      ofs = buf_ofs(&parser->out);
      parser->ptr--;
      CHK(convert_wellknown_struct(parser));
      CHK(insert_varint_len(ofs, &parser->out));
      return true;
    }
    case kArray: {
      /* ListValue list_value = 6; */
      size_t ofs;
      CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 6, &parser->out));
      ofs = buf_ofs(&parser->out);
      parser->ptr--;
      CHK(convert_wellknown_listvalue(parser));
      CHK(insert_varint_len(ofs, &parser->out));
      return true;
    }
  }

  UPB_UNREACHABLE();
}

static int div_round_up(int a, int b) {
  UPB_ASSERT(a >= 0 && b > 0);
  return (a + (b - 1)) / b;
}

static int epoch_days(int year, int month, int day) {
  static const uint16_t month_yday[12] = {0,   31,  59,  90,  120, 151,
                                          181, 212, 243, 273, 304, 334};
  int febs_since_0 = month > 2 ? year + 1 : year;
  int days_since_0 = 365 * year + month_yday[month - 1] + (day - 1) +
                     div_round_up(febs_since_0, 4) -
                     div_round_up(febs_since_0, 100) +
                     div_round_up(febs_since_0, 400);

  /* Convert from 0-epoch (0001-01-01 BC) to Unix Epoch (1970-01-01 AD).
   * Since the "BC" system does not have a year zero, 1 BC == year zero. */
  return days_since_0 - 719528;
}

static int64_t upb_timegm(const struct tm *tp) {
  int64_t ret = epoch_days(tp->tm_year, tp->tm_mon, tp->tm_mday);
  ret = (ret * 24) + tp->tm_hour;
  ret = (ret * 60) + tp->tm_min;
  ret = (ret * 60) + tp->tm_sec;
  return ret;
}

static bool parse_int_digits(int* num, const char** ptr, int digits) {
  uint64_t u64 = 0;
  const char* end = *ptr + digits;
  UPB_ASSERT(digits <= 9);  /* int can't overflow. */
  CHK2(read_u64(*ptr, &u64) == end);
  *num = u64;
  *ptr = end;
  return true;
}

static const char* convert_nanos(const char* ptr, int32_t* nanos) {
  if (*ptr == '.') {
    const char* begin;
    int digits;
    uint64_t u64;

    begin = ++ptr;
    ptr = read_u64(ptr, &u64);
    if (!ptr) return NULL;
    digits = ptr - begin;
    if (digits == 0 || digits > 9) return NULL;
    digits = 9 - digits;
    while (digits-- > 0) {
      u64 *= 10;
    }
    *nanos = u64;
  }
  return ptr;
}

static bool convert_timestamp(upb_jsonparser* parser) {
  int64_t seconds;
  int32_t nanos = 0;
  const char* ptr;
  const char* end;

  CHK(parse_char2(kString, parser));
  ptr = read_str2(parser, &end);
  CHK(end - ptr >= 20);

  {
    struct tm time;

    /* 1972-01-01T01:00:00 */
    CHK(parse_int_digits(&time.tm_year, &ptr, 4));
    CHK(*ptr++ == '-');
    CHK(parse_int_digits(&time.tm_mon, &ptr, 2));
    CHK(*ptr++ == '-');
    CHK(parse_int_digits(&time.tm_mday, &ptr, 2));
    CHK(*ptr++ == 'T');
    CHK(parse_int_digits(&time.tm_hour, &ptr, 2));
    CHK(*ptr++ == ':');
    CHK(parse_int_digits(&time.tm_min, &ptr, 2));
    CHK(*ptr++ == ':');
    CHK(parse_int_digits(&time.tm_sec, &ptr, 2));

    seconds = upb_timegm(&time);
  }

  CHK(ptr = convert_nanos(ptr, &nanos));

  {
    /* [+-]08:00 or Z */
    int offset = 0;
    bool neg = false;

    CHK(ptr != end);
    switch (*ptr++) {
      case '-':
        neg = true;
        /* Fallthrough. */
      case '+':
        CHK(end - ptr == 5);
        CHK(parse_int_digits(&offset, &ptr, 2));
        CHK(*ptr++ == ':');
        CHK(*ptr++ == '0');
        CHK(*ptr++ == '0');
        offset *= 60 * 60;
        seconds += (neg ? offset : -offset);
        break;
      case 'Z':
        CHK(ptr == end);
        break;
      default:
        return false;
    }
  }

  if (seconds < -62135596800) {
    upb_status_seterrf(parser->status,
                       "error parsing timestamp: "
                       "minimum acceptable value is "
                       "0001-01-01T00:00:00Z");
    return false;
  }

  /* int64 seconds = 1;
   * int32 nanos = 2; */
  write_known_tag(UPB_WIRE_TYPE_VARINT, 1, &parser->out);
  write_varint(seconds, &parser->out);
  write_known_tag(UPB_WIRE_TYPE_VARINT, 2, &parser->out);
  write_varint(nanos, &parser->out);
  return true;
}

static bool convert_duration(upb_jsonparser* parser) {
  int64_t seconds;
  int32_t nanos = 0;
  const char* ptr;
  const char* end;

  /* "3.000000001s", "3s", etc. */
  CHK(parse_char2(kString, parser));
  ptr = read_str2(parser, &end);
  CHK(ptr = read_i64(ptr, &seconds));
  CHK(ptr = convert_nanos(ptr, &nanos));
  CHK(*ptr++ == 's');
  CHK(ptr == end);

  if (seconds < -315576000000LL || seconds > 315576000000LL) {
    upb_status_seterrf(parser->status, "Duration out of range.");
  }

  if (seconds < 0) {
    nanos = -nanos;
  }

  /* int64 seconds = 1;
   * int32 nanos = 2; */
  CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 1, &parser->out));
  CHK(write_varint(seconds, &parser->out));
  CHK(write_known_tag(UPB_WIRE_TYPE_VARINT, 2, &parser->out));
  CHK(write_varint((int64_t)nanos, &parser->out));
  return true;
}

static bool convert_fieldmask_field(const char* ptr, const char* end,
                                    upb_jsonparser* parser) {
  size_t ofs;
  CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 1, &parser->out));
  ofs = buf_ofs(&parser->out);

  /* fooBarBaz -> foo_bar_baz */
  while (ptr < end) {
    if (*ptr >= 'A' && *ptr <= 'Z') {
      CHK(write_char('_', &parser->out));
      CHK(write_char(*ptr + 32, &parser->out));
    } else {
      CHK(write_char(*ptr, &parser->out));
    }
    ptr++;
  }

  return insert_varint_len(ofs, &parser->out);
}

static bool convert_fieldmask(upb_jsonparser* parser) {
  const char* start;
  const char* end;
  const char* ptr;

  CHK(parse_char2(kString, parser));
  ptr = read_str2(parser, &end);
  start = ptr;

  if (start == end) {
    return true;
  }

  /* repeated string paths = 1; */
  while (ptr < end) {
    if (*ptr == ',') {
      CHK(convert_fieldmask_field(start, ptr, parser));
      start = ptr + 1;
    }
    ptr++;
  }
  CHK(convert_fieldmask_field(start, ptr, parser));

  return true;
}

static bool convert_any_field(const upb_msgdef* m, upb_jsonparser* parser) {
  if (upb_msgdef_wellknowntype(m) == UPB_WELLKNOWN_UNSPECIFIED) {
    /* For regular types: {"@type": "[user type]", "f1": <V1>, "f2": <V2>}
     * where f1, f2, etc. are the normal fields of this type. */
    return convert_json_field(m, parser);
  } else {
    /* For well-known types: {"@type": "[well-known type]", "value": <X>}
     * where <X> is whatever encoding the WKT normally uses. */
    upb_strview str;
    CHK(parse_char2(kString, parser));
    str = read_str(parser);
    CHK(upb_strview_eql(str, upb_strview_makez("value")));
    return convert_wellknown(m, parser);
  }
}

static const upb_msgdef* convert_any_typeurl(upb_jsonparser* parser) {
  const char* end;
  const char* ptr;

  CHK(parse_char2(kString, parser));
  ptr = read_str2(parser, &end);
  CHK(write_string_field(1, ptr, end - ptr, &parser->out));

  /* type.googleapis.com/google.protobuf.Duration (we just strip to '/') */
  while (ptr < end && *ptr != '/') {
    ptr++;
  }
  CHK(end - ptr > 2);
  ptr++;

  return upb_symtab_lookupmsg2(parser->any_msgs, ptr, end - ptr);
}

static bool convert_any(upb_jsonparser* parser) {
  const upb_msgdef *m;
  size_t ofs;
  const char* start;
  const char* type;
  const char* after_type;

  CHK(parse_char2(kObject, parser));
  start = parser->ptr;

  /* string type_url = 1;
   * bytes value = 2; */

  /* Scan looking for the message type, which is not necessarily first. */
  while (true) {
    upb_strview str;
    type = parser->ptr;
    CHK(parse_char2(kString, parser));
    str = read_str(parser);
    if (upb_strview_eql(str, upb_strview_makez("@type"))) {
      m = convert_any_typeurl(parser);
      break;
    } else {
      skip_json_value(parser);
    }
  }

  CHK(write_known_tag(UPB_WIRE_TYPE_DELIMITED, 2, &parser->out));
  ofs = buf_ofs(&parser->out);

  /* Pick up fields before "@type" */
  after_type = parser->ptr;
  parser->ptr = start;
  while (parser->ptr < type) {
    CHK(convert_any_field(m, parser));
  }

  /* Parse fields after "@type" */
  parser->ptr = after_type;
  while (!try_parse_char2(kEnd, parser)) {
    CHK(convert_any_field(m, parser));
  }

  CHK(insert_varint_len(ofs, &parser->out));

  return true;
}

static bool convert_wellknown(const upb_msgdef* m, upb_jsonparser* parser) {
  switch (upb_msgdef_wellknowntype(m)) {
    case UPB_WELLKNOWN_STRINGVALUE:
    case UPB_WELLKNOWN_BYTESVALUE:
    case UPB_WELLKNOWN_DOUBLEVALUE:
    case UPB_WELLKNOWN_FLOATVALUE:
    case UPB_WELLKNOWN_INT64VALUE:
    case UPB_WELLKNOWN_UINT64VALUE:
    case UPB_WELLKNOWN_UINT32VALUE:
    case UPB_WELLKNOWN_INT32VALUE:
    case UPB_WELLKNOWN_BOOLVALUE:
      return convert_json_value(upb_msgdef_itof(m, 1), parser);
    case UPB_WELLKNOWN_FIELDMASK:
      return convert_fieldmask(parser);
    case UPB_WELLKNOWN_DURATION:
      return convert_duration(parser);
    case UPB_WELLKNOWN_TIMESTAMP:
      return convert_timestamp(parser);
    case UPB_WELLKNOWN_ANY:
      return convert_any(parser);
    case UPB_WELLKNOWN_VALUE:
      return convert_wellknown_value(parser);
    case UPB_WELLKNOWN_LISTVALUE:
      return convert_wellknown_listvalue(parser);
    case UPB_WELLKNOWN_STRUCT:
      return convert_wellknown_struct(parser);
    default:
      UPB_UNREACHABLE();
  }
}

static bool is_map_key(const upb_fielddef* f) {
  return upb_fielddef_number(f) == UPB_MAPENTRY_KEY &&
         upb_msgdef_mapentry(upb_fielddef_containingtype(f));
}

static bool convert_json_array(const upb_fielddef* f, upb_jsonparser* parser) {
  CHK(parse_char2(kArray, parser));
  while (!try_parse_char2(kEnd, parser)) {
    CHK(convert_json_value(f, parser));
  }
  return true;
}

static bool convert_json_map(const upb_fielddef* f, upb_jsonparser* parser) {
  const upb_msgdef* entry = upb_fielddef_msgsubdef(f);
  const upb_fielddef* key = upb_msgdef_itof(entry, UPB_MAPENTRY_KEY);
  const upb_fielddef* value = upb_msgdef_itof(entry, UPB_MAPENTRY_VALUE);

  CHK(parse_char2(kObject, parser));
  while (!try_parse_char2(kEnd, parser)) {
    size_t ofs;
    CHK(write_tag(f, parser));
    ofs = buf_ofs(&parser->out);
    CHK(convert_json_value(key, parser));
    CHK(convert_json_value(value, parser));
    CHK(insert_varint_len(ofs, &parser->out));
  }
  return true;
}

/* google.protobuf.Value is the only type that emits output for "null" */
static bool is_value(const upb_fielddef* f) {
  return upb_fielddef_issubmsg(f) &&
         upb_msgdef_wellknowntype(upb_fielddef_msgsubdef(f)) ==
             UPB_WELLKNOWN_VALUE;
}

static bool convert_json_value(const upb_fielddef* f, upb_jsonparser* parser) {
  CHK(write_tag(f, parser));
  switch (upb_fielddef_type(f)) {
    case UPB_TYPE_BOOL:
      if (is_map_key(f)) {
        upb_strview str;
        CHK(parse_char2(kString, parser));
        str = read_str(parser);
        if (upb_strview_eql(str, upb_strview_makez("false"))) {
          return write_varint(0, &parser->out);
        } else if (upb_strview_eql(str, upb_strview_makez("true"))) {
          return write_varint(1, &parser->out);
        } else {
          upb_status_seterrf(parser->status, "Invalid key for bool map: %.*s",
                             (int)str.size, str.data);
          return false;
        }
      } else {
        switch (consume_char2(parser)) {
          case kFalse:
            CHK(write_varint(0, &parser->out));
            return true;
          case kTrue:
            CHK(write_varint(1, &parser->out));
            return true;
          default:
            /* Should we accept 0/nonzero as true/false? */
            upb_status_seterrf(parser->status,
                               "Invalid value for bool field: %s",
                               upb_fielddef_name(f));
            return false;
        }
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
    }
    case UPB_TYPE_UINT32: {
      uint64_t u64;
      CHK(read_uint(f, UINT32_MAX, &u64, parser));
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_FIXED32: {
          uint32_t u32 = u64;
          return write_str(&u32, 4, &parser->out);
        }
        case UPB_DESCRIPTOR_TYPE_UINT32:
          return write_varint(u64, &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_UINT64: {
      uint64_t u64;
      CHK(read_uint(f, UINT64_MAX, &u64, parser));
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_FIXED64:
          return write_str(&u64, 8, &parser->out);
        case UPB_DESCRIPTOR_TYPE_UINT64:
          return write_varint(u64, &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_INT32: {
      int64_t i64;
    int32_val:
      CHK(read_sint(f, INT32_MAX, &i64, parser));
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_SFIXED32: {
          int32_t i32 = i64;
          return write_str(&i32, 4, &parser->out);
        }
        case UPB_DESCRIPTOR_TYPE_INT32:
        case UPB_DESCRIPTOR_TYPE_ENUM:
          return write_varint(i64, &parser->out);
        case UPB_DESCRIPTOR_TYPE_SINT32:
          return write_varint(zzencode_32(i64), &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_INT64: {
      int64_t i64;
      CHK(read_sint(f, INT64_MAX, &i64, parser));
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_SFIXED64:
          return write_str(&i64, 8, &parser->out);
        case UPB_DESCRIPTOR_TYPE_INT64:
          return write_varint(i64, &parser->out);
        case UPB_DESCRIPTOR_TYPE_SINT64:
          return write_varint(zzencode_64(i64), &parser->out);
        default:
          UPB_UNREACHABLE();
      }
    }
    case UPB_TYPE_STRING: {
      upb_strview str;
      CHK(parse_char2(kString, parser));
      str = read_str(parser);
      CHK(write_varint(str.size, &parser->out));
      CHK(write_str(str.data, str.size, &parser->out));
      return true;
    }
    case UPB_TYPE_BYTES:
      CHK(parse_char2(kString, parser));
      return base64_decode(f, parser);
    case UPB_TYPE_ENUM:
      if (try_parse_char2(kString, parser)) {
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
      if (upb_msgdef_wellknowntype(m) == UPB_WELLKNOWN_UNSPECIFIED) {
        CHK(convert_json_object(m, parser));
        if (upb_fielddef_descriptortype(f) == UPB_DESCRIPTOR_TYPE_GROUP) {
          return write_known_tag(UPB_WIRE_TYPE_END_GROUP,
                                 upb_fielddef_number(f), &parser->out);
        }
      } else {
        CHK(convert_wellknown(m, parser));
      }
      return insert_varint_len(ofs, &parser->out);
    }
  }

  UPB_UNREACHABLE();
}

static bool convert_json_field(const upb_msgdef* m, upb_jsonparser* parser) {
  upb_strview name;
  const upb_fielddef* f;

  CHK(parse_char2(kString, parser));
  name = read_str(parser);
  f = upb_msgdef_lookupjsonname(m, name.data, name.size);

  if (!f) {
    if (parser->options & UPB_JSON_IGNORE_UNKNOWN) {
      skip_json_value(parser);
      return true;
    } else {
      upb_status_seterrf(parser->status,
                         "Unknown field '" UPB_STRVIEW_FORMAT
                         "' when parsing message %s",
                         UPB_STRVIEW_ARGS(name), upb_msgdef_fullname(m));
      return false;
    }
  }

  if (!is_value(f) && try_parse_char2(kNull, parser)) {
    /* JSON "null" indicates a default value, so no need to encode anything. */
    return true;
  }

  if (upb_fielddef_ismap(f)) {
    return convert_json_map(f, parser);
  } else if (upb_fielddef_isseq(f)) {
    return convert_json_array(f, parser);
  } else {
    return convert_json_value(f, parser);
  }
}

static bool convert_json_object(const upb_msgdef* m, upb_jsonparser* parser) {
  CHK(parse_char2(kObject, parser));
  while (!try_parse_char2(kEnd, parser)) {
    CHK(convert_json_field(m, parser));
  }
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
    /* NULL-terminate so we can safely use strtod(), etc. in stage 2. */
    write_char(0, &parser.out);
    *outlen = parser.out.ptr - parser.out.buf - 1;  /* NULL */
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
    /* NULL-terminate for the user's convenience and safety. */
    write_char(0, &parser.out);
    *outlen = parser.out.ptr - parser.out.buf - 1;  /* NULL */
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
