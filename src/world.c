#include "world.h"

#include "logging.h"

int positive_mod(int a, int b) {
  int result = a % b;
  if (result < 0) {
    result += b;
  }
  return result;
}

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

void world_destroy_chunk(World *world, int cx, int cz) {
  for (int i = 0; i < MAX_CHUNKS; i += 1) {
    if (world->chunks[i] != NULL && world->chunks[i]->x == cx && world->chunks[i]->z == cz) {
      chunk_destroy(world->chunks[i]);
      world->chunks[i] = NULL;
    }
  }
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

Entity *world_entity(World *world, int id) {
  for (int i = 0; i < MAX_ENTITIES; i += 1) {
    if (world->entities[i] == NULL) {
      continue;
    }
    Entity *entity = world->entities[i];
    if (entity->id == id) {
      return entity;
    }
  }
  return NULL;
}

int world_add_entity(World *world, Entity *entity) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (world->entities[i] != NULL && world->entities[i]->id == entity->id) {
      entity_destroy(world->entities[i]);
      world->entities[i] = entity;
      return i;
    }
  }

  for (int i = 0; i < MAX_ENTITIES; i += 1) {
    if (world->entities[i] == NULL) {
      world->entities[i] = entity;
      return i;
    }
  }

  FATAL("No space for entity");
  assert(false);
  return -1;
}

void world_destroy_entity(World *world, int id) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (world->entities[i] != NULL && world->entities[i]->id == id) {
      entity_destroy(world->entities[i]);
      world->entities[i] = NULL;
    }
  }
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
  world_update_mesh_if_internal(world, &chunk->sections[s], block_info, biome_info, device);

  // Update neighbors if at upper edge
  if (x == CHUNK_SIZE - 1) {
    Chunk *chunk_x = world_chunk(world, chunk->x + 1, chunk->z);
    if (chunk_x) {
      world_update_mesh_if_internal(world, &chunk_x->sections[s], block_info, biome_info, device);
    }
  }
  if (y == CHUNK_SIZE - 1 && s < Y_SECTIONS - 1) {
    world_update_mesh_if_internal(world, &chunk->sections[s + 1], block_info, biome_info, device);
  }
  if (z == CHUNK_SIZE - 1) {
    Chunk *chunk_z = world_chunk(world, chunk->x, chunk->z + 1);
    if (chunk_z) {
      world_update_mesh_if_internal(world, &chunk_z->sections[s], block_info, biome_info, device);
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

void world_update_mesh_if_internal(World *world, ChunkSection *section, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device) {
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
