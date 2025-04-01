#include "mcapi.h"

#include <assert.h>
#include <curl/curl.h>
#include <openssl/conf.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cglm/cglm.h"
#include "datatypes.h"
#include "libdeflate.h"
#include "nbt.h"
#include "protocol.h"
#include "sockets.h"

#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

/* --- Enums --- */

typedef enum PacketType {
  // Init
  HANDSHAKE = 0,
  STATUS_REQUEST = 0,   // Unimplemented
  STATUS_RESPONSE = 0,  // Unimplemented
  PING_REQUEST = 1,     // Unimplemented
  PING_RESPONSE = 1,    // Unimplemented
  // Login
  LOGIN_DISCONNECT = 0x00,    // Unimplemented
  ENCRYPTION_REQUEST = 0x01,  // Unimplemented
  LOGIN_SUCCESS = 0x02,
  SET_COMPRESSION = 0x03,
  LOGIN_PLUGIN_REQUEST = 0x04,  // Unimplemented
  LOGIN_COOKIE_REQUEST = 0x05,  // Unimplemented

  LOGIN_START = 0x00,
  ENCRYPTION_RESPONSE = 0x01,    // Unimplemented
  LOGIN_PLUGIN_RESPONSE = 0x02,  // Unimplemented
  LOGIN_ACKNOWLEDGED = 0x03,
  COOKIE_RESPONSE = 0x04,  // Unimplemented

  // Config
  // Clientbound
  CONFIG_COOKIE_REQUEST = 0x00,       // Unimplemented
  CLIENTBOUND_PLUGIN_MESSAGE = 0x01,  // Unimplemented
  CONFIG_DISCONNECT = 0x02,           // Unimplemented
  FINISH_CONFIG = 0x03,
  CLIENTBOUND_CONFIG_KEEP_ALIVE = 0x04,  // Unimplemented
  CONFIG_PING = 0x05,                    // Unimplemented
  RESET_CHAT = 0x06,                     // Unimplemented
  REGISTRY_DATA = 0x07,                  // Unimplemented
  REMOVE_RESOURCE_PACK = 0x08,           // Unimplemented
  ADD_RESOURCE_PACK = 0x09,              // Unimplemented
  CONFIG_STORE_COOKIE = 0x0a,            // Unimplemented
  CONFIG_TRANSFER = 0x0b,                // Unimplemented
  FEATURE_FLAGS = 0x0c,                  // Unimplemented
  CONFIG_UPDATE_TAGS = 0x0d,             // Unimplemented
  CLIENTBOUND_KNOWN_PACKS = 0x0e,        // Part implemented

  // Serverbound
  CLIENT_INFORMATION = 0x00,                 // Unimplemented
  CONFIG_COOKIE_RESPONSE = 0x01,             // Unimplemented
  CONFIG_SERVERBOUND_PLUGIN_MESSAGE = 0x02,  // Unimplemented
  ACKNOWLEDGE_FINISH_CONFIG = 0x03,
  CONFIG_SERVERBOUND_KEEP_ALIVE = 0x04,  // Unimplemented
  CONFIG_PONG = 0x05,                    // Unimplemented
  CONFIG_RESOURCE_PACK_RESPONSE = 0x06,  // Unimplemented
  SERVERBOUND_KNOWN_PACKS = 0x07,        // Unimplemented

  // Play

  // Clientbound
  BUNDLE_DELIMITER = 0x00,          // Unimplemented
  SPAWN_ENTITY = 0x01,              // Unimplemented
  SPAWN_EXPERIENCE_ORB = 0x02,      // Unimplemented
  ENTITY_ANIMATION = 0x03,          // Unimplemented
  AWARD_STATISTICS = 0x04,          // Unimplemented
  ACKNOWLEDGE_BLOCK_CHANGE = 0x05,  // Unimplemented
  SET_BLOCK_DESTROY_STAGE = 0x06,
  BLOCK_ENTITY_DATA = 0x07,                    // Unimplemented
  BLOCK_ACTION = 0x08,                         // Unimplemented
  BLOCK_UPDATE = 0x09,                         // Unimplemented
  BOSS_BAR = 0x0a,                             // Unimplemented
  CLIENTBOUND_CHANGE_DIFFICULTY = 0x0b,        // Unimplemented
  CHUNK_BATCH_FINISHED = 0x0c,
  CHUNK_BATCH_START = 0x0d,                    // Unimplemented
  CHUNK_BATCH_BIOMES = 0x0e,                   // Unimplemented
  CLEAR_TITLES = 0x0f,                         // Unimplemented
  COMMAND_SUGGESTION_RESPONSE = 0x10,          // Unimplemented
  COMMANDS = 0x11,                             // Unimplemented
  CLIENTBOUND_CLOSE_CONTAINER = 0x12,          // Unimplemented
  SET_CONTAINER_CONTENT = 0x13,                // Unimplemented
  SET_CONTAINER_PROPERTY = 0x14,               // Unimplemented
  SET_CONTAINER_SLOT = 0x15,                   // Unimplemented
  PLAY_COOKIE_REQUEST = 0x16,                  // Unimplemented
  SET_COOLDOWN = 0x17,                         // Unimplemented
  CHAT_SUGGESTIONS = 0x18,                     // Unimplemented
  PLAY_CLIENTBOUND_PLUGIN_MESSAGE = 0x19,      // Unimplemented
  DAMAGE_EVENT = 0x1a,                         // Unimplemented
  DEBUG_SAMPLE = 0x1b,                         // Unimplemented
  DELETE_MESSAGE = 0x1c,                       // Unimplemented
  DISCONNECT = 0x1d,                           // Unimplemented
  DISGUISED_CHAT_MESSAGE = 0x1e,               // Unimplemented
  ENTITY_EVENT = 0x1f,                         // Unimplemented
  EXPLOSION = 0x20,                            // Unimplemented
  UNLOAD_CHUNK = 0x21,                         // Unimplemented
  GAME_EVENT = 0x22,                           // Unimplemented
  OPEN_HORSE_SCREEN = 0x23,                    // Unimplemented
  HURT_ANIMATION = 0x24,                       // Unimplemented
  INITALIZE_WORLD_BORDER = 0x25,               // Unimplemented
  CLIENTBOUND_KEEPALIVE = 0x26,                // Unimplemented
  CHUNK_DATA_AND_UPDATE_LIGHT = 0x27,          // Unimplemented
  WORLD_EVENT = 0x28,                          // Unimplemented, used for sounds and particles
  PARTICLE = 0x29,                             // Unimplemented
  UPDATE_LIGHT = 0x2a,                         // Unimplemented
  PLAY_LOGIN = 0x2b,                           // Unimplemented
  MAP_DATA = 0x2c,                             // Unimplemented
  MERCHANT_OFFERS = 0x2d,                      // Unimplemented
  UPDATE_ENTITY_POSITION = 0x2e,               // Unimplemented
  UPDATE_ENTITY_POSITION_AND_ROTATION = 0x2f,  // Unimplemented
  UPDATE_ENTITY_ROTATION = 0x30,               // Unimplemented
  CLIENTBOUND_MOVE_VEHICLE = 0x31,             // Unimplemented
  OPEN_BOOK = 0x32,                            // Unimplemented
  OPEN_SCREEN = 0x33,                          // Unimplemented
  OPEN_SIGN_EDITOR = 0x34,                     // Unimplemented
  PLAY_PING = 0x35,                            // Unimplemented
  PLAY_PING_RESPONSE = 0x36,                   // Unimplemented
  PLACE_GHOST_RECIPE = 0x37,                   // Unimplemented
  CLIENTBOUND_PLAYER_ABILITIES = 0x38,         // Unimplemented
  PLAYER_CHAT_MESSAGE = 0x39,                  // Unimplemented
  END_COMBAT = 0x3a,                           // Unimplemented
  ENTER_COMBAT = 0x3b,                         // Unimplemented
  COMBAT_DEATH = 0x3c,                         // Unimplemented
  PLAYER_INFO_REMOVE = 0x3d,                   // Unimplemented
  PLAYER_INFO_UPDATE = 0x3e,                   // Unimplemented
  LOOK_AT = 0x3f,                              // Unimplemented
  SYNCHRONIZE_PLAYER_POSITION = 0x40,          // Unimplemented
  UPDATE_RECIPE_BOOK = 0x41,                   // Unimplemented
  REMOVE_ENTITIES = 0x42,                      // Unimplemented
  REMOVE_ENTITY_EFFECT = 0x43,                 // Unimplemented
  RESET_SCORE = 0x44,                          // Unimplemented
  PLAY_REMOVE_RESOURCE_PACK = 0x45,            // Unimplemented
  PLAY_ADD_RESOURCE_PACK = 0x46,               // Unimplemented
  RESPAWN = 0x47,                              // Unimplemented
  SET_HEAD_ROTATION = 0x48,                    // Unimplemented
  UPDATE_SECTION_BLOCKS = 0x49,                // Unimplemented
  SELECT_ADVANCEMENTS_TAB = 0x4a,              // Unimplemented
  SERVER_DATA = 0x4b,                          // Unimplemented
  SET_ACTION_BAR_TEXT = 0x4c,                  // Unimplemented
  SET_BORDER_CENTER = 0x4d,                    // Unimplemented
  SET_BORDER_LERP_SIZE = 0x4e,                 // Unimplemented
  SET_BORDER_SIZE = 0x4f,                      // Unimplemented
  SET_BORDER_WARNING_DELAY = 0x50,             // Unimplemented
  SET_BORDER_WARNING_DISTANCE = 0x51,          // Unimplemented
  SET_CAMERA = 0x52,                           // Unimplemented
  CLIENTBOUND_SET_HELD_ITEM = 0x53,            // Unimplemented
  SET_CENTER_CHUNK = 0x54,                     // Unimplemented
  SET_RENDER_DISTANCE = 0x55,                  // Unimplemented
  SET_DEFAULT_SPAWN_POSITION = 0x56,           // Unimplemented
  DISPLAY_OBJECTIVE = 0x57,                    // Unimplemented
  SET_ENTITY_METADATA = 0x58,                  // Unimplemented
  LINK_ENTITIES = 0x59,                        // Unimplemented
  SET_ENTITY_VELOCITY = 0x5a,                  // Unimplemented
  SET_EQUIPMENT = 0x5b,                        // Unimplemented
  SET_EXPERIENCE = 0x5c,                       // Unimplemented
  SET_HEALTH = 0x5d,                           // Unimplemented
  UPDATE_OBJECTIVES = 0x5e,                    // Unimplemented
  SET_PASSENGERS = 0x5f,                       // Unimplemented
  UPDATE_TEAMS = 0x60,                         // Unimplemented
  UPDATE_SCORE = 0x61,                         // Unimplemented
  SET_SIMULATION_DISTANCE = 0x62,              // Unimplemented
  SET_SUBTITLE_TEXT = 0x63,                    // Unimplemented
  UPDATE_TIME = 0x64,                          // Unimplemented
  SET_TITLE_TEXT = 0x65,                       // Unimplemented
  SET_TITLE_ANIMATION_TIMES = 0x66,            // Unimplemented
  ENTITY_SOUND_EFFECT = 0x67,                  // Unimplemented
  SOUND_EFFECT = 0x68,                         // Unimplemented
  START_CONFIGURATION = 0x69,                  // Unimplemented
  STOP_SOUND = 0x6a,                           // Unimplemented
  PLAY_STORE_COOKIE = 0x6b,                    // Unimplemented
  SYSTEM_CHAT_MESSAGE = 0x6c,                  // Unimplemented
  SET_TAB_LIST_HEADER_AND_FOOTER = 0x6d,       // Unimplemented
  TAG_QUERY_RESPONSE = 0x6e,                   // Unimplemented
  PICKUP_ITEM = 0x6f,                          // Unimplemented
  TELEPORT_ENTITY = 0x70,                      // Unimplemented
  SET_TICKING_STATE = 0x71,                    // Unimplemented
  STEP_TICK = 0x72,                            // Unimplemented
  PLAY_TRANFER = 0x73,                         // Unimplemented
  UPDATE_ADVANCEMENTS = 0x74,                  // Unimplemented
  UPDATE_ATTRIBUTES = 0x75,                    // Unimplemented
  ENTITY_EFFECTS = 0x76,                       // Unimplemented
  UPDATE_RECIPES = 0x77,                       // Unimplemented
  PLAY_UPDATE_TAGS = 0x78,                     // Unimplemented
  PROJECTILE_POWER = 0x79,                     // Unimplemented

  // Serverbound

  CONFIRM_TELEPORTATION = 0x00,             // Unimplemented
  QUERY_BLOCK_ENTITY_TAG = 0x01,            // Unimplemented
  SERVERBOUND_CHANGE_DIFFICULTY = 0x02,     // Unimplemented
  ACKNOWLEDGE_MESSAGE = 0x03,               // Unimplemented
  CHAT_COMMAND = 0x04,                      // Unimplemented
  SIGNED_CHAT_COMMAND = 0x05,               // Unimplemented
  CHAT_MESSAGE = 0x06,                      // Unimplemented
  PLAYER_SESSION = 0x07,                    // Unimplemented
  CHUNK_BATCH_RECEIVED = 0x08,
  CLIENT_STATUS = 0x09,                     // Unimplemented
  PLAY_CLIENT_INFORMATION = 0x0a,           // Unimplemented
  COMMAND_SUGGESTION_REQUEST = 0x0b,        // Unimplemented
  ACKNOWLEDGE_CONFIGURATION = 0x0c,         // Unimplemented
  CLICK_CONTAINER_BUTTON = 0x0d,            // Unimplemented
  CLICK_CONTAINER = 0x0e,                   // Unimplemented
  SERVERBOUND_CLOSE_CONTAINER = 0x0f,       // Unimplemented
  CHANGE_CONTAINER_SLOT_STATE = 0x10,       // Unimplemented
  PLAY_COOKIE_RESPONSE = 0x11,              // Unimplemented
  PLAY_SERVERBOUND_PLUGIN_MESSAGE = 0x12,   // Unimplemented
  DEBUG_SAMPLE_SUBSCRIPTION = 0x13,         // Unimplemented
  EDIT_BOOK = 0x14,                         // Unimplemented
  QUERY_ENTITY_TAG = 0x15,                  // Unimplemented
  INTERACT = 0x16,                          // Unimplemented
  JIGSAW_GENERATE = 0x17,                   // Unimplemented
  PLAY_SERVERBOUND_KEEP_ALIVE = 0x18,       // Unimplemented
  LOCK_DIFFICULTY = 0x19,                   // Unimplemented
  SET_PLAYER_POSITION = 0x1a,               // Unimplemented
  SET_PLAYER_POSITION_AND_ROTATION = 0x1b,  // Unimplemented
  SET_PLAYER_ROTATION = 0x1c,               // Unimplemented
  SET_PLAYER_ON_GROUND = 0x1d,              // Unimplemented
  SERVERBOUND_MOVE_VEHICLE = 0x1e,          // Unimplemented
  PADDLE_BOAT = 0x1f,                       // Unimplemented
  PICK_ITEM = 0x20,                         // Unimplemented
  PLAY_PING_REQUEST = 0x21,                 // Unimplemented
  PLACE_RECIPE = 0x22,                      // Unimplemented
  SERVERBOUND_PLAYER_ABILITIES = 0x23,      // Unimplemented
  PLAYER_ACTION = 0x24,                     // Unimplemented
  PLAYER_COMMAND = 0x25,                    // Unimplemented
  PLAYER_INPUT = 0x26,                      // Unimplemented
  PLAY_PONG = 0x27,                         // Unimplemented
  CHANGE_RECIPE_BOOK_SETTINGS = 0x28,       // Unimplemented
  SET_SEEN_RECIPE = 0x29,                   // Unimplemented
  RENAME_ITEM = 0x2a,                       // Unimplemented
  PLAY_RESOUCE_PACK_RESPONSE = 0x2b,        // Unimplemented
  SEEN_ADVANCEMENTS = 0x2c,                 // Unimplemented
  SELECT_TRADE = 0x2d,                      // Unimplemented
  SET_BEACON_EFFECT = 0x2e,                 // Unimplemented
  SERVERBOUND_SET_HELD_ITEM = 0x2f,         // Unimplemented
  PROGRAM_COMMAND_BLOCK = 0x30,             // Unimplemented
  PROGRAM_COMMAND_BLOCK_MINECART = 0x31,    // Unimplemented
  SET_CREATIVE_MODE_SLOT = 0x32,            // Unimplemented
  PROGRAM_JIGSAW_BLOCK = 0x33,              // Unimplemented
  PROGRAM_STRUCTURE_BLOCK = 0x34,           // Unimplemented
  UPDATE_SIGN = 0x35,                       // Unimplemented
  SWING_ARM = 0x36,                         // Unimplemented
  TELEPORT_TO_ENTITY = 0x37,                // Unimplemented
  USE_ITEM_ON = 0x38,                       // Unimplemented
  USE_ITEM = 0x39,                          // Unimplemented

} PacketType;

