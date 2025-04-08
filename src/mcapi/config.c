#include "config.h"
#include "internal.h"
#include "packetTypes.h"
#include "protocol.h"
#include "../macros.h"
#include "../nbt.h"

void mcapi_send_acknowledge_finish_config(mcapiConnection *conn) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, PTYPE_CONFIGURATION_SB_FINISH_CONFIGURATION);  // Packet ID

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_serverbound_known_packs(mcapiConnection *conn, mcapiServerboundKnownPacksPacket UNUSED(packet)) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PTYPE_CONFIGURATION_SB_SELECT_KNOWN_PACKS);
  write_varint(&reusable_buffer, 0);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

// ====== Callbacks ======

MCAPI_HANDLER(config, PTYPE_CONFIGURATION_CB_SELECT_KNOWN_PACKS, clientbound_known_packs, mcapiClientboundKnownPacksPacket, ({
  packet->known_pack_count = read_varint(p);
  packet->known_packs = malloc(sizeof(mcapiKnownPack) * packet->known_pack_count);
  for (int i = 0; i < packet->known_pack_count; i++) {
    packet->known_packs[i].namespace = read_string(p);
    packet->known_packs[i].id = read_string(p);
    packet->known_packs[i].version = read_string(p);
  }
}), ({
  free(packet->known_packs);
}))

MCAPI_HANDLER(config, PTYPE_CONFIGURATION_CB_REGISTRY_DATA, registry_data, mcapiRegistryDataPacket, ({
  packet->id = read_string(p);
  packet->entry_count = read_varint(p);
  packet->entry_names = malloc(sizeof(String) * packet->entry_count);
  packet->entries = malloc(sizeof(NBT *) * packet->entry_count);
  for (int i = 0; i < packet->entry_count; i++) {
    packet->entry_names[i] = read_string(p);
    bool present = read_byte(p);
    if (present) {
      packet->entries[i] = read_nbt(p);
    }
  }
}), ({
  for (int i = 0; i < packet->entry_count; i++) {
    destroy_nbt(packet->entries[i]);
  }
  free(packet->entry_names);
  free(packet->entries);
}))

// This has no packet parsing code
MCAPI_HANDLER_NO_PAYLOAD(config, PTYPE_CONFIGURATION_CB_FINISH_CONFIGURATION, finish_config)
