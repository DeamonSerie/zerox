#!/usr/bin/env -S node --experimental-strip-types --disable-warning=ExperimentalWarning
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "..",
);
const outPath = path.join(
  repoRoot,
  "native/zerox-c/src/embedded_runtime_sources.inc",
);
const inputs = [
  ["zerox_embedded_zerox_runtime_h", "native/zerox-c/include/zerox_runtime.h"],
  ["zerox_embedded_zerox_runtime_c", "native/zerox-c/runtime/zerox_runtime.c"],
  [
    "zerox_embedded_zerox_http_curl_c",
    "native/zerox-c/runtime/zerox_http_curl.c",
  ],
  ["zerox_embedded_zerox_crypto_h", "native/zerox-c/runtime/zerox_crypto.h"],
  ["zerox_embedded_zerox_crypto_c", "native/zerox-c/runtime/zerox_crypto.c"],
];

function chunkText(text) {
  const chunks = [];
  let current = "";
  for (const line of text.match(/[^\n]*\n|[^\n]+$/g) || []) {
    if (current && JSON.stringify(current + line).length > 3000) {
      chunks.push(current);
      current = "";
    }
    if (JSON.stringify(line).length <= 3000) {
      current += line;
      continue;
    }
    for (let index = 0; index < line.length; index += 1400) {
      if (current) {
        chunks.push(current);
        current = "";
      }
      chunks.push(line.slice(index, index + 1400));
    }
  }
  if (current) chunks.push(current);
  return chunks;
}

const out = [];
out.push(
  "/* Generated from Zerox runtime sources. Run node --experimental-strip-types --disable-warning=ExperimentalWarning scripts/embed-runtime-sources.mts to refresh. */",
);
out.push("#ifndef ZEROX_EMBEDDED_RUNTIME_SOURCES_INC");
out.push("#define ZEROX_EMBEDDED_RUNTIME_SOURCES_INC");
out.push("");

for (const [name, relativePath] of inputs) {
  const text = fs.readFileSync(path.join(repoRoot, relativePath), "utf8");
  out.push(`static const char *const ${name}[] = {`);
  for (const chunk of chunkText(text)) {
    out.push(`  ${JSON.stringify(chunk)},`);
  }
  out.push("  NULL");
  out.push("};");
  out.push("");
}

out.push("#endif");
out.push("");

fs.writeFileSync(outPath, out.join("\n"));
