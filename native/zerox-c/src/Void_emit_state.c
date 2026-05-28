#include "Void_emit_state.h"
#include "Void_format.h"

#include <stdio.h>
#include <stdlib.h>

static const char *const runtime_helper_symbols[VOID_RUNTIME_HELPER_COUNT] = {
  "_zerox_world_write",
  "_zerox_json_parse_bytes",
  "_zerox_http_fetch_result",
  "_zerox_http_result_ok",
  "_zerox_http_result_status",
  "_zerox_http_result_body_len",
  "_zerox_http_result_error",
  "_zerox_http_response_len",
  "_zerox_http_response_headers_len",
  "_zerox_http_response_body_offset",
  "_zerox_http_header_value",
  "_zerox_http_header_found",
  "_zerox_http_header_offset",
  "_zerox_http_header_len",
  "_zerox_crypto_sha256",
  "_zerox_crypto_hmac_sha256",
  "_zerox_crypto_aes_encrypt",
  "_zerox_crypto_aes_decrypt",
  "_zerox_crypto_chacha20",
  "_zerox_crypto_pbkdf2",
  "_zerox_crypto_random_bytes",
  "_zerox_crypto_sha512",
  "_zerox_crypto_salsa20",
  "_zerox_crypto_chacha20_poly1305_encrypt",
  "_zerox_crypto_chacha20_poly1305_decrypt",
  "_zerox_crypto_des_encrypt",
  "_zerox_crypto_des_decrypt",
  "_zerox_crypto_tdes_encrypt",
  "_zerox_crypto_tdes_decrypt",
  "_zerox_crypto_blowfish_encrypt",
  "_zerox_crypto_blowfish_decrypt",
  "_zerox_crypto_twofish_encrypt",
  "_zerox_crypto_twofish_decrypt",
  "_zerox_crypto_serpent_encrypt",
  "_zerox_crypto_serpent_decrypt",
  "_zerox_crypto_camellia_encrypt",
  "_zerox_crypto_camellia_decrypt",
  "_zerox_crypto_rsa_generate_keypair",
  "_zerox_crypto_rsa_encrypt",
  "_zerox_crypto_rsa_decrypt",
  "_zerox_crypto_rsa_sign",
  "_zerox_crypto_rsa_verify",
  "_zerox_crypto_ecc_generate_keypair",
  "_zerox_crypto_ecc_sign",
  "_zerox_crypto_ecc_verify",
  "_zerox_crypto_ecc_ecdh",
  "_zerox_crypto_ecc_ed25519_sign",
  "_zerox_crypto_ecc_ed25519_verify",
  "_zerox_crypto_sha384",
  "_zerox_crypto_sha3_256",
  "_zerox_crypto_sha3_512",
  "_zerox_crypto_blake2b",
  "_zerox_crypto_blake2s",
  "_zerox_crypto_hmac_sha384",
  "_zerox_crypto_sha3_384",
  "_zerox_crypto_shake128",
  "_zerox_crypto_shake256",
  "_zerox_crypto_hmac_sha512",
  "_zerox_crypto_ecc_ed25519_generate_keypair",
  "_zerox_crypto_ecc_x25519_ecdh",
  "_zerox_crypto_aes_gcm_encrypt",
  "_zerox_crypto_aes_gcm_decrypt",
  "_zerox_crypto_ecc_x25519_generate_keypair",
};

static bool void_emit_state_diag_at(ZDiag *diag, const char *message, int line, int column, const char *actual) {
  if (diag) {
    diag->code = 4004;
    diag->line = line > 0 ? line : 1;
    diag->column = column > 0 ? column : 1;
    diag->length = 1;
    snprintf(diag->message, sizeof(diag->message), "%s", message);
    snprintf(diag->expected, sizeof(diag->expected), "direct AArch64 Void object MVP subset");
    snprintf(diag->actual, sizeof(diag->actual), "%s", actual ? actual : "unsupported construct");
    snprintf(diag->help, sizeof(diag->help), "choose a supported direct target or restrict this program to exported no-parameter functions returning small integer literals");
  }
  return false;
}

static bool void_runtime_helper_valid(VoidRuntimeHelper helper) {
  return helper >= 0 && helper < VOID_RUNTIME_HELPER_COUNT;
}

