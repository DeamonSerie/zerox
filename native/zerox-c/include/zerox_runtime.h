#ifndef ZERO_C_RUNTIME_H
#define ZERO_C_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  const unsigned char *ptr;
  size_t len;
} ZeroxByteView;

typedef struct {
  unsigned char *ptr;
  size_t len;
} ZeroxMutByteView;

typedef enum {
  ZEROX_HTTP_OK = 0,
  ZEROX_HTTP_INVALID_URL = 1,
  ZEROX_HTTP_UNSUPPORTED_PROTOCOL = 2,
  ZEROX_HTTP_DNS = 3,
  ZEROX_HTTP_CONNECT = 4,
  ZEROX_HTTP_TLS = 5,
  ZEROX_HTTP_TIMEOUT = 6,
  ZEROX_HTTP_TOO_LARGE = 7,
  ZEROX_HTTP_PROVIDER_UNAVAILABLE = 8,
  ZEROX_HTTP_IO = 9,
  ZEROX_HTTP_INVALID_REQUEST = 10
} ZeroxHttpError;

#define ZEROX_HTTP_RESPONSE_META_BYTES 24u

int zerox_world_write(int fd, const char *buf, unsigned len);

int64_t zerox_json_parse_bytes(ZeroxByteView input);

uint64_t zerox_http_fetch_result(
  ZeroxByteView request,
  ZeroxMutByteView response_out,
  int64_t timeout_ns
);

uint32_t zerox_http_result_ok(uint64_t result);
uint32_t zerox_http_result_status(uint64_t result);
uint32_t zerox_http_result_body_len(uint64_t result);
uint32_t zerox_http_result_error(uint64_t result);

uint32_t zerox_http_response_len(ZeroxByteView response);
uint32_t zerox_http_response_headers_len(ZeroxByteView response);
uint32_t zerox_http_response_body_offset(ZeroxByteView response);
uint64_t zerox_http_header_value(ZeroxByteView headers, ZeroxByteView name);
uint32_t zerox_http_header_found(uint64_t value);
uint32_t zerox_http_header_offset(uint64_t value);
uint32_t zerox_http_header_len(uint64_t value);

/* ---------- Crypto Runtime Helpers ---------- */

uint32_t zerox_crypto_sha256(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_hmac_sha256(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_aes_encrypt(const unsigned char *key, size_t key_len,
                                 const unsigned char *iv, size_t iv_len,
                                 const unsigned char *data, size_t data_len,
                                 uint32_t mode,
                                 unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_aes_decrypt(const unsigned char *key, size_t key_len,
                                 const unsigned char *iv, size_t iv_len,
                                 const unsigned char *data, size_t data_len,
                                 uint32_t mode,
                                 unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_chacha20(const unsigned char *key, size_t key_len,
                              const unsigned char *nonce, size_t nonce_len,
                              const unsigned char *data, size_t data_len,
                              uint32_t counter,
                              unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_pbkdf2(const unsigned char *password, size_t password_len,
                            const unsigned char *salt, size_t salt_len,
                            uint32_t iterations, uint32_t out_len,
                            unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_random_bytes(unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_sha512(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_salsa20(const unsigned char *key, size_t key_len,
                             const unsigned char *nonce, size_t nonce_len,
                             const unsigned char *data, size_t data_len,
                             uint32_t counter,
                             unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_aes_gcm_encrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      const unsigned char *aad, size_t aad_len,
                                      unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_aes_gcm_decrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      const unsigned char *aad, size_t aad_len,
                                      unsigned char *out, size_t out_cap);

uint32_t zerox_crypto_chacha20_poly1305_encrypt(
    const unsigned char *key, size_t key_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *data, size_t data_len,
    const unsigned char *aad, size_t aad_len,
    unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_chacha20_poly1305_decrypt(
    const unsigned char *key, size_t key_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *data, size_t data_len,
    const unsigned char *aad, size_t aad_len,
    unsigned char *out, size_t out_cap);

/* ---------- DES / 3DES ---------- */
uint32_t zerox_crypto_des_encrypt(const unsigned char *key, size_t key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t mode,
                                  unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_des_decrypt(const unsigned char *key, size_t key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t mode,
                                  unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_tdes_encrypt(const unsigned char *key, size_t key_len,
                                   const unsigned char *data, size_t data_len,
                                   uint32_t mode,
                                   unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_tdes_decrypt(const unsigned char *key, size_t key_len,
                                   const unsigned char *data, size_t data_len,
                                   uint32_t mode,
                                   unsigned char *out, size_t out_cap);

/* ---------- Blowfish ---------- */
uint32_t zerox_crypto_blowfish_encrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_blowfish_decrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap);

/* ---------- Twofish ---------- */
uint32_t zerox_crypto_twofish_encrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_twofish_decrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap);

/* ---------- Serpent ---------- */
uint32_t zerox_crypto_serpent_encrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_serpent_decrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap);

/* ---------- Camellia ---------- */
uint32_t zerox_crypto_camellia_encrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_camellia_decrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap);

/* ---------- RSA ---------- */
uint32_t zerox_crypto_rsa_generate_keypair(uint32_t key_bits,
                                           unsigned char *pub_out, size_t pub_cap,
                                           unsigned char *priv_out, size_t priv_cap);
uint32_t zerox_crypto_rsa_encrypt(const unsigned char *pub_key, size_t pub_key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t padding,
                                  unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_rsa_decrypt(const unsigned char *priv_key, size_t priv_key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t padding,
                                  unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_rsa_sign(const unsigned char *priv_key, size_t priv_key_len,
                               const unsigned char *data, size_t data_len,
                               uint32_t hash_algo, uint32_t padding,
                               unsigned char *out, size_t out_cap);
int zerox_crypto_rsa_verify(const unsigned char *pub_key, size_t pub_key_len,
                           const unsigned char *data, size_t data_len,
                           const unsigned char *sig, size_t sig_len,
                           uint32_t hash_algo, uint32_t padding);

/* ---------- ECC ---------- */
uint32_t zerox_crypto_ecc_generate_keypair(uint32_t curve,
                                           unsigned char *pub_out, size_t pub_cap,
                                           unsigned char *priv_out, size_t priv_cap);
uint32_t zerox_crypto_ecc_sign(const unsigned char *priv_key, size_t priv_key_len,
                               const unsigned char *data, size_t data_len,
                               uint32_t curve,
                               unsigned char *out, size_t out_cap);
int zerox_crypto_ecc_verify(const unsigned char *pub_key, size_t pub_key_len,
                           const unsigned char *data, size_t data_len,
                           const unsigned char *sig, size_t sig_len,
                           uint32_t curve);
uint32_t zerox_crypto_ecc_ecdh(const unsigned char *priv_key, size_t priv_key_len,
                               const unsigned char *peer_public, size_t peer_public_len,
                               uint32_t curve,
                               unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_ecc_ed25519_sign(const unsigned char *priv_key, size_t priv_key_len,
                                       const unsigned char *data, size_t data_len,
                                       unsigned char *out, size_t out_cap);
int zerox_crypto_ecc_ed25519_verify(const unsigned char *pub_key, size_t pub_key_len,
                                   const unsigned char *data, size_t data_len,
                                   const unsigned char *sig, size_t sig_len);
uint32_t zerox_crypto_ecc_x25519_generate_keypair(
    unsigned char *pub_out, size_t pub_cap,
    unsigned char *priv_out, size_t priv_cap);

#endif
