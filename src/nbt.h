#pragma once
#include "datatypes.h"

typedef enum NBTTagType {
  NBT_END = 0x00,
  NBT_BYTE,
  NBT_SHORT,
  NBT_INT,
  NBT_LONG,
  NBT_FLOAT,
  NBT_DOUBLE,
  NBT_BYTE_ARRAY,
  NBT_STRING,
  NBT_LIST,
  NBT_COMPOUND,
  NBT_INT_ARRAY,
  NBT_LONG_ARRAY,
} NBTTagType;

typedef struct NBT NBT;

typedef struct NBT {
  NBTTagType type;
  String name;

  union {
    uint8_t byte_value;
    uint16_t short_value;
    uint32_t int_value;
    uint64_t long_value;
    float float_value;
    double double_value;
    Buffer byte_array_value;
    String string_value;
    struct nbt_list {
      int size;
      NBT* items;
    } list_value;
    struct nbt_compound {
      int count;
      NBT* children;
    } compound_value;
    struct nbt_int_array {
      int size;
      int* data;
    } int_array_value;
    struct nbt_long_array {
      int size;
      long* data;
    } long_array_value;
  };
} NBT;
NBT* read_nbt(ReadableBuffer* p);

NBT* nbt_get_compound_tag(NBT* nbt, char* name);
void destroy_nbt(NBT* nbt);
