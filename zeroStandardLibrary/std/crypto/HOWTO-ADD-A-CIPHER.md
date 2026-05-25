# How to Add a Cipher or Crypto Library to `std.crypto`

This guide explains the architecture and steps for adding a new cipher,
hash function, or cryptographic primitive to Zero's standard library.

## Architecture Overview

Zero's crypto stack has three layers:

```
┌──────────────────────────────────────────┐
│  Layer 3: Pure Zero modules (.0 files)   │
│  e.g. encrypt.0, layered.0, key.0        │
│  No C code needed — pure Zero code.      │
├──────────────────────────────────────────┤
│  Layer 2: Native helper (std_sig + IR)   │
│  Declared with `!` in .0 file, listed in │
│  std_sig.c, lowered in ir.c, emitted via │
│  backend emitters. Calls C runtime.      │
├──────────────────────────────────────────┤
│  Layer 1: C runtime (zero_crypto.c/.h)   │
│  Portable C implementation. Wrapper      │
│  functions prefixed `zero_crypto_*`      │
│  bridge from emitted code to primitives. │
└──────────────────────────────────────────┘
```

## Three Ways to Add Crypto

### Approach A: Pure Zero (easiest, no C code)

For functions that don't need native performance or OS integration,
implement entirely in Zero. The existing `!` marker is NOT used.

**Example:** `cipher.0`'s `keySize`, `blockSize`, `methodName` — all pure Zero.

**Steps:**
1. Create/edit the `.0` file in `zeroStandardLibrary/std/crypto/`
2. Write function bodies using Zero's operators (`^`, `&`, `|`, `<<`, `>>`, `+`, `-`, `*`, `/`, `%`)
3. Do NOT use `!` — the function has a real body
4. No changes needed in `std_sig.c`, `ir.c`, or `zero_crypto.c`

**Constraints:**
- Array constants use `[N]u8 [val1, val2, ...]` or `[N]u8 [default; N]` syntax
- Use `let i u32 0_u32` + `while < i n` for loops
- Use `store<u8> val dst i` for indexed writes
- Use `data[i]` for indexed reads
- Return type must come right after the function name (before parameters)

### Approach B: Native Helper with New C Implementation

For ciphers that need a C implementation (e.g. DES, Blowfish) or when
performance matters.

**Files to modify (7 files minimum):**

```
native/zero-c/
├── runtime/
│   ├── zero_crypto.h       # 1. Declare wrapper function
│   ├── zero_crypto.c       # 2. Implement wrapper + cipher logic
├── include/
│   ├── zero_runtime.h      # 3. Declare for emitted code
│   ├── zero.h              # 4. Add CRYPTO_ op code constant
├── src/
│   ├── std_sig.c           # 5. Add entry with arg count
│   ├── ir.c                # 6. Add IR lowering rule
│   ├── emit_elf64.c        # 7. Add switch case + elf_crypto_kind_to_runtime
│   ├── elf_emit_state.h    # 8. Add ELF_RUNTIME_ enum value
│   ├── elf_emit_state.c    # 9. Add symbol name string
│   ├── emit_coff.c         # 10. Add switch case
│   ├── coff_emit_state.h   # 11. Add COFF_RUNTIME_ enum value
│   ├── coff_emit_state.c   # 12. Add symbol name string
│   ├── emit_macho64.c      # 13. Add switch case
│   ├── macho_emit_state.h  # 14. Add MACHO_RUNTIME_ enum value
│   ├── macho_emit_state.c  # 15. Add symbol name string
```

**Step-by-step:**

#### Step 1: Implement in C

Edit `zero_crypto.c` to add the cipher implementation. Create a wrapper
function matching the pattern:

```c
/* MyCipher: (key_ptr, key_len, data_ptr, data_len, out_ptr, out_cap) -> u32 */
uint32_t zero_crypto_mycipher(
    const unsigned char *key, size_t key_len,
    const unsigned char *data, size_t data_len,
    unsigned char *out, size_t out_cap)
{
    // Your C implementation here
    return bytes_written; // 0 on error
}
```

**Calling convention for emitted code:**
- `Span<u8>` params expand to `ptr, len` (2 args each)
- `MutSpan<u8>` params expand to `ptr, cap` (2 args each)
- `u32` scalars pass as single values
- Applies to all 3 backends (ELF64, COFF, Mach-O)

Declare in `zero_crypto.h` and `zero_runtime.h`.

#### Step 2: Add CRYPTO_ op code

In `zero.h`, add to the `#define` block:

```c
#define CRYPTO_MYCIPHER 12   // next available number
```

#### Step 3: Register in std_sig.c

```c
{"std.crypto.mycipher.funcname", "u32", 5, "codec", "target-neutral", "writes caller buffer", true},
```

The 3rd field (`5`) is the Zero-level arg count including `world`
but NOT counting the return type.

#### Step 4: Add IR lowering in ir.c

Add after the existing crypto lowering rules (around line 1775):

