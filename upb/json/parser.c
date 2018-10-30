
#line 1 "upb/json/parser.rl"
/*
** upb::json::Parser (upb_json_parser)
**
** A parser that uses the Ragel State Machine Compiler to generate
** the finite automata.
**
** Ragel only natively handles regular languages, but we can manually
** program it a bit to handle context-free languages like JSON, by using
** the "fcall" and "fret" constructs.
**
** This parser can handle the basics, but needs several things to be fleshed
** out:
**
** - handling of unicode escape sequences (including high surrogate pairs).
** - properly check and report errors for unknown fields, stack overflow,
**   improper array nesting (or lack of nesting).
** - handling of base64 sequences with padding characters.
** - handling of push-back (non-success returns from sink functions).
** - handling of keys/escape-sequences/etc that span input buffers.
*/

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Need to define __USE_XOPEN before including time.h to make strptime work. */
#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>

#include "upb/json/parser.h"
#include "upb/pb/encoder.h"

#define UPB_JSON_MAX_DEPTH 64

/* Type of value message */
enum {
  VALUE_NULLVALUE   = 0,
  VALUE_NUMBERVALUE = 1,
  VALUE_STRINGVALUE = 2,
  VALUE_BOOLVALUE   = 3,
  VALUE_STRUCTVALUE = 4,
  VALUE_LISTVALUE   = 5
};

/* Forward declare */
static bool is_top_level(upb_json_parser *p);
static bool is_wellknown_msg(upb_json_parser *p, upb_wellknowntype_t type);
static bool is_wellknown_field(upb_json_parser *p, upb_wellknowntype_t type);

static bool is_number_wrapper_object(upb_json_parser *p);
static bool does_number_wrapper_start(upb_json_parser *p);
static bool does_number_wrapper_end(upb_json_parser *p);

static bool is_string_wrapper_object(upb_json_parser *p);
static bool does_string_wrapper_start(upb_json_parser *p);
static bool does_string_wrapper_end(upb_json_parser *p);

static void start_wrapper_object(upb_json_parser *p);
static void end_wrapper_object(upb_json_parser *p);

static void start_value_object(upb_json_parser *p, int value_type);
static void end_value_object(upb_json_parser *p);

static void start_listvalue_object(upb_json_parser *p);
static void end_listvalue_object(upb_json_parser *p);

static void start_structvalue_object(upb_json_parser *p);
static void end_structvalue_object(upb_json_parser *p);

static void start_object(upb_json_parser *p);
static void end_object(upb_json_parser *p);

static void start_any_object(upb_json_parser *p, const char *ptr);
static bool end_any_object(upb_json_parser *p, const char *ptr);

static bool start_subobject(upb_json_parser *p);
static void end_subobject(upb_json_parser *p);

static void start_member(upb_json_parser *p);
static void end_member(upb_json_parser *p);
static bool end_membername(upb_json_parser *p);

static void start_any_member(upb_json_parser *p, const char *ptr);
static void end_any_member(upb_json_parser *p, const char *ptr);
static bool end_any_membername(upb_json_parser *p);

size_t parse(void *closure, const void *hd, const char *buf, size_t size,
             const upb_bufhandle *handle);
bool end(void *closure, const void *hd);

static const char eof_ch = 'e';

/* stringsink */
typedef struct {
  upb_byteshandler handler;
  upb_bytessink sink;
  char *ptr;
  size_t len, size;
} upb_stringsink;


static void *stringsink_start(void *_sink, const void *hd, size_t size_hint) {
  upb_stringsink *sink = _sink;
  sink->len = 0;
  UPB_UNUSED(hd);
  UPB_UNUSED(size_hint);
  return sink;
}

static size_t stringsink_string(void *_sink, const void *hd, const char *ptr,
                                size_t len, const upb_bufhandle *handle) {
  upb_stringsink *sink = _sink;
  size_t new_size = sink->size;

  UPB_UNUSED(hd);
  UPB_UNUSED(handle);

  while (sink->len + len > new_size) {
    new_size *= 2;
  }

  if (new_size != sink->size) {
    sink->ptr = realloc(sink->ptr, new_size);
    sink->size = new_size;
  }

  memcpy(sink->ptr + sink->len, ptr, len);
  sink->len += len;

  return len;
}

void upb_stringsink_init(upb_stringsink *sink) {
  upb_byteshandler_init(&sink->handler);
  upb_byteshandler_setstartstr(&sink->handler, stringsink_start, NULL);
  upb_byteshandler_setstring(&sink->handler, stringsink_string, NULL);

  upb_bytessink_reset(&sink->sink, &sink->handler, sink);

  sink->size = 32;
  sink->ptr = malloc(sink->size);
  sink->len = 0;
}

void upb_stringsink_uninit(upb_stringsink *sink) { free(sink->ptr); }

typedef struct {
  /* For encoding Any value field in binary format. */
  const upb_handlers *encoder_handlers;
  upb_pb_encoder *encoder;
  upb_stringsink stringsink;

  /* For decoding Any value field in json format. */
  upb_json_parsermethod *parser_method;
  upb_json_parser* parser;
  upb_sink sink;

  /* Mark the range of unpacked values in json input before type url. */
  const char *before_type_url_start;
  const char *before_type_url_end;

  /* Mark the range of unpacked values in json input after type url. */
  const char *after_type_url_start;
} upb_jsonparser_any_frame;

typedef struct {
  upb_sink sink;

  /* The current message in which we're parsing, and the field whose value we're
   * expecting next. */
  const upb_msgdef *m;
  const upb_fielddef *f;

  /* The table mapping json name to fielddef for this message. */
  upb_strtable *name_table;

  /* We are in a repeated-field context, ready to emit mapentries as
   * submessages. This flag alters the start-of-object (open-brace) behavior to
   * begin a sequence of mapentry messages rather than a single submessage. */
  bool is_map;

  /* We are in a map-entry message context. This flag is set when parsing the
   * value field of a single map entry and indicates to all value-field parsers
   * (subobjects, strings, numbers, and bools) that the map-entry submessage
   * should end as soon as the value is parsed. */
  bool is_mapentry;

  /* If |is_map| or |is_mapentry| is true, |mapfield| refers to the parent
   * message's map field that we're currently parsing. This differs from |f|
   * because |f| is the field in the *current* message (i.e., the map-entry
   * message itself), not the parent's field that leads to this map. */
  const upb_fielddef *mapfield;

  /* We are in an Any message context. This flag is set when parsing the Any
   * message and indicates to all field parsers (subobjects, strings, numbers,
   * and bools) that the parsed field should be serialized as binary data or
   * cached (type url not found yet). */
  bool is_any;

  /* The type of packed message in Any. */
  upb_jsonparser_any_frame *any_frame;

  /* True if the field to be parsed is unknown. */
  bool is_unknown_field;
} upb_jsonparser_frame;

struct upb_json_parser {
  upb_env *env;
  const upb_json_parsermethod *method;
  upb_bytessink input_;

  /* Stack to track the JSON scopes we are in. */
  upb_jsonparser_frame stack[UPB_JSON_MAX_DEPTH];
  upb_jsonparser_frame *top;
  upb_jsonparser_frame *limit;

  upb_status status;

  /* Ragel's internal parsing stack for the parsing state machine. */
  int current_state;
  int parser_stack[UPB_JSON_MAX_DEPTH];
  int parser_top;

  /* The handle for the current buffer. */
  const upb_bufhandle *handle;

  /* Accumulate buffer.  See details in parser.rl. */
  const char *accumulated;
  size_t accumulated_len;
  char *accumulate_buf;
  size_t accumulate_buf_size;

  /* Multi-part text data.  See details in parser.rl. */
  int multipart_state;
  upb_selector_t string_selector;

  /* Input capture.  See details in parser.rl. */
  const char *capture;

  /* Intermediate result of parsing a unicode escape sequence. */
  uint32_t digit;

  /* For resolve type url in Any. */
  const upb_symtab *symtab;

  /* Whether to proceed if unknown field is met. */
  bool ignore_json_unknown;

  /* Cache for parsing timestamp due to base and zone are handled in different
   * handlers. */
  struct tm tm;
};

struct upb_json_parsermethod {
  upb_refcounted base;

  upb_byteshandler input_handler_;

  /* Mainly for the purposes of refcounting, so all the fielddefs we point
   * to stay alive. */
  const upb_msgdef *msg;

  /* Keys are upb_msgdef*, values are upb_strtable (json_name -> fielddef) */
  upb_inttable name_tables;
};

#define PARSER_CHECK_RETURN(x) if (!(x)) return false

static void json_parser_any_frame_reset(upb_jsonparser_any_frame *frame) {
  frame->encoder_handlers = NULL;
  frame->encoder = NULL;
  frame->parser_method = NULL;
  frame->parser = NULL;
  frame->before_type_url_start = NULL;
  frame->before_type_url_end = NULL;
  frame->after_type_url_start = NULL;
}

static void json_parser_any_frame_set_packed(
    upb_json_parser *p,
    upb_jsonparser_any_frame *frame,
    const upb_msgdef *packed) {
  /* Initialize encoder. */
  frame->encoder_handlers =
      upb_pb_encoder_newhandlers(packed, &frame->encoder_handlers);
  upb_stringsink_init(&frame->stringsink);
  frame->encoder =
      upb_pb_encoder_create(
          p->env, frame->encoder_handlers,
          &frame->stringsink.sink);

  /* Initialize parser. */
  frame->parser_method =
      upb_json_parsermethod_new(packed, &frame->parser_method);
  upb_sink_reset(&frame->sink, frame->encoder_handlers, frame->encoder);
  frame->parser =
      upb_json_parser_create(p->env, frame->parser_method, p->symtab,
                             &frame->sink, p->ignore_json_unknown);
}

static void json_parser_any_frame_free(upb_jsonparser_any_frame *frame) {
  upb_handlers_unref(frame->encoder_handlers,
                     &frame->encoder_handlers);
  upb_json_parsermethod_unref(frame->parser_method,
                              &frame->parser_method);
  upb_stringsink_uninit(&frame->stringsink);
}

static bool json_parser_any_frame_has_type_url(
  upb_jsonparser_any_frame *frame) {
  return frame->encoder != NULL;
}

static bool json_parser_any_frame_has_value_before_type_url(
  upb_jsonparser_any_frame *frame) {
  return frame->before_type_url_start != frame->before_type_url_end;
}

static bool json_parser_any_frame_has_value_after_type_url(
  upb_jsonparser_any_frame *frame) {
  return frame->after_type_url_start != NULL;
}

static bool json_parser_any_frame_has_value(
  upb_jsonparser_any_frame *frame) {
  return json_parser_any_frame_has_value_before_type_url(frame) ||
         json_parser_any_frame_has_value_after_type_url(frame);
}

static void json_parser_any_frame_set_before_type_url_end(
    upb_jsonparser_any_frame *frame,
    const char *ptr) {
  if (frame->encoder == NULL) {
    frame->before_type_url_end = ptr;
  }
}

static void json_parser_any_frame_set_after_type_url_start_once(
    upb_jsonparser_any_frame *frame,
    const char *ptr) {
  if (json_parser_any_frame_has_type_url(frame) &&
      frame->after_type_url_start == NULL) {
    frame->after_type_url_start = ptr;
  }
}

/* Used to signal that a capture has been suspended. */
static char suspend_capture;

static upb_selector_t getsel_for_handlertype(upb_json_parser *p,
                                             upb_handlertype_t type) {
  upb_selector_t sel;
  bool ok = upb_handlers_getselector(p->top->f, type, &sel);
  UPB_ASSERT(ok);
  return sel;
}

static upb_selector_t parser_getsel(upb_json_parser *p) {
  return getsel_for_handlertype(
      p, upb_handlers_getprimitivehandlertype(p->top->f));
}

