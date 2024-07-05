#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "cglm/cglm.h"
#include "chunk.h"
#include "framework.h"
#include "lodepng/lodepng.h"
#include "mcapi.h"
#include "wgpu.h"

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
#define TEXTURE_TILES 40

const float COLLISION_EPSILON = 0.001f;
const float TURN_SPEED = 0.002f;
const float TICKS_PER_SECOND = 20.0f;

typedef struct Game {
  WGPUInstance instance;
  WGPUSurface surface;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUSurfaceConfiguration config;
  WGPUTextureDescriptor depth_texture_descriptor;
  WGPUTexture depth_texture;
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
};

typedef struct Uniforms {
  mat4 view;
  mat4 projection;
  float internal_sky_max;
} Uniforms;

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

cJSON *load_json(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(length + 1);
  if (buffer == NULL) {
    fclose(file);
    return NULL;
  }

  fread(buffer, 1, length, file);
  buffer[length] = '\0';
  fclose(file);

  cJSON *json = cJSON_Parse(buffer);
  free(buffer);

  return json;
}

static void handle_request_adapter(
  WGPURequestAdapterStatus status,
  WGPUAdapter adapter, char const *message,
  void *userdata
) {
  if (status == WGPURequestAdapterStatus_Success) {
    Game *game = userdata;
    game->adapter = adapter;
  } else {
    printf(LOG_PREFIX " request_adapter status=%#.8x message=%s\n", status, message);
  }
}
static void handle_request_device(
  WGPURequestDeviceStatus status,
  WGPUDevice device, char const *message,
  void *userdata
) {
  if (status == WGPURequestDeviceStatus_Success) {
    Game *game = userdata;
    game->device = device;
  } else {
    printf(LOG_PREFIX " request_device status=%#.8x message=%s\n", status, message);
  }
}
static void handle_glfw_key(
  GLFWwindow *window, int key, int scancode,
  int action, int mods
) {
  UNUSED(scancode)
  UNUSED(mods)
  Game *game = glfwGetWindowUserPointer(window);
  if (!game || !game->instance) return;

  switch (action) {
    case GLFW_PRESS:
      game->keys[key] = true;
      switch (key) {
        case GLFW_KEY_ESCAPE:
          glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
          game->mouse_captured = false;
          break;
        case GLFW_KEY_R:
          WGPUGlobalReport report;
          wgpuGenerateReport(game->instance, &report);
          frmwrk_print_global_report(report);
          break;
      }
      break;
    case GLFW_RELEASE:
      game->keys[key] = false;
      break;
  }
}

void update_window_size(Game *game, int width, int height) {
  if (game->depth_texture != NULL) {
    wgpuTextureRelease(game->depth_texture);
  }
  game->config.width = width;
  game->config.height = height;
  wgpuSurfaceConfigure(game->surface, &game->config);
  game->depth_texture_descriptor.size.width = game->config.width;
  game->depth_texture_descriptor.size.height = game->config.height;
  game->depth_texture = wgpuDeviceCreateTexture(game->device, &game->depth_texture_descriptor);
}

static void handle_glfw_framebuffer_size(GLFWwindow *window, int width, int height) {
  if (width == 0 && height == 0) {
    return;
  }

  Game *game = glfwGetWindowUserPointer(window);
  if (!game) return;

  update_window_size(game, width, height);
}

static void handle_glfw_cursor_pos(GLFWwindow *window, double xpos, double ypos) {
  Game *game = glfwGetWindowUserPointer(window);
  if (!game) return;

  vec2 current = {xpos, ypos};
  if (!game->mouse_captured) {
    glm_vec2_copy(current, game->last_mouse);
    return;
  };

  vec2 delta;
  glm_vec2_sub(current, game->last_mouse, delta);
  glm_vec2_copy(current, game->last_mouse);
  game->elevation -= delta[1] * TURN_SPEED;
  game->elevation = glm_clamp(game->elevation, -GLM_PI_2 + 0.1f, GLM_PI_2 - 0.1f);

  float delta_azimuth = -delta[0] * TURN_SPEED;
  glm_vec3_rotate(game->forward, delta_azimuth, game->up);
  glm_vec3_normalize(game->forward);
  glm_vec3_cross(game->forward, game->up, game->right);
  glm_vec3_normalize(game->right);
  glm_vec3_copy(game->forward, game->look);
  glm_vec3_rotate(game->look, game->elevation, game->right);
  glm_vec3_normalize(game->look);
}

