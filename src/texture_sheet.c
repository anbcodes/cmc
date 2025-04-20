
#include <stdlib.h>
#include <cglm/cglm.h>
#include "texture_sheet.h"
#include "lodepng/lodepng.h"
#include "logging.h"
#include "macros.h"

unsigned char *load_image(const char *filename, unsigned *width, unsigned *height) {
  unsigned char *png = NULL;
  size_t pngsize, error;
  error = lodepng_load_file(&png, &pngsize, filename);
  if (!error) {
    unsigned char *image = 0;
    error = lodepng_decode32(&image, width, height, png, pngsize);
    free(png);
    if (!error) {
      return image;
    }
  }
  return NULL;
}

unsigned save_image(const char *filename, unsigned char *image, unsigned width, unsigned height) {
  unsigned char *png = NULL;
  size_t pngsize;

  unsigned error = lodepng_encode32(&png, &pngsize, image, width, height);
  if (!error) {
    error = lodepng_save_file(png, pngsize, filename);
  }

  free(png);
  return error;
}

int texture_sheet_add_file_sub_opacity(TextureSheet *sheet, const char *fname, int sub_opacity) {
  unsigned int width, height;
  unsigned char *rgba = load_image(fname, &width, &height);
  if (rgba == NULL) {
    // printf("%s not found for %s\n", name, texture_name);
    return 0;
    // } else {
    //   printf("%s found for %s, %d\n", name, texture_name, *cur_texture);
  }

  if (sheet->current_id >= sheet->width * sheet->height) {
    WARN("Too many textures, increase TEXTURE_TILES");
    assert(false);
  }
  sheet->current_id += 1;
  int full_width = sheet->texture_size * sheet->width;
  int tile_start_x = (sheet->current_id % sheet->width) * sheet->texture_size;
  int tile_start_y = (sheet->current_id / sheet->width) * sheet->texture_size;
  width = glm_min(width, sheet->texture_size);
  height = glm_min(height, sheet->texture_size);
  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++) {
      int i = (y * width + x) * 4;
      int j = ((tile_start_y + y) * full_width + (tile_start_x + x)) * 4;
      // int j = (texture_id * TEXTURE_SIZE * TEXTURE_SIZE + y * TEXTURE_SIZE + x) * 4;
      sheet->data[j + 0] = rgba[i + 0];
      sheet->data[j + 1] = rgba[i + 1];
      sheet->data[j + 2] = rgba[i + 2];
      sheet->data[j + 3] = MAX(rgba[i + 3] - sub_opacity, 0);
    }
  }

  free(rgba);
  return sheet->current_id;
}

uint16_t texture_sheet_add_file(TextureSheet* sheet, const char *fname) {
  return texture_sheet_add_file_sub_opacity(sheet, fname, 0);
}
