#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cglm/include/cglm/cglm.h"
#include "chunk.h"
#include "framework.h"
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

const float TURN_SPEED = 0.002f;
const float TICKS_PER_SECOND = 60.0f;

struct demo {
  WGPUInstance instance;
  WGPUSurface surface;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUSurfaceConfiguration config;
  WGPUTextureDescriptor depth_texture_descriptor;
  WGPUTexture depth_texture;
  bool keys[GLFW_KEY_LAST + 1];
  bool mouse_captured;
  vec2 last_mouse;
  float elevation;
  float movement_speed;
  vec3 position;
  vec3 velocity;
  vec3 up;
  vec3 forward;
  vec3 right;
  vec3 look;
};

struct uniforms {
  mat4 view;
  mat4 projection;
};

static void handle_request_adapter(
  WGPURequestAdapterStatus status,
  WGPUAdapter adapter, char const *message,
  void *userdata
) {
  if (status == WGPURequestAdapterStatus_Success) {
    struct demo *demo = userdata;
    demo->adapter = adapter;
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
    struct demo *demo = userdata;
    demo->device = device;
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
  struct demo *demo = glfwGetWindowUserPointer(window);
  if (!demo || !demo->instance) return;

  switch (action) {
    case GLFW_PRESS:
      // printf(LOG_PREFIX " key=%d press\n", key);
      demo->keys[key] = true;
      switch (key) {
        case GLFW_KEY_ESCAPE:
          glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
          demo->mouse_captured = false;
          break;
        case GLFW_KEY_R:
          WGPUGlobalReport report;
          wgpuGenerateReport(demo->instance, &report);
          frmwrk_print_global_report(report);
          break;
      }
      break;
    case GLFW_RELEASE:
      // printf(LOG_PREFIX " key=%d release\n", key);
      demo->keys[key] = false;
      break;
  }
}

void update_window_size(struct demo *demo, int width, int height) {
  if (demo->depth_texture != NULL) {
    wgpuTextureRelease(demo->depth_texture);
  }
  demo->config.width = width;
  demo->config.height = height;
  wgpuSurfaceConfigure(demo->surface, &demo->config);
  demo->depth_texture_descriptor.size.width = demo->config.width;
  demo->depth_texture_descriptor.size.height = demo->config.height;
  demo->depth_texture = wgpuDeviceCreateTexture(demo->device, &demo->depth_texture_descriptor);
}

static void handle_glfw_framebuffer_size(GLFWwindow *window, int width, int height) {
  if (width == 0 && height == 0) {
    return;
  }

  struct demo *demo = glfwGetWindowUserPointer(window);
  if (!demo) return;

  update_window_size(demo, width, height);
  // demo->config.width = width;
  // demo->config.height = height;

  // wgpuSurfaceConfigure(demo->surface, &demo->config);
}

static void handle_glfw_cursor_pos(GLFWwindow *window, double xpos, double ypos) {
  struct demo *demo = glfwGetWindowUserPointer(window);
  if (!demo) return;

  vec2 current = {xpos, ypos};
  if (!demo->mouse_captured) {
    glm_vec2_copy(current, demo->last_mouse);
    return;
  };

  // printf(LOG_PREFIX " cursor x=%.1f y=%.1f\n", xpos, ypos);

  vec2 delta;
  glm_vec2_sub(current, demo->last_mouse, delta);
  glm_vec2_copy(current, demo->last_mouse);
  demo->elevation -= delta[1] * TURN_SPEED;
  demo->elevation = glm_clamp(demo->elevation, -GLM_PI_2 + 0.1f, GLM_PI_2 - 0.1f);

  float delta_azimuth = -delta[0] * TURN_SPEED;
  glm_vec3_rotate(demo->forward, delta_azimuth, demo->up);
  glm_vec3_normalize(demo->forward);
  glm_vec3_cross(demo->forward, demo->up, demo->right);
  glm_vec3_normalize(demo->right);
  glm_vec3_copy(demo->forward, demo->look);
  glm_vec3_rotate(demo->look, demo->elevation, demo->right);
  glm_vec3_normalize(demo->look);
}

static void handle_glfw_set_mouse_button(GLFWwindow *window, int button, int action, int mods) {
  UNUSED(mods)
  struct demo *demo = glfwGetWindowUserPointer(window);
  if (!demo) return;

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    switch (action) {
      case GLFW_PRESS:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        demo->mouse_captured = true;
        break;
    }
  }
}

static void handle_glfw_set_scroll(GLFWwindow *window, double xoffset, double yoffset) {
  UNUSED(window)
  UNUSED(xoffset)
  UNUSED(yoffset)
}

