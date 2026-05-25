# Zero Capabilities System

Zero uses a capability-based I/O model where the `World` parameter controls what operations a program can perform. This makes side effects explicit and verifiable.

## The World Parameter

Every `main` function receives a `World` value that gates access to system resources:

```zero
pub fn main Void world World !
  check world.out.write "hello\n"
```

The `World` parameter is how the program declares which capabilities it needs. The compiler validates that the target platform supports all required capabilities.

## Capability Categories

The compiler tracks the following capabilities:

| Capability | Description | Examples |
|------------|-------------|----------|
| `args` | Command-line argument access | `std.args.get` |
| `env` | Environment variable access | — |
| `fs` | Filesystem access | `std.fs.open`, `std.fs.create`, `std.fs.read`, `std.fs.write` |
| `memory` | Memory allocation | `std.mem.fixedAllocator`, `std.mem.bumpAllocator` |
| `net` | Network access | `std.net.host`, `std.http.fetch` |
| `path` | Path resolution | — |
| `proc` | Process management | — |
| `rand` | Random number generation | `std.crypto.key.randomBytes` |
| `stdio` | Standard I/O | `world.out.write`, `world.err.write` |
| `time` | Time operations | `std.time.ms` |
| `web` | Web platform APIs | — |
| `world` | Full system access | — |

## Target Capability Contracts

Each target platform declares which capabilities it supports:

| Target | Capabilities |
|--------|-------------|
| `darwin-arm64` | memory, stdio, args, env, fs, time, rand, net, proc |
| `darwin-x64` | memory, stdio, fs, time, rand |
| `linux-musl-x64` | memory, stdio, args, env, fs, time, rand |
| `linux-musl-arm64` | memory, stdio, time, rand |
| `linux-x64` | memory, stdio, time, rand |
| `linux-arm64` | memory, stdio, time, rand |
| `win32-x64.exe` | memory, stdio, time, rand |
| `win32-arm64.exe` | memory, stdio, time, rand |

## Capability Checking

The compiler performs capability checking at build time:

1. **Collection** — traverse the program to collect all required capabilities
2. **Validation** — compare required capabilities against target capabilities
3. **Error reporting** — if a capability is missing, emit a diagnostic with the required capability and the target

### Missing Capability Error

```bash
zero check --json --target win32-x64.exe examples/file-copy.0
```

If `file-copy.0` uses `std.fs.open` but the target doesn't support `fs`, the compiler reports:

```json
{
  "code": "CAP001",
  "message": "Target 'win32-x64.exe' does not provide required capability 'fs'",
  "expected": "target capability 'fs'",
  "actual": "not provided by target"
}
```

## Using Capabilities

### Standard I/O

The `World` type provides direct I/O access:

```zero
world.out.write "stdout\n"     # write to stdout
world.err.write "stderr\n"     # write to stderr
```

### Filesystem

Filesystem access requires the `fs` capability:

```zero
let fs std.fs.host()
let file std.fs.open fs "path/to/file"
```

### HTTP

Network access requires the `net` capability:

```zero
let net std.net.host()
let client std.http.client net
let response std.http.fetch client request buffer timeout
```

### Random Numbers

```zero
let bytes std.crypto.key.randomBytes 32_world
```

## Self-Host Subset

The compiler itself is written in C, but the project tracks which capabilities would be needed for a hypothetical self-hosted Zero compiler. The self-host subset restricts capabilities to:

- `args` — command-line parsing
- `env` — environment configuration
- `fs` — file I/O
- `memory` — allocation
- `path` — path operations
- `proc` — process spawning
- `stdio` — output
- `time` — timing
- `rand` — random identifiers
