#include <cglm/cglm.h>
#include <wgpu.h>
#include "datatypes.h"

#define FLOATS_PER_VERTEX 14
#define MAX_CHUNKS 1024
#define CHUNK_SIZE 16

// y goes from -64 to 320
// So each chunk is 24 sections tall
#define Y_SECTIONS 24

typedef struct MeshCuboid {
  vec3 from;
  vec3 to;
  uint16_t up_texture;
  vec4 up_uv;
  uint16_t down_texture;
  vec4 down_uv;
  uint16_t north_texture;
  vec4 north_uv;
  uint16_t south_texture;
  vec4 south_uv;
  uint16_t east_texture;
  vec4 east_uv;
  uint16_t west_texture;
  vec4 west_uv;
} MeshCuboid;

typedef struct Mesh {
  vec3 rotation;
  MeshCuboid* elements;
  size_t num_elements;
} Mesh;

typedef struct BlockInfo {
  String name;
  String type;
  int state;
  bool transparent;
  bool passable;
  bool grass;
  bool foliage;
  bool dry_foliage;
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

void chunk_destroy(Chunk *chunk);

typedef struct World {
  Chunk *chunks[MAX_CHUNKS];
} World;

void chunk_section_update_mesh_if_internal(ChunkSection *section, World *world, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);
void chunk_section_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);

Chunk *world_chunk(World *world, int x, int z);
int world_add_chunk(World *world, Chunk *chunk);
int world_get_material(World *world, vec3 position);
void world_get_sky_color(World *world, vec3 position, BiomeInfo *biome_info, vec3 sky_color);
void world_set_block(World *world, vec3 position, int material, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);
void world_target_block(World *world, vec3 position, vec3 look, float reach, vec3 target, vec3 normal, int *material);
void world_init_new_meshes(World *world, BlockInfo *block_info, BiomeInfo *biome_info, WGPUDevice device);