static bool check_stack(upb_json_parser *p) {
  if ((p->top + 1) == p->limit) {
    upb_status_seterrmsg(&p->status, "Nesting too deep");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  return true;
}

static void set_name_table(upb_json_parser *p, upb_jsonparser_frame *frame) {
  upb_value v;
  bool ok = upb_inttable_lookupptr(&p->method->name_tables, frame->m, &v);
  UPB_ASSERT(ok);
  frame->name_table = upb_value_getptr(v);
}

/* There are GCC/Clang built-ins for overflow checking which we could start
 * using if there was any performance benefit to it. */

static bool checked_add(size_t a, size_t b, size_t *c) {
  if (SIZE_MAX - a < b) return false;
  *c = a + b;
  return true;
}

static size_t saturating_multiply(size_t a, size_t b) {
  /* size_t is unsigned, so this is defined behavior even on overflow. */
  size_t ret = a * b;
  if (b != 0 && ret / b != a) {
    ret = SIZE_MAX;
  }
  return ret;
}


/* Base64 decoding ************************************************************/

/* TODO(haberman): make this streaming. */

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

/* Returns the table value sign-extended to 32 bits.  Knowing that the upper
 * bits will be 1 for unrecognized characters makes it easier to check for
 * this error condition later (see below). */
int32_t b64lookup(unsigned char ch) { return b64table[ch]; }

/* Returns true if the given character is not a valid base64 character or
 * padding. */
bool nonbase64(unsigned char ch) { return b64lookup(ch) == -1 && ch != '='; }

static bool base64_push(upb_json_parser *p, upb_selector_t sel, const char *ptr,
                        size_t len) {
  const char *limit = ptr + len;
  for (; ptr < limit; ptr += 4) {
    uint32_t val;
    char output[3];

    if (limit - ptr < 4) {
      upb_status_seterrf(&p->status,
                         "Base64 input for bytes field not a multiple of 4: %s",
                         upb_fielddef_name(p->top->f));
      upb_env_reporterror(p->env, &p->status);
      return false;
    }

    val = b64lookup(ptr[0]) << 18 |
          b64lookup(ptr[1]) << 12 |
          b64lookup(ptr[2]) << 6  |
          b64lookup(ptr[3]);

    /* Test the upper bit; returns true if any of the characters returned -1. */
    if (val & 0x80000000) {
      goto otherchar;
    }

    output[0] = val >> 16;
    output[1] = (val >> 8) & 0xff;
    output[2] = val & 0xff;
    upb_sink_putstring(&p->top->sink, sel, output, 3, NULL);
  }
  return true;

otherchar:
  if (nonbase64(ptr[0]) || nonbase64(ptr[1]) || nonbase64(ptr[2]) ||
      nonbase64(ptr[3]) ) {
    upb_status_seterrf(&p->status,
                       "Non-base64 characters in bytes field: %s",
                       upb_fielddef_name(p->top->f));
    upb_env_reporterror(p->env, &p->status);
    return false;
  } if (ptr[2] == '=') {
    uint32_t val;
    char output;

    /* Last group contains only two input bytes, one output byte. */
    if (ptr[0] == '=' || ptr[1] == '=' || ptr[3] != '=') {
      goto badpadding;
    }

    val = b64lookup(ptr[0]) << 18 |
          b64lookup(ptr[1]) << 12;

    UPB_ASSERT(!(val & 0x80000000));
    output = val >> 16;
    upb_sink_putstring(&p->top->sink, sel, &output, 1, NULL);
    return true;
  } else {
    uint32_t val;
    char output[2];

    /* Last group contains only three input bytes, two output bytes. */
    if (ptr[0] == '=' || ptr[1] == '=' || ptr[2] == '=') {
      goto badpadding;
    }

    val = b64lookup(ptr[0]) << 18 |
          b64lookup(ptr[1]) << 12 |
          b64lookup(ptr[2]) << 6;

    output[0] = val >> 16;
    output[1] = (val >> 8) & 0xff;
    upb_sink_putstring(&p->top->sink, sel, output, 2, NULL);
    return true;
  }

badpadding:
  upb_status_seterrf(&p->status,
                     "Incorrect base64 padding for field: %s (%.*s)",
                     upb_fielddef_name(p->top->f),
                     4, ptr);
  upb_env_reporterror(p->env, &p->status);
  return false;
}


/* Accumulate buffer **********************************************************/

/* Functionality for accumulating a buffer.
 *
 * Some parts of the parser need an entire value as a contiguous string.  For
 * example, to look up a member name in a hash table, or to turn a string into
 * a number, the relevant library routines need the input string to be in
 * contiguous memory, even if the value spanned two or more buffers in the
 * input.  These routines handle that.
 *
 * In the common case we can just point to the input buffer to get this
 * contiguous string and avoid any actual copy.  So we optimistically begin
 * this way.  But there are a few cases where we must instead copy into a
 * separate buffer:
 *
 *   1. The string was not contiguous in the input (it spanned buffers).
 *
 *   2. The string included escape sequences that need to be interpreted to get
 *      the true value in a contiguous buffer. */

static void assert_accumulate_empty(upb_json_parser *p) {
  UPB_ASSERT(p->accumulated == NULL);
  UPB_ASSERT(p->accumulated_len == 0);
}

static void accumulate_clear(upb_json_parser *p) {
  p->accumulated = NULL;
  p->accumulated_len = 0;
}

/* Used internally by accumulate_append(). */
static bool accumulate_realloc(upb_json_parser *p, size_t need) {
  void *mem;
  size_t old_size = p->accumulate_buf_size;
  size_t new_size = UPB_MAX(old_size, 128);
  while (new_size < need) {
    new_size = saturating_multiply(new_size, 2);
  }

  mem = upb_env_realloc(p->env, p->accumulate_buf, old_size, new_size);
  if (!mem) {
    upb_status_seterrmsg(&p->status, "Out of memory allocating buffer.");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  p->accumulate_buf = mem;
  p->accumulate_buf_size = new_size;
  return true;
}

/* Logically appends the given data to the append buffer.
 * If "can_alias" is true, we will try to avoid actually copying, but the buffer
 * must be valid until the next accumulate_append() call (if any). */
static bool accumulate_append(upb_json_parser *p, const char *buf, size_t len,
                              bool can_alias) {
  size_t need;

  if (!p->accumulated && can_alias) {
    p->accumulated = buf;
    p->accumulated_len = len;
    return true;
  }

  if (!checked_add(p->accumulated_len, len, &need)) {
    upb_status_seterrmsg(&p->status, "Integer overflow.");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  if (need > p->accumulate_buf_size && !accumulate_realloc(p, need)) {
    return false;
  }

  if (p->accumulated != p->accumulate_buf) {
    memcpy(p->accumulate_buf, p->accumulated, p->accumulated_len);
    p->accumulated = p->accumulate_buf;
  }

  memcpy(p->accumulate_buf + p->accumulated_len, buf, len);
  p->accumulated_len += len;
  return true;
}

/* Returns a pointer to the data accumulated since the last accumulate_clear()
 * call, and writes the length to *len.  This with point either to the input
 * buffer or a temporary accumulate buffer. */
static const char *accumulate_getptr(upb_json_parser *p, size_t *len) {
  UPB_ASSERT(p->accumulated);
  *len = p->accumulated_len;
  return p->accumulated;
}


/* Mult-part text data ********************************************************/

/* When we have text data in the input, it can often come in multiple segments.
 * For example, there may be some raw string data followed by an escape
 * sequence.  The two segments are processed with different logic.  Also buffer
 * seams in the input can cause multiple segments.
 *
 * As we see segments, there are two main cases for how we want to process them:
 *
 *  1. we want to push the captured input directly to string handlers.
 *
 *  2. we need to accumulate all the parts into a contiguous buffer for further
 *     processing (field name lookup, string->number conversion, etc). */

/* This is the set of states for p->multipart_state. */
enum {
  /* We are not currently processing multipart data. */
  MULTIPART_INACTIVE = 0,

  /* We are processing multipart data by accumulating it into a contiguous
   * buffer. */
  MULTIPART_ACCUMULATE = 1,

  /* We are processing multipart data by pushing each part directly to the
   * current string handlers. */
  MULTIPART_PUSHEAGERLY = 2
};

/* Start a multi-part text value where we accumulate the data for processing at
 * the end. */
static void multipart_startaccum(upb_json_parser *p) {
  assert_accumulate_empty(p);
  UPB_ASSERT(p->multipart_state == MULTIPART_INACTIVE);
  p->multipart_state = MULTIPART_ACCUMULATE;
}

/* Start a multi-part text value where we immediately push text data to a string
 * value with the given selector. */
static void multipart_start(upb_json_parser *p, upb_selector_t sel) {
  assert_accumulate_empty(p);
  UPB_ASSERT(p->multipart_state == MULTIPART_INACTIVE);
  p->multipart_state = MULTIPART_PUSHEAGERLY;
  p->string_selector = sel;
}

static bool multipart_text(upb_json_parser *p, const char *buf, size_t len,
                           bool can_alias) {
  switch (p->multipart_state) {
    case MULTIPART_INACTIVE:
      upb_status_seterrmsg(
          &p->status, "Internal error: unexpected state MULTIPART_INACTIVE");
      upb_env_reporterror(p->env, &p->status);
      return false;

    case MULTIPART_ACCUMULATE:
      if (!accumulate_append(p, buf, len, can_alias)) {
        return false;
      }
      break;

    case MULTIPART_PUSHEAGERLY: {
      const upb_bufhandle *handle = can_alias ? p->handle : NULL;
      upb_sink_putstring(&p->top->sink, p->string_selector, buf, len, handle);
      break;
    }
  }

  return true;
}

/* Note: this invalidates the accumulate buffer!  Call only after reading its
 * contents. */
static void multipart_end(upb_json_parser *p) {
  UPB_ASSERT(p->multipart_state != MULTIPART_INACTIVE);
  p->multipart_state = MULTIPART_INACTIVE;
  accumulate_clear(p);
}


/* Input capture **************************************************************/

/* Functionality for capturing a region of the input as text.  Gracefully
 * handles the case where a buffer seam occurs in the middle of the captured
 * region. */

static void capture_begin(upb_json_parser *p, const char *ptr) {
  UPB_ASSERT(p->multipart_state != MULTIPART_INACTIVE);
  UPB_ASSERT(p->capture == NULL);
  p->capture = ptr;
}

static bool capture_end(upb_json_parser *p, const char *ptr) {
  UPB_ASSERT(p->capture);
  if (multipart_text(p, p->capture, ptr - p->capture, true)) {
    p->capture = NULL;
    return true;
  } else {
    return false;
  }
}

/* This is called at the end of each input buffer (ie. when we have hit a
 * buffer seam).  If we are in the middle of capturing the input, this
 * processes the unprocessed capture region. */
static void capture_suspend(upb_json_parser *p, const char **ptr) {
  if (!p->capture) return;

  if (multipart_text(p, p->capture, *ptr - p->capture, false)) {
    /* We use this as a signal that we were in the middle of capturing, and
     * that capturing should resume at the beginning of the next buffer.
     * 
     * We can't use *ptr here, because we have no guarantee that this pointer
     * will be valid when we resume (if the underlying memory is freed, then
     * using the pointer at all, even to compare to NULL, is likely undefined
     * behavior). */
    p->capture = &suspend_capture;
  } else {
    /* Need to back up the pointer to the beginning of the capture, since
     * we were not able to actually preserve it. */
    *ptr = p->capture;
  }
}

static void capture_resume(upb_json_parser *p, const char *ptr) {
  if (p->capture) {
    UPB_ASSERT(p->capture == &suspend_capture);
    p->capture = ptr;
  }
}


/* Callbacks from the parser **************************************************/

/* These are the functions called directly from the parser itself.
 * We define these in the same order as their declarations in the parser. */

static char escape_char(char in) {
  switch (in) {
    case 'r': return '\r';
    case 't': return '\t';
    case 'n': return '\n';
    case 'f': return '\f';
    case 'b': return '\b';
    case '/': return '/';
    case '"': return '"';
    case '\\': return '\\';
    default:
      UPB_ASSERT(0);
      return 'x';
  }
}

static bool escape(upb_json_parser *p, const char *ptr) {
  char ch = escape_char(*ptr);
  return multipart_text(p, &ch, 1, false);
}

static void start_hex(upb_json_parser *p) {
  p->digit = 0;
}

static void hexdigit(upb_json_parser *p, const char *ptr) {
  char ch = *ptr;

  p->digit <<= 4;

  if (ch >= '0' && ch <= '9') {
    p->digit += (ch - '0');
  } else if (ch >= 'a' && ch <= 'f') {
    p->digit += ((ch - 'a') + 10);
  } else {
    UPB_ASSERT(ch >= 'A' && ch <= 'F');
    p->digit += ((ch - 'A') + 10);
  }
}

static bool end_hex(upb_json_parser *p) {
  uint32_t codepoint = p->digit;

  /* emit the codepoint as UTF-8. */
  char utf8[3]; /* support \u0000 -- \uFFFF -- need only three bytes. */
  int length = 0;
  if (codepoint <= 0x7F) {
    utf8[0] = codepoint;
    length = 1;
  } else if (codepoint <= 0x07FF) {
    utf8[1] = (codepoint & 0x3F) | 0x80;
    codepoint >>= 6;
    utf8[0] = (codepoint & 0x1F) | 0xC0;
    length = 2;
  } else /* codepoint <= 0xFFFF */ {
    utf8[2] = (codepoint & 0x3F) | 0x80;
    codepoint >>= 6;
    utf8[1] = (codepoint & 0x3F) | 0x80;
    codepoint >>= 6;
    utf8[0] = (codepoint & 0x0F) | 0xE0;
    length = 3;
  }
  /* TODO(haberman): Handle high surrogates: if codepoint is a high surrogate
   * we have to wait for the next escape to get the full code point). */

  return multipart_text(p, utf8, length, false);
}

static void start_text(upb_json_parser *p, const char *ptr) {
  capture_begin(p, ptr);
}

static bool end_text(upb_json_parser *p, const char *ptr) {
  return capture_end(p, ptr);
}

static bool start_number(upb_json_parser *p, const char *ptr) {
  if (is_top_level(p)) {
    if (is_number_wrapper_object(p)) {
      start_wrapper_object(p);
    } else if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
      start_value_object(p, VALUE_NUMBERVALUE);
    } else {
      return false;
    }
  } else if (does_number_wrapper_start(p)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_wrapper_object(p);
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_VALUE)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_value_object(p, VALUE_NUMBERVALUE);
  }

  multipart_startaccum(p);
  capture_begin(p, ptr);
  return true;
}

static bool parse_number(upb_json_parser *p, bool is_quoted);

static bool end_number_nontop(upb_json_parser *p, const char *ptr) {
  if (!capture_end(p, ptr)) {
    return false;
  }

  if (p->top->f == NULL) {
    multipart_end(p);
    return true;
  }

  return parse_number(p, false);
}

static bool end_number(upb_json_parser *p, const char *ptr) {
  if (!end_number_nontop(p, ptr)) {
    return false;
  }

  if (does_number_wrapper_end(p)) {
    end_wrapper_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
    return true;
  }

  if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
    end_value_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
    return true;
  }

  return true;
}

