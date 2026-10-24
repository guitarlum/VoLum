Status: ready-for-agent
Blocked by: 07

# 08 Plugin gates (box + CI)

## Scope

May write:
- `NeuralAmpModeler/scripts/validate-vst3-linux.sh`
- `NeuralAmpModeler/scripts/smoke-reaper-linux.sh` and its `.lua`
- `NeuralAmpModeler/scripts/linux/**`
- `.github/workflows/ci.yml`: new job `linux-plugin`

## Work

All runs are headless under `xvfb-run -a`:
- Steinberg `validator`.
- pluginval at strictness 10.
- REAPER Lua smoke, mirroring `smoke-reaper-mac.sh`:
  - load the plugin and open its editor;
  - render 10 s of the fixture DI through the default rig and null it against a
    committed Linux reference;
  - run two instances and close.
- `carla-single` load, process and close.
- The CI job builds the VST3, runs all four gates, and uploads `VoLum-linux-vst3`.

## Carried from ticket 07 (coordinator, 2026-10-24)

- **Start from the arena harness:** `.scratch/linux-support/arena-07/a/`
  (`run-gates.sh`, `reaper-smoke.sh`/`.lua`, `redproof.sh`, `ubsan.supp`).
  Productize it into the scripts above; delete the scratch copies once moved.
- **Sanitizer host smoke is a gate too:** REAPER 2 instances × 20 and pluginval
  against the `VOLUM_SANITIZE` build. The only UBSan suppression is
  `shift-base:stb_image.h`, kept in the repo next to the script; never a
  global `-fno-sanitize`. Decide whether LeakSanitizer stays off for hosts and
  say why.
- `LD_PRELOAD=libasan.so` is required for the uninstrumented hosts (measured
  in 07); keep it in the script, not in CMake.
- **box.ps1 orphan builds:** a dead local SSH client left its remote job
  running, which later took the lock and built next to a retry. Tie the
  remote job to the session (for example `ssh -tt`, or a remote pid file the
  next run kills), and show it with a killed client.
- Add `gdb` to `provision-linux.sh`.
- **UI-thread call sites (07 cold verifier):** the doctest only covers the
  `IPlugLinuxIsUIThread` predicate. Make the gate prove the four
  `IPlugLinuxCheckUIThread` sites in `IGraphicsLinux.cpp` (OpenWindow,
  CloseWindow, ProcessX11Events, OnDisplayTimer) report an off-thread call,
  for example a Debug harness that calls one from a second thread and expects
  the count to rise; red with the check removed.

## Acceptance

- All four gates green on the box and in CI, with the run id.
- Each gate is shown to fail on purpose:
  - a broken plugin for `validator`, pluginval and Carla (remove the factory
    export, or set a wrong reference);
  - a wrong render reference for REAPER.

## Milestone

When 05 to 08 are resolved, merge into `dev` (M2/M3).
