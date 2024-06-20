#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "sockets.h"

#include "mcapi.h"

#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

/* --- Enums --- */

typedef enum PacketType {
  // Init
  HANDSHAKE = 0,
  STATUS_REQUEST = 0, // Unimplemented
  STATUS_RESPONSE = 0, // Unimplemented
  PING_REQUEST = 1, // Unimplemented
  PING_RESPONSE = 1, // Unimplemented
  // Login
  LOGIN_DISCONNECT = 0x00, // Unimplemented
  ENCRYPTION_REQUEST = 0x01, // Unimplemented
  LOGIN_SUCCESS = 0x02,
  SET_COMPRESSION = 0x03,
  LOGIN_PLUGIN_REQUEST = 0x04, // Unimplemented
  LOGIN_COOKIE_REQUEST = 0x05,  // Unimplemented

  LOGIN_START = 0x00,
  ENCRYPTION_RESPONSE = 0x01, // Unimplemented
  LOGIN_PLUGIN_RESPONSE = 0x02, // Unimplemented
  LOGIN_ACKNOWLEDGED = 0x03,
  COOKIE_RESPONSE = 0x04,  // Unimplemented

  // Config
  // Clientbound
  CONFIG_COOKIE_REQUEST = 0x00, // Unimplemented
  CLIENTBOUND_PLUGIN_MESSAGE = 0x01, // Unimplemented
  CONFIG_DISCONNECT = 0x02, // Unimplemented
  FINISH_CONFIG = 0x03,
  CLIENTBOUND_CONFIG_KEEP_ALIVE = 0x04, // Unimplemented
  CONFIG_PING = 0x05, // Unimplemented
  RESET_CHAT = 0x06, // Unimplemented
  REGISTRY_DATA = 0x07, // Unimplemented
  REMOVE_RESOURCE_PACK = 0x08, // Unimplemented
  ADD_RESOURCE_PACK = 0x09, // Unimplemented
  CONFIG_STORE_COOKIE = 0x0a, // Unimplemented
  CONFIG_TRANSFER = 0x0b,  // Unimplemented
  FEATURE_FLAGS = 0x0c, // Unimplemented
  CONFIG_UPDATE_TAGS = 0x0d, // Unimplemented
  CLIENTBOUND_KNOWN_PACKS = 0x0e, // Part implemented

  // Serverbound
  CLIENT_INFORMATION = 0x00, // Unimplemented
  CONFIG_COOKIE_RESPONSE = 0x01, // Unimplemented
  CONFIG_SERVERBOUND_PLUGIN_MESSAGE = 0x02, // Unimplemented
  ACKNOWLEDGE_FINISH_CONFIG = 0x03,
  CONFIG_SERVERBOUND_KEEP_ALIVE = 0x04, // Unimplemented
  CONFIG_PONG = 0x05, // Unimplemented
  CONFIG_RESOURCE_PACK_RESPONSE = 0x06, // Unimplemented
  SERVERBOUND_KNOWN_PACKS = 0x07, // Unimplemented

  // Play

  // Clientbound
  BUNDLE_DELIMITER = 0x00, // Unimplemented
  SPAWN_ENTITY = 0x01, // Unimplemented
  SPAWN_EXPERIENCE_ORB = 0x02, // Unimplemented
  ENTITY_ANIMATION = 0x03, // Unimplemented
  AWARD_STATISTICS = 0x04, // Unimplemented
  ACKNOWLEDGE_BLOCK_CHANGE = 0x05, // Unimplemented
  SET_BLOCK_DESTORY_STAGE = 0x06, // Unimplemented
  BLOCK_ENTITY_DATA = 0x07,// Unimplemented
  BLOCK_ACTION = 0x08, // Unimplemented
  BLOCK_UPDATE = 0x09, // Unimplemented
  BOSS_BAR = 0x0a, // Unimplemented
  CLIENTBOUND_CHANGE_DIFFICULTY = 0x0b, // Unimplemented
  CHUNK_BATCH_FINISHED = 0x0c, // Unimplemented
  CHUNK_BATCH_START = 0x0d, // Unimplemented
  CHUNK_BATCH_BIOMES = 0x0e, // Unimplemented
  CLEAR_TITLES = 0x0f, // Unimplemented
  COMMAND_SUGGESTION_RESPONSE = 0x10, // Unimplemented
  COMMANDS = 0x11, // Unimplemented
  CLIENTBOUND_CLOSE_CONTAINER = 0x12, // Unimplemented
  SET_CONTAINER_CONTENT = 0x13, // Unimplemented
  SET_CONTAINER_PROPERTY = 0x14, // Unimplemented
  SET_CONTAINER_SLOT = 0x15, // Unimplemented
  PLAY_COOKIE_REQUEST = 0x16, // Unimplemented
  SET_COOLDOWN = 0x17, // Unimplemented
  CHAT_SUGGESTIONS = 0x18, // Unimplemented
  PLAY_CLIENTBOUND_PLUGIN_MESSAGE = 0x19, // Unimplemented
  DAMAGE_EVENT = 0x1a, // Unimplemented
  DEBUG_SAMPLE = 0x1b, // Unimplemented
  DELETE_MESSAGE = 0x1c, // Unimplemented
  DISCONNECT = 0x1d, // Unimplemented
  DISGUISED_CHAT_MESSAGE = 0x1e, // Unimplemented
  ENTITY_EVENT = 0x1f, // Unimplemented
  EXPLOSION = 0x20, // Unimplemented
  UNLOAD_CHUNK = 0x21, // Unimplemented
  GAME_EVENT = 0x22, // Unimplemented
  OPEN_HORSE_SCREEN = 0x23, // Unimplemented
  HURT_ANIMATION = 0x24, // Unimplemented
  INITALIZE_WORLD_BORDER = 0x25, // Unimplemented
  CLIENTBOUND_KEEPALIVE = 0x26, // Unimplemented
  CHUNK_DATA_AND_UPDATE_LIGHT = 0x27, // Unimplemented
  WORLD_EVENT = 0x28, // Unimplemented, used for sounds and particles
  PARTICLE = 0x29, // Unimplemented
  UPDATE_LIGHT = 0x2a, // Unimplemented
  PLAY_LOGIN = 0x2b, // Unimplemented
  MAP_DATA = 0x2c, // Unimplemented
  MERCHANT_OFFERS = 0x2d, // Unimplemented
  UPDATE_ENTITY_POSITION = 0x2e, // Unimplemented
  UPDATE_ENTITY_POSITION_AND_ROTATION = 0x2f, // Unimplemented
  UPDATE_ENTITY_ROTATION = 0x30, // Unimplemented
  CLIENTBOUND_MOVE_VEHICLE = 0x31, // Unimplemented
  OPEN_BOOK = 0x32, // Unimplemented
  OPEN_SCREEN = 0x33, // Unimplemented
  OPEN_SIGN_EDITOR = 0x34, // Unimplemented
  PLAY_PING = 0x35, // Unimplemented
  PLAY_PING_RESPONSE = 0x36, // Unimplemented
  PLACE_GHOST_RECIPE = 0x37, // Unimplemented
  CLIENTBOUND_PLAYER_ABILITIES = 0x38, // Unimplemented
  PLAYER_CHAT_MESSAGE = 0x39, // Unimplemented
  END_COMBAT = 0x3a, // Unimplemented
  ENTER_COMBAT = 0x3b, // Unimplemented
  COMBAT_DEATH = 0x3c, // Unimplemented
  PLAYER_INFO_REMOVE = 0x3d, // Unimplemented
  PLAYER_INFO_UPDATE = 0x3e, // Unimplemented
  LOOK_AT = 0x3f, // Unimplemented
  SYNCHRONIZE_PLAYER_POSITION = 0x40, // Unimplemented
  UPDATE_RECIPE_BOOK = 0x41, // Unimplemented
  REMOVE_ENTITIES = 0x42, // Unimplemented
  REMOVE_ENTITY_EFFECT = 0x43, // Unimplemented
  RESET_SCORE = 0x44, // Unimplemented
  PLAY_REMOVE_RESOURCE_PACK = 0x45, // Unimplemented
  PLAY_ADD_RESOURCE_PACK = 0x46, // Unimplemented
  RESPAWN = 0x47, // Unimplemented
  SET_HEAD_ROTATION = 0x48, // Unimplemented
  UPDATE_SECTION_BLOCKS = 0x49, // Unimplemented
  SELECT_ADVANCEMENTS_TAB = 0x4a, // Unimplemented
  SERVER_DATA = 0x4b, // Unimplemented
  SET_ACTION_BAR_TEXT = 0x4c, // Unimplemented
  SET_BORDER_CENTER = 0x4d, // Unimplemented
  SET_BORDER_LERP_SIZE = 0x4e, // Unimplemented
  SET_BORDER_SIZE = 0x4f, // Unimplemented
  SET_BORDER_WARNING_DELAY = 0x50, // Unimplemented
  SET_BORDER_WARNING_DISTANCE = 0x51, // Unimplemented
  SET_CAMERA = 0x52, // Unimplemented
  CLIENTBOUND_SET_HELD_ITEM = 0x53, // Unimplemented
  SET_CENTER_CHUNK = 0x54, // Unimplemented
  SET_RENDER_DISTANCE = 0x55, // Unimplemented
  SET_DEFAULT_SPAWN_POSITION = 0x56, // Unimplemented
  DISPLAY_OBJECTIVE = 0x57, // Unimplemented
  SET_ENTITY_METADATA = 0x58, // Unimplemented
  LINK_ENTITIES = 0x59, // Unimplemented
  SET_ENTITY_VELOCITY = 0x5a, // Unimplemented
  SET_EQUIPMENT = 0x5b, // Unimplemented
  SET_EXPERIENCE = 0x5c, // Unimplemented
  SET_HEALTH = 0x5d, // Unimplemented
  UPDATE_OBJECTIVES = 0x5e, // Unimplemented
  SET_PASSENGERS = 0x5f, // Unimplemented
  UPDATE_TEAMS = 0x60, // Unimplemented
  UPDATE_SCORE = 0x61, // Unimplemented
  SET_SIMULATION_DISTANCE = 0x62, // Unimplemented
  SET_SUBTITLE_TEXT = 0x63, // Unimplemented
  UPDATE_TIME = 0x64, // Unimplemented
  SET_TITLE_TEXT = 0x65, // Unimplemented
  SET_TITLE_ANIMATION_TIMES = 0x66, // Unimplemented
  ENTITY_SOUND_EFFECT = 0x67, // Unimplemented
  SOUND_EFFECT = 0x68, // Unimplemented
  START_CONFIGURATION = 0x69, // Unimplemented
  STOP_SOUND = 0x6a, // Unimplemented
  PLAY_STORE_COOKIE = 0x6b, // Unimplemented
  SYSTEM_CHAT_MESSAGE = 0x6c, // Unimplemented
  SET_TAB_LIST_HEADER_AND_FOOTER = 0x6d, // Unimplemented
  TAG_QUERY_RESPONSE = 0x6e, // Unimplemented
  PICKUP_ITEM = 0x6f, // Unimplemented
  TELEPORT_ENTITY = 0x70, // Unimplemented
  SET_TICKING_STATE = 0x71, // Unimplemented
  STEP_TICK = 0x72, // Unimplemented
  PLAY_TRANFER = 0x73, // Unimplemented
  UPDATE_ADVANCEMENTS = 0x74, // Unimplemented
  UPDATE_ATTRIBUTES = 0x75, // Unimplemented
  ENTITY_EFFECTS = 0x76, // Unimplemented
  UPDATE_RECIPES = 0x77, // Unimplemented
  PLAY_UPDATE_TAGS = 0x78, // Unimplemented
  PROJECTILE_POWER = 0x79, // Unimplemented

  // Serverbound

  CONFIRM_TELEPORTATION = 0x00, // Unimplemented
  QUERY_BLOCK_ENTITY_TAG = 0x01, // Unimplemented
  SERVERBOUND_CHANGE_DIFFICULTY = 0x02, // Unimplemented
  ACKNOWLEDGE_MESSAGE = 0x03, // Unimplemented
  CHAT_COMMAND = 0x04, // Unimplemented
  SIGNED_CHAT_COMMAND = 0x05, // Unimplemented
  CHAT_MESSAGE = 0x06, // Unimplemented
  PLAYER_SESSION = 0x07, // Unimplemented
  CHUNK_BATCH_RECEIVED = 0x08, // Unimplemented
  CLIENT_STATUS = 0x09, // Unimplemented
  PLAY_CLIENT_INFORMATION = 0x0a, // Unimplemented
  COMMAND_SUGGESTION_REQUEST = 0x0b, // Unimplemented
  ACKNOWLEDGE_CONFIGURATION = 0x0c, // Unimplemented
  CLICK_CONTAINER_BUTTON = 0x0d, // Unimplemented
  CLICK_CONTAINER = 0x0e, // Unimplemented
  SERVERBOUND_CLOSE_CONTAINER = 0x0f, // Unimplemented
  CHANGE_CONTAINER_SLOT_STATE = 0x10, // Unimplemented
  PLAY_COOKIE_RESPONSE = 0x11, // Unimplemented
  PLAY_SERVERBOUND_PLUGIN_MESSAGE = 0x12, // Unimplemented
  DEBUG_SAMPLE_SUBSCRIPTION = 0x13, // Unimplemented
  EDIT_BOOK = 0x14, // Unimplemented
  QUERY_ENTITY_TAG = 0x15, // Unimplemented
  INTERACT = 0x16, // Unimplemented
  JIGSAW_GENERATE = 0x17, // Unimplemented
  PLAY_SERVERBOUND_KEEP_ALIVE = 0x18, // Unimplemented
  LOCK_DIFFICULTY = 0x19, // Unimplemented
  SET_PLAYER_POSITION = 0x1a, // Unimplemented
  SET_PLAYER_POSITION_AND_ROTATION = 0x1b, // Unimplemented
  SET_PLAYER_ROTATION = 0x1c, // Unimplemented
  SET_PLAYER_ON_GROUND = 0x1d, // Unimplemented
  SERVERBOUND_MOVE_VEHICLE = 0x1e, // Unimplemented
  PADDLE_BOAT = 0x1f, // Unimplemented
  PICK_ITEM = 0x20, // Unimplemented
  PLAY_PING_REQUEST = 0x21, // Unimplemented
  PLACE_RECIPE = 0x22, // Unimplemented
  SERVERBOUND_PLAYER_ABILITIES = 0x23, // Unimplemented
  PLAYER_ACTION = 0x24, // Unimplemented
  PLAYER_COMMAND = 0x25, // Unimplemented
  PLAYER_INPUT = 0x26, // Unimplemented
  PLAY_PONG = 0x27, // Unimplemented
  CHANGE_RECIPE_BOOK_SETTINGS = 0x28, // Unimplemented
  SET_SEEN_RECIPE = 0x29, // Unimplemented
  RENAME_ITEM = 0x2a, // Unimplemented
  PLAY_RESOUCE_PACK_RESPONSE = 0x2b, // Unimplemented
  SEEN_ADVANCEMENTS = 0x2c, // Unimplemented
  SELECT_TRADE = 0x2d, // Unimplemented
  SET_BEACON_EFFECT = 0x2e, // Unimplemented
  SERVERBOUND_SET_HELD_ITEM = 0x2f, // Unimplemented
  PROGRAM_COMMAND_BLOCK = 0x30, // Unimplemented
  PROGRAM_COMMAND_BLOCK_MINECART = 0x31, // Unimplemented
  SET_CREATIVE_MODE_SLOT = 0x32, // Unimplemented
  PROGRAM_JIGSAW_BLOCK = 0x33, // Unimplemented
  PROGRAM_STRUCTURE_BLOCK = 0x34, // Unimplemented
  UPDATE_SIGN = 0x35, // Unimplemented
  SWING_ARM = 0x36, // Unimplemented
  TELEPORT_TO_ENTITY = 0x37, // Unimplemented
  USE_ITEM_ON = 0x38, // Unimplemented
  USE_ITEM = 0x39, // Unimplemented



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

  // Callbacks

  // Init
  void (*login_success_cb)(mcapiConnection*, mcapiLoginSuccessPacket);
  void (*finish_config_cb)(mcapiConnection*);
  void (*clientbound_known_packs_cb)(mcapiConnection*, mcapiClientboundKnownPacksPacket);
  void (*chunk_and_light_data_cb)(mcapiConnection*, mcapiChunkAndLightDataPacket);
  void (*synchronize_player_position_cb)(mcapiConnection*, mcapiSynchronizePlayerPositionPacket);

  // Play
};

