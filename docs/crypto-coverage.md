# Crypto Library Coverage Analysis

Botan-backed crypto standard library (`std.crypto`). All 49 CRYPTO_ ops (1–49) are
defined in `zerox.h`, registered in `std_sig.c`, lowered in `ir.c`, and emitted for
ELF64, COFF (Windows PE), and Void64 (AArch64) targets. **Mach-O (macOS) emitter
lacks crypto support.**

## Architecture (3-layer Botan-backed stack)

| Layer | Path | Content |
|---|---|---|
| C Runtime | `native/zerox-c/runtime/zerox_crypto.c` | Botan FFI wrappers |
| Zero .0 files | `zeroStandardLibrary/std/crypto/*.0` | 17 module files |
| Emitters | `emit_elf64.c`, `emit_coff.c`, `emit_Void64.c` | All 49 CRYPTO_ ops wired |

## Fully Implemented (Botan-backed, complete)

| Category | Details |
|---|---|
| **Hashing** | SHA-256, SHA-384, SHA-512, SHA-3-256, SHA-3-384, SHA-3-512, BLAKE2b, BLAKE2s, SHAKE-128, SHAKE-256 |
| **MAC** | HMAC-SHA256, HMAC-SHA384, HMAC-SHA512 |
| **KDF** | PBKDF2(HMAC-SHA256) |
| **CSPRNG** | System entropy `randomBytes` |
| **Stream Ciphers** | ChaCha20, XChaCha20 (24-byte nonce), Salsa20 |
| **AEAD** | ChaCha20-Poly1305 (with AAD) |
| **Block Ciphers** (multi-mode: ECB/CBC/CFB/OFB/CTR/GCM) | Blowfish, Twofish, Serpent, Camellia, DES, 3DES |
| **RSA** | Keygen (1024–4096), encrypt/decrypt, sign/verify (all with configurable padding) |
| **ECC** | ECDSA keygen/sign/verify (P-256, P-384, P-521), ECDH, Ed25519 sign/verify + keygen via generic `generateKeyPair(CURVE_ED25519)`, X25519 ECDH + keygen |
| **AES** (multi-mode) | AES-128/192/256 with ECB/CBC/CFB/OFB/CTR/GCM (mode parameter), AES-GCM with AAD |
| **Key Operations** | Random keygen, XOR exchange (validated equal lengths), XOR obfuscation, XOR split/reconstruct |

## Recently Fixed

| What | Fix |
|---|---|
| **AES mode support** | C runtime now accepts `uint32_t mode` param; uses `iv_block_op` instead of hardcoded CBC |
| **AES-GCM with AAD** | New native `encryptGcm`/`decryptGcm` pass AAD through Botan cipher API |
| **RSA padding params** | C runtime maps padding constants to Botan strings (PKCS1v15/OAEP/EMSA3/EMSA4) |
| **Ed25519 generic keygen** | `generateKeyPair(CURVE_ED25519)` delegates to native `ed25519GenerateKeyPair` |
| **X25519 keygen** | New `x25519GenerateKeyPair` via `botan_privkey_create_x25519` |
| **RSA stubs** | All RSA functions are native `!` declarations (no more `ret 0_u32` stubs) |
| **Key exchange** | Validates equal lengths; returns 0 on mismatch instead of silently truncating |

## Remaining Known Limitations

| Item | Issue |
|---|---|
| **MD5** | Delegates to truncated SHA-256 — not real MD5. Pure Zero shim. |

## Completely Missing

| Category | Specifically | Botan Availability |
|---|---|---|
| **Hash: BLAKE2b non-512** | Only BLAKE2b-512 wired | ✅ `BLAKE2b(256)`, etc. |
| **KDF: HKDF** | No HKDF extract/expand | ✅ `HKDF(SHA-256)` |
| **KDF: Argon2** | No password hashing | ✅ Argon2id |
| **KDF: scrypt** | No scrypt | ✅ |
| **AEAD: AES-CCM / AES-GCM-SIV** | Not wired | ✅ |
| **Key agreement: ML-KEM / Kyber** | No post-quantum | ✅ Botan 3.x |
| **Key agreement: FFDH** | No finite-field DH | ✅ |
| **Signature: ML-DSA / Dilithium** | No post-quantum | ✅ Botan 3.x |
| **Signature: ECDSA over X25519** | Ed25519 exists but no X25519 ECDSA | ✅ |
| **MAC: Poly1305 standalone** | Only via ChaCha20-Poly1305 AEAD | ✅ |
| **Emitter: Mach-O (macOS)** | No crypto support — only Void, ELF64, COFF wired | N/A |
