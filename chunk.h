#include "wgpu/webgpu.h"

typedef struct ChunkSection {
  int x;
  int z;
  uint16_t y;
  uint16_t data[16*16*16];
  WGPUBuffer vertex_buffer;
  int num_quads;
} ChunkSection;

void chunk_section_buffer_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], WGPUDevice device);

typedef struct Chunk {
  int x;
  int z;
  ChunkSection sections[16];
} Chunk;