mcapiConnection* mcapi_create_connection(char* hostname, short port) {
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

  mcapiConnection* conn = calloc(1, sizeof(mcapiConnection));

  conn->sockfd = sockfd;

  return conn;
}

void mcapi_set_state(mcapiConnection* conn, mcapiConnState state) {
  conn->state = state;
};

/* --- Buffer --- */

mcapiBuffer mcapi_create_buffer(size_t len) {
  return (mcapiBuffer){
    .ptr = malloc(len),
    .len = len,
  };
}

void mcapi_destroy_buffer(const mcapiBuffer buffer) {
  free(buffer.ptr);
}

void mcapi_print_buf(mcapiBuffer buf) {
  for (int i = 0; i < buf.len; i++) {
    printf("%x ", buf.ptr[i]);
  }

  printf("\n");
}

mcapiString mcapi_to_string(char* c_str) {
  return (mcapiString){
    .ptr = c_str,
    .len = strlen(c_str), 
  };
}

void mcapi_print_str(mcapiString str) {
  fflush(stdout);
  write(STDOUT_FILENO, str.ptr, str.len);
}

typedef struct ResizeableBuffer {
    mcapiBuffer buffer;
    size_t len;
} ResizeableBuffer;

ResizeableBuffer create_resizeable_buffer() {
  return (ResizeableBuffer){
    .buffer = mcapi_create_buffer(16),
    .len = 0,
  };
};