const char *z_void_runtime_helper_symbol(VoidRuntimeHelper helper) {
  if (!void_runtime_helper_valid(helper)) return "";
  return runtime_helper_symbols[helper];
}

void z_void_emit_context_free(VoidEmitContext *ctx) {
  if (!ctx) return;
  for (unsigned i = 0; i < VOID_RUNTIME_HELPER_COUNT; i++) {
    free(ctx->runtime_patches[i].items);
  }
  free(ctx->data_patches);
  free(ctx->call_patches);
}

bool z_void_record_call_patch(VoidEmitContext *ctx, size_t patch_offset, unsigned callee_index, const IrValue *value, ZDiag *diag) {
  if (!ctx || callee_index >= ctx->function_count) {
    return void_emit_state_diag_at(diag, "direct AArch64 Void call target is out of range", value ? value->line : 1, value ? value->column : 1, "invalid callee");
  }
  if (ctx->call_patch_len == ctx->call_patch_cap) {
    ctx->call_patch_cap = z_grow_capacity(ctx->call_patch_cap, ctx->call_patch_len + 1, 8);
    ctx->call_patches = z_checked_reallocarray(ctx->call_patches, ctx->call_patch_cap, sizeof(VoidCallPatch));
  }
  ctx->call_patches[ctx->call_patch_len++] = (VoidCallPatch){.patch_offset = patch_offset, .callee_index = callee_index, .line = value ? value->line : 1, .column = value ? value->column : 1};
  return true;
}

bool z_void_record_data_patch(VoidEmitContext *ctx, size_t patch_offset, unsigned data_offset, const IrValue *value, ZDiag *diag) {
  if (!ctx) return void_emit_state_diag_at(diag, "direct AArch64 Void data relocation requires an emit context", value ? value->line : 1, value ? value->column : 1, "missing context");
  if (ctx->data_patch_len == ctx->data_patch_cap) {
    ctx->data_patch_cap = z_grow_capacity(ctx->data_patch_cap, ctx->data_patch_len + 1, 8);
    ctx->data_patches = z_checked_reallocarray(ctx->data_patches, ctx->data_patch_cap, sizeof(VoidDataPatch));
  }
  ctx->data_patches[ctx->data_patch_len++] = (VoidDataPatch){.patch_offset = patch_offset, .data_offset = data_offset};
  return true;
}

static bool void_record_runtime_patch_at(VoidEmitContext *ctx, VoidRuntimeHelper helper, size_t patch_offset, int line, int column, ZDiag *diag) {
  if (!ctx || !void_runtime_helper_valid(helper)) {
    return void_emit_state_diag_at(diag, "direct AArch64 Void runtime relocation requires an emit context", line, column, "missing context");
  }
  VoidPatchList *list = &ctx->runtime_patches[helper];
  if (list->len == list->cap) {
    list->cap = z_grow_capacity(list->cap, list->len + 1, 4);
    list->items = z_checked_reallocarray(list->items, list->cap, sizeof(VoidPatch));
  }
  list->items[list->len++] = (VoidPatch){.patch_offset = patch_offset};
  return true;
}

bool z_void_record_value_runtime_patch(VoidEmitContext *ctx, VoidRuntimeHelper helper, size_t patch_offset, const IrValue *value, ZDiag *diag) {
  return void_record_runtime_patch_at(ctx, helper, patch_offset, value ? value->line : 1, value ? value->column : 1, diag);
}

bool z_void_record_instr_runtime_patch(VoidEmitContext *ctx, VoidRuntimeHelper helper, size_t patch_offset, const IrInstr *instr, ZDiag *diag) {
  return void_record_runtime_patch_at(ctx, helper, patch_offset, instr ? instr->line : 1, instr ? instr->column : 1, diag);
}

size_t z_void_runtime_patch_count(const VoidEmitContext *ctx, VoidRuntimeHelper helper) {
  if (!ctx || !void_runtime_helper_valid(helper)) return 0;
  return ctx->runtime_patches[helper].len;
}

const VoidPatchList *z_void_runtime_patch_list(const VoidEmitContext *ctx, VoidRuntimeHelper helper) {
  if (!ctx || !void_runtime_helper_valid(helper)) return NULL;
  return &ctx->runtime_patches[helper];
}

