import Anthropic from "@anthropic-ai/sdk";
import { execFile } from "node:child_process";
import { mkdir, rm, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";
import { evalCases, findEvalCase, type EvalCase } from "./cases.js";
import {
  extractZeroSource,
  finalSourceResponseFailures,
  sourcePatternFailures,
} from "./source.js";

type MessageParam = Anthropic.MessageCreateParams["messages"][number];
type Tool = NonNullable<Anthropic.MessageCreateParams["tools"]>[number];
type ContentBlock = Anthropic.Message["content"][number];

interface RunOptions {
  caseId: string | null;
  dryRun: boolean;
  fixture: boolean;
  json: boolean;
  maxTurns: number;
  model: string;
  outDir: string;
}

interface CommandResult {
  code: number;
  stdout: string;
  stderr: string;
}

interface AgentToolCall {
  id: string;
  name: string;
  input: unknown;
}

interface AgentToolResult {
  toolUseId: string;
  name: string;
  output: unknown;
}

interface AgentStep {
  turn: number;
  id: string;
  stopReason: string | null;
  text: string;
  toolCalls: AgentToolCall[];
  toolResults: AgentToolResult[];
  usage: Anthropic.Message["usage"];
}

interface AgentMetrics {
  turnCount: number;
  toolCallCount: number;
  zeroCliCallCount: number;
  zeroSkillLoadCount: number;
  zeroCheckCallCount: number;
  zeroRunCallCount: number;
}

interface AgentRun {
  responseText: string;
  steps: AgentStep[];
  metrics: AgentMetrics | null;
}

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const zero = join(repoRoot, "bin", "zero");
const AI_GATEWAY_URL = "https://ai-gateway.vercel.sh";
const TOOL_OUTPUT_LIMIT = 16_000;

const ALLOWED_ZERO_SUBCOMMANDS = new Set([
  "--version",
  "check",
  "doctor",
  "explain",
  "fix",
  "fmt",
  "graph",
  "parse",
  "run",
  "size",
  "skills",
  "targets",
  "test",
  "tokens",
]);

const systemPrompt = [
  "You are an agent evaluating a Zero programming task.",
  "Use zero_cli as your source of Zero-specific guidance and verification.",
  "First call zero_cli with args [\"skills\", \"get\", \"zero\", \"--full\"].",
  "Load any additional skills recommended by that skill before writing code.",
  "Use zero_cli feedback to check and run your candidate.",
  "For the final answer, output code only: exactly the verified source bytes.",
  "Do not summarize success, mention stdout, add a preamble, or use Markdown fences.",
].join("\n");

const zeroCliTool: Tool = {
  name: "zero_cli",
  description: [
    "Run the repository's local bin/zero compiler with guarded subcommands.",
    "Use this to load Zero skills, check source, explain diagnostics, and run candidate Zero programs.",
    "For source-bearing commands, pass source and omit the source path; the tool writes a scratch .0 file and appends it.",
  ].join(" "),
  input_schema: {
    type: "object",
    properties: {
      args: {
        type: "array",
        minItems: 1,
        items: { type: "string" },
        description:
          "Arguments after bin/zero, for example [\"skills\", \"get\", \"zero\", \"--full\"] or [\"check\", \"--json\"].",
      },
      source: {
        type: "string",
        description:
          "Optional Zero source to write to a scratch .0 file and pass as the final command argument.",
      },
    },
    required: ["args"],
    additionalProperties: false,
  },
};

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const selectedCases = selectCases(options.caseId);

  if (options.dryRun) {
    printJsonOrText(options.json, {
      model: options.model,
      gatewayURL: AI_GATEWAY_URL,
      maxTurns: options.maxTurns,
      cases: selectedCases.map(
        ({ id, title, prompt, expectedStdout, requiredSourcePatterns }) => ({
          id,
          title,
          prompt,
          expectedStdout,
          requiredSourcePatternCount: requiredSourcePatterns.length,
        }),
      ),
    });
    return;
  }

  if (!options.fixture) {
    resolveGatewayCredential();
  }

  await mkdir(options.outDir, { recursive: true });

  const results = [];
  for (const evalCase of selectedCases) {
    results.push(await runCase(evalCase, options));
  }

  const summary = {
    ok: results.every((result) => result.passed),
    model: options.model,
    outDir: options.outDir,
    passed: results.filter((result) => result.passed).length,
    failed: results.filter((result) => !result.passed).length,
    results,
  };

  await writeFile(
    join(options.outDir, "summary.json"),
    `${JSON.stringify(summary, null, 2)}\n`,
  );
  printJsonOrText(options.json, summary);
  if (!summary.ok) process.exitCode = 1;
}

