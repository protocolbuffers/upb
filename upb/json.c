
#include "upb/json.h"

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#include "upb/upb.h"

#include "upb/port_def.inc"

#define CHK(x) if (UPB_UNLIKELY(!(x))) return 0

static bool is_proto3(const upb_msgdef* m) {
  return upb_filedef_syntax(upb_msgdef_file(m)) == UPB_SYNTAX_PROTO3;
}

typedef struct {
  const char *end;
  char *outbuf;
  char *outptr;
  char *outend;
  upb_alloc *alloc;
  upb_status *status;
} parsestate;

static const char* parse_json_object(const char* buf, const upb_msgdef* m,
                                     parsestate* state);
static const char* parse_json_value(const char* buf, const upb_fielddef* f,
                                    parsestate* state);

UPB_NOINLINE static bool realloc_buf(size_t bytes, parsestate* state) {
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

static bool reserve_bytes(size_t bytes, parsestate* state) {
  size_t have = state->outend - state->outptr;
  return (have >= bytes) || realloc_buf(bytes, state);
}

static bool write_str(const void* str, size_t n, parsestate* state) {
  CHK(reserve_bytes(n, state));
  memcpy(state->outptr, str, n);
  state->outptr += n;
  return true;
}

static bool write_tag(const upb_fielddef *f, parsestate* state) {
  return true;
}

static bool write_char(char ch, parsestate* state) {
  CHK(reserve_bytes(1, state));
  *state->outptr = ch;
  state->outptr++;
  return true;
}

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

bool is_eof(const char* buf, parsestate* state) {
  return (buf == state->end);
}

bool has_n_bytes(const char* buf, size_t n, parsestate* state) {
  return state->end - buf >= n;
}

static const char* skip_digits(const char* buf, parsestate* state) {
  while (!is_eof(buf, state) && *buf >= '0' && *buf <= '9') buf++;

  /* Stream can't end on number. */
  CHK(buf != state->end);
  return buf;
}

static const char* skip_whitespace(const char* buf, parsestate* state) {
  while (!is_eof(buf, state) && is_whitespace(*buf)) {
    buf++;
  }
  return is_eof(buf, state) ? NULL : buf;
}

static const char* parse_char(const char* buf, char ch, parsestate* state) {
  CHK(skip_whitespace(buf, state));
  return *buf == ch ? buf + 1 : NULL;
}

static const char* consume_char(const char* buf, parsestate* state, char* ch) {
  CHK(skip_whitespace(buf, state));
  *ch = *buf;
  return buf + 1;
}

static const char* consume_char_nows(const char* buf, parsestate* state, char* ch) {
  CHK(!is_eof(buf, state));
  *ch = *buf;
  return buf + 1;
}

static const char* parse_lit(const char* buf, const char* lit, parsestate* state) {
  size_t len = strlen(lit);
  CHK(has_n_bytes(buf, len, state));
  CHK(memcmp(buf, lit, len) == 0);
  return buf + len;
}

static bool parse_hex_digit(char ch, uint32_t* digit) {
  *digit <<= 4;

  if (ch >= '0' && ch <= '9') {
    *digit += (ch - '0');
  } else if (ch >= 'a' && ch <= 'f') {
    *digit += ((ch - 'a') + 10);
  } else if (ch >= 'A' && ch <= 'F') {
    *digit += ((ch - 'A') + 10);
  } else {
    return false;
  }

  return true;
}

static bool write_utf8_codepoint(uint32_t cp, parsestate* state) {
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

static const char* parse_escape(const char* buf, parsestate* state) {
  char ch;

  CHK(buf = consume_char_nows(buf, state, &ch));

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
      CHK(write_char('b', state));
      break;
    case 'n':
      CHK(write_char('n', state));
      break;
    case 'r':
      CHK(write_char('r', state));
      break;
    case 't':
      CHK(write_char('t', state));
      break;
    case 'u': {
      uint32_t codepoint = 0;
      CHK(has_n_bytes(buf, 4, state));
      CHK(parse_hex_digit(buf[0], &codepoint));
      CHK(parse_hex_digit(buf[1], &codepoint));
      CHK(parse_hex_digit(buf[2], &codepoint));
      CHK(parse_hex_digit(buf[3], &codepoint));
      CHK(write_utf8_codepoint(codepoint, state));
      break;
    }
    default:
      return NULL;
  }
}

static const char* parse_raw_str(const char* buf, size_t* ofs, parsestate* state) {
  const char* span_start = buf;

  *ofs = state->outptr - state->outbuf;

  while (true) {
    char ch;

    CHK(buf = consume_char_nows(buf, state, &ch));

    switch (*buf) {
      case '"':
        goto done;
      case '\\':
        CHK(write_str(span_start, buf - span_start, state));
        CHK(buf = parse_escape(buf, state));
        span_start = buf;
        break;
      default:
        break;
    }
  }

done:
  /* OPT: could avoid writing the string for cases that don't need it. */
  CHK(write_str(span_start, buf - span_start, state));

  /* OPT: write our own strto{d,f} that don't require termination. */
  CHK(reserve_bytes(1, state));
  *state->outptr = 0;

  return buf;
}

/* Base64 decoding ************************************************************/

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

static bool base64_decode(size_t ofs, const upb_fielddef* f,
                          parsestate* state) {
  /* Warning: in and out alias each other.  This works because we consume "in"
   * faster than "out". */
  char* out = state->outbuf + ofs;
  const char* in = out;
  const char* limit = state->outend;
  size_t len = limit - in;

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
  state->outptr = out;
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

static bool convert_float(size_t ofs, parsestate* state) {
  float f = strtof(outptr(ofs, state), NULL);
  pop_output(ofs, state);
  return write_str(&f, sizeof(f));
}

static bool convert_double(size_t ofs, parsestate* state) {
  double d = strtod(outptr(ofs, state), NULL);
  pop_output(ofs, state);
  return write_str(&d, sizeof(d));
}

static bool convert_enum(size_t ofs, const upb_enumdef *e, parsestate* state) {
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

static bool convert_sfixed32(size_t ofs, parsestate* state) {
  int32_t val = 0;
  CONVERT_INT_BODY
}

static bool convert_sfixed64(size_t ofs, parsestate* state) {
  int64_t val = 0;
  CONVERT_INT_BODY
}

static bool convert_fixed32(size_t ofs, parsestate* state) {
  uint32_t val = 0;
  CONVERT_INT_BODY
}

static bool convert_fixed64(size_t ofs, parsestate* state) {
  uint64_t val = 0;
  CONVERT_INT_BODY
}

#undef CONVERT_INT_BODY

static bool convert_int32(size_t ofs, parsestate* state) {

}

static const char* parse_str(const char* buf, const upb_fielddef* f,
                             parsestate* state) {
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

static const char* parse_number(const char* buf, const upb_fielddef* f,
                                parsestate* state) {
  const char* start = buf - 1;
  char* end;
  double d;

  if (*start != '0') {
    CHK(buf = skip_digits(buf, state));
  }

  if (*buf == '.') {
    buf++;
    CHK(buf = skip_digits(buf, state));
  }

  if (*buf == 'e' || *buf == 'E') {
    buf++;
    CHK(buf != state->end);

    if (*buf == '+' || *buf == '-') {
      buf++;
    }

    CHK(buf = skip_digits(buf, state));
  }

  d = strtod(start, &end);
  CHK(end == buf);

  /* NYI: wrappers, Value, Enum */
  write_tag(f, state);
  write_str(&d, sizeof(d), state);

  return buf;
}

static const char* parse_json_array(const char* buf, const upb_fielddef* f,
                                    parsestate* state) {
  CHK(buf = skip_whitespace(buf, state));

  if (*buf == ']') {
    return buf + 1;
  }

  while (true) {
    char ch;

    CHK(buf = parse_json_value(buf, f, state));
    CHK(buf = consume_char(buf, state, &ch));

    switch (ch) {
      case ',':
        break;
      case ']':
        return buf;
      default:
        return NULL;
    }
  }

  UPB_UNREACHABLE();
}

static const char* parse_json_value(const char* buf, const upb_fielddef* f,
                                    parsestate* state) {
  char ch;

  CHK(buf = consume_char(buf, state, &ch));

  switch (ch) {
    case '{': {
      size_t ofs = output_ofs(state);
      CHK(upb_fielddef_issubmsg(f));
      CHK(write_tag(f, state));
      CHK(buf = parse_json_object(buf, upb_fielddef_msgsubdef(f), state));
      CHK(buf = parse_char(buf, '}', state));
      switch (upb_fielddef_descriptortype(f)) {
        case UPB_DESCRIPTOR_TYPE_MESSAGE:
          CHK(insert_length(ofs, state));
          break;
        case UPB_DESCRIPTOR_TYPE_GROUP:
          CHK(write_end_tag(f, state));
          break;
        default:
          UPB_UNREACHABLE();
      }
      break;
    }
    case '[': {
      CHK(buf = parse_json_array(buf, f, state));
      CHK(buf = parse_char(buf, ']', state));
      break;
    }
    case '"':
      CHK(buf = parse_str(buf, f, state));
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
      CHK(buf = parse_number(buf, f, state));
      break;
    case 't':
      CHK(upb_fielddef_type(f) == UPB_TYPE_BOOL);
      CHK(buf = parse_lit(buf, "rue", state));
      write_tag(f, state);
      write_char(1, state);
    case 'f':
      CHK(upb_fielddef_type(f) == UPB_TYPE_BOOL);
      CHK(buf = parse_lit(buf, "alse", state));
      write_tag(f, state);
      write_char(0, state);
      break;
    case 'n':
      CHK(buf = parse_lit(buf, "ull", state));
      break;
    default:
      return NULL;
  }
}

static const char* parse_json_object(const char* buf, const upb_msgdef* m,
                                     parsestate* state) {
  CHK(buf = parse_char(buf, '{', state));
  CHK(buf = skip_whitespace(buf, state));

  if (*buf == '}') {
    return buf + 1;
  }

  while (true) {
    const char* fieldname;
    const upb_fielddef *f = NULL;
    char ch;

    CHK(buf = parse_char(buf, '"', state));
    CHK(buf = parse_raw_str(buf, &fieldname, state));

    /* TODO(haberman): lookup by JSON name. */
    f = upb_msgdef_ntof(m, fieldname, state->outptr - fieldname);
    CHK(f);

    CHK(buf = parse_char(buf, ':', state));
    CHK(buf = parse_json_value(buf, f, state));
    CHK(buf = consume_char(buf, state, &ch));

    switch (ch) {
      case ',':
        break;
      case '}':
        return buf;
      default:
        return NULL;
    }
  }

  UPB_UNREACHABLE();
}

char* upb_jsontobinary(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen) {
  parsestate state = {buf + len, NULL, NULL, NULL};

  CHK(is_proto3(m));
  CHK(buf = parse_json_object(buf, m, &state));
  CHK(skip_whitespace(buf, &state) == NULL);

  *outlen = state.outptr - state.outbuf;
  return state.outbuf;
}

char* upb_binarytojson(const char* buf, size_t len, const upb_msgdef* m,
                       int options, upb_alloc* alloc, size_t* outlen) {
  char* ret;

  if (!is_proto3(m)) return NULL;

  ret = upb_malloc(alloc, 1);
  ret[0] = 0;
  *outlen = 1;

  return ret;
}
