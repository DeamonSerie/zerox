#ifndef ZEROX_C_CRYPTO_H
#define ZEROX_C_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Zero Cryptographic Runtime — Public API
 *
 * All functions are implemented using the Botan library (botan/ffi.h).
 * When Botan is not available at build time, every function returns 0
 * (failure) so programs still compile.
 *
 * Mode constants match the Zero stdlib: see std.crypto.cipher for
 * MODE_ECB (0), MODE_CBC (1), MODE_CFB (2), MODE_OFB (3),
 * MODE_CTR (4), MODE_GCM (5), MODE_CCM (6).
 * ================================================================ */

/* ---------- Hash Functions ---------- */

uint32_t zerox_crypto_sha256(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_sha512(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_sha384(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_sha3_256(const unsigned char *data, size_t data_len,
                              unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_sha3_512(const unsigned char *data, size_t data_len,
                              unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_sha3_384(const unsigned char *data, size_t data_len,
                              unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_blake2b(const unsigned char *data, size_t data_len,
                             unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_blake2s(const unsigned char *data, size_t data_len,
                             unsigned char *out, size_t out_cap);

/* ---------- MAC / KDF ---------- */

uint32_t zerox_crypto_hmac_sha256(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_hmac_sha384(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_hmac_sha512(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_pbkdf2(const unsigned char *password, size_t password_len,
                            const unsigned char *salt, size_t salt_len,
                            uint32_t iterations, uint32_t out_len,
                            unsigned char *out, size_t out_cap);

/* ---------- AES (key size determines AES-128/192/256, mode selects ECB/CBC/CFB/OFB/CTR/GCM/CCM) ---------- */

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

/* ---------- Stream Ciphers ---------- */

uint32_t zerox_crypto_chacha20(const unsigned char *key, size_t key_len,
                              const unsigned char *nonce, size_t nonce_len,
                              const unsigned char *data, size_t data_len,
                              uint32_t counter,
                              unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_salsa20(const unsigned char *key, size_t key_len,
                             const unsigned char *nonce, size_t nonce_len,
                             const unsigned char *data, size_t data_len,
                             uint32_t counter,
                             unsigned char *out, size_t out_cap);

/* ---------- AEAD ---------- */

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

/* ---------- CSPRNG ---------- */

uint32_t zerox_crypto_random_bytes(unsigned char *out, size_t out_cap);

/* ---------- DES / 3DES (ECB/CBC/CFB/OFB via mode param) ---------- */

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

/* ---------- Blowfish (ECB/CBC/CFB/OFB via mode param, with IV) ---------- */

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

/* ---------- Twofish (ECB/CBC/CFB/OFB via mode param, with IV) ---------- */

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

/* ---------- Serpent (ECB/CBC/CFB/OFB via mode param, with IV) ---------- */

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

/* ---------- Camellia (key size determines Camellia-128/192/256) ---------- */

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
uint32_t zerox_crypto_ecc_ed25519_generate_keypair(
    unsigned char *pub_out, size_t pub_cap,
    unsigned char *priv_out, size_t priv_cap);
uint32_t zerox_crypto_ecc_x25519_ecdh(
    const unsigned char *priv_key, size_t priv_key_len,
    const unsigned char *peer_public, size_t peer_public_len,
    unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_ecc_x25519_generate_keypair(
    unsigned char *pub_out, size_t pub_cap,
    unsigned char *priv_out, size_t priv_cap);

/* ---------- XOF Hash ---------- */

uint32_t zerox_crypto_shake128(const unsigned char *data, size_t data_len,
                               uint32_t out_len,
                               unsigned char *out, size_t out_cap);
uint32_t zerox_crypto_shake256(const unsigned char *data, size_t data_len,
                               uint32_t out_len,
                               unsigned char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_C_CRYPTO_H */
