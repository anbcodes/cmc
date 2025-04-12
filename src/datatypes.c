#include "datatypes.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

String to_string(char *c_str) {
  return (String){
    .ptr = (unsigned char *)c_str,
    .len = strlen(c_str),
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
