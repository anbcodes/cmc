#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cglm/cglm.h"
#include "datatypes.h"

/* --- Packet Reader/Writer Code --- */

int SEGMENT_BITS = 0x7F;
int CONTINUE_BIT = 0x80;

void write_byte(WritableBuffer *io, uint8_t value) {
  resizeable_buffer_ensure_capacity(&io->buf, io->cursor + 1);

  io->buf.buffer.ptr[io->cursor] = value;

  io->cursor++;

  if (io->cursor > io->buf.len) io->buf.len = io->cursor;
}

void write_bytes(WritableBuffer *io, void *src, int len) {
  resizeable_buffer_ensure_capacity(&io->buf, io->cursor + len);

  memcpy(io->buf.buffer.ptr, src, len);

  io->cursor += len;
  if (io->cursor > io->buf.len) io->buf.len = io->cursor;
}

void write_buffer(WritableBuffer *io, Buffer buf) {
  resizeable_buffer_ensure_capacity(&io->buf, io->cursor + buf.len);

  memcpy(io->buf.buffer.ptr + io->cursor, buf.ptr, buf.len);

  io->cursor += buf.len;
  if (io->cursor > io->buf.len) io->buf.len = io->cursor;
}

void write_short(WritableBuffer *io, uint16_t value) {
  write_byte(io, value >> 8);
  write_byte(io, value);
}

void write_int(WritableBuffer *io, int value) {
  write_byte(io, value >> (8 * 3));
  write_byte(io, value >> (8 * 2));
  write_byte(io, value >> (8 * 1));
  write_byte(io, value >> (8 * 0));
}

void write_long(WritableBuffer *io, long value) {
  write_byte(io, value >> (8 * 7));
  write_byte(io, value >> (8 * 6));
  write_byte(io, value >> (8 * 5));
  write_byte(io, value >> (8 * 4));
  write_byte(io, value >> (8 * 3));
  write_byte(io, value >> (8 * 2));
  write_byte(io, value >> (8 * 1));
  write_byte(io, value >> (8 * 0));
}

void write_ulong(WritableBuffer *io, uint64_t value) {
  write_byte(io, value >> (8 * 7));
  write_byte(io, value >> (8 * 6));
  write_byte(io, value >> (8 * 5));
  write_byte(io, value >> (8 * 4));
  write_byte(io, value >> (8 * 3));
  write_byte(io, value >> (8 * 2));
  write_byte(io, value >> (8 * 1));
  write_byte(io, value >> (8 * 0));
}

void write_float(WritableBuffer *io, float value) {
  write_int(io, *(int *)(&value));
}

void write_double(WritableBuffer *io, double value) {
  write_long(io, *(long *)(&value));
}

void write_varint(WritableBuffer *io, int value) {
  while (true) {
    if ((value & ~SEGMENT_BITS) == 0) {
      write_byte(io, value);
      return;
    }

    write_byte(io, (value & SEGMENT_BITS) | CONTINUE_BIT);

    value = (int)(((unsigned int)value) >> 7);
  }
}

void write_varlong(WritableBuffer *io, long value) {
  while (true) {
    if ((value & ~((long)SEGMENT_BITS)) == 0) {
      write_byte(io, value);
      return;
    }

    write_byte(io, (value & SEGMENT_BITS) | CONTINUE_BIT);

    // Note: >>> means that the sign bit is shifted with the rest of the number rather than being
    // left alone
    value = (long)(((unsigned long)value) >> 7);
  }
}

void write_string(WritableBuffer *io, String string) {
  write_varint(io, string.len);
  // TODO: Speed up by checking whole string length and adding it all at once
  for (int i = 0; i < string.len; i++) {
    write_byte(io, string.ptr[i]);
  }
}

