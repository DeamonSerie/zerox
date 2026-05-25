# Zero Compiler CLI Commands

The Zero compiler is a single binary that provides all language tooling through subcommands.

## Usage

```
zero <command> [options] [input]
```

## Commands

### `zero --version`

Print the compiler version.

```
zero --version
zero --version --json
```

**JSON fields**: `schemaVersion`, `version`, `commit`, `host`, `backend`, `targets`, `targetCompiler`, `crossCompilation`

### `zero check`

Parse and typecheck Zero source without emitting artifacts.

```
zero check [--json] [--target <target>] [--emit exe|obj] <input>
```

**Flags**:
- `--json` — emit structured JSON output including target readiness
- `--target <target>` — check against a specific target (default: `host`)
- `--emit exe|obj` — check readiness for executable or object emission

### `zero build`

Build a direct native executable or object artifact.

```
zero build [--json] [--emit exe|obj] [--target <target>]
          [--profile debug|dev|release-fast|release-small|tiny|audit]
          [--release <profile>] [--out <file>]
          [--ai-control <path>] [--cc <path>]
          <input>
```

**Flags**:
- `--json` — emit structured build output
- `--emit exe|obj` — emit executable or object file (default: `exe`)
- `--target <target>` — cross-compile target (default: `host`)
- `--profile <name>` — build profile (default: `release`)
- `--release <name>` — alias for `--profile`
- `--out <file>` — output path
- `--ai-control <path>` — path to AI control commands file
- `--cc <path>` — C compiler override

**Profiles**:
- `debug` — debug info, no optimization
- `dev` — development with some optimization
- `release-fast` — optimized for speed
- `release-small` — optimized for size
- `tiny` — minimal size, no debug info
- `audit` — max safety checks

### `zero run`

Build a host executable and run it.

```
zero run [--target <target>] [--profile <name>]
         [--release <profile>] [--out <file>]
         [--ai-control <path>] [--cc <path>]
         <input> [-- args...]
```

Program stdout and stderr pass through unchanged.

### `zero ship`

Produce a deterministic release preview with binary, checksum, and metadata.

```
zero ship [--json] [--target <target>]
          [--profile release-small|tiny|audit]
          [--out <file>]
          <input>
```

Produces: binary, stripped copy, checksum, archive manifest, debug metadata, size report, SBOM placeholder.

### `zero test`

Build and run inline `test` blocks.

```
zero test [--json] [--filter <name>] [--target <target>]
          [--cc <path>] [--out <file>]
          <input>
```

### `zero fmt`

Format Zero source deterministically.

```
zero fmt [--check] <input>
```

- `--check` — verify formatting without modifying files (exit 1 if unformatted)

### `zero tokens`

Emit source token JSON for oracle comparisons.

```
zero tokens --json <input>
```

### `zero parse`

Emit normalized parse JSON.

```
zero parse --json <input>
```

### `zero graph`

Inspect modules, symbols, capabilities, and imports.

```
zero graph [--json] [--target <target>] <input>
```

**JSON output includes**: modules, imports, public symbols, capabilities, effects, ownership, helper use, interface fingerprints.

### `zero size`

Analyze output size and retention reasons.

```
zero size [--json] [--out <artifact>] <input>
```

### `zero mem`

Analyze memory model and allocation.

```
zero mem [--json] [--target <target>] <input>
```

### `zero doc`

Emit documentation JSON for public declarations.

```
zero doc [--json] <input>
```

### `zero dev`

Development introspection.

```
zero dev [--json] [--trace] <input>
```

### `zero time`

Measure compiler phase timing.

```
zero time --json <input>
```

### `zero abi`

Check or dump ABI-safe declarations.

```
zero abi check|dump [--json] [--target <target>] <input>
```

### `zero explain`

Explain a diagnostic code with detailed information.

```
zero explain [--json] <code>
```

Example: `zero explain --json TYP009`

### `zero fix`

Generate typed fix plans for diagnostics.

```
zero fix --plan --json <input>
```

### `zero skills`

List and retrieve version-matched skill content for agents.

```
zero skills [list|get] [--json]
zero skills get <name> [--full]
zero skills get --all
```

Built-in skills include: `zero`, `zero-agent`, `zero-language`, `zero-diagnostics`, `zero-packages`, `zero-builds`, `zero-testing`, `zero-stdlib`.

### `zero new`

Create a project template.

```
zero new cli|lib|package <name>
```

### `zero doctor`

Check host, compiler, target toolchain, and docs readiness.

```
zero doctor [--json]
```

### `zero clean`

Remove generated output.

```
zero clean [--all]
```

- Without `--all`: removes build output while preserving compiler caches
- With `--all`: removes broader `.zero` state while preserving `.zero/bin`

### `zero targets`

Print supported target facts as JSON.

```
zero targets
```

## Supported Targets

| Target | Arch | OS | ABI | Object Format | Linker |
|--------|------|----|-----|---------------|--------|
| `darwin-arm64` | aarch64 | macOS | darwin | Mach-O | `cc` |
| `darwin-x64` | x86_64 | macOS | darwin | Mach-O | `cc` |
| `linux-musl-x64` | x86_64 | Linux | musl | ELF | `zig cc` |
| `linux-musl-arm64` | aarch64 | Linux | musl | ELF | `zig cc` |
| `linux-x64` | x86_64 | Linux | gnu | ELF | `zig cc` |
| `linux-arm64` | aarch64 | Linux | gnu | ELF | `zig cc` |
| `win32-x64.exe` | x86_64 | Windows | msvc | COFF | `lld-link` |
| `win32-arm64.exe` | aarch64 | Windows | msvc | COFF | `lld-link` |

## Global Flags

- `--json` — structured JSON output where supported
- `--help`, `-h` — print help
- `--cc <path>` — C compiler override
- `--dep <path>` — dependency path
- `--trace` — detailed tracing output
