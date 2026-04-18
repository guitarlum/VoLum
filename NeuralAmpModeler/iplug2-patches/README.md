# iPlug2 patches

This folder contains patches that are applied to the `iPlug2` git submodule at
build time. The submodule itself stays at the upstream commit recorded in the
parent repo (no fork required).

## How it works

- Each `*.patch` file in this folder is a `git diff` against the iPlug2 working
  tree at the recorded submodule SHA.
- `apply-iplug2-patches.ps1` (Windows) and `apply-iplug2-patches.sh`
  (macOS / Linux / CI) iterate the patches in lexicographic order and apply
  each one with `git -C iPlug2 apply`. Both scripts are idempotent: if a patch
  is already applied they skip it instead of failing.
- The Windows entry points (`run-app-win.ps1`, `run-tests-win.ps1`,
  `makedist-win.bat`) and the Windows CI jobs invoke the apply script before
  building.

## Editing patches

Do **not** commit the modified `iPlug2/` working tree. Instead:

1. Make the changes inside `iPlug2/...` directly.
2. Regenerate the patch file from the parent repo root:

   ```pwsh
   git -C iPlug2 diff > NeuralAmpModeler/iplug2-patches/0001-app-host-route-selected-asio-channels.patch
   ```

3. Reset the iPlug2 working tree (`git -C iPlug2 checkout -- .`) and verify the
   apply script reproduces the change cleanly.

## Current patches

- `0001-app-host-route-selected-asio-channels.patch` — Makes the standalone
  audio host honor the Audio Settings input/output channel selection on
  Windows ASIO (and other RtAudio backends). Without it, multi-channel
  interfaces like the RME Babyface Pro FS effectively ignore the channel
  pickers and always use device input 1 / outputs 1+2.
