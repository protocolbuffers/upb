
#include "upb/json.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "upb/upb.h"

#include "upb/port_def.inc"

#define CHK(x) if (UPB_UNLIKELY(!(x))) return 0

/* Output Buffer **************************************************************/

typedef struct {
  char* ptr;
  char* buf;
  char* end;
  upb_alloc* alloc;
} outbuf;

UPB_NOINLINE static bool realloc_buf(size_t bytes, outbuf* state) {
  size_t old = state->outend - state->outbuf;
  size_t ptr = state->outptr - state->outbuf;
  size_t need = ptr + bytes;
  size_t n = UPB_MAX(old, 128);
  static const size_t max = LONG_MIN;

  CHK(need > ptr);  /* ptr + bytes didn't overflow. */
  CHK(need < max);  /* we can exceed by doubling n. */

  while (n < need) {
    n *= 2;
  }

  state->outbuf = upb_realloc(state->alloc, state->outbuf, old, n);
  CHK(state->outbuf);

  state->outptr = state->outbuf + ptr;
  state->outend = state->outbuf + n;
  return true;
}

/* Generic JSON parser ********************************************************/

typedef struct {
  const char* end;
  outbuf out;
  int depth;
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

const char* parse_json_object(const char* ptr, jsonparser* state);
static const char* parse_json_array(const char* ptr, jsonparser* state);

static bool reserve_bytes(size_t bytes, jsonparser* state) {
  size_t have = state->outend - state->outptr;
  return (have >= bytes) ? true : realloc_buf(bytes, state);
}

static bool write_str(const void* str, size_t n, jsonparser* state) {
  CHK(reserve_bytes(n, state));
  memcpy(state->outptr, str, n);
  state->outptr += n;
  return true;
}

static bool write_char(char ch, jsonparser* state) {
  CHK(reserve_bytes(1, state));
  *state->outptr = ch;
  state->outptr++;
  return true;
}

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
    return write_char(cp, state);
  } else if (cp <= 0x07FF) {
    utf8[0] = ((cp >> 6) & 0x1F) | 0xC0;
    utf8[1] = ((cp >> 0) & 0x3F) | 0x80;
    return write_str(utf8, 2, state);
  } else /* cp <= 0xFFFF */ {
    utf8[0] = ((cp >> 12) & 0x0F) | 0xE0;
    utf8[1] = ((cp >> 6) & 0x3F) | 0x80;
    utf8[2] = ((cp >> 0) & 0x3F) | 0x80;
    return write_str(utf8, 3, state);
  }

  /* TODO(haberman): Handle high surrogates: if codepoint is a high surrogate
   * we have to wait for the next escape to get the full code point). */
}

