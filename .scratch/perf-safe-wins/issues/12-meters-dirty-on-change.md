# 12 Meters repaint only on a visible change

Status: resolved
Blocked by: 11
Report: U6 (ui-05). Meters dirty on every sender packet, even when the drawn value is clipped to the same
floor or the meter is covered by PLAY.

## Paths
- `NeuralAmpModeler/NeuralAmpModelerControls.h` (`NAMMeterControl` ~L674-723)
- `NeuralAmpModeler/VoLumPlayRuntime.inc.cpp` (`_VolumRefreshPlaySurface`: tell meters whether PLAY covers them)
- Tests: a unit test on the decode/compare helper

## Change
Override `OnMsgFromDelegate` in `NAMMeterControl`: decode like the base class, compute the clipped value the
draw uses (`GetValue` domain, -70..-0.01 dB), and `SetDirty` only if it differs from the current value or
the safety flag changed. Gate here, never in `SetDirty`. Add `SetCovered(bool)` driven from the PLAY refresh:
while covered, update values but never dirty. The return to BUILD already calls `SetAllControlsDirty`.

## Acceptance
- PNG diff vs baseline = 0 changed pixels in BUILD idle and after BUILD <-> PLAY round trips.
- `VOLUM_FRAME_PERF=0` BUILD with a quiet input (`VOLUM_PLAY_FAKE_PEAK` does not drive BUILD meters, so use a
  muted input or the sandbox with the mic muted): meter regions disappear once the level is below -70 dB.

## Proof
Helper test "same clipped value does not dirty" red against the base-class path.

## Verifier conditions
Meter pixels depend only on the clipped average and the safety flag (peak values are not drawn); covered
state cleared on every path back to BUILD.
