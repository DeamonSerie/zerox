#ifndef ZERO_C_VOID_EMIT_STATE_H
#define ZERO_C_VOID_EMIT_STATE_H

#include "zerox.h"

#include <stdint.h>

typedef enum {
  VOID_RUNTIME_WORLD_WRITE,
  VOID_RUNTIME_JSON_PARSE_BYTES,
  VOID_RUNTIME_HTTP_FETCH,
  VOID_RUNTIME_HTTP_RESULT_OK,
  VOID_RUNTIME_HTTP_RESULT_STATUS,
  VOID_RUNTIME_HTTP_RESULT_BODY_LEN,
  VOID_RUNTIME_HTTP_RESULT_ERROR,
  VOID_RUNTIME_HTTP_RESPONSE_LEN,
  VOID_RUNTIME_HTTP_RESPONSE_HEADERS_LEN,
  VOID_RUNTIME_HTTP_RESPONSE_BODY_OFFSET,
  VOID_RUNTIME_HTTP_HEADER_VALUE,
  VOID_RUNTIME_HTTP_HEADER_FOUND,
  VOID_RUNTIME_HTTP_HEADER_OFFSET,
  VOID_RUNTIME_HTTP_HEADER_LEN,
  VOID_RUNTIME_CRYPTO_SHA256,
  VOID_RUNTIME_CRYPTO_HMAC_SHA256,
  VOID_RUNTIME_CRYPTO_AES_ENCRYPT,
  VOID_RUNTIME_CRYPTO_AES_DECRYPT,
  VOID_RUNTIME_CRYPTO_CHACHA20,
  VOID_RUNTIME_CRYPTO_PBKDF2,
  VOID_RUNTIME_CRYPTO_RANDOM_BYTES,
  VOID_RUNTIME_CRYPTO_SHA512,
  VOID_RUNTIME_CRYPTO_SALSA20,
  VOID_RUNTIME_CRYPTO_CHACHA20_POLY1305_ENCRYPT,
  VOID_RUNTIME_CRYPTO_CHACHA20_POLY1305_DECRYPT,
  VOID_RUNTIME_CRYPTO_DES_ENCRYPT,
  VOID_RUNTIME_CRYPTO_DES_DECRYPT,
  VOID_RUNTIME_CRYPTO_TDES_ENCRYPT,
  VOID_RUNTIME_CRYPTO_TDES_DECRYPT,
  VOID_RUNTIME_CRYPTO_BLOWFISH_ENCRYPT,
  VOID_RUNTIME_CRYPTO_BLOWFISH_DECRYPT,
  VOID_RUNTIME_CRYPTO_TWOFISH_ENCRYPT,
  VOID_RUNTIME_CRYPTO_TWOFISH_DECRYPT,
  VOID_RUNTIME_CRYPTO_SERPENT_ENCRYPT,
  VOID_RUNTIME_CRYPTO_SERPENT_DECRYPT,
  VOID_RUNTIME_CRYPTO_CAMELLIA_ENCRYPT,
  VOID_RUNTIME_CRYPTO_CAMELLIA_DECRYPT,
  VOID_RUNTIME_CRYPTO_RSA_GENERATE_KEYPAIR,
  VOID_RUNTIME_CRYPTO_RSA_ENCRYPT,
  VOID_RUNTIME_CRYPTO_RSA_DECRYPT,
  VOID_RUNTIME_CRYPTO_RSA_SIGN,
  VOID_RUNTIME_CRYPTO_RSA_VERIFY,
  VOID_RUNTIME_CRYPTO_ECC_GENERATE_KEYPAIR,
  VOID_RUNTIME_CRYPTO_ECC_SIGN,
  VOID_RUNTIME_CRYPTO_ECC_VERIFY,
  VOID_RUNTIME_CRYPTO_ECC_ECDH,
  VOID_RUNTIME_CRYPTO_ECC_ED25519_SIGN,
  VOID_RUNTIME_CRYPTO_ECC_ED25519_VERIFY,
  VOID_RUNTIME_CRYPTO_SHA384,
  VOID_RUNTIME_CRYPTO_SHA3_256,
  VOID_RUNTIME_CRYPTO_SHA3_512,
  VOID_RUNTIME_CRYPTO_BLAKE2B,
  VOID_RUNTIME_CRYPTO_BLAKE2S,
  VOID_RUNTIME_CRYPTO_HMAC_SHA384,
  VOID_RUNTIME_CRYPTO_SHA3_384,
  VOID_RUNTIME_CRYPTO_SHAKE128,
  VOID_RUNTIME_CRYPTO_SHAKE256,
  VOID_RUNTIME_CRYPTO_HMAC_SHA512,
  VOID_RUNTIME_CRYPTO_ECC_ED25519_GENERATE_KEYPAIR,
  VOID_RUNTIME_CRYPTO_ECC_X25519_ECDH,
  VOID_RUNTIME_CRYPTO_AES_GCM_ENCRYPT,
  VOID_RUNTIME_CRYPTO_AES_GCM_DECRYPT,
  VOID_RUNTIME_CRYPTO_ECC_X25519_GENERATE_KEYPAIR,
  VOID_RUNTIME_HELPER_COUNT
} VoidRuntimeHelper;