/* --- Macros --- */
#define max(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })

#define min(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })

/* --- Create connection --- */

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
};

mcapiConnection *mcapi_create_connection(char *hostname, short port, char *uuid, char *access_token) {
  char protoname[] = "tcp";
  struct protoent *protoent;
  char *server_hostname = hostname;
  char *user_input = NULL;
  in_addr_t in_addr;
  in_addr_t server_addr;
  int sockfd;
  size_t getline_buffer = 0;
  ssize_t nbytes_read, i, user_input_len;
  struct hostent *hostent;
  /* This is the struct used by INet addresses. */
  struct sockaddr_in sockaddr_in;
  unsigned short server_port = port;

  /* Get socket. */
  protoent = getprotobyname(protoname);
  if (protoent == NULL) {
    perror("getprotobyname");
    exit(1);
  }
  sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
  if (sockfd == -1) {
    perror("socket");
    exit(1);
  }

  /* Prepare sockaddr_in. */
  hostent = gethostbyname(server_hostname);
  if (hostent == NULL) {
    fprintf(stderr, "error: gethostbyname(\"%s\")\n", server_hostname);
    exit(1);
  }
  in_addr = inet_addr(inet_ntoa(*(struct in_addr *)*(hostent->h_addr_list)));
  if (in_addr == (in_addr_t)-1) {
    fprintf(stderr, "error: inet_addr(\"%s\")\n", *(hostent->h_addr_list));
    exit(1);
  }
  sockaddr_in.sin_addr.s_addr = in_addr;
  sockaddr_in.sin_family = AF_INET;
  sockaddr_in.sin_port = htons(server_port);

  /* Do the actual connection. */
  if (connect(sockfd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
    perror("connect");
    return 0;
  }

  set_socket_blocking_enabled(sockfd, false);

  // Initalize openssl
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  // OPENSSL_config(NULL);

  mcapiConnection *conn = calloc(1, sizeof(mcapiConnection));
  conn->access_token = to_string(access_token);
  conn->uuid = to_string(uuid);

  conn->sockfd = sockfd;

  conn->compressor = libdeflate_alloc_compressor(6);
  conn->decompressor = libdeflate_alloc_decompressor();

  return conn;
}

void mcapi_destroy_connection(mcapiConnection *conn) {
  libdeflate_free_compressor(conn->compressor);
  libdeflate_free_decompressor(conn->decompressor);

  close(conn->sockfd);

  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
}

void mcapi_set_state(mcapiConnection *conn, mcapiConnState state) {
  conn->state = state;
}

mcapiConnState mcapi_get_state(mcapiConnection *conn) {
  return conn->state;
}

/* --- Sending Packet Code --- */

void send_packet(mcapiConnection *conn, const Buffer packet) {
  WritableBuffer header_buffer = create_writable_buffer();

  Buffer const *rest_of_packet = NULL;
  Buffer compressed_buf = {};
  Buffer encrypted = {};

  if (conn->compression_threshold > 0) {
    if (packet.len < conn->compression_threshold) {
      write_varint(&header_buffer, packet.len + 1);
      write_varint(&header_buffer, 0);
      rest_of_packet = &packet;
    } else {
      int max_size = libdeflate_zlib_compress_bound(conn->compressor, packet.len);
      compressed_buf = create_buffer(max_size);
      compressed_buf.len = libdeflate_zlib_compress(conn->compressor, packet.ptr, packet.len, compressed_buf.ptr, compressed_buf.len);

      write_varint(&header_buffer, packet.len);
      int total_len = compressed_buf.len + header_buffer.buf.len;
      header_buffer.cursor = header_buffer.buf.len = 0;
      write_varint(&header_buffer, total_len);
      write_varint(&header_buffer, packet.len);

      rest_of_packet = &compressed_buf;
    }
  } else {
    write_varint(&header_buffer, packet.len);
    rest_of_packet = &packet;
  }

  Buffer new_packet = create_buffer(header_buffer.buf.len + rest_of_packet->len);
  memcpy(new_packet.ptr, header_buffer.buf.buffer.ptr, header_buffer.buf.len);
  memcpy(new_packet.ptr + header_buffer.buf.len, rest_of_packet->ptr, rest_of_packet->len);

  if (conn->encryption_enabled) {
    int encrypted_len = 0;
    EVP_CipherUpdate(conn->encrypt_ctx, new_packet.ptr, &encrypted_len, new_packet.ptr, new_packet.len);
    new_packet.len = encrypted_len;
  }

  write(conn->sockfd, new_packet.ptr, new_packet.len);

  destroy_buffer(new_packet);
  destroy_writable_buffer(header_buffer);
  if (compressed_buf.len != 0) destroy_buffer(compressed_buf);
  if (encrypted.len != 0) destroy_buffer(encrypted);
}

#define INTERNAL_BUF_SIZE 1024 * 1024
uint8_t internal_buf[INTERNAL_BUF_SIZE];

WritableBuffer reusable_buffer = {
  .buf = {
    .buffer = {
      .ptr = internal_buf,
      .len = INTERNAL_BUF_SIZE,
    }
  },
  .cursor = 0,
};

void mcapi_send_handshake(mcapiConnection *conn, mcapiHandshakePacket p) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, 0x00);  // Packet ID
  write_varint(&reusable_buffer, p.protocol_version);
  write_string(&reusable_buffer, p.server_addr);
  write_short(&reusable_buffer, p.server_port);
  write_varint(&reusable_buffer, p.next_state);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_login_start(mcapiConnection *conn, mcapiLoginStartPacket p) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, LOGIN_START);  // Packet ID
  write_string(&reusable_buffer, p.username);
  write_uuid(&reusable_buffer, p.uuid);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

