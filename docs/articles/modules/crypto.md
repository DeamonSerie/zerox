## Status

Runnable today:

| API | Return | Notes |
| --- | --- | --- |
| `std.crypto.hash32(bytes)` | `u32` | Computes the current 32-bit hash helper over bytes. |
| `std.crypto.hmac32(key, bytes)` | `u32` | Computes the current keyed 32-bit helper over bytes. |
| `std.crypto.constantTimeEql(a, b)` | `Bool` | Compares byte spans without data-dependent early exit. |
| `std.crypto.secureRandomU32()` | `u32` | Reads target entropy where the target provides it. |

Metadata labels:

- effects: codec, memory, or rand
- allocation behavior: no allocation
- target support: hash helpers are target-neutral; secure random requires a rand-capable target
- error behavior: infallible helpers
- ownership notes: borrows caller-provided byte spans
- example: `examples/std-platform.0`

## Reference API (`zeroStandardLibrary/std/crypto/`)

The following API is defined in the Zero standard library at
`zeroStandardLibrary/std/crypto/`. These modules provide the
programming interface for encryption and decryption operations.

### Core API (`encrypt.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `encrypt(data, key, iv, method, mode, padding, out)` | `u32` | Encrypt data with the specified cipher. Returns bytes written. |
| `decrypt(data, key, iv, method, mode, padding, out)` | `u32` | Decrypt data with the specified cipher. Returns bytes written. |
| `encryptWithOptions(data, key, iv, method, mode, padding, options, out)` | `u32` | Encrypt with option flags (obfuscation, double, hybrid). |

### Cipher Selection (`cipher.0`)

Constants for selecting algorithms, modes, and padding:

**Algorithm constants:**
- `AES` / `AES_128` / `AES_192` / `AES_256` - AES with specified key size
- `DES` - Data Encryption Standard  
- `TDES` - Triple DES (TDEA)
- `CHACHA20` - ChaCha20 stream cipher
- `XCHACHA20` - Extended-nonce ChaCha20
- `SALSA20` - Salsa20 stream cipher
- `BLOWFISH` - Blowfish block cipher
- `TWOFISH` - Twofish block cipher
- `SERPENT` - Serpent block cipher
- `CAMELLIA` - Camellia block cipher

**Mode constants:** `MODE_ECB`, `MODE_CBC`, `MODE_CFB`, `MODE_OFB`, `MODE_CTR`, `MODE_GCM`, `MODE_CCM`

**Padding constants:** `PADDING_PKCS7`, `PADDING_ZERO`, `PADDING_ANSIX923`, `PADDING_ISO10126`, `PADDING_NONE`

**Option flags:** `OPTION_OBFUSCATE`, `OPTION_DOUBLE`, `OPTION_HYBRID`, `OPTION_INTEGRITY`

Helper functions: `keySize(algorithm)`, `blockSize(algorithm)`, `methodName(algorithm)`

### AES (`aes.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `encrypt(key, iv, data, mode, out)` | `u32` | AES encryption |
| `decrypt(key, iv, data, mode, out)` | `u32` | AES decryption |
| `encryptGcm(key, iv, data, aad, out)` | `u32` | AES-GCM AEAD encryption with auth tag |
| `decryptGcm(key, iv, data, aad, out)` | `u32` | AES-GCM AEAD decryption with tag verification |
| `generateKey(keyBits, out)` | `u32` | Generate random AES key |

### ChaCha20 / Salsa20 (`chacha.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `chacha20(key, nonce, data, counter, out)` | `u32` | ChaCha20 encrypt/decrypt |
| `xchacha20(key, nonce, data, counter, out)` | `u32` | XChaCha20 (24-byte nonce) |
| `salsa20(key, nonce, data, counter, out)` | `u32` | Salsa20 encrypt/decrypt |
| `chacha20Poly1305Encrypt(key, nonce, data, aad, out)` | `u32` | ChaCha20-Poly1305 AEAD |
| `chacha20Poly1305Decrypt(key, nonce, data, aad, out)` | `u32` | ChaCha20-Poly1305 verify+decrypt |

### DES / 3DES (`des.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `encrypt(key, data, mode, out)` | `u32` | Single DES encryption |
| `decrypt(key, data, mode, out)` | `u32` | Single DES decryption |
| `tripleEncrypt(key, data, mode, out)` | `u32` | Triple DES (3DES/TDEA) encryption |
| `tripleDecrypt(key, data, mode, out)` | `u32` | Triple DES decryption |

### Blowfish (`blowfish.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `encrypt(key, iv, data, mode, out)` | `u32` | Blowfish encryption |
| `decrypt(key, iv, data, mode, out)` | `u32` | Blowfish decryption |

### Obscure Ciphers

**Twofish** (`twofish.0`): `encrypt(key, iv, data, mode, out)`, `decrypt(key, iv, data, mode, out)`

**Serpent** (`serpent.0`): `encrypt(key, iv, data, mode, out)`, `decrypt(key, iv, data, mode, out)`

**Camellia** (`camellia.0`): `encrypt(key, iv, data, mode, out)`, `decrypt(key, iv, data, mode, out)`

### Hash Functions (`hash.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `sha256(data, out)` | `u32` | SHA-256 digest |
| `sha512(data, out)` | `u32` | SHA-512 digest |
| `sha3_256(data, out)` | `u32` | SHA-3-256 digest |
| `md5(data, out)` | `u32` | MD5 digest (legacy only) |
| `hmacSha256(key, data, out)` | `u32` | HMAC-SHA256 |
| `hmacSha512(key, data, out)` | `u32` | HMAC-SHA512 |
| `hash(data, out, algorithm)` | `u32` | Dispatch by algorithm constant |
| `verify(digest, data, algorithm)` | `Bool` | Verify data matches digest |

