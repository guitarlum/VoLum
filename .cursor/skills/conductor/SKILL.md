---
name: conductor
description: Drive implementation until a checkable exit predicate is proven. Use when the user invokes /conductor, asks to loop until a goal, keep going overnight/AFK, finish a spec directory, or dispatch a clear wayfinder map (one worker per spec). Not for foggy product planning (/wayfinder), not for a one-step edit, and not for an ordinary single-slice feature or bug the user is sitting with.
disable-model-invocation: true
---

# Conductor (goal + verify until done)

Implementation loop. Write a checkable **predicate** before the first edit, keep going, do not relax it.

**Skip this skill** when the user is here for one slice (a bug with a failing test, a small feature they will review live). Use Plan mode or implement under `vo-lum-workflow`. Foggy multi-session headlines stay `/wayfinder` until the map is clear. Invoke `/conductor` for AFK / overnight, a `loop.md` that must survive compression, spec overnight, or release dispatch.

Do not install pstack and do not replace Cursor’s Task tool, `/loop`, or cloud agents. This skill is standing orders those already run: predicate on disk, VoLum escalate, verifier, spawn order. Do not invent a second orchestrator, queue files, or Graphite.

## First tool call every turn

Including `/loop` wakes and after compression. Chat memory of the goal and predicate is stale.

1. `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort <slug>`
   (`-Path` when the `loop.md` is not under `.scratch/`). Several loops and no `-Effort`/`-Path` is an error — do not guess.
2. Do not trust a conversation summary for Goal, Predicate, Claimed, Verifier, or Ledger.

If the script exits 1, **stop coding** and settle Goal/Predicate (and `-Effort`) on disk first. Dispatch: add `-RequirePredicate` before spawning any worker.

Template: `docs/agents/issue-tracker.md` (Conductor operations). Attachment when a `.scratch` file is open: `.cursor/rules/volum-scratch-planning.mdc`.

## Three entry modes

Pick one. Do not mix a 13-feature dump into spec overnight.

**Simple goal** — AFK bugfix or polish that needs a durable predicate. Skip wayfinder. Write only `loop.md`. This chat (or one worker + verifier) until the predicate.

**Spec overnight** — one `.scratch/<feature>/spec.md` plus `issues/`. Grain: **that directory only** until tickets + predicate. Claim one ticket before editing it. Do not reopen locked map/spec product calls.

**Release dispatch** — the wayfinder map is clear (`Not yet specified` empty) and the user wants the whole book overnight. **This chat does not implement.** It writes missing `spec.md` / tickets / per-spec `loop.md`, then uses Cursor **Task** (and `/loop` as wake) to run one spec overnight per headline. See **Release dispatch** below.

Never put `ready-for-agent` tickets next to a `map.md`. Specs use the `/to-spec` + `/to-tickets` shape (one file per ticket).

## Predicate

A predicate is commands that can fail. Prefer a per-effort `.scratch/<slug>/verify.ps1`. If the predicate is missing or vague, use **AskQuestion** (recommended option first, labeled `(Recommended)`). Do not invent one.

Not a predicate: “UAT feels good”, “it compiles”, “looks right”, retuning a golden hash.

### Bundles

Pick one and put the commands in `loop.md` **Predicate**:

- **Bug** — new or existing regression test proven red with the fix reverted, then `NeuralAmpModeler/scripts/run-tests-win.ps1` exit 0. (Red-then-green is already `vo-lum-workflow`; do not restate it as a second rule.)
- **UI** — `.scratch/<slug>/verify.ps1` plus `NeuralAmpModeler/scripts/run-app-win.ps1` and/or a named recipe in `docs/screenshot-recipes.md`.
- **DSP** — `NeuralAmpModeler/scripts/run-tests-win.ps1` including golden DSP / rig / pitch-artifact cases. Escalate instead of retuning a hash.
- **Packaging** — the dist script for this change (`NeuralAmpModeler/scripts/makedist-win.bat` / `NeuralAmpModeler/scripts/makedist-mac.sh`) and/or `NeuralAmpModeler/scripts/ci-watch.ps1`.
- **Sound-changing** — the DSP bundle **and** owner listen. Do not close the loop on CI green. Escalate.

