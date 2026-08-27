# Domain Docs

How the engineering skills should consume this repo's domain documentation when
exploring the codebase.

## Before exploring, read these

- **`CONTEXT.md`** at the repo root, or
- **`CONTEXT-MAP.md`** at the repo root if it exists: it points at one
  `CONTEXT.md` per context. Read each one relevant to the topic.
- **`docs/adr/`**: read ADRs that touch the area you're about to work in.

If any of these files don't exist, **proceed silently**. Don't flag their
absence; don't suggest creating them upfront. `/domain-modeling` (reached via
`/grill-with-docs`) creates them lazily when terms or decisions actually get
resolved.

`CONTEXT.md` is a glossary of product nouns, not a spec. Do not put
implementation, file paths, or feature plans in it.

## File structure

Single-context repo:

```
/
├── CONTEXT.md          ← lazy; create on first resolved term
├── docs/adr/
│   └── 0001-agent-planning-workflow.md
└── NeuralAmpModeler/
```

## Use the glossary's vocabulary

When your output names a domain concept (in an issue title, a refactor
proposal, a hypothesis, a test name), use the term as defined in `CONTEXT.md`.
Don't drift to synonyms the glossary explicitly avoids.

If the concept you need isn't in the glossary yet, that's a signal: either
you're inventing language the project doesn't use (reconsider) or there's a
real gap (note it for `/domain-modeling`).

## Flag ADR conflicts

If your output contradicts an existing ADR, surface it explicitly rather than
silently overriding.
