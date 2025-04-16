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

typedef struct NBTValue NBTValue;

typedef struct NBTValue {
  NBTTagType type;
  char* name;

  union {
    uint8_t byte_value;
    uint16_t short_value;
    uint32_t int_value;
    uint64_t long_value;
    float float_value;
    double double_value;
    Buffer byte_array_value;
    char* string_value;
    struct nbt_list {
      int size;
      NBTValue* items;
    } list_value;
    struct nbt_compound {
      int count;
      NBTValue* children;
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
} NBTValue;

typedef struct NBT {
  MemPool pool;
  NBTValue* root;
} NBT;

NBT* read_nbt(ReadableBuffer* p);

NBTValue* nbt_get_compound_tag(NBTValue* nbt, char* name);
void destroy_nbt(NBT* nbt);