void write_uuid(WritableBuffer *io, UUID uuid) {
  write_byte(io, uuid.upper >> (8 * 7));
  write_byte(io, uuid.upper >> (8 * 6));
  write_byte(io, uuid.upper >> (8 * 5));
  write_byte(io, uuid.upper >> (8 * 4));
  write_byte(io, uuid.upper >> (8 * 3));
  write_byte(io, uuid.upper >> (8 * 2));
  write_byte(io, uuid.upper >> (8 * 1));
  write_byte(io, uuid.upper >> (8 * 0));

  write_byte(io, uuid.lower >> (8 * 7));
  write_byte(io, uuid.lower >> (8 * 6));
  write_byte(io, uuid.lower >> (8 * 5));
  write_byte(io, uuid.lower >> (8 * 4));
  write_byte(io, uuid.lower >> (8 * 3));
  write_byte(io, uuid.lower >> (8 * 2));
  write_byte(io, uuid.lower >> (8 * 1));
  write_byte(io, uuid.lower >> (8 * 0));
}

void write_ipos(WritableBuffer *io, ivec3 pos) {
  uint64_t packed = (((int64_t)pos[0] & 0x3FFFFFF) << 38) | (((int64_t)pos[2] & 0x3FFFFFF) << 12) | ((int64_t)pos[1] & 0xFFF);

  write_ulong(io, packed);
}
// Does not do bounds checking
uint8_t read_byte(ReadableBuffer *io) {
  // printf("rb: len=%d, cursor=%d\n", io->buf.len, io->cursor);
  uint8_t byte = io->buf.ptr[io->cursor];
  io->cursor++;
  return byte;
}

Buffer read_bytes(ReadableBuffer *io, size_t size) {
  Buffer buf = {
    .ptr = io->buf.ptr + io->cursor,
    .len = size,
  };

  io->cursor += size;
  return buf;
}

bool has_byte(const ReadableBuffer io) {
  return io.cursor < io.buf.len;
}

short read_short(ReadableBuffer *io) {
  short num = 0;
  num += (uint16_t)read_byte(io) << 8 * 1;
  num += (uint16_t)read_byte(io);
  return num;
}

uint16_t read_ushort(ReadableBuffer *io) {
  uint16_t num = 0;
  num += (uint16_t)read_byte(io) << 8 * 1;
  num += (uint16_t)read_byte(io);
  return num;
}

int read_int(ReadableBuffer *io) {
  int num = 0;
  num += (uint32_t)read_byte(io) << 8 * 3;
  num += (uint32_t)read_byte(io) << 8 * 2;
  num += (uint32_t)read_byte(io) << 8 * 1;
  num += (uint32_t)read_byte(io);
  return num;
}

int64_t read_long(ReadableBuffer *io) {
  int64_t num = 0;

  num += (uint64_t)read_byte(io) << 8 * 7;
  num += (uint64_t)read_byte(io) << 8 * 6;
  num += (uint64_t)read_byte(io) << 8 * 5;
  num += (uint64_t)read_byte(io) << 8 * 4;
  num += (uint64_t)read_byte(io) << 8 * 3;
  num += (uint64_t)read_byte(io) << 8 * 2;
  num += (uint64_t)read_byte(io) << 8 * 1;
  num += (uint64_t)read_byte(io);
  return num;
}

uint64_t read_ulong(ReadableBuffer *io) {
  uint64_t num = 0;

  num += (uint64_t)read_byte(io) << 8 * 7;
  num += (uint64_t)read_byte(io) << 8 * 6;
  num += (uint64_t)read_byte(io) << 8 * 5;
  num += (uint64_t)read_byte(io) << 8 * 4;
  num += (uint64_t)read_byte(io) << 8 * 3;
  num += (uint64_t)read_byte(io) << 8 * 2;
  num += (uint64_t)read_byte(io) << 8 * 1;
  num += (uint64_t)read_byte(io);
  return num;
}

float read_float(ReadableBuffer *io) {
  int value = read_int(io);
  return *(float *)(&value);
}

double read_double(ReadableBuffer *io) {
  long value = read_long(io);
  return *(double *)(&value);
}

// Does not do bounds checking
// From https://github.com/bolderflight/leb128/blob/main/src/leb128.h
int read_varint(ReadableBuffer *io) {
  int value = 0;
  int position = 0;
  uint8_t current_byte;

  while (true) {
    current_byte = read_byte(io);
    value |= (current_byte & SEGMENT_BITS) << position;

    if ((current_byte & CONTINUE_BIT) == 0) break;

    position += 7;

    if (position >= 32) break;  // Too big
  }

  return value;
}

