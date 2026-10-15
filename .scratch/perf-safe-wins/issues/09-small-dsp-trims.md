# 09 Compressor / Tremolo / splice-diagnostic trims

Status: resolved
Blocked by: 03
Report: P12 (own-16, own-14, own-20). Small, bit-identical.

## Paths
- `NeuralAmpModeler/VoLumPreEffects.h` (compressor `fet` twice ~L156-160 / ~L199-201; `pow` at 0 dB ~L194)
- `NeuralAmpModeler/VoLumTremolo.h` (`gAnti` in every mode ~L169-170; `tanh(drive)` per call ~L252-253)
- `NeuralAmpModeler/VoLumPitchShifter.h` (`_SpliceCorr` from `_NoteSplice` ~L689-697)
- Tests touching those (compressor hash in `test_golden_dsp.cpp`, tremolo renders, pitch artifact tests)

## Change
- Compressor: keep the detector's `fet(in*drive)` per channel and reuse it in the wet path; `gain =
  (gainDb == 0.0) ? makeup : pow(10, gainDb/20) * makeup`.
- Tremolo: compute `gAnti` only in Harmonic; cache `tanh(drive)` when `mShape` changes and keep the
  **division** by it (not a reciprocal multiply).
- Pitch: compute `_SpliceCorr` only when a test/debug flag is on; the artifact tests that read
  `MeanSpliceCorr()` turn the flag on.

## Acceptance
- `compressor` hash unchanged; tremolo renders bit-equal to pre-change (add a direct sample-equality check);
  pitch artifact metrics unchanged with the flag on; audio output identical with the flag off.

## Proof
Tremolo equality red against a perturbed cache (reciprocal multiply); compressor hash red against a
perturbed makeup path.

## Verifier conditions
No behavior change when shape/mode change mid-stream; the diagnostic flag defaults off in the product.

## Result
Commit 5206d9ce (0c23a22f with the BOM dropped from the subject). Two workers ran this ticket in the same tree
at once; the committed product diff equals the worker's verified diff (evidence/09/real-change.diff).
- Compressor: detector `fet` kept per channel (`mDriven`, grows only with the channel count) and reused;
  `gain = (gainDb == 0.0) ? makeup : pow(...) * makeup`. `compressor` hash unchanged. New
  `VoLumCompressor output is bit-identical to the pre-trim compressor` (reference copy, mono + stereo,
  quiet/loud, 5 param sets mid-stream; asserts both gain branches ran).
- Tremolo: `gAnti` only in Harmonic; `drive` / `tanh(drive)` cached when `mShape` changes, division kept.
  New fixed-settings and mid-stream bit-equality tests against a reference copy (blocks 1/17/64/512,
  44.1/48/96 kHz, shape and mode change every few blocks).
- Pitch: `_SpliceCorr` behind `DebugSetMeasureSpliceCorr` (default off). The artifact harness turns it on and
  REQUIREs every splice to be measured; a new test checks flag off = 0 measured, bit-identical audio.
- Red proofs (evidence/09/red1-3): reciprocal multiply -> tremolo tests red; 1-ulp below-knee makeup ->
  compressor equality + `compressor` hash red; flag default on -> diagnostic test red; gAnti in Bias only ->
  Harmonic red; channel 0 FET for all channels -> stereo red; never measure -> harness REQUIRE red (0 == 40);
  stale shape cache -> mid-stream red at the first shape decrease. All reverted.
- Full suite on the committed tree: 1161/1161 passed, 1 skipped (evidence/09/final-full-0c23a22f.log).
- Cost (bench09ab: before and after in one binary, interleaved, product codegen, one Windows x64 laptop,
  48 kHz, 64 frames): compressor 0.74x below the knee / 0.89x compressing; tremolo stereo 0.70x at shape 0,
  0.51x Optical/Bias at shape 0.5, 0.78x Harmonic; POLY pitch 0.95-0.98x.
