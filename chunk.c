#include "chunk.h"

#include <stdbool.h>
#include <stdio.h>

#include "framework.h"

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
  return chunk->sections[s].data[x + CHUNK_SIZE * (y + CHUNK_SIZE * z)];
}

void world_set_block(World *world, vec3 position, int material, WGPUDevice device) {
  int chunk_x = (int)floor(position[0] / CHUNK_SIZE);
  int chunk_z = (int)floor(position[2] / CHUNK_SIZE);
  Chunk *chunk = world_chunk(world, chunk_x, chunk_z);
  if (chunk == NULL) {
    return;
  }
  vec3 chunk_position;
  glm_vec3_sub(position, (vec3){chunk->x * CHUNK_SIZE, 0.0f, chunk->z * CHUNK_SIZE}, chunk_position);
  int x = (int)floor(chunk_position[0]);
  int y = positive_mod((int)floor(chunk_position[1]), 16);
  int z = (int)floor(chunk_position[2]);
  int s = (int)floor(chunk_position[1] / 16) + 4;
  chunk->sections[s].data[x + 16 * (y + 16 * z)] = material;

  // Update mesh
  chunk_section_update_mesh_if_internal(&chunk->sections[s], world, device);

  // Update neighbors if at upper edge
  if (x == CHUNK_SIZE - 1) {
    Chunk *chunk_x = world_chunk(world, chunk->x + 1, chunk->z);
    if (chunk_x) {
      chunk_section_update_mesh_if_internal(&chunk_x->sections[s], world, device);
    }
  }
  if (y == CHUNK_SIZE - 1 && s < Y_SECTIONS - 1) {
    chunk_section_update_mesh_if_internal(&chunk->sections[s + 1], world, device);
  }
  if (z == CHUNK_SIZE - 1) {
    Chunk *chunk_z = world_chunk(world, chunk->x, chunk->z + 1);
    if (chunk_z) {
      chunk_section_update_mesh_if_internal(&chunk_z->sections[s], world, device);
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
    int y = positive_mod((int)floor(chunk_location[1]), 16);
    int z = (int)floor(chunk_location[2]);
    int section = (int)floor(chunk_location[1] / 16) + 4;
    uint16_t mat = chunk->sections[section].data[x + 16 * (y + 16 * z)];
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

int face_material_between(int a, int b) {
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
  bool ta = false;
  bool tb = false;
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

void chunk_section_update_mesh_if_internal(ChunkSection *section, World *world, WGPUDevice device) {
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
  chunk_section_update_mesh(&chunk->sections[s], neighbors, device);
}

void chunk_section_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], WGPUDevice device) {
  section->num_quads = 0;
  int mask[16 * 16];
  vec3 base = {section->x * CHUNK_SIZE, section->y * CHUNK_SIZE, section->z * CHUNK_SIZE};
  for (int d = 0; d < 3; d += 1) {
    int u = (d + 1) % 3;
    int v = (d + 2) % 3;
    int x[3] = {0, 0, 0};

    // Go over all the slices in this dimension
    for (x[d] = 0; x[d] < 16; x[d] += 1) {
      // Make a mask
      for (x[u] = 0; x[u] < 16; x[u] += 1) {
        for (x[v] = 0; x[v] < 16; x[v] += 1) {
          int above = section->data[x[0] + 16 * (x[1] + 16 * x[2])];
          int xb[3] = {x[0], x[1], x[2]};
          xb[d] -= 1;
          int below;
          if (xb[d] < 0) {
            if (neighbors[d] == NULL) {
              below = 0;
            } else {
              xb[d] = 15;
              below = neighbors[d]->data[xb[0] + 16 * (xb[1] + 16 * xb[2])];
            }
          } else {
            below = section->data[xb[0] + 16 * (xb[1] + 16 * xb[2])];
          }
          mask[x[v] + 16 * x[u]] = face_material_between(below, above);
        }
      }

      // Greedily find a quad where the mask is the same value and repeat
      for (int j = 0; j < 16; j += 1) {
        for (int i = 0; i < 16;) {
          int m = mask[j + 16 * i];
          if (m == 0) {
            i += 1;
            continue;
          }
          int w = 1;
          while (mask[j + 16 * (i + w)] == m && i + w < 16) {
            w += 1;
          }
          int h = 1;
          bool done = false;
          for (; j + h < 16; h += 1) {
            for (int k = 0; k < w; k += 1) {
              if (mask[(j + h) + 16 * (i + k)] != m) {
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
          int material = abs(m);
          quads[q + 0] = base[0] + x[0];
          quads[q + 1] = base[1] + x[1];
          quads[q + 2] = base[2] + x[2];
          quads[q + 3] = 1.0f;
          quads[q + 4] = 0.0f;
          quads[q + 5] = 0.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = w;
          quads[q + 8] = h;
          quads[q + 9] = material;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = base[0] + x[0] + du[0];
          quads[q + 1] = base[1] + x[1] + du[1];
          quads[q + 2] = base[2] + x[2] + du[2];
          quads[q + 3] = 0.0f;
          quads[q + 4] = 1.0f;
          quads[q + 5] = 0.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = 0.0f;
          quads[q + 8] = h;
          quads[q + 9] = material;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = base[0] + x[0] + du[0] + dv[0];
          quads[q + 1] = base[1] + x[1] + du[1] + dv[1];
          quads[q + 2] = base[2] + x[2] + du[2] + dv[2];
          quads[q + 3] = 1.0f;
          quads[q + 4] = 1.0f;
          quads[q + 5] = 1.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = 0.0f;
          quads[q + 8] = 0.0f;
          quads[q + 9] = material;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = base[0] + x[0] + dv[0];
          quads[q + 1] = base[1] + x[1] + dv[1];
          quads[q + 2] = base[2] + x[2] + dv[2];
          quads[q + 3] = 0.0f;
          quads[q + 4] = 0.0f;
          quads[q + 5] = 1.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = w;
          quads[q + 8] = 0.0f;
          quads[q + 9] = material;
          section->num_quads += 1;

          // Zero out mask
          for (int l = 0; l < h; l += 1) {
            for (int k = 0; k < w; k += 1) {
              mask[(j + l) + 16 * (i + k)] = false;
            }
          }
        }
      }
    }
  }

  if (section->vertex_buffer != NULL) {
    wgpuBufferRelease(section->vertex_buffer);
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
