/* --- Sending Packet Code --- */

#include <libdeflate.h>
#include <openssl/evp.h>
#include <string.h>
#include <unistd.h>

#include "base.h"
#include "../datatypes.h"
#include "protocol.h"
#include "internal.h"

uint8_t internal_buf[INTERNAL_BUF_SIZE];
WritableBuffer reusable_buffer = {
  .buf = {
    .buffer = {
      .ptr = internal_buf,
      .len = INTERNAL_BUF_SIZE,
    }
  },
  .cursor = 0,
};

PacketFunctions PACKET_FUNCTIONS = { 0 };

void send_packet(mcapiConnection *conn, const Buffer packet) {
  WritableBuffer header_buffer = create_writable_buffer(30);

  Buffer const *rest_of_packet = NULL;
  Buffer compressed_buf = {};
  Buffer encrypted = {};

  if (conn->compression_threshold > 0) {
    if (packet.len < (size_t)conn->compression_threshold) {
      write_varint(&header_buffer, packet.len + 1);
      write_varint(&header_buffer, 0);
      rest_of_packet = &packet;
    } else {
      int max_size = libdeflate_zlib_compress_bound(conn->compressor, packet.len);
      compressed_buf = create_buffer(max_size);
      compressed_buf.len = libdeflate_zlib_compress(conn->compressor, packet.ptr, packet.len, compressed_buf.ptr, compressed_buf.len);

      write_varint(&header_buffer, packet.len);
      int total_len = compressed_buf.len + header_buffer.buf.len;
      header_buffer.cursor = header_buffer.buf.len = 0;
      write_varint(&header_buffer, total_len);
      write_varint(&header_buffer, packet.len);

      rest_of_packet = &compressed_buf;
    }
  } else {
    write_varint(&header_buffer, packet.len);
    rest_of_packet = &packet;
  }

  Buffer new_packet = create_buffer(header_buffer.buf.len + rest_of_packet->len);
  memcpy(new_packet.ptr, header_buffer.buf.buffer.ptr, header_buffer.buf.len);
  memcpy(new_packet.ptr + header_buffer.buf.len, rest_of_packet->ptr, rest_of_packet->len);

  if (conn->encryption_enabled) {
    int encrypted_len = 0;
    EVP_CipherUpdate(conn->encrypt_ctx, new_packet.ptr, &encrypted_len, new_packet.ptr, new_packet.len);
    new_packet.len = encrypted_len;
  }

  write(conn->sockfd, new_packet.ptr, new_packet.len);

  destroy_buffer(new_packet);
  destroy_writable_buffer(header_buffer);
  if (compressed_buf.len != 0) destroy_buffer(compressed_buf);
  if (encrypted.len != 0) destroy_buffer(encrypted);
}
