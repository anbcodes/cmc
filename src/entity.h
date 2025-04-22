#pragma once

#include <cglm/cglm.h>
#include <wgpu.h>
#include "texture_sheet.h"

typedef struct EntityInstance {
  vec3 last_pos;
  vec3 pos;
  float last_pos_time;
  float delta_time;
} EntityInstance;

typedef void (*EntityRenderFunction)(EntityInstance* start_instance);

typedef struct EntityInfo {
  char* name;
  int id;
  ivec2 texture_start;
  EntityRenderFunction render;
  int instance_count;
} EntityInfo;

typedef struct Entity {
  int id;
  int index; // Index in world entities array
  int type;
  vec3 last_pos;
  vec3 pos;
  double delta_time;
  double last_pos_time;
  vec3 vel;
  // double x;
  // double y;
  // double z;
  // double vx;
  // double vy;
  // double vz;
  double pitch;
  double yaw;
  double head_yaw;
  bool on_ground;
} Entity;

void entity_move(Entity *entity, vec3 to);
void entity_move_relative(Entity *entity, vec3 delta);
void entity_destroy(Entity *entity);
void entity_update_instance_buffer(Entity *entity, int index, WGPUQueue queue, WGPUBuffer instance_buffer);
void entity_register_entities(EntityInfo entity_info[], EntityTextureSheet* textures);
