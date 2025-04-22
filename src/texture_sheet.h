#pragma once

#include <stdint.h>
#include <cglm/cglm.h>

typedef struct BlockTextureSheet {
  unsigned char* data;
  int width;
  int height;
  int texture_size;
  int current_id;
} BlockTextureSheet;

unsigned char *load_image(const char *filename, unsigned *width, unsigned *height);
unsigned save_image(const char *filename, unsigned char *image, unsigned width, unsigned height);
int block_texture_sheet_add_file_sub_opacity(BlockTextureSheet *sheet, const char *fname, int sub_opacity);
uint16_t block_texture_sheet_add_file(BlockTextureSheet* sheet, const char *fname);

typedef struct EntityTextureSheet {
  unsigned char* data;
  int width;
  int height;
  unsigned int row_max_height;
  ivec2 current_pos;
} EntityTextureSheet;

void entity_texture_sheet_add_file(EntityTextureSheet* sheet, const char* fname);
