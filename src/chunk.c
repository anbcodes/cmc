#include "chunk.h"

#include <stdbool.h>
#include <assert.h>

#include "framework.h"
#include "logging.h"

int positive_mod(int a, int b) {
  int result = a % b;
  if (result < 0) {
    result += b;
  }
  return result;
}

// Max quads per chunk section times 4 vertices per quad times floats per vertex
static float quads[(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * 3) * 4 * FLOATS_PER_VERTEX];

Chunk *world_chunk(World *world, int x, int z) {
  for (int i = 0; i < MAX_CHUNKS; i += 1) {
    if (world->chunks[i] == NULL) {
      continue;
    }
    Chunk *chunk = world->chunks[i];
    if (chunk->x == x && chunk->z == z) {
      return chunk;
    }
  }
  return NULL;
}

void chunk_destroy(Chunk *chunk) {
  for (int j = 0; j < 24; j++) {
    if (chunk->sections[j].num_quads != 0) {
      wgpuBufferRelease(chunk->sections[j].vertex_buffer);
    }
  }
  free(chunk);
}

int world_add_chunk(World *world, Chunk *chunk) {
  for (int i = 0; i < MAX_CHUNKS; i += 1) {
    if (world->chunks[i] != NULL && world->chunks[i]->x == chunk->x && world->chunks[i]->z == chunk->z) {
      chunk_destroy(world->chunks[i]);
      world->chunks[i] = chunk;
      return i;
    }
  }

  for (int i = 0; i < MAX_CHUNKS; i += 1) {
    if (world->chunks[i] == NULL) {
      world->chunks[i] = chunk;
      return i;
    }
  }

  FATAL("No space for chunk");
  assert(false);
  return -1;
}

int world_get_material(World *world, vec3 position) {
  int chunk_x = (int)floor(position[0] / CHUNK_SIZE);
  int chunk_z = (int)floor(position[2] / CHUNK_SIZE);
  Chunk *chunk = world_chunk(world, chunk_x, chunk_z);
  if (chunk == NULL) {
    return 0;
  }
  vec3 chunk_position;
  glm_vec3_sub(position, (vec3){chunk->x * CHUNK_SIZE, 0.0f, chunk->z * CHUNK_SIZE}, chunk_position);
  int x = (int)floor(chunk_position[0]);
  int y = positive_mod((int)floor(chunk_position[1]), CHUNK_SIZE);
  int z = (int)floor(chunk_position[2]);
  int s = (int)floor(chunk_position[1] / CHUNK_SIZE) + 4;
  return chunk->sections[s].data[x + CHUNK_SIZE * (z + CHUNK_SIZE * y)];
}

void world_get_sky_color(World *world, vec3 position, BiomeInfo *biome_info, vec3 sky_color) {
  int chunk_x = (int)floor(position[0] / CHUNK_SIZE);
  int chunk_z = (int)floor(position[2] / CHUNK_SIZE);
  Chunk *chunk = world_chunk(world, chunk_x, chunk_z);
  if (chunk == NULL) {
    glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, sky_color);
    return;
  }
  vec3 chunk_position;
  glm_vec3_sub(position, (vec3){chunk->x * CHUNK_SIZE, 0.0f, chunk->z * CHUNK_SIZE}, chunk_position);
  int x = (int)floor(chunk_position[0]);
  int y = positive_mod((int)floor(chunk_position[1]), CHUNK_SIZE);
  int z = (int)floor(chunk_position[2]);
  int s = (int)floor(chunk_position[1] / CHUNK_SIZE) + 4;
  int biome_x = floor(x / 4.0);
  int biome_z = floor(z / 4.0);
  int biome_y = floor(y / 4.0);
  int biome_index = chunk->sections[s].biome_data[biome_x + 4 * (biome_z + 4 * biome_y)];
  BiomeInfo biome = biome_info[biome_index];
  glm_vec3_copy(biome.sky_color, sky_color);
}

