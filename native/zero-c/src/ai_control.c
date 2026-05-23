#include "zero.h"
#include "zero_ai.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ---------- Helpers ---------- */

static long long ai_now_ms(void) {
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  unsigned long long t = ft.dwLowDateTime / 10000ULL + ft.dwHighDateTime / 10000ULL * 1000ULL;
  return (long long)(t / 10000);
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
#endif
}

static bool ai_write_file(const char *path, const char *data, size_t len) {
  if (!path || !path[0]) return false;
  FILE *f = fopen(path, "a");
  if (!f) return false;
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len;
}

static bool ai_read_file(const char *path, char *buf, size_t buf_size, size_t *out_len) {
  if (!path || !path[0]) return false;
  FILE *f = fopen(path, "r");
  if (!f) return false;
  *out_len = fread(buf, 1, buf_size - 1, f);
  buf[*out_len] = '\0';
  fclose(f);
  return true;
}

/* Simple JSON string extraction: finds "key": "value" or "key": number */
static const char *ai_json_string_value(const char *json, const char *key, char *out, size_t out_size) {
  if (!json || !key) return NULL;
  out[0] = '\0';
  size_t key_len = strlen(key);
  const char *p = json;
  while ((p = strstr(p, key)) != NULL) {
    const char *after_key = p + key_len;
    /* Skip whitespace and colon */
    while (*after_key == ' ' || *after_key == '\t') after_key++;
    if (*after_key != ':') { p = after_key; continue; }
    after_key++;
    while (*after_key == ' ' || *after_key == '\t') after_key++;
    if (*after_key == '"') {
      after_key++;
      size_t i = 0;
      while (*after_key && *after_key != '"' && i < out_size - 1) {
        if (*after_key == '\\' && *(after_key + 1)) {
          after_key++;
          switch (*after_key) {
            case 'n': out[i++] = '\n'; break;
            case 't': out[i++] = '\t'; break;
            case 'r': out[i++] = '\r'; break;
            case '"': out[i++] = '"'; break;
            case '\\': out[i++] = '\\'; break;
            default: out[i++] = *after_key; break;
          }
        } else {
          out[i++] = *after_key;
        }
        after_key++;
      }
      out[i] = '\0';
      return after_key;
    }
    p = after_key;
  }
  return NULL;
}

static long long ai_json_number_value(const char *json, const char *key) {
  char buf[64];
  if (!ai_json_string_value(json, key, buf, sizeof(buf))) return 0;
  return atoll(buf);
}

/* ---------- Phase name conversions ---------- */

static const char *phase_names[Z_AI_PHASE_COUNT] = {
  "none",
  "resolve",
  "parse",
  "interface",
  "check",
  "lower",
  "codegen",
  "object",
  "link"
};

const char *z_ai_phase_name(ZAIPhase phase) {
  if (phase > 0 && phase < Z_AI_PHASE_COUNT) return phase_names[phase];
  return "unknown";
}

ZAIPhase z_ai_phase_from_name(const char *name) {
  if (!name) return Z_AI_PHASE_NONE;
  for (int i = 1; i < Z_AI_PHASE_COUNT; i++) {
    if (strcasecmp(name, phase_names[i]) == 0) return (ZAIPhase)i;
  }
  return Z_AI_PHASE_NONE;
}

/* ---------- Command kind name helpers ---------- */

static const char *command_kind_name(ZAICmdKind kind) {
  switch (kind) {
    case Z_AI_CMD_SET_PROFILE: return "set-profile";
    case Z_AI_CMD_SKIP_PHASE: return "skip-phase";
    case Z_AI_CMD_REORDER_PHASES: return "reorder-phases";
    case Z_AI_CMD_SET_TARGET: return "set-target";
    case Z_AI_CMD_SET_EMIT_KIND: return "set-emit-kind";
    case Z_AI_CMD_FILTER_MODULE: return "filter-module";
    case Z_AI_CMD_INJECT_DIAG: return "inject-diag";
    case Z_AI_CMD_ABORT: return "abort";
    case Z_AI_CMD_STATUS: return "status";
    case Z_AI_CMD_RESET: return "reset";
    case Z_AI_CMD_SET_PARAM: return "set-param";
    default: return "unknown";
  }
}

