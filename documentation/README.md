# Zero Language Documentation

This folder contains comprehensive documentation for the Zero programming language, its compiler, and standard library.

## Table of Contents

### Language Reference

- [Syntax Overview](language/syntax.md) — Whitespace-sensitive syntax, literals, expressions, statements
- [Type System](language/types.md) — Primitives, shapes, enums, choices, generics, type aliases
- [Functions](language/functions.md) — Definitions, signatures, return types, fallibility
- [Memory Model](language/memory.md) — Ownership, borrowing, references, defer, cleanup
- [Modules and Packages](language/modules.md) — File-based modules, packages, imports, visibility
- [Compile-Time Features](language/compile-time.md) — Constants, static parameters, generic specialization
- [Capabilities](language/capabilities.md) — The World system, capability-based I/O

### Compiler Reference

- [CLI Commands](compiler/commands.md) — All `zero` subcommands and flags
- [Build System](compiler/build.md) — Profiles, targets, emitting executables and objects
- [Inspection and Repair](compiler/inspection.md) — JSON contracts, diagnostics, fix plans, skills
- [AI Control](compiler/ai-control.md) — Optional AI-influenced compilation via `--ai-control`
- [Toolchain](compiler/toolchain.md) — Cross-compilation, sysroots, C compiler integration
- [Crypto Runtime](compiler/crypto.md) — Botan-backed cryptographic runtime

### Standard Library Reference

- [Crypto](stdlib/crypto.md) — AES, ChaCha20, RSA, ECDSA, Ed25519, hashing, KDF
- [I/O and Filesystem](stdlib/io.md) — File operations, path handling, read/write
- [HTTP](stdlib/http.md) — HTTP client, fetch, header inspection
- [Memory](stdlib/memory.md) — Spans, mutability, allocations, collections
- [Platform](stdlib/platform.md) — System info, time, environment, args
- [Data Formats](stdlib/data-formats.md) — JSON parsing, varint encoding

### Examples

See [examples/](../../examples/) for runnable Zero programs demonstrating each feature.

### Quick Reference

| Topic | File |
|-------|------|
| Hello World | `examples/hello.0` |
| HTTP Request | `examples/std-http-request.0` |
| AES Encryption | `examples/std-crypto-aes.0` |
| Generic Pair | `examples/generic-pair.0` |
| File I/O | `examples/file-copy.0` |
| Error Handling | `examples/fallibility.0` |
| Ownership | `examples/ownership-cleanup.0` |
| Memory Primitives | `examples/memory-primitives.0` |