static const char* parse_escape(const char* ptr, jsonparser* state) {
  char ch;

  CHK(ptr = consume_char_nows(ptr, state, &ch));

  switch (ch) {
    case '"':
      CHK(write_char('"', state));
      break;
    case '\\':
      CHK(write_char('\\', state));
      break;
    case '/':
      CHK(write_char('/', state));
      break;
    case 'b':
      CHK(write_char('\b', state));
      break;
    case 'n':
      CHK(write_char('\n', state));
      break;
    case 'r':
      CHK(write_char('\r', state));
      break;
    case 't':
      CHK(write_char('\t', state));
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
  bool aliased = true;
  const char* span_start = ptr;

  while (true) {
    char ch;

    CHK(ptr = consume_char_nows(ptr, state, &ch));

    /* TODO: validate UTF-8. */

    switch (ch) {
      case '"':
        goto done;
      case '\\':
        if (aliased) {
          CHK(write_char(kString, state));
          aliased = false;
        }
        CHK(write_str(span_start, ptr - span_start, state));
        CHK(ptr = parse_escape(ptr, state));
        span_start = ptr;
        break;
      default:
        CHK((unsigned char)ch >= 0x20);
        break;
    }
  }

done:
  if (aliased) {
    CHK(write_char(kAliasedString, state));
  } else {
    CHK(write_str(span_start, ptr - span_start, state));
    /* TODO: allow \u0000 which would conflict with this. */
    CHK(write_char(kEnd, state));
  }

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
  CHK(write_char(kNumber, state));
  CHK(write_str(&d, sizeof(d), state));

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
      CHK(write_char(kTrue, state));
    case 'f':
      CHK(ptr = parse_lit(ptr, "false", state));
      CHK(write_char(kFalse, state));
      break;
    case 'n':
      CHK(ptr = parse_lit(ptr, "null", state));
      CHK(write_char(kNull, state));
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
  CHK(write_char(kArray, state));
  CHK(peek_char(ptr, state, &ch));

  if (*ptr == ']') {
    CHK(write_char(kEnd, state));
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
        CHK(write_char(kEnd, state));
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
  CHK(write_char(kObject, state));
  CHK(peek_char(ptr, state, &ch));

  if (*ptr == '}') {
    CHK(write_char(kEnd, state));
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
        CHK(write_char(kEnd, state));
        return ptr;
      default:
        return NULL;
    }
  }

  UPB_UNREACHABLE();
}

/* Schema-aware JSON -> Protobuf translation **********************************/

/* In this stage we have a single buffer we are both reading and writing from.
 * If our write head (outptr) runs into our read head (ptr), we have to resize.
 *
 *  outbuf     outptr->     ptr->   outend
 *    |           |          |        |
 *    V           V          V        V
 *    |-------------------------------|
 *
 * We also have aliasbuf, which is the original input buffer.  We use it to read
 * string data that was aliased by the first parse.
 */

struct upb_jsonparser {
  char* outptr;
  char* outbuf;
  char* outend;
  const char* aliasbuf;
  upb_alloc* alloc;
  const upb_msgdef *m;
  const upb_symtab* any_msgs;
  upb_status *status;
};

static bool is_proto3(const upb_msgdef* m) {
  return upb_filedef_syntax(upb_msgdef_file(m)) == UPB_SYNTAX_PROTO3;
}

static char* reserve_bytes2(size_t bytes, char* ptr, upb_jsonparser* parser) {
  size_t have = ptr - state->outptr;
  if (have < bytes) {
    CHK(realloc_buf(bytes, &parser->out));

  return (have >= bytes) ? true : realloc_buf(bytes, state);
}

static bool write_tag(const upb_fielddef *f, upb_jsonparser* state) {
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

static bool base64_decode(const char* in, size_t len, const upb_fielddef* f,
                          upb_jsonparser* state) {
  /* Warning: in and out may alias each other.  This works because we consume
   * "in" faster than "out". */
  const char* limit = in + len;
  char* out = state->outbuf;

  reserve_bytes2(len / 4 * 3, state);
  out = state->outbuf;

  /* No need to reserve, since out_size <= in_size. */
  if ((len % 4) != 0) {
    upb_status_seterrf(state->status,
                       "Base64 input for bytes field not a multiple of 4: %s",
                       upb_fielddef_name(f));
  }

  for (; in < limit; in += 4, out += 3) {
    uint32_t val;

    val = b64tab(in[0]) << 18 | b64tab(in[1]) << 12 | b64tab(in[2]) << 6 |
          b64tab(in[3]);

    /* Returns true if any of the characters returned -1. */
    if (val & 0x80000000) {
      goto otherchar;
    }

    out[0] = val >> 16;
    out[1] = (val >> 8) & 0xff;
    out[2] = val & 0xff;
  }

finish:
  state->out.ptr = out;
  return true;

otherchar:
  if (nonbase64(in[0]) || nonbase64(in[1]) || nonbase64(in[2]) ||
      nonbase64(in[3])) {
    upb_status_seterrf(state->status,
                       "Non-base64 characters in bytes field: %s",
                       upb_fielddef_name(f));
    return false;
  }
  if (in[2] == '=') {
    uint32_t val;

    /* Last group contains only two input bytes, one output byte. */
    if (in[0] == '=' || in[1] == '=' || in[3] != '=') {
      goto badpadding;
    }

    val = b64tab(in[0]) << 18 | b64tab(in[1]) << 12;
    UPB_ASSERT(!(val & 0x80000000));

    out[0] = val >> 16;
    out += 1;
    goto finish;
  } else {
    uint32_t val;

    /* Last group contains only three input bytes, two output bytes. */
    if (in[0] == '=' || in[1] == '=' || in[2] == '=') {
      goto badpadding;
    }

    val = b64tab(in[0]) << 18 | b64tab(in[1]) << 12 | b64tab(in[2]) << 6;

    out[0] = val >> 16;
    out[1] = (val >> 8) & 0xff;
    out += 2;
    goto finish;
  }

badpadding:
  upb_status_seterrf(state->status,
                     "Incorrect base64 padding for field: %s (%.*s)",
                     upb_fielddef_name(f),
                     4, in);
  return false;
}

#if 0

static bool convert_float(size_t ofs, upb_jsonparser* state) {
  float f = strtof(outptr(ofs, state), NULL);
  pop_output(ofs, state);
  return write_str(&f, sizeof(f));
}

static bool convert_double(size_t ofs, upb_jsonparser* state) {
  double d = strtod(outptr(ofs, state), NULL);
  pop_output(ofs, state);
  return write_str(&d, sizeof(d));
}

static bool convert_enum(size_t ofs, const upb_enumdef *e, upb_jsonparser* state) {
  int32_t num;
  const char* str = outptr(ofs, state);
  size_t size = state->outptr - str;
  CHK(upb_enumdef_ntoi(e, str, size, &num));
  pop_output(ofs, state);
  CHK(write_varint(num, state));
  return true;
}

#define CONVERT_INT_BODY                                   \
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
  pop_output(ofs, state);                                  \
  CHK(write_str(&val, sizeof(val)));                       \
  return true;

static bool convert_sfixed32(size_t ofs, upb_jsonparser* state) {
  int32_t val = 0;
  CONVERT_INT_BODY
}

static bool convert_sfixed64(size_t ofs, upb_jsonparser* state) {
  int64_t val = 0;
  CONVERT_INT_BODY
}

static bool convert_fixed32(size_t ofs, upb_jsonparser* state) {
  uint32_t val = 0;
  CONVERT_INT_BODY
}

static bool convert_fixed64(size_t ofs, upb_jsonparser* state) {
  uint64_t val = 0;
  CONVERT_INT_BODY
}

#undef CONVERT_INT_BODY

static bool convert_int32(size_t ofs, upb_jsonparser* state) {

}

static const char* parse_str(const char* buf, const upb_fielddef* f,
                             upb_jsonparser* state) {
  size_t ofs;
  size_t size;

  CHK(write_tag(f, state));
  CHK(buf = parse_raw_str(buf, &ofs, state));

  switch (upb_fielddef_descriptortype(f)) {
    case UPB_DESCRIPTOR_TYPE_STRING:
      CHK(insert_length(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_BYTES:
      CHK(base64_decode(ofs, f, state));
      CHK(insert_length(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_FLOAT:
      CHK(convert_float(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_DOUBLE:
      CHK(convert_double(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_ENUM:
      CHK(convert_enum(ofs, upb_fielddef_enumsubdef(f), state));
      break;
    case UPB_DESCRIPTOR_TYPE_INT32:
      CHK(convert_int32(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_UINT32:
      CHK(convert_uint32(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_INT64:
      CHK(convert_int64(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_UINT64:
      CHK(convert_uint64(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_SINT32:
      CHK(convert_sint32(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_SINT64:
      CHK(convert_sint64(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_FIXED32:
      CHK(convert_fixed32(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_FIXED64:
      CHK(convert_fixed64(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_SFIXED32:
      CHK(convert_sfixed32(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_SFIXED64:
      CHK(convert_sfixed64(ofs, state));
      break;
    case UPB_DESCRIPTOR_TYPE_BOOL:
      /* Should we accept 0/nonzero as true/false? */
      return NULL;
    case UPB_DESCRIPTOR_TYPE_GROUP:
      return NULL;
    case UPB_DESCRIPTOR_TYPE_MESSAGE: {
      const upb_msgdef* m = upb_fielddef_msgsubdef(f);
      switch (upb_msgdef_wellknowntype(m)) {
        case UPB_WELLKNOWN_STRINGVALUE:
          CHK(insert_length(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_DELIMITED, state));
          break;
        case UPB_WELLKNOWN_BYTESVALUE:
          CHK(base64_decode(ofs, f, state));
          CHK(insert_length(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_DELIMITED, state));
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
        case UPB_WELLKNOWN_DOUBLEVALUE:
          CHK(convert_double(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_64BIT, state));
          break;
        case UPB_WELLKNOWN_FLOATVALUE:
          CHK(convert_float(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_32BIT, state));
          break;
        case UPB_WELLKNOWN_INT64VALUE:
          CHK(convert_int64(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_UINT64VALUE:
          CHK(convert_uint64(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_UINT32VALUE:
          CHK(convert_uint32(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_INT32VALUE:
          CHK(convert_int32(ofs, state));
          CHK(insert_knowntag(1, UPB_WIRE_TYPE_VARINT, state));
          break;
        case UPB_WELLKNOWN_BOOLVALUE:
          /* Should we accept 0/nonzero as true/false? */
          return NULL;
        case UPB_WELLKNOWN_UNSPECIFIED:
        case UPB_WELLKNOWN_ANY:
        case UPB_WELLKNOWN_VALUE:
        case UPB_WELLKNOWN_LISTVALUE:
        case UPB_WELLKNOWN_STRUCT:
          return NULL;
      }
      CHK(insert_length(ofs, state));
    }
  }
}

#endif

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       const upb_symtab* any_msgs, int options,
                       upb_alloc* alloc, size_t* outlen) {
  const char* start = buf;
  jsonparser generic_parser = {buf + len, NULL, NULL, NULL, alloc, 64};

  CHK(is_proto3(m));
  CHK(buf = parse_json_object(buf, &generic_parser));
  CHK(skip_whitespace(buf, &generic_parser) == NULL);

  return generic_parser.outbuf;
  //*outlen = state.outptr - state.outbuf;
  //return state.outbuf;
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
