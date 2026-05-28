#include "zerox.h"

#include <string.h>

typedef struct {
  ZDirectBackend backend;
  const char *object_emitter;
  const char *exe_emitter;
  const char *linker_flavor;
  const char *object_artifact_path;
  const char *exe_artifact_path;
  bool runtime_object_supported;
} ZDirectBackendDescriptor;

typedef struct {
  const char *object_format;
  const char *os;
  const char *arch;
  const char *abi;
  ZDirectBackend backend;
  bool object_supported;
  bool exe_supported;
} ZDirectBackendRule;

static const ZDirectBackendDescriptor direct_backend_descriptors[] = {
  {Z_DIRECT_BACKEND_ELF64, "zerox-elf64", "zerox-elf64-exe", "elf64", "direct-elf64-object", "direct-elf64-exe", true},
  {Z_DIRECT_BACKEND_ELF_AARCH64, "zerox-elf-aarch64", "zerox-elf-aarch64-exe", "elf64", "direct-elf-aarch64-object", "direct-elf-aarch64-exe", false},
  {Z_DIRECT_BACKEND_VOID64, "zerox-Void64", "zerox-Void64-exe", "Void64", "direct-Void64-object", "direct-Void64-exe", true},
  {Z_DIRECT_BACKEND_COFF_X64, "zerox-coff-x64", "zerox-coff-x64-exe", "coff", "direct-coff-x64-object", "direct-coff-x64-exe", false},
};

static const ZDirectBackendRule direct_backend_rules[] = {
  {"elf", "linux", "x86_64", "gnu", Z_DIRECT_BACKEND_ELF64, true, true},
  {"elf", "linux", "x86_64", "musl", Z_DIRECT_BACKEND_ELF64, true, true},
  {"elf", "linux", "aarch64", "gnu", Z_DIRECT_BACKEND_ELF_AARCH64, true, true},
  {"elf", "linux", "aarch64", "musl", Z_DIRECT_BACKEND_ELF_AARCH64, true, true},
  {"macho", "macos", "aarch64", "darwin", Z_DIRECT_BACKEND_VOID64, true, true},
  {"coff", "windows", "x86_64", "msvc", Z_DIRECT_BACKEND_COFF_X64, true, true},
};

static const ZDirectBackendDescriptor *direct_backend_descriptor(ZDirectBackend backend) {
  for (size_t i = 0; i < sizeof(direct_backend_descriptors) / sizeof(direct_backend_descriptors[0]); i++) {
    if (direct_backend_descriptors[i].backend == backend) return &direct_backend_descriptors[i];
  }
  return NULL;
}

static bool target_field_matches(const char *actual, const char *expected) {
  return !expected || (actual && strcmp(actual, expected) == 0);
}

static ZDirectBackend direct_backend_for_target(const ZTargetInfo *target, bool executable) {
  if (!target) return Z_DIRECT_BACKEND_NONE;
  for (size_t i = 0; i < sizeof(direct_backend_rules) / sizeof(direct_backend_rules[0]); i++) {
    const ZDirectBackendRule *rule = &direct_backend_rules[i];
    if (executable ? !rule->exe_supported : !rule->object_supported) continue;
    if (!target_field_matches(target->object_format, rule->object_format)) continue;
    if (!target_field_matches(target->os, rule->os)) continue;
    if (!target_field_matches(target->arch, rule->arch)) continue;
    if (!target_field_matches(target->abi, rule->abi)) continue;
    return rule->backend;
  }
  return Z_DIRECT_BACKEND_NONE;
}

ZDirectBackend z_direct_object_backend(const ZTargetInfo *target) {
  return direct_backend_for_target(target, false);
}

ZDirectBackend z_direct_exe_backend(const ZTargetInfo *target) {
  return direct_backend_for_target(target, true);
}

const char *z_direct_backend_object_emitter(ZDirectBackend backend) {
  const ZDirectBackendDescriptor *descriptor = direct_backend_descriptor(backend);
  return descriptor ? descriptor->object_emitter : "none";
}

const char *z_direct_backend_exe_emitter(ZDirectBackend backend) {
  const ZDirectBackendDescriptor *descriptor = direct_backend_descriptor(backend);
  return descriptor ? descriptor->exe_emitter : "none";
}