void update_player_position(struct demo *demo, float dt) {
  vec3 desired_velocity = {0};
  float speed = demo->movement_speed * TICKS_PER_SECOND;
  if (demo->keys[GLFW_KEY_D]) {
    vec3 delta;
    glm_vec3_scale(demo->right, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (demo->keys[GLFW_KEY_A]) {
    vec3 delta;
    glm_vec3_scale(demo->right, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  if (demo->keys[GLFW_KEY_W]) {
    vec3 delta;
    glm_vec3_scale(demo->forward, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (demo->keys[GLFW_KEY_S]) {
    vec3 delta;
    glm_vec3_scale(demo->forward, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  if (demo->keys[GLFW_KEY_SPACE]) {
    vec3 delta;
    glm_vec3_scale(demo->up, speed, delta);
    glm_vec3_add(desired_velocity, delta, desired_velocity);
  }
  if (demo->keys[GLFW_KEY_LEFT_SHIFT]) {
    vec3 delta;
    glm_vec3_scale(demo->up, speed, delta);
    glm_vec3_sub(desired_velocity, delta, desired_velocity);
  }
  // printf(LOG_PREFIX " velocity x=%.1f y=%.1f z=%.1f\n", desired_velocity[0], desired_velocity[1], desired_velocity[2]);
  // bool moving_vertical = false;
  // if (keys_.count(GLFW_KEY_SPACE) > 0) {
  //   desired_velocity += player_speed_ * up_;
  //   moving_vertical = true;
  // }
  // if (keys_.count(GLFW_KEY_LEFT_SHIFT) > 0) {
  //   desired_velocity -= player_speed_ * up_;
  //   moving_vertical = true;
  // }
  // if (play_mode_ == PlayMode::kFly || moving_vertical) {
  glm_vec3_mix(demo->velocity, desired_velocity, 0.8f, demo->velocity);
  // } else {
  //   // If there's gravity, we want to keep the up_ component of the velocity but
  //   // slow down the forward_ component
  //   glm::vec3 up_component = glm::dot(player_gaussian_.velocity, up_) * up_;
  //   glm::vec3 forward_component =
  //       glm::dot(player_gaussian_.velocity, forward_) * forward_;
  //   player_gaussian_.velocity =
  //       up_component + 0.8f * desired_velocity + 0.2f * forward_component;
  // }

  // Update position
  vec3 delta_position;
  glm_vec3_scale(demo->velocity, dt, delta_position);
  glm_vec3_add(demo->position, delta_position, demo->position);
  // demo->position += player_gaussian_.velocity * dt;
  // std::cout << "Player position: " << player_gaussian_.position.x << " "
  //           << player_gaussian_.position.y << " " << player_gaussian_.position.z
  //           << " " << std::endl;

  // // Update up_ vector
  // up_ = glm::normalize(player_gaussian_.position);
  // right_ = glm::normalize(glm::cross(forward_, up_));
  // forward_ = glm::normalize(glm::cross(up_, right_));
  // look_ = glm::normalize(glm::rotate(forward_, elevation_, right_));

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

  struct demo demo = {
    .movement_speed = 0.1f,
    .position = {0.0f, 0.0f, -1.0f},
    .up = {0.0f, 1.0f, 0.0f},
    .forward = {0.0f, 0.0f, 1.0f},
    .right = {1.0f, 0.0f, 0.0f},
    .look = {0.0f, 0.0f, 1.0f},
  };

  demo.instance = wgpuCreateInstance(NULL);
  assert(demo.instance);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
    glfwCreateWindow(640, 480, "triangle [wgpu-native + glfw]", NULL, NULL);
  assert(window);

  glfwSetWindowUserPointer(window, (void *)&demo);
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
    demo.surface = wgpuInstanceCreateSurface(
      demo.instance,
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
    demo.surface = wgpuInstanceCreateSurface(
      demo.instance,
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
    demo.surface = wgpuInstanceCreateSurface(
      demo.instance,
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
    demo.surface = wgpuInstanceCreateSurface(
      demo.instance,
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
  assert(demo.surface);

  wgpuInstanceRequestAdapter(
    demo.instance,
    &(const WGPURequestAdapterOptions){
      .compatibleSurface = demo.surface,
    },
    handle_request_adapter, &demo
  );
  assert(demo.adapter);

  wgpuAdapterRequestDevice(demo.adapter, NULL, handle_request_device, &demo);
  assert(demo.device);

  WGPUQueue queue = wgpuDeviceGetQueue(demo.device);
  assert(queue);

  WGPUShaderModule shader_module =
    frmwrk_load_shader_module(demo.device, "shader.wgsl");
  assert(shader_module);

  struct uniforms uniforms = {
    .view = GLM_MAT4_IDENTITY_INIT,
    .projection = GLM_MAT4_IDENTITY_INIT,
  };

  WGPUBuffer uniform_buffer = frmwrk_device_create_buffer_init(
    demo.device,
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
  };
  WGPUBindGroupLayoutDescriptor bgl_desc = {
    .entryCount = sizeof(bgl_entries) / sizeof(bgl_entries[0]),
    .entries = bgl_entries,
  };
  WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(demo.device, &bgl_desc);

  WGPUBindGroupEntry bg_entries[] = {
    [0] = {
      .binding = 0,
      .buffer = uniform_buffer,
      .size = sizeof(uniforms),
    },
  };
  WGPUBindGroupDescriptor bg_desc = {
    .layout = bgl,
    .entryCount = sizeof(bg_entries) / sizeof(bg_entries[0]),
    .entries = bg_entries,
  };
  WGPUBindGroup bg = wgpuDeviceCreateBindGroup(demo.device, &bg_desc);

  static const WGPUVertexAttribute vertex_attributes[] = {
    {
      .format = WGPUVertexFormat_Float32x3,
      .offset = 0,
      .shaderLocation = 0,
    },
    {
      .format = WGPUVertexFormat_Float32x4,
      .offset = 0 + 12,  // 0 + sizeof(Float32x3)
      .shaderLocation = 1,
    },
  };

  ChunkSection section = {
    .x = 0,
    .z = 0,
    .y = 0,
    .data = {0},
  };

  for (int x = 0; x < 16; x += 1) {
    for (int z = 0; z < 16; z += 1) {
      for (int y = 0; y < 16; y += 1) {
        // section.data[x + y * 16 + z * 16 * 16] = y < 8 ? 1 : 0;
        // section.data[x + y * 16 + z * 16 * 16] = (y / 8) ? 1 : 0;
        // section.data[x + y * 16 + z * 16 * 16] = (x + y + z) % 10 == 0 ? 1 : 0;
        section.data[x + y * 16 + z * 16 * 16] = (z / 2 + x / 2) % 2;
      }
    }
  }

  ChunkSection *neighbors[3] = {NULL, NULL, NULL};
  chunk_section_buffer_update_mesh(&section, neighbors, demo.device);

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
    demo.device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "index_buffer",
      .content = (void *)indices,
      .content_size = sizeof(indices),
      .usage = WGPUBufferUsage_Index,
    }
  );

  WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(
    demo.device,
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
  wgpuSurfaceGetCapabilities(demo.surface, demo.adapter, &surface_capabilities);

  WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(
    demo.device,
    &(const WGPURenderPipelineDescriptor){
      .label = "render_pipeline",
      .layout = pipeline_layout,
      .vertex = (const WGPUVertexState){
        .module = shader_module,
        .entryPoint = "vs_main",
        .bufferCount = 1,
        .buffers = (const WGPUVertexBufferLayout[]){
          (const WGPUVertexBufferLayout){
            .arrayStride = 3 * 4 + 4 * 4,
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

  demo.config = (const WGPUSurfaceConfiguration){
    .device = demo.device,
    .usage = WGPUTextureUsage_RenderAttachment,
    .format = surface_capabilities.formats[0],
    .presentMode = WGPUPresentMode_Fifo,
    .alphaMode = surface_capabilities.alphaModes[0],
  };

  demo.depth_texture_descriptor = (WGPUTextureDescriptor){
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
  update_window_size(&demo, width, height);

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
    update_player_position(&demo, (float)deltaTime);

    vec3 center;
    glm_vec3_add(demo.position, demo.look, center);

    mat4 view;
    glm_lookat(demo.position, center, demo.up, view);
    memcpy(&uniforms.view, view, sizeof(view));

    mat4 projection;
    glm_perspective(
      GLM_PI_2, (float)demo.config.width / (float)demo.config.height, 0.01f, 100.0f, projection
    );
    memcpy(&uniforms.projection, projection, sizeof(projection));

    // Send uniforms to GPU
    wgpuQueueWriteBuffer(queue, uniform_buffer, 0, &uniforms, sizeof(uniforms));

    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(demo.surface, &surface_texture);
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
          update_window_size(&demo, width, height);
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

    WGPUTextureView depth_frame = wgpuTextureCreateView(demo.depth_texture, NULL);
    assert(depth_frame);

    WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
      demo.device,
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

    wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, 0, section.vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(render_pass_encoder, index_buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, bg, 0, NULL);
    wgpuRenderPassEncoderSetPipeline(render_pass_encoder, render_pipeline);
    // wgpuRenderPassEncoderDraw(render_pass_encoder, 3, 1, 0, 0);
    wgpuRenderPassEncoderDrawIndexed(render_pass_encoder, section.num_quads * 6, 1, 0, 0, 0);
    wgpuRenderPassEncoderEnd(render_pass_encoder);

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
      command_encoder,
      &(const WGPUCommandBufferDescriptor){
        .label = "command_buffer",
      }
    );
    assert(command_buffer);

    wgpuQueueSubmit(queue, 1, (const WGPUCommandBuffer[]){command_buffer});
    wgpuSurfacePresent(demo.surface);

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
  wgpuDeviceRelease(demo.device);
  wgpuAdapterRelease(demo.adapter);
  wgpuSurfaceRelease(demo.surface);
  wgpuBufferRelease(section.vertex_buffer);
  wgpuBufferRelease(uniform_buffer);
  glfwDestroyWindow(window);
  wgpuInstanceRelease(demo.instance);
  glfwTerminate();
  return 0;
}
