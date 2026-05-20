---
name: captain-hindsight
description: Opt-in implementation retro that turns observed friction into concrete tracked artifacts (new/edited rules, skills, scripts, docs) or local backlog items. Use only when the user invokes it ("captain hindsight" or "/captain-hindsight"). Not for trivial tasks.
disable-model-invocation: true
---

# Captain Hindsight (opt-in retro)

Goal: cheap, action-shaped reflection that pays for itself by lowering token use or raising quality next time. Avoid agent theater — everything must end as a concrete artifact or be discarded.

## When to run

Only when the user explicitly asks (`captain hindsight`, `/captain-hindsight`, etc.).

Skip and tell the user "nothing worth a retro" if all of these are true:

- Fewer than ~5 tool calls were needed.
- No failed tool calls, no user corrections, no scope changes mid-task.
- No surprise during implementation (no "I expected X, got Y").

## Process

1. Look back at the implementation chat. Identify at most 5 friction points. A friction point is one of:
   - **Token burn**: re-reading the same file, exploring blind, redoing a search.
   - **Quality miss**: a bug the existing tests/rules/skills would have caught if updated.
   - **Tool gap**: a missing script, MCP tool, or skill that would have shortened the task.
   - **Rule/skill miss**: an existing rule/skill that wasn't loaded when it should have been, or whose guidance was wrong/stale.
   - **Process miss**: a workflow step skipped or done in the wrong order (branching, tests, docs, changelog).
2. For each friction point, classify the proposed action as exactly one of:
   - **Rule**: edit/create `.cursor/rules/*.mdc`.
   - **Skill**: edit/create `.cursor/skills/*/SKILL.md`.
   - **Script**: edit/create under `NeuralAmpModeler/scripts/` or `scripts/`.
   - **Doc**: edit `AGENTS.md`, a README, or a user guide.
   - **Test**: add/update a doctest, packaging check, or layout regression.
   - **Backlog**: file a new `B*/U*/P*/O*/F*/D*/R*` item in `backlog/` (local-only is fine for CIP/refactor).
   - **Discard**: not worth the ceremony; say why in one line.
3. Output exactly:
   - A numbered list. Each entry: `friction (one line)` -> `classification` -> `concrete proposal (one line, name the file path)` -> `expected payoff (one line, e.g. "saves ~N file reads next time" / "would have caught bug X")`.
   - No narrative, no preamble, no "great job."
4. Stop after the list and the closing recommendation. Do NOT implement the proposals unless the user explicitly says "go" / "implement N" / similar.

## Closing recommendation

After the numbered list, append one short block:

`Recommended to implement now: #X, #Y. Defer: #Z. Discard the rest.`

Add one line per pick explaining why it is highest leverage (token savings, bug class avoided, repeat task shortened). Keep it terse: no narrative, no victory lap, just pick the winners.

If every entry is **Discard**, skip this block.

## Constraints

- Hard cap: 5 entries. If you have more, pick the highest-leverage 5 and drop the rest.
- Every entry must name a concrete file path or backlog ID. "Maybe a rule about X" is not allowed.
- If everything classifies as **Discard**, return one line saying so. Do not pad.
- Do not propose duplicates of existing rules/skills/scripts — check first.
- Respect `vo-lum-workflow.mdc`: small slices, no drive-by cleanups, no ceremony for ceremony's sake.

## After the user picks proposals

- Implement only the picked entries.
- For **Rule/Skill/Script/Doc/Test** entries: edit in place on the current branch (usually `dev` or a feature branch), then commit alongside the implementation.
- For **Backlog** entries: write the new file under `backlog/` and add its line to `backlog/README.md` under the correct category. Do not push (it is gitignored).
