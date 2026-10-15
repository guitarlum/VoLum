# 08 Meter senders: skip with no editor, wrap without division

Status: resolved
Blocked by: 01
Report: P11 (audio-04, audio-05). Every instance with a closed editor still computes three meter senders
and two peak scans per block; each sender does two integer divisions per sample.

## Paths
- `NeuralAmpModeler/NeuralAmpModeler.h` (`NAMSender`, ~L66-70; new `std::atomic<bool> mVolumEditorOpen`)
- `NeuralAmpModeler/NeuralAmpModeler.cpp` (`_UpdateMeters` ~L2594-2628, dual peak scan ~L707-714,
  `OnUIOpen` ~L1295, `OnUIClose` ~L1376)
- A VoLum-owned sender (new small header, e.g. `NeuralAmpModeler/VoLumPeakAvgSender.h`) if iPlug2's
  `IPeakAvgSender` cannot be adapted without editing iPlug2
- New test (e.g. `test_volum_meter_sender.cpp`)

## Change
- Set the atomic in `OnUIOpen` / clear it in `OnUIClose`. While false, skip `_UpdateMeters` and the dual
  peak scan (store `false` for the dual-hot flag). On the first audio block after a rising edge, Reset the
  senders (same size, no allocation) so a stale held peak never flashes. Keep the master-safety hold counter
  running.
- Sender: same window, RMS/peak and push logic as `IPeakAvgSender`, but increment-and-wrap with `>=`, never
  `==` (Reset can shrink the window without resetting the count) and no `%`. Keep the upstream quirk where
  `windowPos` is indexed by the block-relative sample, so the pushed data is identical.

## Acceptance
- Pushed `ISenderData` sequence identical to iPlug2's `IPeakAvgSender` over random input and block sizes,
  including a window shrink mid-stream.
- With the editor closed no sender work runs (unit-level: gate function; or a source pin if the gate lives
  in `ProcessBlock`).
- Meters in the running standalone behave as before (quick visual check under the lock).

## Proof
Sequence identity red against a perturbed wrap; gate test red against the old `ProcessBlock`.

## Verifier conditions
Atomic only (no lock) on the audio thread; reset happens on the audio thread after the edge, not from the UI
thread; no allocation.

## Result
Commit fd006ab9. New `NeuralAmpModeler/VoLumPeakAvgSender.h`: `volum::PeakAvgSender` (`NAMSender` base
now) copies `IPeakAvgSender` including the block-relative `windowPos` quirk; `windowPos` wraps by compare,
`mCount` wraps with `>=` then `-= mWindowSize` (rare `%` only if still past the window after a big
shrink), so the sequence equals upstream's `(mCount + 1) % mWindowSize` even after a shrink. `Restart()`
returns a sender to its constructed state without resizing. `volum::StepMeterGate` + `mVolumEditorOpen`
(release store in OnUIOpen/OnUIClose, acquire load on the audio thread) skip `_UpdateMeters` and the dual
peak scan (hot flag stores false) while closed; first block after an open calls `Restart()` on the three
senders on the audio thread. Master-safety hold untouched.
- Tests: `test_volum_meter_sender.cpp` (identity vs `iplug::IPeakAvgSender` over 7500 random blocks
  1..3000 frames with random Reset rates 22.05-96 kHz, 1 and 2 channels; three deterministic shrink cases;
  Restart == fresh sender; gate sequence; ProcessBlock/OnUIOpen/OnUIClose source pin).
- Red: wrap `mCount = 0` and wrap `== mWindowSize` both fail the random and shrink cases; old
  NeuralAmpModeler.cpp/.h fail the source pin. Logs in `evidence/08/`.
- Full suite on a clean test build from fd006ab9: 1157/1157 passed, exit 0.
- Bench (product codegen, 48 kHz, NAMSender settings): 6.17 -> 2.31 ns/sample per sender at 64-frame
  blocks, 6.03 -> 2.23 at 512 (ratio 0.37). Closed editor: sender work 0.
- Standalone (locked self-capture): BUILD OUT meter follows the metronome across successive shots.
