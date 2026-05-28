#ifndef ZERO_C_COFF_EMIT_STATE_H
#define ZERO_C_COFF_EMIT_STATE_H

#include "zerox.h"
#include "coff_format.h"

typedef enum {
  COFF_RUNTIME_WORLD_WRITE,
  COFF_RUNTIME_CRYPTO_SHA256,
  COFF_RUNTIME_CRYPTO_HMAC_SHA256,
  COFF_RUNTIME_CRYPTO_AES_ENCRYPT,
  COFF_RUNTIME_CRYPTO_AES_DECRYPT,
  COFF_RUNTIME_CRYPTO_CHACHA20,
  COFF_RUNTIME_CRYPTO_PBKDF2,
  COFF_RUNTIME_CRYPTO_RANDOM_BYTES,
  COFF_RUNTIME_CRYPTO_SHA512,
  COFF_RUNTIME_CRYPTO_SALSA20,
  COFF_RUNTIME_CRYPTO_CHACHA20_POLY1305_ENCRYPT,
  COFF_RUNTIME_CRYPTO_CHACHA20_POLY1305_DECRYPT,
  COFF_RUNTIME_CRYPTO_DES_ENCRYPT,
  COFF_RUNTIME_CRYPTO_DES_DECRYPT,
  COFF_RUNTIME_CRYPTO_TDES_ENCRYPT,
  COFF_RUNTIME_CRYPTO_TDES_DECRYPT,
  COFF_RUNTIME_CRYPTO_BLOWFISH_ENCRYPT,
  COFF_RUNTIME_CRYPTO_BLOWFISH_DECRYPT,
  COFF_RUNTIME_CRYPTO_TWOFISH_ENCRYPT,
  COFF_RUNTIME_CRYPTO_TWOFISH_DECRYPT,
  COFF_RUNTIME_CRYPTO_SERPENT_ENCRYPT,
  COFF_RUNTIME_CRYPTO_SERPENT_DECRYPT,
  COFF_RUNTIME_CRYPTO_CAMELLIA_ENCRYPT,
  COFF_RUNTIME_CRYPTO_CAMELLIA_DECRYPT,
  COFF_RUNTIME_CRYPTO_RSA_GENERATE_KEYPAIR,
  COFF_RUNTIME_CRYPTO_RSA_ENCRYPT,
  COFF_RUNTIME_CRYPTO_RSA_DECRYPT,
  COFF_RUNTIME_CRYPTO_RSA_SIGN,
  COFF_RUNTIME_CRYPTO_RSA_VERIFY,
  COFF_RUNTIME_CRYPTO_ECC_GENERATE_KEYPAIR,
  COFF_RUNTIME_CRYPTO_ECC_SIGN,
  COFF_RUNTIME_CRYPTO_ECC_VERIFY,
  COFF_RUNTIME_CRYPTO_ECC_ECDH,
  COFF_RUNTIME_CRYPTO_ECC_ED25519_SIGN,
  COFF_RUNTIME_CRYPTO_ECC_ED25519_VERIFY,
  COFF_RUNTIME_CRYPTO_SHA384,
  COFF_RUNTIME_CRYPTO_SHA3_256,
  COFF_RUNTIME_CRYPTO_SHA3_512,
  COFF_RUNTIME_CRYPTO_BLAKE2B,
  COFF_RUNTIME_CRYPTO_BLAKE2S,
  COFF_RUNTIME_CRYPTO_HMAC_SHA384,
  COFF_RUNTIME_CRYPTO_SHA3_384,
  COFF_RUNTIME_CRYPTO_SHAKE128,
  COFF_RUNTIME_CRYPTO_SHAKE256,
  COFF_RUNTIME_CRYPTO_HMAC_SHA512,
  COFF_RUNTIME_CRYPTO_ECC_ED25519_GENERATE_KEYPAIR,
  COFF_RUNTIME_CRYPTO_ECC_X25519_ECDH,
  COFF_RUNTIME_CRYPTO_AES_GCM_ENCRYPT,
  COFF_RUNTIME_CRYPTO_AES_GCM_DECRYPT,
  COFF_RUNTIME_CRYPTO_ECC_X25519_GENERATE_KEYPAIR,
  COFF_RUNTIME_HELPER_COUNT
} CoffRuntimeHelper;

typedef struct {
  size_t patch_offset;
} CoffPatch;

typedef struct {
  CoffPatch *items;
  size_t len;
  size_t cap;
} CoffPatchList;

typedef struct {
  size_t patch_offset;
  unsigned callee_index;
} CoffCallPatch;

typedef struct {
  const IrProgram *program;
  size_t *function_offsets;
  size_t function_count;
  CoffCallPatch *call_patches;
  size_t call_patch_len;
  size_t call_patch_cap;
  ZCoffImageDataPatch *rodata_patches;
  size_t rodata_patch_len;
  size_t rodata_patch_cap;
  CoffPatchList runtime_patches[COFF_RUNTIME_HELPER_COUNT];
  unsigned rodata_base_offset;
} CoffEmitContext;

const char *z_coff_runtime_helper_symbol(CoffRuntimeHelper helper);
void z_coff_emit_context_free(CoffEmitContext *ctx);
bool z_coff_record_call_patch(CoffEmitContext *ctx, size_t patch_offset, unsigned callee_index, const IrValue *value, ZDiag *diag);
bool z_coff_record_rodata_patch(CoffEmitContext *ctx, size_t patch_offset, unsigned data_offset, const IrValue *value, ZDiag *diag);
bool z_coff_record_instr_runtime_patch(CoffEmitContext *ctx, CoffRuntimeHelper helper, size_t patch_offset, const IrInstr *instr, ZDiag *diag);
size_t z_coff_runtime_patch_count(const CoffEmitContext *ctx, CoffRuntimeHelper helper);
size_t z_coff_text_relocation_count(const CoffEmitContext *ctx);
void z_coff_patch_call_patches(ZBuf *text, const CoffEmitContext *ctx);
void z_coff_patch_runtime_patches(ZBuf *text, const CoffEmitContext *ctx, CoffRuntimeHelper helper, size_t target_offset);
void z_coff_append_call_relocations(ZBuf *relocs, const CoffEmitContext *ctx, uint32_t function_symbol_base);
void z_coff_append_rodata_relocations(ZBuf *relocs, const CoffEmitContext *ctx, uint32_t rodata_symbol);
void z_coff_append_runtime_relocations(ZBuf *relocs, const CoffEmitContext *ctx, CoffRuntimeHelper helper, uint32_t runtime_symbol);

#endif
