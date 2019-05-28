
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

static bool put_varint(uint64_t val, outbuf* out) {
  CHK(reserve_bytes(10, out));
  out->ptr += encode_varint(val, out->ptr);
  return true;
}

static size_t buf_ofs(outbuf* out) { return out->ptr - out->buf; }

static bool insert_fixed_len(size_t ofs, outbuf* out) {
  size_t len = buf_ofs(out) - ofs;
  int intlen = len;

  CHK(len <= INT_MAX);
  CHK(reserve_bytes(4, out));
  memmove(out->ptr + 4, out->ptr, len);
  memcpy(out->ptr, &intlen, 4);
  return true;
}

static bool insert_varint_len(size_t ofs, outbuf* out) {
  size_t len = buf_ofs(out) - ofs;
  char varint[10];
  size_t varint_len = encode_varint(len, varint);

  CHK(len <= INT_MAX);
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
  /* 0 is reserved for errors. */
  kEnd = 1,
  kObject,
  kArray,
  kNumber,
  kString,
  kTrue,
  kFalse,
  kNull,
};

static bool parse_json_object(jsonparser* state);
static bool parse_json_array(jsonparser* state);

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

static bool is_eof(jsonparser* state) {
  return (state->ptr == state->end);
}

static bool skip_whitespace(jsonparser* state) {
  while (!is_eof(state) && is_whitespace(*state->ptr)) {
    state->ptr++;
  }
  return !is_eof(state);
}

static bool has_n_bytes(size_t n, jsonparser* state) {
  return state->end - state->ptr >= n;
}

static bool parse_char(char ch, jsonparser* state) {
  CHK(skip_whitespace(state));
  CHK(*state->ptr == ch);
  state->ptr++;
  return true;
}

static UPB_FORCEINLINE bool parse_lit(const char* lit, jsonparser* state) {
  size_t len = strlen(lit);
  CHK(has_n_bytes(len, state));
  CHK(memcmp(state->ptr, lit, len) == 0);
  state->ptr += len;
  return true;
}

/* NULL is not allowed in JSON text, so we use 0 as failure. */
static char peek_char_skipws(jsonparser* state) {
  CHK(skip_whitespace(state));
  return *state->ptr;
}

static char peek_char(jsonparser* state) {
  CHK(!is_eof(state));
  return *state->ptr;
}

static char consume_char_skipws(jsonparser* state) {
  char ch = peek_char_skipws(state);
  CHK(ch);
  state->ptr++;
  return ch;
}

static char consume_char(jsonparser* state) {
  char ch = peek_char(state);
  CHK(ch);
  state->ptr++;
  return ch;
}

static bool skip_digits(jsonparser* state) {
  const char* start = state->ptr;

  while (true) {
    char ch = peek_char(state);
    CHK(ch);
    if (ch < '0' || ch > '9') {
      break;
    }
    state->ptr++;
  }

  /* We must consume at least one digit. */
  return state->ptr != start;
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

static bool parse_escape(jsonparser* state) {
  CHK(parse_char('\\', state));

  switch (consume_char(state)) {
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
      CHK(has_n_bytes(4, state));
      CHK(parse_hex_digit(state->ptr[0], &codepoint));
      CHK(parse_hex_digit(state->ptr[1], &codepoint));
      CHK(parse_hex_digit(state->ptr[2], &codepoint));
      CHK(parse_hex_digit(state->ptr[3], &codepoint));
      CHK(write_utf8_codepoint(codepoint, state));
      state->ptr += 4;
      break;
    }
    default:
      return false;
  }

  return true;
}

static bool parse_json_string(jsonparser* state) {
  const char* span_start;
  size_t ofs;

  CHK(parse_char('"', state));
  CHK(write_char(kString, &state->out));
  span_start = state->ptr;
  ofs = buf_ofs(&state->out);

  while (true) {
    /* TODO: validate UTF-8. */
    switch (peek_char(state)) {
      case '"':
        goto done;
      case '\\':
        CHK(write_str(span_start, state->ptr - span_start, &state->out));
        CHK(parse_escape(state));
        span_start = state->ptr;
        break;
      case 0:
        return false;
      default:
        CHK((unsigned char)*state->ptr >= 0x20);
        state->ptr++;
        break;
    }
  }

done:
  CHK(write_str(span_start, state->ptr - span_start, &state->out));
  state->ptr++;
  CHK(insert_fixed_len(ofs, &state->out));

  return true;
}

