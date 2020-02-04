
#include <string.h>
#include "upb/upb.h"
#include "upb/decode.h"

#include "upb/port_def.inc"

/* Maps descriptor type -> upb field type.  */
static const uint8_t desctype_to_fieldtype[] = {
    -1,               /* invalid descriptor type */
    UPB_TYPE_DOUBLE,  /* DOUBLE */
    UPB_TYPE_FLOAT,   /* FLOAT */
    UPB_TYPE_INT64,   /* INT64 */
    UPB_TYPE_UINT64,  /* UINT64 */
    UPB_TYPE_INT32,   /* INT32 */
    UPB_TYPE_UINT64,  /* FIXED64 */
    UPB_TYPE_UINT32,  /* FIXED32 */
    UPB_TYPE_BOOL,    /* BOOL */
    UPB_TYPE_STRING,  /* STRING */
    UPB_TYPE_MESSAGE, /* GROUP */
    UPB_TYPE_MESSAGE, /* MESSAGE */
    UPB_TYPE_BYTES,   /* BYTES */
    UPB_TYPE_UINT32,  /* UINT32 */
    UPB_TYPE_ENUM,    /* ENUM */
    UPB_TYPE_INT32,   /* SFIXED32 */
    UPB_TYPE_INT64,   /* SFIXED64 */
    UPB_TYPE_INT32,   /* SINT32 */
    UPB_TYPE_INT64,   /* SINT64 */
};

/* Maps descriptor type -> upb map size.  */
static const uint8_t desctype_to_mapsize[] = {
    -1,                 /* invalid descriptor type */
    8,                  /* DOUBLE */
    4,                  /* FLOAT */
    8,                  /* INT64 */
    8,                  /* UINT64 */
    4,                  /* INT32 */
    8,                  /* FIXED64 */
    4,                  /* FIXED32 */
    1,                  /* BOOL */
    UPB_MAPTYPE_STRING, /* STRING */
    sizeof(void *),     /* GROUP */
    sizeof(void *),     /* MESSAGE */
    UPB_MAPTYPE_STRING, /* BYTES */
    4,                  /* UINT32 */
    4,                  /* ENUM */
    4,                  /* SFIXED32 */
    8,                  /* SFIXED64 */
    4,                  /* SINT32 */
    8,                  /* SINT64 */
};

/* Data pertaining to the parse. */
typedef struct {
  const char *field_start;   /* Start of this field. */
  const char *limit;         /* End of delimited region or end of buffer. */
  upb_arena *arena;
  int depth;
  uint32_t end_group;  /* Set to field number of END_GROUP tag, if any. */
} upb_decstate;

typedef union {
  bool bool_val;
  int32_t int32_val;
  int64_t int64_val;
  uint32_t uint32_val;
  uint64_t uint64_val;
  upb_strview str_val;
} wireval;

#define CHK(x) if (!(x)) { return 0; }
#define PTR_AT(msg, ofs, type) (type*)((const char*)(msg) + (ofs))

static const char *decode_msg(upb_decstate *d, const char *ptr, upb_msg *msg,
                              const upb_msglayout *layout);

UPB_NOINLINE
static const char *decode_longvarint64(const char *ptr, const char *limit,
                                       uint64_t *val) {
  uint8_t byte;
  int bitpos = 0;
  uint64_t out = 0;

  do {
    CHK(bitpos < 70 && ptr < limit);
    byte = *ptr;
    out |= (uint64_t)(byte & 0x7F) << bitpos;
    ptr++;
    bitpos += 7;
  } while (byte & 0x80);

  *val = out;
  return ptr;
}

UPB_FORCEINLINE
static const char *decode_varint64(const char *ptr, const char *limit,
                                   uint64_t *val) {
  if (UPB_LIKELY(ptr < limit && (*ptr & 0x80) == 0)) {
    *val = (uint8_t)*ptr;
    return ptr + 1;
  } else {
    return decode_longvarint64(ptr, limit, val);
  }
}

static const char *decode_varint32(const char *ptr, const char *limit,
                                   uint32_t *val) {
  uint64_t u64;
  CHK(ptr = decode_varint64(ptr, limit, &u64))
  CHK(u64 <= UINT32_MAX);
  *val = (uint32_t)u64;
  return ptr;
}

