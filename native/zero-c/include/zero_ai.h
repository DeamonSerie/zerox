#ifndef ZERO_C_ZERO_AI_H
#define ZERO_C_ZERO_AI_H

#include <stdbool.h>
#include <stddef.h>

/*
 * zero_ai.h — Dynamic Compilation Control for ZeroLang
 *
 * This header defines the public API for AI-controlled compilation.
 * An external AI module can send JSON commands to influence compilation
 * parameters, skip phases, or reroute compilation mid-process.
 *
 * Protocol:
 *   The AI writes a JSON command object to the control FD (set via
 *   --ai-control <path> or ZERO_AI_CONTROL env var). The compiler reads
 *   commands at defined hook points between compilation phases. Each
 *   command receives a JSON status event on the status FD.
 *
 *   Backward compatibility: when --ai-control is not specified, all
 *   AI hooks are no-ops and the compiler behaves identically to before.
 */

/* Maximum number of AI command arguments */
#define Z_AI_MAX_ARGS 8

/* Maximum JSON command length */
#define Z_AI_MAX_COMMAND_LEN 4096

/* Maximum feedback entries retained */
#define Z_AI_MAX_FEEDBACK 64

/* ---------- AI Command Kinds ---------- */

typedef enum {
  Z_AI_CMD_UNKNOWN = 0,

  /* Profile control — change optimization profile mid-compilation */
  Z_AI_CMD_SET_PROFILE,

  /* Phase skip — skip one or more compilation phases */
  Z_AI_CMD_SKIP_PHASE,

  /* Phase reorder — reorder remaining phases */
  Z_AI_CMD_REORDER_PHASES,

  /* Target redirect — change output target */
  Z_AI_CMD_SET_TARGET,

  /* Emit kind change — switch between exe/obj/c */
  Z_AI_CMD_SET_EMIT_KIND,

  /* Module filter — include/exclude specific modules */
  Z_AI_CMD_FILTER_MODULE,

  /* Inject diagnostic — inject a custom diagnostic for testing/analysis */
  Z_AI_CMD_INJECT_DIAG,

  /* Abort compilation immediately */
  Z_AI_CMD_ABORT,

  /* Request status snapshot (current phase, timing, errors) */
  Z_AI_CMD_STATUS,

  /* Reset AI control state */
  Z_AI_CMD_RESET,

  /* Set a custom compilation parameter key=value */
  Z_AI_CMD_SET_PARAM,

  /* Count of valid command kinds (for validation) */
  Z_AI_CMD_COUNT
} ZAICmdKind;

/* ---------- Compilation Phase Identifiers ---------- */

typedef enum {
  Z_AI_PHASE_NONE = 0,
  Z_AI_PHASE_RESOLVE,
  Z_AI_PHASE_PARSE,
  Z_AI_PHASE_INTERFACE,
  Z_AI_PHASE_CHECK,
  Z_AI_PHASE_LOWER,
  Z_AI_PHASE_CODEGEN,
  Z_AI_PHASE_OBJECT,
  Z_AI_PHASE_LINK,
  Z_AI_PHASE_COUNT
} ZAIPhase;

/* ---------- AI Command ---------- */

typedef struct {
  ZAICmdKind kind;
  char args[Z_AI_MAX_ARGS][256];
  size_t arg_count;
  long long issued_ms;   /* timestamp when issued by AI */
  long long received_ms; /* timestamp when read by compiler */
} ZAICommand;

/* ---------- AI Feedback Event ---------- */

typedef struct {
  ZAIPhase phase;
  const char *phase_name;
  long long elapsed_ms;
  long long accumulated_ms;
  int diag_count;
  bool phase_completed;
  bool has_error;
  char message[256];
} ZAIFeedback;

/* ---------- AI Control State ---------- */

typedef struct {
  /* Enable flag — false means all hooks are no-ops */
  bool enabled;

  /* Control channel: path to a FIFO or regular file for AI commands.
   * When NULL, no AI control is active. */
  char *control_path;

  /* Status channel: path to a FIFO or regular file for compiler feedback.
   * Defaults to <control_path>.out when not set. */
  char *status_path;

  /* Allowed commands whitelist (bitmask of 1 << ZAICmdKind).
   * 0 means all commands are denied. */
  unsigned long long allowed_commands;

  /* Maximum commands to process per hook call (safety limit) */
  size_t max_commands_per_hook;

  /* Pending commands not yet processed */
  ZAICommand *pending_commands;
  size_t pending_len;
  size_t pending_cap;

  /* Feedback ring buffer */
  ZAIFeedback feedback[Z_AI_MAX_FEEDBACK];
  size_t feedback_count;

  /* Modified compilation parameters */
  char *override_profile;
  char *override_target;
  char *override_emit_kind;
  bool abort_requested;
  bool skip_remaining;
  bool reset_requested;

  /* Internal: command sequence counter */
  size_t command_seq;
} ZAIControlState;

/* ---------- Public API ---------- */

/*
 * Initialize AI control state from a control path.
 * Reads --ai-control <path> argument.
 * Returns true if initialization succeeded.
 */
bool z_ai_init(ZAIControlState *state, const char *control_path);

/*
 * Destroy AI control state and free resources.
 */
void z_ai_destroy(ZAIControlState *state);

/*
 * Process pending AI commands at a hook point.
 * Called between each compilation phase.
 * Returns true if compilation should continue, false if abort requested.
 */
bool z_ai_process_commands(ZAIControlState *state, ZAIPhase current_phase, const char *phase_name);

/*
 * Send a feedback event to the AI status channel.
 */
bool z_ai_send_feedback(ZAIControlState *state, const ZAIFeedback *feedback);

/*
 * Check if a specific command kind is allowed by the whitelist.
 */
bool z_ai_command_allowed(const ZAIControlState *state, ZAICmdKind kind);

/*
 * Parse a JSON command string into a ZAICommand.
 * Returns true on success.
 */
bool z_ai_parse_command(const char *json, size_t json_len, ZAICommand *cmd);

/*
 * Serialize a feedback event to JSON string.
 * Returns a newly allocated string (caller must free).
 */
char *z_ai_feedback_to_json(const ZAIFeedback *feedback);

/*
 * Apply a command's effect to the control state.
 * Returns true if the command was applied successfully.
 */
bool z_ai_apply_command(ZAIControlState *state, const ZAICommand *cmd);

/*
 * Default allowed commands whitelist (safe subset).
 * Returns a bitmask of allowed command kinds.
 */
unsigned long long z_ai_default_allowed_commands(void);

/*
 * Convert a phase enum to its string name.
 */
const char *z_ai_phase_name(ZAIPhase phase);

/*
 * Convert a string to a phase enum (case-insensitive).
 * Returns Z_AI_PHASE_NONE on unknown.
 */
ZAIPhase z_ai_phase_from_name(const char *name);

#endif /* ZERO_C_ZERO_AI_H */
