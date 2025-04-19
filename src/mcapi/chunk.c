#include "chunk.h"
#include <string.h>
#include <assert.h>
#include "base.h"
#include "internal.h"
#include "packetTypes.h"
#include "protocol.h"
#include "../nbt.h"


void mcapi_send_chunk_batch_received(mcapiConnection* conn, mcapiChunkBatchReceivedPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PTYPE_PLAY_SB_CHUNK_BATCH_RECEIVED);
  write_float(&reusable_buffer, packet.chunks_per_tick);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}


// ====== Callbacks ======


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

int calc_compressed_arr_len(int entries, int bits_per_entry) {
  int entries_per_long = floor(64.0 / bits_per_entry);
  int long_count = ceil((float)entries / entries_per_long);
  return long_count;
}

MCAPI_HANDLER(play, PTYPE_PLAY_CB_LEVEL_CHUNK_WITH_LIGHT, chunk_and_light_data, mcapiChunkAndLightDataPacket, ({
  write_buffer_to_file(p->buf, "tmp.bin");

  packet->chunk_x = read_int(p);
  packet->chunk_z = read_int(p);

  // Read the heightmap
  packet->heightmap_count = read_varint(p);
  packet->heightmaps = malloc(packet->heightmap_count * sizeof(mcapiHeightmap));
  for (int i = 0; i < packet->heightmap_count; i++) {
    // Likely the type but not sure
    packet->heightmaps[i].type = read_short(p);

    int longarrlen = calc_compressed_arr_len(16*16, 9);
    read_compressed_long_arr(p, 9, 16*16, longarrlen, packet->heightmaps[i].data);
  }

  // Jump to after the heightmap
  p->cursor = 0x388;
  // printf("Got chunk x=%d z=%d\n", packet->chunk_x, packet->chunk_z);
  int data_len = read_varint(p);

  int startp = p->cursor;

  packet->chunk_section_count = 24;
  packet->chunk_sections = calloc(24, sizeof(mcapiChunkSection));
  for (int i = 0; i < 24; i++) {
    packet->chunk_sections[i].block_count = read_short(p);
    {
      uint8_t bits_per_entry = read_byte(p);
      // DEBUG("bpe blocks = %d, ci = %d, p = %x\n", bits_per_entry, i, p->cursor);
      if (bits_per_entry == 0) {
        int value = read_varint(p);
        // printf("palette_all = %d\n", value);

        for (int j = 0; j < 4096; j++) {
          packet->chunk_sections[i].blocks[j] = value;
        }

        // read_varint(p);  // Read the length of the data array (always 0)
      } else if (bits_per_entry <= 8) {
        int palette_len = read_varint(p);
        int palette[palette_len];

        for (int j = 0; j < palette_len; j++) {
          palette[j] = read_varint(p);
          // printf("palette[%d] = %d\n", j, palette[j]);
        }

        // int compressed_blocks_len = read_varint(p);
        int compressed_blocks_len = calc_compressed_arr_len(4096, bits_per_entry);
        // DEBUG("compressed_blocks_len %d\n", compressed_blocks_len);
        read_compressed_long_arr(p, bits_per_entry, 4096, compressed_blocks_len, packet->chunk_sections[i].blocks);

        for (int j = 0; j < 4096; j++) {
          // if (palette[packet->chunk_sections[i].blocks[j]] == 0) {
          packet->chunk_sections[i].blocks[j] = palette[packet->chunk_sections[i].blocks[j]];
          // }
        }

      } else {
        // int compressed_blocks_len = read_varint(p);
        int compressed_blocks_len = calc_compressed_arr_len(4096, bits_per_entry);
        // DEBUG("compressed_blocks_len no palette %d\n", compressed_blocks_len);
        // printf("no_palette!!!!!!!!!!!!! %d %d\n", bits_per_entry, compressed_blocks_len);
        if (compressed_blocks_len < 10) {
          exit(1);
        }

        read_compressed_long_arr(p, bits_per_entry, 4096, compressed_blocks_len, packet->chunk_sections[i].blocks);
      }
    }

    // Read biomes
    {
      uint8_t bits_per_entry = read_byte(p);
      // DEBUG("bpe biomes = %d p = %x\n", bits_per_entry, p->cursor);
      if (bits_per_entry == 0) {
        int value = read_varint(p);
        for (int j = 0; j < 64; j++) {
          packet->chunk_sections[i].biomes[j] = value;
        }
      } else if (bits_per_entry <= 3) {
        int palette_len = read_varint(p);
        int palette[palette_len];

        for (int j = 0; j < palette_len; j++) {
          palette[j] = read_varint(p);
        }

        // int compressed_biomes_len = read_varint(p);
        int compressed_biomes_len = calc_compressed_arr_len(64, bits_per_entry);

        read_compressed_long_arr(p, bits_per_entry, 64, compressed_biomes_len, packet->chunk_sections[i].biomes);

        for (int j = 0; j < 64; j++) {
          packet->chunk_sections[i].biomes[j] = palette[packet->chunk_sections[i].biomes[j]];
        }
      } else {
        // int compressed_biomes_len = read_varint(p);
        int compressed_biomes_len = calc_compressed_arr_len(64, bits_per_entry);
        read_compressed_long_arr(p, bits_per_entry, 64, compressed_biomes_len, packet->chunk_sections[i].biomes);
      }
    }
  }

  // DEBUG("data_len=%d data_read=%d\n", data_len, p->cursor - startp);

  p->cursor = startp + data_len;

  // Block entities

  packet->block_entity_count = read_varint(p);
  // DEBUG("block_entity_count=%d\n", packet->block_entity_count);
  packet->block_entities = malloc(sizeof(mcapiBlockEntity) * packet->block_entity_count);
  for (int i = 0; i < packet->block_entity_count; i++) {
    uint8_t xz = read_byte(p);
    packet->block_entities[i].x = xz >> 4;
    packet->block_entities[i].z = xz & 0x0F;
    packet->block_entities[i].y = read_short(p);
    packet->block_entities[i].type = read_varint(p);
    packet->block_entities[i].data = read_nbt(p);
  }


  // Sky and block lights
  read_light_data_from_packet(p, packet->block_light_array, packet->sky_light_array);
}), ({
  free(packet->heightmaps);
  packet->heightmaps = NULL;
  free(packet->chunk_sections);
  packet->chunk_sections = NULL;
  for (int i = 0; i < packet->block_entity_count; i++) {
    destroy_nbt(packet->block_entities[i].data);
    packet->block_entities[i].data = NULL;
  }
  free(packet->block_entities);
  packet->block_entities = NULL;
}))

MCAPI_HANDLER(play, PTYPE_PLAY_CB_LIGHT_UPDATE, update_light, mcapiUpdateLightPacket, ({
  packet->chunk_x = read_varint(p);
  packet->chunk_z = read_varint(p);

  read_light_data_from_packet(p, packet->block_light_array, packet->sky_light_array);
}), ({
  // No frees needed
}))

MCAPI_HANDLER(play, PTYPE_PLAY_CB_CHUNK_BATCH_FINISHED, chunk_batch_finished, mcapiChunkBatchFinishedPacket, ({
  packet->batch_size = read_varint(p);
}), ({
  // No frees needed
}))


MCAPI_HANDLER(play, PTYPE_PLAY_CB_BLOCK_UPDATE, block_update, mcapiBlockUpdatePacket, ({
  read_ipos_into(p, packet->position);
  packet->block_id = read_varint(p);
}), ({
  // No free needed
}))

MCAPI_HANDLER(play, PTYPE_PLAY_CB_FORGET_LEVEL_CHUNK, unload_chunk, mcapiUnloadChunk, ({
  packet->cx = read_int(p);
  packet->cz = read_int(p);
}), ({
  // No free needed
}))
