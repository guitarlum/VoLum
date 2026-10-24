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
- 2026-10-24 05:40 | 01 resolved | worker PASS 571a6ef4; coordinator re-ran box.ps1 acceptance independently (exit 0, build1 lock, Ubuntu 26.04 gcc 15.2 / clang 21 / cmake 4.2, pluginval + reaper in tools); merged as 2670f896. 05 unblocked from 02 (disjoint iPlug2-only paths, own compile target); 02 + 05 dispatched as a 2-worker swarm (local, box) | no
- 2026-10-24 07:15 | 05 resolved | worker PASS: superproject f1d8126c (code 532ed7aa), volum/linux 068ac6a3; CI 40061394173 green on 532ed7aa (8/8 jobs incl. macOS + Windows); report-only commit after it. Coordinator review of the iPlug2 diff: changes in existing files sit in OS_LINUX branches except a wdlendian.h float-endian guard (noted); run-loop interface is process-wide with no lock, carried into ticket 07 as required work. Merged as c1903ae1. 06 dispatched | no
- 2026-10-24 07:25 | 02 resolved | worker PASS at d5ff0b35: GCC 1468/1468, Clang ASan+UBSan 1433/1433, Windows 1470/1470; loudest null residual vs Windows -236.2 dBFS; red proof shown. Coordinator review: Windows/macOS hash literals byte-identical (Linux pins appended as new trailing args); no tolerance widened. Considers: `ASAN_OPTIONS=alloc_dealloc_mismatch=0` on Linux only (libstdc++ stable_sort), Windows/macOS sanitizers still catch mismatches; ~13 MB of float64 null WAVs added. Merged as 7ee6a6b0; provision now installs libclang-rt-<major>-dev (4eae9031). Independent box verify + CI on 4eae9031 running. 03 + 04 dispatched (swarm, Grok 4.7) | no
- 2026-10-24 08:00 | 02 independently verified | coordinator box run at 4eae9031: verify-linux.sh rc=0, GCC 1468/1468, Clang ASan+UBSan 1433/1433, `linux gates: MET`. (First attempt's exit 0 was the grep pipeline's; rerun captured the script's rc.) CI 40065659611 on 4eae9031: Windows, installer upgrade, NAMCore, Coverage, Formatting green; macOS pending | no
- 2026-10-24 08:10 | 04 BLOCKED → coordinator decision D1 | CI 40067942125: ubuntu-22.04 g++-15 (PPA ~22~ppa2) differs from box g++-15 (26.04 archive) on 5 Linux pins (oktaverb x3, octaver vintage, mode switches: transcendental-heavy paths), Windows null still ≤ -100 dBFS; tone-stack perf ratio 0.506 > 0.5. D1: Linux exact hashes bind to ONE reference environment (CI ubuntu-22.04 = release build env), identified at runtime (glibc version + compiler `__VERSION__`); there they are required and CI fails if the identity drifts (`VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1`). Elsewhere (box, users) the Windows null test (≤ -100 dBFS, unchanged) is the gate and the hash check prints a skip message. Prefer jammy archive compilers over PPAs on CI (stable identity). Within the owner's pre-authorized Linux golden set; Windows/macOS untouched. Worker 04 resumed | no
- 2026-10-24 08:30 | wake | CI 40065659611 on 4eae9031 (02 + 05 merged) fully green incl. macOS, macOS installer upgrade, macOS standalone e2e: the Linux test changes do not affect Windows/macOS. Workers 03, 04, 06 running | no
