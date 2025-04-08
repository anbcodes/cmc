#pragma once

#include "base.h"

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

typedef struct mcapiUpdateTimePacket {
  long world_age;
  long time_of_day;
} mcapiUpdateTimePacket;

void mcapi_set_update_time_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiUpdateTimePacket*));

typedef struct mcapiClientboundKeepAlivePacket {
  long keep_alive_id; // Random ID from the server
} mcapiClientboundKeepAlivePacket;

void mcapi_set_clientbound_keepalive_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiClientboundKeepAlivePacket*));
