#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cglm/cglm.h>
#include <wgpu.h>
#include <yyjson.h>

#include "cglm/mat4.h"
#include "logging.h"
// #include "cJSON.h"
#include "chunk.h"
#include "datatypes.h"
#include "framework.h"
#include "lodepng/lodepng.h"
#include "macros.h"
#include "mcapi/chunk.h"
#include "mcapi/entity.h"
#include "mcapi/mcapi.h"
#include "mcapi/protocol.h"
#include "nbt.h"
#include "webgpu.h"

#if defined(GLFW_EXPOSE_NATIVE_COCOA)
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_X11
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#define LOG_PREFIX "[triangle]"
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
  unsigned char texture_sheet[TEXTURE_SIZE * TEXTURE_SIZE * TEXTURE_TILES * TEXTURE_TILES * 4];
  int next_texture_loc;
  yyjson_doc *blocks;
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

Game game = {
  .walking_speed = 4.317f,
  .position = {0.0f, 20.0f, 0.0f},
  .up = {0.0f, 1.0f, 0.0f},
  .forward = {0.0f, 0.0f, 1.0f},
  .right = {-1.0f, 0.0f, 0.0f},
  .look = {0.0f, 0.0f, 1.0f},
  .eye_height = 1.62,
  .size = {0.6, 1.8, 0.6},
  .next_texture_loc = 1,
};

unsigned char *load_image(const char *filename, unsigned *width, unsigned *height) {
  unsigned char *png = 0;
  size_t pngsize, error;
  error = lodepng_load_file(&png, &pngsize, filename);
  if (!error) {
    unsigned char *image = 0;
    error = lodepng_decode32(&image, width, height, png, pngsize);
    free(png);
    if (!error) {
      return image;
    }
  }
  return NULL;
}

unsigned save_image(const char *filename, unsigned char *image, unsigned width, unsigned height) {
  unsigned char *png = NULL;
  size_t pngsize;

  unsigned error = lodepng_encode32(&png, &pngsize, image, width, height);
  if (!error) {
    error = lodepng_save_file(png, pngsize, filename);
  }

  free(png);
  return error;
}

yyjson_doc *load_json(const char *filename) {
  yyjson_doc* json = yyjson_read_file(filename, 0, NULL, NULL);

  return json;
}

static void handle_request_adapter(
  WGPURequestAdapterStatus status,
  WGPUAdapter adapter, char const *message,
  void *UNUSED(userdata)
) {
  if (status == WGPURequestAdapterStatus_Success) {
    game.adapter = adapter;
  } else {
    WARN(LOG_PREFIX " request_adapter status=%#.8x message=%s", status, message);
  }
}
static void handle_request_device(
  WGPURequestDeviceStatus status,
  WGPUDevice device, char const *message,
  void *UNUSED(userdata)
) {
  if (status == WGPURequestDeviceStatus_Success) {
    game.device = device;
  } else {
    WARN(LOG_PREFIX " request_device status=%#.8x message=%s", status, message);
  }
}
static void handle_glfw_key(
  GLFWwindow *window, int key, int UNUSED(scancode),
  int action, int UNUSED(mods)
) {
  if (!game.instance) return;

  switch (action) {
    case GLFW_PRESS:
      game.keys[key] = true;
      switch (key) {
        case GLFW_KEY_ESCAPE:
          glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
          game.mouse_captured = false;
          break;
        case GLFW_KEY_R:
          WGPUGlobalReport report;
          wgpuGenerateReport(game.instance, &report);
          frmwrk_print_global_report(report);
          break;
      }
      break;
    case GLFW_RELEASE:
      game.keys[key] = false;
      break;
  }
}

void update_window_size(int width, int height) {
  if (game.depth_texture != NULL) {
    wgpuTextureRelease(game.depth_texture);
  }
  game.config.width = width;
  game.config.height = height;
  wgpuSurfaceConfigure(game.surface, &game.config);
  game.depth_texture_descriptor.size.width = game.config.width;
  game.depth_texture_descriptor.size.height = game.config.height;
  game.depth_texture = wgpuDeviceCreateTexture(game.device, &game.depth_texture_descriptor);
}

static void handle_glfw_framebuffer_size(GLFWwindow *UNUSED(window), int width, int height) {
  if (width == 0 && height == 0) {
    return;
  }

  update_window_size(width, height);
}

static void handle_glfw_cursor_pos(GLFWwindow *UNUSED(window), double xpos, double ypos) {
  vec2 current = {xpos, ypos};
  if (!game.mouse_captured) {
    glm_vec2_copy(current, game.last_mouse);
    return;
  };

  vec2 delta;
  glm_vec2_sub(current, game.last_mouse, delta);
  glm_vec2_copy(current, game.last_mouse);
  game.elevation -= delta[1] * TURN_SPEED;
  game.elevation = glm_clamp(game.elevation, -GLM_PI_2 + 0.1f, GLM_PI_2 - 0.1f);

  float delta_azimuth = -delta[0] * TURN_SPEED;
  glm_vec3_rotate(game.forward, delta_azimuth, game.up);
  glm_vec3_normalize(game.forward);
  glm_vec3_cross(game.forward, game.up, game.right);
  glm_vec3_normalize(game.right);
  glm_vec3_copy(game.forward, game.look);
  glm_vec3_rotate(game.look, game.elevation, game.right);
  glm_vec3_normalize(game.look);
}

mcapiBlockFace face_from_normal(vec3 normal) {
  if (normal[0] == 1)
    return MCAPI_FACE_EAST;
  else if (normal[0] == -1)
    return MCAPI_FACE_WEST;
  else if (normal[1] == 1)
    return MCAPI_FACE_TOP;
  else if (normal[1] == -1)
    return MCAPI_FACE_BOTTOM;
  else if (normal[2] == 1)
    return MCAPI_FACE_SOUTH;
  else if (normal[2] == -1)
    return MCAPI_FACE_NORTH;
  else {
    ERROR("Invalid normal!");
    return 0;
  }
}

static void handle_glfw_set_mouse_button(GLFWwindow *window, int button, int action, int UNUSED(mods)) {
  switch (action) {
    case GLFW_PRESS:
      if (!game.mouse_captured) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        game.mouse_captured = true;
        break;
      }
      float reach = 5.0f;
      vec3 target;
      vec3 normal;
      int material;
      vec3 eye = {game.position[0], game.position[1] + game.eye_height, game.position[2]};
      world_target_block(&game.world, eye, game.look, reach, target, normal, &material);
      if (material != 0) {
        switch (button) {
          case GLFW_MOUSE_BUTTON_LEFT:
            mcapiBlockFace face = face_from_normal(normal);
            game.block_breaking_start = game.current_time;
            game.block_breaking_face = face;
            game.block_breaking_position[0] = floor(target[0]);
            game.block_breaking_position[1] = floor(target[1]);
            game.block_breaking_position[2] = floor(target[2]);
            game.block_breaking_seq_num++;
            mcapi_send_player_action(game.conn, (mcapiPlayerActionPacket){
                                                  .face = game.block_breaking_face,
                                                  .position = {game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2]},
                                                  .status = MCAPI_ACTION_DIG_START,
                                                  .sequence_num = game.block_breaking_seq_num,
                                                });
            DEBUG("Sending break start face=%d, x=%d, y=%d, z=%d, seq=%d", game.block_breaking_face, game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2], game.block_breaking_seq_num);
            break;
          case GLFW_MOUSE_BUTTON_RIGHT:
            vec3 air_position;
            glm_vec3_add(target, normal, air_position);
            world_set_block(&game.world, air_position, 1, game.block_info, game.biome_info, game.device);
            break;
        }
      }
      break;
    case GLFW_RELEASE:
      switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
          if (game.block_breaking_start != 0) {
            mcapi_send_player_action(game.conn, (mcapiPlayerActionPacket){
                                                  .face = MCAPI_FACE_BOTTOM,  // Face is always set to -Y
                                                  .position = {game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2]},
                                                  .status = MCAPI_ACTION_DIG_CANCEL,
                                                  .sequence_num = game.block_breaking_seq_num,
                                                });
            DEBUG("Sending break cancel face=%d, x=%d, y=%d, z=%d, seq=%d", game.block_breaking_face, game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2], game.block_breaking_seq_num);
            game.block_breaking_start = 0;
          }
      }
  }
}

static void handle_glfw_set_scroll(GLFWwindow *UNUSED(window), double UNUSED(xoffset), double UNUSED(yoffset)) {
}

void game_update_player_y(float delta) {
  vec3 p;
  glm_vec3_copy(game.position, p);
  float new_y = p[1] + delta;
  vec3 sz = {game.size[0] / 2, game.size[1], game.size[2] / 2};
  World *w = &game.world;

  // Move +y
  if (delta > 0 && floor(new_y + sz[1]) > floor(p[1] + sz[1])) {
    for (int dx = -1; dx <= 1; dx += 2) {
      for (int dz = -1; dz <= 1; dz += 2) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], new_y + sz[1], p[2] + dz * sz[2]});
        if (!game.block_info[m].passable) {
          game.position[1] = floor(new_y + sz[1]) - sz[1] - COLLISION_EPSILON;
          game.velocity[1] = 0;
          game.fall_speed = 0;
          return;
        }
      }
    }
  }
  // Move -y
  if (delta < 0 && floor(new_y) < floor(p[1])) {
    for (int dx = -1; dx <= 1; dx += 2) {
      for (int dz = -1; dz <= 1; dz += 2) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], new_y, p[2] + dz * sz[2]});
        if (!game.block_info[m].passable) {
          game.position[1] = ceil(new_y) + COLLISION_EPSILON;
          game.velocity[1] = 0;
          game.on_ground = true;
          game.fall_speed = 0;
          return;
        }
      }
    }
  }
  if (new_y > game.position[1] && game.on_ground) {
    game.on_ground = false;
  }
  game.position[1] = new_y;
}

void game_update_player_x(float delta) {
  vec3 p;
  glm_vec3_copy(game.position, p);
  float new_x = p[0] + delta;
  vec3 sz = {game.size[0] / 2, game.size[1], game.size[2] / 2};
  World *w = &game.world;

  // Move +x
  if (delta > 0 && floor(new_x + sz[0]) > floor(p[0] + sz[0])) {
    for (int dz = -1; dz <= 1; dz += 2) {
      for (int dy = 0; dy <= 2; dy += 1) {
        int m = world_get_material(w, (vec3){new_x + sz[0], p[1] + 0.5f * dy * sz[1], p[2] + dz * sz[2]});
        if (!game.block_info[m].passable) {
          game.position[0] = floor(new_x + sz[0]) - sz[0] - COLLISION_EPSILON;
          game.velocity[0] = 0;
          return;
        }
      }
    }
  }
  // Move -x
  if (delta < 0 && floor(new_x - sz[0]) < floor(p[0] - sz[0])) {
    for (int dz = -1; dz <= 1; dz += 2) {
      for (int dy = 0; dy <= 2; dy += 1) {
        int m = world_get_material(w, (vec3){new_x - sz[0], p[1] + 0.5f * dy * sz[1], p[2] + dz * sz[2]});
        if (!game.block_info[m].passable) {
          game.position[0] = ceil(new_x - sz[0]) + sz[0] + COLLISION_EPSILON;
          game.velocity[0] = 0;
          return;
        }
      }
    }
  }
  game.position[0] = new_x;
}

void game_update_player_z(float delta) {
  vec3 p;
  glm_vec3_copy(game.position, p);
  float new_z = p[2] + delta;
  vec3 sz = {game.size[0] / 2, game.size[1], game.size[2] / 2};
  World *w = &game.world;

  // Move +z
  if (delta > 0 && floor(new_z + sz[2]) > floor(p[2] + sz[2])) {
    for (int dx = -1; dx <= 1; dx += 2) {
      for (int dy = 0; dy <= 2; dy += 1) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], p[1] + 0.5f * dy * sz[1], new_z + sz[2]});
        if (!game.block_info[m].passable) {
          game.position[2] = floor(new_z + sz[2]) - sz[2] - COLLISION_EPSILON;
          game.velocity[2] = 0;
          return;
        }
      }
    }
  }
  // Move -z
  if (delta < 0 && floor(new_z - sz[2]) < floor(p[2] - sz[2])) {
    for (int dx = -1; dx <= 1; dx += 2) {
      for (int dy = 0; dy <= 2; dy += 1) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], p[1] + 0.5f * dy * sz[1], new_z - sz[2]});
        if (!game.block_info[m].passable) {
          game.position[2] = ceil(new_z - sz[2]) + sz[2] + COLLISION_EPSILON;
          game.velocity[2] = 0;
          return;
        }
      }
    }
  }
  game.position[2] = new_z;
}

