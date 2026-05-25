# zeroxlang

Experimental programming language for agent workflows.

Built for reliable autonomous software generation.

Optimized for:

- Token efficiency
- Low memory usage
- Fast startup
- Fast builds
- Low runtime latency
- Zero dependencies

> **Safety status**
>
> Security vulnerabilities should be expected. zeroxlang is not ready for production systems, sensitive data, or trusted infrastructure. Run and develop it in isolated, disposable environments.

## Agent Workflow Interfaces

A small program shows function definitions, return rows, prefix calls, fallibility, and indentation:

```zero
fn answer i32
  ret + 40 2

pub fn main Void world World !
  if == answer() 42
    check world.out.write "math works\n"
```

The compiler exposes the workflow through CLI commands with stable structured output.

### Load Version-Matched Rules

The compiler ships skill text that matches the binary being used:

```bash
zerox skills list
zerox skills get zero-language
zerox skills get zero-diagnostics
zerox skills get zero-stdlib
```

Print the language guide bundled with the compiler:

```bash
zerox skills get zero-language
```

### Inspect Compiler Facts

Compiler state is exposed through structured command output instead of prose-only output. The important contract is the stable fields and repair identifiers; today the CLI exposes those fields with `--json`:

```bash
zerox tokens --json examples/hello.0
zerox parse --json examples/hello.0
zerox check --json examples/hello.0
zerox graph --json examples/systems-package
zerox size --json examples/point.0
```

The JSON contracts include diagnostic codes and spans, public symbols, import edges, target readiness, compile-time sandbox facts, retained helpers, and size retention reasons.

### Dynamic Compilation Control (AI Control)

The compiler supports AI-influenced compilation through an optional `--ai-control`
flag. An external AI module sends JSON commands to influence compilation
parameters mid-process at defined hook points between phases.

```bash
# AI writes commands to a control file; the compiler reads them at each phase
zerox build --ai-control /tmp/zerox-ai-commands.json examples/hello.0
```

Commands are JSON objects, one per line in the control file:

```json
{"command": "set-profile", "args": ["debug"]}
{"command": "set-param", "args": ["profile", "tiny"]}
{"command": "skip-phase", "args": ["codegen"]}
{"command": "status", "args": []}
```

The compiler writes JSON feedback events to `<control_path>.out` at each hook
point, including phase timing, diagnostic counts, and applied command results:

```json
{"schemaVersion":1,"kind":"ai-control-feedback","phase":"check","elapsedMs":12,"accumulatedMs":45,"phaseCompleted":true,"message":"check passed"}
```

Supported commands include `set-profile`, `set-param`, `skip-phase`, `set-target`,
`set-emit-kind`, `status`, `reset`, and `abort`. A security whitelist controls
which commands are accepted (dangerous commands like `abort` and `set-target` are
denied by default).

When `--ai-control` is not specified, all hooks are no-ops and the compiler
behaves identically to before.

See `docs/ai-control.md` for the full API reference and protocol documentation.

### Compiler-Native Contracts

Most language ecosystems expose some of these facts through separate tools, editor protocols, or library APIs. zeroxlang keeps the agent-facing inspection and repair path in the compiler CLI.
The inspection and repair surfaces are compiler commands, not editor-only features or a separate analysis service:

| Command | Contract |
| --- | --- |
| `zerox skills get zero-language` | Version-matched language rules bundled with the compiler binary. |
| `zerox check --json` | Diagnostics with code, span, expected/actual fields, fix safety, repair metadata, compile-time sandbox facts, and target readiness. |
| `zerox parse --json` | A stable parse summary with declarations, function signatures, and body node kinds. |
| `zerox graph --json` | Modules, imports, public symbols, capabilities, effects, ownership facts, helper use, and interface fingerprints. |
| `zerox fix --plan --json` | Typed repair plans that describe proposed fixes without editing files. |
| `zerox size --json` | Retained helpers, size reasons, profile policy, backend facts, and artifact budget data. |

### Cryptography Runtime

The compiler delegates cryptographic operations to **Botan** (a FIPS-capable C++ crypto library) instead of using hand-written implementations. The crypto runtime provides 34 `std.crypto.*` functions covering SHA-256, AES, ChaCha20, RSA, ECDSA, Ed25519, and more.

Botan is a **C++ library**. The compiler passes `-lstdc++` alongside `-lbotan-3` at link time. When Botan is not installed, crypto functions compile but return 0 (graceful degradation).

```bash
# System install (Linux)
apt install libbotan-3-dev

# Local build
make -C native/zerox-c botan-build
make -C native/zerox-c
```

See [documentation/compiler/crypto.md](documentation/compiler/crypto.md) for the full API reference.

### Full Documentation

Comprehensive reference docs are in the [documentation/](documentation/) folder:

- [Language Reference](documentation/language/syntax.md) — syntax, types, functions, memory model, capabilities
- [Compiler Commands](documentation/compiler/commands.md) — all subcommands, flags, targets, profiles
- [Build System](documentation/compiler/build.md) — profiles, targets, cross-compilation, linking
- [Inspection & Repair](documentation/compiler/inspection.md) — diagnostics, fix plans, skills system, JSON contracts
- [Crypto Runtime](documentation/compiler/crypto.md) — Botan-backed cryptographic operations API
- [AI Control](docs/ai-control.md) — dynamic AI-influenced compilation

### Repair With Diagnostics

A failing fixture reports a diagnostic with stable fields:

```bash
zerox check --json conformance/check/fail/unknown-name.0
```

Today that output includes fields like:

```json
{
  "code": "NAM003",
  "message": "unknown identifier 'message'",
  "expected": "visible local, parameter, function, or builtin",
  "actual": "no matching visible symbol",
  "repair": {
    "id": "declare-missing-symbol"
  }
}
```

Diagnostics can be explained and turned into typed fix plans:

```bash
zerox explain --json TYP009
zerox fix --plan --json examples/agent-repair-demo/broken.0
```

Run the repair demo:

```bash
pnpm run agent:demo
```

See `examples/agent-repair-demo/` for the broken fixture, suggested edit, fixed fixture, and scripted check-explain-plan-rerun flow.

### Compatibility Policy

zeroxlang is intentionally unstable before 1.0. The repo prefers one current syntax and one formatted style over compatibility layers:

```bash
zerox fmt --check examples/hello.0
zerox check --json examples/hello.0
```

Before 1.0, the project may make breaking changes to simplify the language, standard library, diagnostics, or inspection APIs for agent use.

## Quick Start

Install the latest release:

```bash
curl -fsSL https://zeroxlang.ai/install.sh | bash
export PATH="$HOME/.zerox/bin:$PATH"
zerox --version
```

Check a program:

```bash
zerox check examples/hello.0
```

Run a small executable:

```bash
zerox run examples/add.0
```

Expected output:

```text
math works
```

## Common Commands

```bash
zerox check examples/hello.0
zerox run examples/add.0
zerox build --emit exe --target linux-musl-x64 examples/add.0 --out .zerox/out/add
zerox graph --json examples/systems-package
zerox size --json examples/point.0
zerox skills get zero --full
zerox doctor --json
```

## Validation

```bash
pnpm run docs:test
pnpm run conformance
pnpm run native:test
pnpm run command-contracts
```

Benchmarks run locally by default:

```bash
pnpm run bench
```