static bool parse_json_number(jsonparser* state) {
  const char* start = state->ptr;
  char ch;
  char* end;
  double d;

  CHK(ch = consume_char(state));

  if (ch == '-') {
    CHK(ch = consume_char(state));
  }

  if (ch != '0') {
    CHK(skip_digits(state));
  }

  if (is_eof(state)) return true;

  if (*state->ptr == '.') {
    state->ptr++;
    CHK(skip_digits(state));
  }

  if (is_eof(state)) return true;

  if (*state->ptr == 'e' || *state->ptr == 'E') {
    state->ptr++;
    CHK(!is_eof(state));

    if (*state->ptr == '+' || *state->ptr == '-') {
      state->ptr++;
    }

    CHK(skip_digits(state));
  }

  errno = 0;
  d = strtod(start, &end);

  CHK(errno == 0 && end == state->ptr);
  CHK(write_char(kNumber, &state->out));
  CHK(write_str(&d, sizeof(d), &state->out));

  return true;
}

static bool parse_json_value(jsonparser* state) {
  CHK(--state->depth != 0);

  switch (peek_char_skipws(state)) {
    case '{':
      CHK(parse_json_object(state));
      break;
    case '[':
      CHK(parse_json_array(state));
      break;
    case '"':
      CHK(parse_json_string(state));
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
      CHK(parse_json_number(state));
      break;
    case 't':
      CHK(parse_lit("true", state));
      CHK(write_char(kTrue, &state->out));
    case 'f':
      CHK(parse_lit("false", state));
      CHK(write_char(kFalse, &state->out));
      break;
    case 'n':
      CHK(parse_lit("null", state));
      CHK(write_char(kNull, &state->out));
      break;
    default:
      return false;
  }

  state->depth++;
  return true;
}


static bool parse_json_array(jsonparser* state) {
  CHK(parse_char('[', state));
  CHK(write_char(kArray, &state->out));

  if (peek_char_skipws(state) == ']') {
    state->ptr++;
    CHK(write_char(kEnd, &state->out));
    return true;
  }

  while (true) {
    CHK(parse_json_value(state));
    switch (consume_char_skipws(state)) {
      case ',':
        break;
      case ']':
        CHK(write_char(kEnd, &state->out));
        return true;
      default:
        return false;
    }
  }

  UPB_UNREACHABLE();
}

