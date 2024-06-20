#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum mcapiConnState { MCAPI_STATE_INIT = 0, MCAPI_STATE_STATUS = 1, MCAPI_STATE_LOGIN = 2, MCAPI_STATE_CONFIG = 3, MCAPI_STATE_PLAY = 4 } mcapiConnState;

typedef struct mcapiBuffer {
  uint8_t* ptr;
  size_t len;
} mcapiBuffer;

mcapiBuffer mcapi_create_buffer(size_t len);
void mcapi_destroy_buffer(mcapiBuffer buffer);
void mcapi_print_buf(mcapiBuffer str);

typedef mcapiBuffer mcapiString;

mcapiString mcapi_to_string(char* c_str);
void mcapi_print_str(mcapiString str);

typedef struct mcapiConnection mcapiConnection;

mcapiConnection* mcapi_create_connection(char* hostname, short port);
void mcapi_set_state(mcapiConnection* conn, mcapiConnState state);

typedef struct mcapiUUID {
  uint64_t upper;
  uint64_t lower;
} mcapiUUID;

typedef enum mcapiNBTTagType {
  MCAPI_NBT_END = 0x00,
  MCAPI_NBT_BYTE,
  MCAPI_NBT_SHORT,
  MCAPI_NBT_INT,
  MCAPI_NBT_LONG,
  MCAPI_NBT_FLOAT,
  MCAPI_NBT_DOUBLE,
  MCAPI_NBT_BYTE_ARRAY,
  MCAPI_NBT_STRING,
  MCAPI_NBT_LIST,
  MCAPI_NBT_COMPOUND,
  MCAPI_NBT_INT_ARRAY,
  MCAPI_NBT_LONG_ARRAY,
} mcapiNBTTagType;

typedef struct mcapiNBT mcapiNBT;

typedef struct mcapiNBT {
  mcapiNBTTagType type;
  mcapiString name;

  union {
    uint8_t byte_value;
    uint16_t short_value;
    uint32_t int_value;
    uint64_t long_value;
    float float_value;
    double double_value;
    mcapiBuffer byte_array_value;
    mcapiString string_value;
    struct mcapi_nbt_list {
      int size;
      mcapiNBT* items;
    } list_value;
    struct mcapi_nbt_compound {
      int count;
      mcapiNBT* children;
    } compound_value;
    struct mcapi_nbt_int_array {
      int size;
      int* data;
    } int_array_value;
    struct mcapi_nbt_long_array {
      int size;
      long* data;
    } long_array_value;
  };
} mcapiNBT;

typedef struct mcapiHandshakePacket {
  int protocol_version;  // See protocol version numbers https://wiki.vg/Protocol_version_numbers
  mcapiString server_addr;  // Hostname or IP, e.g. localhost or 127.0.0.1, that was used to connect. The
                      // Notchian server does not use this information. Note that SRV records are a
                      // simple redirect, e.g. if _minecraft._tcp.example.com points to
                      // mc.example.org, users connecting to example.com will provide example.org as
                      // server address in addition to connecting to it.
  short server_port;  // Default is 25565. The Notchian server does not use this information.
  int next_state;     // 1 for Status, 2 for Login.
} mcapiHandshakePacket;

void mcapi_send_handshake(mcapiConnection* conn, mcapiHandshakePacket packet);

typedef struct mcapiLoginStartPacket {
  mcapiString username;  // Player's Username.
  mcapiUUID uuid;       // The UUID of the player logging in. Unused by the Notchian server.
} mcapiLoginStartPacket;

void mcapi_send_login_start(mcapiConnection* conn, mcapiLoginStartPacket packet);

void mcapi_send_login_acknowledged(mcapiConnection* conn);

typedef struct mcapiKnownPack {
  mcapiString namespace;
  mcapiString id;
  mcapiString version;
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

// -- Callbacks --

typedef struct mcapiLoginSuccessProperty {
  mcapiString name;
  mcapiString value;
  bool isSigned;
  mcapiString signature;  // Only if Is Signed is true.
} mcapiLoginSuccessProperty;

typedef struct mcapiLoginSuccessPacket {
  mcapiUUID uuid;
  mcapiString username;
  int number_of_properties;  // Number of elements in the following array.
  mcapiLoginSuccessProperty *properties;
  bool strict_error_handling;  // Whether the client should immediately disconnect upon a packet
                               // processing error. The Notchian client silently ignores them when
                               // this flag is false.
} mcapiLoginSuccessPacket;

void mcapi_set_login_success_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiLoginSuccessPacket));


void mcapi_set_finish_config_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*));

typedef struct mcapiClientboundKnownPacksPacket {
  int known_pack_count;
  mcapiKnownPack *known_packs;
} mcapiClientboundKnownPacksPacket;

void mcapi_set_clientbound_known_packs_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiClientboundKnownPacksPacket));

typedef struct mcapiBitSet {
  int length;
  uint64_t data;
} mcapiBitSet;

typedef struct mcapiBlockEntity {
  uint8_t x;
  uint8_t z;
  short y;
  int type;
  mcapiNBT data;
} mcapiBlockEntity;

typedef struct mcapiChunkSection {
  short block_count;
  int blocks[4096];
  int biomes[64];
} mcapiChunkSection;

typedef struct mcapiChunkAndLightDataPacket {
  int chunk_x;
  int chunk_z;
  mcapiNBT* heightmaps;
  int chunk_section_count;
  mcapiChunkSection* chunk_sections;
  int block_entity_count;
  mcapiBlockEntity* block_entities;
  mcapiBitSet sky_light_mask;
  mcapiBitSet block_light_mask;
  mcapiBitSet empty_sky_light_mask;
  mcapiBitSet empty_block_light_mask;
  int sky_light_array_count;
  uint8_t **sky_light_array;
  int block_light_array_count;
  uint8_t **block_light_array;  
} mcapiChunkAndLightDataPacket;

typedef struct mcapiSynchronizePlayerPositionPacket {
  double x;
  double y;
  double z;
  float yaw;
  float pitch;
  uint8_t flags;
  int teleport_id;
} mcapiSynchronizePlayerPositionPacket;

void mcapi_set_chunk_and_light_data_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiChunkAndLightDataPacket));
void mcapi_set_synchronize_player_position_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiSynchronizePlayerPositionPacket));

void mcapi_poll(mcapiConnection* conn);
