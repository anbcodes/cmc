#include "misc.h"
#include "internal.h"
#include "packetTypes.h"
#include "protocol.h"
#include "../logging.h"

void mcapi_send_play_pong(mcapiConnection *conn, mcapiPlayPongPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PTYPE_PLAY_SB_PONG);
  write_int(&reusable_buffer, packet.id);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_play_ping_request(mcapiConnection *conn, mcapiPingRequestPacket packet) {
  TRACE("Sending play ping request %ld", packet.id);
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PTYPE_PLAY_SB_PING_REQUEST);
  write_long(&reusable_buffer, packet.id);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_serverbound_keepalive(mcapiConnection* conn, mcapiServerboundKeepalivePacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PTYPE_PLAY_SB_KEEP_ALIVE);
  write_long(&reusable_buffer, packet.id);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

// -- Callbacks --

MCAPI_HANDLER(play, PTYPE_PLAY_CB_SET_TIME, update_time, mcapiUpdateTimePacket, ({
  packet->world_age = read_long(p);
  packet->time_of_day = read_long(p);
}), ({
  // No specific frees needed
}))

MCAPI_HANDLER(play, PTYPE_PLAY_CB_KEEP_ALIVE, clientbound_keepalive, mcapiClientboundKeepAlivePacket, ({
  packet->keep_alive_id = read_long(p);
}), ({
  // No specific frees needed
}))