BitSet read_bitset(ReadableBuffer *io) {
  BitSet bitset = {0};
  bitset.length = read_varint(io);
  bitset.data = malloc(sizeof(uint64_t) * bitset.length);
  for (int i = 0; i < bitset.length; i++) {
    bitset.data[i] = read_ulong(io);
  }
  return bitset;
}

void *destroy_bitset(BitSet bs) {
  free(bs.data);
}

bool bitset_at(BitSet bitset, int index) {
  int word = index / 64;
  if (word >= bitset.length) {
    return false;  // Out of bounds (false by default)
  }
  int bit = index % 64;
  return (bitset.data[word] & (1 << bit)) != 0;
}

bool has_varint(ReadableBuffer io) {
  int position = 0;
  uint8_t current_byte;

  while (has_byte(io)) {
    current_byte = read_byte(&io);

    if ((current_byte & CONTINUE_BIT) == 0) return true;

    position += 7;

    if (position >= 32) break;  // Too big
  }

  return false;
}

long read_varlong(ReadableBuffer *io) {
  long value = 0;
  int position = 0;
  uint8_t current_byte;

  while (true) {
    current_byte = read_byte(io);
    value |= (long)(current_byte & SEGMENT_BITS) << position;

    if ((current_byte & CONTINUE_BIT) == 0) break;

    position += 7;

    if (position >= 64) break;  // Too big
  }

  return value;
}

bool has_varlong(ReadableBuffer io) {
  int position = 0;
  uint8_t current_byte;

  while (has_byte(io)) {
    current_byte = read_byte(&io);

    if ((current_byte & CONTINUE_BIT) == 0) return true;

    position += 7;

    if (position >= 64) break;  // Too big
  }

  return false;
}

String read_string(ReadableBuffer *io) {
  int len = read_varint(io);

  String res = {
    .len = len,
    .ptr = io->buf.ptr + io->cursor,
  };

  // printf("rs: len=%ld, cursor=%d, slen=%d\n", io->buf.len, io->cursor, len);
  io->cursor += len;

  return res;
}

UUID read_uuid(ReadableBuffer *io) {
  UUID uuid = {};
  uuid.upper += (uint64_t)read_byte(io) << 8 * 7;
  uuid.upper += (uint64_t)read_byte(io) << 8 * 6;
  uuid.upper += (uint64_t)read_byte(io) << 8 * 5;
  uuid.upper += (uint64_t)read_byte(io) << 8 * 4;
  uuid.upper += (uint64_t)read_byte(io) << 8 * 3;
  uuid.upper += (uint64_t)read_byte(io) << 8 * 2;
  uuid.upper += (uint64_t)read_byte(io) << 8 * 1;
  uuid.upper += (uint64_t)read_byte(io);

  uuid.lower += (uint64_t)read_byte(io) << 8 * 7;
  uuid.lower += (uint64_t)read_byte(io) << 8 * 6;
  uuid.lower += (uint64_t)read_byte(io) << 8 * 5;
  uuid.lower += (uint64_t)read_byte(io) << 8 * 4;
  uuid.lower += (uint64_t)read_byte(io) << 8 * 3;
  uuid.lower += (uint64_t)read_byte(io) << 8 * 2;
  uuid.lower += (uint64_t)read_byte(io) << 8 * 1;
  uuid.lower += (uint64_t)read_byte(io);

  return uuid;
}

void read_ipos_into(ReadableBuffer *io, ivec3 pos) {
  uint64_t packed = read_ulong(io);
  pos[0] = packed >> 38;
  pos[1] = packed & 0xFFF;
  pos[2] = packed & 0x3ffffff000 >> 12;
}

void read_compressed_long_arr(ReadableBuffer *p, int bits_per_entry, int entries, int compressed_len, int to[]) {
  int ind = 0;
  for (int i = 0; i < compressed_len; i++) {
    uint64_t cur = read_ulong(p);
    // printf("comlong i=%d, bpe=%d, total_entries=%d, curr_entry=%d\n", i, bits_per_entry, entries, ind);
    for (int j = 0; j < 64 / bits_per_entry; j++) {
      int block = cur & ((1 << bits_per_entry) - 1);
      cur = cur >> bits_per_entry;
      to[ind] = block;
      ind++;
      if (ind == entries) {
        return;
      }
    }
  }
}