typedef struct EncryptionResponsePacket {
  Buffer enc_shared_secret;
  Buffer enc_verify_token;
} EncryptionResponsePacket;

void send_encryption_response_packet(mcapiConnection *conn, EncryptionResponsePacket p) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, ENCRYPTION_RESPONSE);  // Packet ID
  write_varint(&reusable_buffer, p.enc_shared_secret.len);
  write_buffer(&reusable_buffer, p.enc_shared_secret);
  write_varint(&reusable_buffer, p.enc_verify_token.len);
  write_buffer(&reusable_buffer, p.enc_verify_token);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

// Acknowledgement to the Login Success packet sent by the server.
// This packet will switch the connection state to configuration.
void mcapi_send_login_acknowledged(mcapiConnection *conn) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, LOGIN_ACKNOWLEDGED);  // Packet ID
  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_acknowledge_finish_config(mcapiConnection *conn) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, ACKNOWLEDGE_FINISH_CONFIG);  // Packet ID

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_serverbound_known_packs(mcapiConnection *conn, mcapiServerboundKnownPacksPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, SERVERBOUND_KNOWN_PACKS);
  write_varint(&reusable_buffer, 0);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_confirm_teleportation(mcapiConnection *conn, mcapiConfirmTeleportationPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, CONFIRM_TELEPORTATION);
  write_varint(&reusable_buffer, packet.teleport_id);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_set_player_position_and_rotation(mcapiConnection *conn, mcapiSetPlayerPositionAndRotationPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, SET_PLAYER_POSITION_AND_ROTATION);
  write_double(&reusable_buffer, packet.x);
  write_double(&reusable_buffer, packet.y);
  write_double(&reusable_buffer, packet.z);
  write_float(&reusable_buffer, packet.yaw);
  write_float(&reusable_buffer, packet.pitch);
  write_byte(&reusable_buffer, packet.on_ground);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_player_action(mcapiConnection *conn, mcapiPlayerActionPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PLAYER_ACTION);
  write_varint(&reusable_buffer, packet.status);
  write_ipos(&reusable_buffer, packet.position);
  write_byte(&reusable_buffer, packet.face);
  write_varint(&reusable_buffer, packet.sequence_num);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_chunk_batch_received(mcapiConnection* conn, mcapiChunkBatchReceivedPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, CHUNK_BATCH_RECEIVED);
  write_float(&reusable_buffer, packet.chunks_per_tick);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

