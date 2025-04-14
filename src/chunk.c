#include "chunk.h"

#include <stdbool.h>
#include <assert.h>

#include "cglm/call/mat4x3.h"
#include "cglm/vec3.h"
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
  bool ta = block_info[abs(a)].transparent || !block_info[abs(a)].fullblock;
  bool tb = block_info[abs(b)].transparent || !block_info[abs(b)].fullblock;
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

#define VEC4_EX(vec4) { vec4[0], vec4[1], vec4[2], vec4[3] }
void quad(ChunkSection *section, uint16_t material, vec4 color, int sky_light, int block_light, int normal, vec3 pos[4], vec2 uv[4]) {
  ChunkVertex* qs = (ChunkVertex*)quads;
  int s = section->num_quads * 4;
  ChunkVertex template = {
    .position = {0.0f, 0.0f, 0.0f},
    .coord = {0.0f, 0.0f},

    .color = VEC4_EX(color),
    .sky_light = sky_light,
    .block_light = block_light,
    .normal = normal,
    .material = material,
    .overlay_material = 0,
  };

  for (int i = 0; i < 4; i++) {
    qs[s + i] = template;
    glm_vec3_copy(pos[i], qs[s + i].position);
    glm_vec2_copy(uv[i], qs[s + i].coord);
  }

  section->num_quads++;
}

void draw_quad(ChunkSection *section, vec3 base, vec3 x, vec3 du, vec3 dv, vec4 color, vec2 uv_base, vec2 uv_du, vec2 uv_dv, uint16_t texture, float sky_light, float block_light, int normal, bool swap_corners) {
  int overlay_tile = 0;
  int q = section->num_quads * 4 * FLOATS_PER_VERTEX;
  quads[q + 0] = base[0] + x[0] + (swap_corners ? du[0] + dv[0] : 0);
  quads[q + 1] = base[1] + x[1] + (swap_corners ? du[1] + dv[1] : 0);
  quads[q + 2] = base[2] + x[2] + (swap_corners ? du[2] + dv[2] : 0);
  quads[q + 3] = color[0];
  quads[q + 4] = color[1];
  quads[q + 5] = color[2];
  quads[q + 6] = color[3];
  quads[q + 7] = uv_base[0] + (swap_corners ? uv_du[0] + uv_dv[0] : 0);
  quads[q + 8] = uv_base[1] + (swap_corners ? uv_du[1] + uv_dv[1] : 0);
  quads[q + 9] = texture;
  quads[q + 10] = overlay_tile;
  quads[q + 11] = sky_light;
  quads[q + 12] = block_light;
  quads[q + 13] = normal;
  q += FLOATS_PER_VERTEX;
  quads[q + 0] = base[0] + x[0] + du[0];
  quads[q + 1] = base[1] + x[1] + du[1];
  quads[q + 2] = base[2] + x[2] + du[2];
  quads[q + 3] = color[0];
  quads[q + 4] = color[1];
  quads[q + 5] = color[2];
  quads[q + 6] = color[3];
  quads[q + 7] = uv_base[0] + uv_du[0];
  quads[q + 8] = uv_base[1] + uv_du[1];
  quads[q + 9] = texture;
  quads[q + 10] = overlay_tile;
  quads[q + 11] = sky_light;
  quads[q + 12] = block_light;
  quads[q + 13] = normal;
  q += FLOATS_PER_VERTEX;
  quads[q + 0] = base[0] + x[0] + (swap_corners ? 0 : du[0] + dv[0]);
  quads[q + 1] = base[1] + x[1] + (swap_corners ? 0 : du[1] + dv[1]);
  quads[q + 2] = base[2] + x[2] + (swap_corners ? 0 : du[2] + dv[2]);
  quads[q + 3] = color[0];
  quads[q + 4] = color[1];
  quads[q + 5] = color[2];
  quads[q + 6] = color[3];
  quads[q + 7] = uv_base[0] + (swap_corners ? 0 : uv_du[0] + uv_dv[0]);
  quads[q + 8] = uv_base[1] + (swap_corners ? 0 : uv_du[1] + uv_dv[1]);
  quads[q + 9] = texture;
  quads[q + 10] = overlay_tile;
  quads[q + 11] = sky_light;
  quads[q + 12] = block_light;
  quads[q + 13] = normal;
  q += FLOATS_PER_VERTEX;
  quads[q + 0] = base[0] + x[0] + dv[0];
  quads[q + 1] = base[1] + x[1] + dv[1];
  quads[q + 2] = base[2] + x[2] + dv[2];
  quads[q + 3] = color[0];
  quads[q + 4] = color[1];
  quads[q + 5] = color[2];
  quads[q + 6] = color[3];
  quads[q + 7] = uv_base[0] + uv_dv[0];
  quads[q + 8] = uv_base[1] + uv_dv[1];
  quads[q + 9] = texture;
  quads[q + 10] = overlay_tile;
  quads[q + 11] = sky_light;
  quads[q + 12] = block_light;
  quads[q + 13] = normal;
  q += FLOATS_PER_VERTEX;
  section->num_quads += 1;
}