static bool parse_json_object(jsonparser* state) {
  CHK(parse_char('{', state));
  CHK(write_char(kObject, &state->out));

  if (peek_char_skipws(state) == '}') {
    state->ptr++;
    CHK(write_char(kEnd, &state->out));
    return true;
  }

  while (true) {
    CHK(parse_json_string(state));
    CHK(parse_char(':', state));
    CHK(parse_json_value(state));
    switch (consume_char_skipws(state)) {
      case ',':
        break;
      case '}':
        CHK(write_char(kEnd, &state->out));
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
  const upb_msgdef *m;
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

static bool base64_decode(const upb_fielddef* f, upb_jsonparser* state) {
  upb_strview str = read_str(state);
  size_t ofs = buf_ofs(&state->out);
  const char* in = str.data;
  const char *limit = str.data + str.size;
  /* This is a conservative estimate, assuming no padding. */
  char* out = reserve_bytes(str.size / 4 * 3, &state->out);

  if ((str.size % 4) != 0) {
    upb_status_seterrf(state->status,
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
  insert_varint_len(ofs, &state->out);
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

#define READ_INT_BODY                                          \
  switch (consume_char2(state)) {                              \
    case kNumber:                                              \
      val = read_num(state);                                   \
      return true;                                             \
    case kString: {                                            \
      upb_strview str = read_str(state);                       \
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
      return true;                                             \
    }                                                          \
    default:                                                   \
      return false;                                            \
  }                                                            \
  UPB_UNREACHABLE();

static bool read_int64(upb_jsonparser* state) {
  int64_t val = 0;
  READ_INT_BODY
}

static bool read_uint64(upb_jsonparser* state) {
  uint64_t val = 0;
  READ_INT_BODY
}

#undef CONVERT_INT_BODY

static bool convert_bool(const upb_fielddef* f, upb_jsonparser* state) {
  switch (consume_char2(state)) {
    case kFalse:
      CHK(put_varint(0, &state->out));
      return true;
    case kTrue:
      CHK(put_varint(1, &state->out));
      return true;
    default:
      /* Should we accept 0/nonzero as true/false? */
      return false;
  }
}

static bool read_double(const upb_fielddef* f, double* d,
                        upb_jsonparser* state) {
  switch (consume_char2(state)) {
    case kNumber:
      *d = read_num(state);
      return true;
    case kString: {
      upb_strview str = read_str(state);
      if (upb_strview_eql(str, upb_strview_makez("NaN"))) {
        *d = NAN;
      } else if (upb_strview_eql(str, upb_strview_makez("Infinity"))) {
        *d = INFINITY;
      } else if (upb_strview_eql(str, upb_strview_makez("-Infinity"))) {
        *d = -INFINITY;
      } else {
        return false;
      }
      return true;
    }
    default:
      return false;
  }
}

static bool convert_double(const upb_fielddef* f, upb_jsonparser* parser) {
  double d;
  CHK(read_double(f, &d, parser));
  CHK(write_str(&d, 8, &parser->out));
  return true;
}

static bool convert_float(const upb_fielddef* f, upb_jsonparser* parser) {
  double d;
  float flt;
  CHK(read_double(f, &d, parser));
  flt = d;
  CHK(write_str(&f, 4, &parser->out));
  return true;
}

static bool convert_string(const upb_fielddef* f, upb_jsonparser* parser) {
  upb_strview str;
  CHK(consume_char2(parser) == kString);
  str = read_str(parser);
  CHK(put_varint(str.size, &parser->out));
  CHK(write_str(str.data, str.size, &parser->out));
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
      return convert_bool(f, parser);
    case UPB_TYPE_FLOAT:
      return convert_float(f, parser);
    case UPB_TYPE_DOUBLE:
      return convert_double(f, parser);
    case UPB_TYPE_UINT32:
      return convert_int32(f, parser);
    case UPB_TYPE_INT32:
      return convert_int32(f, parser);
    case UPB_TYPE_INT64:
    case UPB_TYPE_UINT64:
      break;
    case UPB_TYPE_STRING:
      return convert_string(f, parser);
    case UPB_TYPE_BYTES:
      return base64_decode(f, parser);
    case UPB_TYPE_ENUM:
      if (peek_char2(parser) == kString) {
        upb_strview str = read_str(parser);
        const upb_enumdef* e = upb_fielddef_enumsubdef(f);
        int32_t num;
        CHK(upb_enumdef_ntoi(e, str.data, str.size, &num));
        CHK(put_varint(num, &parser->out));
        return true;
      }
      /* Fallthrough. */
    case UPB_TYPE_MESSAGE: {
      const upb_msgdef* m = upb_fielddef_msgsubdef(f);
      size_t ofs = buf_ofs(&parser->out);
      switch (upb_msgdef_wellknowntype(m)) {
        case UPB_WELLKNOWN_UNSPECIFIED:
          return convert_json_object(m, parser);
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
          CHK(insert_varint_len(ofs, &parser->out));
          return true;
        case UPB_WELLKNOWN_FIELDMASK:
          CHK(parse_fieldmask(ofs, parser));
          break;
        case UPB_WELLKNOWN_DURATION:
          CHK(parse_duration(ofs, parser));
          break;
        case UPB_WELLKNOWN_TIMESTAMP:
          CHK(parse_timestamp(ofs, parser));
          break;
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
    if (peek_char(parser) == kEnd) {
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
    if (peek_char(parser) == kEnd) {
      parser->ptr++;
      break;
    }

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

static char* parse_json_stage1(const char* buf, size_t len, int max_depth,
                               upb_alloc* alloc, size_t* outlen,
                               upb_status* s) {
  jsonparser parser = {buf, buf + len, {alloc}, max_depth, s};
  if (parse_json_object(&parser) && !skip_whitespace(&parser)) {
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
  upb_jsonparser parser = {buf, buf + len, {alloc}, m, any_msgs, s, options};
  bool ok;

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

  CHK(is_proto3(m));
  CHK(stage1 = parse_json_stage1(buf, len, max_depth, alloc, &stage1_len, s));

  stage2 = parse_json_stage2(stage1, stage1_len, m, any_msgs, options, alloc,
                             outlen, s);
  upb_free(alloc, stage1);

  return stage2;
}