// Reading packets

// Enables compression. If compression is enabled, all following packets are encoded in the
// compressed packet format. Negative values will disable compression, meaning the packet format
// should remain in the uncompressed packet format. However, this packet is entirely optional, and
// if not sent, compression will also not be enabled (the notchian server does not send the packet
// when compression is disabled).
typedef struct SetCompressionPacket {
  int threshold;  // Maximum size of a packet before it is compressed.
} SetCompressionPacket;

SetCompressionPacket read_set_compression_packet(ReadableBuffer *p) {
  SetCompressionPacket res = {};
  res.threshold = read_varint(p);
  return res;
}

typedef struct EncryptionRequestPacket {
  String serverId;
  Buffer publicKey;
  Buffer verifyToken;
  bool shouldAuthenticate;
} EncryptionRequestPacket;

EncryptionRequestPacket read_encryption_request_packet(ReadableBuffer *p) {
  EncryptionRequestPacket res = {};
  res.serverId = read_string(p);
  int publen = read_varint(p);
  res.publicKey = read_bytes(p, publen);
  int verifylen = read_varint(p);
  res.verifyToken = read_bytes(p, verifylen);
  res.shouldAuthenticate = read_byte(p);
  return res;
}

mcapiLoginSuccessPacket create_login_success_packet(ReadableBuffer *p) {
  mcapiLoginSuccessPacket res = {};
  res.uuid = read_uuid(p);
  res.username = read_string(p);
  res.number_of_properties = read_varint(p);
  res.properties = malloc(sizeof(mcapiLoginSuccessProperty) * res.number_of_properties);

  for (int i = 0; i < res.number_of_properties; i++) {
    res.properties[i] = (mcapiLoginSuccessProperty){
      .name = read_string(p),
      .value = read_string(p),
      .isSigned = read_byte(p),
    };

    if (res.properties[i].isSigned) {
      res.properties[i].signature = read_string(p);
    }
  }

  res.strict_error_handling = read_byte(p);

  return res;
}