ZDirectBackend z_direct_backend_from_emitter(const char *emitter) {
  if (!emitter) return Z_DIRECT_BACKEND_NONE;
  for (size_t i = 0; i < sizeof(direct_backend_descriptors) / sizeof(direct_backend_descriptors[0]); i++) {
    const ZDirectBackendDescriptor *descriptor = &direct_backend_descriptors[i];
    if (strcmp(emitter, descriptor->object_emitter) == 0) return descriptor->backend;
    if (strcmp(emitter, descriptor->exe_emitter) == 0) return descriptor->backend;
  }
  return Z_DIRECT_BACKEND_NONE;
}

const char *z_direct_backend_linker_flavor(ZDirectBackend backend) {
  const ZDirectBackendDescriptor *descriptor = direct_backend_descriptor(backend);
  return descriptor ? descriptor->linker_flavor : "none";
}

const char *z_direct_backend_artifact_path(ZDirectBackend backend, bool executable) {
  const ZDirectBackendDescriptor *descriptor = direct_backend_descriptor(backend);
  if (!descriptor) return "unsupported";
  return executable ? descriptor->exe_artifact_path : descriptor->object_artifact_path;
}

bool z_direct_backend_supports_runtime_object(ZDirectBackend backend) {
  const ZDirectBackendDescriptor *descriptor = direct_backend_descriptor(backend);
  return descriptor ? descriptor->runtime_object_supported : false;
}

bool z_direct_backend_emitter_is_executable(const char *emitter) {
  if (!emitter) return false;
  for (size_t i = 0; i < sizeof(direct_backend_descriptors) / sizeof(direct_backend_descriptors[0]); i++) {
    if (strcmp(emitter, direct_backend_descriptors[i].exe_emitter) == 0) return true;
  }
  return false;
}

bool z_direct_backend_is_request_name(const char *requested_backend) {
  if (!requested_backend || !requested_backend[0]) return false;
  for (size_t i = 0; i < sizeof(direct_backend_descriptors) / sizeof(direct_backend_descriptors[0]); i++) {
    if (strcmp(requested_backend, direct_backend_descriptors[i].object_emitter) == 0) return true;
  }
  return false;
}

bool z_direct_requested_backend_matches(const char *requested_backend, ZDirectBackend backend) {
  if (!requested_backend || !requested_backend[0]) return true;
  const ZDirectBackendDescriptor *descriptor = direct_backend_descriptor(backend);
  return descriptor && strcmp(requested_backend, descriptor->object_emitter) == 0;
}

const char *z_direct_object_emitter(const ZTargetInfo *target) {
  return z_direct_backend_object_emitter(z_direct_object_backend(target));
}

const char *z_direct_exe_emitter(const ZTargetInfo *target) {
  return z_direct_backend_exe_emitter(z_direct_exe_backend(target));
}

const char *z_direct_backend_status(const ZTargetInfo *target) {
  if (!target) return "known-unimplemented";
  if (z_direct_exe_backend(target) != Z_DIRECT_BACKEND_NONE) return "native-exe";
  if (z_direct_object_backend(target) != Z_DIRECT_BACKEND_NONE) return "native-object";
  return "known-unimplemented";
}

const char *z_direct_backend_reason(const ZTargetInfo *target) {
  if (!target) return "unknown target";
  const char *format = target->object_format ? target->object_format : "unknown";
  const char *arch = target->arch ? target->arch : "unknown";
  if (z_direct_object_backend(target) != Z_DIRECT_BACKEND_NONE) {
    if (z_direct_exe_backend(target) != Z_DIRECT_BACKEND_NONE) return "direct object and executable backend available";
    return "direct object backend available; direct executable linker is not implemented for this target";
  }
  if (strcmp(format, "elf") == 0 && strcmp(arch, "aarch64") == 0) return "AArch64 ELF machine-code backend is not implemented yet";
  if (strcmp(format, "coff") == 0 && strcmp(arch, "aarch64") == 0) return "AArch64 COFF machine-code backend is not implemented yet";
  return "direct backend is not implemented for this target format/architecture pair";
}