static void decode_munge(int type, wireval* val) {
  switch (type) {
    case UPB_DESCRIPTOR_TYPE_BOOL:
      val->bool_val = val->uint64_val != 0;
      break;
    case UPB_DESCRIPTOR_TYPE_SINT32: {
      uint32_t n = val->uint32_val;
      val->int32_val = (n >> 1) ^ -(int32_t)(n & 1);
      break;
    }
    case UPB_DESCRIPTOR_TYPE_SINT64: {
      uint64_t n = val->uint32_val;
      val->int64_val = (n >> 1) ^ -(int64_t)(n & 1);
      break;
    }
  }
}

static const upb_msglayout_field *upb_find_field(const upb_msglayout *l,
                                                 uint32_t field_number) {
  static upb_msglayout_field none = {0};

  /* Lots of optimization opportunities here. */
  int i;
  if (l == NULL) return &none;
  for (i = 0; i < l->field_count; i++) {
    if (l->fields[i].number == field_number) {
      return &l->fields[i];
    }
  }

  return &none;  /* Unknown field. */
}

static upb_msg *decode_newsubmsg(upb_decstate *d, const upb_msglayout *layout,
                                 const upb_msglayout_field *field) {
  const upb_msglayout *subl = layout->submsgs[field->submsg_index];
  return _upb_msg_new(subl, d->arena);
}

static bool decode_tosubmsg(upb_decstate *d, upb_msg *submsg,
                            const upb_msglayout *layout,
                            const upb_msglayout_field *field, upb_strview val) {
  const upb_msglayout *subl = layout->submsgs[field->submsg_index];
  const char* saved_limit = d->limit;
  CHK(--d->depth >= 0);
  d->limit = val.data + val.size;
  CHK(decode_msg(d, val.data, submsg, subl));
  d->limit = saved_limit;
  CHK(d->end_group == 0);
  d->depth++;
  return true;
}

static const char *decode_togroup(upb_decstate *d, const char *ptr,
                                  upb_msg *submsg, const upb_msglayout *layout,
                                  const upb_msglayout_field *field) {
  const upb_msglayout *subl = layout->submsgs[field->submsg_index];
  CHK(--d->depth >= 0);
  CHK(ptr = decode_msg(d, ptr, submsg, subl));
  CHK(d->end_group == field->number);
  d->depth++;
  return ptr;
}

static const char *decode_toarray(upb_decstate *d, const char *ptr,
                                  upb_msg *msg, const upb_msglayout *layout,
                                  const upb_msglayout_field *field, wireval val,
                                  int action) {
  upb_array **arrp = PTR_AT(msg, field->offset, void);
  upb_array *arr = *arrp;
  void *mem;

  if (!arr) {
    upb_fieldtype_t type = desctype_to_fieldtype[field->descriptortype];
    arr = _upb_array_new(d->arena, type);
    CHK(arr);
    *arrp = arr;
  }

  if (arr->len == arr->size) {
    CHK(_upb_array_realloc(arr, arr->len + 1, d->arena));
  }

  switch (action) {
    case 0:
    case 2:
    case 3:
      /* Append scalar value. */
      mem = PTR_AT(_upb_array_ptr(arr), arr->len << action, void);
      arr->len++;
      memcpy(mem, &val, 1 << action);
      return ptr;
    case 4:
      /* Append string. */
      mem = PTR_AT(_upb_array_ptr(arr), arr->len * sizeof(upb_strview), void);
      arr->len++;
      memcpy(mem, &val, sizeof(upb_strview));
      return ptr;
    case 5: {
      /* Append submessage / group. */
      upb_msg *submsg = decode_newsubmsg(d, layout, field);
      *PTR_AT(_upb_array_ptr(arr), arr->len * sizeof(void*), upb_msg*) = submsg;
      arr->len++;
      if (UPB_UNLIKELY(field->descriptortype == UPB_DTYPE_GROUP)) {
        CHK(ptr = decode_togroup(d, ptr, submsg, layout, field));
      } else {
        CHK(decode_tosubmsg(d, submsg, layout, field, val.str_val));
      }
      return ptr;
    }
    case 6:
    case 7: {
      /* Fixed packed. */
      int lg2 = action - 4;
      int mask = (1 << lg2) - 1;
      int count = val.str_val.size >> lg2;
      CHK((val.str_val.size & mask) == 0);
      if ((arr->size - arr->len) < count) {
        CHK(_upb_array_realloc(arr, arr->len + count, d->arena));
      }
      mem = PTR_AT(_upb_array_ptr(arr), arr->len << lg2, void);
      arr->len += count;
      memcpy(mem, &val, count << action);
      return ptr;
    }
    case 8:
    case 10:
    case 11: {
      /* Varint packed. */
      int lg2 = action - 10;
      int scale = 1 << lg2;
      const char *ptr = val.str_val.data;
      const char *end = ptr + val.str_val.size;
      char *out = PTR_AT(_upb_array_ptr(arr), arr->len << lg2, void);
      while (ptr < end) {
        wireval elem;
        CHK(ptr = decode_varint64(ptr, end, &elem.uint64_val));
        decode_munge(field->descriptortype, &elem);
        if (arr->len == arr->size) {
          CHK(_upb_array_realloc(arr, arr->len + 1, d->arena));
          out = PTR_AT(_upb_array_ptr(arr), arr->len << lg2, void);
        }
        arr->len++;
        memcpy(out, &elem, scale);
        out += scale;
      }
      CHK(ptr == end);
      return ptr;
    }
    default:
      UPB_UNREACHABLE();
}
}

