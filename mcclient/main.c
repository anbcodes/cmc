#include "mcapi.h"
#include <stdio.h>

/* --- Main Code --- */

void on_login_success(mcapiConnection* conn, mcapiLoginSuccessPacket packet) {
  printf("Username: ");
  mcapi_print_str(packet.username);
  printf("\nUUID: %016lx%016lx\n", packet.uuid.upper, packet.uuid.lower);
  printf("%d Properties:\n", packet.number_of_properties);
  for (int i = 0; i < packet.number_of_properties; i++) {
    printf("  ");
    mcapi_print_str(packet.properties[i].name);
    printf(": ");
    mcapi_print_str(packet.properties[i].value);
    printf("\n");
  }

  mcapi_send_login_acknowledged(conn);

  mcapi_set_state(conn, MCAPI_STATE_CONFIG);
}

void on_known_packs(mcapiConnection* conn, mcapiClientboundKnownPacksPacket packet) {
  mcapi_send_serverbound_known_packs(conn, (mcapiServerboundKnownPacksPacket){});
}

void on_finish_config(mcapiConnection* conn) {
  mcapi_send_acknowledge_finish_config(conn);

  mcapi_set_state(conn, MCAPI_STATE_PLAY);

  printf("Playing!\n");
}

void on_chunk(mcapiConnection* conn, mcapiChunkAndLightDataPacket packet) {
  printf("Got chunk!!! %d, %d\n", packet.chunk_x, packet.chunk_z);
  int total;
  for (int i = 0; i < 24; i++) {
    for (int b = 0; b < 4096; b += 1) {
      total += packet.chunk_sections[i].blocks[b];
    }
    // mcapi_print_buf((mcapiBuffer){
    //   .ptr = &packet.chunk_sections[i].blocks,
    //   .len = 4096,
    // });
    // printf("\n");
  }
  printf("total %d\n", total);
}

void on_position(mcapiConnection* conn, mcapiSynchronizePlayerPositionPacket packet) {
  mcapi_send_confirm_teleportation(conn, (mcapiConfirmTeleportationPacket){teleport_id: packet.teleport_id});
}

int main(int argc, char **argv) {
  // sock_init();
  mcapiConnection* conn = mcapi_create_connection("0.0.0.0", 25565);

  mcapi_send_handshake(conn, (mcapiHandshakePacket){
      .protocol_version = 766,
      .server_addr = "127.0.0.1",
      .server_port = 25565,
      .next_state = 2,
  });

  mcapi_send_login_start(conn, (mcapiLoginStartPacket){
      .username = mcapi_to_string("gdeggr"),
      .uuid =
          (mcapiUUID){
              .upper = 0,
              .lower = 0,
          },
  });

  mcapi_set_state(conn, MCAPI_STATE_LOGIN);

  uint8_t seperator_buf[] = {
      0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  };

  mcapi_set_login_success_cb(conn, on_login_success);
  mcapi_set_clientbound_known_packs_cb(conn, on_known_packs);
  mcapi_set_finish_config_cb(conn, on_finish_config);
  mcapi_set_chunk_and_light_data_cb(conn, on_chunk);
  mcapi_set_synchronize_player_position_cb(conn, on_position);

  while (true) {
    mcapi_poll(conn);
  }

  exit(0);
}