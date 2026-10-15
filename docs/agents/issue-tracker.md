# Issue tracker: Local Markdown

Planning issues and specs for this repo live as markdown files in `.scratch/`.
GitHub Issues stay for incoming user-facing bugs and requests. Do not publish
wayfinder maps, grilling tickets, or agent implementation tickets there.

`backlog/` is the legacy paste-prompt store. New planning goes here. Live
backlog prompts remain until a wayfinder session migrates them.

## Two kinds of effort directory (do not mix)

A directory is **either** a wayfinder map **or** an implementation spec.
A spec (or a simple-goal loop) may also carry `loop.md`; that file is the
conductor SSOT, not a third kind of effort.

| Kind | Marker | `issues/` tickets | Status values |
| --- | --- | --- | --- |
| **Map** | `map.md` | Decision tickets (`Type: research\|prototype\|grilling\|task`) | `open`, `claimed`, `resolved` |
| **Spec** | `spec.md`, no `map.md` | Implementation tickets | `ready-for-agent`, `claimed`, `resolved` |

Never put `ready-for-agent` tickets in a directory that has `map.md`. When a
map is clear, create a **new** `.scratch/<feature-slug>/` for each spec. A
simple `/conductor` goal with no spec writes only `loop.md` in a new slug
directory.

## Conventions

- One effort per directory: `.scratch/<feature-slug>/`
- Implementation issues are one file per ticket at
  `.scratch/<feature-slug>/issues/<NN>-<slug>.md`, numbered from `01`, never a
  single combined tickets file
- Comments append under a `## Comments` heading
- When the effort ships, delete `.scratch/<feature-slug>/`. Specs are working
  memory, not always-loaded docs.

## When a skill says "publish to the issue tracker"

Create a new file under `.scratch/<feature-slug>/` (creating the directory if
needed).

## When a skill says "fetch the relevant ticket"

Read the file at the referenced path. The user will normally pass the path or
the issue number directly.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a file with one **child** file per ticket.

- **Map**: `.scratch/<effort>/map.md` (the Notes / Decisions-so-far / Fog body).
- **Child ticket**: `.scratch/<effort>/issues/NN-<slug>.md`, numbered from `01`,
  with the question in the body. A `Type:` line records the ticket type
  (`research`/`prototype`/`grilling`/`task`); a `Status:` line records
  `open` / `claimed` / `resolved`.
- **Blocking**: a `Blocked by: NN, NN` line near the top. A ticket is unblocked
  when every file it lists is `resolved`.
- **Frontier**: open (`Status: open` or missing), unblocked, and not `claimed`;
  first by number wins.
- **Claim**: set `Status: claimed` and save before any work.
- **Re-read**: after context compression, or whenever the chat might be
  summarizing, read `map.md` and the claimed ticket from disk again. Do not
  treat earlier conversation as the map.
- **Resolve**: append the answer under an `## Answer` heading, set
  `Status: resolved`, then append a context pointer (gist + link) to the map's
  Decisions-so-far in `map.md`.

## After the map (conductor)

When every child of `map.md` is `resolved`, stop `/wayfinder`. Sitting-with-you
work is Plan / implement, not `/conductor`. AFK / overnight: `/conductor`
(see `.cursor/skills/conductor/SKILL.md`). Do not promote to `main` without
owner UAT. One-step and one-slice work skip both `/wayfinder` and `/conductor`.

## Conductor operations

Mechanics live in `.cursor/skills/conductor/SKILL.md`. This file is the
`loop.md` template only.

- **File**: `.scratch/<slug>/loop.md`
- **Status**: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort <slug>`
  (`-Path` if the file is not under `.scratch/`; `-RequirePredicate` before
  dispatch spawns). Several loops with no `-Effort` is an error.
- **Predicate**: prefer `.scratch/<slug>/verify.ps1`. Bundles (bug / UI / DSP /
  packaging / sound) are in the skill.
- **Verifier**: write `hash:` from the status script’s `worktree_hash` and
  `verdict: accepted` or `defects`. A later `git diff HEAD` change invalidates it.
- **Wake prompt**: three lines only (below). Copy into Cursor `/loop`.
- **Ledger**: append-only. Never rewrite rows from memory.

Template:

```markdown
# <slug>

## Goal

<one sentence outcome>

## Predicate

- `pwsh .scratch/<slug>/verify.ps1` exit 0

## Wake prompt

First tool call: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort <slug>`.
Do not trust chat memory. Continue until every Predicate line passes.
Escalate and stop on new product, new sound / golden retune, irreversible git, or a real dead end.

## Verifier

- hash:
- verdict:

## Children

(none)

## Ledger
```