static ZAICmdKind command_kind_from_name(const char *name) {
  if (!name) return Z_AI_CMD_UNKNOWN;
  if (strcmp(name, "set-profile") == 0) return Z_AI_CMD_SET_PROFILE;
  if (strcmp(name, "skip-phase") == 0) return Z_AI_CMD_SKIP_PHASE;
  if (strcmp(name, "reorder-phases") == 0) return Z_AI_CMD_REORDER_PHASES;
  if (strcmp(name, "set-target") == 0) return Z_AI_CMD_SET_TARGET;
  if (strcmp(name, "set-emit-kind") == 0) return Z_AI_CMD_SET_EMIT_KIND;
  if (strcmp(name, "filter-module") == 0) return Z_AI_CMD_FILTER_MODULE;
  if (strcmp(name, "inject-diag") == 0) return Z_AI_CMD_INJECT_DIAG;
  if (strcmp(name, "abort") == 0) return Z_AI_CMD_ABORT;
  if (strcmp(name, "status") == 0) return Z_AI_CMD_STATUS;
  if (strcmp(name, "reset") == 0) return Z_AI_CMD_RESET;
  if (strcmp(name, "set-param") == 0) return Z_AI_CMD_SET_PARAM;
  return Z_AI_CMD_UNKNOWN;
}

/* ---------- Public API ---------- */

unsigned long long z_ai_default_allowed_commands(void) {
  /* Safe subset: status, set-profile, skip-phase, set-param, reset are allowed by default.
   * Dangerous commands (set-target, set-emit-kind, filter-module, inject-diag, abort,
   * reorder-phases) require explicit opt-in. */
  unsigned long long mask = 0;
  mask |= (1ULL << Z_AI_CMD_STATUS);
  mask |= (1ULL << Z_AI_CMD_SET_PROFILE);
  mask |= (1ULL << Z_AI_CMD_SKIP_PHASE);
  mask |= (1ULL << Z_AI_CMD_SET_PARAM);
  mask |= (1ULL << Z_AI_CMD_RESET);
  return mask;
}

bool z_ai_command_allowed(const ZAIControlState *state, ZAICmdKind kind) {
  if (!state || !state->enabled) return false;
  if (kind <= Z_AI_CMD_UNKNOWN || kind >= Z_AI_CMD_COUNT) return false;
  return (state->allowed_commands & (1ULL << kind)) != 0;
}

bool z_ai_init(ZAIControlState *state, const char *control_path) {
  if (!state) return false;
  memset(state, 0, sizeof(*state));

  if (!control_path || !control_path[0]) {
    /* Not enabled — this is the default, backward-compatible path */
    state->enabled = false;
    return true;
  }

  state->control_path = z_strdup(control_path);

  /* Derive status path: <control_path>.out */
  size_t path_len = strlen(control_path);
  state->status_path = z_checked_malloc(path_len + 5);
  memcpy(state->status_path, control_path, path_len);
  memcpy(state->status_path + path_len, ".out", 5);

  state->enabled = true;
  state->allowed_commands = z_ai_default_allowed_commands();
  state->max_commands_per_hook = 16;
  state->command_seq = 0;

  /* Write an init event to the status channel */
  ZAIFeedback init_fb = {
    .phase = Z_AI_PHASE_NONE,
    .phase_name = "init",
    .elapsed_ms = 0,
    .accumulated_ms = 0,
    .diag_count = 0,
    .phase_completed = false,
    .has_error = false,
  };
  snprintf(init_fb.message, sizeof(init_fb.message),
           "AI control initialized; control_path=%s status_path=%s",
           control_path, state->status_path);
  z_ai_send_feedback(state, &init_fb);

  return true;
}

void z_ai_destroy(ZAIControlState *state) {
  if (!state) return;
  free(state->control_path);
  free(state->status_path);
  free(state->override_profile);
  free(state->override_target);
  free(state->override_emit_kind);
  free(state->pending_commands);
  memset(state, 0, sizeof(*state));
}

/*
 * Parse a JSON command object of the form:
 * {"command": "set-profile", "args": ["debug"], "issued_ms": 1234}
 *
 * Returns true on success.
 */
