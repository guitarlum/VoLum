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

## Acceptance

- All four gates green on the box and in CI, with the run id.
- Each gate is shown to fail on purpose:
  - a broken plugin for `validator`, pluginval and Carla (remove the factory
    export, or set a wrong reference);
  - a wrong render reference for REAPER.

## Milestone

When 05 to 08 are resolved, merge into `dev` (M2/M3).
