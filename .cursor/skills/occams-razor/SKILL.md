---
name: occams-razor
description: Periodic redundancy and overhead review of this repo's Cursor rules, skills, and AGENTS.md. Use when the user asks to audit, prune, or trim rules/skills, reduce agent overhead or entropy, or invokes /occams-razor. Opt-in; not for routine tasks.
disable-model-invocation: true
---

# Rules And Skills Overhead Audit

Cut accumulated overhead in `.cursor/rules/*.mdc`, `.cursor/skills/*/SKILL.md`,
and `AGENTS.md`. Entropy only rises by default and frontier models keep getting
smarter, so guidance that was once worth stating becomes dead weight. Run this
every few months or after a major model upgrade.

## Two redundancy classes

1. **Model-knowledge overlap**: lines that restate defaults the current model
   already follows reliably.
2. **Cross-file duplication**: the same fact stated in full in 2+ files.

## The nuanced bar (keep vs cut)

KEEP:

- Any line that overrides a known model failure mode, even if it "sounds
  generic" (e.g. PowerShell `;` not `&&`, here-string commit messages, HTTPS git
  remote, "you run builds/tests", a concrete refactor-at-N-lines trigger).
- Every project-specific fact: param/serialization contracts, file maps,
  packaging invariants, signing fixes, upstream boundaries, version deep-dives.

CUT:

- Lines that only restate reliable model/Cursor defaults: generic
  markdown/citation formatting, generic git safety already in the system prompt,
  bare "write tests" / "match existing patterns" framing (keep the
  project-specific test-file lists).

When unsure whether the model still needs a line, assume it does not, but cut
only if nothing project-specific is lost. Bias toward keeping overrides.

## De-dup rule

Collapse duplicates onto ONE canonical owner plus short pointers. Point ONLY to
an always-loaded source:

- Always-applied rules (`alwaysApply: true`) and `AGENTS.md` are always in context.
- Glob-scoped rules and model-invoked skills are NOT.
- Never replace a fact in a glob rule with a pointer to another glob rule; the
  target may not be loaded. A skill may point to a rule that auto-attaches in the
  same edit context.

## Process

1. Enumerate every rule, skill, and `AGENTS.md`; record line counts.
2. Read each fully. Classify every section/bullet: KEEP / CUT / DEDUP -> owner.
3. Pick a canonical owner for any fact stated in 2+ files.
4. Flag whole files that mostly restate a paired rule as delete-or-slim
   candidates. Never silently delete a file; it may be referenced by `AGENTS.md`
   or invoked by name.
5. Produce a short report first (no edits): per file, the cuts/dedups with
   one-line rationale, plus the net line delta.

## After the user approves

- Branch off `dev` per `vo-lum-workflow.mdc` "Branching".
- Make the edits; keep `.mdc` frontmatter (`description`/`globs`/`alwaysApply`)
  and `SKILL.md` frontmatter intact.
- Verify every new pointer resolves to a still-existing heading in an
  always-loaded source.
- Docs-only: no build/tests. Open a PR into `dev` with before/after line counts
  and any delete-or-slim decisions called out.

## Healthy baseline

A disciplined set trims only single-digit percent per audit. If a pass cuts
>25%, either the corpus drifted badly or the bar slipped toward cutting genuine
overrides; re-check the KEEP list.
