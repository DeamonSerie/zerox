# Zero Evals

TypeScript evals for agent-facing Zero workflows.

Run the checked-in fixture without calling Claude:

```sh
pnpm evals -- --case hello-world --fixture
```

Run live in Vercel Sandbox through Claude Code and Vercel AI Gateway:

```sh
AI_GATEWAY_API_KEY=... pnpm evals -- --case hello-world
```

Live evals create a Vercel Sandbox, upload the current checkout, build the
native compiler, install Claude Code, and run the agent inside the sandbox. The
sandbox network policy injects the AI Gateway bearer credential for
`https://ai-gateway.vercel.sh`, matching the Ovation sandbox setup.

Credential options:

```sh
# Sandbox auth
VERCEL_OIDC_TOKEN=...
# or VERCEL_TOKEN=... VERCEL_TEAM_ID=... VERCEL_PROJECT_ID=...

# AI Gateway auth
AI_GATEWAY_API_KEY=...
# or ANTHROPIC_AUTH_TOKEN=...
```

The model must load Zero's version-matched skill through
`bin/zero skills get zero --full`, then use `bin/zero check` and `bin/zero run`
inside the sandbox to verify its candidate.

The eval system prompt intentionally avoids Zero syntax examples. The model is
expected to learn task-relevant syntax from the version-matched skills and
compiler feedback.
