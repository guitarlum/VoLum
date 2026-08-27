# Local markdown planning tracker, lean docs

Agent planning (wayfinder maps, specs, implementation tickets) lives in tracked
`.scratch/<effort>/`, not GitHub Issues (those stay user-facing) and not new
`backlog/` prompt files. Product specs are working memory: delete the
`.scratch` effort when it ships. `CONTEXT.md` is created only when a product
session actually resolves a term. ADRs only when the decision is hard to
reverse, surprising without context, and the result of a real trade-off.

**Considered options:** GitHub Issues for agent tickets (rejected: public
process noise on a user-facing tracker); gitignored `.scratch/` (rejected: maps
would not survive clone or the next machine); a fat always-loaded spec at repo
root (rejected: it rots and pollutes every session).
