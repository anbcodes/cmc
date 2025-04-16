#include <arpa/inet.h>
#include <curl/curl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/conf.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <libdeflate.h>
#include <string.h>
#include <unistd.h>

#include "base.h"

#include "../datatypes.h"
#include "../logging.h"
#include "../macros.h"
#include "packetTypes.h"
#include "sockets.h"
#include "internal.h"
#include "protocol.h"

#include "login.h"

void dummy_compression_cb(mcapiConnection * UNUSED(c), mcapiSetCompressionPacket * UNUSED(p)) {}

void dummy_encryption_cb(mcapiConnection * UNUSED(c), mcapiEncryptionRequestPacket * UNUSED(p)) {}

mcapiConnection *mcapi_create_connection(char *hostname, short port, char *uuid, char *access_token) {
  char protoname[] = "tcp";
  struct protoent *protoent;
  char *server_hostname = hostname;
  in_addr_t in_addr;
  int sockfd;
  struct hostent *hostent;
  /* This is the struct used by INet addresses. */
  struct sockaddr_in sockaddr_in;
  unsigned short server_port = port;

  /* Get socket. */
  protoent = getprotobyname(protoname);
  if (protoent == NULL) {
    perror("getprotobyname");
    exit(1);
  }
  sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
  if (sockfd == -1) {
    perror("socket");
    exit(1);
  }

  /* Prepare sockaddr_in. */
  hostent = gethostbyname(server_hostname);
  if (hostent == NULL) {
    FATAL("gethostbyname(\"%s\")", server_hostname);
    exit(1);
  }
  in_addr = inet_addr(inet_ntoa(*(struct in_addr *)*(hostent->h_addr_list)));
  if (in_addr == (in_addr_t)-1) {
    FATAL("inet_addr(\"%s\")", *(hostent->h_addr_list));
    exit(1);
  }
  sockaddr_in.sin_addr.s_addr = in_addr;
  sockaddr_in.sin_family = AF_INET;
  sockaddr_in.sin_port = htons(server_port);

  /* Do the actual connection. */
  if (connect(sockfd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
    perror("connect");
    return 0;
  }

  set_socket_blocking_enabled(sockfd, false);

  // Initalize openssl
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  // OPENSSL_config(NULL);

  mcapiConnection *conn = calloc(1, sizeof(mcapiConnection));
  conn->access_token = access_token;
  conn->uuid = uuid;

  conn->sockfd = sockfd;

  conn->compressor = libdeflate_alloc_compressor(6);
  conn->decompressor = libdeflate_alloc_decompressor();

  INFO("Connected to %s:%d", hostname, port);

  // Register compression and encryption to a fake callback in order to register the parsing code
  mcapi_set_set_compression_cb(conn, dummy_compression_cb);
  mcapi_set_encryption_request_cb(conn, dummy_encryption_cb);

  return conn;
}

void mcapi_destroy_connection(mcapiConnection *conn) {
  libdeflate_free_compressor(conn->compressor);
  libdeflate_free_decompressor(conn->decompressor);

  close(conn->sockfd);

  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
}

void mcapi_set_state(mcapiConnection *conn, mcapiConnState state) {
  conn->state = state;
}

mcapiConnState mcapi_get_state(mcapiConnection *conn) {
  return conn->state;
}

#define READABLE_BUF_SIZE 1024 * 8

uint8_t _internal_readable_buffer[READABLE_BUF_SIZE];
ReadableBuffer readable = {
  .cursor = 0,
  .buf = (Buffer){
    .len = 0,
    .ptr = _internal_readable_buffer,
  }
};

void enable_encryption(mcapiConnection *conn, mcapiEncryptionRequestPacket *encrypt_req) {
  Buffer shared_secret = create_buffer(16);
  RAND_bytes(shared_secret.ptr, shared_secret.len);

  if (encrypt_req->shouldAuthenticate) {
    // Get hash
    EVP_MD_CTX *mdCtx = EVP_MD_CTX_new();
    Buffer hash = create_buffer(20);
    if (1 != EVP_DigestInit_ex(mdCtx, EVP_sha1(), NULL)) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestUpdate(mdCtx, encrypt_req->serverId, strlen(encrypt_req->serverId))) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestUpdate(mdCtx, shared_secret.ptr, shared_secret.len)) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestUpdate(mdCtx, encrypt_req->publicKey.ptr, encrypt_req->publicKey.len)) ERR_print_errors_fp(stderr);
    if (1 != EVP_DigestFinal_ex(mdCtx, hash.ptr, NULL)) ERR_print_errors_fp(stderr);
    EVP_MD_CTX_free(mdCtx);

    // Convert hash to string

    char *hex_chars = "0123456789abcdef";

    WritableBuffer hash_as_string = create_writable_buffer(41);

    bool is_neg = hash.ptr[0] & 0b10000000;
    if (is_neg) {
      write_byte(&hash_as_string, '-');
      write_byte(&hash_as_string, hex_chars[((~hash.ptr[0]) & 0b11110000) >> 4]);
      write_byte(&hash_as_string, hex_chars[((~hash.ptr[0]) & 0b00001111)]);
    }
    for (int i = is_neg ? 1 : 0; i < 20; i++) {
      int v = is_neg ? (~hash.ptr[i]) : hash.ptr[i];
      if (is_neg && i == 19) {
        v++;
      }
      write_byte(&hash_as_string, hex_chars[(v & 0b11110000) >> 4]);
      write_byte(&hash_as_string, hex_chars[v & 0b00001111]);
    }

    destroy_buffer(hash);

    // Authenticate with servers

    WritableBuffer json = create_writable_buffer(50);
    write_buffer(&json, string_to_buffer("{\"accessToken\":\""));
    write_buffer(&json, string_to_buffer(conn->access_token));
    write_buffer(&json, string_to_buffer("\", \"selectedProfile\":\""));
    write_buffer(&json, string_to_buffer(conn->uuid));
    write_buffer(&json, string_to_buffer("\", \"serverId\":\""));
    write_buffer(&json, resizable_buffer_to_buffer(hash_as_string.buf));
    write_buffer(&json, string_to_buffer("\"}"));
    write_byte(&json, '\0');  // Allows it to be used like a c str

    destroy_writable_buffer(hash_as_string);

    // printf("JSON\n%s\n", json.buf.buffer.ptr);

    CURL *curl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "https://sessionserver.mojang.com/session/minecraft/join");

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.buf.buffer.ptr);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    destroy_writable_buffer(json);
  }

  // Encryption stuff

  // Decode the key
  EVP_PKEY *pkey = NULL;
  OSSL_DECODER_CTX *decoder_ctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, "DER", NULL, "RSA", EVP_PKEY_PUBLIC_KEY, NULL, NULL);
  // printf("Got here\n");
  fflush(stdout);
  if (decoder_ctx == NULL) {
    ERROR("Encryption Error");
    ERR_print_errors_fp(stderr);
  }
  Buffer pubkey = encrypt_req->publicKey;
  DEBUG("pubkey ptr = %p", encrypt_req->publicKey.ptr);
  if (1 != OSSL_DECODER_from_data(decoder_ctx, (const unsigned char **)&pubkey.ptr, &pubkey.len)) ERR_print_errors_fp(stderr);

  // Encrypt secret (https://www.openssl.org/docs/man3.0/man3/EVP_PKEY_encrypt.html)
  EVP_PKEY_CTX *pkey_ctx = EVP_PKEY_CTX_new(pkey, NULL);
  if (1 != EVP_PKEY_encrypt_init(pkey_ctx)) ERR_print_errors_fp(stderr);
  if (1 != EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING)) ERR_print_errors_fp(stderr);

  size_t ss_encrypted_len = 0;

  // Get length
  if (1 != EVP_PKEY_encrypt(pkey_ctx, NULL, &ss_encrypted_len, shared_secret.ptr, shared_secret.len)) ERR_print_errors_fp(stderr);

  // Encrypt
  Buffer ss_encrypted = create_buffer(ss_encrypted_len);
  if (1 != EVP_PKEY_encrypt(pkey_ctx, ss_encrypted.ptr, &ss_encrypted.len, shared_secret.ptr, shared_secret.len)) ERR_print_errors_fp(stderr);

  // Encrypt token

  size_t vt_encrypted_len = 0;

  // Get length
  if (1 != EVP_PKEY_encrypt(pkey_ctx, NULL, &vt_encrypted_len, encrypt_req->verifyToken.ptr, encrypt_req->verifyToken.len)) ERR_print_errors_fp(stderr);

  // Encrypt
  Buffer vt_encrypted = create_buffer(vt_encrypted_len);
  if (1 != EVP_PKEY_encrypt(pkey_ctx, vt_encrypted.ptr, &vt_encrypted.len, encrypt_req->verifyToken.ptr, encrypt_req->verifyToken.len)) ERR_print_errors_fp(stderr);

  mcapi_send_encryption_response_packet(conn, (mcapiEncryptionResponsePacket){
                                          .enc_shared_secret = ss_encrypted,
                                          .enc_verify_token = vt_encrypted,
                                        });

  destroy_buffer(ss_encrypted);
  destroy_buffer(vt_encrypted);
  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pkey_ctx);
  OSSL_DECODER_CTX_free(decoder_ctx);

  conn->shared_secret = shared_secret;

  // Set up sym encryption/decryption
  conn->encrypt_ctx = EVP_CIPHER_CTX_new();
  conn->decrypt_ctx = EVP_CIPHER_CTX_new();
  if (1 != EVP_CipherInit_ex2(conn->encrypt_ctx, EVP_aes_128_cfb8(), shared_secret.ptr, shared_secret.ptr, 1, NULL)) ERR_print_errors_fp(stderr);
  if (1 != EVP_CipherInit_ex2(conn->decrypt_ctx, EVP_aes_128_cfb8(), shared_secret.ptr, shared_secret.ptr, 0, NULL)) ERR_print_errors_fp(stderr);

  conn->encryption_enabled = true;
}

