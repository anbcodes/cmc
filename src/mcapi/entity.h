#pragma once

#include "base.h"
#include "../datatypes.h"

// typedef struct mcapiServerboundKnownPacksPacket {
//   int known_pack_count;
//   mcapiKnownPack* packs;
// } mcapiServerboundKnownPacksPacket;

// // WARN: Currently completely ignores the packet data and just sends 0
// void mcapi_send_serverbound_known_packs(mcapiConnection* conn, mcapiServerboundKnownPacksPacket packet);


// ====== Callbacks ======

typedef struct mcapiAddEntityPacket {
  int id;
  UUID uuid;
  int type;
  double x;
  double y;
  double z;
  uint8_t pitch;
  uint8_t yaw;
  uint8_t yaw_head;
  int data; // A number depending on the type
  short vx;
  short vy;
  short vz;
} mcapiAddEntityPacket;

void mcapi_set_add_entity_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiAddEntityPacket *));

typedef struct mcapiUpdateEntityPositionPacket {
  int id;
  short dx;
  short dy;
  short dz;
  bool on_ground;
} mcapiUpdateEntityPositionPacket;

void mcapi_set_update_entity_position_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiUpdateEntityPositionPacket *));

typedef struct mcapiUpdateEntityPositionRotationPacket {
  int id;
  short dx;
  short dy;
  short dz;
  uint8_t pitch;
  uint8_t yaw;
  bool on_ground;
} mcapiUpdateEntityPositionRotationPacket;

void mcapi_set_update_entity_position_rotation_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiUpdateEntityPositionRotationPacket *));

typedef struct mcapiTeleportEntityPacket {
  int id;
  double x;
  double y;
  double z;
  double vx;
  double vy;
  double vz;
  float yaw;
  float pitch;
  bool on_ground;
} mcapiTeleportEntityPacket;

void mcapi_set_teleport_entity_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiTeleportEntityPacket *));

typedef struct mcapiRemoveEntitiesPacket {
  int entity_count;
  int *entity_ids;
} mcapiRemoveEntitiesPacket;

void mcapi_set_remove_entities_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiRemoveEntitiesPacket *));
