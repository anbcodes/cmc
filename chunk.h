#include "wgpu/webgpu.h"
#include "cglm/include/cglm/cglm.h"

#define FLOATS_PER_VERTEX 10
#define MAX_CHUNKS 1024
#define CHUNK_SIZE 16

// y goes from -64 to 320
// So each chunk is 24 sections tall
#define Y_SECTIONS 24

typedef struct ChunkSection {
  int x;
  int y;
  int z;
  uint16_t data[16*16*16];
  WGPUBuffer vertex_buffer;
  int num_quads;
} ChunkSection;

typedef struct Chunk {
  int x;
  int z;
  ChunkSection sections[Y_SECTIONS];
} Chunk;

typedef struct World {
  Chunk *chunks[MAX_CHUNKS];
} World;

void chunk_section_update_mesh_if_internal(ChunkSection *section, World *world, WGPUDevice device);
void chunk_section_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], WGPUDevice device);

Chunk *world_chunk(World *world, int x, int z);
void world_set_block(World *world, vec3 position, int material, WGPUDevice device);
void world_target_block(World *world, vec3 position, vec3 look, float reach, vec3 target, vec3 normal, int *material);