void mcapi_poll(mcapiConnection *conn) {
  static ReadableBuffer curr_packet = {};

  int nbytes_read;

  while ((nbytes_read = read(conn->sockfd, readable.buf.ptr, BUFSIZ)) > 0) {
    readable.buf.len = nbytes_read;
    readable.cursor = 0;

    if (conn->encryption_enabled) {
      // Decrypt
      int decrypted_len = 0;
      if (1 != EVP_CipherUpdate(conn->decrypt_ctx, readable.buf.ptr, &decrypted_len, readable.buf.ptr, readable.buf.len)) {
        ERR_print_errors_fp(stderr);
      }
      readable.buf.len = decrypted_len;
    }

    while (readable.cursor < readable.buf.len) {
      if (curr_packet.buf.len == 0) {
        for (int i = 0; i < nbytes_read; i++) {
          if (has_varint(readable)) {
            int len = read_varint(&readable);
            curr_packet = to_readable_buffer(create_buffer(len));
            break;
          }
        }
      } else if (curr_packet.cursor < curr_packet.buf.len) {
        int nbytes_to_copy = MIN(readable.buf.len - readable.cursor, curr_packet.buf.len - curr_packet.cursor);
        memcpy(curr_packet.buf.ptr + curr_packet.cursor, readable.buf.ptr + readable.cursor, nbytes_to_copy);

        readable.cursor += nbytes_to_copy;
        curr_packet.cursor += nbytes_to_copy;
      }
      if (curr_packet.cursor == curr_packet.buf.len && curr_packet.buf.len != 0) {
        // We have a finished packet, reset the cursor
        curr_packet.cursor = 0;

        if (conn->compression_threshold > 0) {
          if (curr_packet.buf.ptr[0] == 0) {
            curr_packet.cursor = 1;  // Skip data length byte
          } else {
            int decompressed_length = read_varint(&curr_packet);
            Buffer decompressed = create_buffer(decompressed_length);

            libdeflate_zlib_decompress(conn->decompressor, curr_packet.buf.ptr + curr_packet.cursor, curr_packet.buf.len - curr_packet.cursor, decompressed.ptr, decompressed.len, &decompressed.len);
            destroy_buffer(curr_packet.buf);
            curr_packet.buf = decompressed;
            curr_packet.cursor = 0;
          }
        }

        // Handle packet

        int type = read_varint(&curr_packet);
        // printf("Handling packet %02x (len %ld)\n", type, curr_packet.buf.len);
        // mcapi_print_buf(curr_packet.buf);
        if (conn->state == MCAPI_STATE_LOGIN) {
          if (type == PTYPE_LOGIN_CB_LOGIN_COMPRESSION) {
            INFO("Enabling compression");
            mcapiSetCompressionPacket* compression = (mcapiSetCompressionPacket *)PACKET_FUNCTIONS.login_create_funcs[PTYPE_LOGIN_CB_LOGIN_COMPRESSION](&curr_packet);
            conn->compression_threshold = compression->threshold;
            PACKET_FUNCTIONS.login_destroy_funcs[PTYPE_LOGIN_CB_LOGIN_COMPRESSION]((mcapiPacket*)compression);
          } else if (type == PTYPE_LOGIN_CB_HELLO) {
            INFO("Enabling encryption");
            mcapiEncryptionRequestPacket* encrypt_req = (mcapiEncryptionRequestPacket *)PACKET_FUNCTIONS.login_create_funcs[PTYPE_LOGIN_CB_HELLO](&curr_packet);
            enable_encryption(conn, encrypt_req);
            PACKET_FUNCTIONS.login_destroy_funcs[PTYPE_LOGIN_CB_HELLO]((mcapiPacket*)encrypt_req);
          }
          if (conn->login_cbs[type]) {
            if (PACKET_FUNCTIONS.login_create_funcs[type]) {
              mcapiPacket* packet = PACKET_FUNCTIONS.login_create_funcs[type](&curr_packet);
              conn->login_cbs[type](conn, packet);
              PACKET_FUNCTIONS.login_destroy_funcs[type](packet);
            } else {
              conn->login_cbs[type](conn, NULL);
            }
          } else {
            WARN("Unknown login packet %02x (len %ld)", type, curr_packet.buf.len);
          }
        } else if (conn->state == MCAPI_STATE_CONFIG) {
          if (conn->config_cbs[type]) {
            if (PACKET_FUNCTIONS.config_create_funcs[type]) {
              mcapiPacket* packet = PACKET_FUNCTIONS.config_create_funcs[type](&curr_packet);
              conn->config_cbs[type](conn, packet);
              PACKET_FUNCTIONS.config_destroy_funcs[type](packet);
            } else {
              conn->config_cbs[type](conn, NULL);
            }
          } else {
            WARN("Unknown config packet %02x (len %ld)", type, curr_packet.buf.len);
          }
        } else if (conn->state == MCAPI_STATE_PLAY) {
          if (conn->play_cbs[type]) {
            if (PACKET_FUNCTIONS.play_create_funcs[type]) {
              mcapiPacket* packet = PACKET_FUNCTIONS.play_create_funcs[type](&curr_packet);
              conn->play_cbs[type](conn, packet);
              PACKET_FUNCTIONS.play_destroy_funcs[type](packet);
            } else {
              conn->play_cbs[type](conn, NULL);
            }
          } else {
            // WARN("Unknown play packet %02x (len %ld)", type, curr_packet.buf.len);
          }
        }

        destroy_buffer(curr_packet.buf);
        curr_packet = (ReadableBuffer){0};
      }
    }
  }
}
