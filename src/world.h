#pragma once
#include <cglm/cglm.h>

#include "chunk.h"
#include "entity.h"

#define MAX_CHUNKS 1024
#define MAX_ENTITIES 1024

typedef struct World {
  Chunk *chunks[MAX_CHUNKS];
  int entity_count;
  Entity *entities[MAX_ENTITIES];
} World;

Chunk *world_chunk(World *world, int x, int z);
int world_add_chunk(World *world, Chunk *chunk);
void world_destroy_chunk(World *world, int cx, int cz);
int world_get_material(World *world, vec3 position);
Entity *world_entity(World *world, int id);
int world_add_entity(World *world, Entity *entity);
void world_destroy_entity(World *world, int id);
void world_get_sky_color(World *world, vec3 position, BiomeInfo *biome_info, vec3 sky_color);
void world_set_block(World *world, vec3 position, int material, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);
void world_target_block(World *world, vec3 position, vec3 look, float reach, vec3 target, vec3 normal, int *material);
void world_init_new_meshes(World *world, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);
void world_update_mesh_if_internal(World *world, ChunkSection *section, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);
