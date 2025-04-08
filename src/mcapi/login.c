#include "login.h"
#include <curl/curl.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include "internal.h"
#include "packetTypes.h"
#include "protocol.h"

void mcapi_send_handshake(mcapiConnection *conn, mcapiHandshakePacket p) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, 0x00);  // Packet ID
  write_varint(&reusable_buffer, p.protocol_version);
  write_string(&reusable_buffer, p.server_addr);
  write_short(&reusable_buffer, p.server_port);
  write_varint(&reusable_buffer, p.next_state);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_login_start(mcapiConnection *conn, mcapiLoginStartPacket p) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, PTYPE_LOGIN_SB_HELLO);  // Packet ID
  write_string(&reusable_buffer, p.username);
  write_uuid(&reusable_buffer, p.uuid);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

void mcapi_send_encryption_response_packet(mcapiConnection *conn, mcapiEncryptionResponsePacket p) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, PTYPE_LOGIN_SB_KEY);  // Packet ID
  write_varint(&reusable_buffer, p.enc_shared_secret.len);
  write_buffer(&reusable_buffer, p.enc_shared_secret);
  write_varint(&reusable_buffer, p.enc_verify_token.len);
  write_buffer(&reusable_buffer, p.enc_verify_token);

  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

// Acknowledgement to the Login Success packet sent by the server.
// This packet will switch the connection state to configuration.
void mcapi_send_login_acknowledged(mcapiConnection *conn) {
  reusable_buffer.cursor = 0;
  reusable_buffer.buf.len = 0;
  write_varint(&reusable_buffer, PTYPE_LOGIN_SB_LOGIN_ACKNOWLEDGED);  // Packet ID
  send_packet(conn, resizable_buffer_to_buffer(reusable_buffer.buf));
}

// ====== Callbacks ======

MCAPI_HANDLER(login, PTYPE_LOGIN_CB_LOGIN_COMPRESSION, set_compression, mcapiSetCompressionPacket, ({
  packet->threshold = read_varint(p);
}), ({
  // Nothing special to free
}))

MCAPI_HANDLER(login, PTYPE_LOGIN_CB_HELLO, encryption_request, mcapiEncryptionRequestPacket, ({
  packet->serverId = read_string(p);
  int publen = read_varint(p);
  packet->publicKey = read_bytes(p, publen);
  int verifylen = read_varint(p);
  packet->verifyToken = read_bytes(p, verifylen);
  packet->shouldAuthenticate = read_byte(p);
}), ({
  // Nothing special to free
}))

// EncryptionRequestPacket read_encryption_request_packet(ReadableBuffer *p) {
//   EncryptionRequestPacket res = {};
//   res.serverId = read_string(p);
//   int publen = read_varint(p);
//   res.publicKey = read_bytes(p, publen);
//   int verifylen = read_varint(p);
//   res.verifyToken = read_bytes(p, verifylen);
//   res.shouldAuthenticate = read_byte(p);
//   return res;
// }

MCAPI_HANDLER(login, PTYPE_LOGIN_CB_LOGIN_FINISHED, login_success, mcapiLoginSuccessPacket, ({
  packet->uuid = read_uuid(p);
  packet->username = read_string(p);
  packet->number_of_properties = read_varint(p);
  packet->properties = malloc(sizeof(mcapiLoginSuccessProperty) * packet->number_of_properties);

  for (int i = 0; i < packet->number_of_properties; i++) {
    packet->properties[i] = (mcapiLoginSuccessProperty){
      .name = read_string(p),
      .value = read_string(p),
      .isSigned = read_byte(p),
    };

    if (packet->properties[i].isSigned) {
      packet->properties[i].signature = read_string(p);
    }
  }

  packet->strict_error_handling = read_byte(p);
}), ({
  free(packet->properties);
}))

// mcapiPacket* create_login_success_packet(ReadableBuffer *p) {
//   mcapiLoginSuccessPacket* res = malloc(sizeof(mcapiLoginSuccessPacket));
//   packet->uuid = read_uuid(p);
//   res->username = read_string(p);
//   res->number_of_properties = read_varint(p);
//   res->properties = malloc(sizeof(mcapiLoginSuccessProperty) * res->number_of_properties);

//   for (int i = 0; i < res->number_of_properties; i++) {
//     res->properties[i] = (mcapiLoginSuccessProperty){
//       .name = read_string(p),
//       .value = read_string(p),
//       .isSigned = read_byte(p),
//     };

//     if (res->properties[i].isSigned) {
//       res->properties[i].signature = read_string(p);
//     }
//   }

//   res->strict_error_handling = read_byte(p);

//   return (mcapiPacket*)res;
// }

// void destroy_login_success_packet(mcapiPacket* _p) {
//   mcapiLoginSuccessPacket *p = (mcapiLoginSuccessPacket *)_p;
//   free(p->properties);
//   free(p);
// }

// MCAPI_SETCB_FUNC(login, PTYPE_LOGIN_CB_LOGIN_FINISHED, login_success, mcapiLoginSuccessPacket)
