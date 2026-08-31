# Issue tracker: Local Markdown

Planning issues and specs for this repo live as markdown files in `.scratch/`.
GitHub Issues stay for incoming user-facing bugs and requests. Do not publish
wayfinder maps, grilling tickets, or agent implementation tickets there.

`backlog/` is the legacy paste-prompt store. New planning goes here. Live
backlog prompts remain until a wayfinder session migrates them.

## Two kinds of effort directory (do not mix)

A directory is **either** a wayfinder map **or** an implementation spec.

| Kind | Marker | `issues/` tickets | Status values |
| --- | --- | --- | --- |
| **Map** | `map.md` | Decision tickets (`Type: research\|prototype\|grilling\|task`) | `open`, `claimed`, `resolved` |
| **Spec** | `spec.md`, no `map.md` | Implementation tickets | `ready-for-agent`, `claimed`, `resolved` |

Never put `ready-for-agent` tickets in a directory that has `map.md`. When a
map is clear, create a **new** `.scratch/<feature-slug>/` for each spec.

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

When every child of `map.md` is `resolved`, stop `/wayfinder`. New chat:

1. Write `.scratch/<feature-slug>/spec.md` plus `issues/` with
   `Status: ready-for-agent` (one feature directory per headline, not inside
   the map folder).
2. One main agent; sub-agents per spec. The iteration loop in
   `vo-lum-workflow` applies: tests, changelog, docs, UAT build for the owner.
3. Do not promote to `main` without that UAT. Set the timebox here — not as a
   standing promise on the map.