async function runCase(evalCase: EvalCase, options: RunOptions) {
  const started = performance.now();
  const caseDir = join(options.outDir, evalCase.id);
  await rm(caseDir, { force: true, recursive: true });
  await mkdir(caseDir, { recursive: true });

  const agentRun: AgentRun = options.fixture
    ? {
        responseText: evalCase.fixtureSource,
        steps: [],
        metrics: null,
      }
    : await runAgentCase(evalCase, options, caseDir);

  const responsePath = join(caseDir, "response.md");
  const stepsPath = options.fixture ? null : join(caseDir, "steps.json");
  const sourcePath = join(caseDir, "candidate.0");
  const source = extractZeroSource(agentRun.responseText);

  await writeFile(responsePath, agentRun.responseText);
  if (stepsPath) {
    await writeFile(stepsPath, `${JSON.stringify(agentRun.steps, null, 2)}\n`);
  }
  await writeFile(sourcePath, source);

  const check = await runCommand(zero, ["check", "--json", sourcePath]);
  let run: CommandResult | null = null;
  let error: string | null = null;
  if (check.code === 0) {
    run = await runCommand(zero, [
      "run",
      "--out",
      join(caseDir, "program"),
      sourcePath,
    ]);
  } else {
    error = "zero check failed";
  }

  const patternFailures = sourcePatternFailures(
    source,
    evalCase.requiredSourcePatterns,
  );
  const responseFormatFailures = finalSourceResponseFailures(
    agentRun.responseText,
    source,
  );
  const agentRequirementFailures = options.fixture
    ? []
    : getAgentRequirementFailures(agentRun.metrics);
  const actualStdout = run?.stdout ?? "";
  const passed =
    check.code === 0 &&
    run?.code === 0 &&
    actualStdout === evalCase.expectedStdout &&
    patternFailures.length === 0 &&
    responseFormatFailures.length === 0 &&
    agentRequirementFailures.length === 0;

  if (!passed && !error) {
    error = failureReason({
      run,
      actualStdout,
      expectedStdout: evalCase.expectedStdout,
      patternFailures,
      responseFormatFailures,
      agentRequirementFailures,
    });
  }

  const result = {
    id: evalCase.id,
    title: evalCase.title,
    passed,
    model: options.model,
    mode: options.fixture ? "fixture" : "agent",
    durationMs: Math.round(performance.now() - started),
    sourcePath,
    responsePath,
    stepsPath,
    agent: agentRun.metrics,
    check,
    run,
    expectedStdout: evalCase.expectedStdout,
    actualStdout,
    sourcePatternFailures: patternFailures,
    responseFormatFailures,
    agentRequirementFailures,
    error,
  };

  await writeFile(
    join(caseDir, "result.json"),
    `${JSON.stringify(result, null, 2)}\n`,
  );
  return result;
}

async function runAgentCase(
  evalCase: EvalCase,
  options: RunOptions,
  caseDir: string,
): Promise<AgentRun> {
  const client = createAnthropicClient();
  const executeZeroCli = createZeroCliExecutor(caseDir);
  const messages: MessageParam[] = [
    {
      role: "user",
      content: [
        evalCase.prompt,
        "",
        "Use zero_cli to load the Zero skill, check your candidate, and run it.",
        "After the source checks and produces the expected output, return code only.",
        "Do not include a success sentence, explanation, or Markdown fence.",
      ].join("\n"),
    },
  ];
  const steps: AgentStep[] = [];
  let latestText = "";

  for (let turn = 1; turn <= options.maxTurns; turn += 1) {
    const message = await client.messages.create({
      model: options.model,
      max_tokens: 2_000,
      temperature: 0,
      system: systemPrompt,
      messages,
      tools: [zeroCliTool],
    });

    const toolUses = message.content.filter(isToolUseBlock);
    latestText = extractText(message.content);
    const step: AgentStep = {
      turn,
      id: message.id,
      stopReason: message.stop_reason,
      text: truncate(latestText, 4_000),
      toolCalls: toolUses.map((toolUse) => ({
        id: toolUse.id,
        name: toolUse.name,
        input: toolUse.input,
      })),
      toolResults: [],
      usage: message.usage,
    };
    steps.push(step);

    if (toolUses.length === 0) {
      return {
        responseText: latestText,
        steps,
        metrics: measureAgent(steps),
      };
    }

    messages.push({
      role: "assistant",
      content: message.content as MessageParam["content"],
    });

    const toolResults = [];
    for (const toolUse of toolUses) {
      const output =
        toolUse.name === "zero_cli"
          ? await executeZeroCli(toolUse.input)
          : {
              error: `Unknown tool: ${toolUse.name}`,
            };
      step.toolResults.push({
        toolUseId: toolUse.id,
        name: toolUse.name,
        output: summarizeToolOutput(output),
      });
      toolResults.push({
        type: "tool_result" as const,
        tool_use_id: toolUse.id,
        content: JSON.stringify(output, null, 2),
        is_error: Boolean(
          output &&
            typeof output === "object" &&
            "error" in output &&
            !("command" in output),
        ),
      });
    }

    messages.push({
      role: "user",
      content: toolResults,
    });
  }

  return {
    responseText: latestText,
    steps,
    metrics: measureAgent(steps),
  };
}