/* |buf| is NULL-terminated. |buf| itself will never include quotes;
 * |is_quoted| tells us whether this text originally appeared inside quotes. */
static bool parse_number_from_buffer(upb_json_parser *p, const char *buf,
                                     bool is_quoted) {
  size_t len = strlen(buf);
  const char *bufend = buf + len;
  char *end;
  upb_fieldtype_t type = upb_fielddef_type(p->top->f);
  double val;
  double dummy;
  double inf = 1.0 / 0.0;  /* C89 does not have an INFINITY macro. */

  errno = 0;

  if (len == 0 || buf[0] == ' ') {
    return false;
  }

  /* For integer types, first try parsing with integer-specific routines.
   * If these succeed, they will be more accurate for int64/uint64 than
   * strtod().
   */
  switch (type) {
    case UPB_TYPE_ENUM:
    case UPB_TYPE_INT32: {
      long val = strtol(buf, &end, 0);
      if (errno == ERANGE || end != bufend) {
        break;
      } else if (val > INT32_MAX || val < INT32_MIN) {
        return false;
      } else {
        upb_sink_putint32(&p->top->sink, parser_getsel(p), val);
        return true;
      }
    }
    case UPB_TYPE_UINT32: {
      unsigned long val = strtoul(buf, &end, 0);
      if (end != bufend) {
        break;
      } else if (val > UINT32_MAX || errno == ERANGE) {
        return false;
      } else {
        upb_sink_putuint32(&p->top->sink, parser_getsel(p), val);
        return true;
      }
    }
    /* XXX: We can't handle [u]int64 properly on 32-bit machines because
     * strto[u]ll isn't in C89. */
    case UPB_TYPE_INT64: {
      long val = strtol(buf, &end, 0);
      if (errno == ERANGE || end != bufend) {
        break;
      } else {
        upb_sink_putint64(&p->top->sink, parser_getsel(p), val);
        return true;
      }
    }
    case UPB_TYPE_UINT64: {
      unsigned long val = strtoul(p->accumulated, &end, 0);
      if (end != bufend) {
        break;
      } else if (errno == ERANGE) {
        return false;
      } else {
        upb_sink_putuint64(&p->top->sink, parser_getsel(p), val);
        return true;
      }
    }
    default:
      break;
  }

  if (type != UPB_TYPE_DOUBLE && type != UPB_TYPE_FLOAT && is_quoted) {
    /* Quoted numbers for integer types are not allowed to be in double form. */
    return false;
  }

  if (len == strlen("Infinity") && strcmp(buf, "Infinity") == 0) {
    /* C89 does not have an INFINITY macro. */
    val = inf;
  } else if (len == strlen("-Infinity") && strcmp(buf, "-Infinity") == 0) {
    val = -inf;
  } else {
    val = strtod(buf, &end);
    if (errno == ERANGE || end != bufend) {
      return false;
    }
  }

  switch (type) {
#define CASE(capitaltype, smalltype, ctype, min, max)                     \
    case UPB_TYPE_ ## capitaltype: {                                      \
      if (modf(val, &dummy) != 0 || val > max || val < min) {             \
        return false;                                                     \
      } else {                                                            \
        upb_sink_put ## smalltype(&p->top->sink, parser_getsel(p),        \
                                  (ctype)val);                            \
        return true;                                                      \
      }                                                                   \
      break;                                                              \
    }
    case UPB_TYPE_ENUM:
    CASE(INT32, int32, int32_t, INT32_MIN, INT32_MAX);
    CASE(INT64, int64, int64_t, INT64_MIN, INT64_MAX);
    CASE(UINT32, uint32, uint32_t, 0, UINT32_MAX);
    CASE(UINT64, uint64, uint64_t, 0, UINT64_MAX);
#undef CASE

    case UPB_TYPE_DOUBLE:
      upb_sink_putdouble(&p->top->sink, parser_getsel(p), val);
      return true;
    case UPB_TYPE_FLOAT:
      if ((val > FLT_MAX || val < -FLT_MAX) && val != inf && val != -inf) {
        return false;
      } else {
        upb_sink_putfloat(&p->top->sink, parser_getsel(p), val);
        return true;
      }
    default:
      return false;
  }
}

static bool parse_number(upb_json_parser *p, bool is_quoted) {
  size_t len;
  const char *buf;

  /* strtol() and friends unfortunately do not support specifying the length of
   * the input string, so we need to force a copy into a NULL-terminated buffer. */
  if (!multipart_text(p, "\0", 1, false)) {
    return false;
  }

  buf = accumulate_getptr(p, &len);

  if (parse_number_from_buffer(p, buf, is_quoted)) {
    multipart_end(p);
    return true;
  } else {
    upb_status_seterrf(&p->status, "error parsing number: %s", buf);
    upb_env_reporterror(p->env, &p->status);
    multipart_end(p);
    return false;
  }
}

static bool parser_putbool(upb_json_parser *p, bool val) {
  bool ok;

  if (p->top->f == NULL) {
    return true;
  }

  if (upb_fielddef_type(p->top->f) != UPB_TYPE_BOOL) {
    upb_status_seterrf(&p->status,
                       "Boolean value specified for non-bool field: %s",
                       upb_fielddef_name(p->top->f));
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  ok = upb_sink_putbool(&p->top->sink, parser_getsel(p), val);
  UPB_ASSERT(ok);

  return true;
}

static bool end_bool(upb_json_parser *p, bool val) {
  if (is_top_level(p)) {
    if (is_wellknown_msg(p, UPB_WELLKNOWN_BOOLVALUE)) {
      start_wrapper_object(p);
    } else if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
      start_value_object(p, VALUE_BOOLVALUE);
    } else {
      return false;
    }
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_BOOLVALUE)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_wrapper_object(p);
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_VALUE)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_value_object(p, VALUE_BOOLVALUE);
  }

  if (p->top->is_unknown_field) {
    return true;
  }

  if (!parser_putbool(p, val)) {
    return false;
  }

  if (is_wellknown_msg(p, UPB_WELLKNOWN_BOOLVALUE)) {
    end_wrapper_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
    return true;
  }

  if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
    end_value_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
    return true;
  }

  return true;
}

static bool end_null(upb_json_parser *p) {
  const char *zero_ptr = "0";

  if (is_top_level(p)) {
    if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
      start_value_object(p, VALUE_NULLVALUE);
    } else {
      return true;
    }
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_VALUE)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_value_object(p, VALUE_NULLVALUE);
  } else {
    return true;
  }

  /* Fill null_value field. */
  multipart_startaccum(p);
  capture_begin(p, zero_ptr);
  capture_end(p, zero_ptr + 1);
  parse_number(p, false);

  end_value_object(p);
  if (!is_top_level(p)) {
    end_subobject(p);
  }

  return true;
}

static bool start_any_stringval(upb_json_parser *p) {
  multipart_startaccum(p);
  return true;
}

static bool start_stringval(upb_json_parser *p) {
  if (is_top_level(p)) {
    if (is_string_wrapper_object(p)) {
      start_wrapper_object(p);
    } else if (is_wellknown_msg(p, UPB_WELLKNOWN_TIMESTAMP) ||
               is_wellknown_msg(p, UPB_WELLKNOWN_DURATION)) {
      start_object(p);
    } else if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
      start_value_object(p, VALUE_STRINGVALUE);
    } else {
      return false;
    }
  } else if (does_string_wrapper_start(p)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_wrapper_object(p);
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_TIMESTAMP) ||
             is_wellknown_field(p, UPB_WELLKNOWN_DURATION)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_object(p);
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_VALUE)) {
    if (!start_subobject(p)) {
      return false;
    }
    start_value_object(p, VALUE_STRINGVALUE);
  }

  if (p->top->f == NULL) {
    multipart_startaccum(p);
    return true;
  }

  if (p->top->is_any) {
    return start_any_stringval(p);
  }

  if (upb_fielddef_isstring(p->top->f)) {
    upb_jsonparser_frame *inner;
    upb_selector_t sel;

    if (!check_stack(p)) return false;

    /* Start a new parser frame: parser frames correspond one-to-one with
     * handler frames, and string events occur in a sub-frame. */
    inner = p->top + 1;
    sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSTR);
    upb_sink_startstr(&p->top->sink, sel, 0, &inner->sink);
    inner->m = p->top->m;
    inner->f = p->top->f;
    inner->name_table = NULL;
    inner->is_map = false;
    inner->is_mapentry = false;
    inner->is_any = false;
    inner->any_frame = NULL;
    inner->is_unknown_field = false;
    p->top = inner;

    if (upb_fielddef_type(p->top->f) == UPB_TYPE_STRING) {
      /* For STRING fields we push data directly to the handlers as it is
       * parsed.  We don't do this yet for BYTES fields, because our base64
       * decoder is not streaming.
       *
       * TODO(haberman): make base64 decoding streaming also. */
      multipart_start(p, getsel_for_handlertype(p, UPB_HANDLER_STRING));
      return true;
    } else {
      multipart_startaccum(p);
      return true;
    }
  } else if (upb_fielddef_type(p->top->f) != UPB_TYPE_BOOL &&
             upb_fielddef_type(p->top->f) != UPB_TYPE_MESSAGE) {
    /* No need to push a frame -- numeric values in quotes remain in the
     * current parser frame.  These values must accmulate so we can convert
     * them all at once at the end. */
    multipart_startaccum(p);
    return true;
  } else {
    upb_status_seterrf(&p->status,
                       "String specified for bool or submessage field: %s",
                       upb_fielddef_name(p->top->f));
    upb_env_reporterror(p->env, &p->status);
    return false;
  }
}

static bool end_any_stringval(upb_json_parser *p) {
  size_t len;
  const char *buf = accumulate_getptr(p, &len);

  /* Set type_url */
  upb_selector_t sel;
  upb_jsonparser_frame *inner;
  if (!check_stack(p)) return false;
  inner = p->top + 1;

  sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSTR);
  upb_sink_startstr(&p->top->sink, sel, 0, &inner->sink);
  sel = getsel_for_handlertype(p, UPB_HANDLER_STRING);
  upb_sink_putstring(&inner->sink, sel, buf, len, NULL);
  sel = getsel_for_handlertype(p, UPB_HANDLER_ENDSTR);
  upb_sink_endstr(&inner->sink, sel);

  multipart_end(p);

  /* Resolve type url */
  if (strncmp(buf, "type.googleapis.com/", 20) == 0 && len > 20) {
    const upb_msgdef *packed = NULL;
    buf += 20;
    len -= 20;

    packed = upb_symtab_lookupmsg2(p->symtab, buf, len);
    if (packed == NULL) {
      upb_status_seterrf(
          &p->status, "Cannot find packed type: %.*s\n", (int)len, buf);
      upb_env_reporterror(p->env, &p->status);
      return false;
    }

    json_parser_any_frame_set_packed(p, p->top->any_frame, packed);
    
    return true;
  } else {
    upb_status_seterrf(
        &p->status, "Invalid type url: %.*s\n", (int)len, buf);
    upb_env_reporterror(p->env, &p->status);
    return false;
  }
}

static bool end_stringval_nontop(upb_json_parser *p) {
  bool ok = true;

  if (is_wellknown_msg(p, UPB_WELLKNOWN_TIMESTAMP) ||
      is_wellknown_msg(p, UPB_WELLKNOWN_DURATION)) {
    multipart_end(p);
    return true;
  }

  if (p->top->is_any) {
    return end_any_stringval(p);
  }

  if (p->top->f == NULL) {
    multipart_end(p);
    return true;
  }

  switch (upb_fielddef_type(p->top->f)) {
    case UPB_TYPE_BYTES:
      if (!base64_push(p, getsel_for_handlertype(p, UPB_HANDLER_STRING),
                       p->accumulated, p->accumulated_len)) {
        return false;
      }
      /* Fall through. */

    case UPB_TYPE_STRING: {
      upb_selector_t sel = getsel_for_handlertype(p, UPB_HANDLER_ENDSTR);
      p->top--;
      upb_sink_endstr(&p->top->sink, sel);
      break;
    }

    case UPB_TYPE_ENUM: {
      /* Resolve enum symbolic name to integer value. */
      const upb_enumdef *enumdef =
          (const upb_enumdef*)upb_fielddef_subdef(p->top->f);

      size_t len;
      const char *buf = accumulate_getptr(p, &len);

      int32_t int_val = 0;
      ok = upb_enumdef_ntoi(enumdef, buf, len, &int_val);

      if (ok) {
        upb_selector_t sel = parser_getsel(p);
        upb_sink_putint32(&p->top->sink, sel, int_val);
      } else {
        upb_status_seterrf(&p->status, "Enum value unknown: '%.*s'", len, buf);
        upb_env_reporterror(p->env, &p->status);
      }

      break;
    }

    case UPB_TYPE_INT32:
    case UPB_TYPE_INT64:
    case UPB_TYPE_UINT32:
    case UPB_TYPE_UINT64:
    case UPB_TYPE_DOUBLE:
    case UPB_TYPE_FLOAT:
      ok = parse_number(p, true);
      break;

    default:
      UPB_ASSERT(false);
      upb_status_seterrmsg(&p->status, "Internal error in JSON decoder");
      upb_env_reporterror(p->env, &p->status);
      ok = false;
      break;
  }

  multipart_end(p);

  return ok;
}