bool z_ai_parse_command(const char *json, size_t json_len, ZAICommand *cmd) {
  if (!json || !cmd || json_len == 0) return false;
  memset(cmd, 0, sizeof(*cmd));

  char kind_buf[64] = {0};
  ai_json_string_value(json, "command", kind_buf, sizeof(kind_buf));
  cmd->kind = command_kind_from_name(kind_buf);
  if (cmd->kind == Z_AI_CMD_UNKNOWN) return false;

  cmd->issued_ms = ai_json_number_value(json, "issued_ms");
  cmd->received_ms = ai_now_ms();

  /* Parse args array (simple bracket-delimited string parsing) */
  const char *args_start = strstr(json, "\"args\"");
  if (args_start) {
    args_start = strchr(args_start, '[');
    if (args_start) {
      args_start++;
      size_t arg_idx = 0;
      while (*args_start && arg_idx < Z_AI_MAX_ARGS) {
        while (*args_start == ' ' || *args_start == '\t' || *args_start == '\n' || *args_start == '\r') args_start++;
        if (*args_start == ']') break;
        if (*args_start == ',') { args_start++; continue; }
        if (*args_start == '"') {
          args_start++;
          size_t ai = 0;
          while (*args_start && *args_start != '"' && ai < sizeof(cmd->args[arg_idx]) - 1) {
            if (*args_start == '\\' && *(args_start + 1)) {
              args_start++;
              switch (*args_start) {
                case 'n': cmd->args[arg_idx][ai++] = '\n'; break;
                case 't': cmd->args[arg_idx][ai++] = '\t'; break;
                case 'r': cmd->args[arg_idx][ai++] = '\r'; break;
                case '"': cmd->args[arg_idx][ai++] = '"'; break;
                case '\\': cmd->args[arg_idx][ai++] = '\\'; break;
                default: cmd->args[arg_idx][ai++] = *args_start; break;
              }
            } else {
              cmd->args[arg_idx][ai++] = *args_start;
            }
            args_start++;
          }
          cmd->args[arg_idx][ai] = '\0';
          arg_idx++;
        } else {
          args_start++;
        }
      }
      cmd->arg_count = arg_idx;
    }
  }

  return true;
}

static void ai_append_json_string(char *buf, size_t buf_size, const char *value) {
  size_t i = strlen(buf);
  buf[i++] = '"';
  for (const char *p = value ? value : ""; *p && i < buf_size - 2; p++) {
    switch (*p) {
      case '"': buf[i++] = '\\'; buf[i++] = '"'; break;
      case '\\': buf[i++] = '\\'; buf[i++] = '\\'; break;
      case '\n': buf[i++] = '\\'; buf[i++] = 'n'; break;
      case '\r': buf[i++] = '\\'; buf[i++] = 'r'; break;
      case '\t': buf[i++] = '\\'; buf[i++] = 't'; break;
      default: buf[i++] = *p; break;
    }
  }
  buf[i++] = '"';
  buf[i] = '\0';
}

char *z_ai_feedback_to_json(const ZAIFeedback *fb) {
  if (!fb) return NULL;
  char buf[1024];
  int n = snprintf(buf, sizeof(buf),
    "{\"schemaVersion\":1,\"kind\":\"ai-control-feedback\","
    "\"phase\":");
  ai_append_json_string(buf + strlen(buf), sizeof(buf) - strlen(buf),
    fb->phase_name ? fb->phase_name : z_ai_phase_name(fb->phase));
  n = (int)strlen(buf);
  snprintf(buf + n, sizeof(buf) - n,
    ",\"elapsedMs\":%lld,\"accumulatedMs\":%lld,"
    "\"diagCount\":%d,\"phaseCompleted\":%s,\"hasError\":%s,"
    "\"message\":",
    fb->elapsed_ms, fb->accumulated_ms,
    fb->diag_count,
    fb->phase_completed ? "true" : "false",
    fb->has_error ? "true" : "false");
  ai_append_json_string(buf + strlen(buf), sizeof(buf) - strlen(buf), fb->message);
  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "}\n");
  return z_strdup(buf);
}

bool z_ai_send_feedback(ZAIControlState *state, const ZAIFeedback *feedback) {
  if (!state || !state->enabled || !feedback) return false;

  /* Store in ring buffer */
  if (state->feedback_count < Z_AI_MAX_FEEDBACK) {
    state->feedback[state->feedback_count] = *feedback;
    state->feedback_count++;
  }

  /* Write to status channel */
  char *json = z_ai_feedback_to_json(feedback);
  if (!json) return false;
  bool ok = ai_write_file(state->status_path, json, strlen(json));
  free(json);
  return ok;
}

