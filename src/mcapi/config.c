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

// MCAPI_SETCB_FUNC(clientbound_known_packs, mcapiClientboundKnownPacksPacket)
// mcapiClientboundKnownPacksPacket create_clientbound_known_packs_packet(ReadableBuffer *p) {
//   mcapiClientboundKnownPacksPacket res = {};
//   res.known_pack_count = read_varint(p);
//   res.known_packs = malloc(sizeof(mcapiKnownPack) * res.known_pack_count);
//   for (int i = 0; i < res.known_pack_count; i++) {
//     res.known_packs[i].namespace = read_string(p);
//     res.known_packs[i].id = read_string(p);
//     res.known_packs[i].version = read_string(p);
//   }

//   return res;
// }

// void destroy_clientbound_known_packs_packet(mcapiClientboundKnownPacksPacket p) {
//   // for (int i = 0; i < p.known_pack_count; i++) {
//   //   free(p.known_packs[i].namespace.ptr);
//   //   free(p.known_packs[i].id.ptr);
//   //   free(p.known_packs[i].version.ptr);
//   // }
//   free(p.known_packs);
// }

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

// mcapiRegistryDataPacket create_registry_data_packet(ReadableBuffer *p) {
//   mcapiRegistryDataPacket res = {0};
//   res.id = read_string(p);
//   res.entry_count = read_varint(p);
//   res.entry_names = malloc(sizeof(String) * res.entry_count);
//   res.entries = malloc(sizeof(NBT *) * res.entry_count);
//   for (int i = 0; i < res.entry_count; i++) {
//     res.entry_names[i] = read_string(p);
//     bool present = read_byte(p);
//     if (present) {
//       res.entries[i] = read_nbt(p);
//     }
//   }
//   return res;
// }

// MCAPI_SETCB_FUNC(registry_data, mcapiRegistryDataPacket)
// void destroy_registry_data_packet(mcapiRegistryDataPacket p) {
//   for (int i = 0; i < p.entry_count; i++) {
//     destroy_nbt(p.entries[i]);
//   }
//   free(p.entry_names);
//   free(p.entries);
// }

// This has no packet parsing code
MCAPI_HANDLER_NO_PAYLOAD(config, PTYPE_CONFIGURATION_CB_FINISH_CONFIGURATION, finish_config)
//MCAPI_SETCB_FUNC1(finish_config)