static bool end_stringval(upb_json_parser *p) {
  if (!end_stringval_nontop(p)) {
    return false;
  }

  if (does_string_wrapper_end(p)) {
    end_wrapper_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
    return true;
  }

  if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
    end_value_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
    return true;
  }

  if (is_wellknown_msg(p, UPB_WELLKNOWN_TIMESTAMP) ||
      is_wellknown_msg(p, UPB_WELLKNOWN_DURATION)) {
    end_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
    return true;
  }

  return true;
}

static void start_duration_base(upb_json_parser *p, const char *ptr) {
  capture_begin(p, ptr);
}

static bool end_duration_base(upb_json_parser *p, const char *ptr) {
  size_t len;
  const char *buf;
  char seconds_buf[14];
  char nanos_buf[12];
  char *end;
  int64_t seconds = 0;
  int32_t nanos = 0;
  double val = 0.0;
  const char *seconds_membername = "seconds";
  const char *nanos_membername = "nanos";
  size_t fraction_start;

  if (!capture_end(p, ptr)) {
    return false;
  }

  buf = accumulate_getptr(p, &len);

  memset(seconds_buf, 0, 14);
  memset(nanos_buf, 0, 12);

  /* Find out base end. The maximus duration is 315576000000, which cannot be
   * represented by double without losing precision. Thus, we need to handle
   * fraction and base separately. */
  for (fraction_start = 0; fraction_start < len && buf[fraction_start] != '.';
       fraction_start++);

  /* Parse base */
  memcpy(seconds_buf, buf, fraction_start);
  seconds = strtol(seconds_buf, &end, 10);
  if (errno == ERANGE || end != seconds_buf + fraction_start) {
    upb_status_seterrf(&p->status, "error parsing duration: %s",
                       seconds_buf);
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  if (seconds > 315576000000) {
    upb_status_seterrf(&p->status, "error parsing duration: "
                                   "maximum acceptable value is "
                                   "315576000000");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  if (seconds < -315576000000) {
    upb_status_seterrf(&p->status, "error parsing duration: "
                                   "minimum acceptable value is "
                                   "-315576000000");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  /* Parse fraction */
  nanos_buf[0] = '0';
  memcpy(nanos_buf + 1, buf + fraction_start, len - fraction_start);
  val = strtod(nanos_buf, &end);
  if (errno == ERANGE || end != nanos_buf + len - fraction_start + 1) {
    upb_status_seterrf(&p->status, "error parsing duration: %s",
                       nanos_buf);
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  nanos = val * 1000000000;
  if (seconds < 0) nanos = -nanos;

  /* Clean up buffer */
  multipart_end(p);

  /* Set seconds */
  start_member(p);
  capture_begin(p, seconds_membername);
  capture_end(p, seconds_membername + 7);
  end_membername(p);
  upb_sink_putint64(&p->top->sink, parser_getsel(p), seconds);
  end_member(p);

  /* Set nanos */
  start_member(p);
  capture_begin(p, nanos_membername);
  capture_end(p, nanos_membername + 5);
  end_membername(p);
  upb_sink_putint32(&p->top->sink, parser_getsel(p), nanos);
  end_member(p);

  /* Continue previous environment */
  multipart_startaccum(p);

  return true;
}

static void start_timestamp_base(upb_json_parser *p, const char *ptr) {
  capture_begin(p, ptr);
}

#define UPB_TIMESTAMP_BASE_SIZE 19

static bool end_timestamp_base(upb_json_parser *p, const char *ptr) {
  size_t len;
  const char *buf;
  /* 3 for GMT and 1 for ending 0 */
  char timestamp_buf[UPB_TIMESTAMP_BASE_SIZE + 4];

  if (!capture_end(p, ptr)) {
    return false;
  }

  buf = accumulate_getptr(p, &len);
  UPB_ASSERT(len == UPB_TIMESTAMP_BASE_SIZE);
  memcpy(timestamp_buf, buf, UPB_TIMESTAMP_BASE_SIZE);
  memcpy(timestamp_buf + UPB_TIMESTAMP_BASE_SIZE, "GMT", 3);
  timestamp_buf[UPB_TIMESTAMP_BASE_SIZE + 3] = 0;

  /* Parse seconds */
  if (strptime(timestamp_buf, "%FT%H:%M:%S%Z", &p->tm) == NULL) {
    upb_status_seterrf(&p->status, "error parsing timestamp: %s", buf);
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  /* Clean up buffer */
  multipart_end(p);
  multipart_startaccum(p);

  return true;
}

static void start_timestamp_fraction(upb_json_parser *p, const char *ptr) {
  capture_begin(p, ptr);
}

static bool end_timestamp_fraction(upb_json_parser *p, const char *ptr) {
  size_t len;
  const char *buf;
  char nanos_buf[12];
  char *end;
  double val = 0.0;
  int32_t nanos;
  const char *nanos_membername = "nanos";

  memset(nanos_buf, 0, 12);

  if (!capture_end(p, ptr)) {
    return false;
  }

  buf = accumulate_getptr(p, &len);

  if (len > 10) {
    upb_status_seterrf(&p->status,
        "error parsing timestamp: at most 9-digit fraction.");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  /* Parse nanos */
  nanos_buf[0] = '0';
  memcpy(nanos_buf + 1, buf, len);
  val = strtod(nanos_buf, &end);

  if (errno == ERANGE || end != nanos_buf + len + 1) {
    upb_status_seterrf(&p->status, "error parsing timestamp nanos: %s",
                       nanos_buf);
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  nanos = val * 1000000000;

  /* Clean up previous environment */
  multipart_end(p);

  /* Set nanos */
  start_member(p);
  capture_begin(p, nanos_membername);
  capture_end(p, nanos_membername + 5);
  end_membername(p);
  upb_sink_putint32(&p->top->sink, parser_getsel(p), nanos);
  end_member(p);

  /* Continue previous environment */
  multipart_startaccum(p);

  return true;
}

static void start_timestamp_zone(upb_json_parser *p, const char *ptr) {
  capture_begin(p, ptr);
}

static bool end_timestamp_zone(upb_json_parser *p, const char *ptr) {
  size_t len;
  const char *buf;
  int hours;
  int64_t seconds;
  const char *seconds_membername = "seconds";

  if (!capture_end(p, ptr)) {
    return false;
  }

  buf = accumulate_getptr(p, &len);

  if (buf[0] != 'Z') {
    if (sscanf(buf + 1, "%2d:00", &hours) != 1) {
      upb_status_seterrf(&p->status, "error parsing timestamp offset");
      upb_env_reporterror(p->env, &p->status);
      return false;
    }

    if (buf[0] == '+') {
      hours = -hours;
    }

    p->tm.tm_hour += hours;
  }

  /* Normalize tm */
  seconds = mktime(&p->tm);

  /* Check timestamp boundary */
  if (seconds < -62135596800) {
    upb_status_seterrf(&p->status, "error parsing timestamp: "
                                   "minimum acceptable value is "
                                   "0001-01-01T00:00:00Z");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  /* Clean up previous environment */
  multipart_end(p);

  /* Set seconds */
  start_member(p);
  capture_begin(p, seconds_membername);
  capture_end(p, seconds_membername + 7);
  end_membername(p);
  upb_sink_putint64(&p->top->sink, parser_getsel(p), seconds);
  end_member(p);

  /* Continue previous environment */
  multipart_startaccum(p);

  return true;
}

static void start_member(upb_json_parser *p) {
  UPB_ASSERT(!p->top->f);
  multipart_startaccum(p);
}

/* Helper: invoked during parse_mapentry() to emit the mapentry message's key
 * field based on the current contents of the accumulate buffer. */
static bool parse_mapentry_key(upb_json_parser *p) {

  size_t len;
  const char *buf = accumulate_getptr(p, &len);

  /* Emit the key field. We do a bit of ad-hoc parsing here because the
   * parser state machine has already decided that this is a string field
   * name, and we are reinterpreting it as some arbitrary key type. In
   * particular, integer and bool keys are quoted, so we need to parse the
   * quoted string contents here. */

  p->top->f = upb_msgdef_itof(p->top->m, UPB_MAPENTRY_KEY);
  if (p->top->f == NULL) {
    upb_status_seterrmsg(&p->status, "mapentry message has no key");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }
  switch (upb_fielddef_type(p->top->f)) {
    case UPB_TYPE_INT32:
    case UPB_TYPE_INT64:
    case UPB_TYPE_UINT32:
    case UPB_TYPE_UINT64:
      /* Invoke end_number. The accum buffer has the number's text already. */
      if (!parse_number(p, true)) {
        return false;
      }
      break;
    case UPB_TYPE_BOOL:
      if (len == 4 && !strncmp(buf, "true", 4)) {
        if (!parser_putbool(p, true)) {
          return false;
        }
      } else if (len == 5 && !strncmp(buf, "false", 5)) {
        if (!parser_putbool(p, false)) {
          return false;
        }
      } else {
        upb_status_seterrmsg(&p->status,
                             "Map bool key not 'true' or 'false'");
        upb_env_reporterror(p->env, &p->status);
        return false;
      }
      multipart_end(p);
      break;
    case UPB_TYPE_STRING:
    case UPB_TYPE_BYTES: {
      upb_sink subsink;
      upb_selector_t sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSTR);
      upb_sink_startstr(&p->top->sink, sel, len, &subsink);
      sel = getsel_for_handlertype(p, UPB_HANDLER_STRING);
      upb_sink_putstring(&subsink, sel, buf, len, NULL);
      sel = getsel_for_handlertype(p, UPB_HANDLER_ENDSTR);
      upb_sink_endstr(&p->top->sink, sel);
      multipart_end(p);
      break;
    }
    default:
      upb_status_seterrmsg(&p->status, "Invalid field type for map key");
      upb_env_reporterror(p->env, &p->status);
      return false;
  }

  return true;
}

/* Helper: emit one map entry (as a submessage in the map field sequence). This
 * is invoked from end_membername(), at the end of the map entry's key string,
 * with the map key in the accumulate buffer. It parses the key from that
 * buffer, emits the handler calls to start the mapentry submessage (setting up
 * its subframe in the process), and sets up state in the subframe so that the
 * value parser (invoked next) will emit the mapentry's value field and then
 * end the mapentry message. */

static bool handle_mapentry(upb_json_parser *p) {
  const upb_fielddef *mapfield;
  const upb_msgdef *mapentrymsg;
  upb_jsonparser_frame *inner;
  upb_selector_t sel;

  /* Map entry: p->top->sink is the seq frame, so we need to start a frame
   * for the mapentry itself, and then set |f| in that frame so that the map
   * value field is parsed, and also set a flag to end the frame after the
   * map-entry value is parsed. */
  if (!check_stack(p)) return false;

  mapfield = p->top->mapfield;
  mapentrymsg = upb_fielddef_msgsubdef(mapfield);

  inner = p->top + 1;
  p->top->f = mapfield;
  sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSUBMSG);
  upb_sink_startsubmsg(&p->top->sink, sel, &inner->sink);
  inner->m = mapentrymsg;
  inner->name_table = NULL;
  inner->mapfield = mapfield;
  inner->is_map = false;
  inner->is_any = false;
  inner->any_frame = NULL;
  inner->is_unknown_field = false;

  /* Don't set this to true *yet* -- we reuse parsing handlers below to push
   * the key field value to the sink, and these handlers will pop the frame
   * if they see is_mapentry (when invoked by the parser state machine, they
   * would have just seen the map-entry value, not key). */
  inner->is_mapentry = false;
  p->top = inner;

  /* send STARTMSG in submsg frame. */
  upb_sink_startmsg(&p->top->sink);

  parse_mapentry_key(p);

  /* Set up the value field to receive the map-entry value. */
  p->top->f = upb_msgdef_itof(p->top->m, UPB_MAPENTRY_VALUE);
  p->top->is_mapentry = true;  /* set up to pop frame after value is parsed. */
  p->top->mapfield = mapfield;
  if (p->top->f == NULL) {
    upb_status_seterrmsg(&p->status, "mapentry message has no value");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  return true;
}

static bool end_membername(upb_json_parser *p) {
  UPB_ASSERT(!p->top->f);

  if (!p->top->m) {
    p->top->is_unknown_field = true;
    multipart_end(p);
    return true;
  }

  if (p->top->is_any) {
    return end_any_membername(p);
  } else if (p->top->is_map) {
    return handle_mapentry(p);
  } else {
    size_t len;
    const char *buf = accumulate_getptr(p, &len);
    upb_value v;

    if (upb_strtable_lookup2(p->top->name_table, buf, len, &v)) {
      p->top->f = upb_value_getconstptr(v);
      multipart_end(p);

      return true;
    } else if (p->ignore_json_unknown) {
      p->top->is_unknown_field = true;
      multipart_end(p);
      return true;
    } else {
      upb_status_seterrf(&p->status, "No such field: %.*s\n", (int)len, buf);
      upb_env_reporterror(p->env, &p->status);
      return false;
    }
  }
}

static bool end_any_membername(upb_json_parser *p) {
  size_t len;
  const char *buf = accumulate_getptr(p, &len);
  upb_value v;

  if (len == 5 && strncmp(buf, "@type", len) == 0) {
    upb_strtable_lookup2(p->top->name_table, "type_url", 8, &v);
    p->top->f = upb_value_getconstptr(v);
    multipart_end(p);
    return true;
  } else {
    p->top->is_unknown_field = true;
    multipart_end(p);
    return true;
  }
}

static void end_member(upb_json_parser *p) {
  /* If we just parsed a map-entry value, end that frame too. */
  if (p->top->is_mapentry) {
    upb_status s = UPB_STATUS_INIT;
    upb_selector_t sel;
    bool ok;
    const upb_fielddef *mapfield;

    UPB_ASSERT(p->top > p->stack);
    /* send ENDMSG on submsg. */
    upb_sink_endmsg(&p->top->sink, &s);
    mapfield = p->top->mapfield;

    /* send ENDSUBMSG in repeated-field-of-mapentries frame. */
    p->top--;
    ok = upb_handlers_getselector(mapfield, UPB_HANDLER_ENDSUBMSG, &sel);
    UPB_ASSERT(ok);
    upb_sink_endsubmsg(&p->top->sink, sel);
  }

  p->top->f = NULL;
  p->top->is_unknown_field = false;
}

static void start_any_member(upb_json_parser *p, const char *ptr) {
  start_member(p);
  json_parser_any_frame_set_after_type_url_start_once(p->top->any_frame, ptr);
}

static void end_any_member(upb_json_parser *p, const char *ptr) {
  json_parser_any_frame_set_before_type_url_end(p->top->any_frame, ptr);
  end_member(p);
}

static bool start_subobject(upb_json_parser *p) {
  if (p->top->is_unknown_field) {
    upb_jsonparser_frame *inner;
    if (!check_stack(p)) return false;

    inner = p->top + 1;
    inner->m = NULL;
    inner->f = NULL;
    inner->is_map = false;
    inner->is_mapentry = false;
    inner->is_any = false;
    inner->any_frame = NULL;
    inner->is_unknown_field = false;
    p->top = inner;
    return true;
  }

  if (upb_fielddef_ismap(p->top->f)) {
    upb_jsonparser_frame *inner;
    upb_selector_t sel;

    /* Beginning of a map. Start a new parser frame in a repeated-field
     * context. */
    if (!check_stack(p)) return false;

    inner = p->top + 1;
    sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSEQ);
    upb_sink_startseq(&p->top->sink, sel, &inner->sink);
    inner->m = upb_fielddef_msgsubdef(p->top->f);
    inner->name_table = NULL;
    inner->mapfield = p->top->f;
    inner->f = NULL;
    inner->is_map = true;
    inner->is_mapentry = false;
    inner->is_any = false;
    inner->any_frame = NULL;
    inner->is_unknown_field = false;
    p->top = inner;

    return true;
  } else if (upb_fielddef_issubmsg(p->top->f)) {
    upb_jsonparser_frame *inner;
    upb_selector_t sel;

    /* Beginning of a subobject. Start a new parser frame in the submsg
     * context. */
    if (!check_stack(p)) return false;

    inner = p->top + 1;

    sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSUBMSG);
    upb_sink_startsubmsg(&p->top->sink, sel, &inner->sink);
    inner->m = upb_fielddef_msgsubdef(p->top->f);
    set_name_table(p, inner);
    inner->f = NULL;
    inner->is_map = false;
    inner->is_mapentry = false;
    inner->is_unknown_field = false;
    p->top = inner;

    if (is_wellknown_msg(p, UPB_WELLKNOWN_ANY)) {
      p->top->is_any = true;
      p->top->any_frame =
          upb_env_malloc(p->env, sizeof(upb_jsonparser_any_frame));
      json_parser_any_frame_reset(p->top->any_frame);
    } else {
      p->top->is_any = false;
      p->top->any_frame = NULL;
    }

    return true;
  } else {
    upb_status_seterrf(&p->status,
                       "Object specified for non-message/group field: %s",
                       upb_fielddef_name(p->top->f));
    upb_env_reporterror(p->env, &p->status);
    return false;
  }
}

static bool start_subobject_full(upb_json_parser *p) {
  if (is_top_level(p)) {
    if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
      start_value_object(p, VALUE_STRUCTVALUE);
      if (!start_subobject(p)) return false;
      start_structvalue_object(p);
    } else if (is_wellknown_msg(p, UPB_WELLKNOWN_STRUCT)) {
      start_structvalue_object(p);
    } else {
      return true;
    }
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_STRUCT)) {
    if (!start_subobject(p)) return false;
    start_structvalue_object(p);
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_VALUE)) {
    if (!start_subobject(p)) return false;
    start_value_object(p, VALUE_STRUCTVALUE);
    if (!start_subobject(p)) return false;
    start_structvalue_object(p);
  }

  return start_subobject(p);
}

static void end_subobject(upb_json_parser *p) {
  if (is_top_level(p)) {
    return;
  }

  if (p->top->is_map) {
    upb_selector_t sel;
    p->top--;
    sel = getsel_for_handlertype(p, UPB_HANDLER_ENDSEQ);
    upb_sink_endseq(&p->top->sink, sel);
  } else {
    upb_selector_t sel;
    bool is_unknown = p->top->m == NULL;
    p->top--;
    if (!is_unknown) {
      sel = getsel_for_handlertype(p, UPB_HANDLER_ENDSUBMSG);
      upb_sink_endsubmsg(&p->top->sink, sel);
    }
  }
}

static void end_subobject_full(upb_json_parser *p) {
  end_subobject(p);

  if (is_wellknown_msg(p, UPB_WELLKNOWN_STRUCT)) {
    end_structvalue_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
  }

  if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
    end_value_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
  }
}

static bool start_array(upb_json_parser *p) {
  upb_jsonparser_frame *inner;
  upb_selector_t sel;

  if (is_top_level(p)) {
    if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
      start_value_object(p, VALUE_LISTVALUE);
      if (!start_subobject(p)) return false;
      start_listvalue_object(p);
    } else if (is_wellknown_msg(p, UPB_WELLKNOWN_LISTVALUE)) {
      start_listvalue_object(p);
    } else {
      return false;
    }
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_LISTVALUE)) {
    if (!start_subobject(p)) return false;
    start_listvalue_object(p);
  } else if (is_wellknown_field(p, UPB_WELLKNOWN_VALUE)) {
    if (!start_subobject(p)) return false;
    start_value_object(p, VALUE_LISTVALUE);
    if (!start_subobject(p)) return false;
    start_listvalue_object(p);
  }

  if (p->top->is_unknown_field) {
    inner = p->top + 1;
    inner->m = NULL;
    inner->name_table = NULL;
    inner->f = NULL;
    inner->is_map = false;
    inner->is_mapentry = false;
    inner->is_any = false;
    inner->any_frame = NULL;
    inner->is_unknown_field = true;
    p->top = inner;

    return true;
  }

  if (!upb_fielddef_isseq(p->top->f)) {
    upb_status_seterrf(&p->status,
                       "Array specified for non-repeated field: %s",
                       upb_fielddef_name(p->top->f));
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  if (!check_stack(p)) return false;

  inner = p->top + 1;
  sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSEQ);
  upb_sink_startseq(&p->top->sink, sel, &inner->sink);
  inner->m = p->top->m;
  inner->name_table = NULL;
  inner->f = p->top->f;
  inner->is_map = false;
  inner->is_mapentry = false;
  inner->is_any = false;
  inner->any_frame = NULL;
  inner->is_unknown_field = false;
  p->top = inner;

  return true;
}