void update_player_position(float dt) {
  vec3 desired_velocity = {0};
  float speed = game.walking_speed;
  if (game.keys[GLFW_KEY_D]) {
    vec3 delta;
    glm_vec3_scale(game.right, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (game.keys[GLFW_KEY_A]) {
    vec3 delta;
    glm_vec3_scale(game.right, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  if (game.keys[GLFW_KEY_W]) {
    vec3 delta;
    glm_vec3_scale(game.forward, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (game.keys[GLFW_KEY_S]) {
    vec3 delta;
    glm_vec3_scale(game.forward, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  if (game.keys[GLFW_KEY_SPACE]) {
    // vec3 delta;
    // glm_vec3_scale(game.up, speed, delta);
    // glm_vec3_add(desired_velocity, delta, desired_velocity);
    if (game.on_ground) {
      game.fall_speed = 0.42f * TICKS_PER_SECOND;
      game.on_ground = false;
    }
  }
  // if (game.keys[GLFW_KEY_LEFT_SHIFT]) {
  //   vec3 delta;
  //   glm_vec3_scale(game.up, speed, delta);
  //   glm_vec3_sub(desired_velocity, delta, desired_velocity);
  // }
  desired_velocity[1] += game.fall_speed;
  glm_vec3_mix(game.velocity, desired_velocity, 0.8f, game.velocity);

  // Update position
  vec3 delta;
  glm_vec3_scale(game.velocity, dt, delta);

  glm_vec3_copy(game.position, game.last_position);

  game_update_player_y(delta[1]);
  if (fabs(delta[0]) > fabs(delta[2])) {
    game_update_player_x(delta[0]);
    game_update_player_z(delta[2]);
  } else {
    game_update_player_z(delta[2]);
    game_update_player_x(delta[0]);
  }

  game.fall_speed -= 0.08f * TICKS_PER_SECOND;
  if (game.fall_speed < 0.0f) {
    game.fall_speed *= 0.98f;
  }

  if (mcapi_get_state(game.conn) == MCAPI_STATE_PLAY) {
    float yaw = -atan2(game.look[0], game.look[2]) / GLM_PIf * 180.0f;
    if (yaw < 0.0f) {
      yaw += 360.0f;
    }
    float pitch = -game.elevation * 180.0f / GLM_PIf;
    mcapi_send_set_player_position_and_rotation(
      game.conn,
      (mcapiSetPlayerPositionAndRotationPacket){
        .x = game.position[0],
        .y = game.position[1],
        .z = game.position[2],
        .yaw = yaw,
        .pitch = pitch,
        .on_ground = true,
      }
    );
  }
}

void on_login_success(mcapiConnection *conn, mcapiLoginSuccessPacket *packet) {
  INFO("Finished login");
  INFO("  Username: %s", packet->username);
  INFO("  UUID: %016lx%016lx", packet->uuid.upper, packet->uuid.lower);
  INFO("  %d Properties:", packet->number_of_properties);
  for (int i = 0; i < packet->number_of_properties; i++) {
    INFO("    %s: %s", packet->properties[i].name, packet->properties[i].value);
  }

  mcapi_send_login_acknowledged(conn);

  mcapi_set_state(conn, MCAPI_STATE_CONFIG);
}

void on_known_packs(mcapiConnection *conn, mcapiClientboundKnownPacksPacket* UNUSED(packet)) {
  mcapi_send_serverbound_known_packs(conn, (mcapiServerboundKnownPacksPacket){});
}

void int_to_rgb(int color, vec3 result) {
  glm_vec3_copy(
    (vec3){
      (float)((color >> 16) & 0xff) / 255.0f,
      (float)((color >> 8) & 0xff) / 255.0f,
      (float)(color & 0xff) / 255.0f,
    },
    result
  );
}

void on_registry(mcapiConnection *UNUSED(conn), mcapiRegistryDataPacket *packet) {
  if (strcmp(packet->id, "minecraft:worldgen/biome") == 0) {
    unsigned int width;
    unsigned int height;
    unsigned char *grass = load_image("data/assets/minecraft/textures/colormap/grass.png", &width, &height);
    assert(width == 256);
    assert(height == 256);
    unsigned char *foliage = load_image("data/assets/minecraft/textures/colormap/foliage.png", &width, &height);
    assert(width == 256);
    assert(height == 256);
    unsigned char *dry_foliage = load_image("data/assets/minecraft/textures/colormap/dry_foliage.png", &width, &height);
    assert(width == 256);
    assert(height == 256);
    for (int i = 0; i < packet->entry_count; i++) {
      BiomeInfo info = {0};
      info.temperature = nbt_get_compound_tag(packet->entries[i]->root, "temperature")->float_value;
      info.downfall = nbt_get_compound_tag(packet->entries[i]->root, "downfall")->float_value;
      NBTValue *effects = nbt_get_compound_tag(packet->entries[i]->root, "effects");
      int_to_rgb(nbt_get_compound_tag(effects, "fog_color")->int_value, info.fog_color);
      int_to_rgb(nbt_get_compound_tag(effects, "water_color")->int_value, info.water_color);
      int_to_rgb(nbt_get_compound_tag(effects, "water_fog_color")->int_value, info.water_fog_color);
      int_to_rgb(nbt_get_compound_tag(effects, "sky_color")->int_value, info.sky_color);

      float clamped_temperature = glm_clamp(info.temperature, 0.0f, 1.0f);
      float clamped_downfall = glm_clamp(info.downfall, 0.0f, 1.0f);
      clamped_downfall *= clamped_temperature;
      int x_index = 255 - (int)(clamped_temperature * 255);
      int y_index = 255 - (int)(clamped_downfall * 255);
      int index = y_index * 256 + x_index;
      NBTValue *grass_color = nbt_get_compound_tag(effects, "grass_color");
      if (grass_color != NULL) {
        info.custom_grass_color = true;
        int_to_rgb(grass_color->int_value, info.grass_color);
      } else {
        info.grass_color[0] = grass[index * 4 + 0] / 255.0f;
        info.grass_color[1] = grass[index * 4 + 1] / 255.0f;
        info.grass_color[2] = grass[index * 4 + 2] / 255.0f;
      }
      // https://minecraft.fandom.com/wiki/Color#Grass
      NBTValue *grass_color_modifier = nbt_get_compound_tag(effects, "grass_color_modifier");
      if (grass_color_modifier != NULL) {
        char* modifier = grass_color_modifier->string_value;
        if (!strcmp(modifier, "swamp")) {
          info.swamp = true;
          // Swamp temperature, which starts at 0.8, is not affected by altitude.
          // Rather, a Perlin noise function is used to gradually vary the temperature of the swamp.
          // When this temperature goes below −0.1, a lush green color is used (#4C763C);
          // otherwise it is set to a sickly brown (#6A7039).
          info.grass_color[0] = 0x6A / 255.0f;
          info.grass_color[1] = 0x70 / 255.0f;
          info.grass_color[2] = 0x39 / 255.0f;
        }
      }

      NBTValue *foliage_color = nbt_get_compound_tag(effects, "foliage_color");
      if (foliage_color != NULL) {
        info.custom_foliage_color = true;
        int_to_rgb(foliage_color->int_value, info.foliage_color);
      } else {
        info.foliage_color[0] = foliage[index * 4 + 0] / 255.0f;
        info.foliage_color[1] = foliage[index * 4 + 1] / 255.0f;
        info.foliage_color[2] = foliage[index * 4 + 2] / 255.0f;
      }

      info.dry_foliage_color[0] = dry_foliage[index * 4 + 0] / 255.0f;
      info.dry_foliage_color[1] = dry_foliage[index * 4 + 1] / 255.0f;
      info.dry_foliage_color[2] = dry_foliage[index * 4 + 2] / 255.0f;

      game.biome_info[i] = info;
      // print_string(packet->entry_names[i]);
      // printf(" %d: %d, temp %f downfall %f x %d y %d color %f %f %f\n", i, info.custom_grass_color, info.temperature, info.downfall, x_index, y_index, info.grass_color[0], info.grass_color[1], info.grass_color[2]);
      // printf("grass %x %x %x\n", grass[index * 4 + 0], grass[index * 4 + 1], grass[index * 4 + 2]);
      // printf("foliage %x %x %x\n", foliage[index * 4 + 0], foliage[index * 4 + 1], foliage[index * 4 + 2]);
    }
    free(grass);
    free(foliage);
  }
}

void on_finish_config(mcapiConnection *conn, void* UNUSED(payload)) {
  mcapi_send_acknowledge_finish_config(conn);

  mcapi_set_state(conn, MCAPI_STATE_PLAY);

  INFO("Playing!");
}

void on_chunk(mcapiConnection *UNUSED(conn), mcapiChunkAndLightDataPacket* packet) {
  Chunk *chunk = world_chunk(&game.world, packet->chunk_x, packet->chunk_z);
  bool is_new = false;
  if (chunk == NULL) {
    chunk = calloc(1, sizeof(Chunk));
    is_new = true;
  } else {
    // Reset chunk mesh
    chunk_destroy_buffers(chunk);
  }

  chunk->x = packet->chunk_x;
  chunk->z = packet->chunk_z;
  for (int i = 0; i < 24; i++) {
    chunk->sections[i].num_quads = 0;
    chunk->sections[i].x = packet->chunk_x;
    chunk->sections[i].y = i - 4;
    chunk->sections[i].z = packet->chunk_z;
    memcpy(chunk->sections[i].data, packet->chunk_sections[i].blocks, 4096 * sizeof(int));
    memcpy(chunk->sections[i].biome_data, packet->chunk_sections[i].biomes, 64 * sizeof(int));
    memcpy(chunk->sections[i].sky_light, packet->sky_light_array[i + 1], 4096);
    memcpy(chunk->sections[i].block_light, packet->block_light_array[i + 1], 4096);
  }
  if (is_new) {
    world_add_chunk(&game.world, chunk);
  }
  world_init_new_meshes(&game.world, game.block_info, game.biome_info, game.device);
}

void on_unload_chunk(mcapiConnection *, mcapiUnloadChunk *p) {
  DEBUG("unload chunk cx: %d cz: %d", p->cx, p->cz);
}

void on_light(mcapiConnection *UNUSED(conn), mcapiUpdateLightPacket *packet) {
  DEBUG("Light updated!!! %d, %d", packet->chunk_x, packet->chunk_z);
  Chunk *chunk = world_chunk(&game.world, packet->chunk_x, packet->chunk_z);
  if (chunk == NULL) {
    WARN("Chunk not loaded!");
    return;
  }
  // perror("In!\n");
  for (int i = 0; i < 24; i++) {
    memcpy(chunk->sections[i].sky_light, packet->sky_light_array[i + 1], 4096);
    memcpy(chunk->sections[i].block_light, packet->block_light_array[i + 1], 4096);
  }
  world_init_new_meshes(&game.world, game.block_info, game.biome_info, game.device);
}

void on_block_update(mcapiConnection *UNUSED(conn), mcapiBlockUpdatePacket *packet) {
  vec3 pos;
  pos[0] = packet->position[0];
  pos[1] = packet->position[1];
  pos[2] = packet->position[2];

  DEBUG("Block update %d %d %d", packet->position[0], packet->position[1], packet->position[2]);

  world_set_block(&game.world, pos, packet->block_id, game.block_info, game.biome_info, game.device);
}

void on_position(mcapiConnection *conn, mcapiSynchronizePlayerPositionPacket *packet) {
  DEBUG("sync player position %f %f %f", packet->x, packet->y, packet->z);
  game.position[0] = packet->x;
  game.position[1] = packet->y;
  game.position[2] = packet->z;

  float pitch = packet->pitch * GLM_PIf / 180.0f;
  float yaw = packet->yaw * GLM_PIf / 180.0f;

  game.elevation = -pitch;
  game.look[0] = -cos(pitch) * sin(yaw);
  game.look[1] = -sin(pitch);
  game.look[2] = cos(pitch) * cos(yaw);
  glm_normalize(game.look);
  glm_vec3_cross(game.look, game.up, game.right);
  glm_normalize(game.right);
  glm_vec3_cross(game.up, game.right, game.forward);
  glm_normalize(game.forward);

  mcapi_send_confirm_teleportation(conn, (mcapiConfirmTeleportationPacket){.teleport_id = packet->teleport_id});
}

void on_update_time(mcapiConnection *UNUSED(conn), mcapiUpdateTimePacket *packet) {
  // printf("World age: %ld, Time of day: %ld\n", packet->world_age, packet->time_of_day % 24000);
  game.time_of_day = packet->time_of_day;
}

void on_set_block_destroy_stage(mcapiConnection *UNUSED(conn), mcapiSetBlockDestroyStagePacket *packet) {
  DEBUG("Destroy stage %d", packet->stage);
}

void on_chunk_batch_finished(mcapiConnection *conn, mcapiChunkBatchFinishedPacket *UNUSED(packet)) {
  TRACE("Chunk batch finished chunks=%d", packet->batch_size);
  mcapi_send_chunk_batch_received(conn, (mcapiChunkBatchReceivedPacket) { .chunks_per_tick = 0.1 });
}

void on_clientbound_keepalive(mcapiConnection *conn, mcapiClientboundKeepAlivePacket *packet) {
  DEBUG("Got keepalive id=%ld, sending response!", packet->keep_alive_id);
  mcapi_send_serverbound_keepalive(conn, (mcapiServerboundKeepalivePacket) {.id = packet->keep_alive_id});
}

int add_file_texture_to_image_sub_opacity(const char *fname, unsigned char *texture_sheet, int *cur_texture, int sub_opacity) {
  unsigned int width, height;
  unsigned char *rgba = load_image(fname, &width, &height);
  if (rgba == NULL) {
    // printf("%s not found for %s\n", name, texture_name);
    return 0;
    // } else {
    //   printf("%s found for %s, %d\n", name, texture_name, *cur_texture);
  }

  int texture_id = *cur_texture;
  if (texture_id >= TEXTURE_TILES * TEXTURE_TILES) {
    WARN("Too many textures, increase TEXTURE_TILES");
    assert(false);
  }
  *cur_texture += 1;
  int full_width = TEXTURE_SIZE * TEXTURE_TILES;
  int tile_start_x = (texture_id % TEXTURE_TILES) * TEXTURE_SIZE;
  int tile_start_y = (texture_id / TEXTURE_TILES) * TEXTURE_SIZE;
  width = glm_min(width, TEXTURE_SIZE);
  height = glm_min(height, TEXTURE_SIZE);
  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++) {
      int i = (y * width + x) * 4;
      int j = ((tile_start_y + y) * full_width + (tile_start_x + x)) * 4;
      // int j = (texture_id * TEXTURE_SIZE * TEXTURE_SIZE + y * TEXTURE_SIZE + x) * 4;
      texture_sheet[j + 0] = rgba[i + 0];
      texture_sheet[j + 1] = rgba[i + 1];
      texture_sheet[j + 2] = rgba[i + 2];
      texture_sheet[j + 3] = MAX(rgba[i + 3] - sub_opacity, 0);
    }
  }

  free(rgba);
  return texture_id;
}

uint16_t add_file_texture_to_image(const char *fname, unsigned char *texture_sheet, int *cur_texture) {
  return add_file_texture_to_image_sub_opacity(fname, texture_sheet, cur_texture, 0);
}

void on_add_entity(mcapiConnection*, mcapiAddEntityPacket *p) {
  DEBUG("add_entity id: %d type %d x %.2f y %.2f z %.2f pitch %d yaw %d yaw_head %d data %d vx %d vy %d vz %d",
    p->id, p->type, p->x, p->y, p->z, p->pitch, p->yaw, p->yaw_head, p->data, p->vx, p->vy, p->vz);
}

void on_update_entity_pos(mcapiConnection*, mcapiUpdateEntityPositionPacket *p) {
  // int id;
  // short dx;
  // short dy;
  // short dz;
  // bool on_ground;

  DEBUG("update_entity_pos id: %d dx: %d dy: %d dz: %d on_ground: %d",
    p->id, p->dx, p->dy, p->dz, p->on_ground);
}

void on_update_entity_pos_rot(mcapiConnection*, mcapiUpdateEntityPositionRotationPacket *p) {
  // int id;
  // short dx;
  // short dy;
  // short dz;
  // uint8_t pitch;
  // uint8_t yaw;
  // bool on_ground;

  DEBUG("update_entity_pos_rot id: %d dx: %d dy: %d dz: %d pitch: %d, yaw: %d on_ground: %d",
    p->id, p->dx, p->dy, p->dz, p->pitch, p->yaw, p->on_ground);
}

void on_teleport_entity(mcapiConnection*, mcapiTeleportEntityPacket *p) {
  // int id;
  // double x;
  // double y;
  // double z;
  // double vx;
  // double vy;
  // double vz;
  // float yaw;
  // float pitch;
  // bool on_ground;

  DEBUG("teleport_entity id: %d x: %.2f y: %.2f z: %.2f vx: %.2f vy: %.2f vz: %.2f yaw: %.2f pitch: %.2f on_ground: %d",
    p->id, p->x, p->y, p->z, p->vx, p->vy, p->vz, p->yaw, p->pitch, p->on_ground);
}

void on_remove_entities(mcapiConnection *, mcapiRemoveEntitiesPacket *p) {
  DEBUG("remove_entities count: %d", p->entity_count);
  for (int i = 0; i < p->entity_count; i++) {
    DEBUG("removed entities[%d]: %d", i, p->entity_ids[i]);
  }
}

void init_mcapi(char *server_ip, int port, char *uuid, char *access_token, char *username) {
  mcapiConnection *conn = mcapi_create_connection(server_ip, port, uuid, access_token);
  game.conn = conn;

  mcapi_send_handshake(
    conn,
    (mcapiHandshakePacket){
      .protocol_version = 770,
      // .protocol_version = 767,
      // .protocol_version = 769,
      .server_addr = server_ip,
      .server_port = port,
      .next_state = 2,
    }
  );

  mcapi_send_login_start(
    conn,
    (mcapiLoginStartPacket){
      .username = username,
      .uuid = (UUID){
        .upper = 0,
        .lower = 0,
      },
    }
  );

  mcapi_set_state(conn, MCAPI_STATE_LOGIN);

  mcapi_set_login_success_cb(conn, on_login_success);
  mcapi_set_clientbound_known_packs_cb(conn, on_known_packs);
  mcapi_set_finish_config_cb(conn, on_finish_config);
  mcapi_set_chunk_and_light_data_cb(conn, on_chunk);
  mcapi_set_unload_chunk_cb(conn, on_unload_chunk);
  mcapi_set_update_light_cb(conn, on_light);
  mcapi_set_block_update_cb(conn, on_block_update);
  mcapi_set_synchronize_player_position_cb(conn, on_position);
  mcapi_set_registry_data_cb(conn, on_registry);
  mcapi_set_update_time_cb(conn, on_update_time);
  mcapi_set_set_block_destroy_stage_cb(conn, on_set_block_destroy_stage);
  mcapi_set_chunk_batch_finished_cb(conn, on_chunk_batch_finished);
  mcapi_set_clientbound_keepalive_cb(conn, on_clientbound_keepalive);
  mcapi_set_add_entity_cb(conn, on_add_entity);
  mcapi_set_update_entity_position_cb(conn, on_update_entity_pos);
  mcapi_set_update_entity_position_rotation_cb(conn, on_update_entity_pos_rot);
  mcapi_set_teleport_entity_cb(conn, on_teleport_entity);
  mcapi_set_remove_entities_cb(conn, on_remove_entities);
}

void chunk_renderer_init() {
  game.shader_module =
    frmwrk_load_shader_module(game.device, "shader.wgsl");
  assert(game.shader_module);

  // unsigned int texture_width;
  // unsigned int texture_height;
  // unsigned char *image_data = load_image("texture-2.png", &texture_width, &texture_height);

  WGPUTextureDescriptor texture_descriptor = {
    .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
    .dimension = WGPUTextureDimension_2D,
    .size = {
      .width = TEXTURE_SIZE * TEXTURE_TILES,
      .height = TEXTURE_SIZE * TEXTURE_TILES,
      .depthOrArrayLayers = 1,
    },
    .format = WGPUTextureFormat_RGBA8UnormSrgb,
    .mipLevelCount = 1,
    .sampleCount = 1,
    .viewFormatCount = 0,
    .viewFormats = NULL,
  };
  WGPUTexture texture = wgpuDeviceCreateTexture(game.device, &texture_descriptor);

  // Create the texture view.
  WGPUTextureViewDescriptor textureViewDescriptor = {
    .format = WGPUTextureFormat_RGBA8UnormSrgb,
    .dimension = WGPUTextureViewDimension_2D,
    .aspect = WGPUTextureAspect_All,
    .mipLevelCount = 1,
    .arrayLayerCount = 1,
  };
  WGPUTextureView texture_view = wgpuTextureCreateView(texture, &textureViewDescriptor);

  WGPUSampler texture_sampler = wgpuDeviceCreateSampler(
    game.device,
    &(WGPUSamplerDescriptor){
      .addressModeU = WGPUAddressMode_Repeat,
      .addressModeV = WGPUAddressMode_Repeat,
      .addressModeW = WGPUAddressMode_Repeat,
      .magFilter = WGPUFilterMode_Nearest,
      .minFilter = WGPUFilterMode_Nearest,
      .mipmapFilter = WGPUMipmapFilterMode_Nearest,
      .maxAnisotropy = 1,
    }
  );

  // WGPUBuffer buffer = frmwrk_device_create_buffer_init(
  //   game.device,
  //   &(const frmwrk_buffer_init_descriptor){
  //     .label = "Texture Buffer",
  //     .content = (void *)game.texture_sheet,
  //     .content_size = TEXTURE_SIZE * TEXTURE_SIZE * TEXTURE_TILES * TEXTURE_TILES * 4,
  //     .usage = WGPUBufferUsage_CopySrc,
  //   }
  // );

  // WGPUImageCopyBuffer image_copy_buffer = {
  //   .buffer = buffer,
  //   .layout = {
  //     .bytesPerRow = TEXTURE_SIZE * TEXTURE_TILES * 4,
  //     .rowsPerImage = TEXTURE_SIZE * TEXTURE_TILES,
  //   },
  // };
  WGPUImageCopyTexture image_copy_texture = {
    .texture = texture,
  };
  WGPUExtent3D copy_size = {
    .width = TEXTURE_SIZE * TEXTURE_TILES,
    .height = TEXTURE_SIZE * TEXTURE_TILES,
    .depthOrArrayLayers = 1,
  };

  WGPUTextureDataLayout texture_data_layout = {
    .offset = 0,
    .bytesPerRow = TEXTURE_SIZE * TEXTURE_TILES * 4,
    .rowsPerImage = TEXTURE_SIZE * TEXTURE_TILES,
  };

  size_t dataSize = TEXTURE_SIZE * TEXTURE_TILES * TEXTURE_SIZE * TEXTURE_TILES * 4;

  wgpuQueueWriteTexture(
    game.queue,
    &image_copy_texture,
    game.texture_sheet,
    dataSize,
    &texture_data_layout,
    &copy_size
  );

  glm_mat4_identity(game.uniforms.view);
  glm_mat4_identity(game.uniforms.projection);

  game.uniform_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Uniform Buffer",
      .content = (void *)&game.uniforms,
      .content_size = sizeof(game.uniforms),
      .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
    }
  );

  WGPUBindGroupLayoutEntry bgl_entries[] = {
    [0] = {
      .binding = 0,
      .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
      .buffer = {
        .type = WGPUBufferBindingType_Uniform,
      },
    },
    [1] = {
      .binding = 1,
      .visibility = WGPUShaderStage_Fragment,
      .texture = {
        .sampleType = WGPUTextureSampleType_Float,
        .viewDimension = WGPUTextureViewDimension_2D,
        .multisampled = false,
      },
    },
    [2] = {
      .binding = 2,
      .visibility = WGPUShaderStage_Fragment,
      .sampler = {
        .type = WGPUSamplerBindingType_Filtering,
      },
    },
  };

  WGPUBindGroupLayoutDescriptor bgl_desc = {
    .entryCount = sizeof(bgl_entries) / sizeof(bgl_entries[0]),
    .entries = bgl_entries,
  };

  WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(game.device, &bgl_desc);

  WGPUBindGroupEntry bg_entries[] = {
    [0] = {
      .binding = 0,
      .buffer = game.uniform_buffer,
      .size = sizeof(game.uniforms),
    },
    [1] = {
      .binding = 1,
      .textureView = texture_view,
    },
    [2] = {
      .binding = 2,
      .sampler = texture_sampler,
    },
  };
  WGPUBindGroupDescriptor bg_desc = {
    .layout = bgl,
    .entryCount = sizeof(bg_entries) / sizeof(bg_entries[0]),
    .entries = bg_entries,
  };
  game.bind_group = wgpuDeviceCreateBindGroup(game.device, &bg_desc);

  static const WGPUVertexAttribute vertex_attributes[] = {
    {
      .format = WGPUVertexFormat_Float32x3,
      .offset = 0,
      .shaderLocation = 0,
    },
    {
      .format = WGPUVertexFormat_Float32x4,
      .offset = 12,
      .shaderLocation = 1,
    },
    {
      .format = WGPUVertexFormat_Float32x2,
      .offset = 12 + 16,
      .shaderLocation = 2,
    },
    {
      .format = WGPUVertexFormat_Float32,
      .offset = 12 + 16 + 8,
      .shaderLocation = 3,
    },
    {
      .format = WGPUVertexFormat_Float32,
      .offset = 12 + 16 + 8 + 4,
      .shaderLocation = 4,
    },
    {
      .format = WGPUVertexFormat_Float32,
      .offset = 12 + 16 + 8 + 4 + 4,
      .shaderLocation = 5,
    },
    {
      .format = WGPUVertexFormat_Float32,
      .offset = 12 + 16 + 8 + 4 + 4 + 4,
      .shaderLocation = 6,
    },
    {
      .format = WGPUVertexFormat_Float32,
      .offset = 12 + 16 + 8 + 4 + 4 + 4 + 4,
      .shaderLocation = 7,
    },
  };

  int max_quads = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * 3;
  uint32_t indices[max_quads * 6];
  for (int i = 0; i < max_quads; i += 1) {
    int offset = i * 6;
    int base = i * 4;
    indices[offset + 0] = base + 0;
    indices[offset + 1] = base + 1;
    indices[offset + 2] = base + 2;
    indices[offset + 3] = base + 2;
    indices[offset + 4] = base + 3;
    indices[offset + 5] = base + 0;
  }

  game.index_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "index_buffer",
      .content = (void *)indices,
      .content_size = sizeof(indices),
      .usage = WGPUBufferUsage_Index,
    }
  );

  game.pipeline_layout = wgpuDeviceCreatePipelineLayout(
    game.device,
    &(const WGPUPipelineLayoutDescriptor){
      .label = "game.pipeline_layout",
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = (const WGPUBindGroupLayout[]){
        bgl,
      },
    }
  );
  assert(game.pipeline_layout);

  game.render_pipeline = wgpuDeviceCreateRenderPipeline(
    game.device,
    &(const WGPURenderPipelineDescriptor){
      .label = "game.render_pipeline",
      .layout = game.pipeline_layout,
      .vertex = (const WGPUVertexState){
        .module = game.shader_module,
        .entryPoint = "vs_main",
        .bufferCount = 1,
        .buffers = (const WGPUVertexBufferLayout[]){
          (const WGPUVertexBufferLayout){
            .arrayStride = FLOATS_PER_VERTEX * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = sizeof(vertex_attributes) /
                              sizeof(vertex_attributes[0]),
            .attributes = vertex_attributes,
          },
        },
      },
      .fragment = &(const WGPUFragmentState){
        .module = game.shader_module,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = (const WGPUColorTargetState[]){
          (const WGPUColorTargetState){
            .format = game.surface_capabilities.formats[0],
            .writeMask = WGPUColorWriteMask_All,
          },
        },
      },
      .primitive = (const WGPUPrimitiveState){
        .topology = WGPUPrimitiveTopology_TriangleList,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_Back,
      },
      .multisample = (const WGPUMultisampleState){
        .count = 1,
        .mask = 0xFFFFFFFF,
      },
      .depthStencil = &(WGPUDepthStencilState){
        .depthWriteEnabled = true,
        .depthCompare = WGPUCompareFunction_LessEqual,
        .format = WGPUTextureFormat_Depth24Plus,
        .stencilBack = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
        .stencilFront = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
      },
    }
  );

  game.render_pipeline_transparent = wgpuDeviceCreateRenderPipeline(
    game.device,
    &(const WGPURenderPipelineDescriptor){
      .label = "game.render_pipeline",
      .layout = game.pipeline_layout,
      .vertex = (const WGPUVertexState){
        .module = game.shader_module,
        .entryPoint = "vs_main",
        .bufferCount = 1,
        .buffers = (const WGPUVertexBufferLayout[]){
          (const WGPUVertexBufferLayout){
            .arrayStride = FLOATS_PER_VERTEX * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = sizeof(vertex_attributes) /
                              sizeof(vertex_attributes[0]),
            .attributes = vertex_attributes,
          },
        },
      },
      .fragment = &(const WGPUFragmentState){
        .module = game.shader_module,
        .entryPoint = "fs_destroy_main",
        .targetCount = 1,
        .targets = (const WGPUColorTargetState[]){
          (const WGPUColorTargetState){
            .format = game.surface_capabilities.formats[0],
            .writeMask = WGPUColorWriteMask_All,
            .blend = (WGPUBlendState[]){
              (WGPUBlendState){
                .color = (WGPUBlendComponent){
                  .srcFactor = WGPUBlendFactor_Dst,
                  .operation = WGPUBlendOperation_Add,
                  .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                },
                .alpha = (WGPUBlendComponent){
                  .srcFactor = WGPUBlendFactor_Zero,
                  .operation = WGPUBlendOperation_Add,
                  .dstFactor = WGPUBlendFactor_One,
                },
              },
            },
          },
        },
      },
      .primitive = (const WGPUPrimitiveState){
        .topology = WGPUPrimitiveTopology_TriangleList,
      },
      .multisample = (const WGPUMultisampleState){
        .count = 1,
        .mask = 0xFFFFFFFF,
      },
      .depthStencil = &(WGPUDepthStencilState){
        .depthWriteEnabled = true,
        .depthCompare = WGPUCompareFunction_LessEqual,
        .format = WGPUTextureFormat_Depth24Plus,
        .stencilBack = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
        .stencilFront = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
      },
    }
  );
  assert(game.render_pipeline);

  game.config = (const WGPUSurfaceConfiguration){
    .device = game.device,
    .usage = WGPUTextureUsage_RenderAttachment,
    .format = game.surface_capabilities.formats[0],
    .presentMode = WGPUPresentMode_Fifo,
    .alphaMode = game.surface_capabilities.alphaModes[0],
  };

  game.depth_texture_descriptor = (WGPUTextureDescriptor){
    .usage = WGPUTextureUsage_RenderAttachment,
    .dimension = WGPUTextureDimension_2D,
    .size = {
      .depthOrArrayLayers = 1,
    },
    .format = WGPUTextureFormat_Depth24Plus,
    .mipLevelCount = 1,
    .sampleCount = 1,
  };
}

void chunk_renderer_render(WGPURenderPassEncoder render_pass_encoder) {
  // Interpolate between last and current position for smooth movement
  vec3 position;
  glm_vec3_lerp(game.last_position, game.position, (game.current_time - game.last_tick_time) * TICKS_PER_SECOND, position);
  vec3 eye = {position[0], position[1] + game.eye_height, position[2]};

  vec3 center;
  glm_vec3_add(eye, game.look, center);

  mat4 view;
  glm_lookat(eye, center, game.up, view);
  memcpy(&game.uniforms.view, view, sizeof(view));

  mat4 projection;
  glm_perspective(
    GLM_PI_2, (float)game.config.width / (float)game.config.height, 0.01f, 100.0f, projection
  );
  memcpy(&game.uniforms.projection, projection, sizeof(projection));

  // Day: 23,961 - 23,999, 0 - 12,039 is at 15
  // Dusk: 12,040 - 13,670 goes from light 15 to 4
  // Night: 13,671 - 22,329 is at 4
  // Dawn: 22,330 - 23,960 goes from light 4 to 15
  float sky_max = 15.0f;
  int time = game.time_of_day % 24000;
  if (time > 12040 && time < 13670) {
    sky_max = 4.0f + (15.0f - 4.0f) * (13670.0f - time) / (13670.0f - 12040.0f);
  } else if (time > 13670 && time < 22330) {
    sky_max = 4.0f;
  } else if (time > 22330 && time < 23960) {
    sky_max = 4.0f + (15.0f - 4.0f) * (time - 22330.0f) / (23960.0f - 22330.0f);
  }

  game.uniforms.internal_sky_max = sky_max / 15.0f;
  // float sky_color_scale = pow(glm_clamp((sky_max - 4.0) / 11.0, 0.0, 1.0), 3.0);

  wgpuQueueWriteBuffer(game.queue, game.uniform_buffer, 0, &game.uniforms, sizeof(game.uniforms));

  wgpuRenderPassEncoderSetIndexBuffer(render_pass_encoder, game.index_buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, game.bind_group, 0, NULL);
  wgpuRenderPassEncoderSetPipeline(render_pass_encoder, game.render_pipeline);

  for (int ci = 0; ci < MAX_CHUNKS; ci += 1) {
    Chunk *chunk = game.world.chunks[ci];
    if (chunk == NULL) {
      continue;
    }
    for (int s = 0; s < 24; s += 1) {
      if (chunk->sections[s].num_quads == 0) {
        continue;
      }
      wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, 0, chunk->sections[s].vertex_buffer, 0, WGPU_WHOLE_SIZE);
      wgpuRenderPassEncoderDrawIndexed(render_pass_encoder, chunk->sections[s].num_quads * 6, 1, 0, 0, 0);
    }
  }

  // Draw blocks breaking
  wgpuRenderPassEncoderSetPipeline(render_pass_encoder, game.render_pipeline_transparent);
  wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, 0, game.block_overlay_vertex_buffer, 0, WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderDrawIndexed(render_pass_encoder, game.number_of_blocks_being_broken * 6 * 6, 1, 0, 0, 0);
}

void sky_renderer_init() {
  game.sky_renderer.shader_module =
    frmwrk_load_shader_module(game.device, "sky.wgsl");
  assert(game.sky_renderer.shader_module);

  game.sky_renderer.uniform_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Uniform Buffer",
      .content = (void *)&game.sky_renderer.uniforms,
      .content_size = sizeof(game.sky_renderer.uniforms),
      .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
    }
  );

  float quad[] = {
    -1.0f,
    -1.0f,
    1.0f,
    -1.0f,
    1.0f,
    1.0f,
    -1.0f,
    -1.0f,
    1.0f,
    1.0f,
    -1.0f,
    1.0f,
  };

  game.sky_renderer.vertex_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Sky Vertex Buffer",
      .content = (void *)quad,
      .content_size = 6 * 2 * sizeof(float),
      .usage = WGPUBufferUsage_Vertex,
    }
  );

  WGPUBindGroupLayoutEntry bgl_entries[] = {
    [0] = {
      .binding = 0,
      .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
      .buffer = {
        .type = WGPUBufferBindingType_Uniform,
      },
    },
  };

  WGPUBindGroupLayoutDescriptor bgl_desc = {
    .entryCount = sizeof(bgl_entries) / sizeof(bgl_entries[0]),
    .entries = bgl_entries,
  };

  WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(game.device, &bgl_desc);

  WGPUBindGroupEntry bg_entries[] = {
    [0] = {
      .binding = 0,
      .buffer = game.sky_renderer.uniform_buffer,
      .size = sizeof(game.sky_renderer.uniforms),
    },
  };
  WGPUBindGroupDescriptor bg_desc = {
    .layout = bgl,
    .entryCount = sizeof(bg_entries) / sizeof(bg_entries[0]),
    .entries = bg_entries,
  };
  game.sky_renderer.bind_group = wgpuDeviceCreateBindGroup(game.device, &bg_desc);

  static const WGPUVertexAttribute vertex_attributes[] = {
    [0] = {
      .format = WGPUVertexFormat_Float32x2,
      .offset = 0,
      .shaderLocation = 0,
    },
  };

  game.sky_renderer.pipeline_layout = wgpuDeviceCreatePipelineLayout(
    game.device,
    &(const WGPUPipelineLayoutDescriptor){
      .label = "game.pipeline_layout",
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = (const WGPUBindGroupLayout[]){
        bgl,
      },
    }
  );
  assert(game.sky_renderer.pipeline_layout);

  game.sky_renderer.render_pipeline = wgpuDeviceCreateRenderPipeline(
    game.device,
    &(const WGPURenderPipelineDescriptor){
      .label = "Sky Render Pipeline",
      .layout = game.sky_renderer.pipeline_layout,
      .vertex = (const WGPUVertexState){
        .module = game.sky_renderer.shader_module,
        .entryPoint = "vs_main",
        .bufferCount = 1,
        .buffers = (const WGPUVertexBufferLayout[]){
          (const WGPUVertexBufferLayout){
            .arrayStride = 2 * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = sizeof(vertex_attributes) /
                              sizeof(vertex_attributes[0]),
            .attributes = vertex_attributes,
          },
        },
      },
      .fragment = &(const WGPUFragmentState){
        .module = game.sky_renderer.shader_module,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = (const WGPUColorTargetState[]){
          (const WGPUColorTargetState){
            .format = game.surface_capabilities.formats[0],
            .writeMask = WGPUColorWriteMask_All,
          },
        },
      },
      .primitive = (const WGPUPrimitiveState){
        .topology = WGPUPrimitiveTopology_TriangleList,
      },
      .multisample = (const WGPUMultisampleState){
        .count = 1,
        .mask = 0xFFFFFFFF,
      },
      .depthStencil = &(WGPUDepthStencilState){
        .depthWriteEnabled = true,
        .depthCompare = WGPUCompareFunction_LessEqual,
        .format = WGPUTextureFormat_Depth24Plus,
        .stencilBack = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
        .stencilFront = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
      },
    }
  );
  assert(game.sky_renderer.render_pipeline);
}

