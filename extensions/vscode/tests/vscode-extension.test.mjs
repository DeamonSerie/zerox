import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { describe, it } from "node:test";

describe("VS Code extension manifest", () => {
  it("contributes Zerox language support for .0 files", async () => {
    const manifest = JSON.parse(await readFile("package.json", "utf8"));
    const language = manifest.contributes?.languages?.find(
      (entry) => entry.id === "zerox",
    );
    const grammar = manifest.contributes?.grammars?.find(
      (entry) => entry.language === "zerox",
    );
    const snippets = manifest.contributes?.snippets?.find(
      (entry) => entry.language === "zerox",
    );

    assert.ok(language);
    assert.deepEqual(language.extensions, [".0"]);
    assert.equal(language.configuration, "./language-configuration/zerox.json");

    assert.ok(grammar);
    assert.equal(grammar.scopeName, "source.zerox");
    assert.equal(grammar.path, "./syntaxes/zerox.tmLanguage.json");

    assert.ok(snippets);
    assert.equal(snippets.path, "./snippets/zerox.json");
  });

  it("ships snippets for main, shape, fun, and test", async () => {
    const snippets = JSON.parse(await readFile("snippets/zerox.json", "utf8"));
    assert.ok(snippets.main);
    assert.ok(snippets.shape);
    assert.ok(snippets.function);
    assert.ok(snippets.test);
  });

  it("highlights core Zerox keywords and types", async () => {
    const grammar = JSON.parse(
      await readFile("syntaxes/zerox.tmLanguage.json", "utf8"),
    );
    const matches = Object.values(grammar.repository)
      .flatMap((entry) => entry.patterns)
      .map((pattern) => pattern.match ?? "")
      .join("\n");

    assert.match(matches, /fun/);
    assert.match(matches, /raises/);
    assert.match(matches, /shape/);
    assert.match(matches, /World/);
    assert.doesNotMatch(matches, /Vercel|Request|Response/);
  });

  it("highlights comments, strings, and numbers", async () => {
    const grammar = JSON.parse(
      await readFile("syntaxes/zerox.tmLanguage.json", "utf8"),
    );
    assert.equal(
      grammar.repository.comments.patterns[0].name,
      "comment.line.double-slash.zerox",
    );
    assert.equal(
      grammar.repository.comments.patterns[1].name,
      "comment.block.zerox",
    );
    assert.equal(
      grammar.repository.strings.patterns[0].name,
      "string.quoted.double.zerox",
    );
    assert.equal(
      grammar.repository.numbers.patterns[0].name,
      "constant.numeric.zerox",
    );
  });
});