function createAnthropicClient() {
  const credential = resolveGatewayCredential();
  const usesApiKey = credential.source === "AI_GATEWAY_API_KEY";
  return new Anthropic({
    apiKey: usesApiKey ? credential.value : null,
    authToken: usesApiKey ? null : credential.value,
    baseURL: AI_GATEWAY_URL,
  });
}

function resolveGatewayCredential() {
  const source =
    process.env.AI_GATEWAY_API_KEY !== undefined
      ? "AI_GATEWAY_API_KEY"
      : process.env.ANTHROPIC_AUTH_TOKEN !== undefined
        ? "ANTHROPIC_AUTH_TOKEN"
        : process.env.VERCEL_OIDC_TOKEN !== undefined
          ? "VERCEL_OIDC_TOKEN"
          : null;
  const value = source ? process.env[source] : undefined;

  if (!source || !value) {
    throw new Error(
      [
        "Missing AI Gateway credential.",
        "Set AI_GATEWAY_API_KEY, ANTHROPIC_AUTH_TOKEN, or VERCEL_OIDC_TOKEN.",
      ].join(" "),
    );
  }

  return { source, value };
}

function createZeroCliExecutor(caseDir: string) {
  let invocation = 0;
  return async (input: unknown) => {
    invocation += 1;
    const toolDir = join(caseDir, "tool");
    await mkdir(toolDir, { recursive: true });
    const { args, source } = parseZeroCliInput(input);
    const commandArgs = normalizeZeroArgs(args);
    let sourcePath: string | null = null;

    if (source !== undefined) {
      sourcePath = join(toolDir, `candidate-${invocation}.0`);
      await writeFile(sourcePath, extractZeroSource(source));
      if (commandArgs[0] === "run" && !commandArgs.includes("--out")) {
        commandArgs.splice(1, 0, "--out", join(toolDir, `program-${invocation}`));
      }
      commandArgs.push(sourcePath);
    }

    const result = await runCommand(zero, commandArgs, 20_000);
    const stdout = truncate(result.stdout, TOOL_OUTPUT_LIMIT);
    const stderr = truncate(result.stderr, TOOL_OUTPUT_LIMIT);
    return {
      command: ["bin/zero", ...commandArgs],
      code: result.code,
      stdout,
      stderr,
      stdoutTruncated: stdout.length !== result.stdout.length,
      stderrTruncated: stderr.length !== result.stderr.length,
      sourcePath,
    };
  };
}

function parseZeroCliInput(input: unknown) {
  if (!input || typeof input !== "object") {
    throw new Error("zero_cli input must be an object");
  }
  const args = "args" in input ? input.args : undefined;
  if (!Array.isArray(args) || args.some((arg) => typeof arg !== "string")) {
    throw new Error("zero_cli args must be an array of strings");
  }
  const source = "source" in input ? input.source : undefined;
  if (source !== undefined && typeof source !== "string") {
    throw new Error("zero_cli source must be a string when provided");
  }
  return { args, source };
}

function normalizeZeroArgs(args: string[]): string[] {
  const commandArgs = args.map((arg) => {
    if (arg.includes("\0")) {
      throw new Error("zero_cli arguments cannot contain NUL bytes");
    }
    if (arg.startsWith("/") || arg.includes("..")) {
      throw new Error(
        "zero_cli only accepts relative arguments inside the eval scratch flow",
      );
    }
    return arg;
  });
  const subcommand = commandArgs[0];
  if (!subcommand || !ALLOWED_ZERO_SUBCOMMANDS.has(subcommand)) {
    const allowed = [...ALLOWED_ZERO_SUBCOMMANDS].sort().join(", ");
    throw new Error(
      `zero_cli subcommand '${subcommand ?? ""}' is not allowed. Allowed: ${allowed}`,
    );
  }
  return commandArgs;
}

