#pragma once

typedef struct Entity {
  int id;
  int type;
  double x;
  double y;
  double z;
  double vx;
  double vy;
  double vz;
  double pitch;
  double yaw;
  double head_yaw;
  bool on_ground;
} Entity;

void entity_destroy(Entity *entity);
