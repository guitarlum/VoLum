# 16 Model-loaded flag consumed with the editor closed

Status: resolved
Blocked by: 01
Report: B3 (bg-07) and side finding 2. With the editor closed, `mNewModelLoadedInDSP` is only cleared inside
`if (GetUI())`, so every idle tick rebuilds the filename (5-7 allocations) and wipes any load error before
the user can see it.

## Paths
- `NeuralAmpModeler/NeuralAmpModeler.cpp` (OnIdle ~L1092-1101; set on the audio thread ~L1991;
  `OnUIOpen` ~L1299-1302)
- `NeuralAmpModeler/NeuralAmpModeler.h` (flag ~L909; new main-thread bool)
- Headless OnIdle regression test

## Change
Take the flag once with `exchange(false)` **before** `_VolumReapAudioThreadRetirees()` (so the published path
that goes with it is committed by the reap). If it was set: recompute `mVolumLastLoadedFile`, clear the load
error once, and set a main-thread-only pending bool that the existing `if (GetUI())` block consumes.
`OnUIOpen` already calls `_UpdateControlsFromModel()` when a model exists.

## Acceptance
- Headless test: after a staged model, OnIdle with no UI consumes the flag, and a later load error survives
  until the editor opens.
- Footer text in the running standalone unchanged (PNG diff BUILD idle = 0).
- Changelog line mentions the fix (a load error while the editor is closed now shows when you open it).

## Proof
Classic red-then-green: the headless test fails on the old OnIdle (flag stays set / error wiped).

## Verifier conditions
Only reader of the flag is OnIdle; ordering vs the reap removes the stale-filename race; no audio-thread
change beyond the existing atomic store.