void destroy_resizeable_buffer(const ResizeableBuffer buffer) {
  mcapi_destroy_buffer(buffer.buffer);
}

void resizeable_buffer_ensure_capacity(ResizeableBuffer *buf, int capacity) {
  if (buf->buffer.len < capacity) {
    // Double the size of the buffer
    mcapiBuffer old_buf = buf->buffer;
    int oldlen = buf->buffer.len;

    buf->buffer = mcapi_create_buffer(oldlen * 2);
    memcpy(buf->buffer.ptr, old_buf.ptr, oldlen);

    mcapi_destroy_buffer(old_buf);
  }
}

mcapiBuffer resizable_buffer_to_buffer(ResizeableBuffer resizeable) {
  return (mcapiBuffer){
    .ptr = resizeable.buffer.ptr,
    .len = resizeable.len,
  };
}

typedef struct ReadableBuffer {
  mcapiBuffer buf;
  int cursor; // Location of next read
} ReadableBuffer;

typedef struct WritableBuffer {
  ResizeableBuffer buf;
  int cursor; // Location of next write
} WritableBuffer;

ReadableBuffer to_readable_buffer(mcapiBuffer buf) {
  return (ReadableBuffer){
    .buf = buf,
    .cursor = 0,
  };
}

WritableBuffer create_writable_buffer() {
  return (WritableBuffer){
    .buf = create_resizeable_buffer(),
    .cursor = 0,
  };
}

