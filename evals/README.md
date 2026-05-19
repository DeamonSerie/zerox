# Zero Evals

TypeScript evals for agent-facing Zero workflows.

Run the checked-in fixture without calling Claude:

```sh
pnpm evals -- --case hello-world --fixture
```

Run live through Vercel AI Gateway:

```sh
AI_GATEWAY_API_KEY=... pnpm evals -- --case hello-world
```

The runner uses the Anthropic TypeScript SDK pointed at
`https://ai-gateway.vercel.sh`. The model must load Zero's version-matched skill
through `bin/zero skills get zero --full`, then use the guarded `zero_cli` tool
to check and run its candidate.

The eval system prompt intentionally avoids Zero syntax examples. The model is
expected to learn task-relevant syntax from the version-matched skills and
compiler feedback.