Audio: existing goldens may stay green without you. Updating a golden, adding a voice, or shipping new sound **escalates**. `NeuralAmpModeler/scripts/pitch-ab-render-win.ps1` is ears-only, not CI.

## loop.md is SSOT

Path: `.scratch/<slug>/loop.md`. Sections: Goal, Predicate, Wake prompt, Verifier, Children, Ledger.

- Copy **Wake prompt** verbatim into Cursor `/loop`. `/loop` is not the process; `NeuralAmpModeler/scripts/ci-watch.ps1` already watches CI.
- **Verifier** is current state (hash + verdict), not a ledger. After a cold review, write `hash:` from `worktree_hash:` in the status output and `verdict: accepted` or `defects`. A later edit that changes `git diff HEAD` makes the status script print `STALE` — run the verifier again. Do not treat an old accepted verdict as live.
- **Children** lists backtick paths to child `loop.md` files. Dispatch aggregates those, not every leftover loop on disk.
- **Ledger** is append-only. One row per iteration: timestamp, what changed, predicate moved yes/no. Never rewrite rows from memory.
- A `-Filter` run is not the effort predicate. Mark that row `predicate=partial` until the Predicate command in `loop.md` (usually `.scratch/<slug>/verify.ps1`) exits 0. Name a test as passed only when that test name appears in the command output.
- Local machine, not a cloud agent: `VoLum.exe`, screenshots, locked-exe copies.

## Roles (coordinator vs spawn)

The wrong default is a swarm. The right default is: this chat stays small, the writer does not certify its own work.

- **Coordinator (this chat).** Owns `loop.md`, claims tickets, writes briefs, runs the status script, escalates. It may implement a **small** unit itself (one file, a tight bugfix, anything that needs the live `VoLum.exe` already in this session). It must not drown in a whole feature’s file reads.
- **Worker.** Spawn for a real implementation unit: a spec ticket, or a simple-goal slice that would flood this context. Fresh window. Brief from disk, not from chat memory (ticket + `loop.md` Goal/Predicate + paths it may write + the exact verify command + escalate rules). One writer per path set. **Local** if it builds, drives `VoLum.exe`, or takes screenshots. Cloud only for read-only or pure-text work that does not need this machine.
- **Verifier.** Mandatory before you treat a unit as done or the loop predicate as true. Different spawn than the worker, **read-only**, cold window. Payload: the diff, the predicate command output, and the status script’s `worktree_hash` — not the worker’s summary. For C++ / maintainability, attach the global `/thermo-nuclear-code-quality-review` skill. For diff-level defects, a cold Task (`bugbot` subagent, or a different model family). It reports defects; it does not edit. A “looks good” from the writer in this chat is not a review. Then write `## Verifier` on disk.

**Do not spawn** when the unit is a few dozen lines, when two tasks would write the same files (`NeuralAmpModeler.cpp`, one overlay header), or as a ritual before every tiny step (test-then-fix on one ticket is one unit, not two workers). Do not require a worker/verifier pair for micro-edits; the cold verifier is for predicate-bearing acceptance boundaries.

**Parallel workers** only for `ready-for-agent` tickets whose allowed paths do not overlap. Otherwise serial: one claimed ticket, one writer.

## Release dispatch

For “wayfinder the minor, then I’m going to bed.” The 1.3.0 bugs were missing predicates, missing verifiers, and grilling UAT in the same window — not “too many features in one night.” Quality still needs a cold writer and a cold reviewer **per spec**. Cursor already spawns those; you do not need 13 tabs if this chat only dispatches.