void destroy_login_success_packet(mcapiLoginSuccessPacket p) {
  free(p.properties);
}

mcapiSetBlockDestroyStagePacket read_set_block_destroy_stage_packet(ReadableBuffer *p) {
  mcapiSetBlockDestroyStagePacket res = {};
  res.entity_id = read_varint(p);
  read_ipos_into(p, res.position);
  res.stage = read_byte(p);
  return res;
}

mcapiClientboundKnownPacksPacket create_clientbound_known_packs_packet(ReadableBuffer *p) {
  mcapiClientboundKnownPacksPacket res = {};
  res.known_pack_count = read_varint(p);
  res.known_packs = malloc(sizeof(mcapiKnownPack) * res.known_pack_count);
  for (int i = 0; i < res.known_pack_count; i++) {
    res.known_packs[i].namespace = read_string(p);
    res.known_packs[i].id = read_string(p);
    res.known_packs[i].version = read_string(p);
  }

  return res;
}

void destroy_clientbound_known_packs_packet(mcapiClientboundKnownPacksPacket p) {
  // for (int i = 0; i < p.known_pack_count; i++) {
  //   free(p.known_packs[i].namespace.ptr);
  //   free(p.known_packs[i].id.ptr);
  //   free(p.known_packs[i].version.ptr);
  // }
  free(p.known_packs);
}

mcapiRegistryDataPacket create_registry_data_packet(ReadableBuffer *p) {
  mcapiRegistryDataPacket res = {0};
  res.id = read_string(p);
  res.entry_count = read_varint(p);
  res.entry_names = malloc(sizeof(String) * res.entry_count);
  res.entries = malloc(sizeof(NBT *) * res.entry_count);
  for (int i = 0; i < res.entry_count; i++) {
    res.entry_names[i] = read_string(p);
    bool present = read_byte(p);
    if (present) {
      res.entries[i] = read_nbt(p);
    }
  }
  return res;
}

void destroy_registry_data_packet(mcapiRegistryDataPacket p) {
  for (int i = 0; i < p.entry_count; i++) {
    destroy_nbt(p.entries[i]);
  }
  free(p.entry_names);
  free(p.entries);
}

void read_light_data_from_packet(ReadableBuffer *p, uint8_t block_light_array[26][4096], uint8_t sky_light_array[26][4096]) {
  BitSet sky_light_mask = read_bitset(p);
  BitSet block_light_mask = read_bitset(p);
  BitSet empty_sky_light_mask = read_bitset(p);
  BitSet empty_block_light_mask = read_bitset(p);

  int sky_light_array_count = read_varint(p);
  int sky_data_count = 0;
  for (int i = 0; i < 24 + 2; i++) {
    if (bitset_at(sky_light_mask, i)) {
      sky_data_count += 1;
      int length = read_varint(p);
      assert(length == 2048);
      Buffer buffer = read_bytes(p, length);
      // Expand from half-bytes to full-bytes for convenience to the client
      for (int ind = 0; ind < 4096; ind += 1) {
        sky_light_array[i][ind] = buffer.ptr[ind / 2] >> (4 * (ind % 2)) & 0x0F;
      }
    } else if (bitset_at(empty_sky_light_mask, i)) {
      memset(sky_light_array[i], 0x0, 4096);
    } else {
      memset(sky_light_array[i], 0xf, 4096);
    }
  }
  assert(sky_data_count == sky_light_array_count);

  int block_light_array_count = read_varint(p);
  int block_data_count = 0;
  for (int i = 0; i < 24 + 2; i++) {
    if (bitset_at(block_light_mask, i)) {
      block_data_count += 1;
      int length = read_varint(p);
      assert(length == 2048);
      Buffer buffer = read_bytes(p, length);
      // Expand from half-bytes to full-bytes for convenience to the client
      for (int ind = 0; ind < 4096; ind += 1) {
        block_light_array[i][ind] = buffer.ptr[ind / 2] >> (4 * (ind % 2)) & 0x0F;
      }
    } else if (bitset_at(empty_block_light_mask, i)) {
      memset(block_light_array[i], 0x0, 4096);
    } else {
      memset(block_light_array[i], 0xf, 4096);
    }
  }
  assert(block_data_count == block_light_array_count);

  destroy_bitset(sky_light_mask);
  destroy_bitset(block_light_mask);
  destroy_bitset(empty_sky_light_mask);
  destroy_bitset(empty_block_light_mask);
}

