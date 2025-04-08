#pragma once

#include <stdint.h>
#include <cglm/cglm.h>
#include "base.h"

typedef struct mcapiConfirmTeleportationPacket {
  int teleport_id;
} mcapiConfirmTeleportationPacket;

void mcapi_send_confirm_teleportation(mcapiConnection* conn, mcapiConfirmTeleportationPacket packet);

typedef struct mcapiSetPlayerPositionAndRotationPacket {
  double x;
  double y;
  double z;
  float yaw;
  float pitch;
  uint8_t on_ground;
} mcapiSetPlayerPositionAndRotationPacket;

void mcapi_send_set_player_position_and_rotation(mcapiConnection* conn, mcapiSetPlayerPositionAndRotationPacket packet);

typedef enum mcapiPlayerActionStatus {
  MCAPI_ACTION_DIG_START = 0,
  MCAPI_ACTION_DIG_CANCEL = 1,
  MCAPI_ACTION_DIG_FINISH = 2,
  MCAPI_ACTION_DROP_ITEM_STACK = 3,
  MCAPI_ACTION_DROP_ITEM = 4,
  MCAPI_ACTION_SHOOT_ARROW = 5,
  MCAPI_ACTION_FINISH_EATING = 5,  // Same as shoot arrow
  MCAPI_ACTION_SWAP_ITEM = 6,
} mcapiPlayerActionStatus;

typedef enum mcapiBlockFace {
  MCAPI_FACE_BOTTOM = 0,
  MCAPI_FACE_TOP,
  MCAPI_FACE_NORTH,
  MCAPI_FACE_SOUTH,
  MCAPI_FACE_WEST,
  MCAPI_FACE_EAST,
} mcapiBlockFace;

typedef struct mcapiPlayerActionPacket {
  mcapiPlayerActionStatus status;  // The action the player is taking against the block (see below).
  ivec3 position;                  // Block position.
  mcapiBlockFace face;             // The face being hit
  int sequence_num;                // Block change sequence number (see #Acknowledge Block Change).
} mcapiPlayerActionPacket;

// Sent when the player mines a block. A Notchian server only accepts digging packets with coordinates within a 6-unit radius between the center of the block and the player's eyes.
void mcapi_send_player_action(mcapiConnection* conn, mcapiPlayerActionPacket packet);

// ====== Callbacks ======

typedef struct mcapiSetBlockDestroyStagePacket {
  int entity_id;
  ivec3 position;
  int8_t stage;  // 0–9 to set it, any other value to remove it.
} mcapiSetBlockDestroyStagePacket;

void mcapi_set_set_block_destroy_stage_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiSetBlockDestroyStagePacket*));

typedef struct mcapiSynchronizePlayerPositionPacket {
  double x;
  double y;
  double z;
  double vx;
  double vy;
  double vz;
  float yaw;
  float pitch;
  uint8_t flags;
  int teleport_id;
} mcapiSynchronizePlayerPositionPacket;

void mcapi_set_synchronize_player_position_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiSynchronizePlayerPositionPacket*));
