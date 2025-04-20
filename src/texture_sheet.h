#pragma once

#include <stdint.h>
typedef struct TextureSheet {
  unsigned char* data;
  int width;
  int height;
  int texture_size;
  int current_id;
} TextureSheet;

unsigned char *load_image(const char *filename, unsigned *width, unsigned *height);
unsigned save_image(const char *filename, unsigned char *image, unsigned width, unsigned height);
int texture_sheet_add_file_sub_opacity(TextureSheet *sheet, const char *fname, int sub_opacity);
uint16_t texture_sheet_add_file(TextureSheet* sheet, const char *fname);
