#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <cglm/cglm.h>
#include "../datatypes.h"

void write_byte(WritableBuffer *io, uint8_t value);
void write_bytes(WritableBuffer *io, void *src, int len);
void write_buffer(WritableBuffer *io, Buffer buf);
void write_short(WritableBuffer *io, uint16_t value);
void write_int(WritableBuffer *io, int value);
void write_long(WritableBuffer *io, long value);
void write_ulong(WritableBuffer *io, uint64_t value);
void write_float(WritableBuffer *io, float value);
void write_double(WritableBuffer *io, double value);
void write_varint(WritableBuffer *io, int value);
void write_varlong(WritableBuffer *io, long value);
void write_string(WritableBuffer *io, char* string);
void write_uuid(WritableBuffer *io, UUID uuid);
void write_ipos(WritableBuffer *io, ivec3 pos);

uint8_t read_byte(ReadableBuffer *io);
Buffer read_bytes(ReadableBuffer *io, size_t size);
bool has_byte(const ReadableBuffer io);
short read_short(ReadableBuffer *io);
uint16_t read_ushort(ReadableBuffer *io);
int read_int(ReadableBuffer *io);
int64_t read_long(ReadableBuffer *io);
uint64_t read_ulong(ReadableBuffer *io);
float read_float(ReadableBuffer *io);
double read_double(ReadableBuffer *io);
int read_varint(ReadableBuffer *io);
BitSet read_bitset(ReadableBuffer *io);
void *destroy_bitset(BitSet bs);
bool bitset_at(BitSet bitset, int index);
bool has_varint(ReadableBuffer io);
long read_varlong(ReadableBuffer *io);
bool has_varlong(ReadableBuffer io);
char* read_string(ReadableBuffer *io);
UUID read_uuid(ReadableBuffer *io);
void read_ipos_into(ReadableBuffer *io, ivec3 pos);
void read_compressed_long_arr(ReadableBuffer *p, int bits_per_entry, int entries, int compressed_len, int to[]);
