#pragma once

#include <cglm/cglm.h>
#include <wgpu.h>

#define FLOATS_PER_VERTEX 14
#define CHUNK_SIZE 16

// y goes from -64 to 320
// So each chunk is 24 sections tall
#define Y_SECTIONS 24

typedef struct MeshFace {
  vec4 uv;
  uint16_t texture;
  int tint_index;
  bool cull;
} MeshFace;

typedef struct MeshCuboid {
  vec3 from;
  vec3 to;
  MeshFace up;
  MeshFace down;
  MeshFace north;
  MeshFace south;
  MeshFace east;
  MeshFace west;
} MeshCuboid;

typedef struct Mesh {
  vec3 rotation;
  MeshCuboid* elements;
  size_t num_elements;
} Mesh;

typedef struct BlockInfo {
  char* name;
  char* type;
  int state;
  bool transparent;
  bool passable;
  bool grass;
  bool foliage;
  bool dry_foliage;
  bool fullblock;
  Mesh mesh;
} BlockInfo;

typedef struct BiomeInfo {
  float temperature;
  float downfall;
  vec3 fog_color;
  vec3 water_color;
  vec3 water_fog_color;
  vec3 sky_color;
  bool swamp;
  bool custom_grass_color;
  vec3 grass_color;
  bool custom_foliage_color;
  vec3 foliage_color;
  vec3 dry_foliage_color;
} BiomeInfo;

#pragma pack(push, 1)
typedef struct ChunkVertex {
  vec3 position;
  vec4 color;
  vec2 coord;
  float material;
  float overlay_material;
  float sky_light;
  float block_light;
  float normal;
} ChunkVertex;
#pragma pack(pop)

typedef struct CompressedChunkVertex {
  vec3 position;
  uint16_t material;
  uint8_t color_r;
  uint8_t color_g;
  uint8_t color_b;
  uint8_t color_a;
  unsigned int sky_light : 4;
  unsigned int block_light : 4;
  int normal : 2;
  vec2 coord;
} CompressedChunkVertex;

typedef struct ChunkSection {
  int x;
  int y;
  int z;
  int data[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
  uint8_t sky_light[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
  uint8_t block_light[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
  int biome_data[4 * 4 * 4];
  WGPUBuffer vertex_buffer;
  int num_quads;
} ChunkSection;

typedef struct Chunk {
  int x;
  int z;
  ChunkSection sections[Y_SECTIONS];
} Chunk;

void chunk_destroy_buffers(Chunk *chunk);
void chunk_destroy(Chunk *chunk);

void chunk_section_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);
