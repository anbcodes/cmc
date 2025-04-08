#include "player.h"
#include "internal.h"
#include "packetTypes.h"
#include "protocol.h"

void mcapi_send_confirm_teleportation(mcapiConnection *conn, mcapiConfirmTeleportationPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PTYPE_PLAY_SB_ACCEPT_TELEPORTATION);
  write_varint(&reusable_buffer, packet.teleport_id);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_set_player_position_and_rotation(mcapiConnection *conn, mcapiSetPlayerPositionAndRotationPacket packet) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;

  write_varint(&reusable_buffer, PTYPE_PLAY_SB_MOVE_PLAYER_POS_ROT);
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

  write_varint(&reusable_buffer, PTYPE_PLAY_SB_PLAYER_ACTION);
  write_varint(&reusable_buffer, packet.status);
  write_ipos(&reusable_buffer, packet.position);
  write_byte(&reusable_buffer, packet.face);
  write_varint(&reusable_buffer, packet.sequence_num);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}


// ====== Callbacks ======

MCAPI_HANDLER(play, PTYPE_PLAY_CB_PLAYER_POSITION, synchronize_player_position, mcapiSynchronizePlayerPositionPacket, ({
  packet->teleport_id = read_varint(p);
  packet->x = read_double(p);
  packet->y = read_double(p);
  packet->z = read_double(p);
  packet->vx = read_double(p);
  packet->vy = read_double(p);
  packet->vz = read_double(p);
  packet->yaw = read_float(p);
  packet->pitch = read_float(p);
  packet->flags = read_byte(p);
}), ({
  // No frees needed
}))

// MCAPI_SETCB_FUNC(synchronize_player_position, mcapiSynchronizePlayerPositionPacket)
// mcapiSynchronizePlayerPositionPacket create_synchronize_player_position_data_packet(ReadableBuffer *p) {
//   mcapiSynchronizePlayerPositionPacket res = {};
//   res.teleport_id = read_varint(p);
//   res.x = read_double(p);
//   res.y = read_double(p);
//   res.z = read_double(p);
//   res.vx = read_double(p);
//   res.vy = read_double(p);
//   res.vz = read_double(p);
//   res.yaw = read_float(p);
//   res.pitch = read_float(p);
//   res.flags = read_byte(p);
//   return res;
// }

MCAPI_HANDLER(play, PTYPE_PLAY_CB_BLOCK_DESTRUCTION, set_block_destroy_stage, mcapiSetBlockDestroyStagePacket, ({
  packet->entity_id = read_varint(p);
  read_ipos_into(p, packet->position);
  packet->stage = read_byte(p);
}), ({
  // No specific frees needed
}))

// MCAPI_SETCB_FUNC(set_block_destroy_stage, mcapiSetBlockDestroyStagePacket)
// mcapiSetBlockDestroyStagePacket read_set_block_destroy_stage_packet(ReadableBuffer *p) {
//   mcapiSetBlockDestroyStagePacket res = {};
//   res.entity_id = read_varint(p);
//   read_ipos_into(p, res.position);
//   res.stage = read_byte(p);
//   return res;
// }
