#pragma once

#include <stdint.h>
#include <cglm/cglm.h>
#include "base.h"

typedef struct mcapiChunkBatchReceivedPacket {
  float chunks_per_tick;   // Desired chunks per tick
} mcapiChunkBatchReceivedPacket;

void mcapi_send_chunk_batch_received(mcapiConnection* conn, mcapiChunkBatchReceivedPacket packet);

// ====== Callbacks ======

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

typedef struct mcapiHeightmap {
  int type;
  int data[256];
} mcapiHeightmap;

typedef struct mcapiChunkAndLightDataPacket {
  int chunk_x;
  int chunk_z;
  int heightmap_count;
  mcapiHeightmap *heightmaps;
  int chunk_section_count;
  mcapiChunkSection* chunk_sections;
  int block_entity_count;
  mcapiBlockEntity* block_entities;
  uint8_t sky_light_array[26][4096];
  uint8_t block_light_array[26][4096];
} mcapiChunkAndLightDataPacket;

void mcapi_set_chunk_and_light_data_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiChunkAndLightDataPacket*));

typedef struct mcapiUpdateLightPacket {
  int chunk_x;
  int chunk_z;
  uint8_t sky_light_array[26][4096];
  uint8_t block_light_array[26][4096];
} mcapiUpdateLightPacket;

void mcapi_set_update_light_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiUpdateLightPacket*));

typedef struct mcapiBlockUpdatePacket {
  ivec3 position;
  int block_id;  // The new block id
} mcapiBlockUpdatePacket;

void mcapi_set_block_update_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiBlockUpdatePacket*));

typedef struct mcapiChunkBatchFinishedPacket {
  int batch_size;  // The number of chunks in the batch
} mcapiChunkBatchFinishedPacket;

void mcapi_set_chunk_batch_finished_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiChunkBatchFinishedPacket*));

typedef struct mcapiUnloadChunk {
  int cx;
  int cz;
} mcapiUnloadChunk;

void mcapi_set_unload_chunk_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiUnloadChunk*));
