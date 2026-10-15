# 06 NAM constant-input skip (NeuralAmpModelerCore hook + VoLum wrapper)

Status: resolved
Blocked by: 05
Report: P6a (nam-06). Helps idle DAW tracks (transport stopped, unarmed tracks, gaps between clips): today
every NAM keeps running at full cost on exact-zero input.

## Paths
- Submodule `NeuralAmpModelerCore/NAM/dsp.h` (+ the A2 / WaveNet model and the container), fork branch
  `volum/perf-safe-wins`
- `NeuralAmpModeler/VoLumResamplingNam.h` (wrap `mEncapsulated->process`, ~L89-95)
- `NeuralAmpModeler/tests/test_volum_nam_exact.cpp` or a new `test_volum_nam_idle_skip.cpp`

## Change
- NAMCore hook (`// VoLum:`): a virtual on `nam::DSP` that returns the receptive field in samples when the
  model is feed-forward (A2 fast / WaveNet), and 0 (= never skip) otherwise (LSTM, unknown). Containers
  forward to the active submodel.
- Wrapper: track how many consecutive **encapsulated-input** samples are bit-equal to one constant value c
  (not only zero: a PRE NAM in front feeds the amp a constant DC value, not zeros). When a whole block is
  bit-equal to c and the run is >= receptive field + max block size, skip `process` and fill the output with
  the model's last output sample (which is then exactly constant). Process normally as soon as any sample in
  the block differs. Reset the run on `Reset` / `SetSlimmableSize` / model swap. On the resampling path only
  the model is skipped; the resampler keeps running.
- No allocation, lock or cross-thread state on the audio thread.

## Acceptance
- Exact-compare doctest: signal -> long exact silence -> constant DC -> signal -> silence, skip-enabled vs
  always-process wrapper: identical output bytes, FULL and LITE, 48 kHz direct and a 44.1 kHz resampling case.
- A counting spy shows `process` is not called during the long constant stretch.
- Exact lock from 04 and all goldens green.

## Proof
Spy assertion red against the old wrapper; exact-compare red against a perturbed skip (resume one block late).

## Verifier conditions
Feed-forward-only (LSTM returns 0); run counter survives chunking (`ProcessNamInChunks`); the constant
output equals what processing would produce (steady state reached after RF); nothing added to the audio
thread except integer/compare work.

## Result
- NAMCore 9c4d8ad (fork `volum/perf-safe-wins`): virtual `DSP::FeedForwardReceptiveField()` (default 0). A2 fast
  returns `_prewarm_samples + 1` (6347 at 48 kHz, Full and Lite); the container forwards to the active submodel.
  Generic WaveNet stays 0: an exact-compare case on `wavenet_a1_standard.nam` (varied blocks) failed at the first
  skip, because its Eigen products round a constant input differently per block size. LSTM stays 0.
- `VoLumResamplingNam.h`: `ProcessEncapsulated` wraps every model call (direct and inside the resampler, so the
  Lanczos pair keeps running). A block is skipped only when every encapsulated-input sample has the bits of the run
  constant c (finite; NaN/Inf never start a run, +0/-0 distinct), the samples of c already fed are >= RF + max
  encapsulated block, and the last processed block's output was bit-constant. The output is filled with that last
  output sample. The run survives `ProcessNamInChunks` chunks and resets on Reset, prewarm and SetSlimmableSize
  (a model swap is a new wrapper). Audio thread: integer compares only.
- Tests: `test_volum_nam_idle_skip.cpp` (new, both registrations). Exact bytes vs always-process through signal,
  1 s silence, 1 s DC, signal, silence: Soldano FULL/LITE 48k/64 and 48k/400 varied, Myth PRE 48k/128, 44.1 kHz
  resampled FULL/64 and LITE/256 varied; PRE output (DC) into the amp; Reset and slice change mid-run; LSTM and
  A1 WaveNet always process; hook values.
- Red proofs: skip disabled (old wrapper) -> spy checks fail (35968 model frames in the silence window, expected 0);
  resume one block late -> exact compare fails in all 9 renders; SetSlimmableSize without run reset -> slice test fails.
- Full `run-tests-win.ps1` exit 0 (1149 passed, 1 skipped = the timing case), goldens and the 04 exact lock unchanged.
- 10 s exact silence through the wrapper, 48k/64 (doctest `--no-skip`): FULL 1059.6 ms -> 20.5 ms, LITE
  319.8 ms -> 10.2 ms (6464 of 480000 model frames processed).