1. Confirm the map is clear. If fog remains, stop and say so — that is `/wayfinder`, not this.
2. One `.scratch/<feature>/` per headline (not inside the map folder). Each gets `spec.md`, `issues/`, and `loop.md` with a checkable predicate. Parent: `.scratch/<release-slug>/loop.md` whose Goal is “every child predicate true” and whose **Children** lists each child `loop.md`.
3. **Order.** Specs that write the same plugin files (`NeuralAmpModeler.cpp`, shared params/chunks/overlay headers) are a **serial stack**: one worker, verifier, next. Docs-only or disjoint paths may run in parallel. Default to serial for VoLum C++ headlines; parallel is the exception. Before the first spawn: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort <release-slug> -RequirePredicate`.
4. For each spec, spawn a **local** Task whose prompt is spec overnight on that directory (paste the worker brief + “follow `/conductor` spec overnight; do not open wayfinder”). Do not pull the worker’s source files into this chat. On completion, spawn a **verifier** on that spec’s diff + test output. Write that spec’s `## Verifier`. Append one parent-ledger row. Then the next spec.
5. Wake with `/loop` (or `NeuralAmpModeler/scripts/ci-watch.ps1` for CI). First tool call remains the status script with `-Effort`. Re-read child `loop.md` files from disk; do not trust the summary of 13 features.
6. Stop when every child predicate is true and a verifier accepted each spec (hash still matches). Owner UAT / listen still required for new sound. One `/captain-hindsight` at the end, not per worker.

If the user names a single spec or a bug, do **not** enter dispatch.

Worker brief (paste verbatim; skip a field only when the unit is so small you are not spawning):

```
GOAL         one sentence from loop.md / the ticket
SCOPE        paths it may write; paths it must not; exclusive branch or worktree
CONTEXT      pointers to spec, ticket, loop.md — paste upstream facts the child cannot see
ACCEPTANCE   checkable lines from the ticket
VERIFY       the Predicate command from loop.md. A -Filter subset is partial only.
FORBIDDEN    no force-push, no golden retune, no product calls, no fixes outside SCOPE
REPORT       status, paths, what you ran, verdict vs VERIFY, follow-ups
```

## Each iteration

1. Status script (above).
2. Escalate and **stop** on: a new product call not in the locked spec/map; new sound / golden retune; irreversible git (force-push, hard reset); a real dead end. Do not ping for reversible mechanics.
3. Pick implementer: this chat if the unit is small or needs the live exe here; otherwise spawn a **worker** with the brief above.
4. Run the **Predicate** command from `loop.md`. A `-Filter` subset may guide the edit; it does not close the loop. Then spawn a **verifier** on the actual diff + the Predicate output. If the verifier finds a real defect, it is not done — fix (same writer) and verify again. Do not skip the verifier because a subset passed. Write `## Verifier`.
5. If it advanced: append a ledger row; commit that unit if the user wants commits as you go.
6. If it did not: revert the attempt; ledger row with predicate=no; try another approach. Do not relax the predicate.
7. Repeat until every Predicate line passes, a verifier has accepted the last unit against the current `worktree_hash`, **and** (spec mode) no `ready-for-agent` / `claimed` tickets remain.

Then stop. Tell the user to run `/captain-hindsight` and implement only script/test winners. Do not auto-run the retro. The verifier is not a substitute for that retro.

## Do not

- Open `/wayfinder` because implementation is hard. If new fog appears, stop and say so.
- Open `/conductor` for a one-slice live bugfix the user is watching. That is Plan + `vo-lum-workflow`.
- Fold UAT grilling + coding + reviews into one marathon with no tickets and no `loop.md`.
- Spawn a worker per micro-step, or let the writer’s chat be the only review.
- Implement 13 headlines in this chat. That is dispatch, or 13 spec overnights — not spec overnight with a long list.
- Declare the map clear while `Not yet specified` still has contents (that is wayfinder, before this skill).
