# Zero Compiler: Inspection and Repair

The Zero compiler is designed for agent-friendly inspection, with structured JSON output, diagnostic codes, and typed fix plans for every error.

## Diagnostic System

Every compiler diagnostic has a stable code with machine-readable fields:

```json
{
  "code": "NAM003",
  "message": "unknown identifier 'message'",
  "expected": "visible local, parameter, function, or builtin",
  "actual": "no matching visible symbol",
  "repair": {
    "id": "declare-missing-symbol",
    "safety": "safe",
    "summary": "Add a declaration for the missing symbol"
  }
}
```

### Diagnostic Code Categories

| Prefix | Category | Example |
|--------|----------|---------|
| `NAM` | Name resolution | `NAM003` — unknown identifier |
| `TYP` | Type system | `TYP009` — type mismatch |
| `BLD` | Build system | `BLD004` — target not buildable |
| `CGEN` | Code generation | `CGEN004` — codegen invariant failed |
| `MEM` | Memory model | — |
| `CAP` | Capabilities | — |

### Explain a Diagnostic

```bash
zero explain --json TYP009
```

Returns detailed information including: code, message, expected, actual, root cause, help text, example of the error, and example of the fix.

## Fix Plans

The compiler can generate typed fix plans that describe proposed edits without modifying files:

```bash
zero fix --plan --json examples/broken.0
```

Output includes:

```json
{
  "diagnostics": [
    {
      "code": "NAM003",
      "repair": {
        "id": "declare-missing-symbol",
        "safety": "safe",
        "summary": "Add a declaration for the missing symbol",
        "appliesEdits": true
      }
    }
  ],
  "patches": [
    {
      "path": "examples/broken.0",
      "line": 3,
      "old": "  check world.out.write message\n",
      "new": "  let message \"hello\"\n  check world.out.write message\n"
    }
  ]
}
```

Fix plans include:
- **`safety`**: `safe`, `unsafe`, or `unknown`
- **`appliesEdits`**: whether the compiler can apply this edit
- **`patches`**: concrete file edits with exact old/new text

### Apply Fixes

```bash
zero fix --plan --json --apply examples/broken.0
```

## Skills System

Skills are version-matched documentation bundled with each compiler binary. They provide agents with accurate, version-specific guidance.

```bash
zero skills list                                # list available skills
zero skills get zero-language                   # get language rules
zero skills get zero-diagnostics                # get diagnostic reference
zero skills get zero-stdlib                     # get stdlib documentation
zero skills get zero --full                     # get full skill content
```

Available skills:
- `zero` — discovery stub, installation, entry points
- `zero-agent` — agent workflow setup
- `zero-language` — language syntax and rules
- `zero-diagnostics` — diagnostic code reference
- `zero-packages` — package system documentation
- `zero-builds` — build system documentation
- `zero-testing` — testing documentation
- `zero-stdlib` — standard library reference

## Structured JSON Output

The following commands support `--json` for structured output contracts:

| Command | Output Includes |
|---------|----------------|
| `check --json` | Diagnostics, target readiness, compile-time facts, compiler phases |
| `parse --json` | Stable parse summary with declarations and body node kinds |
| `graph --json` | Modules, imports, symbols, capabilities, effects, helper use |
| `size --json` | Retained helpers, size reasons, profile policy, budget data |
| `mem --json` | Memory regions, allocator facts, collection facts |
| `doc --json` | Public declarations with docs, capabilities, signatures |
| `fix --plan --json` | Diagnostics with repair plans and editable patches |
| `explain --json` | Detailed diagnostic explanation with examples |
| `ship --json` | Artifact metadata, checksums, size breakdowns |
| `dev --json` | IR structure, phase timing, internal state |
| `time --json` | Phase-level timing data |
| `abi --json` | ABI layout, type sizes, alignment |

## Compile-Time Sandbox

The compiler evaluates compile-time expressions in a sandboxed environment with:

- **No file system access**
- **No network access**
- **No arbitrary code execution**
- **Deterministic evaluation**
- **Time-bounded execution**

Compile-time sandbox facts are exposed through `--json` output, including:
- Whether compile-time code was executed
- What operations were performed
- Any sandbox violations

## Target Readiness

Before emitting code, the compiler checks target readiness, reporting:

- Whether the target supports all required capabilities
- Whether the selected emit kind is available  
- Whether the toolchain (C compiler, linker) is available
- Any backend blockers with structured facts

```bash
zero check --json --emit exe --target linux-arm64 examples/hello.0
```

## Caching

The compiler uses multiple caches for incremental builds:

- **Compile cache** — cache keys based on source content and configuration
- **Interface cache** — module interface fingerprints for dependency tracking
- **Object cache** — cached emitted object files

Cache hit/miss statistics are exposed through `--json` output in `compilerCaches` and `compilerPhases`.
