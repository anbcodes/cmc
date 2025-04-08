#pragma once

#include <openssl/types.h>
#include <stdint.h>
#include "../datatypes.h"

#include "config.h"
#include "login.h"
#include "packetTypes.h"
#include "player.h"
#include "misc.h"
#include "chunk.h"

#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

#define MCAPI_SETCB_FUNC(state, type)                                                \
  void mcapi_set_##name##_cb(mcapiConnection *conn, void (*cb)(mcapiConnection *, type)) { \
    conn->name##_cb = cb;                                                                    \
  } \

#define UNPACK(...) __VA_ARGS__

#define MCAPI_HANDLER(state, packet_id, name, type, create_body, destroy_body)                \
    mcapiPacket* create_##name##_packet(ReadableBuffer *p) {                               \
      type* packet = malloc(sizeof(type));\
      UNPACK create_body\
      return (mcapiPacket*)packet;\
    }\
    \
    void destroy_##name##_packet(mcapiPacket* _p) { \
      type *packet = (type *)_p; \
      UNPACK destroy_body \
      free(packet);      \
    }\
  \
  void mcapi_set_##name##_cb(mcapiConnection *conn, void (*cb)(mcapiConnection *, type *)) { \
    conn->state##_cbs[packet_id] = (Callback)cb;                                                    \
    PACKET_FUNCTIONS.state##_create_funcs[packet_id] = create_##name##_packet;                      \
    PACKET_FUNCTIONS.state##_destroy_funcs[packet_id] = destroy_##name##_packet;                    \
  }

#define MCAPI_HANDLER_NO_PAYLOAD(state, packet_id, name) \
  void mcapi_set_##name##_cb(mcapiConnection *conn, void (*cb)(mcapiConnection *, void *)) { \
    conn->state##_cbs[packet_id] = (Callback)cb;                                                    \
  }

#define MCAPI_SETCB_FUNC1(name)                                                      \
  void mcapi_set_##name##_cb(mcapiConnection *conn, void (*cb)(mcapiConnection *)) { \
    conn->name##_cb = cb;                                                            \
  }

// #define MCAPI_HANDLER(state, packet, name, type, ...)\
//   type * create_##name##_packet(ReadableBuffer *p) {\
//     __VA_ARGS__\
//   \
//   void mcapi_set_##name##_cb(mcapiConnection *conn, void (*cb)(mcapiConnection *, packet)) { \
//     conn->state##_cbs[type] = cb;                                                                    \
//   }\
//   PACKET_FUNCTIONS.state##create_funcs[type] = create_##name##_packet; \
//   PACKET_FUNCTIONS.state##destroy_funcs[type] = create_##name##_packet;

#define INTERNAL_BUF_SIZE 1024 * 1024

extern WritableBuffer reusable_buffer;

typedef struct mcapiConnection mcapiConnection;

void send_packet(mcapiConnection *conn, const Buffer packet);


typedef struct mcapiPacket mcapiPacket;

typedef mcapiPacket * (*CreateHandler)(ReadableBuffer *p);
typedef void (*DestroyHandler)(mcapiPacket*);
typedef void (*Callback)(mcapiConnection *, void*);

struct mcapiConnection {
  int sockfd;
  mcapiConnState state;
  int compression_threshold;

  String uuid;
  String access_token;

  bool encryption_enabled;
  Buffer shared_secret;
  EVP_CIPHER_CTX *encrypt_ctx;
  EVP_CIPHER_CTX *decrypt_ctx;

  struct libdeflate_compressor *compressor;
  struct libdeflate_decompressor *decompressor;

  // Callbacks
  Callback login_cbs[MCAPI_LOGIN_CB_MAX_ID];
  Callback config_cbs[MCAPI_CONFIGURATION_CB_MAX_ID];
  Callback play_cbs[MCAPI_PLAY_CB_MAX_ID];

  // Init
  void (*login_success_cb)(mcapiConnection *, mcapiLoginSuccessPacket);
  void (*finish_config_cb)(mcapiConnection *);
  void (*set_block_destroy_stage_cb)(mcapiConnection *, mcapiSetBlockDestroyStagePacket);
  void (*clientbound_known_packs_cb)(mcapiConnection *, mcapiClientboundKnownPacksPacket);
  void (*registry_data_cb)(mcapiConnection *, mcapiRegistryDataPacket);

  // Play
  void (*chunk_and_light_data_cb)(mcapiConnection *, mcapiChunkAndLightDataPacket);
  void (*update_light_cb)(mcapiConnection *, mcapiUpdateLightPacket);
  void (*synchronize_player_position_cb)(mcapiConnection *, mcapiSynchronizePlayerPositionPacket);
  void (*update_time_cb)(mcapiConnection *, mcapiUpdateTimePacket);
  void (*block_update_cb)(mcapiConnection *, mcapiBlockUpdatePacket);
  void (*chunk_batch_finished_cb)(mcapiConnection *, mcapiChunkBatchFinishedPacket);
  void (*clientbound_keepalive_cb)(mcapiConnection *, mcapiClientboundKeepAlivePacket);
};

typedef struct PacketFunctions {
  CreateHandler login_create_funcs[MCAPI_LOGIN_CB_MAX_ID];
  DestroyHandler login_destroy_funcs[MCAPI_LOGIN_CB_MAX_ID];
  CreateHandler config_create_funcs[MCAPI_CONFIGURATION_CB_MAX_ID];
  DestroyHandler config_destroy_funcs[MCAPI_CONFIGURATION_CB_MAX_ID];
  CreateHandler play_create_funcs[MCAPI_PLAY_CB_MAX_ID];
  DestroyHandler play_destroy_funcs[MCAPI_PLAY_CB_MAX_ID];
} PacketFunctions;

extern PacketFunctions PACKET_FUNCTIONS;
