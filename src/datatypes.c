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

void print_buf(Buffer buf) {
  for (int i = 0; i < buf.len; i++) {
    printf("%02x ", buf.ptr[i]);
  }

  printf("\n");
}

String to_string(char *c_str) {
  return (String){
    .ptr = c_str,
    .len = strlen(c_str),
  };
}

void print_string(String str) {
  fflush(stdout);
  write(1, str.ptr, str.len);
}

ResizeableBuffer create_resizeable_buffer() {
  return (ResizeableBuffer){
    .buffer = create_buffer(16),
    .len = 0,
  };
};

void destroy_resizeable_buffer(const ResizeableBuffer buffer) {
  destroy_buffer(buffer.buffer);
}

void resizeable_buffer_ensure_capacity(ResizeableBuffer *buf, int capacity) {
  if (buf->buffer.len < capacity) {
    // Double the size of the buffer
    Buffer old_buf = buf->buffer;
    int oldlen = buf->buffer.len;

    int newlen = oldlen * 2;
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