static void end_array(upb_json_parser *p) {
  upb_selector_t sel;

  UPB_ASSERT(p->top > p->stack);

  p->top--;

  if (p->top->is_unknown_field) {
    return;
  }

  sel = getsel_for_handlertype(p, UPB_HANDLER_ENDSEQ);
  upb_sink_endseq(&p->top->sink, sel);

  if (is_wellknown_msg(p, UPB_WELLKNOWN_LISTVALUE)) {
    end_listvalue_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
  }

  if (is_wellknown_msg(p, UPB_WELLKNOWN_VALUE)) {
    end_value_object(p);
    if (!is_top_level(p)) {
      end_subobject(p);
    }
  }
}

static void start_object(upb_json_parser *p) {
  if (!p->top->is_map && p->top->m != NULL) {
    upb_sink_startmsg(&p->top->sink);
  }
}

static void end_object(upb_json_parser *p) {
  if (!p->top->is_map && p->top->m != NULL) {
    upb_status status;
    upb_status_clear(&status);
    upb_sink_endmsg(&p->top->sink, &status);
    if (!upb_ok(&status)) {
      upb_env_reporterror(p->env, &status);
    }
  }
}

static void start_any_object(upb_json_parser *p, const char *ptr) {
  start_object(p);
  p->top->any_frame->before_type_url_start = ptr;
  p->top->any_frame->before_type_url_end = ptr;
}

static bool end_any_object(upb_json_parser *p, const char *ptr) {
  const char *value_membername = "value";
  bool is_well_known_packed = false;
  const char *packed_end = ptr + 1;
  upb_selector_t sel;
  upb_jsonparser_frame *inner;

  if (json_parser_any_frame_has_value(p->top->any_frame) &&
      !json_parser_any_frame_has_type_url(p->top->any_frame)) {
    upb_status_seterrmsg(&p->status, "No valid type url");
    upb_env_reporterror(p->env, &p->status);
    return false;
  }

  /* Well known types data is represented as value field. */
  if (upb_msgdef_wellknowntype(p->top->any_frame->parser->top->m) !=
          UPB_WELLKNOWN_UNSPECIFIED) {
    is_well_known_packed = true;

    if (json_parser_any_frame_has_value_before_type_url(p->top->any_frame)) {
      p->top->any_frame->before_type_url_start =
          memchr(p->top->any_frame->before_type_url_start, ':',
                 p->top->any_frame->before_type_url_end -
                 p->top->any_frame->before_type_url_start);
      if (p->top->any_frame->before_type_url_start == NULL) {
        upb_status_seterrmsg(&p->status, "invalid data for well known type.");
        upb_env_reporterror(p->env, &p->status);
        return false;
      }
      p->top->any_frame->before_type_url_start++;
    }

    if (json_parser_any_frame_has_value_after_type_url(p->top->any_frame)) {
      p->top->any_frame->after_type_url_start =
          memchr(p->top->any_frame->after_type_url_start, ':',
                 (ptr + 1) -
                 p->top->any_frame->after_type_url_start);
      if (p->top->any_frame->after_type_url_start == NULL) {
        upb_status_seterrmsg(&p->status, "Invalid data for well known type.");
        upb_env_reporterror(p->env, &p->status);
        return false;
      }
      p->top->any_frame->after_type_url_start++;
      packed_end = ptr;
    }
  }

  if (json_parser_any_frame_has_value_before_type_url(p->top->any_frame)) {
    if (!parse(p->top->any_frame->parser, NULL,
               p->top->any_frame->before_type_url_start,
               p->top->any_frame->before_type_url_end -
               p->top->any_frame->before_type_url_start, NULL)) {
      return false;
    }
  } else {
    if (!is_well_known_packed) {
      if (!parse(p->top->any_frame->parser, NULL, "{", 1, NULL)) {
        return false;
      }
    }
  }

  if (json_parser_any_frame_has_value_before_type_url(p->top->any_frame) &&
      json_parser_any_frame_has_value_after_type_url(p->top->any_frame)) {
    if (!parse(p->top->any_frame->parser, NULL, ",", 1, NULL)) {
      return false;
    }
  }

  if (json_parser_any_frame_has_value_after_type_url(p->top->any_frame)) {
    if (!parse(p->top->any_frame->parser, NULL,
               p->top->any_frame->after_type_url_start,
               packed_end - p->top->any_frame->after_type_url_start, NULL)) {
      return false;
    }
  } else {
    if (!is_well_known_packed) {
      if (!parse(p->top->any_frame->parser, NULL, "}", 1, NULL)) {
        return false;
      }
    }
  }

  if (!end(p->top->any_frame->parser, NULL)) {
    return false;
  }

  p->top->is_any = false;

  /* Set value */
  start_member(p);
  capture_begin(p, value_membername);
  capture_end(p, value_membername + 5);
  end_membername(p);

  if (!check_stack(p)) return false;
  inner = p->top + 1;

  sel = getsel_for_handlertype(p, UPB_HANDLER_STARTSTR);
  upb_sink_startstr(&p->top->sink, sel, 0, &inner->sink);
  sel = getsel_for_handlertype(p, UPB_HANDLER_STRING);
  upb_sink_putstring(&inner->sink, sel, p->top->any_frame->stringsink.ptr,
                     p->top->any_frame->stringsink.len, NULL);
  sel = getsel_for_handlertype(p, UPB_HANDLER_ENDSTR);
  upb_sink_endstr(&inner->sink, sel);

  end_member(p);

  end_object(p);

  /* Deallocate any parse frame. */
  json_parser_any_frame_free(p->top->any_frame);
  upb_env_free(p->env, p->top->any_frame);

  return true;
}

