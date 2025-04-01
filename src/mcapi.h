#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "cglm/cglm.h"
#include "datatypes.h"

typedef struct NBT NBT;

typedef enum mcapiConnState { MCAPI_STATE_INIT = 0,
                              MCAPI_STATE_STATUS = 1,
                              MCAPI_STATE_LOGIN = 2,
                              MCAPI_STATE_CONFIG = 3,
                              MCAPI_STATE_PLAY = 4 } mcapiConnState;

typedef struct mcapiConnection mcapiConnection;

mcapiConnection* mcapi_create_connection(char* hostname, short port, char* uuid, char* access_token);
void mcapi_destroy_connection(mcapiConnection* conn);

void mcapi_set_state(mcapiConnection* conn, mcapiConnState state);
mcapiConnState mcapi_get_state(mcapiConnection* conn);

typedef struct mcapiHandshakePacket {
  int protocol_version;  // See protocol version numbers https://wiki.vg/Protocol_version_numbers
  String server_addr;    // Hostname or IP, e.g. localhost or 127.0.0.1, that was used to connect. The
                         // Notchian server does not use this information. Note that SRV records are a
                         // simple redirect, e.g. if _minecraft._tcp.example.com points to
                         // mc.example.org, users connecting to example.com will provide example.org as
                         // server address in addition to connecting to it.
  short server_port;     // Default is 25565. The Notchian server does not use this information.
  int next_state;        // 1 for Status, 2 for Login.
} mcapiHandshakePacket;

void mcapi_send_handshake(mcapiConnection* conn, mcapiHandshakePacket packet);

typedef struct mcapiLoginStartPacket {
  String username;  // Player's Username.
  UUID uuid;        // The UUID of the player logging in. Unused by the Notchian server.
} mcapiLoginStartPacket;

void mcapi_send_login_start(mcapiConnection* conn, mcapiLoginStartPacket packet);

void mcapi_send_login_acknowledged(mcapiConnection* conn);

typedef struct mcapiKnownPack {
  String namespace;
  String id;
  String version;
} mcapiKnownPack;

typedef struct mcapiServerboundKnownPacksPacket {
  int known_pack_count;
  mcapiKnownPack* packs;
} mcapiServerboundKnownPacksPacket;

// WARN: Currently completely ignores the packet data and just sends 0
void mcapi_send_serverbound_known_packs(mcapiConnection* conn, mcapiServerboundKnownPacksPacket packet);

void mcapi_send_acknowledge_finish_config(mcapiConnection* conn);

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


typedef struct mcapiChunkBatchReceivedPacket {
  float chunks_per_tick;   // Desired chunks per tick
} mcapiChunkBatchReceivedPacket;

void mcapi_send_chunk_batch_received(mcapiConnection* conn, mcapiChunkBatchReceivedPacket packet);

typedef struct mcapiPlayPongPacket {
  int id;   // Id from server
} mcapiPlayPongPacket;

void mcapi_send_play_pong(mcapiConnection* conn, mcapiPlayPongPacket packet);

typedef struct mcapiPingRequestPacket {
  long id;   // Id to send server
} mcapiPingRequestPacket;

void mcapi_send_play_ping_request(mcapiConnection* conn, mcapiPingRequestPacket packet);

typedef struct mcapiServerboundKeepalivePacket {
  long id;   // Id that server sent to us
} mcapiServerboundKeepalivePacket;

void mcapi_send_serverbound_keepalive(mcapiConnection* conn, mcapiServerboundKeepalivePacket packet);


// -- Callbacks --

typedef struct mcapiLoginSuccessProperty {
  String name;
  String value;
  bool isSigned;
  String signature;  // Only if Is Signed is true.
} mcapiLoginSuccessProperty;

typedef struct mcapiLoginSuccessPacket {
  UUID uuid;
  String username;
  int number_of_properties;  // Number of elements in the following array.
  mcapiLoginSuccessProperty* properties;
  bool strict_error_handling;  // Whether the client should immediately disconnect upon a packet
                               // processing error. The Notchian client silently ignores them when
                               // this flag is false.
} mcapiLoginSuccessPacket;

void mcapi_set_login_success_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiLoginSuccessPacket));

void mcapi_set_finish_config_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*));

typedef struct mcapiSetBlockDestroyStagePacket {
  int entity_id;
  ivec3 position;
  int8_t stage;  // 0–9 to set it, any other value to remove it.
} mcapiSetBlockDestroyStagePacket;

void mcapi_set_set_block_destroy_stage_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiSetBlockDestroyStagePacket));

typedef struct mcapiClientboundKnownPacksPacket {
  int known_pack_count;
  mcapiKnownPack* known_packs;
} mcapiClientboundKnownPacksPacket;

void mcapi_set_clientbound_known_packs_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiClientboundKnownPacksPacket));

typedef struct mcapiRegistryDataPacket {
  String id;
  int entry_count;
  String* entry_names;
  NBT** entries;
} mcapiRegistryDataPacket;

void mcapi_set_registry_data_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiRegistryDataPacket));

typedef struct mcapiBlockEntity {
  uint8_t x;
  uint8_t z;
  short y;
  int type;
  NBT* data;
} mcapiBlockEntity;

typedef struct mcapiChunkSection {
  short block_count;
  int blocks[4096];
  int biomes[64];
} mcapiChunkSection;

typedef struct mcapiChunkAndLightDataPacket {
  int chunk_x;
  int chunk_z;
  NBT* heightmaps;
  int chunk_section_count;
  mcapiChunkSection* chunk_sections;
  int block_entity_count;
  mcapiBlockEntity* block_entities;
  uint8_t sky_light_array[26][4096];
  uint8_t block_light_array[26][4096];
} mcapiChunkAndLightDataPacket;

void mcapi_set_chunk_and_light_data_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiChunkAndLightDataPacket));

typedef struct mcapiUpdateLightPacket {
  int chunk_x;
  int chunk_z;
  uint8_t sky_light_array[26][4096];
  uint8_t block_light_array[26][4096];
} mcapiUpdateLightPacket;

void mcapi_set_update_light_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiUpdateLightPacket));

typedef struct mcapiBlockUpdatePacket {
  ivec3 position;
  int block_id;  // The new block id
} mcapiBlockUpdatePacket;

void mcapi_set_block_update_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiBlockUpdatePacket));

typedef struct mcapiChunkBatchFinishedPacket {
  int batch_size;  // The number of chunks in the batch
} mcapiChunkBatchFinishedPacket;

void mcapi_set_chunk_batch_finished_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiChunkBatchFinishedPacket));

typedef struct mcapiSynchronizePlayerPositionPacket {
  double x;
  double y;
  double z;
  float yaw;
  float pitch;
  uint8_t flags;
  int teleport_id;
} mcapiSynchronizePlayerPositionPacket;

void mcapi_set_synchronize_player_position_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiSynchronizePlayerPositionPacket));

typedef struct mcapiUpdateTimePacket {
  long world_age;
  long time_of_day;
} mcapiUpdateTimePacket;

void mcapi_set_update_time_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiUpdateTimePacket));

typedef struct mcapiClientboundKeepAlivePacket {
  long keep_alive_id; // Random ID from the server
} mcapiClientboundKeepAlivePacket;

void mcapi_set_clientbound_keepalive_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiClientboundKeepAlivePacket));

void mcapi_poll(mcapiConnection* conn);