void destroy_writable_buffer(const WritableBuffer buffer) {
  destroy_resizeable_buffer(buffer.buf);
}

/* --- Packet Reader/Writer Code --- */

int SEGMENT_BITS = 0x7F;
int CONTINUE_BIT = 0x80;

void write_byte(WritableBuffer *io, uint8_t value) {
  resizeable_buffer_ensure_capacity(&io->buf, io->cursor + 1);

  io->buf.buffer.ptr[io->cursor] = value;

  io->cursor++;

  if (io->cursor > io->buf.len) io->buf.len = io->cursor;
}

void write_bytes(WritableBuffer *io, void *src, int len) {
  resizeable_buffer_ensure_capacity(&io->buf, io->cursor + len);

  memcpy(io->buf.buffer.ptr, src, len);

  io->cursor += len;
  if (io->cursor > io->buf.len) io->buf.len = io->cursor;
}

void write_short(WritableBuffer *io, uint16_t value) {
  write_byte(io, value >> 8);
  write_byte(io, value);
}

// From https://github.com/bolderflight/leb128/blob/main/src/leb128.h
void write_varint(WritableBuffer *io, int value) {
  // bool negative = (val < 0);
  // size_t i = 0;
  // while (1) {
  //   uint8_t b = val & 0x7F;
  //   /* Ensure an arithmetic shift */
  //   val >>= 7;
  //   if (negative) {
  //     val |= (~0ULL << 57);
  //   }
  //   if (((val == 0) && (!(b & 0x40))) ||
  //       ((val == -1) && (b & 0x40))) {
  //     write_byte(io, b);
  //     return i;
  //   } else {
  //     write_byte(io, b | 0x80);
  //   }
  // }

  while (true) {
    if ((value & ~SEGMENT_BITS) == 0) {
      write_byte(io, value);
      return;
    }

    write_byte(io, (value & SEGMENT_BITS) | CONTINUE_BIT);

    // Note: >>> means that the sign bit is shifted with the rest of the number rather than being
    // left alone
    value = (int)(((unsigned int)value) >> 7);
  }
}

void write_varlong(WritableBuffer *io, long value) {
  while (true) {
    if ((value & ~((long)SEGMENT_BITS)) == 0) {
      write_byte(io, value);
      return;
    }

    write_byte(io, (value & SEGMENT_BITS) | CONTINUE_BIT);

    // Note: >>> means that the sign bit is shifted with the rest of the number rather than being
    // left alone
    value = (long)(((unsigned long)value) >> 7);
  }
}