mcapiChunkAndLightDataPacket create_chunk_and_light_data_packet(ReadableBuffer *p) {
  mcapiChunkAndLightDataPacket res = {};
  res.chunk_x = read_int(p);
  res.chunk_z = read_int(p);
  printf("Chunk x=%d z=%d\n", res.chunk_x, res.chunk_z);
  res.heightmaps = read_nbt(p);
  int data_len = read_varint(p);

  res.chunk_section_count = 24;
  res.chunk_sections = calloc(24, sizeof(mcapiChunkSection));
  for (int i = 0; i < 24; i++) {
    res.chunk_sections[i].block_count = read_short(p);
    {
      uint8_t bits_per_entry = read_byte(p);
      if (bits_per_entry == 0) {
        int value = read_byte(p);

        for (int j = 0; j < 4096; j++) {
          res.chunk_sections[i].blocks[j] = value;
        }

        read_varint(p);  // Read the length of the data array (always 0)
      } else if (bits_per_entry <= 8) {
        int palette_len = read_varint(p);
        int palette[palette_len];

        for (int j = 0; j < palette_len; j++) {
          palette[j] = read_varint(p);
        }

        int compressed_blocks_len = read_varint(p);
        read_compressed_long_arr(p, bits_per_entry, 4096, compressed_blocks_len, res.chunk_sections[i].blocks);

        for (int j = 0; j < 4096; j++) {
          // if (palette[res.chunk_sections[i].blocks[j]] == 0) {
          res.chunk_sections[i].blocks[j] = palette[res.chunk_sections[i].blocks[j]];
          // }
        }

      } else {
        int compressed_blocks_len = read_varint(p);

        read_compressed_long_arr(p, bits_per_entry, 4096, compressed_blocks_len, res.chunk_sections[i].blocks);
      }
    }

    // Read biomes
    {
      uint8_t bits_per_entry = read_byte(p);
      if (bits_per_entry == 0) {
        int value = read_byte(p);
        for (int j = 0; j < 64; j++) {
          res.chunk_sections[i].biomes[j] = value;
        }
        read_varint(p);  // Read the length of the data array (always 0)
      } else if (bits_per_entry <= 3) {
        int palette_len = read_varint(p);
        int palette[palette_len];

        for (int j = 0; j < palette_len; j++) {
          palette[j] = read_varint(p);
        }

        int compressed_biomes_len = read_varint(p);

        read_compressed_long_arr(p, bits_per_entry, 64, compressed_biomes_len, res.chunk_sections[i].biomes);

        for (int j = 0; j < 64; j++) {
          res.chunk_sections[i].biomes[j] = palette[res.chunk_sections[i].biomes[j]];
        }
      } else {
        int compressed_biomes_len = read_varint(p);
        read_compressed_long_arr(p, bits_per_entry, 64, compressed_biomes_len, res.chunk_sections[i].biomes);
      }
    }
  }

  // Block entities

  res.block_entity_count = read_varint(p);
  res.block_entities = malloc(sizeof(mcapiBlockEntity) * res.block_entity_count);
  for (int i = 0; i < res.block_entity_count; i++) {
    uint8_t xz = read_byte(p);
    res.block_entities[i].x = xz >> 4;
    res.block_entities[i].z = xz & 0x0F;
    res.block_entities[i].y = read_short(p);
    res.block_entities[i].type = read_varint(p);
    res.block_entities[i].data = read_nbt(p);
  }

  // Sky and block lights
  read_light_data_from_packet(p, res.block_light_array, res.sky_light_array);

  return res;
}

void destroy_chunk_and_light_data_packet(mcapiChunkAndLightDataPacket p) {
  destroy_nbt(p.heightmaps);
  free(p.chunk_sections);
  for (int i = 0; i < p.block_entity_count; i++) {
    destroy_nbt(p.block_entities[i].data);
  }
  free(p.block_entities);
}

mcapiUpdateLightPacket read_update_light_packet(ReadableBuffer *p) {
  mcapiUpdateLightPacket res = {};
  res.chunk_x = read_varint(p);
  res.chunk_z = read_varint(p);

  read_light_data_from_packet(p, res.block_light_array, res.sky_light_array);
  return res;
}

mcapiChunkBatchFinishedPacket read_chunk_batch_finished_packet(ReadableBuffer *p) {
  mcapiChunkBatchFinishedPacket res = {};

  res.batch_size = read_varint(p);

  return res;
}

mcapiSynchronizePlayerPositionPacket create_synchronize_player_position_data_packet(ReadableBuffer *p) {
  mcapiSynchronizePlayerPositionPacket res = {};
  res.x = read_double(p);
  res.y = read_double(p);
  res.z = read_double(p);
  res.yaw = read_float(p);
  res.pitch = read_float(p);
  res.flags = read_byte(p);
  res.teleport_id = read_varint(p);
  return res;
}

mcapiUpdateTimePacket create_update_time_packet(ReadableBuffer *p) {
  mcapiUpdateTimePacket res = {};
  res.world_age = read_long(p);
  res.time_of_day = read_long(p);
  return res;
}

mcapiBlockUpdatePacket read_block_update_packet(ReadableBuffer *p) {
  mcapiBlockUpdatePacket res = {};
  read_ipos_into(p, res.position);
  res.block_id = read_varint(p);
  return res;
}

#define mcapi_setcb_func(name, packet)                                                       \
  void mcapi_set_##name##_cb(mcapiConnection *conn, void (*cb)(mcapiConnection *, packet)) { \
    conn->name##_cb = cb;                                                                    \
  }

#define mcapi_setcb_func1(name)                                                      \
  void mcapi_set_##name##_cb(mcapiConnection *conn, void (*cb)(mcapiConnection *)) { \
    conn->name##_cb = cb;                                                            \
  }

mcapi_setcb_func(login_success, mcapiLoginSuccessPacket);
mcapi_setcb_func1(finish_config);
mcapi_setcb_func(set_block_destroy_stage, mcapiSetBlockDestroyStagePacket);
mcapi_setcb_func(clientbound_known_packs, mcapiClientboundKnownPacksPacket);
mcapi_setcb_func(registry_data, mcapiRegistryDataPacket);
mcapi_setcb_func(chunk_and_light_data, mcapiChunkAndLightDataPacket);
mcapi_setcb_func(update_light, mcapiUpdateLightPacket);
mcapi_setcb_func(block_update, mcapiBlockUpdatePacket);
mcapi_setcb_func(synchronize_player_position, mcapiSynchronizePlayerPositionPacket);
mcapi_setcb_func(update_time, mcapiUpdateTimePacket);
mcapi_setcb_func(chunk_batch_finished, mcapiChunkBatchFinishedPacket);

