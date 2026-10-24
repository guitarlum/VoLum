# linux-support

## Goal

Native Linux x86_64 VoLum (VST3 with editor + standalone on JACK/ALSA/Pulse) per
`.scratch/linux-support/spec.md`. It is merged into `dev` with every Windows,
macOS and Linux CI job green, the box gates green including the Babyface loopback,
docs and changelog done, and the 1.4.0-beta notes drafted. The owner releases.

## Predicate

- `pwsh .scratch/linux-support/verify.ps1 -Final` exit 0, run from `C:\dev\VoLum-linux`. In order it checks:
  - the HEAD is pushed and the tree is clean;
  - every ticket in `issues/` is `resolved`;
  - clang-format is clean;
  - `run-tests-win.ps1` passes;
  - the box `verify-linux.sh --final` passes: GCC + Clang ASan/UBSan doctest,
    validator, pluginval 10, REAPER, Carla, JACK-dummy, e2e, Babyface gate, UI scenes;
  - the ci.yml run on that exact sha is green on every job, with macOS not
    skipped and the Linux tests/plugin/e2e/package jobs present.
- Every new regression test or gate was shown red with its fix reverted. The
  ticket report names it.
- `feature/linux-support` is merged into `dev` by `merge-verified-branch.ps1`.
- `## Verifier` shows `accepted` at the current `worktree_hash`.
- Without `-Final`, `verify.ps1` is the milestone predicate (gates that exist so far).

## Wake prompt

First tool call: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort linux-support` (from C:\dev\VoLum-linux).
Do not trust chat memory. Continue until every Predicate line passes.
Escalate and stop on new product, new sound / golden retune, irreversible git, or a real dead end.

## Standing orders (owner, 2026-10-24)

- **No time limit.** The loop runs until the Predicate holds, with no owner in the
  loop.
- **Escalation:** a blocker goes to `reports/escalations.md` and only that ticket
  becomes `Status: escalated`. Keep working the rest.
- **Stop the whole loop** only for irreversible git, box disk under 50 GB, or a
  change to Windows/macOS sound or behaviour.
- **Pre-authorized:** the Linux-specific golden hash set (null test ≤ -100 dBFS
  against Windows); installing packages on the box; pushing `volum/linux`
  (guitarlum/iPlug2) and `feature/linux-*` (guitarlum/VoLum); dispatching CI;
  milestone merges into `dev`.
- **Never:** `main`, or touching anything of localai-platform, llama-server, Ollama
  or docker on the box. `localai-guard.sh` gates every box job.
- **Shared `dev` (2026-10-24):** local `dev` is 4 commits ahead of `origin/dev`
  (scratch/agents docs: `a3b2124e`, `42bd5af4`, `d611a746`, `fb127c17`).
  `feature/linux-support` contains them. Before the M1 merge, push `dev` as is
  (no rewrite) so that `merge-verified-branch.ps1` sees local == origin. The
  uncommitted `.scratch/android-shootout` edits in `C:\dev\VoLum` belong to the
  owner: never commit, stash or revert them.
- **Cadence:** a `/loop` wake every 30 min. A wake is a no-op while workers run.
  Parallel workers go in one message; at most 4 builds at once (2 on the box).
- **pstack:** the plugin's skill files left the local cache on 2026-10-24. The
  swarm, arena and interrogate procedures used here are written out in spec.md
  ("How the loop runs").

## Verifier

- hash:
- verdict:

## Children

(none)

## Ledger

- 2026-10-24 05:10 | setup | grilled plan → spec.md, 15 tickets, verify.ps1, loop.md; worktree C:\dev\VoLum-linux on feature/linux-support (fb127c17), iPlug2 volum/linux at b98aacb44 pushed; conductor "second orchestrator" sentence replaced (pstack may drive a loop) | no
- 2026-10-24 05:20 | dispatch | 01 box-provision worker started (local, Grok 4.7, worktree C:\dev\VoLum-linux-01, branch feature/linux-01-box); 02 cloud worker refused: Cursor GitHub app not installed (E1). 02 goes to a local box worker after 01 | no
