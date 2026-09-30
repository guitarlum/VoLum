# 11 Footer and mode toggle stop repainting every tick

Status: resolved
Blocked by: 01
Report: U5 (ui-01, bg-10). Measured: BUILD idle repaints 4 regions on every frame with nothing changing.

## Paths
- `NeuralAmpModeler/NeuralAmpModeler.cpp` (per-idle toggle `SetDirty(false)` ~L862-864; footer branch
  ~L1051-1089)
- `NeuralAmpModeler/VoLumCoreControls.h` (`VoLumFooterControl::SetStatus` ~L563-570)
- `NeuralAmpModeler/tests/test_volum_ui_regressions.cpp` (update any source lock that pins the removed line)

## Change
- `SetStatus`: `if (mText == text && mAlert == alert) return;` before assigning and dirtying.
- Delete the per-idle mode-toggle `SetDirty`. Its own state changes already compare before dirtying, and
  iPlug redraws it whenever anything beneath it repaints.
- Optional: build the footer strings only when an input changed (cached key of branch, file, support file,
  version); the update-footer countdown keeps ticking.

## Acceptance
- PNG diff vs baseline (ticket 01) = 0 changed pixels: BUILD idle, BUILD after a hover, PLAY, footer states
  (model loaded, dual amp, update reminder via `VOLUM_FAKE_UPDATE=1`, load failed if reachable).
- `VOLUM_FRAME_PERF=0` BUILD idle: region count drops by the footer and toggle regions (expect 4 -> 2 while
  meters still move on mic noise; 0 once ticket 12 lands with a quiet input).

## Proof
A unit/source test asserting `SetStatus` with identical text does not dirty, red against the old control.

## Verifier conditions
Nothing drawn above or below the footer/toggle relies on their per-tick dirty region; every footer text
change still reaches the screen (dual-amp text, update countdown, load error).
