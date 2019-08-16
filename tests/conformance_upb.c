/* This is a upb implementation of the upb conformance tests, see:
 *   https://github.com/google/protobuf/tree/master/conformance
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>

#include "conformance/conformance.upb.h"
#include "src/google/protobuf/test_messages_proto3.upb.h"
#include "src/google/protobuf/test_messages_proto3.upbdefs.h"
#include "upb/json.h"

int test_count = 0;

bool CheckedRead(int fd, void *buf, size_t len) {
  size_t ofs = 0;
  while (len > 0) {
    ssize_t bytes_read = read(fd, (char*)buf + ofs, len);

    if (bytes_read == 0) return false;

    if (bytes_read < 0) {
      perror("reading from test runner");
      exit(1);
    }

    len -= bytes_read;
    ofs += bytes_read;
  }

  return true;
}

void CheckedWrite(int fd, const void *buf, size_t len) {
  if ((size_t)write(fd, buf, len) != len) {
    perror("writing to test runner");
    exit(1);
  }
}

bool strview_eql(upb_strview view, const char *str) {
  return view.size == strlen(str) && memcmp(view.data, str, view.size) == 0;
}

/* Stringify "str" to ensure it is static storage duration and will outlive. */
#define SETERR(msg, err, str) \
  conformance_ConformanceResponse_set_##err(msg, upb_strview_makez(str))

void handler(int sig) {
  void *array[10];
  size_t size;

  size = backtrace(array, 10);
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

static const char *proto3_msg =
    "protobuf_test_messages.proto3.TestAllTypesProto3";

#include <ctype.h>
void DoTest(
    const conformance_ConformanceRequest* request,
    conformance_ConformanceResponse *response,
    upb_symtab *symtab,
    upb_arena *arena) {
  protobuf_test_messages_proto3_TestAllTypesProto3 *test_message;
  upb_strview name = conformance_ConformanceRequest_message_type(request);
  const upb_msgdef *m = upb_symtab_lookupmsg2(symtab, name.data, name.size);
  upb_alloc *alloc = upb_arena_alloc(arena);
  upb_status status;

  upb_status_clear(&status);

  if (!m || !strview_eql(name, proto3_msg)) {
    SETERR(response, skipped, "Only proto3 for now.");
    return;
  }

  switch (conformance_ConformanceRequest_payload_case(request)) {
    case conformance_ConformanceRequest_payload_protobuf_payload: {
      upb_strview payload;

      payload = conformance_ConformanceRequest_protobuf_payload(request);
      test_message = protobuf_test_messages_proto3_TestAllTypesProto3_parse(
          payload.data, payload.size, arena);

      if (!test_message) {
        SETERR(response, parse_error, "Error parsing proto input.");
        return;
      }
      break;
    }

    case conformance_ConformanceRequest_payload_json_payload: {
      upb_strview json = conformance_ConformanceRequest_json_payload(request);
      int options = conformance_ConformanceRequest_test_category(request) ==
                            conformance_JSON_IGNORE_UNKNOWN_PARSING_TEST
                        ? UPB_JSON_IGNORE_UNKNOWN
                        : 0;
      size_t bin_size;
      char *bin_buf;

      bin_buf = upb_jsontobinary(json.data, json.size, m, symtab, options, 32,
                                 alloc, &bin_size, &status);

      if (!bin_buf) {
        SETERR(response, parse_error,
               upb_strdup(upb_status_errmsg(&status), alloc));
        return;
      }

      test_message = protobuf_test_messages_proto3_TestAllTypesProto3_parse(
          bin_buf, bin_size, arena);

      if (!test_message) {
        SETERR(response, parse_error, "Error parsing protobuf from JSON.");
        return;
      }

      break;
    }

    case conformance_ConformanceRequest_payload_NOT_SET:
      fprintf(stderr, "conformance_upb: Request didn't have payload.\n");
      return;

    default: {
      SETERR(response, skipped, "Unsupported input format.");
      return;
    }
  }

  switch (conformance_ConformanceRequest_requested_output_format(request)) {
    case conformance_UNSPECIFIED:
      fprintf(stderr, "conformance_upb: Unspecified output format.\n");
      exit(1);

    case conformance_PROTOBUF: {
      size_t bin_len;
      char *bin_buf;

      bin_buf = protobuf_test_messages_proto3_TestAllTypesProto3_serialize(
          test_message, arena, &bin_len);

      if (!bin_buf) {
        SETERR(response, serialize_error, "Error serializing to binary.");
        return;
      }

      conformance_ConformanceResponse_set_protobuf_payload(
          response, upb_strview_make(bin_buf, bin_len));
      break;
    }

      /*
    case conformance_JSON: {
      char *json_buf;
      size_t json_size;
      size_t bin_len;
      char *bin_buf;

      bin_buf = protobuf_test_messages_proto3_TestAllTypesProto3_serialize(
          test_message, arena, &bin_len);

      if (!bin_buf) {
        SETERR(response, serialize_error, "Error serializing to binary.");
        return;
      }

      json_buf = upb_binarytojson(bin_buf, bin_len, m, 0, alloc, &json_size);

      if (!json_buf) {
        SETERR(response, serialize_error, "Error serializing to JSON.");
        return;
      }

      conformance_ConformanceResponse_set_protobuf_payload(
          response, upb_strview_make(json_buf, json_size));
      break;
    }*/

    default: {
      SETERR(response, skipped, "Unsupported output format.");
      return;
    }
  }

  return;
}

bool DoTestIo(upb_symtab *symtab) {
  upb_arena *arena;
  upb_alloc *alloc;
  upb_status status;
  char *serialized_input;
  char *serialized_output;
  uint32_t input_size;
  size_t output_size;
  conformance_ConformanceRequest *request;
  conformance_ConformanceResponse *response;

  if (!CheckedRead(STDIN_FILENO, &input_size, sizeof(uint32_t))) {
    /* EOF. */
    return false;
  }

  arena = upb_arena_new();
  alloc = upb_arena_alloc(arena);
  serialized_input = upb_malloc(alloc, input_size);

  if (!CheckedRead(STDIN_FILENO, serialized_input, input_size)) {
    fprintf(stderr, "conformance_upb: unexpected EOF on stdin.\n");
    exit(1);
  }

  request =
      conformance_ConformanceRequest_parse(serialized_input, input_size, arena);
  response = conformance_ConformanceResponse_new(arena);

  if (request) {
    DoTest(request, response, symtab, arena);
  } else {
    fprintf(stderr, "conformance_upb: parse of ConformanceRequest failed: %s\n",
            upb_status_errmsg(&status));
  }

  serialized_output = conformance_ConformanceResponse_serialize(
      response, arena, &output_size);

  CheckedWrite(STDOUT_FILENO, &output_size, sizeof(uint32_t));
  CheckedWrite(STDOUT_FILENO, serialized_output, output_size);

  test_count++;

  upb_arena_free(arena);

  return true;
}

int main(void) {
  upb_symtab *symtab = upb_symtab_new();

  protobuf_test_messages_proto3_TestAllTypesProto3_getmsgdef(symtab);
  signal(SIGSEGV, handler);

  while (1) {
    if (!DoTestIo(symtab)) {
      fprintf(stderr, "conformance_upb: received EOF from test runner "
                      "after %d tests, exiting\n", test_count);
      break;
    }
  }

  upb_symtab_free(symtab);
  return 0;
}