typedef struct {
  size_t patch_offset;
} VoidPatch;

typedef struct {
  VoidPatch *items;
  size_t len;
  size_t cap;
} VoidPatchList;

typedef struct {
  size_t patch_offset;
  unsigned callee_index;
  int line;
  int column;
} VoidCallPatch;

typedef struct {
  size_t patch_offset;
  unsigned data_offset;
} VoidDataPatch;

typedef struct {
  const IrProgram *program;
  size_t *function_offsets;
  size_t function_count;
  VoidCallPatch *call_patches;
  size_t call_patch_len;
  size_t call_patch_cap;
  VoidDataPatch *data_patches;
  size_t data_patch_len;
  size_t data_patch_cap;
  VoidPatchList runtime_patches[VOID_RUNTIME_HELPER_COUNT];
  unsigned rodata_base_offset;
  bool pie_relative_data;
  bool seed_main_process_args;
} VoidEmitContext;

const char *z_void_runtime_helper_symbol(VoidRuntimeHelper helper);
void z_void_emit_context_free(VoidEmitContext *ctx);
bool z_void_record_call_patch(VoidEmitContext *ctx, size_t patch_offset, unsigned callee_index, const IrValue *value, ZDiag *diag);
bool z_void_record_data_patch(VoidEmitContext *ctx, size_t patch_offset, unsigned data_offset, const IrValue *value, ZDiag *diag);
bool z_void_record_value_runtime_patch(VoidEmitContext *ctx, VoidRuntimeHelper helper, size_t patch_offset, const IrValue *value, ZDiag *diag);
bool z_void_record_instr_runtime_patch(VoidEmitContext *ctx, VoidRuntimeHelper helper, size_t patch_offset, const IrInstr *instr, ZDiag *diag);
size_t z_void_runtime_patch_count(const VoidEmitContext *ctx, VoidRuntimeHelper helper);
const VoidPatchList *z_void_runtime_patch_list(const VoidEmitContext *ctx, VoidRuntimeHelper helper);
bool z_void_has_unsupported_exe_runtime_patches(const VoidEmitContext *ctx);
void z_void_append_call_relocations(ZBuf *relocs, const VoidEmitContext *ctx);
void z_void_append_runtime_relocations(ZBuf *relocs, const VoidEmitContext *ctx, VoidRuntimeHelper helper, unsigned symbol_index);
size_t z_void_data_relocation_count(const VoidEmitContext *ctx);
size_t z_void_text_relocation_count(const VoidEmitContext *ctx);
void z_void_append_data_relocations(ZBuf *relocs, const VoidEmitContext *ctx, unsigned data_symbol_index);

#endif