void enable_encryption(mcapiConnection *conn, EncryptionRequestPacket encrypt_req) {
  Buffer shared_secret = create_buffer(16);
  RAND_bytes(shared_secret.ptr, shared_secret.len);

  if (encrypt_req.shouldAuthenticate) {
    // Get hash
    EVP_MD_CTX *mdCtx = EVP_MD_CTX_new();
    Buffer hash = create_buffer(20);
    if (1 != EVP_DigestInit_ex(mdCtx, EVP_sha1(), NULL)) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestUpdate(mdCtx, encrypt_req.serverId.ptr, encrypt_req.serverId.len)) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestUpdate(mdCtx, shared_secret.ptr, shared_secret.len)) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestUpdate(mdCtx, encrypt_req.publicKey.ptr, encrypt_req.publicKey.len)) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestFinal_ex(mdCtx, hash.ptr, NULL)) ERR_print_errors_fp(stderr);
    EVP_MD_CTX_free(mdCtx);

    // Convert hash to string

    char *hex_chars = "0123456789abcdef";

    WritableBuffer hash_as_string = create_writable_buffer();

    bool is_neg = hash.ptr[0] & 0b10000000;
    if (is_neg) {
      write_byte(&hash_as_string, '-');
      write_byte(&hash_as_string, hex_chars[((~hash.ptr[0]) & 0b11110000) >> 4]);
      write_byte(&hash_as_string, hex_chars[((~hash.ptr[0]) & 0b00001111)]);
    }
    for (int i = is_neg ? 1 : 0; i < 20; i++) {
      int v = is_neg ? (~hash.ptr[i]) : hash.ptr[i];
      if (is_neg && i == 19) {
        v++;
      }
      write_byte(&hash_as_string, hex_chars[(v & 0b11110000) >> 4]);
      write_byte(&hash_as_string, hex_chars[v & 0b00001111]);
    }

    destroy_buffer(hash);

    // Authenticate with servers

    WritableBuffer json = create_writable_buffer();
    write_buffer(&json, to_string("{\"accessToken\":\""));
    write_buffer(&json, conn->access_token);
    write_buffer(&json, to_string("\", \"selectedProfile\":\""));
    write_buffer(&json, conn->uuid);
    write_buffer(&json, to_string("\", \"serverId\":\""));
    write_buffer(&json, resizable_buffer_to_buffer(hash_as_string.buf));
    write_buffer(&json, to_string("\"}"));
    write_byte(&json, '\0');  // Allows it to be used like a c str

    destroy_writable_buffer(hash_as_string);

    printf("JSON\n%s\n", json.buf.buffer.ptr);

    CURL *curl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "https://sessionserver.mojang.com/session/minecraft/join");

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.buf.buffer.ptr);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    destroy_writable_buffer(json);
  }

  // Encryption stuff

  // Decode the key
  EVP_PKEY *pkey = NULL;
  OSSL_DECODER_CTX *decoder_ctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, "DER", NULL, "RSA", EVP_PKEY_PUBLIC_KEY, NULL, NULL);
  printf("Got here\n");
  fflush(stdout);
  if (decoder_ctx == NULL) {
    printf("Error\n");
    ERR_print_errors_fp(stderr);
  }
  Buffer pubkey = encrypt_req.publicKey;
  printf("pubkey %p\n", encrypt_req.publicKey.ptr);
  if (1 != OSSL_DECODER_from_data(decoder_ctx, (const unsigned char **)&pubkey.ptr, &pubkey.len)) ERR_print_errors_fp(stderr);

  // Encrypt secret (https://www.openssl.org/docs/man3.0/man3/EVP_PKEY_encrypt.html)
  EVP_PKEY_CTX *pkey_ctx = EVP_PKEY_CTX_new(pkey, NULL);
  if (1 != EVP_PKEY_encrypt_init(pkey_ctx)) ERR_print_errors_fp(stderr);
  if (1 != EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING)) ERR_print_errors_fp(stderr);

  size_t ss_encrypted_len = 0;

  // Get length
  if (1 != EVP_PKEY_encrypt(pkey_ctx, NULL, &ss_encrypted_len, shared_secret.ptr, shared_secret.len)) ERR_print_errors_fp(stderr);

  // Encrypt
  Buffer ss_encrypted = create_buffer(ss_encrypted_len);
  if (1 != EVP_PKEY_encrypt(pkey_ctx, ss_encrypted.ptr, &ss_encrypted.len, shared_secret.ptr, shared_secret.len)) ERR_print_errors_fp(stderr);

  // Encrypt token

  size_t vt_encrypted_len = 0;

  // Get length
  if (1 != EVP_PKEY_encrypt(pkey_ctx, NULL, &vt_encrypted_len, encrypt_req.verifyToken.ptr, encrypt_req.verifyToken.len)) ERR_print_errors_fp(stderr);

  // Encrypt
  Buffer vt_encrypted = create_buffer(vt_encrypted_len);
  if (1 != EVP_PKEY_encrypt(pkey_ctx, vt_encrypted.ptr, &vt_encrypted.len, encrypt_req.verifyToken.ptr, encrypt_req.verifyToken.len)) ERR_print_errors_fp(stderr);

  send_encryption_response_packet(conn, (EncryptionResponsePacket){
                                          .enc_shared_secret = ss_encrypted,
                                          .enc_verify_token = vt_encrypted,
                                        });

  destroy_buffer(ss_encrypted);
  destroy_buffer(vt_encrypted);
  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pkey_ctx);
  OSSL_DECODER_CTX_free(decoder_ctx);

  conn->shared_secret = shared_secret;

  // Set up sym encryption/decryption
  conn->encrypt_ctx = EVP_CIPHER_CTX_new();
  conn->decrypt_ctx = EVP_CIPHER_CTX_new();
  if (1 != EVP_CipherInit_ex2(conn->encrypt_ctx, EVP_aes_128_cfb8(), shared_secret.ptr, shared_secret.ptr, 1, NULL)) ERR_print_errors_fp(stderr);
  if (1 != EVP_CipherInit_ex2(conn->decrypt_ctx, EVP_aes_128_cfb8(), shared_secret.ptr, shared_secret.ptr, 0, NULL)) ERR_print_errors_fp(stderr);

  conn->encryption_enabled = true;
}

#define READABLE_BUF_SIZE 1024 * 8

uint8_t _internal_readable_buffer[READABLE_BUF_SIZE];
ReadableBuffer readable = {
  .cursor = 0,
  .buf = (Buffer){
    .len = 0,
    .ptr = _internal_readable_buffer,
  }
};