static bool is_string_wrapper(const upb_msgdef *m) {
  upb_wellknowntype_t type = upb_msgdef_wellknowntype(m);
  return type == UPB_WELLKNOWN_STRINGVALUE ||
         type == UPB_WELLKNOWN_BYTESVALUE;
}

static void start_wrapper_object(upb_json_parser *p) {
  const char *membername = "value";

  start_object(p);

  /* Set up context for parsing value */
  start_member(p);
  capture_begin(p, membername);
  capture_end(p, membername + 5);
  end_membername(p);
}

static void end_wrapper_object(upb_json_parser *p) {
  end_member(p);
  end_object(p);
}

static void start_value_object(upb_json_parser *p, int value_type) {
  const char *nullmember = "null_value";
  const char *numbermember = "number_value";
  const char *stringmember = "string_value";
  const char *boolmember = "bool_value";
  const char *structmember = "struct_value";
  const char *listmember = "list_value";
  const char *membername = "";

  switch (value_type) {
    case VALUE_NULLVALUE:
      membername = nullmember;
      break;
    case VALUE_NUMBERVALUE:
      membername = numbermember;
      break;
    case VALUE_STRINGVALUE:
      membername = stringmember;
      break;
    case VALUE_BOOLVALUE:
      membername = boolmember;
      break;
    case VALUE_STRUCTVALUE:
      membername = structmember;
      break;
    case VALUE_LISTVALUE:
      membername = listmember;
      break;
  }

  start_object(p);

  /* Set up context for parsing value */
  start_member(p);
  capture_begin(p, membername);
  capture_end(p, membername + strlen(membername));
  end_membername(p);
}

static void end_value_object(upb_json_parser *p) {
  end_member(p);
  end_object(p);
}

static void start_listvalue_object(upb_json_parser *p) {
  const char *membername = "values";

  start_object(p);

  /* Set up context for parsing value */
  start_member(p);
  capture_begin(p, membername);
  capture_end(p, membername + strlen(membername));
  end_membername(p);
}

static void end_listvalue_object(upb_json_parser *p) {
  end_member(p);
  end_object(p);
}

static void start_structvalue_object(upb_json_parser *p) {
  const char *membername = "fields";

  start_object(p);

  /* Set up context for parsing value */
  start_member(p);
  capture_begin(p, membername);
  capture_end(p, membername + strlen(membername));
  end_membername(p);
}

static void end_structvalue_object(upb_json_parser *p) {
  end_member(p);
  end_object(p);
}

static bool is_top_level(upb_json_parser *p) {
  return p->top == p->stack && p->top->f == NULL && !p->top->is_unknown_field;
}

static bool is_wellknown_msg(upb_json_parser *p, upb_wellknowntype_t type) {
  return p->top->m != NULL && upb_msgdef_wellknowntype(p->top->m) == type;
}

static bool is_wellknown_field(upb_json_parser *p, upb_wellknowntype_t type) {
  return p->top->f != NULL &&
         upb_fielddef_issubmsg(p->top->f) &&
         (upb_msgdef_wellknowntype(upb_fielddef_msgsubdef(p->top->f))
              == type);
}

static bool does_number_wrapper_start(upb_json_parser *p) {
  return p->top->f != NULL &&
         upb_fielddef_issubmsg(p->top->f) &&
         upb_msgdef_isnumberwrapper(upb_fielddef_msgsubdef(p->top->f));
}

static bool does_number_wrapper_end(upb_json_parser *p) {
  return p->top->m != NULL && upb_msgdef_isnumberwrapper(p->top->m);
}

static bool is_number_wrapper_object(upb_json_parser *p) {
  return p->top->m != NULL && upb_msgdef_isnumberwrapper(p->top->m);
}

static bool does_string_wrapper_start(upb_json_parser *p) {
  return p->top->f != NULL &&
         upb_fielddef_issubmsg(p->top->f) &&
         is_string_wrapper(upb_fielddef_msgsubdef(p->top->f));
}

static bool does_string_wrapper_end(upb_json_parser *p) {
  return p->top->m != NULL && is_string_wrapper(p->top->m);
}

static bool is_string_wrapper_object(upb_json_parser *p) {
  return p->top->m != NULL && is_string_wrapper(p->top->m);
}

#define CHECK_RETURN_TOP(x) if (!(x)) goto error


/* The actual parser **********************************************************/

/* What follows is the Ragel parser itself.  The language is specified in Ragel
 * and the actions call our C functions above.
 *
 * Ragel has an extensive set of functionality, and we use only a small part of
 * it.  There are many action types but we only use a few:
 *
 *   ">" -- transition into a machine
 *   "%" -- transition out of a machine
 *   "@" -- transition into a final state of a machine.
 *
 * "@" transitions are tricky because a machine can transition into a final
 * state repeatedly.  But in some cases we know this can't happen, for example
 * a string which is delimited by a final '"' can only transition into its
 * final state once, when the closing '"' is seen. */


#line 2576 "upb/json/parser.rl"



#line 2422 "upb/json/parser.c"
static const char _json_actions[] = {
	0, 1, 0, 1, 1, 1, 3, 1, 
	4, 1, 6, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 21, 1, 23, 1, 24, 1, 
	26, 1, 27, 1, 28, 1, 30, 1, 
	32, 1, 33, 1, 34, 1, 35, 1, 
	37, 1, 38, 2, 4, 9, 2, 5, 
	6, 2, 7, 3, 2, 7, 9, 2, 
	14, 15, 2, 16, 17, 2, 18, 19, 
	2, 22, 20, 2, 24, 26, 2, 29, 
	2, 2, 30, 38, 2, 31, 20, 2, 
	33, 38, 2, 34, 38, 2, 35, 38, 
	2, 36, 25, 2, 37, 38, 4, 14, 
	15, 16, 17
};

static const char _json_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 1, 0, 0, 1, 1, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	37, 43, 45, 41, 47, 0, 0, 0, 
	0, 0
};

static const int json_start = 1;

static const int json_en_number_machine = 23;
static const int json_en_string_machine = 32;
static const int json_en_duration_machine = 40;
static const int json_en_timestamp_machine = 47;
static const int json_en_value_machine = 75;
static const int json_en_main = 1;


#line 2579 "upb/json/parser.rl"