static void handle_glfw_set_mouse_button(GLFWwindow *window, int button, int action, int mods) {
  static int seq_num = 12;
  UNUSED(mods)
  Game *game = glfwGetWindowUserPointer(window);
  if (!game) return;

  switch (action) {
    case GLFW_PRESS:
      if (!game->mouse_captured) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        game->mouse_captured = true;
        break;
      }
      float reach = 5.0f;
      vec3 target;
      vec3 normal;
      int material;
      vec3 eye = {game->position[0], game->position[1] + game->eye_height, game->position[2]};
      world_target_block(&game->world, eye, game->look, reach, target, normal, &material);
      if (material != 0) {
        switch (button) {
          case GLFW_MOUSE_BUTTON_LEFT:
            world_set_block(&game->world, target, 0, game->block_info, game->biome_info, game->device);
            mcapi_send_player_action(game->conn, (mcapiPlayerActionPacket){
                                                   .face = MCAPI_FACE_EAST,
                                                   .position = {target[0], target[1], target[2]},
                                                   .status = MCAPI_ACTION_DIG_START,
                                                   .sequence_num = seq_num,
                                                 });
            seq_num++;
            break;
          case GLFW_MOUSE_BUTTON_RIGHT:
            vec3 air_position;
            glm_vec3_add(target, normal, air_position);
            world_set_block(&game->world, air_position, 1, game->block_info, game->biome_info, game->device);
            break;
        }
      }
      break;
  }
}

static void handle_glfw_set_scroll(GLFWwindow *window, double xoffset, double yoffset) {
  UNUSED(window)
  UNUSED(xoffset)
  UNUSED(yoffset)
}

void game_update_player_y(Game *game, float delta) {
  vec3 p;
  glm_vec3_copy(game->position, p);
  float new_y = p[1] + delta;
  vec3 sz = {game->size[0] / 2, game->size[1], game->size[2] / 2};
  World *w = &game->world;

  // Move +y
  if (delta > 0 && floor(new_y + sz[1]) > floor(p[1] + sz[1])) {
    for (int dx = -1; dx <= 1; dx += 2) {
      for (int dz = -1; dz <= 1; dz += 2) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], new_y + sz[1], p[2] + dz * sz[2]});
        if (!game->block_info[m].passable) {
          game->position[1] = floor(new_y + sz[1]) - sz[1] - COLLISION_EPSILON;
          game->velocity[1] = 0;
          game->fall_speed = 0;
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
        if (!game->block_info[m].passable) {
          game->position[1] = ceil(new_y) + COLLISION_EPSILON;
          game->velocity[1] = 0;
          game->on_ground = true;
          game->fall_speed = 0;
          return;
        }
      }
    }
  }
  if (new_y > game->position[1] && game->on_ground) {
    game->on_ground = false;
  }
  game->position[1] = new_y;
}

void game_update_player_x(Game *game, float delta) {
  vec3 p;
  glm_vec3_copy(game->position, p);
  float new_x = p[0] + delta;
  vec3 sz = {game->size[0] / 2, game->size[1], game->size[2] / 2};
  World *w = &game->world;

  // Move +x
  if (delta > 0 && floor(new_x + sz[0]) > floor(p[0] + sz[0])) {
    for (int dz = -1; dz <= 1; dz += 2) {
      for (int dy = 0; dy <= 2; dy += 1) {
        int m = world_get_material(w, (vec3){new_x + sz[0], p[1] + 0.5f * dy * sz[1], p[2] + dz * sz[2]});
        if (!game->block_info[m].passable) {
          game->position[0] = floor(new_x + sz[0]) - sz[0] - COLLISION_EPSILON;
          game->velocity[0] = 0;
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
        if (!game->block_info[m].passable) {
          game->position[0] = ceil(new_x - sz[0]) + sz[0] + COLLISION_EPSILON;
          game->velocity[0] = 0;
          return;
        }
      }
    }
  }
  game->position[0] = new_x;
}

void game_update_player_z(Game *game, float delta) {
  vec3 p;
  glm_vec3_copy(game->position, p);
  float new_z = p[2] + delta;
  vec3 sz = {game->size[0] / 2, game->size[1], game->size[2] / 2};
  World *w = &game->world;

  // Move +z
  if (delta > 0 && floor(new_z + sz[2]) > floor(p[2] + sz[2])) {
    for (int dx = -1; dx <= 1; dx += 2) {
      for (int dy = 0; dy <= 2; dy += 1) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], p[1] + 0.5f * dy * sz[1], new_z + sz[2]});
        if (!game->block_info[m].passable) {
          game->position[2] = floor(new_z + sz[2]) - sz[2] - COLLISION_EPSILON;
          game->velocity[2] = 0;
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
        if (!game->block_info[m].passable) {
          game->position[2] = ceil(new_z - sz[2]) + sz[2] + COLLISION_EPSILON;
          game->velocity[2] = 0;
          return;
        }
      }
    }
  }
  game->position[2] = new_z;
}