bool z_ai_apply_command(ZAIControlState *state, const ZAICommand *cmd) {
  if (!state || !cmd) return false;
  if (!z_ai_command_allowed(state, cmd->kind)) return false;

  switch (cmd->kind) {
    case Z_AI_CMD_SET_PROFILE:
      if (cmd->arg_count >= 1 && cmd->args[0][0]) {
        free(state->override_profile);
        state->override_profile = z_strdup(cmd->args[0]);
        return true;
      }
      return false;

    case Z_AI_CMD_SET_TARGET:
      if (cmd->arg_count >= 1 && cmd->args[0][0]) {
        free(state->override_target);
        state->override_target = z_strdup(cmd->args[0]);
        return true;
      }
      return false;

    case Z_AI_CMD_SET_EMIT_KIND:
      if (cmd->arg_count >= 1 && cmd->args[0][0]) {
        free(state->override_emit_kind);
        state->override_emit_kind = z_strdup(cmd->args[0]);
        return true;
      }
      return false;

    case Z_AI_CMD_SKIP_PHASE:
      /* skip-phase: args[0] is phase name to skip from remaining */
      state->skip_remaining = true;
      return true;

    case Z_AI_CMD_ABORT:
      state->abort_requested = true;
      return true;

    case Z_AI_CMD_RESET:
      free(state->override_profile);
      free(state->override_target);
      free(state->override_emit_kind);
      state->override_profile = NULL;
      state->override_target = NULL;
      state->override_emit_kind = NULL;
      state->abort_requested = false;
      state->skip_remaining = false;
      state->allowed_commands = z_ai_default_allowed_commands();
      return true;

    case Z_AI_CMD_SET_PARAM:
      /* set-param: args[0]=key, args[1]=value */
      if (cmd->arg_count >= 2 && cmd->args[0][0] && cmd->args[1][0]) {
        if (strcmp(cmd->args[0], "profile") == 0) {
          free(state->override_profile);
          state->override_profile = z_strdup(cmd->args[1]);
          return true;
        }
        if (strcmp(cmd->args[0], "target") == 0) {
          free(state->override_target);
          state->override_target = z_strdup(cmd->args[1]);
          return true;
        }
        if (strcmp(cmd->args[0], "emit") == 0) {
          free(state->override_emit_kind);
          state->override_emit_kind = z_strdup(cmd->args[1]);
          return true;
        }
        /* Unknown params are silently accepted for extensibility */
        return true;
      }
      return false;

    case Z_AI_CMD_STATUS:
      /* Status is handled in the processing loop, not here */
      return true;

    default:
      return false;
  }
}

bool z_ai_process_commands(ZAIControlState *state, ZAIPhase current_phase, const char *phase_name) {
  if (!state || !state->enabled) return true;
  if (state->abort_requested) return false;

  const char *pn = phase_name ? phase_name : z_ai_phase_name(current_phase);

  /* Read commands from control file */
  char cmd_buf[Z_AI_MAX_COMMAND_LEN];
  size_t cmd_len = 0;

  /* Check if control file exists and has new content */
  if (state->control_path && state->control_path[0]) {
    if (ai_read_file(state->control_path, cmd_buf, sizeof(cmd_buf), &cmd_len) && cmd_len > 0) {
      /* Clear the control file after reading */
      FILE *f = fopen(state->control_path, "w");
      if (f) fclose(f);

      /* Parse each line as a separate command */
      char *line = cmd_buf;
      size_t processed = 0;
      while (*line && processed < state->max_commands_per_hook) {
        /* Skip whitespace */
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        if (!*line) break;

        /* Find end of line */
        char *end = line;
        while (*end && *end != '\n') end++;
        size_t line_len = (size_t)(end - line);

        if (line_len > 0) {
          ZAICommand cmd;
          if (z_ai_parse_command(line, line_len, &cmd)) {
            /* Validate command is allowed */
            if (z_ai_command_allowed(state, cmd.kind)) {
              /* Apply the command */
              if (z_ai_apply_command(state, &cmd)) {
                ZAIFeedback fb = {
                  .phase = current_phase,
                  .phase_name = pn,
                  .elapsed_ms = 0,
                  .accumulated_ms = 0,
                  .diag_count = 0,
                  .phase_completed = false,
                  .has_error = false,
                };
                snprintf(fb.message, sizeof(fb.message),
                         "applied command: %s", command_kind_name(cmd.kind));
                z_ai_send_feedback(state, &fb);
              }
              state->command_seq++;
            } else {
              ZAIFeedback fb = {
                .phase = current_phase,
                .phase_name = pn,
                .diag_count = 1,
                .has_error = true,
              };
              snprintf(fb.message, sizeof(fb.message),
                       "command denied: %s (not in whitelist)",
                       command_kind_name(cmd.kind));
              z_ai_send_feedback(state, &fb);
            }
          }
        }

        line = end;
        processed++;
      }
    }
  }

  if (state->abort_requested) {
    ZAIFeedback fb = {
      .phase = current_phase,
      .phase_name = pn,
      .has_error = true,
    };
    snprintf(fb.message, sizeof(fb.message), "abort requested by AI");
    z_ai_send_feedback(state, &fb);
    return false;
  }

  return true;
}