void sky_renderer_render(WGPURenderPassEncoder render_pass_encoder) {
  glm_vec3_copy(game.look, game.sky_renderer.uniforms.look);
  game.sky_renderer.uniforms.aspect = (float)game.config.width / (float)game.config.height;
  world_get_sky_color(&game.world, game.position, game.biome_info, game.sky_renderer.uniforms.sky_color);
  game.sky_renderer.uniforms.time_of_day = game.time_of_day % 24000;
  wgpuQueueWriteBuffer(game.queue, game.sky_renderer.uniform_buffer, 0, &game.sky_renderer.uniforms, sizeof(game.sky_renderer.uniforms));

  wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, game.sky_renderer.bind_group, 0, NULL);
  wgpuRenderPassEncoderSetPipeline(render_pass_encoder, game.sky_renderer.render_pipeline);
  wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, 0, game.sky_renderer.vertex_buffer, 0, WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderDraw(render_pass_encoder, 6, 1, 0, 0);
}

void block_selected_renderer_init() {
  game.block_selected_renderer.shader_module =
    frmwrk_load_shader_module(game.device, "block_outline.wgsl");
  assert(game.block_selected_renderer.shader_module);

  // game.sky_renderer.uniform_buffer = frmwrk_device_create_buffer_init(
  //   game.device,
  //   &(const frmwrk_buffer_init_descriptor){
  //     .label = "Uniform Buffer",
  //     .content = (void *)&game.sky_renderer.uniforms,
  //     .content_size = sizeof(game.sky_renderer.uniforms),
  //     .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
  //   }
  // );

  game.block_selected_renderer.uniform_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Selected Uniform Buffer",
      .content = (void *)&game.block_selected_renderer.uniforms,
      .content_size = sizeof(game.block_selected_renderer.uniforms),
      .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
    }
  );
  DEBUG("selected uniform buffer %p", (void*)game.block_selected_renderer.uniform_buffer);

  float vertices[18 * 4 * 6];

  int i = 0;
  float s = 0.005;

  for (int d = 0; d <= 2; d++) {
    for (int p = 0; p <= 1; p++) {
      for (int p1 = 0; p1 <= 1; p1++) {
        for (int p2 = 0; p2 <= 1; p2++) {
          int d1 = p1 == 1 ? (d + 1) % 3 : (d + 2) % 3;
          int d2 = p1 == 0 ? (d + 1) % 3 : (d + 2) % 3;
          vertices[i + d] = p;
          vertices[i + d1] = p1;
          vertices[i + d2] = p2;
          i += 3;
          vertices[i + d] = p;
          vertices[i + d1] = p1;
          vertices[i + d2] = p2 == 1 ? p2 - s : s;
          i += 3;
          vertices[i + d] = p;
          vertices[i + d1] = 1 - p1;
          vertices[i + d2] = p2 == 1 ? p2 - s : s;
          i += 3;
          vertices[i + d] = p;
          vertices[i + d1] = p1;
          vertices[i + d2] = p2;
          i += 3;
          vertices[i + d] = p;
          vertices[i + d1] = 1 - p1;
          vertices[i + d2] = p2 == 1 ? p2 - s : s;
          i += 3;
          vertices[i + d] = p;
          vertices[i + d1] = 1 - p1;
          vertices[i + d2] = p2;
          i += 3;
        }
      }
    }
  }

  game.block_selected_renderer.vertex_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Selected Vertex Buffer",
      .content = (void *)vertices,
      .content_size = sizeof(vertices),
      .usage = WGPUBufferUsage_Vertex,
    }
  );

  WGPUBindGroupLayoutEntry bgl_entries[] = {
    [0] = {
      .binding = 0,
      .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
      .buffer = {
        .type = WGPUBufferBindingType_Uniform,
      },
    },
  };

  WGPUBindGroupLayoutDescriptor bgl_desc = {
    .entryCount = sizeof(bgl_entries) / sizeof(bgl_entries[0]),
    .entries = bgl_entries,
  };

  WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(game.device, &bgl_desc);

  WGPUBindGroupEntry bg_entries[] = {
    [0] = {
      .binding = 0,
      .buffer = game.block_selected_renderer.uniform_buffer,
      .size = sizeof(game.block_selected_renderer.uniforms),
    },
  };
  WGPUBindGroupDescriptor bg_desc = {
    .layout = bgl,
    .entryCount = sizeof(bg_entries) / sizeof(bg_entries[0]),
    .entries = bg_entries,
  };
  game.block_selected_renderer.bind_group = wgpuDeviceCreateBindGroup(game.device, &bg_desc);

  static const WGPUVertexAttribute vertex_attributes[] = {
    [0] = {
      .format = WGPUVertexFormat_Float32x3,
      .offset = 0,
      .shaderLocation = 0,
    },
  };

  game.block_selected_renderer.pipeline_layout = wgpuDeviceCreatePipelineLayout(
    game.device,
    &(const WGPUPipelineLayoutDescriptor){
      .label = "block_selected_renderer.pipeline_layout",
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = (const WGPUBindGroupLayout[]){
        bgl,
      },
    }
  );
  assert(game.block_selected_renderer.pipeline_layout);

  game.block_selected_renderer.render_pipeline = wgpuDeviceCreateRenderPipeline(
    game.device,
    &(const WGPURenderPipelineDescriptor){
      .label = "Block Selected Render Pipeline",
      .layout = game.block_selected_renderer.pipeline_layout,
      .vertex = (const WGPUVertexState){
        .module = game.block_selected_renderer.shader_module,
        .entryPoint = "vs_main",
        .bufferCount = 1,
        .buffers = (const WGPUVertexBufferLayout[]){
          (const WGPUVertexBufferLayout){
            .arrayStride = 3 * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = sizeof(vertex_attributes) /
                              sizeof(vertex_attributes[0]),
            .attributes = vertex_attributes,
          },
        },
      },
      .fragment = &(const WGPUFragmentState){
        .module = game.block_selected_renderer.shader_module,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = (const WGPUColorTargetState[]){
          (const WGPUColorTargetState){
            .format = game.surface_capabilities.formats[0],
            .writeMask = WGPUColorWriteMask_All,
          },
        },
      },
      .primitive = (const WGPUPrimitiveState){
        .topology = WGPUPrimitiveTopology_TriangleList,
      },
      .multisample = (const WGPUMultisampleState){
        .count = 1,
        .mask = 0xFFFFFFFF,
      },
      .depthStencil = &(WGPUDepthStencilState){
        .depthWriteEnabled = true,
        .depthCompare = WGPUCompareFunction_LessEqual,
        .format = WGPUTextureFormat_Depth24Plus,
        .stencilBack = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
        .stencilFront = (WGPUStencilFaceState){
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Replace,
          .depthFailOp = WGPUStencilOperation_Replace,
          .passOp = WGPUStencilOperation_Replace,
        },
      },
    }
  );
  assert(game.block_selected_renderer.render_pipeline);
}