void write_string(WritableBuffer *io, mcapiString string) {
  write_varint(io, string.len);
  // TODO: Speed up by checking whole string length and adding it all at once
  for (int i = 0; i < string.len; i++) {
    write_byte(io, string.ptr[i]);
  }
}

void write_uuid(WritableBuffer *io, mcapiUUID uuid) {
  write_byte(io, uuid.upper >> (8 * 7));
  write_byte(io, uuid.upper >> (8 * 6));
  write_byte(io, uuid.upper >> (8 * 5));
  write_byte(io, uuid.upper >> (8 * 4));
  write_byte(io, uuid.upper >> (8 * 3));
  write_byte(io, uuid.upper >> (8 * 2));
  write_byte(io, uuid.upper >> (8 * 1));
  write_byte(io, uuid.upper >> (8 * 0));

  write_byte(io, uuid.lower >> (8 * 7));
  write_byte(io, uuid.lower >> (8 * 6));
  write_byte(io, uuid.lower >> (8 * 5));
  write_byte(io, uuid.lower >> (8 * 4));
  write_byte(io, uuid.lower >> (8 * 3));
  write_byte(io, uuid.lower >> (8 * 2));
  write_byte(io, uuid.lower >> (8 * 1));
  write_byte(io, uuid.lower >> (8 * 0));
}

// Does not do bounds checking
uint8_t read_byte(ReadableBuffer *io) {
  // printf("rb: len=%d, cursor=%d\n", io->buf.len, io->cursor);
  uint8_t byte = io->buf.ptr[io->cursor];
  io->cursor++;
  return byte;
}

mcapiBuffer read_bytes(ReadableBuffer *io, size_t size) {
  printf("rbs: len=%d, cursor=%d, count=%d\n", io->buf.len, io->cursor, size);
  mcapiBuffer buf = {
    .ptr = io->buf.ptr+io->cursor,
    .len = size,
  };

  io->cursor += size;
  return buf;
}

bool has_byte(const ReadableBuffer io) {
  return io.cursor < io.buf.len;
}

short read_short(ReadableBuffer *io) {
  short num = 0;
  num += (uint16_t)read_byte(io) << 8 * 1;
  num += (uint16_t)read_byte(io);
  return num;
}

uint16_t read_ushort(ReadableBuffer *io) {
  uint16_t num = 0;
  num += (uint16_t)read_byte(io) << 8 * 1;
  num += (uint16_t)read_byte(io);
  return num;
}

int read_int(ReadableBuffer *io) {
  int num = 0;
  num += (uint32_t)read_byte(io) << 8 * 3;
  num += (uint32_t)read_byte(io) << 8 * 2;
  num += (uint32_t)read_byte(io) << 8 * 1;
  num += (uint32_t)read_byte(io);
  return num;
}

int64_t read_long(ReadableBuffer *io) {
  int64_t num = 0;

  num += (uint64_t)read_byte(io) << 8 * 7;
  num += (uint64_t)read_byte(io) << 8 * 6;
  num += (uint64_t)read_byte(io) << 8 * 5;
  num += (uint64_t)read_byte(io) << 8 * 4;
  num += (uint64_t)read_byte(io) << 8 * 3;
  num += (uint64_t)read_byte(io) << 8 * 2;
  num += (uint64_t)read_byte(io) << 8 * 1;
  num += (uint64_t)read_byte(io);
  return num;
}

float read_float(ReadableBuffer *io) {
  int value = read_int(io);
  return *(float*)(&value);
}

double read_double(ReadableBuffer *io) {
  long value = read_long(io);
  return *(double*)(&value);
}

// Does not do bounds checking
// From https://github.com/bolderflight/leb128/blob/main/src/leb128.h
int read_varint(ReadableBuffer *io) {
  // int res = 0;
  // size_t shift = 0;

  // while (1) {
  //   uint8_t b = read_byte(io);
  //   int slice = b & 0x7F;
  //   res |= slice << shift;
  //   shift += 7;
  //   if (!(b & 0x80)) {
  //     if ((shift < 32) && (b & 0x40)) {
  //       return res | (-1ULL) << shift;
  //     }
  //     return res;
  //   }
  // }

  int value = 0;
  int position = 0;
  uint8_t current_byte;

  while (true) {
    current_byte = read_byte(io);
    value |= (current_byte & SEGMENT_BITS) << position;

    if ((current_byte & CONTINUE_BIT) == 0) break;

    position += 7;

    if (position >= 32) break;  // Too big
  }

  return value;
}

bool has_varint(ReadableBuffer io) {
  int position = 0;
  uint8_t current_byte;

  while (has_byte(io)) {
    current_byte = read_byte(&io);

    if ((current_byte & CONTINUE_BIT) == 0) return true;

    position += 7;

    if (position >= 32) break;  // Too big
  }

  return false;
}

long read_varlong(ReadableBuffer *io) {
  long value = 0;
  int position = 0;
  uint8_t current_byte;

  while (true) {
    current_byte = read_byte(io);
    value |= (long)(current_byte & SEGMENT_BITS) << position;

    if ((current_byte & CONTINUE_BIT) == 0) break;

    position += 7;

    if (position >= 64) break;  // Too big
  }

  return value;
}

bool has_varlong(ReadableBuffer io) {
  int position = 0;
  uint8_t current_byte;

  while (has_byte(io)) {
    current_byte = read_byte(&io);

    if ((current_byte & CONTINUE_BIT) == 0) return true;

    position += 7;

    if (position >= 64) break;  // Too big
  }

  return false;
}