void cubiod(ChunkSection *section, vec3 base, MeshCuboid element, BlockInfo *block_info, BiomeInfo *biome_info, int sky_light, int block_light) {
  vec4 block_color = {1.0f, 1.0f, 1.0f, 1.0f};
  vec4 no_color = {1.0f, 1.0f, 1.0f, 1.0f};
  if (block_info->grass) {
    glm_vec3_copy(biome_info->grass_color, block_color);
  } else if (block_info->foliage) {
    glm_vec3_copy(biome_info->foliage_color, block_color);
  } else if (block_info->dry_foliage) {
    glm_vec3_copy(biome_info->dry_foliage_color, block_color);
  }

  vec3 a0;
  glm_vec3_scale(element.from, 1.0/16.0, a0);
  vec3 a1;
  glm_vec3_scale(element.to, 1.0/16.0, a1);

  /*
  3---------2
  |         |
  |         |
  |         |
  0---------1
  */

  // Top face
  if (element.up.texture != 0) {
    int normal = 2;
    vec4 uv;
    glm_vec4_scale(element.up.uv, 1.0/16.0, uv);

    vec3 pos[4] = {
      {a1[0], a1[1], a1[2]}, // 2
      {a1[0], a1[1], a0[2]}, // 1
      {a0[0], a1[1], a0[2]}, // 0
      {a0[0], a1[1], a1[2]}, // 3
    };
    vec2 coord[4] = {
      {uv[2], uv[3]},
      {uv[2], uv[1]},
      {uv[0], uv[1]},
      {uv[0], uv[3]},
    };

    for (int i = 0; i < 4; i++) {
      glm_vec3_add(base, pos[i], pos[i]);
    }
    quad(section, element.up.texture, element.up.tint_index != 0 ? block_color : no_color, sky_light, block_light, normal, pos, coord);
  }
  // Bottom face
  if (element.down.texture != 0) {
    int normal = -2;
    vec4 uv;
    glm_vec4_scale(element.down.uv, 1.0/16.0, uv);

    vec3 pos[4] = {
      {a0[0], a0[1], a0[2]}, // 0
      {a1[0], a0[1], a0[2]}, // 1
      {a1[0], a0[1], a1[2]}, // 2
      {a0[0], a0[1], a1[2]}, // 3
    };
    vec2 coord[4] = {
      {uv[0], uv[1]},
      {uv[2], uv[1]},
      {uv[2], uv[3]},
      {uv[0], uv[3]},
    };

    for (int i = 0; i < 4; i++) {
      glm_vec3_add(base, pos[i], pos[i]);
    }
    quad(section, element.down.texture, element.down.tint_index != 0 ? block_color : no_color, sky_light, block_light, normal, pos, coord);
  }
  // North face
  if (element.north.texture != 0) {
    int normal = -3;
    vec4 uv;
    glm_vec4_scale(element.north.uv, 1.0/16.0, uv);

    vec3 pos[4] = {
      {a1[0], a1[1], a0[2]}, // 2
      {a1[0], a0[1], a0[2]}, // 1
      {a0[0], a0[1], a0[2]}, // 0
      {a0[0], a1[1], a0[2]}, // 3
    };
    vec2 coord[4] = {
      {uv[2], uv[3]}, // 2
      {uv[2], uv[1]}, // 1
      {uv[0], uv[1]}, // 0
      {uv[0], uv[3]}, // 3
    };

    for (int i = 0; i < 4; i++) {
      glm_vec3_add(base, pos[i], pos[i]);
    }
    quad(section, element.north.texture, element.north.tint_index != 0 ? block_color : no_color, sky_light, block_light, normal, pos, coord);
  }
  // South face
  if (element.south.texture != 0) {
    int normal = 3;
    vec4 uv;
    glm_vec4_scale(element.south.uv, 1.0/16.0, uv);

    vec3 pos[4] = {
      {a0[0], a0[1], a1[2]}, // 0
      {a1[0], a0[1], a1[2]}, // 1
      {a1[0], a1[1], a1[2]}, // 2
      {a0[0], a1[1], a1[2]}, // 3
    };
    vec2 coord[4] = {
      {uv[0], uv[1]}, // 0
      {uv[2], uv[1]}, // 1
      {uv[2], uv[3]}, // 2
      {uv[0], uv[3]}, // 3
    };

    for (int i = 0; i < 4; i++) {
      glm_vec3_add(base, pos[i], pos[i]);
    }
    quad(section, element.south.texture, element.south.tint_index != 0 ? block_color : no_color, sky_light, block_light, normal, pos, coord);
  }
  // East face
  if (element.east.texture != 0) {
    int normal = 1;
    vec4 uv;
    glm_vec4_scale(element.east.uv, 1.0/16.0, uv);

    vec3 pos[4] = {
      {a1[0], a0[1], a0[2]}, // 0
      {a1[0], a1[1], a0[2]}, // 1
      {a1[0], a1[1], a1[2]}, // 2
      {a1[0], a0[1], a1[2]}, // 3
    };
    vec2 coord[4] = {
      {uv[2], uv[3]}, // 2
      {uv[2], uv[1]}, // 1
      {uv[0], uv[1]}, // 0
      {uv[0], uv[3]}, // 3
    };

    for (int i = 0; i < 4; i++) {
      glm_vec3_add(base, pos[i], pos[i]);
    }
    quad(section, element.east.texture, element.east.tint_index != 0 ? block_color : no_color, sky_light, block_light, normal, pos, coord);
  }
  // West face
  if (element.west.texture != 0) {
    int normal = -1;
    vec4 uv;
    glm_vec4_scale(element.west.uv, 1.0/16.0, uv);

    vec3 pos[4] = {
      {a0[0], a1[1], a1[2]}, // 2
      {a0[0], a1[1], a0[2]}, // 1
      {a0[0], a0[1], a0[2]}, // 0
      {a0[0], a0[1], a1[2]}, // 3
    };
    vec2 coord[4] = {
      {uv[0], uv[1]}, // 0
      {uv[2], uv[1]}, // 1
      {uv[2], uv[3]}, // 2
      {uv[0], uv[3]}, // 3
    };

    for (int i = 0; i < 4; i++) {
      glm_vec3_add(base, pos[i], pos[i]);
    }
    quad(section, element.west.texture, element.west.tint_index != 0 ? block_color : no_color, sky_light, block_light, normal, pos, coord);
  }
}

