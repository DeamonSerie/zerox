# Zero Compiler: Toolchain

The Zero compiler integrates with the host C compilation toolchain for building executables. This document explains how the toolchain works, how to configure it, and how cross-compilation is handled.

## Toolchain Architecture

The compiler delegates compilation and linking of runtime components to an external C compiler toolchain. The toolchain is responsible for:

1. Compiling C runtime source files into object files
2. Linking object files into executables

### Default Toolchain

The compiler uses the system `cc` command by default:

```
cc <flags> -I<include_dir> -c <source>.c -o <source>.o     # compile
cc <pre_link_flags> <objects> -o <exe> <post_object_flags>  # link
```

### Toolchain Selection

The compiler selects a toolchain in this priority order:

1. `--cc <path>` CLI flag
2. `ZERO_CC` environment variable
3. Default: `cc` (host default)

### Toolchain Drivers

| Driver | Identifier | Use Case |
|--------|------------|----------|
| `host-cc` | Host C compiler | Building for host target |
| `override-cc` | User-specified C compiler | Custom toolchain path |
| `zig-cc` | Zig C compiler | Cross-compilation |

## Cross-Compilation

When targeting a non-host platform, the compiler uses `zig cc` as the C compiler driver. Zig provides bundled cross-compilation toolchains for all supported targets.

### How It Works

```
# Host build
zero build --emit exe examples/hello.0
# Uses: cc -o hello hello.zero.o zero-runtime.o

# Cross-compilation  
zero build --emit exe --target linux-musl-x64 examples/hello.0
# Uses: zig cc -target x86_64-linux-musl -o hello hello.zero.o zero-runtime.o
```

### Zig Toolchain Setup

The Zig toolchain is discovered through the system PATH. The compiler looks for `zig cc` and configures it with:

- `ZIG_GLOBAL_CACHE_DIR=.zero/zig-global-cache`
- `ZIG_LOCAL_CACHE_DIR=.zero/zig-local-cache`
- `-target <target-triple>`

### Sysroot

Cross-compilation targets that require a sysroot (e.g., `linux-gnu`, `windows-msvc`) use environment variables to locate system headers and libraries:

| Target | Sysroot Env Var |
|--------|----------------|
| `darwin-x64` | `ZERO_MACOS_SYSROOT` |
| `linux-x64` | `ZERO_LINUX_X64_SYSROOT` |
| `linux-arm64` | `ZERO_LINUX_ARM64_SYSROOT` |
| `win32-x64.exe` | `ZERO_WINDOWS_X64_SYSROOT` |
| `win32-arm64.exe` | `ZERO_WINDOWS_ARM64_SYSROOT` |

Targets using `zig cc` (linux-musl) do not require a sysroot because Zig bundles the necessary headers and libc.

## Runtime Compilation

When building an executable, the compiler needs to compile and link C runtime code. This happens automatically based on what the Zero program uses.

### Runtime Components

| Component | Condition | Source | Library Dep |
|-----------|-----------|--------|-------------|
| `zero_runtime.c` | Always | Embedded in compiler | None |
| `zero_crypto.c` | Program uses crypto | Embedded in compiler | `libbotan-3`, `libstdc++` |
| `zero_http_curl.c` | Program uses HTTP | Embedded in compiler | `libcurl` |

### Compilation Process

For each runtime component needed:

1. Write the embedded C source to a temporary file
2. Write any required headers to a temporary include directory
3. Invoke the C compiler to compile to a `.o` file
4. Collect the object files for linking

### Crypto Toolchain Integration

Since Botan is a C++ library, the crypto runtime requires special handling:

- **Compile time**: The C compiler must be able to find `botan/ffi.h` (either via system paths or `ZERO_BOTAN_DIR`)
- **Link time**: The linker needs `-lbotan-3 -lstdc++` (either via pkg-config or directly)

The `bin/zero` wrapper script automatically detects a locally-built Botan and sets `ZERO_BOTAN_DIR`.

## Toolchain Information Commands

### `zero doctor`

Check toolchain availability:

```bash
zero doctor --json
```

Reports:
- C compiler availability and version
- Target-specific toolchain readiness
- Sysroot status
- Any missing dependencies

### `zero targets`

List available compilation targets:

```bash
zero targets
```

Returns JSON with target names, architectures, ABIs, and backend support.

## Configuration

### Environment Variables

| Variable | Purpose |
|----------|---------|
| `ZERO_CC` | Override the C compiler path |
| `ZERO_BOTAN_DIR` | Path to locally-built Botan library |
| `ZERO_*_SYSROOT` | Sysroot path for cross-compilation targets |
| `ZERO_NATIVE_TEST_ALLOW_LOCAL` | Allow local native compiler test execution |
| `ZERO_BENCH_MODE` | Benchmark mode configuration |
| `ZERO_HTTP_TEST_CA_BUNDLE` | CA bundle path for HTTP test requests |