mcapiString read_string(ReadableBuffer *io) {
  int len = read_varint(io);

  mcapiString res = {
    .len = len,
    .ptr = io->buf.ptr + io->cursor,
  };

  printf("rs: len=%d, cursor=%d, slen=%d\n", io->buf.len, io->cursor, len);
  io->cursor += len;

  return res;
}

mcapiUUID read_uuid(ReadableBuffer *io) {
  mcapiUUID uuid = {};
  uuid.upper += (u_int64_t)read_byte(io) << 8 * 7;
  uuid.upper += (u_int64_t)read_byte(io) << 8 * 6;
  uuid.upper += (u_int64_t)read_byte(io) << 8 * 5;
  uuid.upper += (u_int64_t)read_byte(io) << 8 * 4;
  uuid.upper += (u_int64_t)read_byte(io) << 8 * 3;
  uuid.upper += (u_int64_t)read_byte(io) << 8 * 2;
  uuid.upper += (u_int64_t)read_byte(io) << 8 * 1;
  uuid.upper += (u_int64_t)read_byte(io);

  uuid.lower += (u_int64_t)read_byte(io) << 8 * 7;
  uuid.lower += (u_int64_t)read_byte(io) << 8 * 6;
  uuid.lower += (u_int64_t)read_byte(io) << 8 * 5;
  uuid.lower += (u_int64_t)read_byte(io) << 8 * 4;
  uuid.lower += (u_int64_t)read_byte(io) << 8 * 3;
  uuid.lower += (u_int64_t)read_byte(io) << 8 * 2;
  uuid.lower += (u_int64_t)read_byte(io) << 8 * 1;
  uuid.lower += (u_int64_t)read_byte(io);

  return uuid;
}

void read_compressed_long_arr(ReadableBuffer* p, int bits_per_entry, int entries, int to[]) {
  int ind = 0;
  for (int i = 0;;i++) {
    long value = read_long(p);
    uint64_t cur = ntohll(value);
    // printf("comlong i=%d, bpe=%d, total_entries=%d, curr_entry=%d\n", i, bits_per_entry, entries, ind);
    for (int j = 0; j < 64 / bits_per_entry; j++) {
      int block = cur & ((1 << bits_per_entry) - 1);
      cur = cur >> bits_per_entry;
      to[ind] = block;
      ind++;
      if (ind == entries) {
        return;
      }
    }
  }
}

size_t nbt_reader(void* _p, uint8_t* data, size_t size) {
  ReadableBuffer* p = _p;


  int to_read = min(size, p->buf.len - p->cursor);

  printf("c: cursor=%d, buflen=%ld, to_read=%d, reqsize=%d\n", p->cursor, p->buf.len, to_read, size);

  for (int i = 0; i < to_read; i++) {
    data[i] = read_byte(p);
  }

  return to_read;
}

mcapiString read_nbt_string(ReadableBuffer *p) {
  mcapiString res = {
    .len = read_ushort(p),
  };

  res.ptr = p->buf.ptr + p->cursor;

  printf("rns: len=%d, cursor=%d, slen=%d\n", p->buf.len, p->cursor, res.len);

  p->cursor += res.len;

  return res;
}

void read_nbt_into(ReadableBuffer* p, mcapiNBT* nbt);

void read_nbt_value(ReadableBuffer* p, mcapiNBT* nbt, mcapiNBTTagType type) {
  nbt->type = type;

  int size; // used in some branches

  switch (type) {
    case MCAPI_NBT_BYTE:
      nbt->byte_value = read_byte(p);
      break;
    case MCAPI_NBT_SHORT:
      nbt->short_value = read_short(p);
      break;
    case MCAPI_NBT_INT:
      nbt->int_value = read_int(p);
      break;
    case MCAPI_NBT_LONG:
      nbt->long_value = read_long(p);
      break;
    case MCAPI_NBT_FLOAT:
      nbt->float_value = read_float(p);
      break;
    case MCAPI_NBT_DOUBLE:
      nbt->double_value = read_double(p);
      break;
    case MCAPI_NBT_BYTE_ARRAY:
      size = read_int(p);
      printf("byte_array_size %d\n", size);
      nbt->byte_array_value = read_bytes(p, size);
      break;
    case MCAPI_NBT_STRING:
      nbt->string_value = read_nbt_string(p);
      break;
    case MCAPI_NBT_LIST:
      mcapiNBTTagType list_type = read_byte(p);
      size = nbt->list_value.size = read_int(p);
      printf("List tag len=%d\n", size);
      nbt->list_value.items = calloc(nbt->list_value.size, sizeof(mcapiNBT));
      for (int i = 0; i < size; i++) {
        nbt->list_value.items[i].type = list_type;
        read_nbt_value(p, nbt->list_value.items + i, list_type);
      }
      break;
    case MCAPI_NBT_COMPOUND:
      int curr_buflen = 4;
      nbt->compound_value.children = calloc(curr_buflen, sizeof(mcapiNBT));
      for (int i = 0;;i++) {
        printf("Compound index %d\n", i);
        printf("Here 1 cursor=%d len=%d\n", p->cursor, p->buf.len);
        if (i >= curr_buflen) {
          int old_buflen = curr_buflen;
          curr_buflen *= 2;
          mcapiNBT* old = nbt->compound_value.children;
          nbt->compound_value.children = calloc(curr_buflen, sizeof(mcapiNBT));
          memcpy(nbt->compound_value.children, old, old_buflen * sizeof(mcapiNBT));
          free(old);
        }
        read_nbt_into(p, nbt->compound_value.children + i);
        if (nbt->compound_value.children[i].type == MCAPI_NBT_END) {
          nbt->compound_value.count = i + 1;
          break;
        }
      }
      break;
    case MCAPI_NBT_INT_ARRAY:
      size = nbt->int_array_value.size = read_int(p);
      printf("int_array_size %d\n", size);
      nbt->int_array_value.data = malloc(sizeof(int)*size);
      for (int i = 0; i < size; i++) {
        nbt->int_array_value.data[i] = read_int(p);
      }
      break;
    case MCAPI_NBT_LONG_ARRAY:
      size = nbt->long_array_value.size = read_int(p);
      printf("long_array_size %d\n", size);
      nbt->long_array_value.data = malloc(sizeof(int64_t)*size);
      for (int i = 0; i < size; i++) {
        nbt->long_array_value.data[i] = read_long(p);
      }
      break;
  }
}

