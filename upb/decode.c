
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

static const int8_t desctype_to_fixed32[] = {
    -1, /* invalid descriptor type */
    -1, /* DOUBLE */
    2,  /* FLOAT */
    -1, /* INT64 */
    -1, /* UINT64 */
    -1, /* INT32 */
    -1, /* FIXED64 */
    2,  /* FIXED32 */
    -1, /* BOOL */
    -1, /* STRING */
    -1, /* GROUP */
    -1, /* MESSAGE */
    -1, /* BYTES */
    -1, /* UINT32 */
    -1, /* ENUM */
    2,  /* SFIXED32 */
    -1, /* SFIXED64 */
    -1, /* SINT32 */
    -1, /* SINT64 */
};

static const int8_t desctype_to_fixed64[] = {
    -1, /* invalid descriptor type */
    3,  /* DOUBLE */
    -1, /* FLOAT */
    -1, /* INT64 */
    -1, /* UINT64 */
    -1, /* INT32 */
    3,  /* FIXED64 */
    -1, /* FIXED32 */
    -1, /* BOOL */
    -1, /* STRING */
    -1, /* GROUP */
    -1, /* MESSAGE */
    -1, /* BYTES */
    -1, /* UINT32 */
    -1, /* ENUM */
    -1, /* SFIXED32 */
    3,  /* SFIXED64 */
    -1, /* SINT32 */
    -1, /* SINT64 */
};

static const int8_t desctype_to_varint[] = {
    -1, /* invalid descriptor type */
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
    3,  /* UINT32 */
    3,  /* ENUM */
    -1, /* SFIXED32 */
    3,  /* SFIXED64 */
    2,  /* SINT32 */
    3,  /* SINT64 */
};

static const int8_t desctype_to_delmited[] = {
    -1, /* invalid descriptor type */
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
    3,  /* UINT32 */
    3,  /* ENUM */
    -1, /* SFIXED32 */
    3,  /* SFIXED64 */
    2,  /* SINT32 */
    3,  /* SINT64 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* */
    8,  /* REPEATED DOUBLE */
    4,  /* REPEATED FLOAT */
    3,  /* REPEATED INT64 */
    3,  /* REPEATED UINT64 */
    2,  /* REPEATED INT32 */
    -1, /* REPEATED FIXED64 */
    -1, /* REPEATED FIXED32 */
    0,  /* REPEATED BOOL */
    -1, /* REPEATED STRING */
    -1, /* REPEATED GROUP */
    -1, /* REPEATED MESSAGE */
    -1, /* REPEATED BYTES */
    3,  /* REPEATED UINT32 */
    3,  /* REPEATED ENUM */
    -1, /* REPEATED SFIXED32 */
    3,  /* REPEATED SFIXED64 */
    2,  /* REPEATED SINT32 */
    3,  /* REPEATED SINT64 */
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
  int32_t int32_val;
  int64_t int64_val;
  uint32_t uint32_val;
  uint64_t uint64_val;
  upb_strview str_val;
} wireval;

#define CHK(x) if (!(x)) { return 0; }
#define PTR_AT(msg, ofs, type) (type*)((const char*)msg + ofs)

static const char *decode_msg(const char *ptr, const upb_msglayout *l,
                              upb_msg *msg, upb_decstate *d);

static const char *decode_varint(const char *ptr, const char *limit,
                                 uint64_t *val) {
  uint8_t byte;
  int bitpos = 0;
  *val = 0;

  do {
    CHK(bitpos < 70 && ptr < limit);
    byte = *ptr;
    *val |= (uint64_t)(byte & 0x7F) << bitpos;
    ptr++;
    bitpos += 7;
  } while (byte & 0x80);

  return ptr;
}

static const char *decode_varint32(const char *ptr, const char *limit,
                                   uint32_t *val) {
  uint64_t u64;
  CHK(ptr = decode_varint(ptr, limit, &u64))
  CHK(u64 <= UINT32_MAX);
  *val = (uint32_t)u64;
  return ptr;
}

