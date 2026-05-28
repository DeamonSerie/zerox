/* ===================================================================
 * Zero Cryptographic Runtime — Botan-backed implementation
 *
 * All zerox_crypto_* functions are implemented using Botan's C FFI
 * (botan/ffi.h). When Botan is not available at build time, every
 * function returns 0 (failure) so the program still compiles.
 * =================================================================== */

#include "zerox_crypto.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if __has_include(<botan/ffi.h>)
#  include <botan/ffi.h>
#  define HAVE_BOTAN 1
#endif

/* ---------- Mode constants matching std.crypto.cipher ---------- */
#define ZEROX_MODE_ECB 0
#define ZEROX_MODE_CBC 1
#define ZEROX_MODE_CFB 2
#define ZEROX_MODE_OFB 3
#define ZEROX_MODE_CTR 4
#define ZEROX_MODE_GCM 5
#define ZEROX_MODE_CCM 6

/* ---------- Internal helpers ---------- */

#ifdef HAVE_BOTAN

/* Build a Botan cipher spec string for block ciphers.
   Returns 0 on success, -1 on bad mode. */
static int block_cipher_spec(const char *base, uint32_t mode,
                             char *out, size_t out_cap) {
  const char *suffix;
  switch (mode) {
    case ZEROX_MODE_ECB: suffix = "/ECB/PKCS7";   break;
    case ZEROX_MODE_CBC: suffix = "/CBC/PKCS7";   break;
    case ZEROX_MODE_CFB: suffix = "/CFB/PKCS7";   break;
    case ZEROX_MODE_OFB: suffix = "/OFB/PKCS7";   break;
    case ZEROX_MODE_CTR: suffix = "/CTR/NoPadding"; break;
    case ZEROX_MODE_GCM: suffix = "/GCM";          break;
    default:            suffix = "/CBC/PKCS7";    break;
  }
  int n = snprintf(out, out_cap, "%s%s", base, suffix);
  return (n > 0 && (size_t)n < out_cap) ? 0 : -1;
}

/* Determine AES base name from key length. */
static const char *aes_base(size_t key_len) {
  switch (key_len) {
    case 16: return "AES-128";
    case 24: return "AES-192";
    case 32: return "AES-256";
    default: return NULL;
  }
}

/* Determine Camellia base name from key length. */
static const char *camellia_base(size_t key_len) {
  switch (key_len) {
    case 16: return "Camellia-128";
    case 24: return "Camellia-192";
    case 32: return "Camellia-256";
    default: return NULL;
  }
}

/* Run a one-shot hash via Botan. Returns output length or 0. */
static uint32_t botan_hash_one(const char *algo,
                                const unsigned char *data, size_t data_len,
                                unsigned char *out, size_t out_cap) {
  botan_hash_t h = NULL;
  if (botan_hash_init(&h, algo, 0) != 0) return 0;
  size_t out_len = 0;
  botan_hash_output_length(h, &out_len);
  if (out_cap < out_len) { botan_hash_destroy(h); return 0; }
  int rc = botan_hash_update(h, data, data_len);
  if (rc != 0) { botan_hash_destroy(h); return 0; }
  rc = botan_hash_final(h, out);
  botan_hash_destroy(h);
  return (rc == 0) ? (uint32_t)out_len : 0;
}

/* Run a one-shot MAC via Botan. Returns output length or 0. */
static uint32_t botan_mac_one(const char *algo,
                               const unsigned char *key, size_t key_len,
                               const unsigned char *data, size_t data_len,
                               unsigned char *out, size_t out_cap) {
  botan_mac_t m = NULL;
  if (botan_mac_init(&m, algo, 0) != 0) return 0;
  size_t out_len = 0;
  botan_mac_output_length(m, &out_len);
  if (out_cap < out_len) { botan_mac_destroy(m); return 0; }
  int rc = botan_mac_set_key(m, key, key_len);
  if (rc != 0) { botan_mac_destroy(m); return 0; }
  rc = botan_mac_update(m, data, data_len);
  if (rc != 0) { botan_mac_destroy(m); return 0; }
  rc = botan_mac_final(m, out);
  botan_mac_destroy(m);
  return (rc == 0) ? (uint32_t)out_len : 0;
}

/* Run a block cipher encrypt/decrypt via Botan cipher API.
   data_len is input length, out_cap is output buffer capacity.
   Returns bytes written or 0. */