void block_selected_renderer_render(WGPURenderPassEncoder render_pass_encoder) {
  // Interpolate between last and current position for smooth movement
  vec3 position;
  glm_vec3_lerp(game.last_position, game.position, (game.current_time - game.last_tick_time) * TICKS_PER_SECOND, position);
  vec3 eye = {position[0], position[1] + game.eye_height, position[2]};

  vec3 center;
  glm_vec3_add(eye, game.look, center);

  mat4 view;
  glm_lookat(eye, center, game.up, view);
  memcpy(&game.block_selected_renderer.uniforms.view, view, sizeof(view));

  mat4 projection;
  glm_perspective(
    GLM_PI_2, (float)game.config.width / (float)game.config.height, 0.01f, 100.0f, projection
  );
  memcpy(&game.block_selected_renderer.uniforms.projection, projection, sizeof(projection));

  float reach = 5.0f;
  vec3 normal;
  int material;
  world_target_block(&game.world, eye, game.look, reach, game.block_selected_renderer.uniforms.position, normal, &material);
  game.target_material = material;
  if (material == 0) {
    return;
  }
  game.block_selected_renderer.uniforms.position[0] = floor(game.block_selected_renderer.uniforms.position[0]);
  game.block_selected_renderer.uniforms.position[1] = floor(game.block_selected_renderer.uniforms.position[1]);
  game.block_selected_renderer.uniforms.position[2] = floor(game.block_selected_renderer.uniforms.position[2]);

  int tx = floor(game.block_selected_renderer.uniforms.position[0]);
  int ty = floor(game.block_selected_renderer.uniforms.position[1]);
  int tz = floor(game.block_selected_renderer.uniforms.position[2]);
  // DEBUG("Target block material %d location %d %d %d", material, tx, ty, tz)
  // printf("name: ");
  // print_string(game.block_info[material].name);
  // printf(" type: ");
  // print_string(game.block_info[material].type);
  // printf("\n");

  if (game.block_breaking_start != 0 && (tx != game.block_breaking_position[0] || ty != game.block_breaking_position[1] || tz != game.block_breaking_position[2])) {
    mcapi_send_player_action(game.conn, (mcapiPlayerActionPacket){
                                          .face = game.block_breaking_face,
                                          .position = {game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2]},
                                          .status = MCAPI_ACTION_DIG_CANCEL,
                                          .sequence_num = game.block_breaking_seq_num,
                                        });

    game.block_breaking_position[0] = tx;
    game.block_breaking_position[1] = ty;
    game.block_breaking_position[2] = tz;
    game.block_breaking_start = game.current_time;
    game.block_breaking_face = face_from_normal(normal);
    game.block_breaking_seq_num++;

    mcapi_send_player_action(game.conn, (mcapiPlayerActionPacket){
                                          .face = game.block_breaking_face,
                                          .position = {game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2]},
                                          .status = MCAPI_ACTION_DIG_START,
                                          .sequence_num = game.block_breaking_seq_num,
                                        });
    DEBUG("Sending break start face=%d, x=%d, y=%d, z=%d, seq=%d", game.block_breaking_face, game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2], game.block_breaking_seq_num);
  }

  wgpuQueueWriteBuffer(game.queue, game.block_selected_renderer.uniform_buffer, 0, &game.block_selected_renderer.uniforms, sizeof(game.block_selected_renderer.uniforms));

  wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, game.block_selected_renderer.bind_group, 0, NULL);
  wgpuRenderPassEncoderSetPipeline(render_pass_encoder, game.block_selected_renderer.render_pipeline);
  wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, 0, game.block_selected_renderer.vertex_buffer, 0, WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderDraw(render_pass_encoder, 18 * 4 * 2, 1, 0, 0);
}