void world_set_block(World *world, vec3 position, int material, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device) {
  int chunk_x = (int)floor(position[0] / CHUNK_SIZE);
  int chunk_z = (int)floor(position[2] / CHUNK_SIZE);
  Chunk *chunk = world_chunk(world, chunk_x, chunk_z);
  if (chunk == NULL) {
    return;
  }
  vec3 chunk_position;
  glm_vec3_sub(position, (vec3){chunk->x * CHUNK_SIZE, 0.0f, chunk->z * CHUNK_SIZE}, chunk_position);
  int x = (int)floor(chunk_position[0]);
  int y = positive_mod((int)floor(chunk_position[1]), CHUNK_SIZE);
  int z = (int)floor(chunk_position[2]);
  int s = (int)floor(chunk_position[1] / CHUNK_SIZE) + 4;
  chunk->sections[s].data[x + CHUNK_SIZE * (z + CHUNK_SIZE * y)] = material;

  // Update mesh
  chunk_section_update_mesh_if_internal(&chunk->sections[s], world, block_info, biome_info, device);

  // Update neighbors if at upper edge
  if (x == CHUNK_SIZE - 1) {
    Chunk *chunk_x = world_chunk(world, chunk->x + 1, chunk->z);
    if (chunk_x) {
      chunk_section_update_mesh_if_internal(&chunk_x->sections[s], world, block_info, biome_info, device);
    }
  }
  if (y == CHUNK_SIZE - 1 && s < Y_SECTIONS - 1) {
    chunk_section_update_mesh_if_internal(&chunk->sections[s + 1], world, block_info, biome_info, device);
  }
  if (z == CHUNK_SIZE - 1) {
    Chunk *chunk_z = world_chunk(world, chunk->x, chunk->z + 1);
    if (chunk_z) {
      chunk_section_update_mesh_if_internal(&chunk_z->sections[s], world, block_info, biome_info, device);
    }
  }
}

void world_target_block(World *world, vec3 position, vec3 look, float reach, vec3 target, vec3 normal, int *material) {
  int chunk_x = (int)floor(position[0] / CHUNK_SIZE);
  int chunk_z = (int)floor(position[2] / CHUNK_SIZE);
  Chunk *chunk = world_chunk(world, chunk_x, chunk_z);
  if (chunk == NULL) {
    *material = 0;
    return;
  }
  float distance = 0.0f;
  vec3 location;
  glm_vec3_copy(position, location);
  vec3 delta;
  glm_vec3_scale(look, 0.01f, delta);
  while (distance < reach) {
    int chunk_x = (int)floor(location[0] / CHUNK_SIZE);
    int chunk_z = (int)floor(location[2] / CHUNK_SIZE);
    if (chunk->x != chunk_x || chunk->z != chunk_z) {
      chunk = world_chunk(world, chunk_x, chunk_z);
      if (chunk == NULL) {
        *material = 0;
        return;
      }
    }
    vec3 chunk_location;
    glm_vec3_sub(location, (vec3){chunk->x * CHUNK_SIZE, 0.0f, chunk->z * CHUNK_SIZE}, chunk_location);
    int x = (int)floor(chunk_location[0]);
    int y = positive_mod((int)floor(chunk_location[1]), CHUNK_SIZE);
    int z = (int)floor(chunk_location[2]);
    int section = (int)floor(chunk_location[1] / CHUNK_SIZE) + 4;
    int mat = chunk->sections[section].data[x + CHUNK_SIZE * (z + CHUNK_SIZE * y)];
    if (mat != 0) {
      glm_vec3_copy(location, target);
      glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, normal);
      vec3 low_dist;
      glm_vec3_sub(location, (vec3){floor(location[0]), floor(location[1]), floor(location[2])}, low_dist);
      vec3 high_dist;
      glm_vec3_sub((vec3){ceil(location[0]), ceil(location[1]), ceil(location[2])}, location, high_dist);
      int min_x_dir = low_dist[0] < high_dist[0] ? -1 : 1;
      float min_x = low_dist[0] < high_dist[0] ? low_dist[0] : high_dist[0];
      int min_y_dir = low_dist[1] < high_dist[1] ? -1 : 1;
      float min_y = low_dist[1] < high_dist[1] ? low_dist[1] : high_dist[1];
      int min_z_dir = low_dist[2] < high_dist[2] ? -1 : 1;
      float min_z = low_dist[2] < high_dist[2] ? low_dist[2] : high_dist[2];
      if (min_x < min_y && min_x < min_z) {
        normal[0] = min_x_dir;
      } else if (min_y < min_z) {
        normal[1] = min_y_dir;
      } else {
        normal[2] = min_z_dir;
      }
      *material = mat;
      return;
    }
    distance += 0.01f;
    glm_vec3_add(location, delta, location);
  }
  *material = 0;
}