function parseArgs(args: string[]): RunOptions {
  let caseId: string | null = null;
  let dryRun = false;
  let fixture = false;
  let json = false;
  let maxTurns = 10;
  let model = process.env.ZERO_EVAL_MODEL ?? "anthropic/claude-sonnet-4.6";
  let outDir = join(repoRoot, ".zero", "evals", "runs", timestamp());

  for (let i = 0; i < args.length; i += 1) {
    const arg = args[i];
    if (arg === "--") {
      continue;
    } else if (arg === "--case") {
      caseId = requiredValue(args, ++i, "--case");
    } else if (arg === "--model") {
      model = requiredValue(args, ++i, "--model");
    } else if (arg === "--max-turns") {
      maxTurns = parsePositiveInt(
        requiredValue(args, ++i, "--max-turns"),
        "--max-turns",
      );
    } else if (arg === "--out") {
      outDir = resolve(requiredValue(args, ++i, "--out"));
    } else if (arg === "--dry-run") {
      dryRun = true;
    } else if (arg === "--fixture") {
      fixture = true;
    } else if (arg === "--json") {
      json = true;
    } else if (arg === "--help" || arg === "-h") {
      printHelp();
      process.exit(0);
    } else {
      throw new Error(`Unknown evals flag: ${arg}`);
    }
  }

  return { caseId, dryRun, fixture, json, maxTurns, model, outDir };
}

function selectCases(caseId: string | null) {
  if (!caseId) return evalCases;
  const evalCase = findEvalCase(caseId);
  if (!evalCase) {
    const ids = evalCases.map((item) => item.id).join(", ");
    throw new Error(`Unknown eval case '${caseId}'. Available cases: ${ids}`);
  }
  return [evalCase];
}

function requiredValue(args: string[], index: number, flag: string): string {
  const value = args[index];
  if (!value || value.startsWith("--")) {
    throw new Error(`${flag} requires a value`);
  }
  return value;
}

function parsePositiveInt(value: string, flag: string): number {
  const parsed = Number.parseInt(value, 10);
  if (!Number.isSafeInteger(parsed) || parsed <= 0) {
    throw new Error(`${flag} requires a positive integer`);
  }
  return parsed;
}

async function runCommand(
  command: string,
  args: string[],
  timeoutMs = 15_000,
): Promise<CommandResult> {
  return new Promise((resolveCommand) => {
    execFile(
      command,
      args,
      { cwd: repoRoot, encoding: "utf8", timeout: timeoutMs },
      (error, stdout, stderr) => {
        const code =
          typeof error?.code === "number" ? error.code : error ? 1 : 0;
        resolveCommand({ code, stdout, stderr });
      },
    );
  });
}

function failureReason(input: {
  run: CommandResult | null;
  actualStdout: string;
  expectedStdout: string;
  patternFailures: string[];
  responseFormatFailures: string[];
  agentRequirementFailures: string[];
}) {
  if (!input.run) return "zero run did not execute";
  if (input.run.code !== 0) return `zero run exited ${input.run.code}`;
  if (input.actualStdout !== input.expectedStdout) {
    return "stdout did not match expected output";
  }
  if (input.patternFailures.length > 0) {
    return "source did not match required patterns";
  }
  if (input.responseFormatFailures.length > 0) {
    return "final response included prose or Markdown";
  }
  if (input.agentRequirementFailures.length > 0) {
    return "agent did not use required Zero CLI feedback";
  }
  return "unknown failure";
}

function getAgentRequirementFailures(metrics: AgentMetrics | null): string[] {
  if (!metrics) return ["missing agent metrics"];
  const failures = [];
  if (metrics.zeroCliCallCount === 0) failures.push("zero_cli was not called");
  if (metrics.zeroSkillLoadCount === 0) {
    failures.push("zero skill was not loaded");
  }
  if (metrics.zeroCheckCallCount === 0) failures.push("zero check was not called");
  if (metrics.zeroRunCallCount === 0) failures.push("zero run was not called");
  return failures;
}