void read_json_arr_as_vec4(vec4 dst, yyjson_val * src) {
  if (src) {
    dst[0] = yyjson_get_num(yyjson_arr_get(src, 0));
    dst[1] = yyjson_get_num(yyjson_arr_get(src, 1));
    dst[2] = yyjson_get_num(yyjson_arr_get(src, 2));
    dst[3] = yyjson_get_num(yyjson_arr_get(src, 3));
  } else {
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 16;
    dst[2] = 16;
  }
}

uint16_t lookup_model_texture(yyjson_mut_val * textures, const char* texture_name) {
  static WritableBuffer texture_cache = { 0 };
  if (texture_cache.buf.buffer.ptr == NULL) {
    texture_cache = create_writable_buffer(1024*4);
  }

  if (texture_name[0] == '#') {
    // if (!strcmp(texture_name, "#overlay")) {
    //   yyjson_val* overlay = yyjson_obj_get(textures, "overlay");
    //   if (overlay) {
    //     DEBUG("Looking up overlay: %s", overlay->valuestring);
    //   }
    // }
    texture_name = yyjson_mut_get_str(yyjson_mut_obj_get(textures, texture_name + 1));
    if (strncmp(texture_name, "minecraft:", 10) == 0) {
      texture_name = texture_name + 10;
    }
  }

  int found_index = -1;
  for (int i = texture_cache.cursor-1; i > 0;) {
    uint8_t len = texture_cache.buf.buffer.ptr[i];
    uint16_t texture_index = (texture_cache.buf.buffer.ptr[i-2] << 8) + texture_cache.buf.buffer.ptr[i-1];
    char* str = (char*)texture_cache.buf.buffer.ptr + i - 3 - len + 1;
    // DEBUG("Loop i=%d len=%d ind=%d str=%s name=%s\n", i, len, texture_index, str, texture_name);
    if (len - 1 == strlen(texture_name) && strncmp(str, texture_name, MIN(len, strlen(texture_name))) == 0) {
      found_index = texture_index;
      break;
    }
    i -= 3 + len;
  }

  if (found_index != -1) {
    if (!strcmp(texture_name, "block/grass_block_side_overlay")) {
      DEBUG("Overlay was already loaded");
    }
    return found_index;
  }

  if (!strcmp(texture_name, "block/grass_block_side_overlay")) {
    DEBUG("Should be loading overlay");
  }
  char fname[1000];
  snprintf(fname, 1000, "data/assets/minecraft/textures/%s.png", texture_name);
  uint16_t index = add_file_texture_to_image(fname, game.texture_sheet, &game.next_texture_loc);
  write_buffer(&texture_cache, string_to_buffer(texture_name));
  write_byte(&texture_cache, 0);
  write_short(&texture_cache, index);
  write_byte(&texture_cache, strlen(texture_name) + 1);

  return index;
}

