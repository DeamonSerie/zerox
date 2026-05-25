# Zero Compiler: Build System

The Zero compiler has a direct native code generation backend that emits executables and object files without intermediate languages.

## Build Profiles

Profiles control optimization level, debug info, safety checks, and code size.

| Profile | Optimization | Debug Info | Safety | Use Case |
|---------|-------------|------------|--------|----------|
| `debug` | None | Full | Max checks | Development, debugging |
| `dev` | Moderate | Source-level | Checks on | Day-to-day development |
| `release-fast` | Speed (`-O2`) | Minimal | — | Performance-sensitive builds |
| `release-small` | Size (`-Os`) | Minimal | — | Space-constrained targets |
| `tiny` | Size (`-Oz`) | None | — | Minimal binary size |
| `audit` | None | Full | Max checks + runtime verification | Security-critical builds |

### Profile Semantics

Each profile defines:

- **Optimization goal**: speed, size, or none
- **Bounds policy**: checked or unchecked
- **Panic policy**: trap, abort, or unwind
- **Debug info**: none, minimal, or full
- **Symbol policy**: strip or retain
- **Runtime metadata**: full, minimal, or none
- **Budget**: estimated binary size budget for `tiny` profile

## Emit Kinds

| Kind | Output | Description |
|------|--------|-------------|
| `exe` | Executable | Native executable for the target platform |
| `obj` | Object file | Relocatable object file (`.o`) for external linking |
| `c` | C source | C source file (legacy backend) |

## Cross-Compilation

Zero cross-compiles using a Zig-based toolchain.

```bash
zero build --emit exe --target linux-musl-x64 examples/hello.0 --out hello
```

When cross-compiling:
- The compiler uses `zig cc` as the C compiler driver
- System headers and libraries are provided via sysroot
- Target capabilities are validated before emission

### Targets

| Target | Backend | Cross-Compiler |
|--------|---------|----------------|
| Host | Direct native | `cc` (system) |
| Darwin ARM64 | Direct native | `cc` |
| Darwin x64 | Direct native | `cc` |
| Linux musl x64 | Direct native | `zig cc` |
| Linux musl ARM64 | Direct native | `zig cc` |
| Linux gnu x64 | Direct native | `zig cc` |
| Linux gnu ARM64 | Direct native | `zig cc` |
| Windows MSVC x64 | Direct native | `lld-link` |
| Windows MSVC ARM64 | Direct native | `lld-link` |

## Toolchain

The compiler uses the host C toolchain for compilation and linking of runtime components.

### How Linking Works

When building an executable, the compiler:

1. Emits a `.zero.o` object file (the compiled Zero program)
2. Compiles `zero_runtime.c` → `.zero-runtime.o` (runtime support)
3. If crypto is used: compiles `zero_crypto.c` → `.zero-crypto.o` (crypto runtime)
4. If HTTP is used: compiles `zero_http_curl.c` → `.zero-http-curl.o` (HTTP provider)
5. Links all objects into the final executable

### Link Flags

- **HTTP**: `-lcurl`
- **Crypto**: `-lbotan-3 -lstdc++` (Botan is a C++ library)
- Crypto library detection: `ZERO_BOTAN_DIR` → `pkg-config` → system paths

### C Compiler Discovery

The compiler discovers the C compiler in this order:
1. `--cc <path>` CLI flag
2. `ZERO_CC` environment variable
3. System `cc` command
4. `zig cc` (for cross-compilation)

## Ship (Release Pipeline)

The `zero ship` command produces a deterministic release artifact:

```
zero ship --target linux-musl-x64 examples/hello.0 --out .zero/ship/hello
```

Produces:
- **Binary**: the executable
- **Stripped binary**: copy with debug info removed
- **Checksum**: FNV-1a hash of the binary
- **Archive manifest**: list of included files
- **Debug metadata**: debug info file
- **Size report**: section-by-section breakdown
- **SBOM placeholder**: software bill of materials

## Caching

The compiler maintains several caches for incremental compilation:

- **Emitted object cache** — keyed by source content hash + target + profile
- **Module interface cache** — keyed by public interface fingerprints
- **Source file cache** — keyed by file content hash

Cache keys include:
- Source interface hash
- Dependency hashes
- Target facts hash
- Profile selection
- Capability constraints

## Build Output

The compiler writes output to:
- Default: `<input-name-without-ext>`
- With `--out <path>`: the specified path
- Runtime objects: temporary files in the same directory as the output
- These are cleaned up after successful linking
