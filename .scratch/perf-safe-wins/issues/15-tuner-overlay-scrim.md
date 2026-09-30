# 15 Tuner overlay: panel-only repaint

Status: resolved
Blocked by: 14
Report: U9 (ui-08). With the tuner open, the whole window (BUILD or PLAY tree, then scrim, then panel) is
redrawn every idle tick, even when no pitch is detected.

## Paths
- `NeuralAmpModeler/VoLumTunerMetronomeOverlay.h` (full-window control, scrim ~L30-44, `SetResult`
  ~L130-136)
- `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp` (attach the scrim control just before the tuner)
- Every path that shows or hides the tuner (dismiss, overlay stack, UI-mode switch, halt), found by search

## Change
Move the translucent scrim into its own static full-window control attached directly before the tuner,
shown and hidden with it (the `VoLumNameDialog` `mScrim` pattern). Shrink the tuner control to its panel
rect (padded). Compare `TunerResult` before `SetDirty`. Mouse-down on the scrim dismisses exactly as today,
and Esc works while the pointer is over the scrim.

## Acceptance
- PNG diff vs baseline = 0 changed pixels with the tuner open over BUILD and over PLAY, and after closing it.
- `VOLUM_FRAME_PERF=0` with the tuner open: region size shrinks to the panel; no repaint when the result
  does not change.
- Every hide path hides the scrim too (test or source pin listing them).

## Proof
Test "unchanged result does not dirty" red against the old overlay; hide-path pin red when one path skips the
scrim.

## Verifier conditions
Z-order unchanged (scrim directly under the panel); modal behavior (clicks outside dismiss, keyboard) same.

## Result (2026-09-29, round 2)
- `VoLumTunerScrimControl` (static, mouse-transparent, same fill) attached directly before the tuner; the tuner
  rect is its panel padded 12 px, its target rect stays the full window (clicks outside the panel and Esc
  anywhere still reach it, as before). `Hide()` override hides/shows the scrim, so Show, `_Dismiss` (click, Esc,
  key-handler Dismiss) and `_ToggleVoLumTuner` all cover it. `SetResult` skips unchanged readings via the pure
  `VoLumTunerDirty.h` (new header, outside the listed paths, for the test).
- Red: old `SetResult` (no compare) -> gate case red; `_Dismiss` via `IControl::Hide` -> hide-path pin red
  (`evidence/15/red.log`). Green: suite 1195 passed, 1 skipped.
- Pixels: 13 states vs `baseline-r2/` 0 changed except `t02` (tuner over PLAY): 1-LSB stage variant, 2895 px,
  bit-identical to the variant already seen after ticket 13; one of two recaptures matches 0.
- Probe / `VOLUM_FRAME_PERF=0`, tuner open over BUILD, quiet room: dirty 200/200 ticks full window -> 0/200 after
  settling (8/200 panel-sized, 12.6% of window, right after opening); frames logged in 20 s 637 (4.4 ms region
  median) -> 0. Over PLAY: still full window every tick (PLAY `Tick`), tuner no longer adds a dirty control.

## Parked (owner, 2026-09-26 22:00)
Budget: lean plan. Not in this loop; stays specified for a later effort.