size_t parse(void *closure, const void *hd, const char *buf, size_t size,
             const upb_bufhandle *handle) {
  upb_json_parser *parser = closure;

  /* Variables used by Ragel's generated code. */
  int cs = parser->current_state;
  int *stack = parser->parser_stack;
  int top = parser->parser_top;

  const char *p = buf;
  const char *pe = buf + size;
  const char *eof = &eof_ch;

  parser->handle = handle;

  UPB_UNUSED(hd);
  UPB_UNUSED(handle);

  capture_resume(parser, buf);

  
#line 2491 "upb/json/parser.c"
	{
	const char *_acts;
	unsigned int _nacts;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	switch ( cs ) {
case 1:
	switch( (*p) ) {
		case 32: goto tr0;
		case 34: goto tr2;
		case 45: goto tr3;
		case 91: goto tr4;
		case 102: goto tr5;
		case 110: goto tr6;
		case 116: goto tr7;
		case 123: goto tr8;
	}
	if ( (*p) > 13 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr3;
	} else if ( (*p) >= 9 )
		goto tr0;
	goto tr1;
case 0:
	goto _out;
case 2:
	if ( (*p) == 34 )
		goto tr9;
	goto tr1;
case 103:
	if ( (*p) == 32 )
		goto tr142;
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr142;
	goto tr1;
case 104:
	if ( (*p) == 32 )
		goto tr143;
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr143;
	goto tr1;
case 3:
	switch( (*p) ) {
		case 32: goto tr11;
		case 93: goto tr12;
		case 125: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr11;
	goto tr10;
case 4:
	switch( (*p) ) {
		case 32: goto tr13;
		case 44: goto tr14;
		case 93: goto tr12;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr13;
	goto tr1;
case 5:
	switch( (*p) ) {
		case 32: goto tr14;
		case 93: goto tr1;
		case 125: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr14;
	goto tr10;
case 6:
	if ( (*p) == 97 )
		goto tr15;
	goto tr1;
case 7:
	if ( (*p) == 108 )
		goto tr16;
	goto tr1;
case 8:
	if ( (*p) == 115 )
		goto tr17;
	goto tr1;
case 9:
	if ( (*p) == 101 )
		goto tr18;
	goto tr1;
case 105:
	if ( (*p) == 32 )
		goto tr144;
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr144;
	goto tr1;
case 10:
	if ( (*p) == 117 )
		goto tr19;
	goto tr1;
case 11:
	if ( (*p) == 108 )
		goto tr20;
	goto tr1;
case 12:
	if ( (*p) == 108 )
		goto tr21;
	goto tr1;
case 106:
	if ( (*p) == 32 )
		goto tr145;
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr145;
	goto tr1;
case 13:
	if ( (*p) == 114 )
		goto tr22;
	goto tr1;
case 14:
	if ( (*p) == 117 )
		goto tr23;
	goto tr1;
case 15:
	if ( (*p) == 101 )
		goto tr24;
	goto tr1;
case 107:
	if ( (*p) == 32 )
		goto tr146;
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr146;
	goto tr1;
case 16:
	switch( (*p) ) {
		case 32: goto tr25;
		case 34: goto tr26;
		case 125: goto tr27;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr25;
	goto tr1;
case 17:
	if ( (*p) == 34 )
		goto tr28;
	goto tr1;
case 18:
	switch( (*p) ) {
		case 32: goto tr29;
		case 58: goto tr30;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr29;
	goto tr1;
case 19:
	switch( (*p) ) {
		case 32: goto tr30;
		case 93: goto tr1;
		case 125: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr30;
	goto tr31;
case 20:
	switch( (*p) ) {
		case 32: goto tr32;
		case 44: goto tr33;
		case 125: goto tr34;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr32;
	goto tr1;
case 21:
	switch( (*p) ) {
		case 32: goto tr35;
		case 44: goto tr36;
		case 125: goto tr27;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr35;
	goto tr1;
case 22:
	switch( (*p) ) {
		case 32: goto tr36;
		case 34: goto tr26;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr36;
	goto tr1;
case 108:
	if ( (*p) == 32 )
		goto tr147;
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr147;
	goto tr1;
case 23:
	switch( (*p) ) {
		case 45: goto tr37;
		case 48: goto tr38;
	}
	if ( 49 <= (*p) && (*p) <= 57 )
		goto tr39;
	goto tr1;
case 24:
	if ( (*p) == 48 )
		goto tr38;
	if ( 49 <= (*p) && (*p) <= 57 )
		goto tr39;
	goto tr1;
case 25:
	switch( (*p) ) {
		case 46: goto tr41;
		case 69: goto tr42;
		case 101: goto tr42;
	}
	goto tr40;
case 109:
	goto tr1;
case 26:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr43;
	goto tr1;
case 27:
	switch( (*p) ) {
		case 69: goto tr42;
		case 101: goto tr42;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr43;
	goto tr40;
case 28:
	switch( (*p) ) {
		case 43: goto tr44;
		case 45: goto tr44;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr45;
	goto tr1;
case 29:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr45;
	goto tr1;
case 30:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr45;
	goto tr40;
case 31:
	switch( (*p) ) {
		case 46: goto tr41;
		case 69: goto tr42;
		case 101: goto tr42;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr39;
	goto tr40;
case 32:
	switch( (*p) ) {
		case 34: goto tr47;
		case 92: goto tr48;
	}
	goto tr46;
case 33:
	switch( (*p) ) {
		case 34: goto tr50;
		case 92: goto tr51;
	}
	goto tr49;
case 110:
	goto tr1;
case 34:
	switch( (*p) ) {
		case 34: goto tr52;
		case 47: goto tr52;
		case 92: goto tr52;
		case 98: goto tr52;
		case 102: goto tr52;
		case 110: goto tr52;
		case 114: goto tr52;
		case 116: goto tr52;
		case 117: goto tr53;
	}
	goto tr1;
case 35:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr54;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr54;
	} else
		goto tr54;
	goto tr1;
case 36:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr55;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr55;
	} else
		goto tr55;
	goto tr1;
case 37:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr56;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr56;
	} else
		goto tr56;
	goto tr1;
case 38:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr57;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr57;
	} else
		goto tr57;
	goto tr1;
case 39:
	switch( (*p) ) {
		case 34: goto tr59;
		case 92: goto tr60;
	}
	goto tr58;
case 40:
	switch( (*p) ) {
		case 45: goto tr61;
		case 48: goto tr62;
	}
	if ( 49 <= (*p) && (*p) <= 57 )
		goto tr63;
	goto tr1;
case 41:
	if ( (*p) == 48 )
		goto tr64;
	if ( 49 <= (*p) && (*p) <= 57 )
		goto tr65;
	goto tr1;
case 42:
	switch( (*p) ) {
		case 46: goto tr66;
		case 115: goto tr67;
	}
	goto tr1;
case 43:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr68;
	goto tr1;
case 44:
	if ( (*p) == 115 )
		goto tr67;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr68;
	goto tr1;
case 45:
	if ( (*p) == 34 )
		goto tr69;
	goto tr1;
case 111:
	goto tr1;
case 46:
	switch( (*p) ) {
		case 46: goto tr66;
		case 115: goto tr67;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr65;
	goto tr1;
case 47:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr70;
	goto tr1;
case 48:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr71;
	goto tr1;
case 49:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr72;
	goto tr1;
case 50:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr73;
	goto tr1;
case 51:
	if ( (*p) == 45 )
		goto tr74;
	goto tr1;
case 52:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr75;
	goto tr1;
case 53:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr76;
	goto tr1;
case 54:
	if ( (*p) == 45 )
		goto tr77;
	goto tr1;
case 55:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr78;
	goto tr1;
case 56:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr79;
	goto tr1;
case 57:
	if ( (*p) == 84 )
		goto tr80;
	goto tr1;
case 58:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr81;
	goto tr1;
case 59:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr82;
	goto tr1;
case 60:
	if ( (*p) == 58 )
		goto tr83;
	goto tr1;
case 61:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr84;
	goto tr1;
case 62:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr85;
	goto tr1;
case 63:
	if ( (*p) == 58 )
		goto tr86;
	goto tr1;
case 64:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr87;
	goto tr1;
case 65:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr88;
	goto tr1;
case 66:
	switch( (*p) ) {
		case 43: goto tr89;
		case 45: goto tr89;
		case 46: goto tr90;
		case 90: goto tr91;
	}
	goto tr1;
case 67:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr92;
	goto tr1;
case 68:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr93;
	goto tr1;
case 69:
	if ( (*p) == 58 )
		goto tr94;
	goto tr1;
case 70:
	if ( (*p) == 48 )
		goto tr95;
	goto tr1;
case 71:
	if ( (*p) == 48 )
		goto tr96;
	goto tr1;
case 72:
	if ( (*p) == 34 )
		goto tr97;
	goto tr1;
case 112:
	goto tr1;
case 73:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr98;
	goto tr1;
case 74:
	switch( (*p) ) {
		case 43: goto tr99;
		case 45: goto tr99;
		case 90: goto tr100;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr98;
	goto tr1;
case 75:
	switch( (*p) ) {
		case 34: goto tr101;
		case 45: goto tr102;
		case 91: goto tr103;
		case 102: goto tr104;
		case 110: goto tr105;
		case 116: goto tr106;
		case 123: goto tr107;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr102;
	goto tr1;
case 76:
	if ( (*p) == 34 )
		goto tr108;
	goto tr1;
case 77:
	goto tr109;
case 113:
	goto tr1;
case 78:
	goto tr110;
case 79:
	switch( (*p) ) {
		case 32: goto tr112;
		case 93: goto tr113;
		case 125: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr112;
	goto tr111;
case 80:
	switch( (*p) ) {
		case 32: goto tr114;
		case 44: goto tr115;
		case 93: goto tr113;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr114;
	goto tr1;
case 81:
	switch( (*p) ) {
		case 32: goto tr115;
		case 93: goto tr1;
		case 125: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr115;
	goto tr111;
case 82:
	if ( (*p) == 97 )
		goto tr116;
	goto tr1;
case 83:
	if ( (*p) == 108 )
		goto tr117;
	goto tr1;
case 84:
	if ( (*p) == 115 )
		goto tr118;
	goto tr1;
case 85:
	if ( (*p) == 101 )
		goto tr119;
	goto tr1;
case 86:
	goto tr120;
case 87:
	if ( (*p) == 117 )
		goto tr121;
	goto tr1;
case 88:
	if ( (*p) == 108 )
		goto tr122;
	goto tr1;
case 89:
	if ( (*p) == 108 )
		goto tr123;
	goto tr1;
case 90:
	goto tr124;
case 91:
	if ( (*p) == 114 )
		goto tr125;
	goto tr1;
case 92:
	if ( (*p) == 117 )
		goto tr126;
	goto tr1;
case 93:
	if ( (*p) == 101 )
		goto tr127;
	goto tr1;
case 94:
	goto tr128;
case 95:
	switch( (*p) ) {
		case 32: goto tr129;
		case 34: goto tr130;
		case 125: goto tr131;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr129;
	goto tr1;
case 96:
	if ( (*p) == 34 )
		goto tr132;
	goto tr1;
case 97:
	switch( (*p) ) {
		case 32: goto tr133;
		case 58: goto tr134;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr133;
	goto tr1;
case 98:
	switch( (*p) ) {
		case 32: goto tr134;
		case 93: goto tr1;
		case 125: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr134;
	goto tr135;
case 99:
	switch( (*p) ) {
		case 32: goto tr136;
		case 44: goto tr137;
		case 125: goto tr138;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr136;
	goto tr1;
case 100:
	switch( (*p) ) {
		case 32: goto tr139;
		case 44: goto tr140;
		case 125: goto tr131;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr139;
	goto tr1;
case 101:
	switch( (*p) ) {
		case 32: goto tr140;
		case 34: goto tr130;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr140;
	goto tr1;
case 102:
	goto tr141;
	}

	tr1: cs = 0; goto _again;
	tr0: cs = 1; goto _again;
	tr2: cs = 2; goto f0;
	tr11: cs = 3; goto _again;
	tr4: cs = 3; goto f2;
	tr13: cs = 4; goto _again;
	tr10: cs = 4; goto f5;
	tr14: cs = 5; goto _again;
	tr5: cs = 6; goto _again;
	tr15: cs = 7; goto _again;
	tr16: cs = 8; goto _again;
	tr17: cs = 9; goto _again;
	tr6: cs = 10; goto _again;
	tr19: cs = 11; goto _again;
	tr20: cs = 12; goto _again;
	tr7: cs = 13; goto _again;
	tr22: cs = 14; goto _again;
	tr23: cs = 15; goto _again;
	tr25: cs = 16; goto _again;
	tr8: cs = 16; goto f3;
	tr26: cs = 17; goto f7;
	tr29: cs = 18; goto _again;
	tr28: cs = 18; goto f9;
	tr30: cs = 19; goto _again;
	tr31: cs = 20; goto f5;
	tr35: cs = 21; goto _again;
	tr32: cs = 21; goto f10;
	tr36: cs = 22; goto _again;
	tr33: cs = 22; goto f10;
	tr37: cs = 24; goto _again;
	tr38: cs = 25; goto _again;
	tr41: cs = 26; goto _again;
	tr43: cs = 27; goto _again;
	tr42: cs = 28; goto _again;
	tr44: cs = 29; goto _again;
	tr45: cs = 30; goto _again;
	tr39: cs = 31; goto _again;
	tr52: cs = 32; goto f18;
	tr49: cs = 33; goto _again;
	tr46: cs = 33; goto f14;
	tr58: cs = 33; goto f21;
	tr48: cs = 34; goto _again;
	tr51: cs = 34; goto f17;
	tr60: cs = 34; goto f23;
	tr53: cs = 35; goto _again;
	tr54: cs = 36; goto f19;
	tr55: cs = 37; goto f20;
	tr56: cs = 38; goto f20;
	tr57: cs = 39; goto f20;
	tr61: cs = 41; goto f24;
	tr64: cs = 42; goto _again;
	tr62: cs = 42; goto f24;
	tr66: cs = 43; goto _again;
	tr68: cs = 44; goto _again;
	tr67: cs = 45; goto f25;
	tr65: cs = 46; goto _again;
	tr63: cs = 46; goto f24;
	tr70: cs = 48; goto f27;
	tr71: cs = 49; goto _again;
	tr72: cs = 50; goto _again;
	tr73: cs = 51; goto _again;
	tr74: cs = 52; goto _again;
	tr75: cs = 53; goto _again;
	tr76: cs = 54; goto _again;
	tr77: cs = 55; goto _again;
	tr78: cs = 56; goto _again;
	tr79: cs = 57; goto _again;
	tr80: cs = 58; goto _again;
	tr81: cs = 59; goto _again;
	tr82: cs = 60; goto _again;
	tr83: cs = 61; goto _again;
	tr84: cs = 62; goto _again;
	tr85: cs = 63; goto _again;
	tr86: cs = 64; goto _again;
	tr87: cs = 65; goto _again;
	tr88: cs = 66; goto _again;
	tr89: cs = 67; goto f28;
	tr99: cs = 67; goto f31;
	tr92: cs = 68; goto _again;
	tr93: cs = 69; goto _again;
	tr94: cs = 70; goto _again;
	tr95: cs = 71; goto _again;
	tr96: cs = 72; goto _again;
	tr91: cs = 72; goto f28;
	tr100: cs = 72; goto f31;
	tr90: cs = 73; goto f29;
	tr98: cs = 74; goto _again;
	tr101: cs = 76; goto f0;
	tr108: cs = 77; goto f4;
	tr113: cs = 77; goto f6;
	tr102: cs = 78; goto f1;
	tr112: cs = 79; goto _again;
	tr103: cs = 79; goto f2;
	tr114: cs = 80; goto _again;
	tr111: cs = 80; goto f5;
	tr115: cs = 81; goto _again;
	tr104: cs = 82; goto _again;
	tr116: cs = 83; goto _again;
	tr117: cs = 84; goto _again;
	tr118: cs = 85; goto _again;
	tr119: cs = 86; goto _again;
	tr105: cs = 87; goto _again;
	tr121: cs = 88; goto _again;
	tr122: cs = 89; goto _again;
	tr123: cs = 90; goto _again;
	tr106: cs = 91; goto _again;
	tr125: cs = 92; goto _again;
	tr126: cs = 93; goto _again;
	tr127: cs = 94; goto _again;
	tr129: cs = 95; goto _again;
	tr107: cs = 95; goto f3;
	tr130: cs = 96; goto f7;
	tr133: cs = 97; goto _again;
	tr132: cs = 97; goto f9;
	tr134: cs = 98; goto _again;
	tr135: cs = 99; goto f5;
	tr139: cs = 100; goto _again;
	tr136: cs = 100; goto f10;
	tr140: cs = 101; goto _again;
	tr137: cs = 101; goto f10;
	tr131: cs = 102; goto f8;
	tr138: cs = 102; goto f11;
	tr142: cs = 103; goto _again;
	tr9: cs = 103; goto f4;
	tr12: cs = 103; goto f6;
	tr143: cs = 103; goto f38;
	tr144: cs = 103; goto f39;
	tr145: cs = 103; goto f40;
	tr146: cs = 103; goto f41;
	tr147: cs = 103; goto f42;
	tr3: cs = 104; goto f1;
	tr18: cs = 105; goto _again;
	tr21: cs = 106; goto _again;
	tr24: cs = 107; goto _again;
	tr27: cs = 108; goto f8;
	tr34: cs = 108; goto f11;
	tr40: cs = 109; goto f13;
	tr47: cs = 110; goto f15;
	tr50: cs = 110; goto f16;
	tr59: cs = 110; goto f22;
	tr69: cs = 111; goto f26;
	tr97: cs = 112; goto f30;
	tr109: cs = 113; goto f32;
	tr110: cs = 113; goto f33;
	tr120: cs = 113; goto f34;
	tr124: cs = 113; goto f35;
	tr128: cs = 113; goto f36;
	tr141: cs = 113; goto f37;

	f13: _acts = _json_actions + 3; goto execFuncs;
	f14: _acts = _json_actions + 5; goto execFuncs;
	f17: _acts = _json_actions + 7; goto execFuncs;
	f20: _acts = _json_actions + 9; goto execFuncs;
	f23: _acts = _json_actions + 11; goto execFuncs;
	f18: _acts = _json_actions + 13; goto execFuncs;
	f15: _acts = _json_actions + 15; goto execFuncs;
	f24: _acts = _json_actions + 17; goto execFuncs;
	f25: _acts = _json_actions + 19; goto execFuncs;
	f26: _acts = _json_actions + 21; goto execFuncs;
	f27: _acts = _json_actions + 23; goto execFuncs;
	f5: _acts = _json_actions + 25; goto execFuncs;
	f9: _acts = _json_actions + 27; goto execFuncs;
	f10: _acts = _json_actions + 29; goto execFuncs;
	f8: _acts = _json_actions + 31; goto execFuncs;
	f2: _acts = _json_actions + 33; goto execFuncs;
	f6: _acts = _json_actions + 35; goto execFuncs;
	f38: _acts = _json_actions + 37; goto execFuncs;
	f4: _acts = _json_actions + 39; goto execFuncs;
	f41: _acts = _json_actions + 41; goto execFuncs;
	f39: _acts = _json_actions + 43; goto execFuncs;
	f40: _acts = _json_actions + 45; goto execFuncs;
	f42: _acts = _json_actions + 47; goto execFuncs;
	f32: _acts = _json_actions + 49; goto execFuncs;
	f16: _acts = _json_actions + 51; goto execFuncs;
	f19: _acts = _json_actions + 54; goto execFuncs;
	f21: _acts = _json_actions + 57; goto execFuncs;
	f22: _acts = _json_actions + 60; goto execFuncs;
	f29: _acts = _json_actions + 63; goto execFuncs;
	f31: _acts = _json_actions + 66; goto execFuncs;
	f30: _acts = _json_actions + 69; goto execFuncs;
	f7: _acts = _json_actions + 72; goto execFuncs;
	f11: _acts = _json_actions + 75; goto execFuncs;
	f1: _acts = _json_actions + 78; goto execFuncs;
	f33: _acts = _json_actions + 81; goto execFuncs;
	f0: _acts = _json_actions + 84; goto execFuncs;
	f36: _acts = _json_actions + 87; goto execFuncs;
	f34: _acts = _json_actions + 90; goto execFuncs;
	f35: _acts = _json_actions + 93; goto execFuncs;
	f3: _acts = _json_actions + 96; goto execFuncs;
	f37: _acts = _json_actions + 99; goto execFuncs;
	f28: _acts = _json_actions + 102; goto execFuncs;

execFuncs:
	_nacts = *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
#line 2427 "upb/json/parser.rl"
	{ p--; {cs = stack[--top];goto _again;} }
	break;
	case 2:
#line 2429 "upb/json/parser.rl"
	{ p--; {stack[top++] = cs; cs = 23; goto _again;} }
	break;
	case 3:
#line 2433 "upb/json/parser.rl"
	{ start_text(parser, p); }
	break;
	case 4:
#line 2434 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_text(parser, p)); }
	break;
	case 5:
#line 2440 "upb/json/parser.rl"
	{ start_hex(parser); }
	break;
	case 6:
#line 2441 "upb/json/parser.rl"
	{ hexdigit(parser, p); }
	break;
	case 7:
#line 2442 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_hex(parser)); }
	break;
	case 8:
#line 2448 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(escape(parser, p)); }
	break;
	case 9:
#line 2454 "upb/json/parser.rl"
	{ p--; {cs = stack[--top];goto _again;} }
	break;
	case 10:
#line 2466 "upb/json/parser.rl"
	{ start_duration_base(parser, p); }
	break;
	case 11:
#line 2467 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_duration_base(parser, p)); }
	break;
	case 12:
#line 2469 "upb/json/parser.rl"
	{ p--; {cs = stack[--top];goto _again;} }
	break;
	case 13:
#line 2474 "upb/json/parser.rl"
	{ start_timestamp_base(parser, p); }
	break;
	case 14:
#line 2475 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_timestamp_base(parser, p)); }
	break;
	case 15:
#line 2477 "upb/json/parser.rl"
	{ start_timestamp_fraction(parser, p); }
	break;
	case 16:
#line 2478 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_timestamp_fraction(parser, p)); }
	break;
	case 17:
#line 2480 "upb/json/parser.rl"
	{ start_timestamp_zone(parser, p); }
	break;
	case 18:
#line 2481 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_timestamp_zone(parser, p)); }
	break;
	case 19:
#line 2483 "upb/json/parser.rl"
	{ p--; {cs = stack[--top];goto _again;} }
	break;
	case 20:
#line 2488 "upb/json/parser.rl"
	{
        if (is_wellknown_msg(parser, UPB_WELLKNOWN_TIMESTAMP)) {
          {stack[top++] = cs; cs = 47; goto _again;}
        } else if (is_wellknown_msg(parser, UPB_WELLKNOWN_DURATION)) {
          {stack[top++] = cs; cs = 40; goto _again;}
        } else {
          {stack[top++] = cs; cs = 32; goto _again;}
        }
      }
	break;
	case 21:
#line 2499 "upb/json/parser.rl"
	{ p--; {stack[top++] = cs; cs = 75; goto _again;} }
	break;
	case 22:
#line 2504 "upb/json/parser.rl"
	{
        if (is_wellknown_msg(parser, UPB_WELLKNOWN_ANY)) {
          start_any_member(parser, p);
        } else {
          start_member(parser);
        }
      }
	break;
	case 23:
#line 2511 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_membername(parser)); }
	break;
	case 24:
#line 2514 "upb/json/parser.rl"
	{
        if (is_wellknown_msg(parser, UPB_WELLKNOWN_ANY)) {
          end_any_member(parser, p);
        } else {
          end_member(parser);
        }
      }
	break;
	case 25:
#line 2525 "upb/json/parser.rl"
	{
        if (is_wellknown_msg(parser, UPB_WELLKNOWN_ANY)) {
          start_any_object(parser, p);
        } else {
          start_object(parser);
        }
      }
	break;
	case 26:
#line 2534 "upb/json/parser.rl"
	{
        if (is_wellknown_msg(parser, UPB_WELLKNOWN_ANY)) {
          CHECK_RETURN_TOP(end_any_object(parser, p));
        } else {
          end_object(parser);
        }
      }
	break;
	case 27:
#line 2546 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(start_array(parser)); }
	break;
	case 28:
#line 2550 "upb/json/parser.rl"
	{ end_array(parser); }
	break;
	case 29:
#line 2555 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(start_number(parser, p)); }
	break;
	case 30:
#line 2556 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_number(parser, p)); }
	break;
	case 31:
#line 2558 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(start_stringval(parser)); }
	break;
	case 32:
#line 2559 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_stringval(parser)); }
	break;
	case 33:
#line 2561 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_bool(parser, true)); }
	break;
	case 34:
#line 2563 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_bool(parser, false)); }
	break;
	case 35:
#line 2565 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_null(parser)); }
	break;
	case 36:
#line 2567 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(start_subobject_full(parser)); }
	break;
	case 37:
#line 2568 "upb/json/parser.rl"
	{ end_subobject_full(parser); }
	break;
	case 38:
#line 2573 "upb/json/parser.rl"
	{ p--; {cs = stack[--top];goto _again;} }
	break;
#line 3520 "upb/json/parser.c"
		}
	}
	goto _again;

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _json_actions + _json_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
#line 2425 "upb/json/parser.rl"
	{ p--; {cs = stack[--top];goto _again;} }
	break;
	case 30:
#line 2556 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_number(parser, p)); }
	break;
	case 33:
#line 2561 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_bool(parser, true)); }
	break;
	case 34:
#line 2563 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_bool(parser, false)); }
	break;
	case 35:
#line 2565 "upb/json/parser.rl"
	{ CHECK_RETURN_TOP(end_null(parser)); }
	break;
	case 37:
#line 2568 "upb/json/parser.rl"
	{ end_subobject_full(parser); }
	break;
#line 3561 "upb/json/parser.c"
		}
	}
	}

	_out: {}
	}

#line 2601 "upb/json/parser.rl"

  if (p != pe) {
    upb_status_seterrf(&parser->status, "Parse error at '%.*s'\n", pe - p, p);
    upb_env_reporterror(parser->env, &parser->status);
  } else {
    capture_suspend(parser, &p);
  }

error:
  /* Save parsing state back to parser. */
  parser->current_state = cs;
  parser->parser_top = top;

  return p - buf;
}

static bool end(void *closure, const void *hd) {
  upb_json_parser *parser = closure;

  /* Prevent compile warning on unused static constants. */
  UPB_UNUSED(json_start);
  UPB_UNUSED(json_en_duration_machine);
  UPB_UNUSED(json_en_number_machine);
  UPB_UNUSED(json_en_string_machine);
  UPB_UNUSED(json_en_timestamp_machine);
  UPB_UNUSED(json_en_value_machine);
  UPB_UNUSED(json_en_main);

  parse(parser, hd, &eof_ch, 0, NULL);

  return parser->current_state >= 
#line 3601 "upb/json/parser.c"
103
#line 2631 "upb/json/parser.rl"
;
}

static void json_parser_reset(upb_json_parser *p) {
  int cs;
  int top;

  p->top = p->stack;
  p->top->f = NULL;
  p->top->is_map = false;
  p->top->is_mapentry = false;
  p->top->is_any = false;
  p->top->any_frame = NULL;
  p->top->is_unknown_field = false;

  /* Emit Ragel initialization of the parser. */
  
#line 3621 "upb/json/parser.c"
	{
	cs = json_start;
	top = 0;
	}

#line 2648 "upb/json/parser.rl"
  p->current_state = cs;
  p->parser_top = top;
  accumulate_clear(p);
  p->multipart_state = MULTIPART_INACTIVE;
  p->capture = NULL;
  p->accumulated = NULL;
  upb_status_clear(&p->status);
}

static void visit_json_parsermethod(const upb_refcounted *r,
                                    upb_refcounted_visit *visit,
                                    void *closure) {
  const upb_json_parsermethod *method = (upb_json_parsermethod*)r;
  visit(r, upb_msgdef_upcast2(method->msg), closure);
}

static void free_json_parsermethod(upb_refcounted *r) {
  upb_json_parsermethod *method = (upb_json_parsermethod*)r;

  upb_inttable_iter i;
  upb_inttable_begin(&i, &method->name_tables);
  for(; !upb_inttable_done(&i); upb_inttable_next(&i)) {
    upb_value val = upb_inttable_iter_value(&i);
    upb_strtable *t = upb_value_getptr(val);
    upb_strtable_uninit(t);
    upb_gfree(t);
  }

  upb_inttable_uninit(&method->name_tables);

  upb_gfree(r);
}

static void add_jsonname_table(upb_json_parsermethod *m, const upb_msgdef* md) {
  upb_msg_field_iter i;
  upb_strtable *t;

  /* It would be nice to stack-allocate this, but protobufs do not limit the
   * length of fields to any reasonable limit. */
  char *buf = NULL;
  size_t len = 0;

  if (upb_inttable_lookupptr(&m->name_tables, md, NULL)) {
    return;
  }

  /* TODO(haberman): handle malloc failure. */
  t = upb_gmalloc(sizeof(*t));
  upb_strtable_init(t, UPB_CTYPE_CONSTPTR);
  upb_inttable_insertptr(&m->name_tables, md, upb_value_ptr(t));

  for(upb_msg_field_begin(&i, md);
      !upb_msg_field_done(&i);
      upb_msg_field_next(&i)) {
    const upb_fielddef *f = upb_msg_iter_field(&i);

    /* Add an entry for the JSON name. */
    size_t field_len = upb_fielddef_getjsonname(f, buf, len);
    if (field_len > len) {
      size_t len2;
      buf = upb_grealloc(buf, 0, field_len);
      len = field_len;
      len2 = upb_fielddef_getjsonname(f, buf, len);
      UPB_ASSERT(len == len2);
    }
    upb_strtable_insert(t, buf, upb_value_constptr(f));

    if (strcmp(buf, upb_fielddef_name(f)) != 0) {
      /* Since the JSON name is different from the regular field name, add an
       * entry for the raw name (compliant proto3 JSON parsers must accept
       * both). */
      upb_strtable_insert(t, upb_fielddef_name(f), upb_value_constptr(f));
    }

    if (upb_fielddef_issubmsg(f)) {
      add_jsonname_table(m, upb_fielddef_msgsubdef(f));
    }
  }

  upb_gfree(buf);
}

/* Public API *****************************************************************/

upb_json_parser *upb_json_parser_create(upb_env *env,
                                        const upb_json_parsermethod *method,
                                        const upb_symtab* symtab,
                                        upb_sink *output,
                                        bool ignore_json_unknown) {
#ifndef NDEBUG
  const size_t size_before = upb_env_bytesallocated(env);
#endif
  upb_json_parser *p = upb_env_malloc(env, sizeof(upb_json_parser));
  if (!p) return false;

  p->env = env;
  p->method = method;
  p->limit = p->stack + UPB_JSON_MAX_DEPTH;
  p->accumulate_buf = NULL;
  p->accumulate_buf_size = 0;
  upb_bytessink_reset(&p->input_, &method->input_handler_, p);

  json_parser_reset(p);
  upb_sink_reset(&p->top->sink, output->handlers, output->closure);
  p->top->m = upb_handlers_msgdef(output->handlers);
  if (is_wellknown_msg(p, UPB_WELLKNOWN_ANY)) {
    p->top->is_any = true;
    p->top->any_frame =
        upb_env_malloc(p->env, sizeof(upb_jsonparser_any_frame));
    json_parser_any_frame_reset(p->top->any_frame);
  } else {
    p->top->is_any = false;
    p->top->any_frame = NULL;
  }
  set_name_table(p, p->top);
  p->symtab = symtab;

  p->ignore_json_unknown = ignore_json_unknown;

  /* If this fails, uncomment and increase the value in parser.h. */
  /* fprintf(stderr, "%zd\n", upb_env_bytesallocated(env) - size_before); */
  UPB_ASSERT_DEBUGVAR(upb_env_bytesallocated(env) - size_before <=
                      UPB_JSON_PARSER_SIZE);
  return p;
}

upb_bytessink *upb_json_parser_input(upb_json_parser *p) {
  return &p->input_;
}

upb_json_parsermethod *upb_json_parsermethod_new(const upb_msgdef* md,
                                                 const void* owner) {
  static const struct upb_refcounted_vtbl vtbl = {visit_json_parsermethod,
                                                  free_json_parsermethod};
  upb_json_parsermethod *ret = upb_gmalloc(sizeof(*ret));
  upb_refcounted_init(upb_json_parsermethod_upcast_mutable(ret), &vtbl, owner);

  ret->msg = md;
  upb_ref2(md, ret);

  upb_byteshandler_init(&ret->input_handler_);
  upb_byteshandler_setstring(&ret->input_handler_, parse, ret);
  upb_byteshandler_setendstr(&ret->input_handler_, end, ret);

  upb_inttable_init(&ret->name_tables, UPB_CTYPE_PTR);

  add_jsonname_table(ret, md);

  return ret;
}

const upb_byteshandler *upb_json_parsermethod_inputhandler(
    const upb_json_parsermethod *m) {
  return &m->input_handler_;
}
