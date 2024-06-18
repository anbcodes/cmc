#include "wgpu/webgpu.h"
#include "cglm/include/cglm/cglm.h"

#define FLOATS_PER_VERTEX 10
#define MAX_CHUNKS 1024
#define CHUNK_SIZE 16

typedef struct ChunkSection {
  int x;
  int y;
  int z;
  uint16_t data[16*16*16];
  WGPUBuffer vertex_buffer;
  int num_quads;
} ChunkSection;

void chunk_section_buffer_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], WGPUDevice device);

typedef struct Chunk {
  int x;
  int z;
  // y goes from -64 to 320
  // So each chunk is 24 sections tall
  ChunkSection sections[24];
} Chunk;

typedef struct World {
  Chunk *chunks[MAX_CHUNKS];
} World;

Chunk *world_chunk(World *world, int x, int z);

void world_target_block(World *world, vec3 position, vec3 look, float reach, vec3 target, vec3 normal, int *material);