static bool decode_tomap(upb_decstate *d, upb_msg *msg,
                         const upb_msglayout *layout,
                         const upb_msglayout_field *field, wireval val) {
  upb_map **map_p = PTR_AT(msg, field->offset, upb_map*);
  upb_map *map = *map_p;
  upb_map_entry ent;

  if (!map) {
    /* Lazily create map. */
    const upb_msglayout *entry = layout->submsgs[field->submsg_index];
    const upb_msglayout_field *key_field = &entry->fields[0];
    const upb_msglayout_field *val_field = &entry->fields[1];
    char key_size = desctype_to_mapsize[key_field->descriptortype];
    char val_size = desctype_to_mapsize[val_field->descriptortype];
    UPB_ASSERT(key_field->offset == 0);
    UPB_ASSERT(val_field->offset == sizeof(upb_strview));
    map = _upb_map_new(d->arena, key_size, val_size);
    *map_p = map;
  }

  /* Parse map entry. */
  memset(&ent, 0, sizeof(ent));
  CHK(decode_tosubmsg(d, &ent, layout, field, val.str_val));

  /* Insert into map. */
  _upb_map_set(map, &ent.k, map->key_size, &ent.v, map->val_size, d->arena);
  return true;
}

static const char *decode_tomsg(upb_decstate *d, const char *ptr, upb_msg *msg,
                                const upb_msglayout *layout,
                                const upb_msglayout_field *field, wireval val,
                                int action) {
  void *mem = PTR_AT(msg, field->offset, void);
  int presence = field->presence;
  int type = field->descriptortype;

  /* Set presence if necessary. */
  if (presence < 0) {
    *PTR_AT(msg, presence, int32_t) = field->number;  /* Oneof case */
  } else if (presence > 0) {
    uint32_t hasbit = presence;
    *PTR_AT(msg, hasbit / 32, uint32_t) |= (1 << (hasbit % 32));  /* Hasbit */
  }

  decode_munge(type, &val);

  /* Store into message. */
  switch (action) {
    case 5: {
      upb_msg **submsgp = mem;
      upb_msg *submsg = *submsgp;
      if (!submsg) {
        submsg = decode_newsubmsg(d, layout, field);
        *submsgp = submsg;
      }
      if (UPB_UNLIKELY(type == UPB_DTYPE_GROUP)) {
        CHK(ptr = decode_togroup(d, ptr, submsg, layout, field));
      } else {
        CHK(decode_tosubmsg(d, submsg, layout, field, val.str_val));
      }
      return ptr;
    }
    case 4:
      memcpy(mem, &val, sizeof(upb_strview));
      return ptr;
    case 3:
    case 2:
    case 0:
      memcpy(mem, &val, 1 << action);
      return ptr;
    default:
      UPB_UNREACHABLE();
  }
}

static const int8_t varint_actions[19] = {
    -1, /* field not found */
    -1, /* DOUBLE */
    -1, /* FLOAT */
    3,  /* INT64 */
    3,  /* UINT64 */
    2,  /* INT32 */
    -1, /* FIXED64 */
    -1, /* FIXED32 */
    0,  /* BOOL */
    -1, /* STRING */
    -1, /* GROUP */
    -1, /* MESSAGE */
    -1, /* BYTES */
    2,  /* UINT32 */
    2,  /* ENUM */
    -1, /* SFIXED32 */
    -1,  /* SFIXED64 */
    2,  /* SINT32 */
    3,  /* SINT64 */
};

