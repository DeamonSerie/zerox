#!/usr/bin/env -S node --experimental-strip-types --disable-warning=ExperimentalWarning
import { execFileSync } from "node:child_process";
import { mkdirSync } from "node:fs";

const targetByHost = {
  "darwin:arm64": "darwin-arm64",
  "darwin:x64": "darwin-x64",
  "linux:arm64": "linux-musl-arm64",
  "linux:x64": "linux-musl-x64",
  "win32:arm64": "win32-arm64.exe",
  "win32:x64": "win32-x64.exe",
};

function run(command, args, options = {}) {
  execFileSync(command, args, { stdio: "inherit", ...options });
}

const target = targetByHost[`${process.platform}:${process.arch}`];
if (!target) {
  console.error(
    `native smoke does not know a runnable target for ${process.platform}/${process.arch}`,
  );
  process.exit(1);
}

const out = ".zerox/out/add-native";
const exe = target.startsWith("win32-") ? `${out}.exe` : out;

mkdirSync(".zerox/out", { recursive: true });
run("make", ["-C", "native/zerox-c"]);
run("bin/zerox", ["check", "examples/hello.0"]);
run("bin/zerox", [
  "build",
  "--emit",
  "exe",
  "--target",
  target,
  "examples/add.0",
  "--out",
  out,
]);
run(exe, []);