void read_nbt_into(ReadableBuffer* p, mcapiNBT* nbt) {
  mcapiNBTTagType type = read_byte(p);
  printf("NBT_TYPE %d\n", type);
  nbt->type = type;

  if (type == MCAPI_NBT_END) {
    return;
  }

  // All other tags are named
  nbt->name = read_nbt_string(p);
  read_nbt_value(p, nbt, type);
}

mcapiNBT* read_nbt(ReadableBuffer* p) {
  mcapiNBT* nbt = calloc(1, sizeof(mcapiNBT));

  int type = read_byte(p);
  printf("first type %d\n", type);

  read_nbt_value(p, nbt, type);

  return nbt;
}

/* --- Sending Packet Code --- */

void send_packet(mcapiConnection *conn, const mcapiBuffer packet) {
  WritableBuffer header_buffer = create_writable_buffer();

  printf("Sending packet %x (compthresh %d, len %ld)\n", packet.ptr[0], conn->compression_threshold, packet.len);

  if (conn->compression_threshold > 0) {
    write_varint(&header_buffer, packet.len + 1);

    if (packet.len < conn->compression_threshold) {
      printf("Sending uncompressed\n");
      write_varint(&header_buffer, 0);
    } else {
      perror("No support for compressed packets!");
      exit(1);
    }

  } else {
    write_varint(&header_buffer, packet.len);
  }

  write(conn->sockfd, header_buffer.buf.buffer.ptr, header_buffer.buf.len);
  write(conn->sockfd, packet.ptr, packet.len);

  destroy_writable_buffer(header_buffer);
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

void mcapi_send_handshake(mcapiConnection* conn, mcapiHandshakePacket p) {
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

// Acknowledgement to the Login Success packet sent by the server.
// This packet will switch the connection state to configuration.
void mcapi_send_login_acknowledged(mcapiConnection *conn) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, LOGIN_ACKNOWLEDGED);  // Packet ID
  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_acknowledge_finish_config(mcapiConnection* conn) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, ACKNOWLEDGE_FINISH_CONFIG);  // Packet ID
  
  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_serverbound_known_packs(mcapiConnection* conn, mcapiServerboundKnownPacksPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, SERVERBOUND_KNOWN_PACKS);
  write_varint(&reusable_buffer, 0);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_confirm_teleportation(mcapiConnection* conn, mcapiConfirmTeleportationPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, CONFIRM_TELEPORTATION);
  write_varint(&reusable_buffer, packet.teleport_id);

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

void destory_login_success_packet(mcapiLoginSuccessPacket p) {
  free(p.properties);
}

mcapiClientboundKnownPacksPacket create_clientbound_known_packs_packet(ReadableBuffer *p) {
  mcapiClientboundKnownPacksPacket res = {};
  res.known_pack_count = read_varint(p);
  res.known_packs = malloc(sizeof(mcapiKnownPack)*res.known_pack_count);
  for (int i = 0; i < res.known_pack_count; i++) {
    res.known_packs[i].namespace = read_string(p);
    res.known_packs[i].id = read_string(p);
    res.known_packs[i].version = read_string(p);
  }

  return res;
};


void destroy_clientbound_known_packs_packet(mcapiClientboundKnownPacksPacket p) {
  free(p.known_packs);
}

mcapiChunkAndLightDataPacket create_chunk_and_light_data_packet(ReadableBuffer *p) {
  mcapiChunkAndLightDataPacket res = {};
  res.chunk_x = read_int(p);
  res.chunk_z = read_int(p);
  printf("Chunk x=%d z=%d\n", res.chunk_x, res.chunk_z);
  res.heightmaps = read_nbt(p);
  mcapi_print_buf((mcapiBuffer){
        .len = 40,
        .ptr = p->buf.ptr + p->cursor,
      });
  int data_len = read_varint(p);

  res.chunk_section_count = 24;
  res.chunk_sections = malloc(sizeof(mcapiChunkSection) * 24);
  for (int i = 0; i < 24; i++) {
    res.chunk_sections[i].block_count = read_short(p);
    {
      uint8_t bits_per_entry = read_byte(p);
      if (bits_per_entry == 0) {
        int value = read_byte(p);
        
        memset(res.chunk_sections[i].blocks, value, 4096);
        read_varint(p); // Read the length of the data array (always 0)
      } else if (bits_per_entry <= 8) {
        int palette_len = read_byte(p);
        int palette[palette_len];

        for (int j = 0; j < palette_len; j++) {
          palette[j] = read_varint(p);
        }

        int compressed_blocks_len = read_varint(p);
        read_compressed_long_arr(p, bits_per_entry, 4096, res.chunk_sections[i].blocks);

        for (int j = 0; j < 4096; j++) {
          res.chunk_sections[i].blocks[j] = palette[res.chunk_sections[i].blocks[j]];
        }
      } else {
        int compressed_blocks_len = read_varint(p);

        read_compressed_long_arr(p, bits_per_entry, 4096, res.chunk_sections[i].blocks);
      }
    }

    // Read biomes
    {
      uint8_t bits_per_entry = read_byte(p);
      if (bits_per_entry == 0) {
        int value = read_byte(p);
        memset(res.chunk_sections[i].biomes, value, 64);
        read_varint(p); // Read the length of the data array (always 0)
      } else if (bits_per_entry <= 3) {
        int palette_len = read_byte(p);
        int palette[palette_len];

        for (int j = 0; j < palette_len; j++) {
          palette[j] = read_varint(p);
        }

        int compressed_blocks_len = read_varint(p);

        read_compressed_long_arr(p, bits_per_entry, 64, res.chunk_sections[i].biomes);

        for (int j = 0; j < 64; j++) {
          res.chunk_sections[i].biomes[j] = palette[res.chunk_sections[i].biomes[j]];
        }
      } else {
        int compressed_biomes_len = read_varint(p);
        read_compressed_long_arr(p, bits_per_entry, 64, res.chunk_sections[i].biomes);
      }
    }
  }

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

#define mcapi_setcb_func(name, packet) void mcapi_set_##name##_cb(mcapiConnection* conn, void(*cb)(mcapiConnection*, packet)) {\
  conn->name##_cb = cb;\
}