function measureAgent(steps: AgentStep[]): AgentMetrics {
  const toolCalls = steps.flatMap((step) => step.toolCalls);
  const zeroCliCalls = toolCalls.filter((call) => call.name === "zero_cli");
  return {
    turnCount: steps.length,
    toolCallCount: toolCalls.length,
    zeroCliCallCount: zeroCliCalls.length,
    zeroSkillLoadCount: zeroCliCalls.filter((call) =>
      isZeroSkillLoad(inputArgs(call.input)),
    ).length,
    zeroCheckCallCount: zeroCliCalls.filter(
      (call) => firstZeroArg(call.input) === "check",
    ).length,
    zeroRunCallCount: zeroCliCalls.filter(
      (call) => firstZeroArg(call.input) === "run",
    ).length,
  };
}

function isZeroSkillLoad(args: unknown): boolean {
  return (
    Array.isArray(args) &&
    args[0] === "skills" &&
    args[1] === "get" &&
    args[2] === "zero" &&
    args.includes("--full")
  );
}

function inputArgs(input: unknown): unknown {
  if (!input || typeof input !== "object" || !("args" in input)) return null;
  return input.args;
}

function firstZeroArg(input: unknown): string | null {
  const args = inputArgs(input);
  if (!Array.isArray(args) || typeof args[0] !== "string") return null;
  return args[0];
}

function isToolUseBlock(
  block: ContentBlock,
): block is Extract<ContentBlock, { type: "tool_use" }> {
  return block.type === "tool_use";
}

function extractText(content: ContentBlock[]): string {
  return content
    .filter((block): block is Extract<ContentBlock, { type: "text" }> => {
      return block.type === "text";
    })
    .map((block) => block.text)
    .join("");
}

function summarizeToolOutput(output: unknown) {
  if (!output || typeof output !== "object") return output;
  const maybeZeroOutput = output as {
    command?: unknown;
    code?: unknown;
    stdout?: unknown;
    stderr?: unknown;
    stdoutTruncated?: unknown;
    stderrTruncated?: unknown;
    sourcePath?: unknown;
  };
  if (!Array.isArray(maybeZeroOutput.command)) return output;
  return {
    command: maybeZeroOutput.command,
    code: maybeZeroOutput.code,
    stdout: truncate(String(maybeZeroOutput.stdout ?? ""), 4_000),
    stderr: truncate(String(maybeZeroOutput.stderr ?? ""), 2_000),
    stdoutTruncated: maybeZeroOutput.stdoutTruncated,
    stderrTruncated: maybeZeroOutput.stderrTruncated,
    sourcePath: maybeZeroOutput.sourcePath,
  };
}

function truncate(text: string, limit: number): string {
  if (text.length <= limit) return text;
  return `${text.slice(0, limit)}\n...[truncated ${text.length - limit} chars]`;
}

function timestamp(): string {
  return new Date().toISOString().replace(/[:.]/g, "-");
}

function printJsonOrText(json: boolean, value: unknown) {
  if (json) {
    console.log(JSON.stringify(value, null, 2));
    return;
  }

  if (isSummary(value)) {
    for (const result of value.results) {
      const steps = result.agent
        ? `, ${result.agent.turnCount} turns, ${result.agent.toolCallCount} tools`
        : "";
      console.log(
        `${result.passed ? "PASS" : "FAIL"} ${result.id} ${result.mode} (${result.durationMs} ms${steps})`,
      );
      if (!result.passed && result.error) console.log(`  ${result.error}`);
      if (result.agentRequirementFailures.length > 0) {
        console.log(`  ${result.agentRequirementFailures.join("; ")}`);
      }
      if (result.responseFormatFailures.length > 0) {
        console.log(`  ${result.responseFormatFailures.join("; ")}`);
      }
    }
    console.log(`results: ${value.outDir}`);
    return;
  }

  console.log(JSON.stringify(value, null, 2));
}

function isSummary(value: unknown): value is {
  outDir: string;
  results: Array<{
    id: string;
    mode: string;
    passed: boolean;
    durationMs: number;
    error: string | null;
    agent: AgentMetrics | null;
    agentRequirementFailures: string[];
    responseFormatFailures: string[];
  }>;
} {
  return Boolean(
    value &&
      typeof value === "object" &&
      "results" in value &&
      Array.isArray(value.results),
  );
}

function printHelp() {
  console.log(`Usage: pnpm evals -- [options]

Options:
  --case <id>       Run one case, e.g. hello-world
  --model <id>      AI Gateway model id (default: ZERO_EVAL_MODEL or anthropic/claude-sonnet-4.6)
  --max-turns <n>   Maximum Claude tool turns (default: 10)
  --out <dir>       Output directory (default: .zero/evals/runs/<timestamp>)
  --dry-run         Print selected cases without calling Claude
  --fixture         Run checked-in fixture answers instead of calling Claude
  --json            Print JSON output
`);
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
});
