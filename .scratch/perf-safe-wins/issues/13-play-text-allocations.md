# 13 PLAY text fit memo and per-frame allocations

Status: resolved
Blocked by: 12
Report: U7 (art-03, art-06). Every PLAY frame trims long rail names by measuring the text once per removed
character, plus a few dozen heap allocations per frame.

## Paths
- `NeuralAmpModeler/VoLumPlaySurface.h` (`clipped` lambda ~L1281-1295, `TwoDigits` ~L1818-1823, title /
  secondary copies ~L1070-1077, Dual role string ~L1050)
- `NeuralAmpModeler/VoLumColorHelpers.h` (`FitTextToWidth` ~L187-210, only if a memo helper lives there)
- `NeuralAmpModeler/VoLumTriptychMotifs.h` (reverb motif `pts` reserve ~L700/L723; helix vectors ~L525)

## Change
Memoize fitted strings per rail row keyed by (text, font, size, width, draw scale), recomputed when `SetData`
changes a name or the layout width changes; `TwoDigits` from a static 0-127 table or a stack buffer;
reference member strings instead of copying; `reserve` the reverb motif points and hoist the helix vectors.

## Acceptance
- PNG diff vs baseline = 0 changed pixels in PLAY (with long preset names on the rail; use the seeded sandbox
  and rename a preset if needed) and in BUILD pedal cards.
- Allocation count per PLAY frame drops (debug allocation hook or ETW heap trace; log numbers).

## Proof
Unit test for the memo (same inputs -> no re-measure; changed width -> re-measure), red without the memo.

## Verifier conditions
Memo invalidated on rescale, font change and name change; identical strings reach `DrawText`.

## Result (2026-09-29, round 2)
- Pure `VoLumTextFit.h` (trim loop + one-entry memo keyed by text, font, size, align, angle, width,
  `GetTotalScale()`); `FitTextToWidth` wraps it; PLAY rail rows use one memo per line, reset in `OnRescale`.
  `TwoDigits` on the stack, banner title/secondary by reference, Dual role labels in member buffers,
  helix strands in `std::array`, reverb `pts.reserve`. New pure header is outside the listed paths (testability).
- Red: memo forced to re-measure -> "same inputs" case 2727 vs 27 measures; old DrawSlot call -> source pin red
  (`evidence/13/red-*.log`). Green: suite 1189 passed, 1 skipped (`evidence/13/suite-green.log`).
- Pixels: 13 states vs `baseline-r2/` 0 changed (masks: meters, tuner readout; PLAY art pinned). `p04`/`t02`
  show a launch-to-launch 1-LSB stage variant also seen between two HEAD captures; recaptures match 0.
- Probe (temporary, not committed): PLAY frame 80 -> 5 heap allocations; draw submit ~2.35 -> ~2.3 ms (noise).

## Parked (owner, 2026-09-26 22:00)
Budget: lean plan. Not in this loop; stays specified for a later effort.
