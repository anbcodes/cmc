#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

typedef struct Buffer {
  uint8_t* ptr;
  size_t len;
} Buffer;

Buffer create_buffer(size_t len);
void destroy_buffer(Buffer buffer);
Buffer copy_buffer(Buffer buffer);
void print_buffer(Buffer str);
Buffer string_to_buffer(const char* c_str);
char* copy_string(const char* str);

void write_buffer_to_file(Buffer buf, const char* filename);

typedef struct ReadableBuffer {
  Buffer buf;
  size_t cursor;  // Location of next read
} ReadableBuffer;
ReadableBuffer to_readable_buffer(Buffer buf);

typedef struct ResizeableBuffer {
  Buffer buffer;
  size_t len;
} ResizeableBuffer;

ResizeableBuffer create_resizeable_buffer(size_t inital_capacity);
void destroy_resizeable_buffer(const ResizeableBuffer buffer);
void resizeable_buffer_ensure_capacity(ResizeableBuffer* buf, size_t capacity);
Buffer resizable_buffer_to_buffer(ResizeableBuffer resizeable);

typedef struct WritableBuffer {
  ResizeableBuffer buf;
  size_t cursor;  // Location of next write
} WritableBuffer;
WritableBuffer create_writable_buffer(size_t inital_capacity);
void destroy_writable_buffer(const WritableBuffer buffer);

typedef struct UUID {
  uint64_t upper;
  uint64_t lower;
} UUID;

typedef struct BitSet {
  int length;
  uint64_t* data;
} BitSet;

// Mempool
typedef struct MemPool {
  WritableBuffer data;
} MemPool;
MemPool mempool_create(size_t inital_capacity);
void* mempool_malloc(MemPool pool, size_t length);
void* mempool_calloc(MemPool pool, size_t length, size_t element_size);
void mempool_destroy(MemPool pool);

// HashMap
// typedef struct HashElement {
//   String key;
//   Buffer value;
// } HashElement;

// typedef struct HashMap {
//   HashElement* table;
//   size_t capacity;
//   size_t filled;
// } HashMap;

// HashMap* hashmap_create(int capacity);
// void hashmap_insert(HashMap* map, String key, Buffer value);
// void hashmap_resize(HashMap *map, int new_capacity);
// Buffer hashmap_get(const HashMap* map, String key);
// void hashmap_destroy_all_values(HashMap* map);
// void hashmap_destroy(HashMap* map);