void draw_cubiod(ChunkSection *section, vec3 base, int bx, int by, int bz, MeshCuboid element, BlockInfo *block_info, BiomeInfo *biome_info) {
  vec3 b = {bx, by, bz};
  vec3 from, to;
  glm_vec3_copy(element.from, from);
  glm_vec3_copy(element.to, to);
  vec3 x_start;
  glm_vec3_scale(from, 1.0/16.0, x_start);
  glm_vec3_add(x_start, b, x_start);
  vec3 x_end;
  glm_vec3_scale(to, 1.0/16.0, x_end);
  glm_vec3_add(x_end, b, x_end);
  vec3 delta;
  glm_vec3_sub(x_end, x_start, delta);
  float sky_light = 1.0;
  float block_light = 1.0;

  for (int d = 0; d < 3; d++) {
    for (int side = -1; side <= 1; side += 2) {
      int u;
      int v;
      if (d == 0) {
        u = 2;
        v = 1;
      } else if (d == 1) {
        u = 0;
        v = 2;
      } else {
        u = 0;
        v = 1;
      }

      float normal = side * (d + 1);

      MeshFace *face;
      if (d == 0 && side == 1) {
        face = &element.east;
      } else if (d == 0 && side == -1) {
        face = &element.west;
      } else if (d == 2 && side == 1) {
        face = &element.south;
      } else if (d == 2 && side == -1) {
        face = &element.north;
      } else if (d == 1 && side == 1) {
        face = &element.up;
      } else if (d == 1 && side == -1) {
        face = &element.down;
      } else {
        assert(false);
      }

      vec3 x = {0};
      glm_vec3_copy(x_start, x);
      if (side == 1) {
        x[d] = x_end[d];
      }

      vec3 du = {0};
      du[u] = delta[u];
      vec3 dv = {0};
      dv[v] = delta[v];

      vec4 uv;
      glm_vec4_copy(face->uv, uv);
      glm_vec4_scale(uv, 1.0 / 16.0, uv);

      vec2 uv_base = {uv[0], uv[1]};
      vec2 uv_du = {uv[2] - uv[0], 0};
      // Flip the texture coordinate y since texture origin is top left
      vec2 uv_dv = {0, -(uv[3] - uv[1])};
      glm_vec4_copy(element.north.uv, uv);

      bool swap_corners = false;
      // Flip things when it's on the back face
      if (side < 0) {
        swap_corners = !swap_corners;
        uv_du[0] = -uv_du[0];
      }
      // Also flip things when it's the x dimension
      if (d == 0 || d == 1) {
        swap_corners = !swap_corners;
        uv_du[0] = -uv_du[0];
      }

      vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
      ivec3 biome_x = {floor(x[0] / 4.0), floor(x[1] / 4.0), floor(x[2] / 4.0)};
      int biome_index = section->biome_data[biome_x[0] + 4 * (biome_x[2] + 4 * biome_x[1])];
      BiomeInfo biome = biome_info[biome_index];
      if (face->tint_index == 1) {
        if (block_info->grass) {
          glm_vec3_copy(biome.grass_color, color);
        } else if (block_info->foliage) {
          glm_vec3_copy(biome.foliage_color, color);
        } else if (block_info->dry_foliage) {
          glm_vec3_copy(biome.dry_foliage_color, color);
        }
      }

      // WARN("uv_base %f %f", uv_base[0], uv_base[1]);
      // WARN("uv_du %f %f", uv_du[0], uv_du[1]);
      // WARN("uv_dv %f %f", uv_dv[0], uv_dv[1]);
      draw_quad(section, base, x, du, dv, color, uv_base, uv_du, uv_dv, face->texture, sky_light, block_light, normal, swap_corners);
    }
  }
}