void read_face_from_json(yyjson_val* faces, char* face_name, MeshFace* into, yyjson_mut_val *textures) {
  yyjson_val *face = yyjson_obj_get(faces, face_name);

  if (face == NULL) {
    return;
  }

  read_json_arr_as_vec4(into->uv, yyjson_obj_get(face, "uv"));
  into->texture = lookup_model_texture(textures, yyjson_get_str(yyjson_obj_get(face, "texture")));
  yyjson_val* tint_index_j = yyjson_obj_get(face, "tintindex");
  into->tint_index = tint_index_j != NULL ? yyjson_get_num(tint_index_j) + 1 : 0;
  // TODO: Figure out how cullface actually works
  into->cull = yyjson_obj_get(face, "cullface") != NULL;
}

// Adds the elements array to the mesh, each element is a MeshCubiod. Returns the number of elements added.
int add_elements_to_blockinfo(BlockInfo* info, yyjson_val* elements, yyjson_mut_val* textures) {
  size_t old_count = info->mesh.num_elements;
  info->mesh.num_elements = old_count + yyjson_arr_size(elements);
  size_t start_index = 0;

  if (old_count != 0) {
    // We need to copy the old elements
    MeshCuboid* old = info->mesh.elements;

    info->mesh.elements = calloc(info->mesh.num_elements, sizeof(MeshCuboid));
    memcpy(info->mesh.elements, old, sizeof(MeshCuboid) * old_count);

    start_index = old_count;

    free(old);
  } else {
    info->mesh.elements = calloc(info->mesh.num_elements, sizeof(MeshCuboid));
  }
  // DEBUG("Elements for %s: %d", info->name, info->mesh.num_elements);
  if (info->mesh.num_elements == 0) {
    return 0;
  }
  size_t arr_index, max;
  yyjson_val* cur_element;
  yyjson_arr_foreach(elements, arr_index, max, cur_element) {
    MeshCuboid cubiod = {0};
    yyjson_val* jfrom = yyjson_obj_get(cur_element, "from");
    yyjson_val* jto = yyjson_obj_get(cur_element, "to");
    yyjson_val* jfaces = yyjson_obj_get(cur_element, "faces");
    cubiod.from[0] = yyjson_get_num(yyjson_arr_get(jfrom, 0));
    cubiod.from[1] = yyjson_get_num(yyjson_arr_get(jfrom, 1));
    cubiod.from[2] = yyjson_get_num(yyjson_arr_get(jfrom, 2));

    cubiod.to[0] = yyjson_get_num(yyjson_arr_get(jto, 0));
    cubiod.to[1] = yyjson_get_num(yyjson_arr_get(jto, 1));
    cubiod.to[2] = yyjson_get_num(yyjson_arr_get(jto, 2));

    read_face_from_json(jfaces, "up", &cubiod.up, textures);
    read_face_from_json(jfaces, "down", &cubiod.down, textures);
    read_face_from_json(jfaces, "north", &cubiod.north, textures);
    read_face_from_json(jfaces, "south", &cubiod.south, textures);
    read_face_from_json(jfaces, "east", &cubiod.east, textures);
    read_face_from_json(jfaces, "west", &cubiod.west, textures);

    info->mesh.elements[start_index + arr_index] = cubiod;
  }

  return info->mesh.num_elements - old_count;
}

void rotate(float *x, float *y, int angle) {
  float tx = *x;
  float ty = *y;
  if (angle == 90) {
    *x = -(ty - 8) + 8;
    *y = tx;
  } else if (angle == 180) {
    *x = -(tx - 8) + 8;
    *y = -(ty - 8) + 8;
  } else if (angle == 270) {
    *x = ty;
    *y = -(tx - 8) + 8;
  }
}

void order(float *x, float *y) {
  if (*x > *y) {
    float temp = *x;
    *x = *y;
    *y = temp;
  }
}

void load_model(yyjson_val* model_spec, BlockInfo* info) {
  char fname[1000];

  yyjson_val *model_name = yyjson_obj_get(model_spec, "model");
  const char *model_name_str = yyjson_get_str(model_name);
  if (model_name_str == NULL) {
    WARN("model property not found for %s!", info->name);
    return;
  }

  if (strncmp(model_name_str, "minecraft:", 10) == 0) {
    model_name_str += 10;
  }
  snprintf(fname, 1000, "data/assets/minecraft/models/%s.json", model_name_str);
  yyjson_doc *model = load_json(fname);
  if (model == NULL) {
    WARN("model not found %s", fname);
    return;
  }
  // Read the hierarchy
  int num_read_elements = 0;
  yyjson_mut_doc *textures_doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *textures = yyjson_mut_obj(textures_doc);
  yyjson_mut_doc_set_root(textures_doc, textures);
  yyjson_doc *parent_model = model;
  yyjson_val *parent_model_root = yyjson_doc_get_root(parent_model);
  while (parent_model) {
    // We need to read the textures in each time
    yyjson_val * parent_textures = yyjson_obj_get(parent_model_root, "textures");

    if (parent_textures != NULL) {
      size_t ind, max;
      yyjson_val* ptexture;
      yyjson_val* ptexture_name;
      yyjson_obj_foreach(parent_textures, ind, max, ptexture_name, ptexture) {
        const char* ptexture_value = yyjson_get_str(ptexture);
        if (ptexture_value[0] == '#') {
          // Look up real texture
          const char* t = yyjson_mut_get_str(yyjson_mut_obj_get(textures, ptexture_value + 1));
          ptexture_value = t;
        }
        // DEBUG("yyjson=%s", yyjson_get_str(ptexture_name));
        const char* ptexture_name_str = yyjson_get_str(ptexture_name);

        yyjson_mut_obj_put(textures, yyjson_mut_strcpy(textures_doc, ptexture_name_str), yyjson_mut_strcpy(textures_doc, ptexture_value));
      }
    }

    // Other than that, it's just parsing the elements
    yyjson_val* elements = yyjson_obj_get(parent_model_root, "elements");
    if (elements != NULL && num_read_elements == 0) {
      num_read_elements = add_elements_to_blockinfo(info, elements, textures);
    }

    yyjson_val *parent_item = yyjson_obj_get(parent_model_root, "parent");
    if (parent_item != NULL) {
      const char* parent_name = yyjson_get_str(parent_item);
      if (strncmp(parent_name, "minecraft:", 10) == 0) {
        parent_name += 10;
      }
      snprintf(fname, 1000, "data/assets/minecraft/models/%s.json", parent_name);
      if (parent_model != model) {
        yyjson_doc_free(parent_model);
      }
      parent_model = load_json(fname);
      parent_model_root = yyjson_doc_get_root(parent_model);
    } else {
      parent_model = NULL;
    }
  }
  yyjson_mut_doc_free(textures_doc);

  // Uncomment to skip rotations
  // return;

  // Perform rotation
  yyjson_val *y_rotation_j = yyjson_obj_get(model_spec, "y");
  int y_rotation = 0;
  if (y_rotation_j != NULL) {
    y_rotation = yyjson_get_num(y_rotation_j);
  }

  for (size_t i = info->mesh.num_elements - num_read_elements; i < info->mesh.num_elements; i++) {
    MeshCuboid* el = &info->mesh.elements[i];
    rotate(el->up.uv + 0, el->up.uv + 1, y_rotation);
    rotate(el->up.uv + 2, el->up.uv + 3, y_rotation);
    rotate(el->down.uv + 0, el->down.uv + 1, 360 - y_rotation);
    rotate(el->down.uv + 2, el->down.uv + 3, 360 - y_rotation);
    // rotate(el->north.uv + 0, el->north.uv + 1, y_rotation);
    // rotate(el->north.uv + 2, el->north.uv + 3, y_rotation);
    // rotate(el->south.uv + 0, el->south.uv + 1, y_rotation);
    // rotate(el->south.uv + 2, el->south.uv + 3, y_rotation);
    // rotate(el->east.uv + 0, el->east.uv + 1, y_rotation);
    // rotate(el->east.uv + 2, el->east.uv + 3, y_rotation);
    // rotate(el->west.uv + 0, el->west.uv + 1, y_rotation);
    // rotate(el->west.uv + 2, el->west.uv + 3, y_rotation);
    rotate(el->from + 0, el->from + 2, y_rotation);
    rotate(el->to + 0, el->to + 2, y_rotation);
    order(el->from + 0, el->to + 0);
    order(el->from + 2, el->to + 2);

    if (y_rotation == 90) {
      MeshFace north = el->north;
      el->north = el->east;
      el->east = el->south;
      el->south = el->west;
      el->west = north;
    } else if (y_rotation == 180) {
      MeshFace north = el->north;
      el->north = el->south;
      el->south = north;
      MeshFace east = el->east;
      el->east = el->west;
      el->west = east;
    } else if (y_rotation == 270) {
      MeshFace north = el->north;
      el->north = el->west;
      el->west = el->south;
      el->south = el->east;
      el->east = north;
    }
  }
}

void load_multipart_state(yyjson_val *state, BlockInfo* info, yyjson_val *multipart) {
  yyjson_val *properties = yyjson_obj_get(state, "properties");

  // Iterate through multiparts
  // Each one can add elements if its state properties matches the "when" clause
  yyjson_val* part;
  size_t index, max;
  yyjson_arr_foreach(multipart, index, max, part) {
    bool all_properties_match = true;
    yyjson_val* when = yyjson_obj_get(part, "when");
    if (when != NULL) {
      yyjson_val* condition_key, * condition_value;
      size_t index, max;
      yyjson_obj_foreach(when, index, max, condition_key, condition_value) {
        yyjson_val* prop = yyjson_obj_get(properties, yyjson_get_str(condition_key));
        all_properties_match = yyjson_equals(prop, condition_value);
        if (!all_properties_match) {
          break;
        }
      }
    }

    if (!all_properties_match) {
      continue;
    }

    yyjson_val* apply = yyjson_obj_get(part, "apply");
    if (yyjson_is_arr(apply)) {
      // Pick the first element if there are mulitple options for the variant
      apply = yyjson_arr_get(apply, 0);
    }

    load_model(apply, info);
  }
}


