#include "datatypes.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logging.h"

/* --- Buffer --- */

Buffer create_buffer(size_t len) {
  return (Buffer){
    .ptr = malloc(len),
    .len = len,
  };
}

void destroy_buffer(const Buffer buffer) {
  free(buffer.ptr);
}

Buffer copy_buffer(Buffer buffer) {
  Buffer b = create_buffer(buffer.len);
  memcpy(b.ptr, buffer.ptr, b.len);
  return b;
}

void print_buffer(Buffer buf) {
  for (size_t i = 0; i < buf.len; i++) {
    printf("%02x ", buf.ptr[i]);
  }

  printf("\n");
}

void write_buffer_to_file(Buffer buf, const char* filename) {
  FILE *fptr;

  // Open a file in writing mode
  fptr = fopen(filename, "w");

  fwrite(buf.ptr, buf.len, 1, fptr);

  fclose(fptr);
}

String to_string(const char *c_str) {
  return (String){
    .ptr = (unsigned char *)c_str,
    .len = strlen(c_str),
  };
}

void copy_into_cstr(String string, char *c_str) {
  memcpy(c_str, string.ptr, string.len);
  c_str[string.len] = '\0';
}

String substr(String s, int start, int end) {
  while (start < 0) start += s.len;
  while (end < 0) end += s.len;

  return (String) {
    .ptr = s.ptr + start,
    .len = end - start + 1
  };
}

void print_string(String str) {
  write(1, str.ptr, str.len);
  fflush(stdout);
}

void fprint_string(FILE* fp, String str) {
  fwrite(str.ptr, str.len, 1, fp);
  fflush(stderr);
}

bool strings_equal(String s1, String s2) {
  if (s1.len != s2.len) {
    return false;
  } else {
    return strncmp((char*)s1.ptr, (char*)s2.ptr, s1.len) == 0;
  }
}

ResizeableBuffer create_resizeable_buffer() {
  return (ResizeableBuffer){
    .buffer = create_buffer(16),
    .len = 0,
  };
}

void destroy_resizeable_buffer(const ResizeableBuffer buffer) {
  destroy_buffer(buffer.buffer);
}

void resizeable_buffer_ensure_capacity(ResizeableBuffer *buf, size_t capacity) {
  if (buf->buffer.len < capacity) {
    // Double the size of the buffer
    Buffer old_buf = buf->buffer;
    size_t oldlen = buf->buffer.len;

    size_t newlen = oldlen * 2;
    while (newlen < capacity) {
      newlen *= 2;
    }

    buf->buffer = create_buffer(newlen);
    memcpy(buf->buffer.ptr, old_buf.ptr, oldlen);

    destroy_buffer(old_buf);
  }
}

Buffer resizable_buffer_to_buffer(ResizeableBuffer resizeable) {
  return (Buffer){
    .ptr = resizeable.buffer.ptr,
    .len = resizeable.len,
  };
}

ReadableBuffer to_readable_buffer(Buffer buf) {
  return (ReadableBuffer){
    .buf = buf,
    .cursor = 0,
  };
}

WritableBuffer create_writable_buffer() {
  return (WritableBuffer){
    .buf = create_resizeable_buffer(),
    .cursor = 0,
  };
}

void destroy_writable_buffer(const WritableBuffer buffer) {
  destroy_resizeable_buffer(buffer.buf);
}

// ========= Hash Table ===========

HashMap* hashmap_create(int capacity) {
  HashMap* map = malloc(sizeof(HashMap));
  map->table = calloc(capacity, sizeof(HashElement));
  map->capacity = capacity;
  map->filled = 0;

  return map;
}

int hashmap_hash(const HashMap* map, String key) {
  int hash = 0;
  for (size_t i = 0; i < key.len; i++) {
    hash = (hash * 31 + key.ptr[i]) % map->capacity;
  }
  return hash;
}

void hashmap_resize(HashMap *map, int new_capacity) {
  HashElement* old_table = map->table;
  size_t old_capacity = map->capacity;
  size_t old_filled = map->filled;

  map->table = calloc(new_capacity, sizeof(HashElement));
  map->capacity = new_capacity;
  map->filled = old_filled;

  for (size_t i = 0; i < old_capacity; i++) {
    if (old_table[i].key.len != 0) {
      hashmap_insert(map, old_table[i].key, old_table->value);
    }
  }

  free(old_table);
}

/** Takes ownership of the key. It does not make a copy but, it will free the key when the hash map is destroyed. If you want to use the key after insertion, make a copy of it. */
void hashmap_insert(HashMap* map, String key, Buffer value) {
  if ((float)map->filled / map->capacity > 0.75) {
    hashmap_resize(map, map->capacity * 2);
  }
  int hash = hashmap_hash(map, key);

  while (map->table[hash].key.len != 0) {
    // DEBUG("Looking for free spot, this spot (%d) has strlen=%d str=%sb filled=%d cap=%d", hash, map->table[hash].key.len, map->table[hash].key, map->filled, map->capacity);
    hash = (hash + 1) % map->capacity;
  }

  DEBUG("Inserting key=%sb value=%sb into index %d", key, value, hash);
  map->table[hash].key = key;
  map->table[hash].value = value;
  map->filled += 1;
}

Buffer hashmap_get(const HashMap* map, String key) {
  int hash = hashmap_hash(map, key);

  while (map->table[hash].key.len != 0) {
    DEBUG("Checking index %d tofind=%sb, stored=%sb", hash, key, map->table[hash].key);
    if (strings_equal(map->table[hash].key, key)) {
      DEBUG("Found! value=%sb", map->table[hash].value);
      return map->table[hash].value;
    }
    hash = (hash + 1) % map->capacity;
  }

  return (Buffer){.ptr = NULL, .len = 0};
}

void hashmap_destroy_all_values(HashMap* map) {
  for (size_t i = 0; i < map->capacity; i++) {
    if (map->table[i].value.ptr != NULL) {
      DEBUG("Freeing #%d %sb", i, map->table[i].value);
      free(map->table[i].value.ptr);
    }
  }
}

void hashmap_destroy(HashMap* map) {
  for (size_t i = 0; i < map->capacity; i++) {
    if (map->table[i].key.len != 0) {
      free(map->table[i].key.ptr);
    }
  }
  free(map->table);

  free(map);
}