```c
/* std.crypto.mycipher.funcname: (key, data, out, world) -> u32 */
if (strcmp(callee_name, "std.crypto.mycipher.funcname") == 0 && expr->args.len == 3) {
    IrValue *key_view = NULL, *data_view = NULL, *out_view = NULL;
    if (!ir_lower_byte_view(program, ir, fun, expr->args.items[0], &key_view) ||
        !ir_lower_byte_view(program, ir, fun, expr->args.items[1], &data_view) ||
        !ir_lower_byte_view(program, ir, fun, expr->args.items[2], &out_view)) {
        ir_free_value(key_view); ir_free_value(data_view); ir_free_value(out_view);
        free(callee_name); return false;
    }
    IrValue *value = ir_new_value(ir, IR_VALUE_CRYPTO_HELPER, IR_TYPE_U32,
                                  expr->line, expr->column);
    value->error_code = CRYPTO_MYCIPHER;
    ir_value_push_arg(ir, value, key_view);
    ir_value_push_arg(ir, value, data_view);
    ir_value_push_arg(ir, value, out_view);
    free(callee_name);
    *out = value;
    return true;
}
```

#### Step 5: Wire all 3 backends

For each backend, 3 files need changes:

**state.h** — add enum value:
```c
ELF_RUNTIME_CRYPTO_MYCIPHER,
// (after ELF_RUNTIME_CRYPTO_RANDOM_BYTES, before ELF_RUNTIME_HELPER_COUNT)
```

**state.c** — add symbol name:
```c
"zero_crypto_mycipher",
```

**emit*.c** — add case in existing `CRYPTO_HELPER` switch:
```c
case CRYPTO_MYCIPHER: helper = ELF_RUNTIME_CRYPTO_MYCIPHER; break;
```

Also add to `elf_crypto_kind_to_runtime()` in `emit_elf64.c`:
```c
case CRYPTO_MYCIPHER: return ELF_RUNTIME_CRYPTO_MYCIPHER;
```

#### Step 6: Declare in .0 file

With return type FIRST:
```zero
pub fn myFunc u32 key Span<u8> data Span<u8> out MutSpan<u8> world World !
```

The `!` marks this as a native helper. The return type `u32` must come
right after the function name.

### Approach C: Adapting Existing C Code

If `zero_crypto.c` already has the primitive (e.g. `zero_sha512()` exists
but isn't wired up), you only need steps 2-6. Write a thin wrapper:

```c
uint32_t zero_crypto_sha512(const unsigned char *data, size_t data_len,
                            unsigned char *out, size_t out_cap) {
    return zero_sha512(data, data_len, out, out_cap);
}
```

## Common Patterns

### Signature Conventions

| Zero signature | C signature (after expansion) |
|---|---|
| `fn f u32 data Span<u8> out MutSpan<u8> world World !` | `f(data_ptr, data_len, out_ptr, out_cap) -> u32` |
| `fn f u32 key Span<u8> data Span<u8> out MutSpan<u8> world World !` | `f(key_ptr, key_len, data_ptr, data_len, out_ptr, out_cap) -> u32` |
| `fn f u32 key Span<u8> iv Span<u8> data Span<u8> mode u32 out MutSpan<u8> world World !` | `f(key_ptr, key_len, iv_ptr, iv_len, data_ptr, data_len, mode, out_ptr, out_cap) -> u32` |

### Checklist for Adding a New Native Helper

- [ ] C implementation in `zero_crypto.c` with `zero_crypto_*` wrapper
- [ ] Declaration in `zero_crypto.h` and `zero_runtime.h`
- [ ] `CRYPTO_*` constant in `zero.h`
- [ ] `std_sig.c` entry
- [ ] `ir.c` lowering rule
- [ ] `elf_emit_state.h` + `.c` (enum + symbol)
- [ ] `emit_elf64.c` (switch + crypto_kind_to_runtime)
- [ ] `coff_emit_state.h` + `.c` (enum + symbol)
- [ ] `emit_coff.c` (switch case)
- [ ] `macho_emit_state.h` + `.c` (enum + symbol)
- [ ] `emit_macho64.c` (switch case)
- [ ] `.0` file declaration with `!` and return type FIRST
- [ ] Build: `gcc -std=c11 -Iinclude src/*.c -o .zero/bin/zero`
- [ ] Check: `.zero/bin/zero check path/to/file.0`

## Important Gotchas

1. **Return type position**: When using `Span<u8>` or `MutSpan<u8>` in
   parameter types, the return type must come RIGHT AFTER the function
   name, NOT after the parameters. Wrong: `fn encrypt key Span<u8> ... u32 world`
   Right: `fn encrypt u32 key Span<u8> ... world`.

2. **Arg count in std_sig.c**: Count ALL Zero arguments including `world`
   but NOT the return type.

3. **C wrapper ordering**: The C wrapper's parameter order must match the
   Zero function's parameter order WITH byte views expanded to ptr/len pairs.

4. **Parallel enum positions**: The enum value in `*_emit_state.h` and the
   symbol name in `*_emit_state.c` must be at the SAME INDEX position
   for the lookup to work.

5. **`!` vs body**: Use `!` ONLY when the function is backed by a native
   C helper. Without `!`, the function body is pure Zero code and no
   `std_sig.c` entry is needed.