void load_variant_state(yyjson_val* state, BlockInfo* info, yyjson_val* variants) {
  char key_buffer[1000];
  char value_buffer[1000];

  yyjson_val* state_id = yyjson_obj_get(state, "id");
  yyjson_val* properties = yyjson_obj_get(state, "properties");

  // New plan
  // Iterate through variants
  // Parse the object key for property values "name=value,name=value,...,name=value"
  // For each name, make sure properties has the same property value
  size_t index, max;
  yyjson_val* variant_key, *variant_value;
  variant_value = yyjson_obj_get(variants, "");
  if (properties != NULL && variant_value == NULL) {
    yyjson_obj_foreach(variants, index, max, variant_key, variant_value) {
      bool all_properties_match = true;
      const char *ch = yyjson_get_str(variant_key);
      while (*ch != '\0') {
        const char *end = ch;
        while (*end != '=') {
          end += 1;
        }
        memcpy(key_buffer, ch, end - ch);
        key_buffer[end - ch] = '\0';
        ch = end + 1;
        end = ch;
        while (*end != ',' && *end != '\0') {
          end += 1;
        }
        memcpy(value_buffer, ch, end - ch);
        value_buffer[end - ch] = '\0';
        if (*end == ',') {
          ch = end + 1;
        } else {
          ch = end;
        }
        yyjson_val* property_value = yyjson_obj_get(properties, key_buffer);
        if (property_value == NULL) {
          all_properties_match = false;
          break;
        }
        if (yyjson_is_true(property_value) && strcmp(value_buffer, "true")) {
          all_properties_match = false;
          break;
        }
        if (yyjson_is_false(property_value) && strcmp(value_buffer, "false")) {
          all_properties_match = false;
          break;
        }
        if (yyjson_is_str(property_value) && strcmp(value_buffer, yyjson_get_str(property_value))) {
          all_properties_match = false;
          break;
        }
        if (yyjson_get_int(property_value) && atoi(value_buffer) != yyjson_get_int(property_value)) {
          all_properties_match = false;
          break;
        }
      }
      if (all_properties_match) {
        break;
      }
    }
  }

  if (variant_value == NULL) {
    WARN("Failed to find variant for state %d", yyjson_get_int(state_id));
    assert(false);
    return;
  }

  if (yyjson_is_arr(variant_value)) {
    // Pick the first element if there are mulitple options for the variant
    variant_value = yyjson_arr_get(variant_value, 0);
  }

  load_model(variant_value, info);
}

void load_block_states(const char* block_name, yyjson_val* block) {
  char fname[1000];

  BlockInfo shared_info = {0};
  if (strncmp(block_name, "minecraft:", 10) == 0) {
    block_name += 10;
  }

  yyjson_val* definition = yyjson_obj_get(block, "definition");
  yyjson_val* type_json = yyjson_obj_get(definition, "type");
  const char* type = yyjson_get_str(type_json);
  // TODO deal with this correctly, these pointers get copied into a bunch of spots
  // A mempool for all the blockstates could work well, or just ignore it and assume we never free them
  shared_info.type = copy_string(type);
  shared_info.name = copy_string(block_name);
  if (
    strcmp(type, "minecraft:air") == 0 ||
    strcmp(type, "minecraft:flower") == 0 ||
    strcmp(type, "minecraft:vine") == 0 ||
    strcmp(type, "minecraft:dry_vegetation") == 0 ||
    strcmp(type, "minecraft:firefly_bush") == 0 ||
    strcmp(type, "minecraft:mushroom") == 0 ||
    strcmp(type, "minecraft:liquid") == 0 ||
    strcmp(type, "minecraft:seagrass") == 0 ||
    strcmp(type, "minecraft:tall_seagrass") == 0 ||
    strcmp(type, "minecraft:flower_bed") == 0 ||
    strcmp(type, "minecraft:bush") == 0
  ) {
    shared_info.passable = true;
    shared_info.transparent = true;
  }
  if (strcmp(type, "minecraft:leaf_litter") == 0) {
    shared_info.passable = true;
    shared_info.transparent = true;
    shared_info.dry_foliage = true;
  }
  if (strcmp(type, "minecraft:grass") == 0) {
    shared_info.grass = true;
  }
  if (
    strcmp(type, "minecraft:tall_grass") == 0 ||
    strcmp(type, "minecraft:double_plant") == 0 ||
    strcmp(type, "minecraft:sugar_cane") == 0 ||
    strcmp(type, "minecraft:bush") == 0
  ) {
    shared_info.passable = true;
    shared_info.transparent = true;
    shared_info.grass = true;
  }
  if (
    strcmp(type, "minecraft:leaves") == 0 ||
    strcmp(type, "minecraft:tinted_particle_leaves") == 0 ||
    strcmp(type, "minecraft:waterlily") == 0 ||
    strcmp(type, "minecraft:vine") == 0
  ) {
    shared_info.transparent = true;
    shared_info.foliage = true;
  }

  snprintf(fname, 1000, "data/assets/minecraft/blockstates/%s.json", block_name);
  yyjson_doc* blockstate_doc = load_json(fname);
  if (blockstate_doc == NULL) {
    WARN("blockstate not found for %s", block_name);
    return;
  }
  yyjson_val* blockstate = yyjson_doc_get_root(blockstate_doc);


  yyjson_val* multipart = yyjson_obj_get(blockstate, "multipart");
  yyjson_val* variants = yyjson_obj_get(blockstate, "variants");

  yyjson_val* states = yyjson_obj_get(block, "states");
  if (states != NULL) {
    yyjson_val* state;
    size_t index, max;
    yyjson_arr_foreach(states, index, max, state) {
      int id = -1;
      yyjson_val* state_id = yyjson_obj_get(state, "id");
      if (state_id != NULL && yyjson_is_int(state_id)) {
        id = yyjson_get_int(state_id);
      } else {
        WARN("No state id");
      }

      BlockInfo info = shared_info;
      info.state = id;
      if (multipart != NULL) {
        load_multipart_state(state, &info, multipart);
      } else if (variants != NULL) {
        load_variant_state(state, &info, variants);
      } else {
        WARN("Should be multipart or variant");
        assert(false);
      }

      // Calculate if it is a full block
      info.fullblock = info.mesh.num_elements != 0;
      for (size_t el = 0; el < info.mesh.num_elements; el++) {
        if (!(glm_vec3_eq(info.mesh.elements[el].from, 0) && glm_vec3_eq(info.mesh.elements[el].to, 16))) {
          info.fullblock = false;
          break;
        }
      }

      // Save the block state
      if (id != -1) {
        game.block_info[id] = info;
      }
    }
  } else {
    WARN("No states for %s", shared_info.name);
  }
  yyjson_doc_free(blockstate_doc);
}

void load_blocks() {
  game.blocks = load_json("data/blocks.json");
  yyjson_val* blocks = yyjson_doc_get_root(game.blocks);
  yyjson_val* block_name;
  yyjson_val* block_value;
  size_t index, max;
  yyjson_obj_foreach(blocks, index, max, block_name, block_value) {
    load_block_states(yyjson_get_str(block_name), block_value);
  }
}

void init_glfw() {
  if (!glfwInit()) exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  game.window = glfwCreateWindow(960, 720, "cmc!!!!!!!!!!!!!!!", NULL, NULL);
  assert(game.window);

  glfwSetKeyCallback(game.window, handle_glfw_key);
  glfwSetFramebufferSizeCallback(game.window, handle_glfw_framebuffer_size);
  glfwSetCursorPosCallback(game.window, handle_glfw_cursor_pos);
  glfwSetMouseButtonCallback(game.window, handle_glfw_set_mouse_button);
  glfwSetScrollCallback(game.window, handle_glfw_set_scroll);
}

void init_surface() {
  game.instance = wgpuCreateInstance(NULL);
  assert(game.instance);

#if defined(GLFW_EXPOSE_NATIVE_COCOA)
  {
    id metal_layer = NULL;
    NSWindow *ns_window = glfwGetCocoaWindow(game.window);
    [ns_window.contentView setWantsLayer:YES];
    metal_layer = [CAMetalLayer layer];
    [ns_window.contentView setLayer:metal_layer];
    game.surface = wgpuInstanceCreateSurface(
      game.instance,
      &(const WGPUSurfaceDescriptor){
        .nextInChain =
          (const WGPUChainedStruct *)&(
            const WGPUSurfaceDescriptorFromMetalLayer
          ){
            .chain =
              (const WGPUChainedStruct){
                .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
              },
            .layer = metal_layer,
          },
      }
    );
  }
#elif defined(GLFW_EXPOSE_NATIVE_WAYLAND) && defined(GLFW_EXPOSE_NATIVE_X11)
  if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
    Display *x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(game.window);
    game.surface = wgpuInstanceCreateSurface(
      game.instance,
      &(const WGPUSurfaceDescriptor){
        .nextInChain =
          (const WGPUChainedStruct *)&(
            const WGPUSurfaceDescriptorFromXlibWindow
          ){
            .chain =
              (const WGPUChainedStruct){
                .sType = WGPUSType_SurfaceDescriptorFromXlibWindow,
              },
            .display = x11_display,
            .window = x11_window,
          },
      }
    );
  }
  if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
    struct wl_display *wayland_display = glfwGetWaylandDisplay();
    struct wl_surface *wayland_surface = glfwGetWaylandWindow(game.window);
    game.surface = wgpuInstanceCreateSurface(
      game.instance,
      &(const WGPUSurfaceDescriptor){
        .nextInChain =
          (const WGPUChainedStruct *)&(
            const WGPUSurfaceDescriptorFromWaylandSurface
          ){
            .chain =
              (const WGPUChainedStruct){
                .sType =
                  WGPUSType_SurfaceDescriptorFromWaylandSurface,
              },
            .display = wayland_display,
            .surface = wayland_surface,
          },
      }
    );
  }
#elif defined(GLFW_EXPOSE_NATIVE_WIN32)
  {
    HWND hwnd = glfwGetWin32Window(game.window);
    HINSTANCE hinstance = GetModuleHandle(NULL);
    game.surface = wgpuInstanceCreateSurface(
      game.instance,
      &(const WGPUSurfaceDescriptor){
        .nextInChain =
          (const WGPUChainedStruct *)&(
            const WGPUSurfaceDescriptorFromWindowsHWND
          ){
            .chain =
              (const WGPUChainedStruct){
                .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
              },
            .hinstance = hinstance,
            .hwnd = hwnd,
          },
      }
    );
  }
#else
#error "Unsupported GLFW native platform"
#endif
  assert(game.surface);

  wgpuInstanceRequestAdapter(
    game.instance,
    &(const WGPURequestAdapterOptions){
      .compatibleSurface = game.surface,
    },
    handle_request_adapter, &game
  );
  assert(game.adapter);

  wgpuAdapterRequestDevice(game.adapter, NULL, handle_request_device, &game);
  assert(game.device);

  game.queue = wgpuDeviceGetQueue(game.device);
  assert(game.queue);

  wgpuSurfaceGetCapabilities(game.surface, game.adapter, &game.surface_capabilities);

  game.config = (const WGPUSurfaceConfiguration){
    .device = game.device,
    .usage = WGPUTextureUsage_RenderAttachment,
    .format = game.surface_capabilities.formats[0],
    .presentMode = WGPUPresentMode_Fifo,
    .alphaMode = game.surface_capabilities.alphaModes[0],
  };

  game.depth_texture_descriptor = (WGPUTextureDescriptor){
    .usage = WGPUTextureUsage_RenderAttachment,
    .dimension = WGPUTextureDimension_2D,
    .size = {
      .depthOrArrayLayers = 1,
    },
    .format = WGPUTextureFormat_Depth24Plus,
    .mipLevelCount = 1,
    .sampleCount = 1,
  };
}

