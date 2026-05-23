# AI Control — Dynamic Compilation Control

Zero supports AI-influenced compilation through an optional `--ai-control` flag.
This allows an external AI module to send JSON commands that influence
compilation parameters mid-process.

## Overview

When `--ai-control <path>` is passed, the compiler reads JSON commands from the
specified file at defined hook points between compilation phases. Feedback
events are written to `<path>.out`.

When the flag is absent, all hooks are no-ops and the compiler behaves
identically to before.

## Usage

```sh
zero build --ai-control /tmp/zero-ai-commands.json examples/hello.0
```

The compiler reads commands from the control file at each phase hook.
After reading, the control file is cleared so the AI can write new commands
for the next hook.

## Compilation Phases

The compiler has these hook points:

| Phase      | Description                        |
|------------|------------------------------------|
| resolve    | Before source resolution           |
| check      | After type-checking                |
| lower      | Before IR lowering                 |
| codegen    | Before machine code emission       |
| link       | Before linking (runtime exe only)  |

## Command Format

Commands are JSON objects, one per line:

```json
{"command": "set-profile", "args": ["debug"], "issued_ms": 1700000000000}
{"command": "skip-phase", "args": ["codegen"], "issued_ms": 1700000001000}
{"command": "status", "args": [], "issued_ms": 1700000002000}
{"command": "set-param", "args": ["profile", "tiny"], "issued_ms": 1700000003000}
{"command": "abort", "args": [], "issued_ms": 1700000004000}
{"command": "reset", "args": [], "issued_ms": 1700000005000}
```

## Supported Commands

| command       | args                  | Description                                      | Default Permission |
|---------------|-----------------------|--------------------------------------------------|--------------------|
| status        | []                    | Request phase status snapshot                    | Allowed            |
| set-profile   | [profile_name]        | Change optimization profile mid-compilation      | Allowed            |
| skip-phase    | [phase_name]          | Skip remaining compilation phases                | Allowed            |
| set-param     | [key, value]          | Set a compilation parameter (profile, target, emit) | Allowed         |
| reset         | []                    | Reset all AI overrides to initial state          | Allowed            |
| set-target    | [target_name]         | Change target triple                             | Denied by default  |
| set-emit-kind | [exe\|obj]            | Switch emit kind mid-compilation                 | Denied by default  |
| abort         | []                    | Abort compilation immediately                    | Denied by default  |
| filter-module | [module_name]         | Filter modules (reserved for future use)         | Denied by default  |
| inject-diag   | [code, message]       | Inject a diagnostic (reserved for future use)    | Denied by default  |
| reorder-phases| [phase, ...]          | Reorder remaining phases (reserved)              | Denied by default  |

## Security

- Commands not in the allowed whitelist are silently ignored.
- By default, only `status`, `set-profile`, `skip-phase`, `set-param`, and
  `reset` are permitted. Dangerous operations require explicit opt-in.
- The control file is read and cleared atomically at each hook point.
- Maximum 16 commands processed per hook (configurable).

## Feedback Format

The compiler writes one JSON feedback event per line to `<control_path>.out`:

```json
{"schemaVersion":1,"kind":"ai-control-feedback","phase":"check","elapsedMs":12,"accumulatedMs":45,"diagCount":0,"phaseCompleted":true,"hasError":false,"message":"check passed"}
{"schemaVersion":1,"kind":"ai-control-feedback","phase":"lower","elapsedMs":0,"accumulatedMs":45,"diagCount":0,"phaseCompleted":true,"hasError":false,"message":"applied command: set-profile"}
```

## Backward Compatibility

When `--ai-control` is not specified, the compiler runs exactly as before.
All AI control code paths are conditional on `command.ai_control` being true.
Existing workflows, scripts, and CI pipelines are unaffected.

## API Reference (zero_ai.h)

The public API is defined in `include/zero_ai.h`. Key functions:

- `z_ai_init()` — Initialize AI control state from a control path
- `z_ai_process_commands()` — Process pending commands at a hook point
- `z_ai_send_feedback()` — Send a feedback event to the AI status channel
- `z_ai_parse_command()` — Parse a JSON command string
- `z_ai_apply_command()` — Apply a command's effect to the control state
- `z_ai_command_allowed()` — Check if a command kind is allowed
- `z_ai_feedback_to_json()` — Serialize feedback to JSON

## Env Variables

- `ZERO_AI_CONTROL` — Alternative way to specify the control path (not yet implemented, reserved for future use)
