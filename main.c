#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cglm/include/cglm/cglm.h"
#include "chunk.h"
#include "framework.h"
#include "lodepng/lodepng.h"
#include "wgpu/webgpu.h"
#include "wgpu/wgpu.h"

#if defined(GLFW_EXPOSE_NATIVE_COCOA)
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_X11
#include "GLFW/include/GLFW/glfw3.h"
#include "GLFW/include/GLFW/glfw3native.h"

#define LOG_PREFIX "[triangle]"


const float COLLISION_EPSILON = 0.001f;
const float TURN_SPEED = 0.002f;
const float TICKS_PER_SECOND = 60.0f;

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
  float elevation;
  float movement_speed;
  bool on_ground;
  vec3 position;
  vec3 velocity;
  vec3 up;
  vec3 forward;
  vec3 right;
  vec3 look;
  World world;
} Game;

struct uniforms {
  mat4 view;
  mat4 projection;
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
            world_set_block(&game->world, target, 0, game->device);
            break;
          case GLFW_MOUSE_BUTTON_RIGHT:
            vec3 air_position;
            glm_vec3_add(target, normal, air_position);
            world_set_block(&game->world, air_position, 1, game->device);
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
        if (m != 0) {
          game->position[1] = floor(new_y + sz[1]) - sz[1] - COLLISION_EPSILON;
          game->velocity[1] = 0;
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
        if (m != 0) {
          game->position[1] = ceil(new_y) + COLLISION_EPSILON;
          game->velocity[1] = 0;
          game->on_ground = true;
          return;
        }
      }
    }
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
      for (int dy = 0; dy < sz[1]; dy += 1) {
        int m = world_get_material(w, (vec3){new_x + sz[0], p[1] + dy, p[2] + dz * sz[2]});
        if (m != 0) {
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
      for (int dy = 0; dy < sz[1]; dy += 1) {
        int m = world_get_material(w, (vec3){new_x - sz[0], p[1] + dy, p[2] + dz * sz[2]});
        if (m != 0) {
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
      for (int dy = 0; dy < sz[1]; dy += 1) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], p[1] + dy, new_z + sz[2]});
        if (m != 0) {
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
      for (int dy = 0; dy < sz[1]; dy += 1) {
        int m = world_get_material(w, (vec3){p[0] + dx * sz[0], p[1] + dy, new_z - sz[2]});
        if (m != 0) {
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
  float speed = game->movement_speed * TICKS_PER_SECOND;
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
    vec3 delta;
    glm_vec3_scale(game->up, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (game->keys[GLFW_KEY_LEFT_SHIFT]) {
    vec3 delta;
    glm_vec3_scale(game->up, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  glm_vec3_mix(game->velocity, desired_velocity, 0.2f, game->velocity);
  //   // If there's gravity, we want to keep the up_ component of the velocity but
  //   // slow down the forward_ component
  //   glm::vec3 up_component = glm::dot(player_gaussian_.velocity, up_) * up_;
  //   glm::vec3 forward_component =
  //       glm::dot(player_gaussian_.velocity, forward_) * forward_;
  //   player_gaussian_.velocity =
  //       up_component + 0.8f * desired_velocity + 0.2f * forward_component;

  // Update position
  vec3 delta;
  glm_vec3_scale(game->velocity, dt, delta);

  vec3 p;
  glm_vec3_copy(game->position, p);
  vec3 np;
  glm_vec3_add(game->position, delta, np);

  game_update_player_y(game, delta[1]);
  if (abs(delta[0]) > abs(delta[2])) {
    game_update_player_x(game, delta[0]);
    game_update_player_z(game, delta[2]);
  } else {
    game_update_player_z(game, delta[2]);
    game_update_player_x(game, delta[0]);
  }

  // // Gravity
  // if (play_mode_ == PlayMode::kNormal) {
  //   player_gaussian_.velocity += gravity_ * dt * up_;
  // }

  // CollidePlayer(player_gaussian_, player_eye_height_, gaussians_);
}

int main(int argc, char *argv[]) {
  UNUSED(argc)
  UNUSED(argv)
  frmwrk_setup_logging(WGPULogLevel_Warn);

  if (!glfwInit()) exit(EXIT_FAILURE);

  Game game = {
    .movement_speed = 0.1f,
    .position = {0.0f, 20.0f, 0.0f},
    .up = {0.0f, 1.0f, 0.0f},
    .forward = {0.0f, 0.0f, 1.0f},
    .right = {-1.0f, 0.0f, 0.0f},
    .look = {0.0f, 0.0f, 1.0f},
    .eye_height = 1.62,
    .size = {0.6, 1.8, 0.6},
  };

  game.instance = wgpuCreateInstance(NULL);
  assert(game.instance);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
    glfwCreateWindow(640, 480, "triangle [wgpu-native + glfw]", NULL, NULL);
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

  unsigned int texture_width;
  unsigned int texture_height;
  unsigned char *image_data = load_image("texture-2.png", &texture_width, &texture_height);

  WGPUTextureDescriptor texture_descriptor = {
    .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
    .dimension = WGPUTextureDimension_2D,
    .size = {
      .width = texture_width,
      .height = texture_height,
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
      .content = (void *)image_data,
      .content_size = texture_width * texture_height * 4,
      .usage = WGPUBufferUsage_CopySrc,
    }
  );

  WGPUImageCopyBuffer image_copy_buffer = {
    .buffer = buffer,
    .layout = {
      .bytesPerRow = texture_width * 4,
      .rowsPerImage = texture_height,
    },
  };
  WGPUImageCopyTexture image_copy_texture = {
    .texture = texture,
  };
  WGPUExtent3D copy_size = {
    .width = texture_width,
    .height = texture_height,
    .depthOrArrayLayers = 1,
  };

  WGPUTextureDataLayout texture_data_layout = {
    .offset = 0,
    .bytesPerRow = texture_width * 4,
    .rowsPerImage = texture_height,
  };

  size_t dataSize = texture_width * texture_height * 4;

  wgpuQueueWriteTexture(
    queue,
    &image_copy_texture,
    image_data,
    dataSize,
    &texture_data_layout,
    &copy_size
  );

  free(image_data);

  struct uniforms uniforms = {
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
      .visibility = WGPUShaderStage_Vertex,
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
  };

  int n = 0;
  for (int cx = -2; cx <= 2; cx += 1) {
    for (int cz = -2; cz <= 2; cz += 1) {
      Chunk *chunk = malloc(sizeof(Chunk));
      game.world.chunks[n] = chunk;
      n += 1;
      chunk->x = cx;
      chunk->z = cz;
      for (int s = 0; s < 24; s += 1) {
        chunk->sections[s].x = cx;
        chunk->sections[s].y = s - 4;
        chunk->sections[s].z = cz;
        for (int x = 0; x < 16; x += 1) {
          for (int z = 0; z < 16; z += 1) {
            for (int y = 0; y < 16; y += 1) {
              float wx = cx * CHUNK_SIZE + x;
              float wy = s * CHUNK_SIZE - 64 + y;
              float wz = cz * CHUNK_SIZE + z;
              double surface = 5.0f * sin(wx / 3.0f) * cos(wz / 3.0f) + 10.0f;
              float material = 0;
              if (wy < surface) {
                material = 1;
                if (wy < surface - 2) {
                  material = 2;
                }
                if (wy < surface - 5) {
                  material = 3;
                }
              }
              chunk->sections[s].data[x + y * 16 + z * 16 * 16] = material;
            }
          }
        }
      }
    }
  }

  for (int ci = 0; ci < MAX_CHUNKS; ci += 1) {
    Chunk *chunk = game.world.chunks[ci];
    if (chunk == NULL) {
      continue;
    }
    Chunk *x_chunk = world_chunk(&game.world, chunk->x - 1, chunk->z);
    Chunk *z_chunk = world_chunk(&game.world, chunk->x, chunk->z - 1);
    if (x_chunk == NULL || z_chunk == NULL) {
      continue;
    }
    for (int s = 0; s < 24; s += 1) {
      ChunkSection *neighbors[3] = {NULL, NULL, NULL};
      neighbors[0] = &x_chunk->sections[s];
      neighbors[2] = &z_chunk->sections[s];
      if (s > 0) {
        neighbors[1] = &chunk->sections[s - 1];
      }
      chunk_section_update_mesh(&chunk->sections[s], neighbors, game.device);
    }
  }

  int max_quads = 16 * 16 * 16 * 3;
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
            .format = surface_capabilities.formats[0],
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
    .format = surface_capabilities.formats[0],
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

  double lastTime = glfwGetTime();
  double targetDeltaTime = 1.0 / 60.0;

  while (!glfwWindowShouldClose(window)) {
    double currentTime = glfwGetTime();
    double deltaTime = currentTime - lastTime;
    if (deltaTime < targetDeltaTime) {
      continue;
    }
    lastTime = currentTime;

    glfwPollEvents();
    update_player_position(&game, (float)deltaTime);

    vec3 eye = {game.position[0], game.position[1] + game.eye_height, game.position[2]};
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