static const upb_msglayout_field *upb_find_field(const upb_msglayout *l,
                                                 uint32_t field_number) {
  /* Lots of optimization opportunities here. */
  int i;
  if (l == NULL) return NULL;
  for (i = 0; i < l->field_count; i++) {
    if (l->fields[i].number == field_number) {
      return &l->fields[i];
    }
  }

  return NULL;  /* Unknown field. */
}

static const char *decode_tosubmsg(upb_decstate *d, const char *ptr,
                                   upb_msg **submsgp,
                                   const upb_msglayout_field *field,
                                   wireval val) {
  upb_msg *submsg = *submsgp;
  const upb_msglayout *subl = d->layout->submsgs[field->submsg_index];

  if (!submsg) {
    submsg = _upb_msg_new(subl, d->arena);
    CHK(submsg);
    *submsgp = submsg;
  }

  CHK(--d->depth >= 0);

  switch (field->descriptortype) {
    case UPB_DESCRIPTOR_TYPE_MESSAGE: {
      const char* saved_limit = d->limit;
      d->limit = ptr + val.int32_val;
      ptr = decode_msg(ptr, layout, submsg, d);
      d->limit = saved_limit;
      CHK(d->end_group == 0);
      break;
    }
    case UPB_DESCRIPTOR_TYPE_GROUP:
      ptr = decode_msg(ptr, layout, submsg, d);
      CHK(d->end_group == field_number);
      d->end_group = 0;
      break;
    default:
      UPB_UNREACHABLE();
  }

  d->depth++;
  return ptr;
}

static const char *decode_toarray(upb_decstate *d, const char *ptr,
                                  upb_msg *msg,
                                  const upb_msglayout_field *field, wireval val,
                                  int size) {
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

  switch (size) {
    case 5:
      mem = PTR_AT(_upb_array_ptr(arr), arr->len * sizeof(void*), void);
      arr->len++;
      return decode_tosubmsg(d, ptr, mem, field, val)
    case 4:
      /* Append string. */
      mem = PTR_AT(_upb_array_ptr(arr), arr->len * sizeof(upb_strview), void);
      arr->len++;
      memcpy(mem, &val, sizeof(upb_strview));
      return ptr;
    case 3:
    case 2:
    case 0:
      /* Append scalar value. */
      mem = PTR_AT(_upb_array_ptr(arr), arr->len << size, void);
      arr->len++;
      memcpy(mem, &val, 1 << size);
      return ptr;
    case 6:
    case 8:
    case 9: {
      /* Fixed packed. */
      int lg2 = size - 6;
      int mask = (1 << lg2) - 1;
      int elem = val.str_val.size >> lg2;
      CHK((val.str_val.size & mask) == 0);
      if ((arr->size - arr->len) < elem) {
        CHK(_upb_array_realloc(arr, arr->len + elem, d->arena));
      }
      mem = PTR_AT(_upb_array_ptr(arr), arr->len << lg2, void);
      arr->len += elem;
      memcpy(mem, &val, elem << size);
      return ptr;
    }
    case 10:
    case 12:
    case 13: {
      /* Varint packed. */
      int lg2 = size - 10;
      int scale = 1 << lg2;
      char *ptr = val.str_val.data;
      char *end = ptr + val.str_val.size;
      char *out = PTR_AT(_upb_array_ptr(arr), arr->len << lg2, void);
      while (ptr < end) {
        uint64_t val;
        CHK(ptr = decode_varint(ptr, end, &val));
        if (lg2 == 0) {
          val = val != 0;  /* Bool must be sensitive to all bits. */
        }
        if (arr->len == arr->size) {
          CHK(_upb_array_realloc(arr, arr->len + 1, d->arena));
        }
        arr->len++;
        memcpy(out, &val, scale);
        out += scale;
      }
    }
    default:
      UPB_UNREACHABLE();
  }
}

