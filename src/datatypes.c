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

Buffer string_to_buffer(const char *c_str) {
  return (Buffer){
    .ptr = (unsigned char *)c_str,
    .len = strlen(c_str),
  };
}

// Makes a copy of a string (using malloc)
char* copy_string(const char* str) {
  size_t len = strlen(str);
  char* new = malloc(len + 1);
  strcpy(new, str);
  new[len] = '\0';
  return new;
}

ResizeableBuffer create_resizeable_buffer(size_t inital_capacity) {
  return (ResizeableBuffer){
    .buffer = create_buffer(inital_capacity == 0 ? 16 : inital_capacity),
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

WritableBuffer create_writable_buffer(size_t inital_capacity) {
  return (WritableBuffer){
    .buf = create_resizeable_buffer(inital_capacity),
    .cursor = 0,
  };
}

void destroy_writable_buffer(const WritableBuffer buffer) {
  destroy_resizeable_buffer(buffer.buf);
}

// === mem pool ===

// Creates a mempool, pass 0 to inital_capacity to use the default
MemPool mempool_create(size_t inital_capacity) {
  MemPool pool = {.data = create_writable_buffer(inital_capacity)};
  return pool;
}

void* mempool_malloc(MemPool pool, size_t length) {
  resizeable_buffer_ensure_capacity(&pool.data.buf, pool.data.cursor + length);
  void* ptr = &pool.data.buf.buffer.ptr[pool.data.cursor];
  DEBUG("malloc %p", ptr);
  pool.data.cursor += length;

  return ptr;
}

void* mempool_calloc(MemPool pool, size_t length, size_t element_size) {
  int tot_len = length*element_size;
  void* ptr = mempool_malloc(pool, tot_len);
  memset(ptr, 0, tot_len);
  DEBUG("calloc %p", ptr);
  return ptr;
}

void mempool_destroy(MemPool pool) {
  destroy_writable_buffer(pool.data);
}
