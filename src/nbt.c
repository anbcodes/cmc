#include "nbt.h"

#include <string.h>

#include "datatypes.h"
#include "macros.h"
#include "protocol.h"

size_t nbt_reader(void *_p, uint8_t *data, size_t size) {
  ReadableBuffer *p = _p;

  int to_read = MIN(size, p->buf.len - p->cursor);

  DEBUG("c: cursor=%lu, buflen=%ld, to_read=%d, reqsize=%ld", p->cursor, p->buf.len, to_read, size);

  for (int i = 0; i < to_read; i++) {
    data[i] = read_byte(p);
  }

  return to_read;
}

String read_nbt_string(ReadableBuffer *p) {
  String res = {
    .len = read_ushort(p),
  };

  res.ptr = p->buf.ptr + p->cursor;

  p->cursor += res.len;

  return res;
}

void read_nbt_into(ReadableBuffer *p, NBT *nbt);

void read_nbt_value(ReadableBuffer *p, NBT *nbt, NBTTagType type) {
  nbt->type = type;

  int size;  // used in some branches

  switch (type) {
    case NBT_BYTE:
      nbt->byte_value = read_byte(p);
      break;
    case NBT_SHORT:
      nbt->short_value = read_short(p);
      break;
    case NBT_INT:
      nbt->int_value = read_int(p);
      break;
    case NBT_LONG:
      nbt->long_value = read_long(p);
      break;
    case NBT_FLOAT:
      nbt->float_value = read_float(p);
      break;
    case NBT_DOUBLE:
      nbt->double_value = read_double(p);
      break;
    case NBT_BYTE_ARRAY:
      size = read_int(p);
      nbt->byte_array_value = read_bytes(p, size);
      break;
    case NBT_STRING:
      nbt->string_value = read_nbt_string(p);
      break;
    case NBT_LIST:
      NBTTagType list_type = read_byte(p);
      size = nbt->list_value.size = read_int(p);
      nbt->list_value.items = calloc(nbt->list_value.size, sizeof(NBT));
      for (int i = 0; i < size; i++) {
        nbt->list_value.items[i].type = list_type;
        read_nbt_value(p, nbt->list_value.items + i, list_type);
      }
      break;
    case NBT_COMPOUND:
      int curr_buflen = 4;
      nbt->compound_value.children = calloc(curr_buflen, sizeof(NBT));
      for (int i = 0;; i++) {
        if (i >= curr_buflen) {
          int old_buflen = curr_buflen;
          curr_buflen *= 2;
          NBT *old = nbt->compound_value.children;
          nbt->compound_value.children = calloc(curr_buflen, sizeof(NBT));
          memcpy(nbt->compound_value.children, old, old_buflen * sizeof(NBT));
          free(old);
        }
        read_nbt_into(p, nbt->compound_value.children + i);
        if (nbt->compound_value.children[i].type == NBT_END) {
          nbt->compound_value.count = i + 1;
          break;
        }
      }
      break;
    case NBT_INT_ARRAY:
      size = nbt->int_array_value.size = read_int(p);
      nbt->int_array_value.data = malloc(sizeof(int) * size);
      for (int i = 0; i < size; i++) {
        nbt->int_array_value.data[i] = read_int(p);
      }
      break;
    case NBT_LONG_ARRAY:
      size = nbt->long_array_value.size = read_int(p);
      nbt->long_array_value.data = malloc(sizeof(int64_t) * size);
      for (int i = 0; i < size; i++) {
        nbt->long_array_value.data[i] = read_long(p);
      }
      break;
    case NBT_END:
      // We do not need to handle this case
      break;
  }
}

void read_nbt_into(ReadableBuffer *p, NBT *nbt) {
  NBTTagType type = read_byte(p);
  nbt->type = type;

  if (type == NBT_END) {
    return;
  }

  // All other tags are named
  nbt->name = read_nbt_string(p);
  read_nbt_value(p, nbt, type);
}

NBT *read_nbt(ReadableBuffer *p) {
  NBT *nbt = calloc(1, sizeof(NBT));

  int type = read_byte(p);

  read_nbt_value(p, nbt, type);

  return nbt;
}

NBT *nbt_get_compound_tag(NBT *nbt, char *name) {
  if (nbt->type != NBT_COMPOUND) {
    return NULL;
  }
  for (int i = 0; i < nbt->compound_value.count; i++) {
    // mcapi_print_str(nbt->compound_value.children[i].name);
    // printf("\n");
    if (nbt->compound_value.children[i].name.len == 0) {
      continue;
    }
    if (strncmp((char*)nbt->compound_value.children[i].name.ptr, name, nbt->compound_value.children[i].name.len) == 0) {
      return nbt->compound_value.children + i;
    }
  }
  return NULL;
}
void _destroy_nbt_recur(NBT *nbt);

void destroy_nbt(NBT *nbt) {
  _destroy_nbt_recur(nbt);
  free(nbt);
}

void _destroy_nbt_recur(NBT *nbt) {
  if (nbt->type == NBT_COMPOUND) {
    for (int i = 0; i < nbt->compound_value.count; i++) {
      _destroy_nbt_recur(&nbt->compound_value.children[i]);
    }
    free(nbt->compound_value.children);
  } else if (nbt->type == NBT_LIST) {
    for (int i = 0; i < nbt->list_value.size; i++) {
      _destroy_nbt_recur(&nbt->list_value.items[i]);
    }
    free(nbt->list_value.items);
  } else if (nbt->type == NBT_BYTE_ARRAY) {
    destroy_buffer(nbt->byte_array_value);
  } else if (nbt->type == NBT_INT_ARRAY) {
    free(nbt->int_array_value.data);
  } else if (nbt->type == NBT_LONG_ARRAY) {
    free(nbt->long_array_value.data);
  }
}