void chunk_section_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device) {
  section->num_quads = 0;
  MaskInfo mask[16 * 16];
  vec3 base = {section->x * CHUNK_SIZE, section->y * CHUNK_SIZE, section->z * CHUNK_SIZE};

  // Create full blocks
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

          BlockInfo info = abs(m.material) > 65535 ? block_info[0] : block_info[abs(m.material)]; // Fails here!!!!

          for (size_t el = 0; el < info.mesh.num_elements; el++) {
            MeshFace face = {};
            if (info.fullblock) {
              if (d == 1 && m.material > 0) {
                face = info.mesh.elements[el].up;
              } else if (d == 1 && m.material < 0) {
                face = info.mesh.elements[el].down;
              } else if (d == 0 && m.material > 0) {
                face = info.mesh.elements[el].north;
              } else if (d == 0 && m.material < 0) {
                face = info.mesh.elements[el].south;
              } else if (d == 2 && m.material > 0) {
                face = info.mesh.elements[el].east;
              } else if (d == 2 && m.material < 0) {
                face = info.mesh.elements[el].west;
              } else {
                face = info.mesh.elements[el].up;
              }
            }

            if (!face.texture) {
              continue;
            }

            // Get the biome color
            vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
            ivec3 biome_x = {floor(x[0] / 4.0), floor(x[1] / 4.0), floor(x[2] / 4.0)};
            int biome_index = section->biome_data[biome_x[0] + 4 * (biome_x[2] + 4 * biome_x[1])];
            BiomeInfo biome = biome_info[biome_index];
            if (face.tint_index == 1) {
              if (info.grass) {
                glm_vec3_copy(biome.grass_color, color);
              } else if (info.foliage) {
                glm_vec3_copy(biome.foliage_color, color);
              } else if (info.dry_foliage) {
                glm_vec3_copy(biome.dry_foliage_color, color);
              }
            }

            int normal = (m.material > 0 ? 1 : -1) * (d + 1);
            vec4 uv = {0, 0, w, h};
            vec2 uv_base = {uv[0], uv[1]};
            vec2 uv_du = {uv[2] - uv[0], 0};
            // Flip the texture coordinate y since texture origin is top left
            vec2 uv_dv = {0, -(uv[3] - uv[1])};

            vec3 vec_x = {x[0], x[1], x[2]};
            vec3 vec_du = {du[0], du[1], du[2]};
            vec3 vec_dv = {dv[0], dv[1], dv[2]};
            bool swap_corners = false;
            // Flip things when it's on the back face
            if (m.material < 0) {
              swap_corners = !swap_corners;
              uv_du[0] = -uv_du[0];
            }
            // Also flip things when it's the x dimension
            if (d == 0) {
              swap_corners = !swap_corners;
              uv_du[0] = -uv_du[0];
            }
            draw_quad(section, base, vec_x, vec_du, vec_dv, color, uv_base, uv_du, uv_dv, face.texture, m.sky_light / 15.0f, m.block_light / 15.0f, normal, swap_corners);
          }
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

  // Create non-full blocks
  for (int x = 0; x < CHUNK_SIZE; x++) {
    for (int y = 0; y < CHUNK_SIZE; y++) {
      for (int z = 0; z < CHUNK_SIZE; z++) {
        int index = x + CHUNK_SIZE * (z + CHUNK_SIZE * y);
        int state = section->data[index];
        int sky_light = section->sky_light[index];
        int block_light = section->block_light[index];
        BlockInfo info = block_info[state];
        ivec3 biome_x = {floor(x / 4.0), floor(y / 4.0), floor(z / 4.0)};
        int biome_index = section->biome_data[biome_x[0] + 4 * (biome_x[2] + 4 * biome_x[1])];
        vec3 block_base = {x, y, z};
        glm_vec3_add(base, block_base, block_base);
        BiomeInfo *block_biome_info = &biome_info[biome_index];

        if (!info.fullblock) {
          for (size_t el = 0; el < info.mesh.num_elements; el++) {
            cubiod(section, block_base, info.mesh.elements[el], &info, block_biome_info, sky_light, block_light);
            // draw_cubiod(section, base, x, y, z, info.mesh.elements[el], &info, biome_info);
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
