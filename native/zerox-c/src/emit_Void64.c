#include "zerox.h"
#include "aarch64_emit.h"
#include "Void_emit_state.h"
#include "Void_format.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void append_u8(ZBuf *buf, unsigned value) {
  zbuf_append_char(buf, (char)(value & 0xffu));
}

static void append_bytes(ZBuf *buf, const char *bytes, size_t len);
static size_t void_align_size(size_t value, size_t alignment);

#define VOID_SCRATCH_SLOT_COUNT 32u
#define VOID_SCRATCH_SLOT_BYTES 8u

static void append_bytes(ZBuf *buf, const char *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) append_u8(buf, (unsigned char)bytes[i]);
}

static bool void_diag_at(ZDiag *diag, const char *message, int line, int column, const char *actual) {
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

static bool void_diag(ZDiag *diag, const char *message) {
  return void_diag_at(diag, message, 1, 1, "unsupported feature");
}

static bool void_return_literal(const IrFunction *fun, uint32_t *out, ZDiag *diag) {
  if (!fun || fun->param_count != 0) {
    return void_diag_at(diag, "direct AArch64 Void object backend currently supports exported functions without parameters", fun ? fun->line : 1, fun ? fun->column : 1, fun ? fun->name : "missing function");
  }
  if (fun->return_type != IR_TYPE_U8 && fun->return_type != IR_TYPE_I32 && fun->return_type != IR_TYPE_U32 && fun->return_type != IR_TYPE_USIZE) {
    return void_diag_at(diag, "direct AArch64 Void object backend currently supports primitive 32-bit-or-smaller integer returns", fun->line, fun->column, fun->name);
  }
  for (size_t i = 0; i < fun->instr_len; i++) {
    const IrInstr *instr = &fun->instrs[i];
    if (instr->kind != IR_INSTR_RETURN || !instr->value || instr->value->kind != IR_VALUE_INT || instr->value->int_value > 65535) continue;
    *out = (uint32_t)instr->value->int_value;
    return true;
  }
  return void_diag_at(diag, "direct AArch64 Void object backend currently requires a small integer literal return", fun->line, fun->column, fun->name);
}

static bool void_is_literal_return_function(const IrFunction *fun, uint32_t *out, ZDiag *diag) {
  if (!fun || fun->local_len != 0 || fun->instr_len != 1) return false;
  return void_return_literal(fun, out, diag);
}

static size_t void_align_size(size_t value, size_t alignment) {
  return z_void_align(value, alignment);
}

static void void_pad_to(ZBuf *buf, size_t offset) {
  z_void_pad_to(buf, offset);
}

static bool void_type_is_scalar32(IrTypeKind type) {
  return type == IR_TYPE_BOOL || type == IR_TYPE_U8 || type == IR_TYPE_U16 || type == IR_TYPE_I32 || type == IR_TYPE_U32 || type == IR_TYPE_USIZE;
}

static bool void_type_is_scalar64(IrTypeKind type) {
  return type == IR_TYPE_I64 || type == IR_TYPE_U64;
}

static bool void_type_is_unsigned(IrTypeKind type) {
  return type == IR_TYPE_U8 || type == IR_TYPE_U16 || type == IR_TYPE_USIZE || type == IR_TYPE_U32 || type == IR_TYPE_U64;
}

static bool void_type_is_scalar(IrTypeKind type) {
  return void_type_is_scalar32(type) || void_type_is_scalar64(type);
}

static void void_emit_cast_normalize_reg(ZBuf *text, unsigned reg, IrTypeKind source, IrTypeKind target) {
  switch (target) {
    case IR_TYPE_BOOL:
    case IR_TYPE_U8:
      z_aarch64_emit_uxtb_w(text, reg, reg);
      return;
    case IR_TYPE_U16:
      z_aarch64_emit_uxth_w(text, reg, reg);
      return;
    case IR_TYPE_I32:
    case IR_TYPE_U32:
    case IR_TYPE_USIZE:
      z_aarch64_emit_mov_w(text, reg, reg);
      return;
    case IR_TYPE_I64:
    case IR_TYPE_U64:
      if (source == IR_TYPE_I32) z_aarch64_emit_sxtw_x(text, reg, reg);
      else if (!void_type_is_scalar64(source)) z_aarch64_emit_mov_w(text, reg, reg);
      return;
    default:
      return;
  }
}

static unsigned void_slot_offset(unsigned local_index) {
  return local_index * 8;
}

static unsigned void_local_slot_offset(const IrFunction *fun, unsigned local_index, unsigned slot_offset, unsigned frame_size) {
  if (fun && local_index < fun->local_len && fun->locals[local_index].frame_offset > 0 && frame_size >= fun->locals[local_index].frame_offset) {
    return frame_size - fun->locals[local_index].frame_offset + slot_offset;
  }
  return VOID_SCRATCH_SLOT_COUNT * VOID_SCRATCH_SLOT_BYTES + void_slot_offset(local_index) + slot_offset;
}

static void void_emit_load_local_w(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned slot_offset, unsigned frame_size) {
  unsigned offset = void_local_slot_offset(fun, local_index, slot_offset, frame_size);
  z_aarch64_emit_load_w_sp(text, reg, offset);
}

static void void_emit_load_local_x(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned slot_offset, unsigned frame_size) {
  unsigned offset = void_local_slot_offset(fun, local_index, slot_offset, frame_size);
  z_aarch64_emit_load_x_sp(text, reg, offset);
}

static void void_emit_store_local_w(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned slot_offset, unsigned frame_size) {
  unsigned offset = void_local_slot_offset(fun, local_index, slot_offset, frame_size);
  z_aarch64_emit_store_w_sp(text, reg, offset);
}

static void void_emit_store_local_x(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned slot_offset, unsigned frame_size) {
  unsigned offset = void_local_slot_offset(fun, local_index, slot_offset, frame_size);
  z_aarch64_emit_store_x_sp(text, reg, offset);
}

static void void_emit_load_local_b(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned slot_offset, unsigned frame_size) {
  unsigned offset = void_local_slot_offset(fun, local_index, slot_offset, frame_size);
  z_aarch64_emit_load_b_sp(text, reg, offset);
}

static void void_emit_store_local_b(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned slot_offset, unsigned frame_size) {
  unsigned offset = void_local_slot_offset(fun, local_index, slot_offset, frame_size);
  z_aarch64_emit_store_b_sp(text, reg, offset);
}

static bool void_scratch_slot(unsigned slot, unsigned *offset, const IrValue *value, ZDiag *diag) {
  if (slot >= VOID_SCRATCH_SLOT_COUNT) {
    return void_diag_at(diag, "direct AArch64 Void expression nesting exceeds scratch register spill capacity", value ? value->line : 1, value ? value->column : 1, "expression too deep");
  }
  *offset = slot * VOID_SCRATCH_SLOT_BYTES;
  return true;
}

static bool void_emit_store_scratch(ZBuf *text, unsigned reg, IrTypeKind type, unsigned slot, const IrValue *value, ZDiag *diag) {
  unsigned offset = 0;
  if (!void_scratch_slot(slot, &offset, value, diag)) return false;
  if (void_type_is_scalar64(type)) z_aarch64_emit_store_x_sp(text, reg, offset);
  else z_aarch64_emit_store_w_sp(text, reg, offset);
  return true;
}

static bool void_emit_load_scratch(ZBuf *text, unsigned reg, IrTypeKind type, unsigned slot, const IrValue *value, ZDiag *diag) {
  unsigned offset = 0;
  if (!void_scratch_slot(slot, &offset, value, diag)) return false;
  if (void_type_is_scalar64(type)) z_aarch64_emit_load_x_sp(text, reg, offset);
  else z_aarch64_emit_load_w_sp(text, reg, offset);
  return true;
}

static void void_emit_load_field(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned field_offset, IrTypeKind type, unsigned frame_size) {
  if (type == IR_TYPE_U8 || type == IR_TYPE_BOOL) {
    void_emit_load_local_b(text, fun, reg, local_index, field_offset, frame_size);
  } else {
    void_emit_load_local_w(text, fun, reg, local_index, field_offset, frame_size);
  }
}

static void void_emit_store_field(ZBuf *text, const IrFunction *fun, unsigned reg, unsigned local_index, unsigned field_offset, IrTypeKind type, unsigned frame_size) {
  if (type == IR_TYPE_U8 || type == IR_TYPE_BOOL) {
    void_emit_store_local_b(text, fun, reg, local_index, field_offset, frame_size);
  } else {
    void_emit_store_local_w(text, fun, reg, local_index, field_offset, frame_size);
  }
}

static void void_emit_binary_reg(ZBuf *text, IrBinaryOp op, unsigned dst, unsigned lhs, unsigned rhs, bool wide) {
  if (op == IR_BIN_ADD) {
    if (wide) z_aarch64_emit_add_x_reg(text, dst, lhs, rhs);
    else z_aarch64_emit_add_w_reg(text, dst, lhs, rhs);
  } else if (op == IR_BIN_SUB) {
    if (wide) z_aarch64_emit_sub_x_reg(text, dst, lhs, rhs);
    else z_aarch64_emit_sub_w_reg(text, dst, lhs, rhs);
  } else if (op == IR_BIN_MUL) {
    if (wide) z_aarch64_emit_mul_x_reg(text, dst, lhs, rhs);
    else z_aarch64_emit_mul_w_reg(text, dst, lhs, rhs);
  } else if (op == IR_BIN_XOR) {
    if (wide) z_aarch64_emit_eor_x_reg(text, dst, lhs, rhs);
    else z_aarch64_emit_eor_w_reg(text, dst, lhs, rhs);
  } else if (op == IR_BIN_SHL) {
    if (wide) z_aarch64_emit_lsl_x_reg(text, dst, lhs, rhs);
    else z_aarch64_emit_lsl_w_reg(text, dst, lhs, rhs);
  } else if (op == IR_BIN_SHR) {
    if (wide) z_aarch64_emit_lsr_x_reg(text, dst, lhs, rhs);
    else z_aarch64_emit_lsr_w_reg(text, dst, lhs, rhs);
  } else if (op == IR_BIN_ROR) {
    if (wide) z_aarch64_emit_ror_x_reg(text, dst, lhs, rhs);
    else z_aarch64_emit_ror_w_reg(text, dst, lhs, rhs);
  }
}

static bool void_const_u32_value(const IrValue *value, unsigned *out) {
  if (!value || value->kind != IR_VALUE_INT || value->int_value > UINT32_MAX) return false;
  if (out) *out = (unsigned)value->int_value;
  return true;
}

static unsigned void_cond_for_compare(IrCompareOp op) {
  switch (op) {
    case IR_CMP_EQ: return 0;
    case IR_CMP_NE: return 1;
    case IR_CMP_LT: return 11;
    case IR_CMP_LE: return 13;
    case IR_CMP_GT: return 12;
    case IR_CMP_GE: return 10;
  }
  return 0;
}

static unsigned void_invert_cond(unsigned cond) {
  return cond ^ 1u;
}

static VoidRuntimeHelper void_runtime_helper_for_value(IrValueKind kind) {
  switch (kind) {
    case IR_VALUE_HTTP_RESULT_OK: return VOID_RUNTIME_HTTP_RESULT_OK;
    case IR_VALUE_HTTP_RESULT_STATUS: return VOID_RUNTIME_HTTP_RESULT_STATUS;
    case IR_VALUE_HTTP_RESULT_BODY_LEN: return VOID_RUNTIME_HTTP_RESULT_BODY_LEN;
    case IR_VALUE_HTTP_RESULT_ERROR: return VOID_RUNTIME_HTTP_RESULT_ERROR;
    case IR_VALUE_HTTP_RESPONSE_LEN: return VOID_RUNTIME_HTTP_RESPONSE_LEN;
    case IR_VALUE_HTTP_RESPONSE_HEADERS_LEN: return VOID_RUNTIME_HTTP_RESPONSE_HEADERS_LEN;
    case IR_VALUE_HTTP_RESPONSE_BODY_OFFSET: return VOID_RUNTIME_HTTP_RESPONSE_BODY_OFFSET;
    case IR_VALUE_HTTP_HEADER_VALUE: return VOID_RUNTIME_HTTP_HEADER_VALUE;
    case IR_VALUE_HTTP_HEADER_FOUND: return VOID_RUNTIME_HTTP_HEADER_FOUND;
    case IR_VALUE_HTTP_HEADER_OFFSET: return VOID_RUNTIME_HTTP_HEADER_OFFSET;
    case IR_VALUE_HTTP_HEADER_LEN: return VOID_RUNTIME_HTTP_HEADER_LEN;
    default: return VOID_RUNTIME_HELPER_COUNT;
  }
}

static bool void_readonly_data_byte(const IrProgram *program, unsigned offset, unsigned char *out) {
  if (!program) return false;
  for (size_t i = 0; i < program->data_segment_len; i++) {
    const IrDataSegment *segment = &program->data_segments[i];
    if (offset >= segment->offset && offset < segment->offset + segment->len) {
      if (out) *out = segment->bytes[offset - segment->offset];
      return true;
    }
  }
  return false;
}

static bool void_byte_view_const_len(const IrValue *view, unsigned *out) {
  if (!view) return false;
  if (view->kind == IR_VALUE_STRING_LITERAL || view->kind == IR_VALUE_ARRAY_BYTE_VIEW) {
    if (out) *out = view->data_len;
    return true;
  }
  if (view->kind == IR_VALUE_BYTE_SLICE) {
    unsigned base_len = 0;
    if (!void_byte_view_const_len(view->left, &base_len)) return false;
    unsigned start = 0;
    unsigned end = base_len;
    if (view->index && !void_const_u32_value(view->index, &start)) return false;
    if (view->right && !void_const_u32_value(view->right, &end)) return false;
    if (start > end || end > base_len) return false;
    if (out) *out = end - start;
    return true;
  }
  return false;
}

static bool void_byte_view_const_byte(const IrProgram *program, const IrValue *view, unsigned index, unsigned char *out) {
  if (!view) return false;
  if (view->kind == IR_VALUE_STRING_LITERAL) {
    if (index >= view->data_len) return false;
    return void_readonly_data_byte(program, view->data_offset + index, out);
  }
  if (view->kind == IR_VALUE_BYTE_SLICE) {
    unsigned len = 0;
    unsigned start = 0;
    if (!void_byte_view_const_len(view, &len) || index >= len) return false;
    if (view->index && !void_const_u32_value(view->index, &start)) return false;
    return void_byte_view_const_byte(program, view->left, start + index, out);
  }
  return false;
}

static bool void_emit_rodata_ptr_literal(ZBuf *text, unsigned reg, unsigned data_offset, VoidEmitContext *ctx, const IrValue *value, ZDiag *diag) {
  if (ctx && ctx->pie_relative_data) {
    size_t patch_offset = text->len;
    z_aarch64_emit_adrp_add_placeholder(text, reg);
    return z_void_record_data_patch(ctx, patch_offset, data_offset, value, diag);
  }
  while (((text->len + 8) % 8) != 0) z_aarch64_emit_nop(text);
  z_aarch64_emit_ldr_x_literal8(text, reg);
  z_aarch64_emit_b_offset_words(text, 3);
  size_t patch_offset = text->len;
  z_aarch64_append_u64(text, data_offset - (ctx ? ctx->rodata_base_offset : 0));
  return z_void_record_data_patch(ctx, patch_offset, data_offset, value, diag);
}

static bool void_emit_byte_view_ptr_at(ZBuf *text, const IrFunction *fun, const IrValue *view, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag);
static bool void_emit_byte_view_ptr(ZBuf *text, const IrFunction *fun, const IrValue *view, unsigned reg, unsigned frame_size, VoidEmitContext *ctx, ZDiag *diag) {
  return void_emit_byte_view_ptr_at(text, fun, view, reg, frame_size, 0, ctx, diag);
}
static bool void_emit_byte_view_len_at(ZBuf *text, const IrFunction *fun, const IrValue *view, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag);
static bool void_emit_byte_view_len(ZBuf *text, const IrFunction *fun, const IrValue *view, unsigned reg, unsigned frame_size, VoidEmitContext *ctx, ZDiag *diag) {
  return void_emit_byte_view_len_at(text, fun, view, reg, frame_size, 0, ctx, diag);
}
static bool void_emit_value_to_reg_at(ZBuf *text, const IrFunction *fun, const IrValue *value, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag);
static bool void_emit_value_to_reg(ZBuf *text, const IrFunction *fun, const IrValue *value, unsigned reg, unsigned frame_size, VoidEmitContext *ctx, ZDiag *diag) {
  return void_emit_value_to_reg_at(text, fun, value, reg, frame_size, 0, ctx, diag);
}

static bool void_emit_json_parse_bytes_call_at(ZBuf *text, const IrFunction *fun, const IrValue *value, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag) {
  if (!void_emit_byte_view_ptr_at(text, fun, value->left, 0, frame_size, scratch_slot, ctx, diag)) return false;
  if (!void_emit_store_scratch(text, 0, IR_TYPE_U64, scratch_slot, value ? value->left : NULL, diag)) return false;
  if (!void_emit_byte_view_len_at(text, fun, value->left, 1, frame_size, scratch_slot + 1, ctx, diag)) return false;
  if (!void_emit_load_scratch(text, 0, IR_TYPE_U64, scratch_slot, value ? value->left : NULL, diag)) return false;
  size_t patch = z_aarch64_emit_bl_placeholder(text);
  return z_void_record_value_runtime_patch(ctx, VOID_RUNTIME_JSON_PARSE_BYTES, patch, value, diag);
}

static bool void_emit_byte_view_len_at(ZBuf *text, const IrFunction *fun, const IrValue *view, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag) {
  if (!view) return void_diag_at(diag, "direct AArch64 Void byte view is missing", 1, 1, "missing byte view");
  if (view->kind == IR_VALUE_STRING_LITERAL || view->kind == IR_VALUE_ARRAY_BYTE_VIEW) {
    if (view->data_len > 65535) return void_diag_at(diag, "direct AArch64 Void byte-view length is too large for the current MVP", view->line, view->column, "large byte view");
    z_aarch64_emit_movz_w(text, reg, view->data_len);
    return true;
  }
  if (view->kind == IR_VALUE_LOCAL && view->local_index < fun->local_len && fun->locals[view->local_index].type == IR_TYPE_BYTE_VIEW) {
    void_emit_load_local_w(text, fun, reg, view->local_index, 8, frame_size);
    return true;
  }
  if (view->kind == IR_VALUE_MAYBE_VALUE && view->local_index < fun->local_len && fun->locals[view->local_index].type == IR_TYPE_MAYBE_BYTE_VIEW) {
    void_emit_load_local_w(text, fun, reg, view->local_index, 16, frame_size);
    return true;
  }
  if (view->kind == IR_VALUE_BYTE_SLICE) {
    unsigned start = 0;
    unsigned end = 0;
    if ((!view->index || void_const_u32_value(view->index, &start)) &&
        void_const_u32_value(view->right, &end) && end >= start && end - start <= 65535) {
      z_aarch64_emit_movz_w(text, reg, end - start);
      return true;
    }
    if ((!view->index || void_const_u32_value(view->index, &start)) && view->right) {
      if (!void_emit_value_to_reg_at(text, fun, view->right, reg, frame_size, scratch_slot, ctx, diag)) return false;
      if (start > 0) z_aarch64_emit_sub_w_imm(text, reg, reg, start);
      return true;
    }
    if (view->index && view->right) {
      unsigned tmp = reg == 8 ? 9 : 8;
      if (!void_emit_value_to_reg_at(text, fun, view->right, reg, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, reg, view->right ? view->right->type : IR_TYPE_U32, scratch_slot, view->right, diag)) return false;
      if (!void_emit_value_to_reg_at(text, fun, view->index, tmp, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, reg, view->right ? view->right->type : IR_TYPE_U32, scratch_slot, view->right, diag)) return false;
      void_emit_binary_reg(text, IR_BIN_SUB, reg, reg, tmp, false);
      return true;
    }
  }
  (void)ctx;
  return void_diag_at(diag, "direct AArch64 Void byte-view length currently requires a literal, constant slice, or byte-view local", view->line, view->column, "unsupported byte view length");
}

static bool void_emit_byte_view_ptr_at(ZBuf *text, const IrFunction *fun, const IrValue *view, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag) {
  if (!view) return void_diag_at(diag, "direct AArch64 Void byte view is missing", 1, 1, "missing byte view");
  if (view->kind == IR_VALUE_LOCAL && view->local_index < fun->local_len && fun->locals[view->local_index].type == IR_TYPE_BYTE_VIEW) {
    void_emit_load_local_x(text, fun, reg, view->local_index, 0, frame_size);
    return true;
  }
  if (view->kind == IR_VALUE_MAYBE_VALUE && view->local_index < fun->local_len && fun->locals[view->local_index].type == IR_TYPE_MAYBE_BYTE_VIEW) {
    void_emit_load_local_x(text, fun, reg, view->local_index, 8, frame_size);
    return true;
  }
  if (view->kind == IR_VALUE_ARRAY_BYTE_VIEW && view->array_index < fun->local_len) {
    const IrLocal *local = &fun->locals[view->array_index];
    if (!local->is_array || local->element_type != IR_TYPE_U8) return void_diag_at(diag, "direct AArch64 Void byte-view array requires [N]u8", view->line, view->column, "unsupported array view");
    z_aarch64_emit_add_x_sp_imm(text, reg, void_local_slot_offset(fun, view->array_index, 0, frame_size));
    return true;
  }
  if (view->kind == IR_VALUE_STRING_LITERAL) {
    return void_emit_rodata_ptr_literal(text, reg, view->data_offset, ctx, view, diag);
  }
  if (view->kind == IR_VALUE_BYTE_SLICE) {
    unsigned start = 0;
    if (!void_emit_byte_view_ptr_at(text, fun, view->left, reg, frame_size, scratch_slot, ctx, diag)) return false;
    if (!view->index) return true;
    if (void_const_u32_value(view->index, &start)) {
      if (start > 4095) return void_diag_at(diag, "direct AArch64 Void byte slice constant start is too large", view->line, view->column, "unsupported byte slice");
      if (start > 0) z_aarch64_emit_add_x_imm(text, reg, reg, start);
      return true;
    }
    unsigned tmp = reg == 8 ? 9 : 8;
    if (!void_emit_store_scratch(text, reg, IR_TYPE_U64, scratch_slot, view, diag)) return false;
    if (!void_emit_value_to_reg_at(text, fun, view->index, tmp, frame_size, scratch_slot + 1, ctx, diag)) return false;
    if (!void_emit_load_scratch(text, reg, IR_TYPE_U64, scratch_slot, view, diag)) return false;
    z_aarch64_emit_add_x_reg(text, reg, reg, tmp);
    return true;
  }
  return void_diag_at(diag, "direct AArch64 Void value is not a supported byte view", view->line, view->column, "unsupported byte view");
}

static bool void_emit_call_to_reg(ZBuf *text, const IrFunction *fun, const IrValue *value, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag) {
  if (value->arg_len > 8) return void_diag_at(diag, "direct AArch64 Void call supports at most eight arguments", value->line, value->column, "too many arguments");
  if (scratch_slot + value->arg_len >= VOID_SCRATCH_SLOT_COUNT) {
    return void_diag_at(diag, "direct AArch64 Void call argument nesting exceeds scratch spill capacity", value->line, value->column, "too many nested call arguments");
  }
  for (size_t i = 0; i < value->arg_len; i++) {
    const IrValue *arg = value->args[i];
    if (!void_emit_value_to_reg_at(text, fun, arg, 8, frame_size, scratch_slot + (unsigned)value->arg_len, ctx, diag)) return false;
    if (!void_emit_store_scratch(text, 8, arg ? arg->type : IR_TYPE_I32, scratch_slot + (unsigned)i, arg, diag)) return false;
  }
  for (size_t i = 0; i < value->arg_len; i++) {
    const IrValue *arg = value->args[i];
    if (!void_emit_load_scratch(text, (unsigned)i, arg ? arg->type : IR_TYPE_I32, scratch_slot + (unsigned)i, arg, diag)) return false;
  }
  size_t patch = z_aarch64_emit_bl_placeholder(text);
  if (!z_void_record_call_patch(ctx, patch, value->callee_index, value, diag)) return false;
  if (reg != 0) {
    if (void_type_is_scalar64(value->type)) z_aarch64_emit_mov_x(text, reg, 0);
    else z_aarch64_emit_mov_w(text, reg, 0);
  }
  return true;
}

static bool void_emit_cast_value_to_reg_at(ZBuf *text, const IrFunction *fun, const IrValue *value, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag) {
  if (!void_emit_value_to_reg_at(text, fun, value->left, reg, frame_size, scratch_slot, ctx, diag)) return false;
  void_emit_cast_normalize_reg(text, reg, value->left ? value->left->type : IR_TYPE_UNSUPPORTED, value->type);
  return true;
}

static bool void_emit_value_to_reg_at(ZBuf *text, const IrFunction *fun, const IrValue *value, unsigned reg, unsigned frame_size, unsigned scratch_slot, VoidEmitContext *ctx, ZDiag *diag) {
  if (!value) return void_diag_at(diag, "direct AArch64 Void expression is missing", 1, 1, "missing expression");
  switch (value->kind) {
    case IR_VALUE_BOOL: case IR_VALUE_INT:
      if (void_type_is_scalar64(value->type)) z_aarch64_emit_movz_x(text, reg, (uint64_t)value->int_value);
      else z_aarch64_emit_movz_w(text, reg, (uint32_t)value->int_value);
      return true;
    case IR_VALUE_LOCAL:
      if (value->local_index >= fun->local_len) return void_diag_at(diag, "direct AArch64 Void local index is out of range", value->line, value->column, "invalid local");
      if (fun->locals[value->local_index].type == IR_TYPE_BYTE_VIEW) {
        return void_diag_at(diag, "direct AArch64 Void byte-view local cannot be used as a scalar", value->line, value->column, "byte-view local");
      }
      if (void_type_is_scalar64(fun->locals[value->local_index].type)) void_emit_load_local_x(text, fun, reg, value->local_index, 0, frame_size);
      else void_emit_load_local_w(text, fun, reg, value->local_index, 0, frame_size);
      return true;
    case IR_VALUE_CAST: return void_emit_cast_value_to_reg_at(text, fun, value, reg, frame_size, scratch_slot, ctx, diag);
    case IR_VALUE_BINARY:
      if (value->binary_op == IR_BIN_AND) {
        if (!void_emit_value_to_reg_at(text, fun, value->left, reg, frame_size, scratch_slot, ctx, diag)) return false;
        size_t left_false = z_aarch64_emit_cbz_w_placeholder(text, reg);
        if (!void_emit_value_to_reg_at(text, fun, value->right, reg, frame_size, scratch_slot, ctx, diag)) return false;
        size_t right_false = z_aarch64_emit_cbz_w_placeholder(text, reg);
        z_aarch64_emit_movz_w(text, reg, 1);
        size_t end_patch = z_aarch64_emit_b_placeholder(text);
        z_aarch64_patch_cond19(text, left_false, text->len);
        z_aarch64_patch_cond19(text, right_false, text->len);
        z_aarch64_emit_movz_w(text, reg, 0);
        z_aarch64_patch_branch26(text, end_patch, text->len);
        return true;
      }
      if (value->binary_op == IR_BIN_OR) {
        if (!void_emit_value_to_reg_at(text, fun, value->left, reg, frame_size, scratch_slot, ctx, diag)) return false;
        size_t eval_right = z_aarch64_emit_cbz_w_placeholder(text, reg);
        z_aarch64_emit_movz_w(text, reg, 1);
        size_t left_true_end = z_aarch64_emit_b_placeholder(text);
        z_aarch64_patch_cond19(text, eval_right, text->len);
        if (!void_emit_value_to_reg_at(text, fun, value->right, reg, frame_size, scratch_slot, ctx, diag)) return false;
        size_t right_false = z_aarch64_emit_cbz_w_placeholder(text, reg);
        z_aarch64_emit_movz_w(text, reg, 1);
        size_t right_true_end = z_aarch64_emit_b_placeholder(text);
        z_aarch64_patch_cond19(text, right_false, text->len);
        z_aarch64_emit_movz_w(text, reg, 0);
        z_aarch64_patch_branch26(text, left_true_end, text->len);
        z_aarch64_patch_branch26(text, right_true_end, text->len);
        return true;
      }
      if (value->binary_op != IR_BIN_ADD && value->binary_op != IR_BIN_SUB && value->binary_op != IR_BIN_MUL &&
          value->binary_op != IR_BIN_DIV && value->binary_op != IR_BIN_MOD &&
          value->binary_op != IR_BIN_XOR && value->binary_op != IR_BIN_SHL &&
          value->binary_op != IR_BIN_SHR && value->binary_op != IR_BIN_ROR)
        return void_diag_at(diag, "direct AArch64 Void binary operator is unsupported", value->line, value->column, "unsupported operator");
      if (!void_emit_value_to_reg_at(text, fun, value->left, 8, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 8, value->left ? value->left->type : IR_TYPE_I32, scratch_slot, value->left, diag)) return false;
      if (!void_emit_value_to_reg_at(text, fun, value->right, 9, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 8, value->left ? value->left->type : IR_TYPE_I32, scratch_slot, value->left, diag)) return false;
      bool wide = void_type_is_scalar64(value->type);
      if (value->binary_op == IR_BIN_DIV) {
        z_aarch64_emit_div_reg(text, reg, 8, 9, void_type_is_unsigned(value->type), wide);
      } else if (value->binary_op == IR_BIN_MOD) {
        z_aarch64_emit_div_reg(text, 10, 8, 9, void_type_is_unsigned(value->type), wide);
        z_aarch64_emit_msub_reg(text, reg, 10, 9, 8, wide);
      } else {
        void_emit_binary_reg(text, value->binary_op, reg, 8, 9, wide);
      }
      return true;
    case IR_VALUE_COMPARE: {
      if (!value->left || !value->right) {
        return void_diag_at(diag, "direct AArch64 Void comparison requires two operands", value->line, value->column, "invalid comparison");
      }
      if (!void_emit_value_to_reg_at(text, fun, value->left, 8, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 8, value->left->type, scratch_slot, value->left, diag)) return false;
      if (!void_emit_value_to_reg_at(text, fun, value->right, 9, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 8, value->left->type, scratch_slot, value->left, diag)) return false;
      z_aarch64_emit_cmp_w(text, 8, 9);
      z_aarch64_emit_movz_w(text, reg, 0);
      size_t false_patch = z_aarch64_emit_b_cond_placeholder(text, void_invert_cond(void_cond_for_compare(value->compare_op)));
      z_aarch64_emit_movz_w(text, reg, 1);
      z_aarch64_patch_cond19(text, false_patch, text->len);
      return true;
    }
    case IR_VALUE_CALL:
      return void_emit_call_to_reg(text, fun, value, reg, frame_size, scratch_slot, ctx, diag);
    case IR_VALUE_JSON_PARSE_BYTES:
      if (!void_emit_json_parse_bytes_call_at(text, fun, value, frame_size, scratch_slot, ctx, diag)) return false;
      if (reg != 0) z_aarch64_emit_mov_x(text, reg, 0);
      return true;
    case IR_VALUE_JSON_VALIDATE_BYTES:
      if (!void_emit_json_parse_bytes_call_at(text, fun, value, frame_size, scratch_slot, ctx, diag)) return false;
      z_aarch64_emit_cmp_x(text, 0, 31);
      z_aarch64_emit_movz_w(text, reg, 0);
      {
        size_t invalid = z_aarch64_emit_b_cond_placeholder(text, 11); // signed less than
        z_aarch64_emit_movz_w(text, reg, 1);
        z_aarch64_patch_cond19(text, invalid, text->len);
      }
      return true;
    case IR_VALUE_JSON_STREAM_TOKENS_BYTES:
      if (!void_emit_json_parse_bytes_call_at(text, fun, value, frame_size, scratch_slot, ctx, diag)) return false;
      z_aarch64_emit_cmp_x(text, 0, 31);
      {
        size_t ok = z_aarch64_emit_b_cond_placeholder(text, 10); // signed greater or equal
        if (reg != 0) z_aarch64_emit_mov_x(text, reg, 31);
        else z_aarch64_emit_mov_x(text, 0, 31);
        size_t done = z_aarch64_emit_b_placeholder(text);
        z_aarch64_patch_cond19(text, ok, text->len);
        if (reg != 0) z_aarch64_emit_mov_x(text, reg, 0);
        z_aarch64_patch_branch26(text, done, text->len);
      }
      return true;
    case IR_VALUE_HTTP_FETCH: {
      if (!void_emit_byte_view_ptr_at(text, fun, value->left, 0, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 0, IR_TYPE_U64, scratch_slot, value->left, diag)) return false;
      if (!void_emit_byte_view_len_at(text, fun, value->left, 1, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 1, IR_TYPE_U32, scratch_slot + 1, value->left, diag)) return false;
      if (!void_emit_byte_view_ptr_at(text, fun, value->right, 2, frame_size, scratch_slot + 2, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 2, IR_TYPE_U64, scratch_slot + 2, value->right, diag)) return false;
      if (!void_emit_byte_view_len_at(text, fun, value->right, 3, frame_size, scratch_slot + 3, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 3, IR_TYPE_U32, scratch_slot + 3, value->right, diag)) return false;
      if (!void_emit_value_to_reg_at(text, fun, value->index, 4, frame_size, scratch_slot + 4, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 0, IR_TYPE_U64, scratch_slot, value->left, diag)) return false;
      if (!void_emit_load_scratch(text, 1, IR_TYPE_U32, scratch_slot + 1, value->left, diag)) return false;
      if (!void_emit_load_scratch(text, 2, IR_TYPE_U64, scratch_slot + 2, value->right, diag)) return false;
      if (!void_emit_load_scratch(text, 3, IR_TYPE_U32, scratch_slot + 3, value->right, diag)) return false;
      size_t patch = z_aarch64_emit_bl_placeholder(text);
      if (!z_void_record_value_runtime_patch(ctx, VOID_RUNTIME_HTTP_FETCH, patch, value, diag)) return false;
      if (reg != 0) z_aarch64_emit_mov_x(text, reg, 0);
      return true;
    }
    case IR_VALUE_HTTP_RESULT_OK:
    case IR_VALUE_HTTP_RESULT_STATUS:
    case IR_VALUE_HTTP_RESULT_BODY_LEN:
    case IR_VALUE_HTTP_RESULT_ERROR:
    case IR_VALUE_HTTP_HEADER_FOUND:
    case IR_VALUE_HTTP_HEADER_OFFSET:
    case IR_VALUE_HTTP_HEADER_LEN: {
      if (!void_emit_value_to_reg_at(text, fun, value->left, 0, frame_size, scratch_slot, ctx, diag)) return false;
      size_t patch = z_aarch64_emit_bl_placeholder(text);
      if (!z_void_record_value_runtime_patch(ctx, void_runtime_helper_for_value(value->kind), patch, value, diag)) return false;
      if (reg != 0) z_aarch64_emit_mov_w(text, reg, 0);
      return true;
    }
    case IR_VALUE_HTTP_RESPONSE_LEN:
    case IR_VALUE_HTTP_RESPONSE_HEADERS_LEN:
    case IR_VALUE_HTTP_RESPONSE_BODY_OFFSET: {
      if (!void_emit_byte_view_ptr_at(text, fun, value->left, 0, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 0, IR_TYPE_U64, scratch_slot, value->left, diag)) return false;
      if (!void_emit_byte_view_len_at(text, fun, value->left, 1, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 0, IR_TYPE_U64, scratch_slot, value->left, diag)) return false;
      size_t patch = z_aarch64_emit_bl_placeholder(text);
      if (!z_void_record_value_runtime_patch(ctx, void_runtime_helper_for_value(value->kind), patch, value, diag)) return false;
      if (reg != 0) z_aarch64_emit_mov_w(text, reg, 0);
      return true;
    }
    case IR_VALUE_HTTP_HEADER_VALUE: {
      if (!void_emit_byte_view_ptr_at(text, fun, value->left, 0, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 0, IR_TYPE_U64, scratch_slot, value->left, diag)) return false;
      if (!void_emit_byte_view_len_at(text, fun, value->left, 1, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 1, IR_TYPE_U32, scratch_slot + 1, value->left, diag)) return false;
      if (!void_emit_byte_view_ptr_at(text, fun, value->right, 2, frame_size, scratch_slot + 2, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 2, IR_TYPE_U64, scratch_slot + 2, value->right, diag)) return false;
      if (!void_emit_byte_view_len_at(text, fun, value->right, 3, frame_size, scratch_slot + 3, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 0, IR_TYPE_U64, scratch_slot, value->left, diag)) return false;
      if (!void_emit_load_scratch(text, 1, IR_TYPE_U32, scratch_slot + 1, value->left, diag)) return false;
      if (!void_emit_load_scratch(text, 2, IR_TYPE_U64, scratch_slot + 2, value->right, diag)) return false;
      size_t patch = z_aarch64_emit_bl_placeholder(text);
      if (!z_void_record_value_runtime_patch(ctx, VOID_RUNTIME_HTTP_HEADER_VALUE, patch, value, diag)) return false;
      if (reg != 0) z_aarch64_emit_mov_x(text, reg, 0);
      return true;
    }
    case IR_VALUE_VEC_LEN:
    case IR_VALUE_VEC_CAPACITY:
      if (value->local_index >= fun->local_len || fun->locals[value->local_index].type != IR_TYPE_VEC) return void_diag_at(diag, "direct AArch64 Void Vec helper requires a Vec local", value->line, value->column, "invalid Vec local");
      void_emit_load_local_w(text, fun, reg, value->local_index, value->kind == IR_VALUE_VEC_LEN ? 8 : 12, frame_size);
      return true;
    case IR_VALUE_VEC_PUSH: {
      if (value->local_index >= fun->local_len || fun->locals[value->local_index].type != IR_TYPE_VEC) return void_diag_at(diag, "direct AArch64 Void Vec push requires a Vec local", value->line, value->column, "invalid Vec local");
      void_emit_load_local_w(text, fun, 8, value->local_index, 8, frame_size);
      void_emit_load_local_w(text, fun, 9, value->local_index, 12, frame_size);
      z_aarch64_emit_cmp_w(text, 8, 9);
      size_t ok_patch = z_aarch64_emit_b_cond_placeholder(text, 3); // unsigned lower
      z_aarch64_emit_movz_w(text, reg, 0);
      size_t end_patch = z_aarch64_emit_b_placeholder(text);
      z_aarch64_patch_cond19(text, ok_patch, text->len);
      void_emit_store_local_w(text, fun, 8, value->local_index, 8, frame_size);
      void_emit_load_local_x(text, fun, 9, value->local_index, 0, frame_size);
      z_aarch64_emit_add_x_reg(text, 9, 9, 8);
      if (!void_emit_store_scratch(text, 9, IR_TYPE_U64, scratch_slot, value, diag)) return false;
      if (!void_emit_value_to_reg_at(text, fun, value->left, 10, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 9, IR_TYPE_U64, scratch_slot, value, diag)) return false;
      z_aarch64_emit_store_b_imm(text, 10, 9, 0);
      z_aarch64_emit_add_w_imm(text, 8, 8, 1);
      void_emit_store_local_w(text, fun, 8, value->local_index, 8, frame_size);
      z_aarch64_emit_movz_w(text, reg, 1);
      z_aarch64_patch_branch26(text, end_patch, text->len);
      return true;
    }
    case IR_VALUE_ARGS_LEN:
      z_aarch64_emit_mov_w(text, reg, 20);
      return true;
    case IR_VALUE_MAYBE_HAS:
      if (value->local_index >= fun->local_len ||
          (fun->locals[value->local_index].type != IR_TYPE_MAYBE_BYTE_VIEW && fun->locals[value->local_index].type != IR_TYPE_MAYBE_SCALAR)) {
        return void_diag_at(diag, "direct AArch64 Void maybe helper requires a Maybe local", value->line, value->column, "invalid maybe local");
      }
      void_emit_load_local_w(text, fun, reg, value->local_index, 0, frame_size);
      return true;
    case IR_VALUE_BYTE_VIEW_LEN:
      return void_emit_byte_view_len_at(text, fun, value->left, reg, frame_size, scratch_slot, ctx, diag);
    case IR_VALUE_BYTE_VIEW_INDEX_LOAD: {
      unsigned const_index = 0;
      unsigned char byte = 0;
      if (void_const_u32_value(value->index, &const_index) &&
          void_byte_view_const_byte(ctx ? ctx->program : NULL, value->left, const_index, &byte)) {
        z_aarch64_emit_movz_w(text, reg, byte);
        return true;
      }
      if (!value->index || !void_emit_value_to_reg_at(text, fun, value->index, 8, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 8, value->index ? value->index->type : IR_TYPE_U32, scratch_slot, value->index, diag)) return false;
      if (!void_emit_byte_view_len_at(text, fun, value->left, 9, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 8, value->index ? value->index->type : IR_TYPE_U32, scratch_slot, value->index, diag)) return false;
      z_aarch64_emit_cmp_w(text, 8, 9);
      size_t ok_patch = z_aarch64_emit_b_cond_placeholder(text, 3); // unsigned lower
      z_aarch64_emit_brk(text);
      z_aarch64_patch_cond19(text, ok_patch, text->len);
      if (!void_emit_byte_view_ptr_at(text, fun, value->left, 9, frame_size, scratch_slot + 1, ctx, diag)) return false;
      if (!void_emit_load_scratch(text, 8, value->index ? value->index->type : IR_TYPE_U32, scratch_slot, value->index, diag)) return false;
      z_aarch64_emit_add_x_reg(text, 9, 9, 8);
      z_aarch64_emit_load_b_imm(text, reg, 9, 0);
      return true;
    }
    case IR_VALUE_INDEX_LOAD: {
      if (value->array_index >= fun->local_len) return void_diag_at(diag, "direct AArch64 Void indexed load array is out of range", value->line, value->column, "invalid array local");
      const IrLocal *local = &fun->locals[value->array_index];
      unsigned const_index = 0;
      if (local->is_array && local->element_type != IR_TYPE_U8 && void_const_u32_value(value->index, &const_index) && const_index < local->array_len) {
        void_emit_load_local_w(text, fun, reg, value->array_index, const_index * 4u, frame_size);
        return true;
      }
      if (local->is_array && (local->element_type == IR_TYPE_U32 || local->element_type == IR_TYPE_I32 || local->element_type == IR_TYPE_USIZE)) {
        if (!value->index || !void_emit_value_to_reg_at(text, fun, value->index, 8, frame_size, scratch_slot, ctx, diag)) return false;
        if (!void_emit_store_scratch(text, 8, value->index ? value->index->type : IR_TYPE_U32, scratch_slot, value->index, diag)) return false;
        z_aarch64_emit_movz_w(text, 9, local->array_len);
        z_aarch64_emit_cmp_w(text, 8, 9);
        size_t ok_patch = z_aarch64_emit_b_cond_placeholder(text, 3); // unsigned lower
        z_aarch64_emit_brk(text);
        z_aarch64_patch_cond19(text, ok_patch, text->len);
        if (!void_emit_load_scratch(text, 8, value->index ? value->index->type : IR_TYPE_U32, scratch_slot, value->index, diag)) return false;
        z_aarch64_emit_add_x_sp_imm(text, 9, void_local_slot_offset(fun, value->array_index, 0, frame_size));
        z_aarch64_emit_add_x_reg_lsl(text, 9, 9, 8, 2);
        z_aarch64_emit_load_w_imm(text, reg, 9, 0);
        return true;
      }
      if (!local->is_array || local->element_type != IR_TYPE_U8) return void_diag_at(diag, "direct AArch64 Void indexed load requires [N]u8 or integer arrays", value->line, value->column, "unsupported array local");
      if (!value->index || !void_emit_value_to_reg_at(text, fun, value->index, 8, frame_size, scratch_slot, ctx, diag)) return false;
      if (!void_emit_store_scratch(text, 8, value->index ? value->index->type : IR_TYPE_U32, scratch_slot, value->index, diag)) return false;
      z_aarch64_emit_movz_w(text, 9, local->array_len);
      z_aarch64_emit_cmp_w(text, 8, 9);
      size_t ok_patch = z_aarch64_emit_b_cond_placeholder(text, 3); // unsigned lower
      z_aarch64_emit_brk(text);
      z_aarch64_patch_cond19(text, ok_patch, text->len);
      if (!void_emit_load_scratch(text, 8, value->index ? value->index->type : IR_TYPE_U32, scratch_slot, value->index, diag)) return false;
      z_aarch64_emit_add_x_sp_imm(text, 9, void_local_slot_offset(fun, value->array_index, 0, frame_size));
      z_aarch64_emit_add_x_reg(text, 9, 9, 8);
      z_aarch64_emit_load_b_imm(text, reg, 9, 0);
      return true;
    }
    case IR_VALUE_FIELD_LOAD:
      if (value->local_index >= fun->local_len) return void_diag_at(diag, "direct AArch64 Void field load record is out of range", value->line, value->column, "invalid record local");
      if (!fun->locals[value->local_index].is_record) return void_diag_at(diag, "direct AArch64 Void field load requires record local", value->line, value->column, "non-record local");
      void_emit_load_field(text, fun, reg, value->local_index, value->field_offset, value->type, frame_size);
      return true;
    case IR_VALUE_CRYPTO_HELPER: {
      VoidRuntimeHelper helper;
      switch (value->error_code) {
        case CRYPTO_SHA256: helper = VOID_RUNTIME_CRYPTO_SHA256; break;
        case CRYPTO_HMAC_SHA256: helper = VOID_RUNTIME_CRYPTO_HMAC_SHA256; break;
        case CRYPTO_AES_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_AES_ENCRYPT; break;
        case CRYPTO_AES_DECRYPT: helper = VOID_RUNTIME_CRYPTO_AES_DECRYPT; break;
        case CRYPTO_CHACHA20: helper = VOID_RUNTIME_CRYPTO_CHACHA20; break;
        case CRYPTO_PBKDF2: helper = VOID_RUNTIME_CRYPTO_PBKDF2; break;
        case CRYPTO_RANDOM_BYTES: helper = VOID_RUNTIME_CRYPTO_RANDOM_BYTES; break;
        case CRYPTO_SHA512: helper = VOID_RUNTIME_CRYPTO_SHA512; break;
        case CRYPTO_SALSA20: helper = VOID_RUNTIME_CRYPTO_SALSA20; break;
        case CRYPTO_CHACHA20_POLY1305_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_CHACHA20_POLY1305_ENCRYPT; break;
        case CRYPTO_CHACHA20_POLY1305_DECRYPT: helper = VOID_RUNTIME_CRYPTO_CHACHA20_POLY1305_DECRYPT; break;
        case CRYPTO_DES_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_DES_ENCRYPT; break;
        case CRYPTO_DES_DECRYPT: helper = VOID_RUNTIME_CRYPTO_DES_DECRYPT; break;
        case CRYPTO_TDES_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_TDES_ENCRYPT; break;
        case CRYPTO_TDES_DECRYPT: helper = VOID_RUNTIME_CRYPTO_TDES_DECRYPT; break;
        case CRYPTO_BLOWFISH_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_BLOWFISH_ENCRYPT; break;
        case CRYPTO_BLOWFISH_DECRYPT: helper = VOID_RUNTIME_CRYPTO_BLOWFISH_DECRYPT; break;
        case CRYPTO_TWOFISH_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_TWOFISH_ENCRYPT; break;
        case CRYPTO_TWOFISH_DECRYPT: helper = VOID_RUNTIME_CRYPTO_TWOFISH_DECRYPT; break;
        case CRYPTO_SERPENT_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_SERPENT_ENCRYPT; break;
        case CRYPTO_SERPENT_DECRYPT: helper = VOID_RUNTIME_CRYPTO_SERPENT_DECRYPT; break;
        case CRYPTO_CAMELLIA_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_CAMELLIA_ENCRYPT; break;
        case CRYPTO_CAMELLIA_DECRYPT: helper = VOID_RUNTIME_CRYPTO_CAMELLIA_DECRYPT; break;
        case CRYPTO_RSA_GENERATE_KEYPAIR: helper = VOID_RUNTIME_CRYPTO_RSA_GENERATE_KEYPAIR; break;
        case CRYPTO_RSA_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_RSA_ENCRYPT; break;
        case CRYPTO_RSA_DECRYPT: helper = VOID_RUNTIME_CRYPTO_RSA_DECRYPT; break;
        case CRYPTO_RSA_SIGN: helper = VOID_RUNTIME_CRYPTO_RSA_SIGN; break;
        case CRYPTO_RSA_VERIFY: helper = VOID_RUNTIME_CRYPTO_RSA_VERIFY; break;
        case CRYPTO_ECC_GENERATE_KEYPAIR: helper = VOID_RUNTIME_CRYPTO_ECC_GENERATE_KEYPAIR; break;
        case CRYPTO_ECC_SIGN: helper = VOID_RUNTIME_CRYPTO_ECC_SIGN; break;
        case CRYPTO_ECC_VERIFY: helper = VOID_RUNTIME_CRYPTO_ECC_VERIFY; break;
        case CRYPTO_ECC_ECDH: helper = VOID_RUNTIME_CRYPTO_ECC_ECDH; break;
        case CRYPTO_ECC_ED25519_SIGN: helper = VOID_RUNTIME_CRYPTO_ECC_ED25519_SIGN; break;
        case CRYPTO_ECC_ED25519_VERIFY: helper = VOID_RUNTIME_CRYPTO_ECC_ED25519_VERIFY; break;
        case CRYPTO_SHA384: helper = VOID_RUNTIME_CRYPTO_SHA384; break;
        case CRYPTO_SHA3_256: helper = VOID_RUNTIME_CRYPTO_SHA3_256; break;
        case CRYPTO_SHA3_512: helper = VOID_RUNTIME_CRYPTO_SHA3_512; break;
        case CRYPTO_BLAKE2B: helper = VOID_RUNTIME_CRYPTO_BLAKE2B; break;
        case CRYPTO_BLAKE2S: helper = VOID_RUNTIME_CRYPTO_BLAKE2S; break;
        case CRYPTO_HMAC_SHA384: helper = VOID_RUNTIME_CRYPTO_HMAC_SHA384; break;
        case CRYPTO_SHA3_384: helper = VOID_RUNTIME_CRYPTO_SHA3_384; break;
        case CRYPTO_SHAKE128: helper = VOID_RUNTIME_CRYPTO_SHAKE128; break;
        case CRYPTO_SHAKE256: helper = VOID_RUNTIME_CRYPTO_SHAKE256; break;
        case CRYPTO_HMAC_SHA512: helper = VOID_RUNTIME_CRYPTO_HMAC_SHA512; break;
        case CRYPTO_ECC_ED25519_GENERATE_KEYPAIR: helper = VOID_RUNTIME_CRYPTO_ECC_ED25519_GENERATE_KEYPAIR; break;
        case CRYPTO_ECC_X25519_ECDH: helper = VOID_RUNTIME_CRYPTO_ECC_X25519_ECDH; break;
        case CRYPTO_AES_GCM_ENCRYPT: helper = VOID_RUNTIME_CRYPTO_AES_GCM_ENCRYPT; break;
        case CRYPTO_AES_GCM_DECRYPT: helper = VOID_RUNTIME_CRYPTO_AES_GCM_DECRYPT; break;
        case CRYPTO_ECC_X25519_GENERATE_KEYPAIR: helper = VOID_RUNTIME_CRYPTO_ECC_X25519_GENERATE_KEYPAIR; break;
        default: return void_diag_at(diag, "direct AArch64 Void unsupported crypto operation", value->line, value->column, "unknown crypto kind");
      }
      /* Load expanded arguments into x0-x7 registers */
      size_t reg_idx = 0;
      for (size_t i = 0; i < value->arg_len && reg_idx < 8; i++) {
        IrValue *arg = value->args[i];
        if (arg->type == IR_TYPE_BYTE_VIEW || arg->type == IR_TYPE_MAYBE_BYTE_VIEW) {
          /* Byte view: load ptr into reg, len into next reg */
          if (!void_emit_byte_view_ptr_at(text, fun, arg, (unsigned)reg_idx, frame_size, scratch_slot + (unsigned)reg_idx, ctx, diag)) return false;
          if ((unsigned)reg_idx != reg_idx) {} /* cast warning guard */
          reg_idx++;
          if (reg_idx >= 8) break;
          if (!void_emit_byte_view_len_at(text, fun, arg, (unsigned)reg_idx, frame_size, scratch_slot + (unsigned)reg_idx, ctx, diag)) return false;
          reg_idx++;
        } else {
          /* Scalar: load value into reg */
          if (!void_emit_value_to_reg_at(text, fun, arg, (unsigned)reg_idx, frame_size, scratch_slot + (unsigned)reg_idx, ctx, diag)) return false;
          reg_idx++;
        }
      }
      size_t patch = z_aarch64_emit_bl_placeholder(text);
      if (!z_void_record_value_runtime_patch(ctx, helper, patch, value, diag)) return false;
      if (reg != 0) z_aarch64_emit_mov_w(text, reg, 0);
      return true;
    }
    default: {
      char actual[64];
      snprintf(actual, sizeof(actual), "unsupported value kind %d", value ? (int)value->kind : -1);
      return void_diag_at(diag, "direct AArch64 Void value kind is unsupported", value->line, value->column, actual);
    }
  }
}

static size_t void_function_frame_bytes(const IrFunction *fun) {
  uint32_t literal = 0;
  if (void_is_literal_return_function(fun, &literal, NULL)) return 0;
  unsigned base = (unsigned)(fun ? (fun->frame_bytes ? fun->frame_bytes : fun->local_len * 8) : 0);
  return void_align_size(base + VOID_SCRATCH_SLOT_COUNT * VOID_SCRATCH_SLOT_BYTES, 16);
}

size_t z_void64_stack_bytes_from_ir(const IrProgram *program) {
  size_t total = 0;
  for (size_t i = 0; program && i < program->function_len; i++) {
    total += void_function_frame_bytes(&program->functions[i]);
  }
  return total;
}

size_t z_void64_max_frame_bytes_from_ir(const IrProgram *program) {
  size_t max_frame = 0;
  for (size_t i = 0; program && i < program->function_len; i++) {
    size_t frame = void_function_frame_bytes(&program->functions[i]);
    if (frame > max_frame) max_frame = frame;
  }
  return max_frame;
}

static unsigned void_frame_size(const IrFunction *fun) {
  return (unsigned)void_function_frame_bytes(fun);
}

static void void_emit_epilogue(ZBuf *text, unsigned frame_size, bool restore_process_args) {
  if (frame_size > 0) z_aarch64_emit_add_sp_imm(text, frame_size);
  z_aarch64_emit_ldp_x29_x30_sp_post16(text);
  if (restore_process_args) z_aarch64_emit_ldp_x20_x21_sp_post16(text);
  z_aarch64_emit_ret(text);
}

static bool void_emit_instrs(ZBuf *text, const IrFunction *fun, const IrInstr *instrs, size_t len, unsigned frame_size, bool restore_process_args, VoidEmitContext *ctx, ZDiag *diag);

static bool void_emit_world_write(ZBuf *text, const IrFunction *fun, const IrInstr *instr, unsigned frame_size, VoidEmitContext *ctx, ZDiag *diag) {
  if (!instr || !instr->value) return void_diag_at(diag, "direct AArch64 Void World write requires bytes", instr ? instr->line : 1, instr ? instr->column : 1, "missing byte view");
  if (!void_emit_byte_view_ptr(text, fun, instr->value, 1, frame_size, ctx, diag)) return false;
  if (!void_emit_byte_view_len(text, fun, instr->value, 2, frame_size, ctx, diag)) return false;
  z_aarch64_emit_movz_w(text, 0, instr->field_offset == 2 ? 2u : 1u);
  size_t patch = z_aarch64_emit_bl_placeholder(text);
  if (!z_void_record_instr_runtime_patch(ctx, VOID_RUNTIME_WORLD_WRITE, patch, instr, diag)) return false;
  size_t ok_patch = z_aarch64_emit_cbz_w_placeholder(text, 0);
  z_aarch64_emit_brk(text);
  z_aarch64_patch_cond19(text, ok_patch, text->len);
  return true;
}

static bool void_emit_args_get_to_local(ZBuf *text, const IrFunction *fun, const IrValue *value, const IrLocal *local, unsigned frame_size, VoidEmitContext *ctx, ZDiag *diag) {
  if (!value || !value->left) return void_diag_at(diag, "direct AArch64 Void std.args.get requires an index", value ? value->line : 1, value ? value->column : 1, "missing index");
  if (!void_emit_value_to_reg(text, fun, value->left, 10, frame_size, ctx, diag)) return false;
  z_aarch64_emit_cmp_w(text, 10, 20);
  size_t in_range = z_aarch64_emit_b_cond_placeholder(text, 3); // unsigned lower
  z_aarch64_emit_movz_w(text, 8, 0);
  void_emit_store_local_w(text, fun, 8, local->index, 0, frame_size);
  void_emit_store_local_x(text, fun, 8, local->index, 8, frame_size);
  void_emit_store_local_w(text, fun, 8, local->index, 16, frame_size);
  size_t end_patch = z_aarch64_emit_b_placeholder(text);
  z_aarch64_patch_cond19(text, in_range, text->len);

  z_aarch64_emit_add_x_reg_lsl(text, 12, 21, 10, 3);
  z_aarch64_emit_load_x_imm(text, 12, 12, 0);
  z_aarch64_emit_movz_w(text, 10, 0);
  size_t loop_start = text->len;
  z_aarch64_emit_add_x_reg(text, 13, 12, 10);
  z_aarch64_emit_load_b_imm(text, 14, 13, 0);
  size_t done_patch = z_aarch64_emit_cbz_w_placeholder(text, 14);
  z_aarch64_emit_add_w_imm(text, 10, 10, 1);
  size_t loop_patch = z_aarch64_emit_b_placeholder(text);
  z_aarch64_patch_branch26(text, loop_patch, loop_start);
  z_aarch64_patch_cond19(text, done_patch, text->len);

  z_aarch64_emit_movz_w(text, 8, 1);
  void_emit_store_local_w(text, fun, 8, local->index, 0, frame_size);
  void_emit_store_local_x(text, fun, 12, local->index, 8, frame_size);
  void_emit_store_local_w(text, fun, 10, local->index, 16, frame_size);
  z_aarch64_patch_branch26(text, end_patch, text->len);
  return true;
}

static bool void_emit_instr(ZBuf *text, const IrFunction *fun, const IrInstr *instr, unsigned frame_size, bool restore_process_args, VoidEmitContext *ctx, ZDiag *diag) {
  if (instr->kind == IR_INSTR_WORLD_WRITE) {
    return void_emit_world_write(text, fun, instr, frame_size, ctx, diag);
  }
  if (instr->kind == IR_INSTR_LOCAL_SET) {
    if (instr->local_index >= fun->local_len) return void_diag_at(diag, "direct AArch64 Void local store is out of range", instr->line, instr->column, "invalid local");
    if (fun->locals[instr->local_index].type == IR_TYPE_BYTE_VIEW) {
      if (!void_emit_byte_view_ptr(text, fun, instr->value, 8, frame_size, ctx, diag)) return false;
      void_emit_store_local_x(text, fun, 8, instr->local_index, 0, frame_size);
      if (!void_emit_byte_view_len(text, fun, instr->value, 8, frame_size, ctx, diag)) return false;
      void_emit_store_local_w(text, fun, 8, instr->local_index, 8, frame_size);
      return true;
    }
    if (fun->locals[instr->local_index].type == IR_TYPE_ALLOC) {
      if (!instr->value || instr->value->kind != IR_VALUE_FIXED_BUF_ALLOC) return void_diag_at(diag, "direct AArch64 Void FixedBufAlloc local requires std.mem.fixedBufAlloc", instr->line, instr->column, "unsupported allocator initializer");
      if (!void_emit_byte_view_ptr(text, fun, instr->value->left, 8, frame_size, ctx, diag)) return false;
      void_emit_store_local_x(text, fun, 8, instr->local_index, 0, frame_size);
      if (!void_emit_byte_view_len(text, fun, instr->value->left, 8, frame_size, ctx, diag)) return false;
      void_emit_store_local_w(text, fun, 8, instr->local_index, 8, frame_size);
      z_aarch64_emit_movz_w(text, 8, 0);
      void_emit_store_local_w(text, fun, 8, instr->local_index, 12, frame_size);
      return true;
    }
    if (fun->locals[instr->local_index].type == IR_TYPE_VEC) {
      if (!instr->value || instr->value->kind != IR_VALUE_VEC_INIT) return void_diag_at(diag, "direct AArch64 Void Vec local requires std.mem.vec", instr->line, instr->column, "unsupported Vec initializer");
      if (!void_emit_byte_view_ptr(text, fun, instr->value->left, 8, frame_size, ctx, diag)) return false;
      void_emit_store_local_x(text, fun, 8, instr->local_index, 0, frame_size);
      z_aarch64_emit_movz_w(text, 8, 0);
      void_emit_store_local_w(text, fun, 8, instr->local_index, 8, frame_size);
      if (!void_emit_byte_view_len(text, fun, instr->value->left, 8, frame_size, ctx, diag)) return false;
      void_emit_store_local_w(text, fun, 8, instr->local_index, 12, frame_size);
      return true;
    }
    if (fun->locals[instr->local_index].type == IR_TYPE_MAYBE_BYTE_VIEW) {
      if (instr->value && instr->value->kind == IR_VALUE_ARGS_GET) {
        return void_emit_args_get_to_local(text, fun, instr->value, &fun->locals[instr->local_index], frame_size, ctx, diag);
      }
      if (!instr->value || instr->value->kind != IR_VALUE_ALLOC_BYTES || instr->value->local_index >= fun->local_len || fun->locals[instr->value->local_index].type != IR_TYPE_ALLOC) return void_diag_at(diag, "direct AArch64 Void allocation source is invalid", instr->line, instr->column, "invalid allocation");
      if (!void_emit_value_to_reg(text, fun, instr->value->left, 10, frame_size, ctx, diag)) return false;
      void_emit_load_local_w(text, fun, 8, instr->value->local_index, 12, frame_size);
      void_emit_load_local_w(text, fun, 9, instr->value->local_index, 8, frame_size);
      z_aarch64_emit_add_w_imm(text, 11, 8, 0);
      void_emit_binary_reg(text, IR_BIN_ADD, 11, 11, 10, false);
      z_aarch64_emit_cmp_w(text, 11, 9);
      size_t ok_patch = z_aarch64_emit_b_cond_placeholder(text, 9); // unsigned lower or same
      z_aarch64_emit_movz_w(text, 8, 0);
      void_emit_store_local_w(text, fun, 8, instr->local_index, 0, frame_size);
      void_emit_store_local_x(text, fun, 8, instr->local_index, 8, frame_size);
      void_emit_store_local_w(text, fun, 8, instr->local_index, 16, frame_size);
      size_t end_patch = z_aarch64_emit_b_placeholder(text);
      z_aarch64_patch_cond19(text, ok_patch, text->len);
      z_aarch64_emit_movz_w(text, 12, 1);
      void_emit_store_local_w(text, fun, 12, instr->local_index, 0, frame_size);
      void_emit_load_local_x(text, fun, 12, instr->value->local_index, 0, frame_size);
      z_aarch64_emit_add_x_reg(text, 12, 12, 8);
      void_emit_store_local_x(text, fun, 12, instr->local_index, 8, frame_size);
      void_emit_store_local_w(text, fun, 10, instr->local_index, 16, frame_size);
      void_emit_store_local_w(text, fun, 11, instr->value->local_index, 12, frame_size);
      z_aarch64_patch_branch26(text, end_patch, text->len);
      return true;
    }
    if (fun->locals[instr->local_index].type == IR_TYPE_MAYBE_SCALAR) {
      if (!instr->value) return void_diag_at(diag, "direct AArch64 Void Maybe scalar initializer is missing", instr->line, instr->column, "missing maybe value");
      if (instr->value->kind == IR_VALUE_MAYBE_SCALAR_LITERAL) {
        z_aarch64_emit_movz_w(text, 8, instr->value->data_len ? 1u : 0u);
        void_emit_store_local_w(text, fun, 8, instr->local_index, 0, frame_size);
        z_aarch64_emit_movz_x(text, 8, (uint64_t)instr->value->int_value);
        void_emit_store_local_x(text, fun, 8, instr->local_index, 8, frame_size);
        return true;
      }
      if (instr->value->kind == IR_VALUE_JSON_PARSE_BYTES) {
        if (instr->value->local_index >= fun->local_len || fun->locals[instr->value->local_index].type != IR_TYPE_ALLOC) {
          return void_diag_at(diag, "direct AArch64 Void JSON parse allocator is invalid", instr->line, instr->column, "invalid allocator");
        }
        if (!void_emit_value_to_reg(text, fun, instr->value, 8, frame_size, ctx, diag)) return false;
        z_aarch64_emit_cmp_x(text, 8, 31);
        size_t fail = z_aarch64_emit_b_cond_placeholder(text, 11); // signed less than
        void_emit_load_local_w(text, fun, 9, instr->value->local_index, 12, frame_size);
        z_aarch64_emit_mov_w(text, 10, 8);
        void_emit_binary_reg(text, IR_BIN_ADD, 11, 9, 10, false);
        void_emit_load_local_w(text, fun, 12, instr->value->local_index, 8, frame_size);
        z_aarch64_emit_cmp_w(text, 11, 12);
        size_t overflow = z_aarch64_emit_b_cond_placeholder(text, 8); // unsigned higher
        z_aarch64_emit_movz_w(text, 9, 1);
        void_emit_store_local_w(text, fun, 9, instr->local_index, 0, frame_size);
        void_emit_store_local_x(text, fun, 8, instr->local_index, 8, frame_size);
        void_emit_store_local_w(text, fun, 11, instr->value->local_index, 12, frame_size);
        size_t end = z_aarch64_emit_b_placeholder(text);
        z_aarch64_patch_cond19(text, fail, text->len);
        z_aarch64_patch_cond19(text, overflow, text->len);
        z_aarch64_emit_movz_w(text, 9, 0);
        void_emit_store_local_w(text, fun, 9, instr->local_index, 0, frame_size);
        void_emit_store_local_x(text, fun, 9, instr->local_index, 8, frame_size);
        z_aarch64_patch_branch26(text, end, text->len);
        return true;
      }
      if (!void_emit_value_to_reg(text, fun, instr->value, 8, frame_size, ctx, diag)) return false;
      z_aarch64_emit_cmp_x(text, 8, 31);
      size_t fail = z_aarch64_emit_b_cond_placeholder(text, 11); // signed less than
      z_aarch64_emit_movz_w(text, 9, 1);
      void_emit_store_local_w(text, fun, 9, instr->local_index, 0, frame_size);
      void_emit_store_local_x(text, fun, 8, instr->local_index, 8, frame_size);
      size_t end = z_aarch64_emit_b_placeholder(text);
      z_aarch64_patch_cond19(text, fail, text->len);
      z_aarch64_emit_movz_w(text, 9, 0);
      void_emit_store_local_w(text, fun, 9, instr->local_index, 0, frame_size);
      void_emit_store_local_x(text, fun, 9, instr->local_index, 8, frame_size);
      z_aarch64_patch_branch26(text, end, text->len);
      return true;
    }
    if (!void_emit_value_to_reg(text, fun, instr->value, 8, frame_size, ctx, diag)) return false;
    if (void_type_is_scalar64(fun->locals[instr->local_index].type)) void_emit_store_local_x(text, fun, 8, instr->local_index, 0, frame_size);
    else void_emit_store_local_w(text, fun, 8, instr->local_index, 0, frame_size);
    return true;
  }
  if (instr->kind == IR_INSTR_FIELD_STORE) {
    if (instr->local_index >= fun->local_len) return void_diag_at(diag, "direct AArch64 Void field store record is out of range", instr->line, instr->column, "invalid record local");
    if (!fun->locals[instr->local_index].is_record) return void_diag_at(diag, "direct AArch64 Void field store requires record local", instr->line, instr->column, "non-record local");
    if (!void_emit_value_to_reg(text, fun, instr->value, 8, frame_size, ctx, diag)) return false;
    void_emit_store_field(text, fun, 8, instr->local_index, instr->field_offset, instr->value ? instr->value->type : IR_TYPE_I32, frame_size);
    return true;
  }
  if (instr->kind == IR_INSTR_INDEX_STORE) {
    if (instr->array_index >= fun->local_len) return void_diag_at(diag, "direct AArch64 Void indexed store array is out of range", instr->line, instr->column, "invalid array local");
    const IrLocal *local = &fun->locals[instr->array_index];
    unsigned const_index = 0;
    if (local->is_array && local->element_type != IR_TYPE_U8 && void_const_u32_value(instr->index, &const_index) && const_index < local->array_len) {
      if (!void_emit_value_to_reg(text, fun, instr->value, 10, frame_size, ctx, diag)) return false;
      void_emit_store_local_w(text, fun, 10, instr->array_index, const_index * 4u, frame_size);
      return true;
    }
    if (local->is_array && (local->element_type == IR_TYPE_U32 || local->element_type == IR_TYPE_I32 || local->element_type == IR_TYPE_USIZE)) {
      if (!void_emit_value_to_reg(text, fun, instr->value, 10, frame_size, ctx, diag)) return false;
      if (!instr->index || !void_emit_value_to_reg(text, fun, instr->index, 8, frame_size, ctx, diag)) return false;
      z_aarch64_emit_movz_w(text, 9, local->array_len);
      z_aarch64_emit_cmp_w(text, 8, 9);
      size_t ok_patch = z_aarch64_emit_b_cond_placeholder(text, 3); // unsigned lower
      z_aarch64_emit_brk(text);
      z_aarch64_patch_cond19(text, ok_patch, text->len);
      z_aarch64_emit_add_x_sp_imm(text, 9, void_local_slot_offset(fun, instr->array_index, 0, frame_size));
      z_aarch64_emit_add_x_reg_lsl(text, 9, 9, 8, 2);
      z_aarch64_emit_store_w_imm(text, 10, 9, 0);
      return true;
    }
    if (!local->is_array || local->element_type != IR_TYPE_U8) return void_diag_at(diag, "direct AArch64 Void indexed store requires [N]u8 or integer arrays", instr->line, instr->column, "unsupported array local");
    if (!void_emit_value_to_reg(text, fun, instr->value, 10, frame_size, ctx, diag)) return false;
    if (!instr->index || !void_emit_value_to_reg(text, fun, instr->index, 8, frame_size, ctx, diag)) return false;
    z_aarch64_emit_movz_w(text, 9, local->array_len);
    z_aarch64_emit_cmp_w(text, 8, 9);
    size_t ok_patch = z_aarch64_emit_b_cond_placeholder(text, 3); // unsigned lower
    z_aarch64_emit_brk(text);
    z_aarch64_patch_cond19(text, ok_patch, text->len);
    z_aarch64_emit_add_x_sp_imm(text, 9, void_local_slot_offset(fun, instr->array_index, 0, frame_size));
    z_aarch64_emit_add_x_reg(text, 9, 9, 8);
    z_aarch64_emit_store_b_imm(text, 10, 9, 0);
    return true;
  }
  if (instr->kind == IR_INSTR_EXPR) {
    return !instr->value || void_emit_value_to_reg(text, fun, instr->value, 0, frame_size, ctx, diag);
  }
  if (instr->kind == IR_INSTR_RETURN) {
    if (instr->value && !void_emit_value_to_reg(text, fun, instr->value, 0, frame_size, ctx, diag)) return false;
    void_emit_epilogue(text, frame_size, restore_process_args);
    return true;
  }
  if (instr->kind == IR_INSTR_IF) {
    if (!void_emit_value_to_reg(text, fun, instr->value, 0, frame_size, ctx, diag)) return false;
    size_t false_patch = z_aarch64_emit_cbz_w_placeholder(text, 0);
    if (!void_emit_instrs(text, fun, instr->then_instrs, instr->then_len, frame_size, restore_process_args, ctx, diag)) return false;
    if (instr->else_len > 0) {
      size_t end_patch = z_aarch64_emit_b_placeholder(text);
      z_aarch64_patch_cond19(text, false_patch, text->len);
      if (!void_emit_instrs(text, fun, instr->else_instrs, instr->else_len, frame_size, restore_process_args, ctx, diag)) return false;
      z_aarch64_patch_branch26(text, end_patch, text->len);
    } else {
      z_aarch64_patch_cond19(text, false_patch, text->len);
    }
    return true;
  }
  if (instr->kind == IR_INSTR_WHILE) {
    size_t loop_start = text->len;
    if (!void_emit_value_to_reg(text, fun, instr->value, 0, frame_size, ctx, diag)) return false;
    size_t false_patch = z_aarch64_emit_cbz_w_placeholder(text, 0);
    if (!void_emit_instrs(text, fun, instr->then_instrs, instr->then_len, frame_size, restore_process_args, ctx, diag)) return false;
    size_t loop_patch = z_aarch64_emit_b_placeholder(text);
    z_aarch64_patch_branch26(text, loop_patch, loop_start);
    z_aarch64_patch_cond19(text, false_patch, text->len);
    return true;
  }
  char actual[64];
  snprintf(actual, sizeof(actual), "unsupported instruction kind %d", instr ? (int)instr->kind : -1);
  return void_diag_at(diag, "direct AArch64 Void instruction kind is unsupported", instr->line, instr->column, actual);
}

static bool void_emit_instrs(ZBuf *text, const IrFunction *fun, const IrInstr *instrs, size_t len, unsigned frame_size, bool restore_process_args, VoidEmitContext *ctx, ZDiag *diag) {
  for (size_t i = 0; i < len; i++) {
    if (!void_emit_instr(text, fun, &instrs[i], frame_size, restore_process_args, ctx, diag)) return false;
  }
  return true;
}

static bool void_validate_function(const IrFunction *fun, ZDiag *diag) {
  uint32_t ignored = 0;
  if (void_is_literal_return_function(fun, &ignored, NULL)) return true;
  if (fun->param_count > 8) return void_diag_at(diag, "direct AArch64 Void object backend supports at most eight parameters", fun->line, fun->column, fun->name);
  if (fun->return_type != IR_TYPE_VOID && !void_type_is_scalar(fun->return_type)) {
    return void_diag_at(diag, "direct AArch64 Void object backend currently supports only Void and primitive integer returns", fun->line, fun->column, fun->name);
  }
  for (size_t i = 0; i < fun->local_len; i++) {
    if (fun->locals[i].type == IR_TYPE_BYTE_VIEW) {
      if (fun->locals[i].is_param) {
        return void_diag_at(diag, "direct AArch64 Void object backend does not yet support byte-view parameters", fun->locals[i].line, fun->locals[i].column, fun->locals[i].name);
      }
      continue;
    }
    if (fun->locals[i].is_array && (fun->locals[i].element_type == IR_TYPE_U8 || fun->locals[i].element_type == IR_TYPE_U32 || fun->locals[i].element_type == IR_TYPE_I32 || fun->locals[i].element_type == IR_TYPE_USIZE)) continue;
    if (fun->locals[i].is_record) continue;
    if (fun->locals[i].type == IR_TYPE_ALLOC || fun->locals[i].type == IR_TYPE_MAYBE_BYTE_VIEW || fun->locals[i].type == IR_TYPE_MAYBE_SCALAR) continue;
    if (fun->locals[i].type == IR_TYPE_VEC) continue;
    if (fun->locals[i].is_array || !void_type_is_scalar(fun->locals[i].type)) {
      return void_diag_at(diag, "direct AArch64 Void object backend currently supports only primitive scalar locals", fun->locals[i].line, fun->locals[i].column, fun->locals[i].name);
    }
  }
  return true;
}

static bool void_emit_function_text(ZBuf *text, const IrFunction *fun, VoidEmitContext *ctx, ZDiag *diag) {
  uint32_t literal = 0;
  if (void_is_literal_return_function(fun, &literal, NULL)) {
    z_aarch64_emit_literal_return(text, literal);
    return true;
  }

  unsigned frame_size = void_frame_size(fun);
  bool seed_process_args = ctx && ctx->seed_main_process_args && fun->is_exported && fun->name && strcmp(fun->name, "main") == 0;
  if (seed_process_args) z_aarch64_emit_stp_x20_x21_sp_pre16(text);
  z_aarch64_emit_stp_x29_x30_sp_pre16(text);
  z_aarch64_emit_mov_x29_sp(text);
  if (frame_size > 0) z_aarch64_emit_sub_sp_imm(text, frame_size);
  if (seed_process_args) {
    z_aarch64_emit_mov_x(text, 20, 0);
    z_aarch64_emit_mov_x(text, 21, 1);
  }
  for (size_t i = 0; i < fun->param_count; i++) {
    if (void_type_is_scalar64(fun->locals[i].type)) void_emit_store_local_x(text, fun, (unsigned)i, (unsigned)i, 0, frame_size);
    else void_emit_store_local_w(text, fun, (unsigned)i, (unsigned)i, 0, frame_size);
  }
  if (!void_emit_instrs(text, fun, fun->instrs, fun->instr_len, frame_size, seed_process_args, ctx, diag)) return false;
  if (fun->instr_len == 0 || fun->instrs[fun->instr_len - 1].kind != IR_INSTR_RETURN) void_emit_epilogue(text, frame_size, seed_process_args);
  return true;
}

static unsigned void_rodata_base_offset(const IrProgram *program) {
  if (!program || program->data_segment_len == 0) return 0;
  unsigned base = program->data_segments[0].offset;
  for (size_t i = 1; i < program->data_segment_len; i++) {
    if (program->data_segments[i].offset < base) base = program->data_segments[i].offset;
  }
  return base;
}

static void void_append_rodata(ZBuf *rodata, const IrProgram *program, unsigned base_offset) {
  for (size_t i = 0; program && i < program->data_segment_len; i++) {
    const IrDataSegment *segment = &program->data_segments[i];
    void_pad_to(rodata, segment->offset - base_offset);
    append_bytes(rodata, (const char *)segment->bytes, segment->len);
  }
}

bool z_emit_void64_object_from_ir(const IrProgram *program, ZBuf *out, ZDiag *diag) {
  if (!program || !out) return void_diag(diag, "direct Void backend received no program");
  if (!program->mir_valid) {
    bool ok = void_diag_at(diag, program->mir_message[0] ? program->mir_message : "direct backend lowering failed", program->mir_line, program->mir_column, program->mir_actual);
    z_diag_set_backend_blocker(diag, &program->backend_blocker);
    return ok;
  }
  if (program->function_len == 0) return void_diag_at(diag, "direct AArch64 Void object backend requires at least one exported function", 1, 1, "empty program");
  bool has_export = false;
  for (size_t i = 0; i < program->function_len; i++) {
    if (program->functions[i].is_exported) has_export = true;
    if (!void_validate_function(&program->functions[i], diag)) return false;
  }
  if (!has_export) return void_diag_at(diag, "direct AArch64 Void object backend requires at least one exported function", 1, 1, "no exported function");

  ZBuf text;
  ZBuf rodata;
  ZBuf relocs;
  zbuf_init(&text);
  zbuf_init(&rodata);
  zbuf_init(&relocs);
  bool has_rodata = program->readonly_data_bytes > 0 || program->data_segment_len > 0;
  unsigned rodata_base_offset = void_rodata_base_offset(program);
  if (has_rodata) void_append_rodata(&rodata, program, rodata_base_offset);
  size_t *offsets = z_checked_calloc(program->function_len, sizeof(size_t));
  uint32_t *string_offsets = z_checked_calloc(program->function_len, sizeof(uint32_t));
  if (!offsets) {
    free(string_offsets);
    zbuf_free(&relocs);
    zbuf_free(&rodata);
    zbuf_free(&text);
    return void_diag(diag, "out of memory while emitting Void object");
  }
  if (!string_offsets) {
    free(offsets);
    zbuf_free(&relocs);
    zbuf_free(&rodata);
    zbuf_free(&text);
    return void_diag(diag, "out of memory while emitting Void symbols");
  }

  ZBuf strings;
  zbuf_init(&strings);
  append_u8(&strings, 0);
  VoidEmitContext ctx = {
    .program = program,
    .function_offsets = offsets,
    .function_count = program->function_len,
    .rodata_base_offset = rodata_base_offset,
    .pie_relative_data = true,
    .seed_main_process_args = true
  };
  for (size_t i = 0; i < program->function_len; i++) {
    const IrFunction *fun = &program->functions[i];
    void_pad_to(&text, void_align_size(text.len, 4));
    offsets[i] = text.len;
    if (!void_emit_function_text(&text, fun, &ctx, diag)) {
      zbuf_free(&strings);
      free(string_offsets);
      free(offsets);
      z_void_emit_context_free(&ctx);
      zbuf_free(&relocs);
      zbuf_free(&rodata);
      zbuf_free(&text);
      return false;
    }
    string_offsets[i] = (uint32_t)strings.len;
    zbuf_append_char(&strings, '_');
    zbuf_append(&strings, fun->name ? fun->name : "zerox_fn");
    append_u8(&strings, 0);
  }
  z_void_append_call_relocations(&relocs, &ctx);
  if (has_rodata) {
    z_void_append_data_relocations(&relocs, &ctx, (unsigned)program->function_len);
  }
  uint32_t next_runtime_symbol = (uint32_t)program->function_len + (has_rodata ? 1u : 0u);
  uint32_t runtime_symbol_indices[VOID_RUNTIME_HELPER_COUNT] = {0};
  for (unsigned helper = 0; helper < VOID_RUNTIME_HELPER_COUNT; helper++) {
    VoidRuntimeHelper runtime_helper = (VoidRuntimeHelper)helper;
    if (z_void_runtime_patch_count(&ctx, runtime_helper) == 0) continue;
    runtime_symbol_indices[helper] = next_runtime_symbol++;
    z_void_append_runtime_relocations(&relocs, &ctx, runtime_helper, runtime_symbol_indices[helper]);
  }

  const uint32_t const_addr = has_rodata ? (uint32_t)void_align_size(text.len, 8) : 0;
  const uint32_t nsyms = next_runtime_symbol;
  uint32_t rodata_string_offset = 0;
  if (has_rodata) {
    rodata_string_offset = (uint32_t)strings.len;
    zbuf_append(&strings, "l_.zerox_rodata");
    append_u8(&strings, 0);
  }
  uint32_t runtime_string_offsets[VOID_RUNTIME_HELPER_COUNT] = {0};
  for (unsigned helper = 0; helper < VOID_RUNTIME_HELPER_COUNT; helper++) {
    VoidRuntimeHelper runtime_helper = (VoidRuntimeHelper)helper;
    if (z_void_runtime_patch_count(&ctx, runtime_helper) == 0) continue;
    runtime_string_offsets[helper] = (uint32_t)strings.len;
    zbuf_append(&strings, z_void_runtime_helper_symbol(runtime_helper));
    append_u8(&strings, 0);
  }

  ZVoidSymbol *symbols = z_checked_calloc(nsyms, sizeof(ZVoidSymbol));
  if (!symbols) {
    zbuf_free(&strings);
    free(string_offsets);
    free(offsets);
    z_void_emit_context_free(&ctx);
    zbuf_free(&relocs);
    zbuf_free(&rodata);
    zbuf_free(&text);
    return void_diag(diag, "out of memory while emitting Void symbols");
  }
  size_t symbol_len = 0;
  for (size_t i = 0; i < program->function_len; i++) {
    symbols[symbol_len++] = (ZVoidSymbol){
      .string_offset = string_offsets[i],
      .type = program->functions[i].is_exported ? 0x0f : 0x0e,
      .section = 1,
      .value = offsets[i]
    };
  }
  if (has_rodata) {
    symbols[symbol_len++] = (ZVoidSymbol){
      .string_offset = rodata_string_offset,
      .type = 0x0e,
      .section = 2,
      .value = const_addr
    };
  }
  for (unsigned helper = 0; helper < VOID_RUNTIME_HELPER_COUNT; helper++) {
    VoidRuntimeHelper runtime_helper = (VoidRuntimeHelper)helper;
    if (z_void_runtime_patch_count(&ctx, runtime_helper) == 0) continue;
    symbols[symbol_len++] = (ZVoidSymbol){ .string_offset = runtime_string_offsets[helper], .type = 0x01 };
  }

  const uint32_t text_reloc_count = (uint32_t)z_void_text_relocation_count(&ctx);
  ZVoidObjectImage image = {
    .text = &text,
    .rodata = has_rodata ? &rodata : NULL,
    .relocs = &relocs,
    .strings = &strings,
    .symbols = symbols,
    .symbol_len = symbol_len,
    .text_reloc_count = text_reloc_count
  };
  z_void_write_object64(out, &image);
  free(symbols);

  z_void_emit_context_free(&ctx);
  free(string_offsets);
  free(offsets);
  zbuf_free(&strings);
  zbuf_free(&relocs);
  zbuf_free(&rodata);
  zbuf_free(&text);
  return true;
}

static const IrFunction *void_find_executable_main(const IrProgram *program, ZDiag *diag, unsigned *out_index) {
  const IrFunction *fun = NULL;
  unsigned index = 0;
  for (size_t i = 0; program && i < program->function_len; i++) {
    if (program->functions[i].is_exported && strcmp(program->functions[i].name, "main") == 0) {
      if (fun) {
        void_diag_at(diag, "direct AArch64 Void executable backend requires exactly one exported main function", program->functions[i].line, program->functions[i].column, program->functions[i].name);
        return NULL;
      }
      fun = &program->functions[i];
      index = (unsigned)i;
    }
  }
  if (!fun) {
    void_diag_at(diag, "direct AArch64 Void executable backend requires an exported main function", 1, 1, "missing main");
    return NULL;
  }
  if (fun->param_count != 0) {
    void_diag_at(diag, "direct AArch64 Void executable main must not take parameters", fun->line, fun->column, fun->name);
    return NULL;
  }
  if (fun->return_type != IR_TYPE_VOID && !void_type_is_scalar32(fun->return_type)) {
    void_diag_at(diag, "direct AArch64 Void executable main must return Void or a 32-bit-or-smaller scalar", fun->line, fun->column, fun->name);
    return NULL;
  }
  if (out_index) *out_index = index;
  return fun;
}

static size_t void_emit_exe_start_stub(ZBuf *text) {
  z_aarch64_emit_mov_x(text, 20, 0);
  z_aarch64_emit_mov_x(text, 21, 1);
  size_t patch = z_aarch64_emit_b_placeholder(text); // tail-call main so it returns to dyld's LC_MAIN trampoline
  return patch;
}

static size_t void_emit_exe_world_write(ZBuf *text) {
  size_t offset = text->len;
  z_aarch64_emit_movz_x(text, 16, 0x02000004u); // Darwin SYS_write(fd=x0, buf=x1, len=x2)
  z_aarch64_emit_svc(text, 0x80);
  z_aarch64_emit_movz_w(text, 0, 0);   // report success to the checked std.io shim
  z_aarch64_emit_ret(text);
  return offset;
}

bool z_emit_void64_exe_from_ir(const IrProgram *program, ZBuf *out, ZDiag *diag) {
  if (!program || !out) return void_diag(diag, "direct Void executable backend received no program");
  if (!program->mir_valid) {
    bool ok = void_diag_at(diag, program->mir_message[0] ? program->mir_message : "direct backend lowering failed", program->mir_line, program->mir_column, program->mir_actual);
    z_diag_set_backend_blocker(diag, &program->backend_blocker);
    return ok;
  }
  unsigned main_index = 0;
  if (!void_find_executable_main(program, diag, &main_index)) return false;
  for (size_t i = 0; i < program->function_len; i++) {
    if (!void_validate_function(&program->functions[i], diag)) return false;
  }

  ZBuf text;
  ZBuf rodata;
  zbuf_init(&text);
  zbuf_init(&rodata);
  bool has_rodata = program->readonly_data_bytes > 0 || program->data_segment_len > 0;
  unsigned rodata_base_offset = void_rodata_base_offset(program);
  if (has_rodata) void_append_rodata(&rodata, program, rodata_base_offset);

  size_t *offsets = z_checked_calloc(program->function_len, sizeof(size_t));
  if (!offsets) {
    zbuf_free(&rodata);
    zbuf_free(&text);
    return void_diag(diag, "out of memory while emitting Void executable");
  }

  VoidEmitContext ctx = {
    .program = program,
    .function_offsets = offsets,
    .function_count = program->function_len,
    .rodata_base_offset = rodata_base_offset,
    .pie_relative_data = true
  };
  size_t start_call_patch = void_emit_exe_start_stub(&text);
  void_pad_to(&text, void_align_size(text.len, 16));
  for (size_t i = 0; i < program->function_len; i++) {
    void_pad_to(&text, void_align_size(text.len, 4));
    offsets[i] = text.len;
    if (!void_emit_function_text(&text, &program->functions[i], &ctx, diag)) {
      z_void_emit_context_free(&ctx);
      free(offsets);
      zbuf_free(&rodata);
      zbuf_free(&text);
      return false;
    }
  }

  if (z_void_has_unsupported_exe_runtime_patches(&ctx)) {
    z_void_emit_context_free(&ctx);
    free(offsets);
    zbuf_free(&rodata);
    zbuf_free(&text);
    return void_diag_at(diag, "direct AArch64 Void executable runtime helpers require object emission and an explicit runtime link step", 1, 1, "use --emit obj and link zero_runtime.c");
  }

  size_t world_write_offset = 0;
  if (z_void_runtime_patch_count(&ctx, VOID_RUNTIME_WORLD_WRITE) > 0) {
    void_pad_to(&text, void_align_size(text.len, 4));
    world_write_offset = void_emit_exe_world_write(&text);
  }
  z_aarch64_patch_branch26(&text, start_call_patch, offsets[main_index]);
  for (size_t i = 0; i < ctx.call_patch_len; i++) {
    const VoidCallPatch *patch = &ctx.call_patches[i];
    z_aarch64_patch_branch26(&text, patch->patch_offset, offsets[patch->callee_index]);
  }
  const VoidPatchList *world_write_patches = z_void_runtime_patch_list(&ctx, VOID_RUNTIME_WORLD_WRITE);
  for (size_t i = 0; world_write_patches && i < world_write_patches->len; i++) {
    z_aarch64_patch_branch26(&text, world_write_patches->items[i].patch_offset, world_write_offset);
  }

  const char *code_signature_id = "zerox-direct";
  ZBuf rebase;
  zbuf_init(&rebase);
  ZVoidExecutableLayout layout;
  z_void_compute_executable64_layout(&layout, &text, has_rodata ? &rodata : NULL, &rebase, code_signature_id);
  for (size_t i = 0; i < ctx.data_patch_len; i++) {
    const VoidDataPatch *patch = &ctx.data_patches[i];
    uint64_t addr = layout.base_addr + layout.rodata_offset + (patch->data_offset - rodata_base_offset);
    if (ctx.pie_relative_data) z_aarch64_patch_adrp_add(&text, patch->patch_offset, layout.base_addr + layout.text_offset + patch->patch_offset, addr);
    else z_void_patch_u64(&text, patch->patch_offset, addr);
  }
  if (ctx.data_patch_len > 0 && !ctx.pie_relative_data) {
    append_u8(&rebase, 0x11); // REBASE_OPCODE_SET_TYPE_IMM | REBASE_TYPE_POINTER
    for (size_t i = 0; i < ctx.data_patch_len; i++) {
      append_u8(&rebase, 0x21); // REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB, __TEXT segment
      z_void_append_uleb128(&rebase, layout.text_offset + ctx.data_patches[i].patch_offset);
      append_u8(&rebase, 0x51); // REBASE_OPCODE_DO_REBASE_IMM_TIMES, once
    }
    append_u8(&rebase, 0x00);
  }
  z_void_compute_executable64_layout(&layout, &text, has_rodata ? &rodata : NULL, &rebase, code_signature_id);
  ZVoidExecutableImage image = {
    .text = &text,
    .rodata = has_rodata ? &rodata : NULL,
    .rebase = &rebase,
    .layout = layout,
    .code_signature_id = code_signature_id
  };
  z_void_write_executable64(out, &image);

  z_void_emit_context_free(&ctx);
  free(offsets);
  zbuf_free(&rebase);
  zbuf_free(&rodata);
  zbuf_free(&text);
  return true;
}