static const int8_t delim_actions[37] = {
    -1, /* field not found */
    -1, /* DOUBLE */
    -1, /* FLOAT */
    -1, /* INT64 */
    -1, /* UINT64 */
    -1, /* INT32 */
    -1, /* FIXED64 */
    -1, /* FIXED32 */
    -1, /* BOOL */
    4,  /* STRING */
    -1, /* GROUP */
    5,  /* MESSAGE */
    4,  /* BYTES */
    -1, /* UINT32 */
    -1, /* ENUM */
    -1, /* SFIXED32 */
    -1, /* SFIXED64 */
    -1, /* SINT32 */
    -1, /* SINT64 */
    7,  /* REPEATED DOUBLE */
    6,  /* REPEATED FLOAT */
    11, /* REPEATED INT64 */
    11, /* REPEATED UINT64 */
    10, /* REPEATED INT32 */
    7,  /* REPEATED FIXED64 */
    6,  /* REPEATED FIXED32 */
    8,  /* REPEATED BOOL */
    4,  /* REPEATED STRING */
    5,  /* REPEATED GROUP */
    5,  /* REPEATED MESSAGE */
    4,  /* REPEATED BYTES */
    10, /* REPEATED UINT32 */
    10, /* REPEATED ENUM */
    6,  /* REPEATED SFIXED32 */
    7,  /* REPEATED SFIXED64 */
    10, /* REPEATED SINT32 */
    11, /* REPEATED SINT64 */
};

static const char *decode_msg(upb_decstate *d, const char *ptr, upb_msg *msg,
                              const upb_msglayout *layout) {
  while (ptr < d->limit) {
    uint32_t tag;
    const upb_msglayout_field *field;
    int field_number;
    int wire_type;
    const char *field_start = ptr;
    wireval val;
    int action;

    CHK(ptr = decode_varint32(ptr, d->limit, &tag));
    field_number = tag >> 3;
    wire_type = tag & 7;

    field = upb_find_field(layout, field_number);

    switch (wire_type) {
      case UPB_WIRE_TYPE_VARINT:
        CHK(ptr = decode_varint64(ptr, d->limit, &val.uint64_val));
        action = varint_actions[field->descriptortype];
        break;
      case UPB_WIRE_TYPE_32BIT:
        CHK(d->limit - ptr >= 4);
        memcpy(&val, ptr, 4);
        ptr += 4;
        action = (1 << field->descriptortype) & 0x8084 ? 2 : -1;
        break;
      case UPB_WIRE_TYPE_64BIT:
        CHK(d->limit - ptr >= 8);
        memcpy(&val, ptr, 8);
        ptr += 8;
        action = (1 << field->descriptortype) & 0x10042 ? 3 : -1;
        break;
      case UPB_WIRE_TYPE_DELIMITED: {
        uint32_t size;
        int ndx = field->descriptortype;
        if (field->label == UPB_LABEL_REPEATED) ndx += 18;
        CHK(ptr = decode_varint32(ptr, d->limit, &size));
        CHK(size < INT32_MAX);
        CHK((size_t)(d->limit - ptr) >= size);
        val.str_val.data = ptr;
        val.str_val.size = size;
        ptr += size;
        action = delim_actions[ndx];
        break;
      }
      case UPB_WIRE_TYPE_START_GROUP:
        val.int32_val = field_number;
        action = field->descriptortype == UPB_DTYPE_GROUP ? 5 : -1;
        break;
      case UPB_WIRE_TYPE_END_GROUP:
        d->end_group = field_number;
        return ptr;
      default:
        CHK(false);
        return NULL;
    }

    if (action >= 0) {
      switch (field->label) {
        case UPB_LABEL_REPEATED:
          CHK(ptr = decode_toarray(d, ptr, msg, layout, field, val, action));
          break;
        case UPB_LABEL_MAP:
          CHK(decode_tomap(d, msg, layout, field, val));
          break;
        default:
          CHK(ptr = decode_tomsg(d, ptr, msg, layout, field, val, action));
          break;
      }
    } else {
      CHK(field_number != 0);
      if (wire_type == UPB_WIRE_TYPE_START_GROUP) {
        CHK(ptr = decode_msg(d, ptr, NULL, NULL));
      }
      if (msg) {
        CHK(upb_msg_addunknown(msg, field_start, ptr - field_start, d->arena));
      }
    }
  }

  CHK(ptr == d->limit);
  return ptr;
}

bool upb_decode(const char *buf, size_t size, void *msg, const upb_msglayout *l,
                upb_arena *arena) {
  upb_decstate state;
  state.limit = buf + size;
  state.arena = arena;
  state.depth = 64;
  state.end_group = 0;

  CHK(decode_msg(&state, buf, msg, l));
  return state.end_group == 0;
}

#undef CHK
#undef PTR_AT