#define mcapi_setcb_func1(name) void mcapi_set_##name##_cb(mcapiConnection* conn, void(*cb)(mcapiConnection*)) {\
  conn->name##_cb = cb;\
}

mcapi_setcb_func(login_success, mcapiLoginSuccessPacket);
mcapi_setcb_func1(finish_config);
mcapi_setcb_func(clientbound_known_packs, mcapiClientboundKnownPacksPacket);
mcapi_setcb_func(chunk_and_light_data, mcapiChunkAndLightDataPacket);
mcapi_setcb_func(synchronize_player_position, mcapiSynchronizePlayerPositionPacket);

#define READABLE_BUF_SIZE 1024 * 8

uint8_t _internal_readable_buffer[READABLE_BUF_SIZE];
ReadableBuffer readable = {
  .cursor = 0,
  .buf = (mcapiBuffer) {
    .len = 0,
    .ptr = _internal_readable_buffer,
  }
};


void mcapi_poll(mcapiConnection* conn) {
  static ReadableBuffer curr_packet = {};
  static mcapiBuffer to_process = {};

  int nbytes_read;

  while ((nbytes_read = read(conn->sockfd, readable.buf.ptr, BUFSIZ)) > 0) {

    readable.buf.len = nbytes_read;
    readable.cursor = 0;

    readable.cursor = 0;

    while (readable.cursor < readable.buf.len) {
      if (curr_packet.buf.len == 0) {
        for (int i = 0; i < nbytes_read; i++) {
          if (has_varint(readable)) {
            int len = read_varint(&readable);
            printf("Read len %ld\n", len);
            curr_packet = to_readable_buffer(mcapi_create_buffer(len));
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
            printf("Skipping first byte\n");
            curr_packet.cursor = 1;  // Skip data length byte
          } else {
            perror("No support for compressed packets yet!\n");
            exit(1);
          }
        }

        // Handle packet

        int type = read_varint(&curr_packet);
        printf("Handling packet %x (len %ld)\n", type, curr_packet.buf.len);
        mcapi_print_buf(curr_packet.buf);
        if (conn->state == MCAPI_STATE_LOGIN) {
          switch(type) {
            case SET_COMPRESSION:
              printf("Enabling compression\n");
              conn->compression_threshold = read_set_compression_packet(&curr_packet).threshold;
              break;
            case LOGIN_SUCCESS:
              mcapiLoginSuccessPacket packet = create_login_success_packet(&curr_packet);
              if (conn->login_success_cb) (*conn->login_success_cb)(conn, packet);
              destory_login_success_packet(packet);
              break;
            default:
              printf("Unknown Login Packet %x (len %ld)\n", type, curr_packet.buf.len);
          }
/*
Unknown Play Packet 2b
Unknown Play Packet b
Unknown Play Packet 38
Unknown Play Packet 53
Unknown Play Packet 6d
Unknown Play Packet 1f
Unknown Play Packet 41
Unknown Play Packet 40
Unknown Play Packet 4b
Unknown Play Packet 6c
*/

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
            default:
              printf("Unknown Config Packet %x (len %ld)\n", type, curr_packet.buf.len);
          }

        } else if (conn->state == MCAPI_STATE_PLAY) {
          switch (type) {
            case CHUNK_DATA_AND_UPDATE_LIGHT:
              mcapiChunkAndLightDataPacket packet = create_chunk_and_light_data_packet(&curr_packet);
              if (conn->chunk_and_light_data_cb) (*conn->chunk_and_light_data_cb)(conn, packet);
              // TODO: destory_chunk_and_light_data_packet(curr_packet)
              break;
            case SYNCHRONIZE_PLAYER_POSITION:
              mcapiSynchronizePlayerPositionPacket syncPacket = create_synchronize_player_position_data_packet(&curr_packet);
              if (conn->synchronize_player_position_cb) (*conn->synchronize_player_position_cb)(conn, syncPacket);
              break;
            default:
              printf("Unknown Play Packet %x (len %ld)\n", type, curr_packet.buf.len);
          }
        }

        mcapi_destroy_buffer(curr_packet.buf);
        curr_packet = (ReadableBuffer){0};
      }
    }
  }
}