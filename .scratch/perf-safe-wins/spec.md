# perf-safe-wins

Implement every **Safe win** from the perf sweep (`.scratch/perf-sweep/report.md`, cold-verified in
`.scratch/perf-sweep/verify.md`) as one ticket each, so VoLum uses less CPU with sound, looks, latency and
saved state unchanged. Mode: `/conductor` spec overnight on this directory only.

## Owner decisions (locked; do not reopen)

Perf sweep grilling (2026-09-24) and the conductor grilling (2026-09-26):

- Sound: golden renders pass **without regeneration**. If any golden or hash moves, stop and escalate.
  Never widen a tolerance.
- Looks: static frames pixel-identical, animation cadence unchanged. Skipping redraws when nothing visible
  changed is allowed.
- Latency and reported PDC never grow. Memory: at most a few MB per instance.
- SIMD: keep the SSE2 floor. No `/arch:AVX*`, no `/fp:fast`, no `-ffast-math`.
- Scope: every Safe-win row of the report, **including** the process-global model cache (bg-05) and the
  submodule tickets. Needs-listen, Needs-look, Upstream-only and Parked rows are out of scope.
- Settings-write debounce (B1): write after **500 ms** without a change, and at least every **2 s**
  during a long drag. Close / quit flushes stay synchronous.
- Burst tests (C2): pass/fail is a **machine-independent ratio** against the current algorithm kept in the
  test as a reference (new burst <= 0.35x reference unless the ticket says otherwise), plus an absolute
  64-frame-deadline check that runs **locally only** (skipped when `CI` or `GITHUB_ACTIONS` is set and
  under sanitizers).

## Where the work happens

- Main checkout `C:\dev\VoLum` on branch `feature/perf-safe-wins`, created 2026-09-26 from
  `fix/1.3.0-sweep-round1` at `a370773d` (owner, 2026-09-26: the other agent finished, no worktree needed).
  `dev` is 109 commits behind and lacks the 1.3.0 code these tickets touch.
- Standalone launches (`run-app-win.ps1`, `.ui-sandbox-launch.ps1`, `ui-drive.ps1`, e2e) kill every
  `VoLum.exe` on the machine: take the shared lock first in case another agent returns
  (`pwsh .scratch/1.3.0-feedback/buildlock.ps1 -Acquire -Owner perf-safe-wins`, `-Release` after).
- Submodule tickets (04, 05, 06 in NeuralAmpModelerCore; 07 in AudioDSPTools):
  - NeuralAmpModelerCore: branch `volum/perf-safe-wins` from the pinned commit `27027cc5` (fork branch
    `volum/a2-headscale-detector-test`).
  - AudioDSPTools: branch `volum/perf-safe-wins` from the pinned commit `446b8977` (fork branch
    `effect-staging-stereo-reverb`).
  - Commit inside the submodule first, push with the explicit HTTPS URL (never change git config):
    `git -C NeuralAmpModelerCore push https://github.com/guitarlum/NeuralAmpModelerCore.git volum/perf-safe-wins`
    (AudioDSPTools: `https://github.com/guitarlum/AudioDSPTools.git`). Then commit the parent pointer
    together with the VoLum-side tests and changelog line. Never force-push.
  - Mark VoLum-only code in these files with a `// VoLum:` comment so upstream syncs stay readable.

## Definition of done (every ticket)

1. Claim the ticket (`Status: claimed`) before editing; one writer at a time (serial stack: most tickets
   touch `NeuralAmpModeler.cpp`, `VoLumPlaySurface.h` or a submodule).
2. **Proof the test bites.** Perf tickets change no output, so the regression proof is:
   - an **exact-output lock** (bit equality against a reference kept in the test, or a per-OS hash in the
     `test_golden_dsp.cpp` pattern) shown **red** against a deliberately perturbed build (e.g. change one
     constant), then green on the real change; and
   - where the ticket names a **speed check**, it is shown red against the old code and green after.
   Bugfix parts (tickets 16, 17) get a classic red-then-green regression test.
   Log both proofs in the ledger. Revert the perturbation before committing.
3. `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` exit 0 in the worktree. New test files are registered
   in both `NeuralAmpModeler-Tests.vcxproj` and `tests/CMakeLists.txt`.
4. UI tickets (11-15): PNG diff against the baseline shots from ticket 01 shows 0 changed pixels in the
   named states, and the `VOLUM_FRAME_PERF=0` frame/region counts move as the ticket predicts.
5. One changelog line per ticket in `NeuralAmpModeler/installer/changelog.txt` (project style:
   `DD/MM/YYYY - VoLum 1.3.0 (perf - ...)`), plain words, what the player notices (smoother, less CPU).
   No user-guide change unless a ticket says so.
6. clang-format clean on changed C++ files. One commit per ticket (message via `git commit -F <file>`).
7. Cold verifier (a fresh read-only Task, never the writer) on the ticket's diff + test output, with the
   ticket's "Verifier conditions" pasted in. `defects` means fix and verify again.
8. Set `Status: resolved`, append the ledger row in `loop.md`.

## Escalate and stop

- Any golden reference, hash or tolerance would have to change.
- A ticket's Safe-win condition cannot be met (e.g. bit-identity fails), or the fix would need a
  Needs-listen / Needs-look change (report sections of the same names).
- A product call not settled above (new UI, new setting, changed wording).
- Force-push, history rewrite, or anything touching `main` / `dev`.

## Tickets (serial order)

| # | Ticket | Report ID | Area |
|---|---|---|---|
| 01 | Test build parity + UI/perf baselines | C1 | infra |
| 02 | Tuner YIN lane-parallel SIMD + burst test | P2 | DSP |
| 03 | Pitch tracker / WSOLA lag-blocking, shared Octaver tracker + burst tests | P1 | DSP |
| 04 | A2 LeakyReLU `cwiseMax` (NAMCore) + NAM exact-output lock | P4 | DSP, submodule |
| 05 | A2 ring tail-mirror copies only written columns (NAMCore) | P5 | DSP, submodule |
| 06 | NAM constant-input skip (NAMCore hook + wrapper) | P6a | DSP, submodule |
| 07 | Fixed-degree filter core (AudioDSPTools) | P8 | DSP, submodule |
| 08 | Meter senders: skip with no editor, wrap without division | P11 | DSP |
| 09 | Compressor / Tremolo / splice-diagnostic trims | P12 | DSP |
| 10 | Lanczos resamplers only when resampling | B4 | memory |
| 11 | Footer and mode toggle stop repainting every tick | U5 | UI |
| 12 | Meters repaint only on a visible change | U6 | UI |
| 13 | PLAY text fit memo and per-frame allocations | U7 | UI |
| 14 | PLAY refresh: compare before dirty, field-wise AmpSettingsEqual | U4 | UI / state |
| 15 | Tuner overlay: panel-only repaint | U9 | UI |
| 16 | Model-loaded flag consumed with the editor closed | B3 | background, bugfix |
| 17 | Debounced standalone settings writes | B1 | background |
| 18 | Loader prefetch without building models; cap to cache size | B2 | background |
| 19 | Process-global model data cache | B2 (bg-05) | background |

## Implementation Recommendation

- **Attach before coding:** the `conductor` skill (spec overnight), and `native-build-debugger` when a
  build or CI run fails. The glob rules (`neural-amp-modeler-native`, `volum-ui`, `volum-state-params`,
  `volum-submodules`) attach on the files they own.
- **Run after only if requested:** `/captain-hindsight` once the loop closes.
- **Why:** a long serial stack of small DSP, UI and background refactors whose only acceptance is "same
  output, less work", so every unit needs its own exact-output lock and a cold verifier.