void generate_block_breaking_mesh() {
  int q = 0;
  float *quads = game.block_overlay_buffer;
  for (int block = 0; block < game.number_of_blocks_being_broken; block++) {
    float s = 0.0005;
    vec3 pos;
    pos[0] = game.blocks_being_broken[block].position[0];
    pos[1] = game.blocks_being_broken[block].position[1];
    pos[2] = game.blocks_being_broken[block].position[2];
    int texture = game.destroy_stage_textures[game.blocks_being_broken[block].stage];
    // printf("Blocks being broken stage %d, texture %d\n", game.blocks_being_broken[block].stage, texture);

    for (int d = 0; d <= 2; d++) {
      int d1 = (d + 1) % 3;
      int d2 = (d + 2) % 3;
      float du[3] = {0};
      du[d1] = 1;
      float dv[3] = {0};
      dv[d2] = 1;
      for (int p = 0; p <= 1; p++) {
        float dd[3] = {0};
        memset(dd, 0, 3 * sizeof(float));
        dd[d] = p;

        int normal = (p == 1 ? 1 : -1) * (d + 1);
        quads[q + 0] = pos[0] + (dd[0] == 0 ? -s : 1 + s);
        quads[q + 1] = pos[1] + (dd[1] == 0 ? -s : 1 + s);
        quads[q + 2] = pos[2] + (dd[2] == 0 ? -s : 1 + s);
        quads[q + 3] = 1;
        quads[q + 4] = 1;
        quads[q + 5] = 1;
        quads[q + 6] = 1;
        quads[q + 7] = 1;
        quads[q + 8] = 1;
        quads[q + 9] = texture;
        quads[q + 10] = 0;  // overlay tile
        quads[q + 11] = 1;  // sky light
        quads[q + 12] = 1;  // block light
        quads[q + 13] = normal;
        q += FLOATS_PER_VERTEX;

        quads[q + 0] = pos[0] + (du[0] + dd[0] == 0 ? -s : 1 + s);
        quads[q + 1] = pos[1] + (du[1] + dd[1] == 0 ? -s : 1 + s);
        quads[q + 2] = pos[2] + (du[2] + dd[2] == 0 ? -s : 1 + s);
        quads[q + 3] = 1;
        quads[q + 4] = 1;
        quads[q + 5] = 1;
        quads[q + 6] = 1;
        quads[q + 7] = 0;
        quads[q + 8] = 1;
        quads[q + 9] = texture;
        quads[q + 10] = 0;  // overlay tile
        quads[q + 11] = 1;  // sky light
        quads[q + 12] = 1;  // block light
        quads[q + 13] = normal;

        q += FLOATS_PER_VERTEX;
        quads[q + 0] = pos[0] + (du[0] + dv[0] + dd[0] == 0 ? -s : 1 + s);
        quads[q + 1] = pos[1] + (du[1] + dv[1] + dd[1] == 0 ? -s : 1 + s);
        quads[q + 2] = pos[2] + (du[2] + dv[2] + dd[2] == 0 ? -s : 1 + s);
        quads[q + 3] = 1;
        quads[q + 4] = 1;
        quads[q + 5] = 1;
        quads[q + 6] = 1;
        quads[q + 7] = 0;
        quads[q + 8] = 0;
        quads[q + 9] = texture;
        quads[q + 10] = 0;  // overlay tile
        quads[q + 11] = 1;  // sky light
        quads[q + 12] = 1;  // block light
        quads[q + 13] = normal;
        q += FLOATS_PER_VERTEX;

        quads[q + 0] = pos[0] + (dv[0] + dd[0] == 0 ? -s : 1 + s);
        quads[q + 1] = pos[1] + (dv[1] + dd[1] == 0 ? -s : 1 + s);
        quads[q + 2] = pos[2] + (dv[2] + dd[2] == 0 ? -s : 1 + s);
        quads[q + 3] = 1;
        quads[q + 4] = 1;
        quads[q + 5] = 1;
        quads[q + 6] = 1;
        quads[q + 7] = 1;
        quads[q + 8] = 0;
        quads[q + 9] = texture;
        quads[q + 10] = 0;  // overlay tile
        quads[q + 11] = 1;  // sky light
        quads[q + 12] = 1;  // block light
        quads[q + 13] = normal;
        q += FLOATS_PER_VERTEX;
      }
    }
  }
}

void update_block_breaking_stages() {
  float block_destruction_time = 1.5 * 0.6;

  if (game.block_breaking_start != 0 && game.current_time - game.block_breaking_start > block_destruction_time) {
    vec3 pos;
    pos[0] = game.block_breaking_position[0];
    pos[1] = game.block_breaking_position[1];
    pos[2] = game.block_breaking_position[2];
    world_set_block(&game.world, pos, 0, game.block_info, game.biome_info, game.device);
    mcapi_send_player_action(game.conn, (mcapiPlayerActionPacket){
                                          .face = game.block_breaking_face,
                                          .position = {game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2]},
                                          .status = MCAPI_ACTION_DIG_FINISH,
                                          .sequence_num = game.block_breaking_seq_num,
                                        });
    DEBUG("Sending block broken face=%d, x=%d, y=%d, z=%d, seq=%d", game.block_breaking_face, game.block_breaking_position[0], game.block_breaking_position[1], game.block_breaking_position[2], game.block_breaking_seq_num);
    game.block_breaking_start = 0;
  }

  // Calculate block breaking stage
  bool changed = false;

  if (game.block_breaking_start != 0) {
    if (game.number_of_blocks_being_broken == 0 || !glm_ivec3_eqv(game.blocks_being_broken[0].position, game.block_breaking_position)) {
      game.number_of_blocks_being_broken = 1;
      game.blocks_being_broken[0].position[0] = game.block_breaking_position[0];
      game.blocks_being_broken[0].position[1] = game.block_breaking_position[1];
      game.blocks_being_broken[0].position[2] = game.block_breaking_position[2];
      changed = true;
    }

    int new_stage = (game.current_time - game.block_breaking_start) / block_destruction_time * 10;

    if (game.blocks_being_broken[0].stage != new_stage) {
      game.blocks_being_broken[0].stage = new_stage;
      changed = true;
    }
  } else {
    if (game.number_of_blocks_being_broken != 0) {
      game.number_of_blocks_being_broken = 0;
    }
  }

  // Generate block breaking mesh
  if (changed) {
    generate_block_breaking_mesh();
    wgpuQueueWriteBuffer(game.queue, game.block_overlay_vertex_buffer, 0, &game.block_overlay_buffer, sizeof(game.block_overlay_buffer));
  }
}

int tick_count = 0;

void tick() {
  tick_count += 1;
  update_player_position((float)(1.0 / TICKS_PER_SECOND));
  update_block_breaking_stages();
  if (tick_count % 10 == 0) {
    DEBUG("target_material: %s x: %.2f y: %.2f z: %.2f chunk: %d %d", game.block_info[game.target_material].name, game.position[0], game.position[1], game.position[2], (int)(floor(game.position[0] / CHUNK_SIZE)), (int)(floor(game.position[2] / CHUNK_SIZE)));
  }
}

int main(int argc, char *argv[]) {
  INFO("Starting cmc...");
  if (argc < 6) {
    perror("Usage: cmc [username] [server ip] [port] [uuid] [access_token]\n");
    exit(1);
  }

  char *username = argv[1];
  char *server_ip = argv[2];
  long long _port = strtol(argv[3], NULL, 10);
  char *uuid = argv[4];
  char *access_token = argv[5];

  if (_port < 1 || _port > 65535) {
    FATAL("Invalid port. Must be between 1 and 65535");
    exit(1);
  }

  unsigned short port = _port;

  // Init block overlay renderer
  char fname[100];
  for (int i = 0; i < 10; i++) {
    snprintf(fname, 100, "data/assets/minecraft/textures/block/destroy_stage_%d.png", i);
    game.destroy_stage_textures[i] = add_file_texture_to_image_sub_opacity(fname, game.texture_sheet, &game.next_texture_loc, 64);
  }

  init_mcapi(server_ip, port, uuid, access_token, username);
  frmwrk_setup_logging(WGPULogLevel_Warn);
  load_blocks();
  save_image("texture_sheet.png", game.texture_sheet, TEXTURE_SIZE * TEXTURE_TILES, TEXTURE_SIZE * TEXTURE_TILES);

  init_glfw();
  init_surface();
  block_selected_renderer_init();
  chunk_renderer_init();
  sky_renderer_init();

  game.block_overlay_vertex_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Block Overlay Vertex Buffer",
      .content = (void *)game.block_overlay_buffer,
      .content_size = sizeof(game.block_overlay_buffer),
      .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
    }
  );

  int width, height;
  glfwGetWindowSize(game.window, &width, &height);
  update_window_size(width, height);

  game.last_render_time = glfwGetTime();
  game.target_render_time = 1.0 / 60.0;

  game.last_tick_time = glfwGetTime();
  game.target_tick_time = 1.0 / TICKS_PER_SECOND;

  while (!glfwWindowShouldClose(game.window)) {
    mcapi_poll(game.conn);
    glfwPollEvents();

    game.current_time = glfwGetTime();

    double delta_tick_time = game.current_time - game.last_tick_time;
    if (delta_tick_time >= game.target_tick_time) {
      tick();
      game.last_tick_time = game.current_time;
    }

    double delta_render_time = game.current_time - game.last_render_time;
    if (delta_render_time < game.target_render_time) {
      continue;
    }
    game.last_render_time = game.current_time;

    // Update block destruction

    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(game.surface, &surface_texture);
    switch (surface_texture.status) {
      case WGPUSurfaceGetCurrentTextureStatus_Success:
        // All good, could check for `surface_texture.suboptimal` here.
        break;
      case WGPUSurfaceGetCurrentTextureStatus_Timeout:
      case WGPUSurfaceGetCurrentTextureStatus_Outdated:
      case WGPUSurfaceGetCurrentTextureStatus_Lost: {
        // Skip this frame, and re-configure surface.
        if (surface_texture.texture != NULL) {
          wgpuTextureRelease(surface_texture.texture);
        }
        int width, height;
        glfwGetWindowSize(game.window, &width, &height);
        if (width != 0 && height != 0) {
          update_window_size(width, height);
        }
        continue;
      }
      case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
      case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
      case WGPUSurfaceGetCurrentTextureStatus_Force32:
        // Fatal error
        FATAL(LOG_PREFIX " get_current_texture status=%#.8x", surface_texture.status);
        abort();
    }
    assert(surface_texture.texture);

    WGPUTextureView frame = wgpuTextureCreateView(surface_texture.texture, NULL);
    assert(frame);

    WGPUTextureView depth_frame = wgpuTextureCreateView(game.depth_texture, NULL);
    assert(depth_frame);

    WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
      game.device,
      &(const WGPUCommandEncoderDescriptor){
        .label = "command_encoder",
      }
    );
    assert(command_encoder);

    WGPURenderPassEncoder render_pass_encoder =
      wgpuCommandEncoderBeginRenderPass(
        command_encoder,
        &(const WGPURenderPassDescriptor){
          .label = "render_pass_encoder",
          .colorAttachmentCount = 1,
          .colorAttachments = (const WGPURenderPassColorAttachment[]){
            (const WGPURenderPassColorAttachment){
              .view = frame,
              .loadOp = WGPULoadOp_Clear,
              .storeOp = WGPUStoreOp_Store,
              //  .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
              .clearValue = (const WGPUColor){
                .r = 0.0,
                .g = 0.0,
                .b = 0.0,
                .a = 1.0,
              },
            },
          },
          .depthStencilAttachment = &(WGPURenderPassDepthStencilAttachment){
            .view = depth_frame,
            .depthClearValue = 1.0f,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
          },
        }
      );
    assert(render_pass_encoder);

    sky_renderer_render(render_pass_encoder);
    chunk_renderer_render(render_pass_encoder);
    block_selected_renderer_render(render_pass_encoder);

    wgpuRenderPassEncoderEnd(render_pass_encoder);

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
      command_encoder,
      &(const WGPUCommandBufferDescriptor){
        .label = "command_buffer",
      }
    );
    assert(command_buffer);

    wgpuQueueSubmit(game.queue, 1, (const WGPUCommandBuffer[]){command_buffer});
    wgpuSurfacePresent(game.surface);

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass_encoder);
    wgpuCommandEncoderRelease(command_encoder);
    wgpuTextureViewRelease(frame);
    wgpuTextureViewRelease(depth_frame);
    wgpuTextureRelease(surface_texture.texture);
  }

  // Free chunks
  for (int i = 0; i < MAX_CHUNKS; i++) {
    if (game.world.chunks[i] != NULL) {
      chunk_destroy(game.world.chunks[i]);
    }
  }

  wgpuRenderPipelineRelease(game.render_pipeline);
  wgpuPipelineLayoutRelease(game.pipeline_layout);
  wgpuShaderModuleRelease(game.shader_module);
  wgpuSurfaceCapabilitiesFreeMembers(game.surface_capabilities);
  wgpuQueueRelease(game.queue);
  wgpuDeviceRelease(game.device);
  wgpuAdapterRelease(game.adapter);
  wgpuSurfaceRelease(game.surface);
  wgpuBufferRelease(game.uniform_buffer);
  glfwDestroyWindow(game.window);
  wgpuInstanceRelease(game.instance);
  glfwTerminate();
  mcapi_destroy_connection(game.conn);
  yyjson_doc_free(game.blocks);
  return 0;
}
