#include "nbt.h"

#include <string.h>

#include "datatypes.h"
#include "macros.h"
#include "mcapi/protocol.h"
#include "logging.h"

size_t nbt_reader(void *_p, uint8_t *data, size_t size) {
  ReadableBuffer *p = _p;

  int to_read = MIN(size, p->buf.len - p->cursor);

  DEBUG("c: cursor=%lu, buflen=%ld, to_read=%d, reqsize=%ld", p->cursor, p->buf.len, to_read, size);

  for (int i = 0; i < to_read; i++) {
    data[i] = read_byte(p);
  }

  return to_read;
}

char* read_nbt_string(NBT* nbt, ReadableBuffer *p) {
  short len = read_ushort(p);
  if (len < 0 || (size_t)len > p->buf.len - p->cursor) {
    ERROR("Invalid string length: %d (cursor=%lu, buflen=%ld)", len, p->cursor, p->buf.len);
    return NULL;
  }

  char* res = mempool_malloc(nbt->pool, len + 1);

  memcpy(res, p->buf.ptr + p->cursor, len);
  res[len] = '\0';

  p->cursor += len;

  return res;
}

void read_nbt_into(ReadableBuffer *p, NBT* root, NBTValue *nbt);

void read_nbt_value(ReadableBuffer *p, NBT* root, NBTValue *nbt, NBTTagType type) {
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
      nbt->string_value = read_nbt_string(root, p);
      break;
    case NBT_LIST:
      NBTTagType list_type = read_byte(p);
      size = nbt->list_value.size = read_int(p);
      nbt->list_value.items = mempool_calloc(root->pool, nbt->list_value.size, sizeof(NBTValue));
      for (int i = 0; i < size; i++) {
        nbt->list_value.items[i].type = list_type;
        read_nbt_value(p, root, nbt->list_value.items + i, list_type);
      }
      break;
    case NBT_COMPOUND:
      int curr_buflen = 4;
      nbt->compound_value.children = calloc(curr_buflen, sizeof(NBTValue));
      for (int i = 0;; i++) {
        if (i >= curr_buflen) {
          int old_buflen = curr_buflen;
          curr_buflen *= 2;
          NBTValue *old = nbt->compound_value.children;
          nbt->compound_value.children = calloc(curr_buflen, sizeof(NBTValue));
          memcpy(nbt->compound_value.children, old, old_buflen * sizeof(NBTValue));
          free(old);
        }
        read_nbt_into(p, root, nbt->compound_value.children + i);
        if (nbt->compound_value.children[i].type == NBT_END) {
          nbt->compound_value.count = i + 1;
          break;
        }
      }
      NBTValue* old_children = nbt->compound_value.children;
      nbt->compound_value.children = mempool_calloc(root->pool, nbt->compound_value.count, sizeof(NBTValue));
      memcpy(nbt->compound_value.children, old_children, nbt->compound_value.count * sizeof(NBTValue));
      free(old_children);
      break;
    case NBT_INT_ARRAY:
      size = nbt->int_array_value.size = read_int(p);
      nbt->int_array_value.data = mempool_malloc(root->pool, sizeof(int) * size);
      for (int i = 0; i < size; i++) {
        nbt->int_array_value.data[i] = read_int(p);
      }
      break;
    case NBT_LONG_ARRAY:
      size = nbt->long_array_value.size = read_int(p);
      nbt->long_array_value.data = mempool_malloc(root->pool, sizeof(int64_t) * size);
      for (int i = 0; i < size; i++) {
        nbt->long_array_value.data[i] = read_long(p);
      }
      break;
    case NBT_END:
      // We do not need to handle this case
      break;
  }
}

void read_nbt_into(ReadableBuffer *p, NBT* root, NBTValue *nbt) {
  NBTTagType type = read_byte(p);
  nbt->type = type;

  if (type == NBT_END) {
    return;
  }

  // All other tags are named
  nbt->name = read_nbt_string(root, p);
  read_nbt_value(p, root, nbt, type);
}

NBT *read_nbt(ReadableBuffer *p) {
  NBT *root = calloc(1, sizeof(NBT));
  root->pool = mempool_create(1024);
  NBTValue *nbt = mempool_calloc(root->pool, 1, sizeof(NBTValue));
  root->root = nbt;

  int type = read_byte(p);

  read_nbt_value(p, root, nbt, type);

  return root;
}

NBTValue *nbt_get_compound_tag(NBTValue* nbt, char *name) {
  if (nbt->type != NBT_COMPOUND) {
    return NULL;
  }
  for (int i = 0; i < nbt->compound_value.count; i++) {
    // mcapi_print_str(nbt->compound_value.children[i].name);
    // printf("\n");
    if (nbt->compound_value.children[i].name == NULL || strlen(nbt->compound_value.children[i].name) == 0) {
      continue;
    }
    if (strcmp(nbt->compound_value.children[i].name, name) == 0) {
      return nbt->compound_value.children + i;
    }
  }
  return NULL;
}

void destroy_nbt(NBT *nbt) {
  mempool_destroy(nbt->pool);
  free(nbt);
}
