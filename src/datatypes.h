#pragma once
#include <stddef.h>
#include <stdint.h>

#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

/* --- Macros --- */
#define max(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })

#define min(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })

typedef struct Buffer {
  uint8_t* ptr;
  size_t len;
} Buffer;

Buffer create_buffer(size_t len);
void destroy_buffer(Buffer buffer);
void print_buffer(Buffer str);

typedef Buffer String;

String to_string(char* c_str);
void print_string(String str);

typedef struct ReadableBuffer {
  Buffer buf;
  int cursor;  // Location of next read
} ReadableBuffer;
ReadableBuffer to_readable_buffer(Buffer buf);

typedef struct ResizeableBuffer {
  Buffer buffer;
  size_t len;
} ResizeableBuffer;

ResizeableBuffer create_resizeable_buffer();
void destroy_resizeable_buffer(const ResizeableBuffer buffer);
void resizeable_buffer_ensure_capacity(ResizeableBuffer* buf, int capacity);
Buffer resizable_buffer_to_buffer(ResizeableBuffer resizeable);

typedef struct WritableBuffer {
  ResizeableBuffer buf;
  int cursor;  // Location of next write
} WritableBuffer;
WritableBuffer create_writable_buffer();
void destroy_writable_buffer(const WritableBuffer buffer);

typedef struct UUID {
  uint64_t upper;
  uint64_t lower;
} UUID;

typedef struct BitSet {
  int length;
  uint64_t* data;
} BitSet;