void update_player_position(Game *game, float dt) {
  vec3 desired_velocity = {0};
  float speed = game->walking_speed;
  if (game->keys[GLFW_KEY_D]) {
    vec3 delta;
    glm_vec3_scale(game->right, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (game->keys[GLFW_KEY_A]) {
    vec3 delta;
    glm_vec3_scale(game->right, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  if (game->keys[GLFW_KEY_W]) {
    vec3 delta;
    glm_vec3_scale(game->forward, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (game->keys[GLFW_KEY_S]) {
    vec3 delta;
    glm_vec3_scale(game->forward, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  if (game->keys[GLFW_KEY_SPACE]) {
    // vec3 delta;
    // glm_vec3_scale(game->up, speed, delta);
    // glm_vec3_add(desired_velocity, delta, desired_velocity);
    if (game->on_ground) {
      game->fall_speed = 0.42f * TICKS_PER_SECOND;
      game->on_ground = false;
    }
  }
  // if (game->keys[GLFW_KEY_LEFT_SHIFT]) {
  //   vec3 delta;
  //   glm_vec3_scale(game->up, speed, delta);
  //   glm_vec3_sub(desired_velocity, delta, desired_velocity);
  // }
  desired_velocity[1] += game->fall_speed;
  glm_vec3_mix(game->velocity, desired_velocity, 0.8f, game->velocity);

  // Update position
  vec3 delta;
  glm_vec3_scale(game->velocity, dt, delta);

  glm_vec3_copy(game->position, game->last_position);

  game_update_player_y(game, delta[1]);
  if (abs(delta[0]) > abs(delta[2])) {
    game_update_player_x(game, delta[0]);
    game_update_player_z(game, delta[2]);
  } else {
    game_update_player_z(game, delta[2]);
    game_update_player_x(game, delta[0]);
  }

  game->fall_speed -= 0.08f * TICKS_PER_SECOND;
  if (game->fall_speed < 0.0f) {
    game->fall_speed *= 0.98f;
  }
}

void on_login_success(mcapiConnection *conn, mcapiLoginSuccessPacket packet) {
  printf("Username: ");
  mcapi_print_str(packet.username);
  printf("\nUUID: %016lx%016lx\n", packet.uuid.upper, packet.uuid.lower);
  printf("%d Properties:\n", packet.number_of_properties);
  for (int i = 0; i < packet.number_of_properties; i++) {
    printf("  ");
    mcapi_print_str(packet.properties[i].name);
    printf(": ");
    mcapi_print_str(packet.properties[i].value);
    printf("\n");
  }

  mcapi_send_login_acknowledged(conn);

  mcapi_set_state(conn, MCAPI_STATE_CONFIG);
}

void on_known_packs(mcapiConnection *conn, mcapiClientboundKnownPacksPacket packet) {
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

void on_registry(mcapiConnection *conn, mcapiRegistryDataPacket packet) {
  if (strncmp(packet.id.ptr, "minecraft:worldgen/biome", packet.id.len) == 0) {
    unsigned int width;
    unsigned int height;
    unsigned char *grass = load_image("data/assets/minecraft/textures/colormap/grass.png", &width, &height);
    assert(width == 256);
    assert(height == 256);
    unsigned char *foliage = load_image("data/assets/minecraft/textures/colormap/foliage.png", &width, &height);
    assert(width == 256);
    assert(height == 256);
    for (int i = 0; i < packet.entry_count; i++) {
      BiomeInfo info = {0};
      info.temperature = mcapi_nbt_get_compound_tag(packet.entries[i], "temperature")->float_value;
      info.downfall = mcapi_nbt_get_compound_tag(packet.entries[i], "downfall")->float_value;
      mcapiNBT *effects = mcapi_nbt_get_compound_tag(packet.entries[i], "effects");
      int_to_rgb(mcapi_nbt_get_compound_tag(effects, "fog_color")->int_value, info.fog_color);
      int_to_rgb(mcapi_nbt_get_compound_tag(effects, "water_color")->int_value, info.water_color);
      int_to_rgb(mcapi_nbt_get_compound_tag(effects, "water_fog_color")->int_value, info.water_fog_color);
      int_to_rgb(mcapi_nbt_get_compound_tag(effects, "sky_color")->int_value, info.sky_color);

      float clamped_temperature = glm_clamp(info.temperature, 0.0f, 1.0f);
      float clamped_downfall = glm_clamp(info.downfall, 0.0f, 1.0f);
      clamped_downfall *= clamped_temperature;
      int x_index = 255 - (int)(clamped_temperature * 255);
      int y_index = 255 - (int)(clamped_downfall * 255);
      int index = y_index * 256 + x_index;
      mcapiNBT *grass_color = mcapi_nbt_get_compound_tag(effects, "grass_color");
      if (grass_color != NULL) {
        info.custom_grass_color = true;
        int_to_rgb(grass_color->int_value, info.grass_color);
      } else {
        info.grass_color[0] = grass[index * 4 + 0] / 255.0f;
        info.grass_color[1] = grass[index * 4 + 1] / 255.0f;
        info.grass_color[2] = grass[index * 4 + 2] / 255.0f;
      }
      mcapiNBT *foliage_color = mcapi_nbt_get_compound_tag(effects, "foliage_color");
      if (foliage_color != NULL) {
        info.custom_foliage_color = true;
        int_to_rgb(foliage_color->int_value, info.foliage_color);
      } else {
        info.foliage_color[0] = foliage[index * 4 + 0] / 255.0f;
        info.foliage_color[1] = foliage[index * 4 + 1] / 255.0f;
        info.foliage_color[2] = foliage[index * 4 + 2] / 255.0f;
      }
      game.biome_info[i] = info;
      // mcapi_print_str(packet.entry_names[i]);
      // printf(" %d: %d, temp %f downfall %f x %d y %d color %f %f %f\n", i, info.custom_grass_color, info.temperature, info.downfall, x_index, y_index, info.grass_color[0], info.grass_color[1], info.grass_color[2]);
      // printf("grass %x %x %x\n", grass[index * 4 + 0], grass[index * 4 + 1], grass[index * 4 + 2]);
      // printf("foliage %x %x %x\n", foliage[index * 4 + 0], foliage[index * 4 + 1], foliage[index * 4 + 2]);
    }
    // free(grass);
    // free(foliage);
  }
}

void on_finish_config(mcapiConnection *conn) {
  mcapi_send_acknowledge_finish_config(conn);

  mcapi_set_state(conn, MCAPI_STATE_PLAY);

  printf("Playing!\n");
}

void on_chunk(mcapiConnection *conn, mcapiChunkAndLightDataPacket packet) {
  Chunk *chunk = malloc(sizeof(Chunk));
  chunk->x = packet.chunk_x;
  chunk->z = packet.chunk_z;
  for (int i = 0; i < 24; i++) {
    chunk->sections[i].x = packet.chunk_x;
    chunk->sections[i].y = i - 4;
    chunk->sections[i].z = packet.chunk_z;
    memcpy(chunk->sections[i].data, packet.chunk_sections[i].blocks, 4096 * sizeof(int));
    memcpy(chunk->sections[i].biome_data, packet.chunk_sections[i].biomes, 64 * sizeof(int));
    memcpy(chunk->sections[i].sky_light, packet.sky_light_array[i + 1], 4096);
    memcpy(chunk->sections[i].block_light, packet.block_light_array[i + 1], 4096);
  }
  world_add_chunk(&game.world, chunk);
  world_init_new_meshes(&game.world, game.block_info, game.biome_info, game.device);
}

void on_position(mcapiConnection *conn, mcapiSynchronizePlayerPositionPacket packet) {
  printf("%f %f %f\n", packet.x, packet.y, packet.z);
  game.position[0] = packet.x;
  game.position[1] = packet.y;
  game.position[2] = packet.z;

  float pitch = packet.pitch * GLM_PIf / 180.0f;
  float yaw = packet.yaw * GLM_PIf / 180.0f;

  game.elevation = -pitch;
  game.look[0] = -cos(pitch) * sin(yaw);
  game.look[1] = -sin(pitch);
  game.look[2] = cos(pitch) * cos(yaw);
  glm_normalize(game.look);
  glm_vec3_cross(game.look, game.up, game.right);
  glm_normalize(game.right);
  glm_vec3_cross(game.up, game.right, game.forward);
  glm_normalize(game.forward);

  mcapi_send_confirm_teleportation(conn, (mcapiConfirmTeleportationPacket){teleport_id : packet.teleport_id});
}

void on_update_time(mcapiConnection *conn, mcapiUpdateTimePacket packet) {
  printf("World age: %ld, Time of day: %ld\n", packet.world_age, packet.time_of_day % 24000);
  game.time_of_day = packet.time_of_day;
}

int add_texture(cJSON *textures, const char *name, unsigned char *texture_sheet, int *cur_texture) {
  char fname[1000];
  cJSON *texture = cJSON_GetObjectItemCaseSensitive(textures, name);
  if (texture == NULL) {
    return 0;
  }
  char *texture_name = texture->valuestring;
  if (strncmp(texture_name, "minecraft:", 10) == 0) {
    texture_name += 10;
  }
  snprintf(fname, 1000, "data/assets/minecraft/textures/%s.png", texture_name);
  unsigned int width, height;
  unsigned char *rgba = load_image(fname, &width, &height);
  if (rgba == NULL) {
    // printf("%s not found for %s\n", name, texture_name);
    return 0;
    // } else {
    //   printf("%s found for %s, %d\n", name, texture_name, *cur_texture);
  }
  int texture_id = *cur_texture;
  *cur_texture += 1;
  int full_width = TEXTURE_SIZE * TEXTURE_TILES;
  int tile_start_x = (texture_id % TEXTURE_TILES) * TEXTURE_SIZE;
  int tile_start_y = (texture_id / TEXTURE_TILES) * TEXTURE_SIZE;
  width = glm_min(width, TEXTURE_SIZE);
  height = glm_min(height, TEXTURE_SIZE);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int i = (y * width + x) * 4;
      int j = ((tile_start_y + y) * full_width + (tile_start_x + x)) * 4;
      // int j = (texture_id * TEXTURE_SIZE * TEXTURE_SIZE + y * TEXTURE_SIZE + x) * 4;
      texture_sheet[j + 0] = rgba[i + 0];
      texture_sheet[j + 1] = rgba[i + 1];
      texture_sheet[j + 2] = rgba[i + 2];
      texture_sheet[j + 3] = rgba[i + 3];
    }
  }
  return texture_id;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    perror("Usage: cmc [username] [server ip] [port]\n");
    exit(1);
  }

  char *username = argv[1];
  char *server_ip = argv[2];
  long long _port = strtol(argv[3], NULL, 10);

  if (_port < 1 || _port > 65535) {
    perror("Invalid port. Must be between 1 and 65535\n");
    exit(1);
  }

  unsigned short port = _port;

  frmwrk_setup_logging(WGPULogLevel_Warn);

  mcapiConnection *conn = mcapi_create_connection(server_ip, port);
  game.conn = conn;

  mcapi_send_handshake(
    conn,
    (mcapiHandshakePacket){
      .protocol_version = 767,
      .server_addr = server_ip,
      .server_port = port,
      .next_state = 2,
    }
  );

  mcapi_send_login_start(
    conn,
    (mcapiLoginStartPacket){
      .username = mcapi_to_string(username),
      .uuid = (mcapiUUID){
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
  mcapi_set_synchronize_player_position_cb(conn, on_position);
  mcapi_set_registry_data_cb(conn, on_registry);
  mcapi_set_update_time_cb(conn, on_update_time);

  int num_id = 0;
  int max_id = -1;
  int cur_texture = 1;
  char fname[1000];
  cJSON *blocks = load_json("data/blocks.json");
  cJSON *block = blocks->child;
  unsigned char texture_sheet[TEXTURE_SIZE * TEXTURE_SIZE * TEXTURE_TILES * TEXTURE_TILES * 4] = {0};
  while (block != NULL) {
    BlockInfo info = {0};
    char *block_name = block->string;
    if (strncmp(block_name, "minecraft:", 10) == 0) {
      block_name += 10;
    }

    cJSON *definition = cJSON_GetObjectItemCaseSensitive(block, "definition");
    cJSON *type = cJSON_GetObjectItemCaseSensitive(definition, "type");
    if (
      strcmp(type->valuestring, "minecraft:air") == 0 ||
      strcmp(type->valuestring, "minecraft:flower") == 0
    ) {
      info.passable = true;
      info.transparent = true;
    }
    if (strcmp(type->valuestring, "minecraft:grass") == 0) {
      info.grass = true;
    }
    if (
      strcmp(type->valuestring, "minecraft:tall_grass") == 0 ||
      strcmp(type->valuestring, "minecraft:double_plant") == 0
    ) {
      info.passable = true;
      info.transparent = true;
      info.grass = true;
    }
    if (strcmp(type->valuestring, "minecraft:leaves") == 0) {
      info.transparent = true;
      info.foliage = true;
    }

    snprintf(fname, 1000, "data/assets/minecraft/models/block/%s.json", block_name);
    cJSON *model = load_json(fname);
    if (model == NULL) {
      snprintf(fname, 1000, "data/assets/minecraft/models/item/%s.json", block_name);
      model = load_json(fname);
    }
    if (model == NULL) {
      snprintf(fname, 1000, "data/assets/minecraft/blockstates/%s.json", block_name);
      cJSON *blockstate = load_json(fname);
      if (blockstate == NULL) {
        printf("blockstate not found for %s\n", block_name);
        block = block->next;
        continue;
      }
      cJSON *variants = cJSON_GetObjectItemCaseSensitive(blockstate, "variants");
      if (variants == NULL) {
        printf("variants not found for %s\n", block_name);
        block = block->next;
        continue;
      }
      cJSON *variant = variants->child;
      if (variant == NULL) {
        printf("variant not found for %s\n", block_name);
        block = block->next;
        continue;
      }
      cJSON *model_name = cJSON_GetObjectItemCaseSensitive(variant, "model");
      if (model_name == NULL) {
        printf("model not found for %s\n", block_name);
        block = block->next;
        continue;
      }
      char *model_name_str = model_name->valuestring;
      if (strncmp(model_name_str, "minecraft:", 10) == 0) {
        model_name_str += 10;
      }
      snprintf(fname, 1000, "data/assets/minecraft/models/%s.json", model_name_str);
      model = load_json(fname);
    }
    if (model == NULL) {
      printf("model not found for %s\n", block_name);
      block = block->next;
      continue;
    }
    cJSON *textures = cJSON_GetObjectItemCaseSensitive(model, "textures");

    info.texture = add_texture(textures, "texture", texture_sheet, &cur_texture);
    info.texture_bottom = add_texture(textures, "bottom", texture_sheet, &cur_texture);
    info.texture_top = add_texture(textures, "top", texture_sheet, &cur_texture);
    info.texture_end = add_texture(textures, "end", texture_sheet, &cur_texture);
    info.texture_side = add_texture(textures, "side", texture_sheet, &cur_texture);
    info.texture_overlay = add_texture(textures, "overlay", texture_sheet, &cur_texture);
    info.texture_all = add_texture(textures, "all", texture_sheet, &cur_texture);
    info.texture_cross = add_texture(textures, "cross", texture_sheet, &cur_texture);
    info.texture_layer0 = add_texture(textures, "layer0", texture_sheet, &cur_texture);

    cJSON *states = cJSON_GetObjectItemCaseSensitive(block, "states");
    if (states != NULL) {
      cJSON *state = states->child;
      while (state != NULL) {
        cJSON *state_id = cJSON_GetObjectItemCaseSensitive(state, "id");
        if (state_id != NULL && cJSON_IsNumber(state_id)) {
          int id = state_id->valueint;
          num_id += 1;
          if (id > max_id) {
            max_id = id;
          }
          game.block_info[id] = info;
        }
        state = state->next;
      }
    }
    block = block->next;
  }
  // save_image("texture_sheet.png", texture_sheet, TEXTURE_SIZE * TEXTURE_TILES, TEXTURE_SIZE * TEXTURE_TILES);

  game.instance = wgpuCreateInstance(NULL);
  assert(game.instance);

  if (!glfwInit()) exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
    glfwCreateWindow(640, 480, "cmc", NULL, NULL);
  assert(window);

  glfwSetWindowUserPointer(window, (void *)&game);
  glfwSetKeyCallback(window, handle_glfw_key);
  glfwSetFramebufferSizeCallback(window, handle_glfw_framebuffer_size);
  glfwSetCursorPosCallback(window, handle_glfw_cursor_pos);
  glfwSetMouseButtonCallback(window, handle_glfw_set_mouse_button);
  glfwSetScrollCallback(window, handle_glfw_set_scroll);

#if defined(GLFW_EXPOSE_NATIVE_COCOA)
  {
    id metal_layer = NULL;
    NSWindow *ns_window = glfwGetCocoaWindow(window);
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
    Window x11_window = glfwGetX11Window(window);
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
    struct wl_surface *wayland_surface = glfwGetWaylandWindow(window);
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
    HWND hwnd = glfwGetWin32Window(window);
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

  WGPUQueue queue = wgpuDeviceGetQueue(game.device);
  assert(queue);

  WGPUShaderModule shader_module =
    frmwrk_load_shader_module(game.device, "shader.wgsl");
  assert(shader_module);

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
      .mipmapFilter = WGPUFilterMode_Nearest,
      .maxAnisotropy = 1,
    }
  );

  WGPUBuffer buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Texture Buffer",
      .content = (void *)texture_sheet,
      .content_size = TEXTURE_SIZE * TEXTURE_SIZE * TEXTURE_TILES * TEXTURE_TILES * 4,
      .usage = WGPUBufferUsage_CopySrc,
    }
  );

  WGPUImageCopyBuffer image_copy_buffer = {
    .buffer = buffer,
    .layout = {
      .bytesPerRow = TEXTURE_SIZE * TEXTURE_TILES * 4,
      .rowsPerImage = TEXTURE_SIZE * TEXTURE_TILES,
    },
  };
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
    queue,
    &image_copy_texture,
    texture_sheet,
    dataSize,
    &texture_data_layout,
    &copy_size
  );

  Uniforms uniforms = {
    .view = GLM_MAT4_IDENTITY_INIT,
    .projection = GLM_MAT4_IDENTITY_INIT,
  };

  WGPUBuffer uniform_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Uniform Buffer",
      .content = (void *)&uniforms,
      .content_size = sizeof(uniforms),
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
      .buffer = uniform_buffer,
      .size = sizeof(uniforms),
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
  WGPUBindGroup bg = wgpuDeviceCreateBindGroup(game.device, &bg_desc);

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

  // Create a sample world

  // int n = 0;
  // for (int cx = -2; cx <= 2; cx += 1) {
  //   for (int cz = -2; cz <= 2; cz += 1) {
  //     Chunk *chunk = malloc(sizeof(Chunk));
  //     game.world.chunks[n] = chunk;
  //     n += 1;
  //     chunk->x = cx;
  //     chunk->z = cz;
  //     for (int s = 0; s < 24; s += 1) {
  //       chunk->sections[s].x = cx;
  //       chunk->sections[s].y = s - 4;
  //       chunk->sections[s].z = cz;
  //       for (int x = 0; x < CHUNK_SIZE; x += 1) {
  //         for (int z = 0; z < CHUNK_SIZE; z += 1) {
  //           for (int y = 0; y < CHUNK_SIZE; y += 1) {
  //             float wx = cx * CHUNK_SIZE + x;
  //             float wy = s * CHUNK_SIZE - 64 + y;
  //             float wz = cz * CHUNK_SIZE + z;
  //             double surface = 5.0f * sin(wx / 15.0f) * cos(wz / 15.0f) + 10.0f;
  //             float material = 0;
  //             if (wy < surface) {
  //               material = 1;
  //               if (wy < surface - 2) {
  //                 material = 2;
  //               }
  //               if (wy < surface - 5) {
  //                 material = 3;
  //               }
  //             }
  //             chunk->sections[s].data[x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE] = material;
  //           }
  //         }
  //       }
  //     }
  //   }
  // }
  // world_init_new_meshes(&game.world, game.device);

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
  int index_count = sizeof(indices) / sizeof(indices[0]);

  WGPUBuffer index_buffer = frmwrk_device_create_buffer_init(
    game.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "index_buffer",
      .content = (void *)indices,
      .content_size = sizeof(indices),
      .usage = WGPUBufferUsage_Index,
    }
  );

  WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(
    game.device,
    &(const WGPUPipelineLayoutDescriptor){
      .label = "pipeline_layout",
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = (const WGPUBindGroupLayout[]){
        bgl,
      },
    }
  );
  assert(pipeline_layout);

  WGPUSurfaceCapabilities surface_capabilities = {0};
  wgpuSurfaceGetCapabilities(game.surface, game.adapter, &surface_capabilities);

  WGPUTextureFormat surface_format = surface_capabilities.formats[0];

  WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(
    game.device,
    &(const WGPURenderPipelineDescriptor){
      .label = "render_pipeline",
      .layout = pipeline_layout,
      .vertex = (const WGPUVertexState){
        .module = shader_module,
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
        .module = shader_module,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = (const WGPUColorTargetState[]){
          (const WGPUColorTargetState){
            .format = surface_format,
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
        .depthCompare = WGPUCompareFunction_Less,
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
  assert(render_pipeline);

  game.config = (const WGPUSurfaceConfiguration){
    .device = game.device,
    .usage = WGPUTextureUsage_RenderAttachment,
    .format = surface_format,
    .presentMode = WGPUPresentMode_Fifo,
    .alphaMode = surface_capabilities.alphaModes[0],
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

  int width, height;
  glfwGetWindowSize(window, &width, &height);
  update_window_size(&game, width, height);

  double lastRenderTime = glfwGetTime();
  double targetRenderTime = 1.0 / 60.0;

  double lastTickTime = glfwGetTime();
  double targetTickTime = 1.0 / TICKS_PER_SECOND;

  while (!glfwWindowShouldClose(window)) {
    mcapi_poll(conn);
    glfwPollEvents();

    double currentTime = glfwGetTime();

    double deltaTickTime = currentTime - lastTickTime;
    if (deltaTickTime >= targetTickTime) {
      // update_player_position(&game, (float)deltaTickTime);
      // Force the update to be one tick
      update_player_position(&game, (float)(1.0 / TICKS_PER_SECOND));
      lastTickTime = currentTime;
      if (mcapi_get_state(conn) == MCAPI_STATE_PLAY) {
        float yaw = -atan2(game.look[0], game.look[2]) / GLM_PIf * 180.0f;
        if (yaw < 0.0f) {
          yaw += 360.0f;
        }
        float pitch = -game.elevation * 180.0f / GLM_PIf;
        mcapi_send_set_player_position_and_rotation(
          conn,
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

    double deltaRenderTime = currentTime - lastRenderTime;
    if (deltaRenderTime < targetRenderTime) {
      continue;
    }
    lastRenderTime = currentTime;

    // vec3 movement;
    // glm_vec3_scale(game.last_movement, deltaTickTime * TICKS_PER_SECOND, movement);
    // vec3 position;
    // glm_vec3_add(game.position, movement, position);
    // vec3 eye = {position[0], position[1] + game.eye_height, position[2]};

    // Interpolate between last and current position for smooth movement
    vec3 position;
    glm_vec3_lerp(game.last_position, game.position, (currentTime - lastTickTime) * TICKS_PER_SECOND, position);
    vec3 eye = {position[0], position[1] + game.eye_height, position[2]};

    // No interpolation
    // vec3 eye = {game.position[0], game.position[1] + game.eye_height, game.position[2]};

    vec3 center;
    glm_vec3_add(eye, game.look, center);

    mat4 view;
    glm_lookat(eye, center, game.up, view);
    memcpy(&uniforms.view, view, sizeof(view));

    mat4 projection;
    glm_perspective(
      GLM_PI_2, (float)game.config.width / (float)game.config.height, 0.01f, 100.0f, projection
    );
    memcpy(&uniforms.projection, projection, sizeof(projection));

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

    uniforms.internal_sky_max = sky_max / 15.0f;
    float sky_color_scale = pow(glm_clamp((sky_max - 4.0) / 11.0, 0.0, 1.0), 3.0);

    // Send uniforms to GPU
    wgpuQueueWriteBuffer(queue, uniform_buffer, 0, &uniforms, sizeof(uniforms));

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
        glfwGetWindowSize(window, &width, &height);
        if (width != 0 && height != 0) {
          update_window_size(&game, width, height);
        }
        continue;
      }
      case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
      case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
      case WGPUSurfaceGetCurrentTextureStatus_Force32:
        // Fatal error
        printf(LOG_PREFIX " get_current_texture status=%#.8x\n", surface_texture.status);
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

    vec3 sky_color;
    world_get_sky_color(&game.world, game.position, game.biome_info, sky_color);

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
                .r = pow(sky_color[0] * sky_color_scale, 2.2),
                .g = pow(sky_color[1] * sky_color_scale, 2.2),
                .b = pow(sky_color[2] * sky_color_scale, 2.2),
                // .r = pow(0.431, 2.2),
                // .g = pow(0.694, 2.2),
                // .b = pow(1.000, 2.2),
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

    wgpuRenderPassEncoderSetIndexBuffer(render_pass_encoder, index_buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, bg, 0, NULL);
    wgpuRenderPassEncoderSetPipeline(render_pass_encoder, render_pipeline);

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

    wgpuRenderPassEncoderEnd(render_pass_encoder);

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
      command_encoder,
      &(const WGPUCommandBufferDescriptor){
        .label = "command_buffer",
      }
    );
    assert(command_buffer);

    wgpuQueueSubmit(queue, 1, (const WGPUCommandBuffer[]){command_buffer});
    wgpuSurfacePresent(game.surface);

    wgpuCommandBufferRelease(command_buffer);
    wgpuRenderPassEncoderRelease(render_pass_encoder);
    wgpuCommandEncoderRelease(command_encoder);
    wgpuTextureViewRelease(frame);
    wgpuTextureRelease(surface_texture.texture);
  }

  wgpuRenderPipelineRelease(render_pipeline);
  wgpuPipelineLayoutRelease(pipeline_layout);
  wgpuShaderModuleRelease(shader_module);
  wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);
  wgpuQueueRelease(queue);
  wgpuDeviceRelease(game.device);
  wgpuAdapterRelease(game.adapter);
  wgpuSurfaceRelease(game.surface);
  // chunk_release(chunk);
  wgpuBufferRelease(uniform_buffer);
  glfwDestroyWindow(window);
  wgpuInstanceRelease(game.instance);
  glfwTerminate();
  return 0;
}