Hash constants: `HASH_SHA256`, `HASH_SHA3_256`, `HASH_MD5`, `HASH_SHA512`

### Key Management (`key.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `randomBytes(count, out)` | `u32` | Secure random bytes |
| `generate(algorithm, out)` | `u32` | Generate key for algorithm |
| `derive(password, salt, iterations, keyLen, out)` | `u32` | PBKDF2-HMAC-SHA256 key derivation |
| `exchange(type, privateKey, peerKey, out)` | `u32` | Key exchange (ECDH, DH) |
| `obfuscate(key, context, out)` | `u32` | Key obfuscation transform |
| `deobfuscate(key, context, out)` | `u32` | Reverse obfuscation |
| `split(secret, total, threshold, out)` | `u32` | Shamir's secret sharing split |
| `reconstruct(shares, count, shareSize, out)` | `u32` | Shamir's secret sharing reconstruct |

### Layered / Hybrid Encryption (`layered.0`)

| Function | Returns | Description |
| --- | --- | --- |
| `doubleEncrypt(data, outerKey, outerMethod, innerKey, innerMethod, outerMode, innerMode, out)` | `u32` | Two-cipher layered encryption |
| `doubleDecrypt(data, outerKey, outerMethod, innerKey, innerMethod, outerMode, innerMode, out)` | `u32` | Two-cipher layered decryption |
| `hybridEncrypt(data, publicKey, asymMethod, symMethod, symMode, out)` | `u32` | Asymmetric-wrap hybrid encryption |
| `hybridDecrypt(data, privateKey, asymMethod, symMethod, symMode, out)` | `u32` | Asymmetric-wrap hybrid decryption |
| `obfuscateLayer(data, key, out)` | `u32` | Non-standard obfuscation transform |
| `deobfuscateLayer(data, key, out)` | `u32` | Remove obfuscation |

### Asymmetric Cryptography

**RSA** (`rsa.0`):
- `generateKeyPair(keyBits, pubOut, privOut)` - Generate RSA key pair
- `encrypt(publicKey, data, padding, out)` - RSA encryption
- `decrypt(privateKey, data, padding, out)` - RSA decryption
- `sign(privateKey, data, hashAlgo, padding, out)` - RSA digital signature
- `verify(publicKey, data, signature, hashAlgo, padding)` - RSA signature verification

**ECC** (`ecc.0`):
- `generateKeyPair(curve, pubOut, privOut)` - Generate EC key pair
- `sign(privateKey, data, curve, out)` - ECDSA signing
- `verify(publicKey, data, signature, curve)` - ECDSA verification
- `ecdh(privateKey, peerPublic, curve, out)` - ECDH shared secret
- `ed25519Sign(privateKey, data, out)` - Ed25519 signing
- `ed25519Verify(publicKey, data, signature)` - Ed25519 verification

Curve constants: `CURVE_P256`, `CURVE_P384`, `CURVE_P521`, `CURVE_X25519`, `CURVE_ED25519`

## Example

```zero
use std.crypto
use std.crypto.cipher

pub fn main Void world World !
  # AES-256-GCM encryption
  let mut key [32]u8 [0_u8; 32]
  let mut iv [12]u8 [0_u8; 12]

  let mut ciphertext [1024]u8 [0_u8; 1024]
  let ct_out MutSpan<u8> ciphertext
  let enc_len std.crypto.aes.encrypt key iv "Hello" cipher.MODE_GCM ct_out world

  let mut plaintext [1024]u8 [0_u8; 1024]
  let pt_out MutSpan<u8> plaintext
  let dec_len std.crypto.aes.decrypt key iv ciphertext[0..enc_len] cipher.MODE_GCM pt_out world

  if std.mem.eql "Hello" plaintext[0..dec_len]
    check world.out.write "AES-256-GCM round-trip: OK\n"

  # SHA-256 hashing
  let mut digest [32]u8 [0_u8; 32]
  let dg_out MutSpan<u8> digest
  let dg_len std.crypto.hash.sha256 "example data" dg_out world

  # ChaCha20 stream cipher
  let mut nonce [12]u8 [0_u8; 12]
  let enc2_len std.crypto.chacha.chacha20 key nonce "stream test" 0_u32 ct_out world
```

## Design Notes

`std.crypto` is extensible via the `zeroStandardLibrary/std/crypto/` module
system. New cipher implementations can be added as `.0` files in that
directory following the same API conventions:

1. Functions accept `Span<u8>` for inputs and `MutSpan<u8>` for outputs
2. Length returns are `u32` (0 indicates failure)
3. Algorithm and mode selection uses `u32` constants
4. The `world World !` suffix enables runtime I/O for random generation
5. Caller-provided output buffers prevent allocation overhead

Algorithm priorities:
- **Standard methods**: AES, ChaCha20, SHA-256, RSA, ECC (recommended for
  all new applications)
- **Legacy support**: DES, 3DES, MD5 (interop only; not secure for new uses)
- **Obscure/enhanced**: Twofish, Serpent, Camellia, XChaCha20, Salsa20
  (alternative security profiles)
- **Layered**: Double encryption, hybrid schemes, key obfuscation
  (defense-in-depth for high-security environments)
