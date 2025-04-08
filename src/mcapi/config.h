#pragma once

#include "../datatypes.h"
#include "base.h"

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


// ====== Callbacks ======

typedef struct mcapiClientboundKnownPacksPacket {
  int known_pack_count;
  mcapiKnownPack* known_packs;
} mcapiClientboundKnownPacksPacket;

void mcapi_set_clientbound_known_packs_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiClientboundKnownPacksPacket *));

typedef struct mcapiRegistryDataPacket {
  String id;
  int entry_count;
  String* entry_names;
  NBT** entries;
} mcapiRegistryDataPacket;

void mcapi_set_registry_data_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiRegistryDataPacket *));

// No payload
void mcapi_set_finish_config_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, void*));