static uint32_t botan_block_op(const char *spec,
                                const unsigned char *key, size_t key_len,
                                const unsigned char *iv, size_t iv_len,
                                const unsigned char *data, size_t data_len,
                                unsigned char *out, size_t out_cap,
                                int encrypt) {
  unsigned flags = encrypt ? BOTAN_CIPHER_INIT_FLAG_ENCRYPT
                           : BOTAN_CIPHER_INIT_FLAG_DECRYPT;
  botan_cipher_t c = NULL;
  if (botan_cipher_init(&c, spec, flags) != 0) return 0;
  int rc = botan_cipher_set_key(c, key, key_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  rc = botan_cipher_start(c, iv, iv_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }

  size_t written = 0;
  rc = botan_cipher_update(c, BOTAN_CIPHER_UPDATE_FLAG_FINAL,
                           out, out_cap, &written,
                           data, data_len);
  botan_cipher_destroy(c);
  return (rc == 0) ? (uint32_t)written : 0;
}

/* AEAD encrypt/decrypt with AAD support via Botan cipher API.
   For GCM mode, AAD is processed before the main data. */
static uint32_t botan_aead_op(const char *spec,
                               const unsigned char *key, size_t key_len,
                               const unsigned char *iv, size_t iv_len,
                               const unsigned char *aad, size_t aad_len,
                               const unsigned char *data, size_t data_len,
                               unsigned char *out, size_t out_cap,
                               int encrypt) {
  unsigned flags = encrypt ? BOTAN_CIPHER_INIT_FLAG_ENCRYPT
                           : BOTAN_CIPHER_INIT_FLAG_DECRYPT;
  botan_cipher_t c = NULL;
  if (botan_cipher_init(&c, spec, flags) != 0) return 0;
  int rc = botan_cipher_set_key(c, key, key_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  rc = botan_cipher_start(c, iv, iv_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  
  /* Process AAD if provided */
  if (aad && aad_len > 0) {
    size_t aad_written = 0;
    rc = botan_cipher_update(c, BOTAN_CIPHER_UPDATE_FLAG_NONE,
                             NULL, 0, &aad_written,
                             aad, aad_len);
    if (rc != 0) { botan_cipher_destroy(c); return 0; }
  }

  size_t written = 0;
  rc = botan_cipher_update(c, BOTAN_CIPHER_UPDATE_FLAG_FINAL,
                           out, out_cap, &written,
                           data, data_len);
  botan_cipher_destroy(c);
  return (rc == 0) ? (uint32_t)written : 0;
}

/* Block cipher encrypt/decrypt for legacy ciphers (DES, 3DES) that
   have NO IV parameter in their Zero API. For ECB mode iv is NULL/0;
   for CBC use a zero IV. */
static uint32_t legacy_block_op(const char *base,
                                 const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 uint32_t mode,
                                 unsigned char *out, size_t out_cap,
                                 int encrypt) {
  char spec[64];
  if (block_cipher_spec(base, mode, spec, sizeof(spec)) != 0) return 0;
  const unsigned char *iv = NULL;
  size_t iv_len = 0;
  unsigned char zero_iv[16] = {0};
  if (mode != ZEROX_MODE_ECB) {
    iv = zero_iv;
    iv_len = 8; /* DES/3DES block size */
  }
  return botan_block_op(spec, key, key_len, iv, iv_len,
                        data, data_len, out, out_cap, encrypt);
}

/* Block cipher encrypt/decrypt for ciphers WITH an IV parameter. */
static uint32_t iv_block_op(const char *base,
                             const unsigned char *key, size_t key_len,
                             const unsigned char *iv, size_t iv_len,
                             const unsigned char *data, size_t data_len,
                             uint32_t mode,
                             unsigned char *out, size_t out_cap,
                             int encrypt) {
  char spec[64];
  if (block_cipher_spec(base, mode, spec, sizeof(spec)) != 0) return 0;
  return botan_block_op(spec, key, key_len, iv, iv_len,
                        data, data_len, out, out_cap, encrypt);
}

#endif /* HAVE_BOTAN */

/* ===================================================================
 * SHA-256
 * =================================================================== */

uint32_t zerox_crypto_sha256(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("SHA-256", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * SHA-512
 * =================================================================== */

uint32_t zerox_crypto_sha512(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("SHA-512", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * SHA-384
 * =================================================================== */

uint32_t zerox_crypto_sha384(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("SHA-384", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * SHA-3-256
 * =================================================================== */

uint32_t zerox_crypto_sha3_256(const unsigned char *data, size_t data_len,
                              unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("SHA-3(256)", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * SHA-3-512
 * =================================================================== */

uint32_t zerox_crypto_sha3_512(const unsigned char *data, size_t data_len,
                              unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("SHA-3(512)", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * SHA-3-384
 * =================================================================== */

uint32_t zerox_crypto_sha3_384(const unsigned char *data, size_t data_len,
                              unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("SHA-3(384)", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * SHAKE-128 (XOF)
 * =================================================================== */

static uint32_t botan_xof_one(const char *algo,
                               const unsigned char *data, size_t data_len,
                               uint32_t out_len,
                               unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  botan_hash_xof_t xof = NULL;
  if (botan_hash_xof_init(&xof, algo, 0) != 0) return 0;
  if (out_cap < out_len) { botan_hash_xof_destroy(xof); return 0; }
  int rc = botan_hash_xof_update(xof, data, data_len);
  if (rc != 0) { botan_hash_xof_destroy(xof); return 0; }
  rc = botan_hash_xof_output(xof, out, out_len);
  botan_hash_xof_destroy(xof);
  return (rc == 0) ? out_len : 0;
#else
  (void)algo; (void)data; (void)data_len;
  (void)out_len; (void)out; (void)out_cap;
  return 0;
#endif
}

uint32_t zerox_crypto_shake128(const unsigned char *data, size_t data_len,
                               uint32_t out_len,
                               unsigned char *out, size_t out_cap) {
  return botan_xof_one("SHAKE-128", data, data_len, out_len, out, out_cap);
}

/* ===================================================================
 * SHAKE-256 (XOF)
 * =================================================================== */

uint32_t zerox_crypto_shake256(const unsigned char *data, size_t data_len,
                               uint32_t out_len,
                               unsigned char *out, size_t out_cap) {
  return botan_xof_one("SHAKE-256", data, data_len, out_len, out, out_cap);
}

/* ===================================================================
 * BLAKE2b
 * =================================================================== */

uint32_t zerox_crypto_blake2b(const unsigned char *data, size_t data_len,
                             unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("BLAKE2b(512)", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * BLAKE2s
 * =================================================================== */

uint32_t zerox_crypto_blake2s(const unsigned char *data, size_t data_len,
                             unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_hash_one("BLAKE2s(256)", data, data_len, out, out_cap);
#else
  (void)data; (void)data_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * HMAC-SHA384
 * =================================================================== */

uint32_t zerox_crypto_hmac_sha384(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_mac_one("HMAC(SHA-384)", key, key_len, data, data_len,
                       out, out_cap);
#else
  (void)key; (void)key_len; (void)data; (void)data_len;
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * HMAC-SHA512
 * =================================================================== */

uint32_t zerox_crypto_hmac_sha512(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_mac_one("HMAC(SHA-512)", key, key_len, data, data_len,
                       out, out_cap);
#else
  (void)key; (void)key_len; (void)data; (void)data_len;
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * HMAC-SHA256
 * =================================================================== */

uint32_t zerox_crypto_hmac_sha256(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len,
                                 unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return botan_mac_one("HMAC(SHA-256)", key, key_len, data, data_len,
                       out, out_cap);
#else
  (void)key; (void)key_len; (void)data; (void)data_len;
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * PBKDF2-HMAC-SHA256
 * =================================================================== */

uint32_t zerox_crypto_pbkdf2(const unsigned char *password, size_t password_len,
                            const unsigned char *salt, size_t salt_len,
                            uint32_t iterations, uint32_t out_len,
                            unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  size_t actual = (out_len < (uint32_t)out_cap) ? (size_t)out_len : out_cap;
  int rc = botan_pbkdf(NULL, "PBKDF2(SHA-256)",
                       out, actual, NULL, 0,
                       password, password_len,
                       salt, salt_len, iterations);
  return (rc == 0) ? (uint32_t)actual : 0;
#else
  (void)password; (void)password_len; (void)salt; (void)salt_len;
  (void)iterations; (void)out_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * AES Encrypt
 * =================================================================== */

uint32_t zerox_crypto_aes_encrypt(const unsigned char *key, size_t key_len,
                                 const unsigned char *iv, size_t iv_len,
                                 const unsigned char *data, size_t data_len,
                                 uint32_t mode,
                                 unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  const char *base = aes_base(key_len);
  if (!base) return 0;
  return iv_block_op(base, key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * AES Decrypt
 * =================================================================== */

uint32_t zerox_crypto_aes_decrypt(const unsigned char *key, size_t key_len,
                                 const unsigned char *iv, size_t iv_len,
                                 const unsigned char *data, size_t data_len,
                                 uint32_t mode,
                                 unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  const char *base = aes_base(key_len);
  if (!base) return 0;
  return iv_block_op(base, key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * AES-GCM AEAD Encrypt (with AAD)
 * =================================================================== */

uint32_t zerox_crypto_aes_gcm_encrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      const unsigned char *aad, size_t aad_len,
                                      unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  const char *base = aes_base(key_len);
  if (!base) return 0;
  char spec[64];
  if (block_cipher_spec(base, ZEROX_MODE_GCM, spec, sizeof(spec)) != 0) return 0;
  return botan_aead_op(spec, key, key_len, iv, iv_len, aad, aad_len,
                        data, data_len, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)aad; (void)aad_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * AES-GCM AEAD Decrypt (with AAD)
 * =================================================================== */

uint32_t zerox_crypto_aes_gcm_decrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      const unsigned char *aad, size_t aad_len,
                                      unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  const char *base = aes_base(key_len);
  if (!base) return 0;
  char spec[64];
  if (block_cipher_spec(base, ZEROX_MODE_GCM, spec, sizeof(spec)) != 0) return 0;
  return botan_aead_op(spec, key, key_len, iv, iv_len, aad, aad_len,
                        data, data_len, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)aad; (void)aad_len; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * ChaCha20 / XChaCha20
 * =================================================================== */

uint32_t zerox_crypto_chacha20(const unsigned char *key, size_t key_len,
                              const unsigned char *nonce, size_t nonce_len,
                              const unsigned char *data, size_t data_len,
                              uint32_t counter,
                              unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  /* Botan's ChaCha20 accepts a 12-byte nonce.
     For XChaCha20 (24-byte nonce) we use HChaCha20 internally.
     Botan has "ChaCha20" and "XChaCha20" as separate specs. */
  const char *spec = (nonce_len >= 24) ? "XChaCha20" : "ChaCha20";
  botan_cipher_t c = NULL;
  unsigned flags = BOTAN_CIPHER_INIT_FLAG_ENCRYPT; /* same for decrypt — ChaCha20 is XOR */
  if (botan_cipher_init(&c, spec, flags) != 0) return 0;
  int rc = botan_cipher_set_key(c, key, key_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }

  /* Set the nonce as the IV. Some Botan versions need the counter
     combined with the nonce for ChaCha20. We handle this by setting
     the nonce as the IV and letting Botan manage the counter. */
  size_t actual_nonce = (nonce_len >= 24) ? 24 : 12;
  rc = botan_cipher_start(c, nonce, actual_nonce);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }

  size_t written = 0;
  rc = botan_cipher_update(c, BOTAN_CIPHER_UPDATE_FLAG_FINAL,
                           out, out_cap, &written,
                           data, data_len);
  botan_cipher_destroy(c);
  return (rc == 0) ? (uint32_t)written : 0;
#else
  (void)key; (void)key_len; (void)nonce; (void)nonce_len;
  (void)data; (void)data_len; (void)counter; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Salsa20
 * =================================================================== */

uint32_t zerox_crypto_salsa20(const unsigned char *key, size_t key_len,
                             const unsigned char *nonce, size_t nonce_len,
                             const unsigned char *data, size_t data_len,
                             uint32_t counter,
                             unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  botan_cipher_t c = NULL;
  if (botan_cipher_init(&c, "Salsa20", BOTAN_CIPHER_INIT_FLAG_ENCRYPT) != 0) return 0;
  int rc = botan_cipher_set_key(c, key, key_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  rc = botan_cipher_start(c, nonce, nonce_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  size_t written = 0;
  rc = botan_cipher_update(c, BOTAN_CIPHER_UPDATE_FLAG_FINAL,
                           out, out_cap, &written,
                           data, data_len);
  botan_cipher_destroy(c);
  return (rc == 0) ? (uint32_t)written : 0;
#else
  (void)key; (void)key_len; (void)nonce; (void)nonce_len;
  (void)data; (void)data_len; (void)counter; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * ChaCha20-Poly1305 AEAD Encrypt
 * =================================================================== */

uint32_t zerox_crypto_chacha20_poly1305_encrypt(
    const unsigned char *key, size_t key_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *data, size_t data_len,
    const unsigned char *aad, size_t aad_len,
    unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  botan_cipher_t c = NULL;
  if (botan_cipher_init(&c, "ChaCha20Poly1305",
                         BOTAN_CIPHER_INIT_FLAG_ENCRYPT) != 0) return 0;
  int rc = botan_cipher_set_key(c, key, key_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  if (aad && aad_len > 0) {
    rc = botan_cipher_set_associated_data(c, aad, aad_len);
    if (rc != 0) { botan_cipher_destroy(c); return 0; }
  }
  rc = botan_cipher_start(c, nonce, nonce_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  size_t written = 0;
  rc = botan_cipher_update(c, BOTAN_CIPHER_UPDATE_FLAG_FINAL,
                           out, out_cap, &written,
                           data, data_len);
  botan_cipher_destroy(c);
  return (rc == 0) ? (uint32_t)written : 0;
#else
  (void)key; (void)key_len; (void)nonce; (void)nonce_len;
  (void)data; (void)data_len; (void)aad; (void)aad_len;
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * ChaCha20-Poly1305 AEAD Decrypt
 * =================================================================== */

uint32_t zerox_crypto_chacha20_poly1305_decrypt(
    const unsigned char *key, size_t key_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *data, size_t data_len,
    const unsigned char *aad, size_t aad_len,
    unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  botan_cipher_t c = NULL;
  if (botan_cipher_init(&c, "ChaCha20Poly1305",
                         BOTAN_CIPHER_INIT_FLAG_DECRYPT) != 0) return 0;
  int rc = botan_cipher_set_key(c, key, key_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  if (aad && aad_len > 0) {
    rc = botan_cipher_set_associated_data(c, aad, aad_len);
    if (rc != 0) { botan_cipher_destroy(c); return 0; }
  }
  rc = botan_cipher_start(c, nonce, nonce_len);
  if (rc != 0) { botan_cipher_destroy(c); return 0; }
  size_t written = 0;
  rc = botan_cipher_update(c, BOTAN_CIPHER_UPDATE_FLAG_FINAL,
                           out, out_cap, &written,
                           data, data_len);
  botan_cipher_destroy(c);
  return (rc == 0) ? (uint32_t)written : 0;
#else
  (void)key; (void)key_len; (void)nonce; (void)nonce_len;
  (void)data; (void)data_len; (void)aad; (void)aad_len;
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Random bytes
 * =================================================================== */

uint32_t zerox_crypto_random_bytes(unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  if (!out || out_cap == 0) return 0;
  int rc = botan_system_rng_get(out, out_cap);
  return (rc == 0) ? (uint32_t)out_cap : 0;
#else
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * DES Encrypt
 * =================================================================== */

uint32_t zerox_crypto_des_encrypt(const unsigned char *key, size_t key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t mode,
                                  unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return legacy_block_op("DES", key, key_len, data, data_len,
                         mode, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)data; (void)data_len;
  (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * DES Decrypt
 * =================================================================== */

uint32_t zerox_crypto_des_decrypt(const unsigned char *key, size_t key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t mode,
                                  unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return legacy_block_op("DES", key, key_len, data, data_len,
                         mode, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)data; (void)data_len;
  (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Triple-DES (3DES/TDEA) Encrypt
 * =================================================================== */

uint32_t zerox_crypto_tdes_encrypt(const unsigned char *key, size_t key_len,
                                   const unsigned char *data, size_t data_len,
                                   uint32_t mode,
                                   unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return legacy_block_op("TripleDES", key, key_len, data, data_len,
                         mode, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)data; (void)data_len;
  (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Triple-DES (3DES/TDEA) Decrypt
 * =================================================================== */

uint32_t zerox_crypto_tdes_decrypt(const unsigned char *key, size_t key_len,
                                   const unsigned char *data, size_t data_len,
                                   uint32_t mode,
                                   unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return legacy_block_op("TripleDES", key, key_len, data, data_len,
                         mode, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)data; (void)data_len;
  (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Blowfish Encrypt
 * =================================================================== */

uint32_t zerox_crypto_blowfish_encrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return iv_block_op("Blowfish", key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Blowfish Decrypt
 * =================================================================== */

uint32_t zerox_crypto_blowfish_decrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return iv_block_op("Blowfish", key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Twofish Encrypt
 * =================================================================== */

uint32_t zerox_crypto_twofish_encrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return iv_block_op("Twofish", key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Twofish Decrypt
 * =================================================================== */

uint32_t zerox_crypto_twofish_decrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return iv_block_op("Twofish", key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Serpent Encrypt
 * =================================================================== */

uint32_t zerox_crypto_serpent_encrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return iv_block_op("Serpent", key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Serpent Decrypt
 * =================================================================== */

uint32_t zerox_crypto_serpent_decrypt(const unsigned char *key, size_t key_len,
                                      const unsigned char *iv, size_t iv_len,
                                      const unsigned char *data, size_t data_len,
                                      uint32_t mode,
                                      unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  return iv_block_op("Serpent", key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Camellia Encrypt
 * =================================================================== */

uint32_t zerox_crypto_camellia_encrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  const char *base = camellia_base(key_len);
  if (!base) return 0;
  return iv_block_op(base, key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 1);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Camellia Decrypt
 * =================================================================== */

uint32_t zerox_crypto_camellia_decrypt(const unsigned char *key, size_t key_len,
                                       const unsigned char *iv, size_t iv_len,
                                       const unsigned char *data, size_t data_len,
                                       uint32_t mode,
                                       unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  const char *base = camellia_base(key_len);
  if (!base) return 0;
  return iv_block_op(base, key, key_len, iv, iv_len,
                     data, data_len, mode, out, out_cap, 0);
#else
  (void)key; (void)key_len; (void)iv; (void)iv_len;
  (void)data; (void)data_len; (void)mode; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * RSA Generate Keypair
 * =================================================================== */

uint32_t zerox_crypto_rsa_generate_keypair(uint32_t key_bits,
                                           unsigned char *pub_out, size_t pub_cap,
                                           unsigned char *priv_out, size_t priv_cap) {
#ifdef HAVE_BOTAN
  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) return 0;

  botan_privkey_t key = NULL;
  int rc = botan_privkey_create_rsa(&key, rng, key_bits, 65537);
  if (rc != 0) { botan_rng_destroy(rng); return 0; }

  /* Export private key as DER */
  size_t priv_len = priv_cap;
  rc = botan_privkey_export(key, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            priv_out, &priv_len);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  /* Export public key as DER */
  botan_pubkey_t pub = NULL;
  rc = botan_privkey_export_pubkey(&pub, key);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  size_t pub_len = pub_cap;
  rc = botan_pubkey_export(pub, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            pub_out, &pub_len);
  botan_pubkey_destroy(pub);
  botan_privkey_destroy(key);
  botan_rng_destroy(rng);

  return (rc == 0) ? (uint32_t)priv_len : 0;
#else
  (void)key_bits; (void)pub_out; (void)pub_cap;
  (void)priv_out; (void)priv_cap;
  return 0;
#endif
}

/* ---------- RSA padding helpers ---------- */

static const char *rsa_padding_encrypt(uint32_t padding) {
  switch (padding) {
    case 0: return "PKCS1v15";
    case 1: return "OAEP(SHA-256)";
    default: return "OAEP(SHA-256)";
  }
}

static const char *rsa_padding_sign(uint32_t padding) {
  switch (padding) {
    case 0: return "EMSA3(SHA-256)";
    case 1: return "EMSA4(SHA-256)";
    case 2: return "EMSA4(SHA-256)";
    default: return "EMSA4(SHA-256)";
  }
}

/* ===================================================================
 * RSA Encrypt
 * =================================================================== */

uint32_t zerox_crypto_rsa_encrypt(const unsigned char *pub_key, size_t pub_key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t padding,
                                  unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  botan_pubkey_t key = NULL;
  if (botan_pubkey_load(&key, pub_key, pub_key_len) != 0) return 0;

  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) {
    botan_pubkey_destroy(key);
    return 0;
  }

  botan_pk_op_encrypt_t enc = NULL;
  const char *pad = rsa_padding_encrypt(padding);
  int rc = botan_pk_op_encrypt_create(&enc, key, pad, 0);
  if (rc != 0) { botan_rng_destroy(rng); botan_pubkey_destroy(key); return 0; }

  size_t written = 0;
  rc = botan_pk_op_encrypt(enc, rng, out, &written, out_cap, data, data_len);

  botan_pk_op_encrypt_destroy(enc);
  botan_rng_destroy(rng);
  botan_pubkey_destroy(key);

  return (rc == 0) ? (uint32_t)written : 0;
#else
  (void)pub_key; (void)pub_key_len; (void)data; (void)data_len;
  (void)padding; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * RSA Decrypt
 * =================================================================== */

uint32_t zerox_crypto_rsa_decrypt(const unsigned char *priv_key, size_t priv_key_len,
                                  const unsigned char *data, size_t data_len,
                                  uint32_t padding,
                                  unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  botan_privkey_t key = NULL;
  if (botan_privkey_load(&key, priv_key, priv_key_len) != 0) return 0;

  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) {
    botan_privkey_destroy(key);
    return 0;
  }

  botan_pk_op_decrypt_t dec = NULL;
  const char *pad = rsa_padding_encrypt(padding);
  int rc = botan_pk_op_decrypt_create(&dec, key, pad, 0);
  if (rc != 0) { botan_rng_destroy(rng); botan_privkey_destroy(key); return 0; }

  size_t written = 0;
  rc = botan_pk_op_decrypt(dec, out, &written, out_cap, data, data_len);

  botan_pk_op_decrypt_destroy(dec);
  botan_rng_destroy(rng);
  botan_privkey_destroy(key);

  return (rc == 0) ? (uint32_t)written : 0;
#else
  (void)priv_key; (void)priv_key_len; (void)data; (void)data_len;
  (void)padding; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * RSA Sign
 * =================================================================== */

uint32_t zerox_crypto_rsa_sign(const unsigned char *priv_key, size_t priv_key_len,
                               const unsigned char *data, size_t data_len,
                               uint32_t hash_algo, uint32_t padding,
                               unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  (void)hash_algo;

  botan_privkey_t key = NULL;
  if (botan_privkey_load(&key, priv_key, priv_key_len) != 0) return 0;

  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) {
    botan_privkey_destroy(key);
    return 0;
  }

  botan_pk_op_sign_t sig = NULL;
  const char *pad = rsa_padding_sign(padding);
  int rc = botan_pk_op_sign_create(&sig, key, pad, 0);
  if (rc != 0) { botan_rng_destroy(rng); botan_privkey_destroy(key); return 0; }

  rc = botan_pk_op_sign_update(sig, data, data_len);
  if (rc != 0) { botan_pk_op_sign_destroy(sig); botan_rng_destroy(rng); botan_privkey_destroy(key); return 0; }

  size_t sig_len = out_cap;
  rc = botan_pk_op_sign_finish(sig, rng, out, &sig_len);

  botan_pk_op_sign_destroy(sig);
  botan_rng_destroy(rng);
  botan_privkey_destroy(key);

  return (rc == 0) ? (uint32_t)sig_len : 0;
#else
  (void)priv_key; (void)priv_key_len; (void)data; (void)data_len;
  (void)hash_algo; (void)padding; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * RSA Verify
 * =================================================================== */

int zerox_crypto_rsa_verify(const unsigned char *pub_key, size_t pub_key_len,
                           const unsigned char *data, size_t data_len,
                           const unsigned char *sig, size_t sig_len,
                           uint32_t hash_algo, uint32_t padding) {
#ifdef HAVE_BOTAN
  (void)hash_algo;

  botan_pubkey_t key = NULL;
  if (botan_pubkey_load(&key, pub_key, pub_key_len) != 0) return -1;

  botan_pk_op_verify_t ver = NULL;
  const char *pad = rsa_padding_sign(padding);
  int rc = botan_pk_op_verify_create(&ver, key, pad, 0);
  if (rc != 0) { botan_pubkey_destroy(key); return -1; }

  rc = botan_pk_op_verify_update(ver, data, data_len);
  if (rc != 0) { botan_pk_op_verify_destroy(ver); botan_pubkey_destroy(key); return -1; }

  rc = botan_pk_op_verify_finish(ver, sig, sig_len);

  botan_pk_op_verify_destroy(ver);
  botan_pubkey_destroy(key);

  return (rc == 0) ? 1 : 0;
#else
  (void)pub_key; (void)pub_key_len; (void)data; (void)data_len;
  (void)sig; (void)sig_len; (void)hash_algo; (void)padding;
  return -1;
#endif
}

/* ===================================================================
 * ECC Generate Keypair
 * =================================================================== */

uint32_t zerox_crypto_ecc_generate_keypair(uint32_t curve,
                                           unsigned char *pub_out, size_t pub_cap,
                                           unsigned char *priv_out, size_t priv_cap) {
#ifdef HAVE_BOTAN
  (void)curve;

  /* Map curve constant to Botan curve name */
  const char *curve_name;
  switch (curve) {
    case 0:  curve_name = "secp256r1"; break;
    case 1:  curve_name = "secp384r1"; break;
    case 2:  curve_name = "secp521r1"; break;
    default: curve_name = "secp256r1"; break;
  }

  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) return 0;

  botan_privkey_t key = NULL;
  int rc = botan_privkey_create_ecdsa(&key, rng, curve_name);
  if (rc != 0) { botan_rng_destroy(rng); return 0; }

  /* Export private key as DER */
  size_t priv_len = priv_cap;
  rc = botan_privkey_export(key, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            priv_out, &priv_len);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  /* Export public key */
  botan_pubkey_t pub = NULL;
  rc = botan_privkey_export_pubkey(&pub, key);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  size_t pub_len = pub_cap;
  rc = botan_pubkey_export(pub, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            pub_out, &pub_len);
  botan_pubkey_destroy(pub);
  botan_privkey_destroy(key);
  botan_rng_destroy(rng);

  return (rc == 0) ? (uint32_t)pub_len : 0;
#else
  (void)curve; (void)pub_out; (void)pub_cap;
  (void)priv_out; (void)priv_cap;
  return 0;
#endif
}

/* ===================================================================
 * ECC Sign (ECDSA)
 * =================================================================== */

uint32_t zerox_crypto_ecc_sign(const unsigned char *priv_key, size_t priv_key_len,
                               const unsigned char *data, size_t data_len,
                               uint32_t curve,
                               unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  (void)curve;

  botan_privkey_t key = NULL;
  if (botan_privkey_load(&key, priv_key, priv_key_len) != 0) return 0;

  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) {
    botan_privkey_destroy(key);
    return 0;
  }

  botan_pk_op_sign_t sig = NULL;
  int rc = botan_pk_op_sign_create(&sig, key, "DER", 0); /* ECDSA uses DER signature encoding */
  if (rc != 0) { botan_rng_destroy(rng); botan_privkey_destroy(key); return 0; }

  rc = botan_pk_op_sign_update(sig, data, data_len);
  if (rc != 0) { botan_pk_op_sign_destroy(sig); botan_rng_destroy(rng); botan_privkey_destroy(key); return 0; }

  size_t sig_len = out_cap;
  rc = botan_pk_op_sign_finish(sig, rng, out, &sig_len);

  botan_pk_op_sign_destroy(sig);
  botan_rng_destroy(rng);
  botan_privkey_destroy(key);

  return (rc == 0) ? (uint32_t)sig_len : 0;
#else
  (void)priv_key; (void)priv_key_len; (void)data; (void)data_len;
  (void)curve; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * ECC Verify (ECDSA)
 * =================================================================== */

int zerox_crypto_ecc_verify(const unsigned char *pub_key, size_t pub_key_len,
                           const unsigned char *data, size_t data_len,
                           const unsigned char *sig, size_t sig_len,
                           uint32_t curve) {
#ifdef HAVE_BOTAN
  (void)curve;

  botan_pubkey_t key = NULL;
  if (botan_pubkey_load(&key, pub_key, pub_key_len) != 0) return -1;

  botan_pk_op_verify_t ver = NULL;
  int rc = botan_pk_op_verify_create(&ver, key, "DER", 0);
  if (rc != 0) { botan_pubkey_destroy(key); return -1; }

  rc = botan_pk_op_verify_update(ver, data, data_len);
  if (rc != 0) { botan_pk_op_verify_destroy(ver); botan_pubkey_destroy(key); return -1; }

  rc = botan_pk_op_verify_finish(ver, sig, sig_len);

  botan_pk_op_verify_destroy(ver);
  botan_pubkey_destroy(key);

  return (rc == 0) ? 1 : 0;
#else
  (void)pub_key; (void)pub_key_len; (void)data; (void)data_len;
  (void)sig; (void)sig_len; (void)curve;
  return -1;
#endif
}

/* ===================================================================
 * ECC ECDH
 * =================================================================== */

uint32_t zerox_crypto_ecc_ecdh(const unsigned char *priv_key, size_t priv_key_len,
                               const unsigned char *peer_public, size_t peer_public_len,
                               uint32_t curve,
                               unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  (void)curve;

  botan_privkey_t key = NULL;
  if (botan_privkey_load(&key, priv_key, priv_key_len) != 0) return 0;

  botan_pubkey_t peer = NULL;
  if (botan_pubkey_load(&peer, peer_public, peer_public_len) != 0) {
    botan_privkey_destroy(key);
    return 0;
  }

  botan_pk_op_ka_t ka = NULL;
  int rc = botan_pk_op_key_agreement_create(&ka, key, "KDF2(SHA-256)", 0);
  if (rc != 0) { botan_pubkey_destroy(peer); botan_privkey_destroy(key); return 0; }

  size_t shared_len = out_cap;
  rc = botan_pk_op_key_agreement(ka, out, &shared_len, peer, peer_public_len, NULL, 0);

  botan_pk_op_key_agreement_destroy(ka);
  botan_pubkey_destroy(peer);
  botan_privkey_destroy(key);

  return (rc == 0) ? (uint32_t)shared_len : 0;
#else
  (void)priv_key; (void)priv_key_len; (void)peer_public; (void)peer_public_len;
  (void)curve; (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Ed25519 Sign
 * =================================================================== */

uint32_t zerox_crypto_ecc_ed25519_sign(const unsigned char *priv_key, size_t priv_key_len,
                                       const unsigned char *data, size_t data_len,
                                       unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  botan_privkey_t key = NULL;
  /* Ed25519 private key is 32 bytes raw */
  if (botan_privkey_load_ed25519(&key, priv_key, priv_key_len) != 0) return 0;

  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) {
    botan_privkey_destroy(key);
    return 0;
  }

  botan_pk_op_sign_t sig = NULL;
  int rc = botan_pk_op_sign_create(&sig, key, "Pure", 0);
  if (rc != 0) { botan_rng_destroy(rng); botan_privkey_destroy(key); return 0; }

  rc = botan_pk_op_sign_update(sig, data, data_len);
  if (rc != 0) { botan_pk_op_sign_destroy(sig); botan_rng_destroy(rng); botan_privkey_destroy(key); return 0; }

  size_t sig_len = out_cap;
  rc = botan_pk_op_sign_finish(sig, rng, out, &sig_len);

  botan_pk_op_sign_destroy(sig);
  botan_rng_destroy(rng);
  botan_privkey_destroy(key);

  return (rc == 0) ? (uint32_t)sig_len : 0;
#else
  (void)priv_key; (void)priv_key_len; (void)data; (void)data_len;
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * Ed25519 Verify
 * =================================================================== */

int zerox_crypto_ecc_ed25519_verify(const unsigned char *pub_key, size_t pub_key_len,
                                   const unsigned char *data, size_t data_len,
                                   const unsigned char *sig, size_t sig_len) {
#ifdef HAVE_BOTAN
  botan_pubkey_t key = NULL;
  if (botan_pubkey_load_ed25519(&key, pub_key, pub_key_len) != 0) return -1;

  botan_pk_op_verify_t ver = NULL;
  int rc = botan_pk_op_verify_create(&ver, key, "Pure", 0);
  if (rc != 0) { botan_pubkey_destroy(key); return -1; }

  rc = botan_pk_op_verify_update(ver, data, data_len);
  if (rc != 0) { botan_pk_op_verify_destroy(ver); botan_pubkey_destroy(key); return -1; }

  rc = botan_pk_op_verify_finish(ver, sig, sig_len);

  botan_pk_op_verify_destroy(ver);
  botan_pubkey_destroy(key);

  return (rc == 0) ? 1 : 0;
#else
  (void)pub_key; (void)pub_key_len; (void)data; (void)data_len;
  (void)sig; (void)sig_len;
  return -1;
#endif
}

/* ===================================================================
 * Ed25519 Generate Keypair
 * =================================================================== */

uint32_t zerox_crypto_ecc_ed25519_generate_keypair(
    unsigned char *pub_out, size_t pub_cap,
    unsigned char *priv_out, size_t priv_cap) {
#ifdef HAVE_BOTAN
  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) return 0;

  botan_privkey_t key = NULL;
  int rc = botan_privkey_create_ed25519(&key, rng);
  if (rc != 0) { botan_rng_destroy(rng); return 0; }

  /* Export private key */
  size_t priv_len = priv_cap;
  rc = botan_privkey_export(key, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            priv_out, &priv_len);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  /* Export public key */
  botan_pubkey_t pub = NULL;
  rc = botan_privkey_export_pubkey(&pub, key);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  size_t pub_len = pub_cap;
  rc = botan_pubkey_export(pub, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            pub_out, &pub_len);
  botan_pubkey_destroy(pub);
  botan_privkey_destroy(key);
  botan_rng_destroy(rng);

  return (rc == 0) ? (uint32_t)pub_len : 0;
#else
  (void)pub_out; (void)pub_cap; (void)priv_out; (void)priv_cap;
  return 0;
#endif
}

/* ===================================================================
 * X25519 ECDH (Curve25519 Key Agreement)
 * =================================================================== */

uint32_t zerox_crypto_ecc_x25519_ecdh(
    const unsigned char *priv_key, size_t priv_key_len,
    const unsigned char *peer_public, size_t peer_public_len,
    unsigned char *out, size_t out_cap) {
#ifdef HAVE_BOTAN
  /* Load X25519 private key (DER-encoded, as exported by keygen) */
  botan_privkey_t key = NULL;
  int rc = botan_privkey_load(&key, priv_key, priv_key_len);
  if (rc != 0) return 0;

  /* Load peer's public key */
  botan_pubkey_t peer = NULL;
  rc = botan_pubkey_load(&peer, peer_public, peer_public_len);
  if (rc != 0) { botan_privkey_destroy(key); return 0; }

  /* Perform key agreement */
  botan_pk_op_ka_t ka = NULL;
  rc = botan_pk_op_key_agreement_create(&ka, key, "KDF2(SHA-256)", 0);
  if (rc != 0) { botan_pubkey_destroy(peer); botan_privkey_destroy(key); return 0; }

  size_t shared_len = out_cap;
  rc = botan_pk_op_key_agreement(ka, out, &shared_len, peer, peer_public_len, NULL, 0);

  botan_pk_op_key_agreement_destroy(ka);
  botan_pubkey_destroy(peer);
  botan_privkey_destroy(key);

  return (rc == 0) ? (uint32_t)shared_len : 0;
#else
  (void)priv_key; (void)priv_key_len; (void)peer_public; (void)peer_public_len;
  (void)out; (void)out_cap;
  return 0;
#endif
}

/* ===================================================================
 * X25519 Generate Keypair
 * =================================================================== */

uint32_t zerox_crypto_ecc_x25519_generate_keypair(
    unsigned char *pub_out, size_t pub_cap,
    unsigned char *priv_out, size_t priv_cap) {
#ifdef HAVE_BOTAN
  botan_rng_t rng = NULL;
  if (botan_rng_init(&rng, "system", 0) != 0) return 0;

  botan_privkey_t key = NULL;
  int rc = botan_privkey_create_x25519(&key, rng);
  if (rc != 0) { botan_rng_destroy(rng); return 0; }

  /* Export private key as DER */
  size_t priv_len = priv_cap;
  rc = botan_privkey_export(key, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            priv_out, &priv_len);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  /* Export public key */
  botan_pubkey_t pub = NULL;
  rc = botan_privkey_export_pubkey(&pub, key);
  if (rc != 0) { botan_privkey_destroy(key); botan_rng_destroy(rng); return 0; }

  size_t pub_len = pub_cap;
  rc = botan_pubkey_export(pub, BOTAN_PRIVKEY_EXPORT_FLAG_DER,
                            pub_out, &pub_len);
  botan_pubkey_destroy(pub);
  botan_privkey_destroy(key);
  botan_rng_destroy(rng);

  return (rc == 0) ? (uint32_t)pub_len : 0;
#else
  (void)pub_out; (void)pub_cap; (void)priv_out; (void)priv_cap;
  return 0;
#endif
}
