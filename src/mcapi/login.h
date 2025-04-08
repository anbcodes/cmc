#pragma once

#include "base.h"
#include "../datatypes.h"

typedef struct mcapiHandshakePacket {
  int protocol_version;  // See protocol version numbers https://wiki.vg/Protocol_version_numbers
  String server_addr;    // Hostname or IP, e.g. localhost or 127.0.0.1, that was used to connect. The
                         // Notchian server does not use this information. Note that SRV records are a
                         // simple redirect, e.g. if _minecraft._tcp.example.com points to
                         // mc.example.org, users connecting to example.com will provide example.org as
                         // server address in addition to connecting to it.
  short server_port;     // Default is 25565. The Notchian server does not use this information.
  int next_state;        // 1 for Status, 2 for Login.
} mcapiHandshakePacket;

void mcapi_send_handshake(mcapiConnection* conn, mcapiHandshakePacket packet);

typedef struct mcapiLoginStartPacket {
  String username;  // Player's Username.
  UUID uuid;        // The UUID of the player logging in. Unused by the Notchian server.
} mcapiLoginStartPacket;

void mcapi_send_login_start(mcapiConnection* conn, mcapiLoginStartPacket packet);
void mcapi_send_login_acknowledged(mcapiConnection* conn);

typedef struct mcapiEncryptionResponsePacket {
  Buffer enc_shared_secret;
  Buffer enc_verify_token;
} mcapiEncryptionResponsePacket;
void mcapi_send_encryption_response_packet(mcapiConnection *conn, mcapiEncryptionResponsePacket p);

// ====== Callbacks ======

// Enables compression. If compression is enabled, all following packets are encoded in the
// compressed packet format. Negative values will disable compression, meaning the packet format
// should remain in the uncompressed packet format. However, this packet is entirely optional, and
// if not sent, compression will also not be enabled (the notchian server does not send the packet
// when compression is disabled).
typedef struct mcapiSetCompressionPacket {
  int threshold;  // Maximum size of a packet before it is compressed.
} mcapiSetCompressionPacket;

void mcapi_set_set_compression_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiSetCompressionPacket*));

typedef struct mcapiEncryptionRequestPacket {
  String serverId;
  Buffer publicKey;
  Buffer verifyToken;
  bool shouldAuthenticate;
} mcapiEncryptionRequestPacket;

void mcapi_set_encryption_request_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiEncryptionRequestPacket*));

typedef struct mcapiLoginSuccessProperty {
  String name;
  String value;
  bool isSigned;
  String signature;  // Only if Is Signed is true.
} mcapiLoginSuccessProperty;

typedef struct mcapiLoginSuccessPacket {
  UUID uuid;
  String username;
  int number_of_properties;  // Number of elements in the following array.
  mcapiLoginSuccessProperty* properties;
  bool strict_error_handling;  // Whether the client should immediately disconnect upon a packet
                               // processing error. The Notchian client silently ignores them when
                               // this flag is false.
} mcapiLoginSuccessPacket;

void mcapi_set_login_success_cb(mcapiConnection* conn, void (*cb)(mcapiConnection*, mcapiLoginSuccessPacket*));
