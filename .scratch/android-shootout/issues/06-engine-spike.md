# 06 Engine spike

Status: blocked
Blocked by: 01
Branch: `feature/android-engine-spike` off `feature/android-shootout`
Worker: gpt-5.6-sol-high (with `.cursor/rules/neural-amp-modeler-native.mdc`)

## Goal

Answer with evidence: does the current-dev VoLum DSP build for Android, and does
it produce the same output as the desktop code? Nothing is wired into a UI.

## Scope

May write: `NeuralAmpModeler/projects/android/engine/**`,
`.scratch/android-shootout/reports/06-engine-spike.md`,
`.scratch/android-shootout/evidence/spike/**`.
Must not write: desktop sources, submodules, `core/`, `design/`, `ui-*/`, `app/`.
The box worktree needs `NeuralAmpModelerCore`, `AudioDSPTools` and `eigen`
submodules (public GitHub/GitLab URLs in `.gitmodules`); do not init `iPlug2`.

## Deliverables

- An Android library module (`engine/`, CMake + NDK from
  `~/volum-android/ndk-version.txt`) compiling the iPlug2-free VoLum DSP for
  `arm64-v8a` and `x86_64`: NAM core incl. A2 / slimmable, `AudioDSPTools`, and
  every VoLum DSP header the desktop processing chain uses
  (`VoLumProcessingPlan.h`, `VoLumProcessBlock.inc.cpp` call graph) that does not
  need iPlug2.
- A host (Linux x86_64 on the box) build of the same sources rendering a fixed
  input WAV through one bundled rig from `rigs/` with a fixed parameter set, and
  the same render via an instrumented test on the emulator. Null test: report
  peak and RMS difference in dBFS; pass means ≤ -90 dBFS RMS or a written root
  cause.
- A table of desktop DSP files: compiles on Android / needs iPlug2 (which symbol)
  / not attempted.
- Emulator real-time factor for that render, labelled "x86 emulator, not a phone".
- `evidence/spike/<sha>/spike.json`
  (`{"commit","ndk_build","host_build","null_test_db_rms","pass"}`).

## Acceptance

- `spike.json` `"pass": true` for the branch head, report written.
- Cold verifier (different family) checks the report's claims against the logs
  and writes `verdicts/spike.md` with `commit: <head sha>` and
  `verdict: pass | defects`. No golden in the desktop tests is touched.
- Pushed to GitHub.
