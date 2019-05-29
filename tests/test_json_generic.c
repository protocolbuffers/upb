
#include <stdio.h>
#include <stdlib.h>

#include "upb/json.h"

static char *upb_readfile(const char *filename, size_t *len) {
  long size;
  char *buf;
  FILE *f = fopen(filename, "rb");
  if(!f) return NULL;
  if(fseek(f, 0, SEEK_END) != 0) goto error;
  size = ftell(f);
  if(size < 0) goto error;
  if(fseek(f, 0, SEEK_SET) != 0) goto error;
  buf = (char*)malloc(size + 1);
  if(size && fread(buf, size, 1, f) != 1) goto error;
  fclose(f);
  if (len) *len = size;
  buf[size] = '\0';
  return buf;

error:
  fclose(f);
  return NULL;
}

int main(int argc, char* argv[]) {
  char* data;
  char* parsed_data;
  char* ptr;
  char* end;
  size_t len;
  size_t parsed_len;
  upb_status status;

  if (argc < 2) {
    printf("Usage: test_json <test filename>\n");
    return -1;
  }

  data = upb_readfile(argv[1], &len);

  if (!data) {
    printf("Error opening file: %s\n", argv[1]);
    return 2;
  }

  printf("Read %d bytes from file '%s'\n", (int)len, argv[1]);
  upb_status_clear(&status);
  parsed_data = _parse_json_stage1(data, len, 64, &upb_alloc_global,
                                   &parsed_len, &status);

  if (!parsed_data) {
    printf("Parse error.\n");
    return 1;
  }

  printf("Parse succeeded, output %d bytes\n", (int)parsed_len);

  ptr = parsed_data;
  end = parsed_data + parsed_len;

  while(ptr < end) {
    char ch = *ptr;
    ptr++;
    switch (ch) {
      case kEnd:
        printf("kEnd ");
        break;
      case kObject:
        printf("kObject ");
        break;
      case kArray:
        printf("kArray ");
        break;
      case kNumber: {
        double d;
        memcpy(&d, ptr, 8);
        ptr += 8;
        printf("%f ", d);
        break;
      }
      case kString: {
        int len;
        memcpy(&len, ptr, 4);
        ptr += 4;
        printf("\"%.*s\" ", len, ptr);
        ptr += len;
        break;
      }
      case kTrue:
        printf("kTrue ");
        break;
      case kFalse:
        printf("kFalse ");
        break;
      case kNull:
        printf("kNull ");
        break;
      default:
        printf("Bad char from JSON parser: %d\n", (int)*ptr);
        return 3;
    }
  }

  printf("\n");

  return 0;
}