void world_init_new_meshes(World *world, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device) {
  for (int ci = 0; ci < MAX_CHUNKS; ci += 1) {
    Chunk *chunk = world->chunks[ci];
    if (chunk == NULL) {
      continue;
    }
    Chunk *x_chunk = world_chunk(world, chunk->x - 1, chunk->z);
    Chunk *z_chunk = world_chunk(world, chunk->x, chunk->z - 1);
    if (x_chunk == NULL || z_chunk == NULL) {
      continue;
    }
    for (int s = 0; s < 24; s += 1) {
      if (chunk->sections[s].vertex_buffer != NULL) {
        continue;
      }
      ChunkSection *neighbors[3] = {NULL, NULL, NULL};
      neighbors[0] = &x_chunk->sections[s];
      neighbors[2] = &z_chunk->sections[s];
      if (s > 0) {
        neighbors[1] = &chunk->sections[s - 1];
      }
      chunk_section_update_mesh(&chunk->sections[s], neighbors, block_info, biome_info, device);
    }
  }
}

int face_material_between(int a, int b, BlockInfo *block_info) {
  if (a == 0 && b == 0) {
    return 0;
  }
  if (a == 0 && b != 0) {
    return -b;
  }
  if (a != 0 && b == 0) {
    return a;
  }
  // At this point, neither a or b are air
  // TODO: Need to lookup block transparency
  bool ta = block_info[abs(a)].transparent;
  bool tb = block_info[abs(b)].transparent;
  if (!ta && !tb) {
    return 0;
  }
  if (ta && !tb) {
    return -b;
  }
  if (!ta && tb) {
    return a;
  }
  // Arbitrary, both are semi-transparent blocks
  return a;
}

void chunk_section_update_mesh_if_internal(ChunkSection *section, World *world, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device) {
  Chunk *chunk = world_chunk(world, section->x, section->z);
  Chunk *x_chunk = world_chunk(world, section->x - 1, section->z);
  Chunk *z_chunk = world_chunk(world, section->x, section->z - 1);
  if (chunk == NULL || x_chunk == NULL || z_chunk == NULL) {
    return;
  }
  int s = section->y + 4;
  ChunkSection *neighbors[3] = {&x_chunk->sections[s], NULL, &z_chunk->sections[s]};
  if (s > 0) {
    neighbors[1] = &chunk->sections[s - 1];
  }
  chunk_section_update_mesh(&chunk->sections[s], neighbors, block_info, biome_info, device);
}

typedef struct MaskInfo {
  int material;
  char sky_light;
  char block_light;
} MaskInfo;

bool mask_equal(MaskInfo a, MaskInfo b) {
  return a.material == b.material && a.sky_light == b.sky_light && a.block_light == b.block_light;
}