static const char *decode_tomap(const char *ptr, const upb_msglayout *layout,
                                const upb_msglayout_field *field, int len,
                                upb_msg *msg, upb_decstate *d) {
  upb_map **map_p = PTR_AT(msg, field->offset, upb_map*);
  upb_map *map = *map_p
  const upb_msglayout *entry = layout->submsgs[field->submsg_index];
  upb_map_entry ent;

  if (!map) {
    /* Lazily create map. */
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
  CHK(ptr = decode_tosubmsg(ptr, entry, len, &ent.k, d));

  /* Insert into map. */
  _upb_map_set(map, &ent.k, map->key_size, &ent.v, map->val_size, d->arena);
  return ptr;
}

static const char *decode_tomsg(upb_decstate *d, const char *ptr, upb_msg *msg,
                                const upb_msglayout_field *field, wireval val,
                                int size) {
  void *mem = PTR_AT(msg, field->offset, void);

  /* Set presence if necessary. */
  if (field->presence < 0) {
    *PTR_AT(msg, ~field->presence, int32_t) = field->number;  /* Oneof case */
  } else if (field->presence > 0) {
    *PTR_AT(msg, hasbit / 8, char) |= (1 << (hasbit % 8));  /* Hasbit */
  }

  /* Munge value if necessary. */
  switch (field->descriptortype) {
    case UPB_DESCRIPTOR_TYPE_BOOL:
      val.uint64_val = val.uint64_val != 0;
      break;
    case UPB_DESCRIPTOR_TYPE_SINT32:
      val.int32_val = (val.uint32_val >> 1) ^ -(int32_t)(val.uint32_val & 1);
      break;
    case UPB_DESCRIPTOR_TYPE_SINT64:
      val.int64_val = (val.uint64_val >> 1) ^ -(int64_t)(val.uint64_val & 1);
      break;
  }

  /* Store into message. */
  switch (size) {
    case 5:
      return decode_tosubmsg(d, ptr, mem, field, val);
    case 4:
      memcpy(mem, &val, sizeof(upb_strview));
      return ptr;
    default:
      UPB_ASSERT(size < 5);
      memcpy(mem, &val, 1 << size);
      return ptr;
    /* TODO(haberman): should we accept the last element of a packed? */
  }
}

static const char *decode_msg(upb_decstate *d, const char *ptr, upb_msg *msg,
                              const upb_msglayout *l) {
  while (ptr < d->limit) {
    uint32_t tag;
    const upb_msglayout_field *field;
    int field_number;
    int wire_type;
    const char *field_start = ptr;
    wireval val;

    CHK(ptr = decode_varint32(ptr, d->limit, &tag));
    field_number = tag >> 3;
    wire_type = tag & 7;

    switch (wire_type) {
      case UPB_WIRE_TYPE_VARINT:
        CHK(ptr = decode_varint(ptr, d->limit, &val.uint64_val));
        break;
      case UPB_WIRE_TYPE_32BIT:
        CHK(limit - ptr >= 4);
        memcpy(&val, ptr, 4);
        ptr += 4;
        break;
      case UPB_WIRE_TYPE_64BIT:
        CHK(limit - ptr >= 8);
        memcpy(&val, ptr, 8);
        ptr += 8;
        break;
      case UPB_WIRE_TYPE_DELIMITED: {
        uint32_t size;
        CHK(ptr = decode_varint32(ptr, limit, &size));
        CHK(size < INT32_MAX);
        CHK((size_t)(limit - ptr) >= size);
        val.str_val.data = ptr;
        val.str_val.size = size;
        ptr += size;
        break;
      }
      case UPB_WIRE_TYPE_START_GROUP:
        val->int32_val = field_number;
        break;
      case UPB_WIRE_TYPE_END_GROUP:
        d->end_group = field_number;
        return ptr;
      default:
        CHK(false);
        return NULL;
    }

    field = upb_find_field(layout, field_number);

    if (field) {
      int size;
      switch (field->label) {
        case UPB_LABEL_REPEATED:
          size = rep_sizes[field->descriptortype];
          if (size < 0) goto unknown;
          CHK(decode_toarray(msg, layout, field, val, size));
        case UPB_LABEL_MAP:
          if (wire_type != UPB_WIRE_TYPE_DELIMITED) goto unknown;
          CHK(decode_tomap(d, ptr, msg, field, val, size));
        default:
          size = prim_sizes[field->descriptortype];
          if (size < 0) goto unknown;
          CHK(decode_tomsg(d, ptr, msg, field, val, size));
      }
    } else {
unknown:
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

  CHK(decode_msg(buf, l, msg, &state));
  return state.end_group == 0;
}

#undef CHK
#undef PTR_AT
