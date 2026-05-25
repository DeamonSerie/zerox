# Zero Crypto Runtime

The Zero compiler provides cryptographic operations through a Botan-backed runtime. All crypto functions are available via the `std.crypto` standard library module.

## Overview

The crypto runtime uses **Botan** (a FIPS-capable C++ crypto library) through its C FFI interface. When Botan is not installed, all crypto functions compile but return 0 at runtime (graceful degradation).

### Installation

**Option A: System install (Linux)**

```bash
apt install libbotan-3-dev
```

**Option B: Local build (any platform with python3 + make)**

```bash
make -C native/zero-c botan-build
make -C native/zero-c
```

The `bin/zero` wrapper script detects the locally-built Botan automatically.

## Supported Operations

### Hash Functions

| Function | Description | Output Size |
|----------|-------------|-------------|
| `std.crypto.hash.sha256` | SHA-256 hash | 32 bytes |
| `std.crypto.hash.sha512` | SHA-512 hash | 64 bytes |

### MAC / KDF

| Function | Description |
|----------|-------------|
| `std.crypto.hash.hmac` | HMAC-SHA256 message authentication |
| `std.crypto.hash.pbkdf2` | PBKDF2-HMAC-SHA256 key derivation |

### Symmetric Ciphers

| Module | Algorithm | Key Sizes | Modes |
|--------|-----------|-----------|-------|
| `std.crypto.aes` | AES | 128/192/256 bits | CBC, GCM |
| `std.crypto.des` | DES | 56 bits | ECB, CBC, CFB, OFB |
| `std.crypto.des` | Triple-DES (3DES) | 112/168 bits | ECB, CBC, CFB, OFB |
| `std.crypto.blowfish` | Blowfish | 32-448 bits | ECB, CBC, CFB, OFB |
| `std.crypto.twofish` | Twofish | 128/192/256 bits | ECB, CBC, CFB, OFB |
| `std.crypto.serpent` | Serpent | 128/192/256 bits | ECB, CBC, CFB, OFB |
| `std.crypto.camellia` | Camellia | 128/192/256 bits | ECB, CBC, CFB, OFB |

### Stream Ciphers

| Module | Algorithm | Nonce Size |
|--------|-----------|------------|
| `std.crypto.chacha` | ChaCha20 / XChaCha20 | 12 / 24 bytes |
| `std.crypto.chacha` | Salsa20 | 8 bytes |

### AEAD

| Function | Algorithm |
|----------|-----------|
| `std.crypto.chacha.chacha20Poly1305Encrypt` | ChaCha20-Poly1305 encryption + authentication |
| `std.crypto.chacha.chacha20Poly1305Decrypt` | ChaCha20-Poly1305 decryption + verification |

### Asymmetric Cryptography (RSA)

| Function | Description |
|----------|-------------|
| `std.crypto.rsa.generateKeypair` | Generate RSA key pair |
| `std.crypto.rsa.encrypt` | RSA encryption (OAEP-SHA256) |
| `std.crypto.rsa.decrypt` | RSA decryption (OAEP-SHA256) |
| `std.crypto.rsa.sign` | RSA signature (PSS-SHA256) |
| `std.crypto.rsa.verify` | RSA signature verification |

### Asymmetric Cryptography (ECC)

| Function | Description |
|----------|-------------|
| `std.crypto.ecc.generateKeypair` | Generate ECDSA key pair (secp256r1/384r1/521r1) |
| `std.crypto.ecc.sign` | ECDSA sign |
| `std.crypto.ecc.verify` | ECDSA verify |
| `std.crypto.ecc.ecdh` | ECDH key agreement (KDF2-SHA256) |
| `std.crypto.ecc.ed25519Sign` | Ed25519 sign |
| `std.crypto.ecc.ed25519Verify` | Ed25519 verify |

### CSPRNG

| Function | Description |
|----------|-------------|
| `std.crypto.key.randomBytes` | Cryptographically secure random bytes |

### Mode Constants

| Constant | Value |
|----------|-------|
| `std.crypto.cipher.MODE_ECB` | 0 |
| `std.crypto.cipher.MODE_CBC` | 1 |
| `std.crypto.cipher.MODE_CFB` | 2 |
| `std.crypto.cipher.MODE_OFB` | 3 |
| `std.crypto.cipher.MODE_CTR` | 4 |
| `std.crypto.cipher.MODE_GCM` | 5 |
| `std.crypto.cipher.MODE_CCM` | 6 |

## Architecture

The crypto runtime is implemented in two parts:

1. **`runtime/zero_crypto.c`** — C source implementing 34 `zero_crypto_*` functions via Botan's C FFI
2. **`runtime/zero_crypto.h`** — Public API header with function declarations
3. **Compiler integration** — `src/main.c` compiles and links the crypto runtime when needed

### Compilation

When a Zero program uses crypto functions (`zero_crypto_*`), the compiler:

1. Detects the usage via runtime import audit
2. Compiles `zero_crypto.c` into a `.o` file using the system C compiler
3. Links with `-lbotan-3 -lstdc++` (or via pkg-config)

### Environment Variables

- `ZERO_BOTAN_DIR` — Path to a locally-built Botan directory. The compiler adds `-I$ZERO_BOTAN_DIR/build/include/public` for compilation and `-L$ZERO_BOTAN_DIR` for linking.
- The `bin/zero` wrapper automatically sets `ZERO_BOTAN_DIR` if it detects a built Botan in the project tree.

### Graceful Degradation

If Botan is not installed:

- The crypto object compiles without errors
- All `zero_crypto_*` functions return 0 at runtime
- Programs that don't use crypto work normally
- Programs that use crypto will get 0-length results (callers should check return values)

## Return Value Convention

All crypto functions return a `u32`:

- **Non-zero**: the number of bytes written to the output buffer (success)
- **Zero**: failure (wrong key size, missing Botan, invalid mode, etc.)
