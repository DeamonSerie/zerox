# How to Write Documentation for Your Creations

This guide explains how AI agents should approach writing documentation for software projects. Use these principles when creating or updating documentation for any codebase.

## Why Documentation Matters for Agents

Documentation is the primary way agents and humans understand a codebase. Well-structured, thorough documentation enables:

- **Faster onboarding** — agents can understand the project in fewer turns
- **Correct changes** — knowing the full context reduces hallucinations and mistakes
- **Autonomous work** — agents can self-serve answers without asking for human guidance
- **Consistent decisions** — documented conventions prevent arbitrary choices

## Core Principles

### 1. Be Complete, Not Verbose

Cover every feature, but don't pad with fluff. Every paragraph should answer a question someone would actually ask.

**Good**: "Zero supports AES-256-GCM encryption via `std.crypto.aes.encrypt`. The function takes a 32-byte key, 12-byte IV, and returns the ciphertext length or 0 on failure."

**Bad**: "Encryption is a very important feature of the Zero language. It allows users to protect their data using advanced cryptographic algorithms. One such algorithm is AES."

### 2. Structure for Scanning

Use clear hierarchy: headings, subheadings, lists, and tables. Readers (both agents and humans) scan before they read.

- Use `##` and `###` for logical sections
- Use tables for structured data (function signatures, options, types)
- Use lists for sequences and sets
- Use code blocks for examples

### 3. Show Working Examples

Every feature should have a concrete, runnable example. Examples should be:

- **Self-contained** — a reader can understand them in isolation
- **Correct** — they should actually compile and run
- **Illustrative** — they demonstrate the specific feature, not multiple features at once

```zero
# Example of generic pairs
fn makePair<T: Type, U: Type> Pair<T,U> left T right U
  ret Pair . left left right right

pub fn main Void world World !
  let pair makePair 40 2_u32
  if == pair.left 40
    check world.out.write "pair works\n"
```

### 4. Document Contracts, Not Internals

Focus on what the user needs to know: behavior, inputs, outputs, error conditions, and constraints. Implementation details belong in code comments or developer docs, not public-facing documentation.

**Document**:
- Function signatures and parameters
- Return values and error codes
- Type constraints and requirements
- Capabilities needed
- Performance characteristics (if notable)

**Don't document**:
- Internal data structures
- Algorithm choices (unless they affect behavior)
- Implementation history
- Future plans

### 5. Link to the Source of Truth

Don't repeat information that lives in the code. Link to examples, source files, and generated docs.

```
See [examples/generic-pair.0](../examples/generic-pair.0) for a runnable example.
```

### 6. Keep a Stable Structure

Once you establish a documentation structure, maintain it. Agents build expectations based on where things were found before.

**Good** structure:
```
documentation/
  README.md                   # Overview, table of contents
  language/                   # Language features
    syntax.md
    types.md
    functions.md
  compiler/                   # Compiler features
    commands.md
    build.md
  stdlib/                     # Standard library
    crypto.md
    io.md
```

## Documentation Workflow for Agents

### Step 1: Survey the Codebase

Before writing, understand the full scope:

1. **Read the README** — get the project's stated purpose and features
2. **Explore examples** — see what features exist in practice
3. **Read source code** — check CLI commands, type definitions, public APIs
4. **Check existing docs** — understand what's already documented
5. **Run the tool** — test commands and examples to verify behavior

### Step 2: Categorize Features

Organize features into natural groups:

- **Core language** — syntax, types, control flow, functions
- **Compiler** — commands, flags, build profiles, targets
- **Standard library** — modules, APIs, types
- **Tooling** — editor support, CI, testing

### Step 3: Document Each Feature

For each feature, answer:

- What is it? (one-sentence summary)
- How do you use it? (syntax, API, command)
- What are the inputs and outputs?
- What are the constraints or requirements?
- What errors can occur?
- Show an example.

### Step 4: Verify Accuracy

- Check that code examples are syntactically correct
- Verify command flags and options match the actual implementation
- Confirm file paths and references are valid

### Step 5: Cross-Reference

Link related features to each other:

- "See [Types](types.md) for type parameter syntax"
- "Use `zero build --profile tiny` (see [Build Profiles](build.md))"

## Markdown Style Guide

### Code Blocks

Always specify the language:

```zero
pub fn main Void world World !
  check world.out.write "hello\n"
```

```bash
zero build --emit exe examples/hello.0
```

```json
{"command": "set-profile", "args": ["debug"]}
```

### File References

Use backtick-wrapped relative paths:

```
See `examples/hello.0` for a minimal program.
```

### Emphasis

Use **bold** for key terms and `code` for code identifiers:

```
The **main** function takes a `World` parameter.
```

### Tables

Use tables for structured data (types, commands, options):

| Command | Description | JSON Support |
|---------|-------------|-------------|
| `check` | Type-check source | Yes |
| `build` | Emit executable or object | Yes |
| `run` | Build and run | No |

### Callouts

Use blockquotes for notes and warnings:

> **Note**: The crypto runtime requires Botan to be installed. Without it, all crypto functions return 0.

> **Warning**: Security vulnerabilities should be expected. Zero is not ready for production systems.

## What Not to Do

### Don't Document the Unimplemented

If a feature doesn't exist yet, don't document it. Don't say "coming soon" or "planned". Either implement it or leave it out.

### Don't Guess

If you're not sure about a behavior, check the source code, run a test, or ask. Guesses create documentation that misleads both agents and humans.

### Don't Editorialize

Avoid subjective opinions:

- ~~"This is the best way to do X"~~
- ~~"The elegant design of Y"~~
- ~~"Simply use Z for maximum productivity"~~

State facts, describe behavior, and let the reader decide.

### Don't Duplicate Without Reason

If information exists in one place (like function signatures in a header file), reference it rather than copying it. Duplication creates drift.

## Template: Documenting a Module

```markdown
# Module Name

One-paragraph summary of what this module does.

## Functions

### `functionName`

```
fn functionName param1_type param1_name param2_type param2_name
```

Description of what the function does.

**Parameters**:
- `param1_name` (`param1_type`) — what this parameter is for
- `param2_name` (`param2_type`) — what this parameter is for

**Returns**: The return type and what it represents.

**Errors**: What errors can occur and when.

**Example**:

```zero
let result functionName arg1 arg2
```

## Types

### `TypeName`

Description of the type.

**Fields**:
- `field1` (`Type`) — description

**Example**:

```zero
let value TypeName . field1 arg1
```
```

## Template: Documenting a CLI Command

```markdown
## `zero command`

One-sentence summary.

```
zero command [--json] [--flag] <input>
```

**Flags**:
- `--json` — emit structured JSON output
- `--flag` — description

**Arguments**:
- `<input>` — source file or package path

**Example**:

```bash
zero command --json path/to/file.0
```

**Output**: Description of stdout/stderr and exit codes.
```

## Checklist for Documentation Quality

Before submitting documentation, verify:

- [ ] Every feature mentioned has a working example
- [ ] Code blocks have language annotations
- [ ] File paths are correct and relative
- [ ] Command flags match the actual implementation
- [ ] No placeholder text ("TODO", "FIXME", "coming soon")
- [ ] Tables render correctly
- [ ] Links between documents work
- [ ] No subjective editorializing
- [ ] Information is not duplicated across files
- [ ] The document passes a spell-check