void chunk_section_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device) {
  section->num_quads = 0;
  MaskInfo mask[16 * 16];
  vec3 base = {section->x * CHUNK_SIZE, section->y * CHUNK_SIZE, section->z * CHUNK_SIZE};
  for (int d = 0; d < 3; d += 1) {
    int u = (d + 1) % 3;
    int v = (d + 2) % 3;
    int x[3] = {0, 0, 0};

    if (d == 0) {
      u = (d + 2) % 3;
      v = (d + 1) % 3;
    }

    // Go over all the slices in this dimension
    for (x[d] = 0; x[d] < 16; x[d] += 1) {
      // Make a mask
      for (x[u] = 0; x[u] < 16; x[u] += 1) {
        for (x[v] = 0; x[v] < 16; x[v] += 1) {
          int above_index = x[0] + CHUNK_SIZE * (x[2] + CHUNK_SIZE * x[1]);
          int above = section->data[above_index];
          int above_sky_light = section->sky_light[above_index];
          int above_block_light = section->block_light[above_index];
          int xb[3] = {x[0], x[1], x[2]};
          xb[d] -= 1;
          int below;
          int below_sky_light;
          int below_block_light;
          if (xb[d] < 0) {
            if (neighbors[d] == NULL) {
              below = 0;
              below_sky_light = 15;
              below_block_light = 15;
            } else {
              xb[d] = 15;
              int below_index = xb[0] + CHUNK_SIZE * (xb[2] + CHUNK_SIZE * xb[1]);
              below = neighbors[d]->data[below_index];
              below_sky_light = neighbors[d]->sky_light[below_index];
              below_block_light = neighbors[d]->block_light[below_index];
            }
          } else {
            int below_index = xb[0] + CHUNK_SIZE * (xb[2] + CHUNK_SIZE * xb[1]);
            below = section->data[below_index];
            below_sky_light = section->sky_light[below_index];
            below_block_light = section->block_light[below_index];
          }
          int material = face_material_between(below, above, block_info);
          mask[x[v] + CHUNK_SIZE * x[u]].material = material;
          mask[x[v] + CHUNK_SIZE * x[u]].sky_light = material < 0 ? below_sky_light : above_sky_light;
          mask[x[v] + CHUNK_SIZE * x[u]].block_light = material < 0 ? below_block_light : above_block_light;
        }
      }

      // Greedily find a quad where the mask is the same value and repeat
      for (int j = 0; j < CHUNK_SIZE; j += 1) {
        for (int i = 0; i < CHUNK_SIZE;) {
          MaskInfo m = mask[j + CHUNK_SIZE * i];
          if (m.material == 0) {
            i += 1;
            continue;
          }
          int w = 1;
          while (mask_equal(mask[j + CHUNK_SIZE * (i + w)], m) && i + w < 16) {
            w += 1;
          }
          int h = 1;
          bool done = false;
          for (; j + h < CHUNK_SIZE; h += 1) {
            for (int k = 0; k < w; k += 1) {
              if (!mask_equal(mask[(j + h) + CHUNK_SIZE * (i + k)], m)) {
                done = true;
                break;
              }
            }
            if (done) {
              break;
            }
          }

          // Add quad
          x[u] = i;
          x[v] = j;
          int du[3] = {0};
          du[u] = w;
          int dv[3] = {0};
          dv[v] = h;
          int q = section->num_quads * 4 * FLOATS_PER_VERTEX;

          BlockInfo info = abs(m.material) > 65535 ? block_info[0] : block_info[abs(m.material)]; // Fails here!!!!
          int tile = 0;
          if (info.mesh.num_elements > 0) {
            tile = info.mesh.elements[0].up_texture;
          }
          // int tile = info.texture;
          // if (tile == 0) {
          //   tile = info.texture_all;
          // }
          // if (tile == 0) {
          //   tile = info.texture_cross;
          // }
          // if (tile == 0) {
          //   tile = info.texture_layer0;
          // }
          // if (tile == 0) {
          //   tile = info.texture_vine;
          // }
          // if (tile == 0) {
          //   tile = info.texture_flowerbed;
          // }
          // if (d == 1 && info.texture_end != 0) {
          //   tile = info.texture_end;
          // }
          // if (m.material < 0 && d == 1 && info.texture_bottom != 0) {
          //   tile = info.texture_bottom;
          // }
          // if (m.material > 0 && d == 1 && info.texture_top != 0) {
          //   tile = info.texture_top;
          // }
          // if (d != 1 && info.texture_side != 0) {
          //   tile = info.texture_side;
          // }

          // Get the biome color
          vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
          ivec3 biome_x = {floor(x[0] / 4.0), floor(x[1] / 4.0), floor(x[2] / 4.0)};
          int biome_index = section->biome_data[biome_x[0] + 4 * (biome_x[2] + 4 * biome_x[1])];
          BiomeInfo biome = biome_info[biome_index];
          if (info.grass) {
            // Don't set the grass color for the bottom of the block
            if (!(d == 1 && m.material < 0)) {
              glm_vec3_copy(biome.grass_color, color);
            }
          }
          if (info.foliage) {
            glm_vec3_copy(biome.foliage_color, color);
          }
          if (info.dry_foliage) {
            glm_vec3_copy(biome.dry_foliage_color, color);
          }

          // Needed for grass overlay on the sides
          // Would need something like this for redstone and maybe a few other things
          int overlay_tile = 0;
          // if (d != 1 && info.texture_overlay != 0) {
          //   overlay_tile = info.texture_overlay;
          // }
          int normal = (m.material > 0 ? 1 : -1) * (d + 1);
          quads[q + 0] = base[0] + x[0];
          quads[q + 1] = base[1] + x[1];
          quads[q + 2] = base[2] + x[2];
          quads[q + 3] = color[0];
          quads[q + 4] = color[1];
          quads[q + 5] = color[2];
          quads[q + 6] = color[3];
          quads[q + 7] = w;
          quads[q + 8] = h;
          quads[q + 9] = tile;
          quads[q + 10] = overlay_tile;
          quads[q + 11] = m.sky_light / 15.0f;
          quads[q + 12] = m.block_light / 15.0f;
          quads[q + 13] = normal;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = base[0] + x[0] + du[0];
          quads[q + 1] = base[1] + x[1] + du[1];
          quads[q + 2] = base[2] + x[2] + du[2];
          quads[q + 3] = color[0];
          quads[q + 4] = color[1];
          quads[q + 5] = color[2];
          quads[q + 6] = color[3];
          quads[q + 7] = 0.0f;
          quads[q + 8] = h;
          quads[q + 9] = tile;
          quads[q + 10] = overlay_tile;
          quads[q + 11] = m.sky_light / 15.0f;
          quads[q + 12] = m.block_light / 15.0f;
          quads[q + 13] = normal;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = base[0] + x[0] + du[0] + dv[0];
          quads[q + 1] = base[1] + x[1] + du[1] + dv[1];
          quads[q + 2] = base[2] + x[2] + du[2] + dv[2];
          quads[q + 3] = color[0];
          quads[q + 4] = color[1];
          quads[q + 5] = color[2];
          quads[q + 6] = color[3];
          quads[q + 7] = 0.0f;
          quads[q + 8] = 0.0f;
          quads[q + 9] = tile;
          quads[q + 10] = overlay_tile;
          quads[q + 11] = m.sky_light / 15.0f;
          quads[q + 12] = m.block_light / 15.0f;
          quads[q + 13] = normal;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = base[0] + x[0] + dv[0];
          quads[q + 1] = base[1] + x[1] + dv[1];
          quads[q + 2] = base[2] + x[2] + dv[2];
          quads[q + 3] = color[0];
          quads[q + 4] = color[1];
          quads[q + 5] = color[2];
          quads[q + 6] = color[3];
          quads[q + 7] = w;
          quads[q + 8] = 0.0f;
          quads[q + 9] = tile;
          quads[q + 10] = overlay_tile;
          quads[q + 11] = m.sky_light / 15.0f;
          quads[q + 12] = m.block_light / 15.0f;
          quads[q + 13] = normal;
          section->num_quads += 1;

          // Zero out mask
          for (int l = 0; l < h; l += 1) {
            for (int k = 0; k < w; k += 1) {
              mask[(j + l) + CHUNK_SIZE * (i + k)].material = 0;
            }
          }
        }
      }
    }
  }

  if (section->vertex_buffer != NULL) {
    wgpuBufferRelease(section->vertex_buffer);
    section->vertex_buffer = NULL;
  }

  section->vertex_buffer = frmwrk_device_create_buffer_init(
    device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Vertex Buffer",
      .content = (void *)quads,
      .content_size = section->num_quads * 4 * FLOATS_PER_VERTEX * sizeof(float),
      .usage = WGPUBufferUsage_Vertex,
    }
  );
}