bool z_void_has_unsupported_exe_runtime_patches(const VoidEmitContext *ctx) {
  if (!ctx) return false;
  for (unsigned i = 0; i < VOID_RUNTIME_HELPER_COUNT; i++) {
    if (i == VOID_RUNTIME_WORLD_WRITE) continue;
    if (ctx->runtime_patches[i].len > 0) return true;
  }
  return false;
}

static void void_append_branch_relocations(ZBuf *relocs, const VoidPatchList *patches, unsigned symbol_index) {
  for (size_t i = 0; patches && i < patches->len; i++) {
    uint32_t reloc_info = (symbol_index & 0x00ffffffu) |
                          (1u << 24) |
                          (2u << 25) |
                          (1u << 27) |
                          (2u << 28);
    z_void_append_u32(relocs, (uint32_t)patches->items[i].patch_offset);
    z_void_append_u32(relocs, reloc_info);
  }
}

void z_void_append_call_relocations(ZBuf *relocs, const VoidEmitContext *ctx) {
  for (size_t i = 0; ctx && i < ctx->call_patch_len; i++) {
    const VoidCallPatch *patch = &ctx->call_patches[i];
    uint32_t reloc_info = (patch->callee_index & 0x00ffffffu) |
                          (1u << 24) |
                          (2u << 25) |
                          (1u << 27) |
                          (2u << 28);
    z_void_append_u32(relocs, (uint32_t)patch->patch_offset);
    z_void_append_u32(relocs, reloc_info);
  }
}

void z_void_append_runtime_relocations(ZBuf *relocs, const VoidEmitContext *ctx, VoidRuntimeHelper helper, unsigned symbol_index) {
  void_append_branch_relocations(relocs, z_void_runtime_patch_list(ctx, helper), symbol_index);
}

size_t z_void_data_relocation_count(const VoidEmitContext *ctx) {
  if (!ctx) return 0;
  if (!ctx->pie_relative_data) return ctx->data_patch_len;
  size_t count = ctx->data_patch_len * 2;
  for (size_t i = 0; i < ctx->data_patch_len; i++) {
    const VoidDataPatch *patch = &ctx->data_patches[i];
    if (patch->data_offset != ctx->rodata_base_offset) count += 2;
  }
  return count;
}

size_t z_void_text_relocation_count(const VoidEmitContext *ctx) {
  if (!ctx) return 0;
  size_t count = ctx->call_patch_len + z_void_data_relocation_count(ctx);
  for (unsigned i = 0; i < VOID_RUNTIME_HELPER_COUNT; i++) {
    count += ctx->runtime_patches[i].len;
  }
  return count;
}

static void void_append_reloc(ZBuf *relocs, uint32_t address, uint32_t symbol_or_addend, bool pcrel, unsigned length, bool external, unsigned type) {
  uint32_t reloc_info = (symbol_or_addend & 0x00ffffffu) |
                        ((pcrel ? 1u : 0u) << 24) |
                        ((length & 3u) << 25) |
                        ((external ? 1u : 0u) << 27) |
                        ((type & 15u) << 28);
  z_void_append_u32(relocs, address);
  z_void_append_u32(relocs, reloc_info);
}

void z_void_append_data_relocations(ZBuf *relocs, const VoidEmitContext *ctx, unsigned data_symbol_index) {
  for (size_t i = 0; ctx && i < ctx->data_patch_len; i++) {
    const VoidDataPatch *patch = &ctx->data_patches[i];
    if (ctx->pie_relative_data) {
      uint32_t addend = patch->data_offset - ctx->rodata_base_offset;
      if (addend != 0) void_append_reloc(relocs, (uint32_t)patch->patch_offset + 4u, addend, false, 2, false, 10);
      void_append_reloc(relocs, (uint32_t)patch->patch_offset + 4u, data_symbol_index, false, 2, true, 4);
      if (addend != 0) void_append_reloc(relocs, (uint32_t)patch->patch_offset, addend, false, 2, false, 10);
      void_append_reloc(relocs, (uint32_t)patch->patch_offset, data_symbol_index, true, 2, true, 3);
    } else {
      void_append_reloc(relocs, (uint32_t)patch->patch_offset, data_symbol_index, false, 3, true, 0);
    }
  }
}
