#include "entity.h"
#include "internal.h"
#include "packetTypes.h"
#include "protocol.h"

// void mcapi_send_acknowledge_finish_config(mcapiConnection *conn) {
//   reusable_buffer.cursor = 0;
//   reusable_buffer.buf.len = 0;
//   write_varint(&reusable_buffer, PTYPE_CONFIGURATION_SB_FINISH_CONFIGURATION);  // Packet ID

//   send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
// }

// ====== Callbacks ======

MCAPI_HANDLER(play, PTYPE_PLAY_CB_ADD_ENTITY, add_entity, mcapiAddEntityPacket, ({
  packet->id = read_varint(p);
  packet->uuid = read_uuid(p);
  packet->type = read_varint(p);
  packet->x = read_double(p);
  packet->y = read_double(p);
  packet->z = read_double(p);
  packet->yaw = read_byte(p);
  packet->pitch = read_byte(p);
  packet->yaw_head = read_byte(p);
  packet->data = read_varint(p);
  packet->vx = read_short(p);
  packet->vy = read_short(p);
  packet->vz = read_short(p);
}), ({
  // Nothing to free
}))

// DEBUG [/home/andrew/code/cmc/src/main.c:841] add_entity id: 41749 uuid: 0 type 0 x -0.00 y 8378834209423325517232372138004413361254973220902108883751006867462368413704488685667605661492157673647149244936893345189723595278909440.00 z 691.97 pitch 48 yaw 75 yaw_head 64 data 40 vx 498076 vy 9275 vz 16487

MCAPI_HANDLER(play, PTYPE_PLAY_CB_MOVE_ENTITY_POS, update_entity_position, mcapiUpdateEntityPositionPacket, ({
  packet->id = read_varint(p);
  packet->dx = read_short(p);
  packet->dy = read_short(p);
  packet->dz = read_short(p);
  packet->on_ground = read_byte(p);
}), ({
  // Nothing to free
}))

MCAPI_HANDLER(play, PTYPE_PLAY_CB_MOVE_ENTITY_POS_ROT, update_entity_position_rotation, mcapiUpdateEntityPositionRotationPacket, ({
  packet->id = read_varint(p);
  packet->dx = read_short(p);
  packet->dy = read_short(p);
  packet->dz = read_short(p);
  packet->pitch = read_byte(p);
  packet->yaw = read_byte(p);
  packet->on_ground = read_byte(p);
}), ({
  // Nothing to free
}))

MCAPI_HANDLER(play, PTYPE_PLAY_CB_ENTITY_POSITION_SYNC, teleport_entity, mcapiTeleportEntityPacket, ({
  packet->id = read_varint(p);
  packet->x = read_double(p);
  packet->y = read_double(p);
  packet->z = read_double(p);
  packet->vx = read_double(p);
  packet->vy = read_double(p);
  packet->vz = read_double(p);
  packet->yaw = read_float(p);
  packet->pitch = read_float(p);
  packet->on_ground = read_byte(p);
}), ({
  // Nothing to free
}))

MCAPI_HANDLER(play, PTYPE_PLAY_CB_REMOVE_ENTITIES, remove_entities, mcapiRemoveEntitiesPacket, ({
  packet->entity_count = read_varint(p);
  packet->entity_ids = malloc(packet->entity_count * sizeof(int));
  for (int i = 0; i < packet->entity_count; i++) {
    packet->entity_ids[i] = read_varint(p);
  }
}), ({
  free(packet->entity_ids);
}))