void mcapi_poll(mcapiConnection *conn) {
  static ReadableBuffer curr_packet = {};
  static Buffer to_process = {};

  int nbytes_read;

  while ((nbytes_read = read(conn->sockfd, readable.buf.ptr, BUFSIZ)) > 0) {
    readable.buf.len = nbytes_read;
    readable.cursor = 0;

    if (conn->encryption_enabled) {
      // Decrypt
      int decrypted_len = 0;
      if (1 != EVP_CipherUpdate(conn->decrypt_ctx, readable.buf.ptr, &decrypted_len, readable.buf.ptr, readable.buf.len)) {
        ERR_print_errors_fp(stderr);
      }
      readable.buf.len = decrypted_len;
    }

    while (readable.cursor < readable.buf.len) {
      if (curr_packet.buf.len == 0) {
        for (int i = 0; i < nbytes_read; i++) {
          if (has_varint(readable)) {
            int len = read_varint(&readable);
            curr_packet = to_readable_buffer(create_buffer(len));
            break;
          }
        }
      } else if (curr_packet.cursor < curr_packet.buf.len) {
        int nbytes_to_copy = min(readable.buf.len - readable.cursor, curr_packet.buf.len - curr_packet.cursor);
        memcpy(curr_packet.buf.ptr + curr_packet.cursor, readable.buf.ptr + readable.cursor, nbytes_to_copy);

        readable.cursor += nbytes_to_copy;
        curr_packet.cursor += nbytes_to_copy;
      }
      if (curr_packet.cursor == curr_packet.buf.len && curr_packet.buf.len != 0) {
        // We have a finished packet, reset the cursor
        curr_packet.cursor = 0;

        if (conn->compression_threshold > 0) {
          if (curr_packet.buf.ptr[0] == 0) {
            curr_packet.cursor = 1;  // Skip data length byte
          } else {
            int decompressed_length = read_varint(&curr_packet);
            Buffer decompressed = create_buffer(decompressed_length);

            libdeflate_zlib_decompress(conn->decompressor, curr_packet.buf.ptr + curr_packet.cursor, curr_packet.buf.len - curr_packet.cursor, decompressed.ptr, decompressed.len, &decompressed.len);
            destroy_buffer(curr_packet.buf);
            curr_packet.buf = decompressed;
            curr_packet.cursor = 0;
            // perror("No support for compressed packets yet!\n");
            // exit(1);
          }
        }

        // Handle packet

        int type = read_varint(&curr_packet);
        // printf("Handling packet %02x (len %ld)\n", type, curr_packet.buf.len);
        // mcapi_print_buf(curr_packet.buf);
        if (conn->state == MCAPI_STATE_LOGIN) {
          switch (type) {
            case SET_COMPRESSION:
              printf("Enabling compression\n");
              conn->compression_threshold = read_set_compression_packet(&curr_packet).threshold;
              break;
            case ENCRYPTION_REQUEST:
              printf("Enabling encryption\n");
              EncryptionRequestPacket encrypt_req = read_encryption_request_packet(&curr_packet);
              enable_encryption(conn, encrypt_req);

              break;
            case LOGIN_SUCCESS:
              mcapiLoginSuccessPacket packet = create_login_success_packet(&curr_packet);
              if (conn->login_success_cb) (*conn->login_success_cb)(conn, packet);
              destroy_login_success_packet(packet);
              break;
            default:
              printf("Unknown login packet %02x (len %ld)\n", type, curr_packet.buf.len);
              break;
          }
        } else if (conn->state == MCAPI_STATE_CONFIG) {
          switch (type) {
            case FINISH_CONFIG:
              if (conn->finish_config_cb) (*conn->finish_config_cb)(conn);
              break;
            case CLIENTBOUND_KNOWN_PACKS:
              mcapiClientboundKnownPacksPacket packet = create_clientbound_known_packs_packet(&curr_packet);
              if (conn->clientbound_known_packs_cb) (*conn->clientbound_known_packs_cb)(conn, packet);
              destroy_clientbound_known_packs_packet(packet);
              break;
            case REGISTRY_DATA:
              mcapiRegistryDataPacket registry_data_packet = create_registry_data_packet(&curr_packet);
              if (conn->registry_data_cb) (*conn->registry_data_cb)(conn, registry_data_packet);
              destroy_registry_data_packet(registry_data_packet);
              break;
            default:
              printf("Unknown config packet %02x (len %ld)\n", type, curr_packet.buf.len);
              break;
          }

        } else if (conn->state == MCAPI_STATE_PLAY) {
          switch (type) {
            case SET_BLOCK_DESTROY_STAGE:
              mcapiSetBlockDestroyStagePacket sbds_packet = read_set_block_destroy_stage_packet(&curr_packet);
              if (conn->set_block_destroy_stage_cb) (*conn->set_block_destroy_stage_cb)(conn, sbds_packet);
              break;
            case CHUNK_DATA_AND_UPDATE_LIGHT:
              mcapiChunkAndLightDataPacket chunk_packet = create_chunk_and_light_data_packet(&curr_packet);
              if (conn->chunk_and_light_data_cb) (*conn->chunk_and_light_data_cb)(conn, chunk_packet);
              destroy_chunk_and_light_data_packet(chunk_packet);
              break;
            case CHUNK_BATCH_FINISHED:
              mcapiChunkBatchFinishedPacket cbf_packet = read_chunk_batch_finished_packet(&curr_packet);
              if (conn->chunk_batch_finished_cb) (*conn->chunk_batch_finished_cb)(conn, cbf_packet);
              break;
            case UPDATE_LIGHT:
              mcapiUpdateLightPacket light_packet = read_update_light_packet(&curr_packet);
              if (conn->update_light_cb) (*conn->update_light_cb)(conn, light_packet);
              break;
            case BLOCK_UPDATE:
              mcapiBlockUpdatePacket bu_packet = read_block_update_packet(&curr_packet);
              if (conn->block_update_cb) (*conn->block_update_cb)(conn, bu_packet);
              break;
            case SYNCHRONIZE_PLAYER_POSITION:
              mcapiSynchronizePlayerPositionPacket sync_packet = create_synchronize_player_position_data_packet(&curr_packet);
              if (conn->synchronize_player_position_cb) (*conn->synchronize_player_position_cb)(conn, sync_packet);
              break;
            case UPDATE_TIME:
              mcapiUpdateTimePacket time_packet = create_update_time_packet(&curr_packet);
              if (conn->update_time_cb) (*conn->update_time_cb)(conn, time_packet);
            default:
              // printf("Unknown play packet %02x (len %ld)\n", type, curr_packet.buf.len);
              break;
          }
        }

        destroy_buffer(curr_packet.buf);
        curr_packet = (ReadableBuffer){0};
      }
    }
  }
}
