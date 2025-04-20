#pragma once

#include <wgpu.h>
#include <cglm/cglm.h>
#include <GLFW/glfw3.h>

#include "mcapi/mcapi.h"
#include "texture_sheet.h"
#include "world.h"

#define MAX_BLOCKS 65536
#define MAX_BIOMES 128

#define TEXTURE_SIZE 16
// There are 899 textures in 1.20.6 - Should fit in 32*32 = 1024 sheet
// #define TEXTURE_TILES 32
// #define TEXTURE_TILES 40
#define TEXTURE_TILES 40

// The max number of blocks that can be being broken at the same time
#define MAX_CONCURRENT_BLOCK_BREAKING 50

const float COLLISION_EPSILON = 0.001f;
const float TURN_SPEED = 0.002f;
const float TICKS_PER_SECOND = 20.0f;

typedef struct Uniforms {
  mat4 view;
  mat4 projection;
  float internal_sky_max;
} Uniforms;

typedef struct SkyUniforms {
  vec3 look;
  float aspect;
  vec3 sky_color;
  float time_of_day;
} SkyUniforms;

typedef struct BlockSelectedUniforms {
  mat4 view;
  mat4 projection;
  vec3 position;
} BlockSelectedUniforms;

typedef struct SkyRenderer {
  WGPUShaderModule shader_module;
  WGPUBindGroup bind_group;
  WGPURenderPipeline render_pipeline;
  WGPUPipelineLayout pipeline_layout;
  WGPUBuffer uniform_buffer;
  SkyUniforms uniforms;
  WGPUBuffer vertex_buffer;
} SkyRenderer;

typedef struct BlockSelectedRenderer {
  WGPUShaderModule shader_module;
  WGPUBindGroup bind_group;
  WGPURenderPipeline render_pipeline;
  WGPUPipelineLayout pipeline_layout;
  BlockSelectedUniforms uniforms;
  WGPUBuffer uniform_buffer;
  WGPUBuffer vertex_buffer;
} BlockSelectedRenderer;

typedef struct EntityUniforms {
  mat4 view;
  mat4 projection;
  vec3 position;
} EntityUniforms;

typedef struct {
    float position[3];
} EntityInstance;

typedef struct EntityRenderer {
  WGPUShaderModule shader_module;
  WGPUBindGroup bind_group;
  WGPURenderPipeline render_pipeline;
  WGPUPipelineLayout pipeline_layout;
  EntityUniforms uniforms;
  WGPUBuffer uniform_buffer;
  WGPUBuffer vertex_buffer;
  WGPUBuffer instance_buffer;
  EntityInstance instance_data[MAX_ENTITIES];
} EntityRenderer;

typedef struct BlockBeingBroken {
  ivec3 position;
  int stage;
} BlockBeingBroken;

typedef struct Game {
  WGPUInstance instance;
  WGPUSurface surface;
  WGPUSurfaceCapabilities surface_capabilities;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUQueue queue;
  WGPUSurfaceConfiguration config;
  WGPUTextureDescriptor depth_texture_descriptor;
  WGPUTexture depth_texture;
  WGPUBuffer index_buffer;
  WGPUBindGroup bind_group;
  WGPURenderPipeline render_pipeline;
  WGPURenderPipeline render_pipeline_transparent;
  WGPUBuffer uniform_buffer;
  Uniforms uniforms;
  WGPUPipelineLayout pipeline_layout;
  WGPUShaderModule shader_module;
  SkyRenderer sky_renderer;
  BlockSelectedRenderer block_selected_renderer;
  EntityRenderer entity_renderer;
  GLFWwindow *window;
  float eye_height;
  vec3 size;
  bool keys[GLFW_KEY_LAST + 1];
  bool mouse_captured;
  vec2 last_mouse;
  BlockInfo block_info[MAX_BLOCKS];
  BiomeInfo biome_info[MAX_BIOMES];
  float elevation;
  float walking_speed;
  float fall_speed;
  bool on_ground;
  vec3 position;
  vec3 last_position;
  vec3 velocity;
  vec3 up;
  vec3 forward;
  vec3 right;
  vec3 look;
  long time_of_day;
  World world;
  mcapiConnection *conn;
  TextureSheet texture_sheet;
  unsigned char texture_sheet_data[TEXTURE_SIZE * TEXTURE_SIZE * TEXTURE_TILES * TEXTURE_TILES * 4];
  double target_render_time;
  double last_render_time;
  double target_tick_time;
  double last_tick_time;
  double current_time;
  int target_material;

  double block_breaking_start;
  ivec3 block_breaking_position;
  mcapiBlockFace block_breaking_face;
  int block_breaking_seq_num;

  int destroy_stage_textures[10];
  WGPUBuffer block_overlay_vertex_buffer;
  float block_overlay_buffer[FLOATS_PER_VERTEX * 4 * 6 * MAX_CONCURRENT_BLOCK_BREAKING];

  int number_of_blocks_being_broken;
  BlockBeingBroken blocks_being_broken[MAX_CONCURRENT_BLOCK_BREAKING];
} Game;
