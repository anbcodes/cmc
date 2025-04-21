#pragma once

#include <cglm/cglm.h>
#include <wgpu.h>

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

typedef struct EntityInstance {
  vec3 last_pos;
  vec3 pos;
  float last_pos_time;
  float delta_time;
} EntityInstance;

void entity_move(Entity *entity, vec3 to);
void entity_move_relative(Entity *entity, vec3 delta);
void entity_destroy(Entity *entity);
void entity_update_instance_buffer(Entity *entity, int index, WGPUQueue queue, WGPUBuffer instance_buffer);
