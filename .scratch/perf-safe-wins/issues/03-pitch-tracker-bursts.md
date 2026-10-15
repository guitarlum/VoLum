# 03 Pitch tracker / WSOLA lag-blocking, shared Octaver tracker + burst tests

Status: resolved
Blocked by: 01
Report: P1 (own-01, own-02, own-03, own-13). Measured worst blocks at 64 frames: Octaver 3.2-9 ms,
INSTANT 1.5-5.1 ms, POLY 0.7-2.1 ms, one burst per 480 samples (`bench-summary.md`). This is a crackle risk,
not only CPU.

## Paths
- `NeuralAmpModeler/VoLumPitchShifter.h` (`_UpdatePeriod` ~L595-640, `_WsolaRefineRange` ~L473-520,
  `_ReadAtDelay` ~L402-416, `mWrite` wrap ~L307, `VoLumPitch::Process` dry ring ~L829-834, voices ~L846-851)
- `NeuralAmpModeler/tests/test_volum_pitch_artifacts.cpp` (extend) and/or a new burst test file

## Change
1. `_UpdatePeriod`: compute 4 (or 8) lags per pass over k, one accumulator per lag in today's k order;
   `best` chosen in ascending lag order; store r per lag in scratch (sized in `Configure`) and read `rm`/`rp`
   from it instead of recomputing; fill `mPeriodScratch` by direct index copy (integer delays, frac 0).
2. `_WsolaRefineRange`: same lag-blocking for `dot` and `sn`, results into `mCorrScratch`, argmax in lag
   order; `_WsolaPick` untouched.
3. Replace runtime `% size` in `_ReadAtDelay` / `mWrite` / dry ring with compare-and-wrap (`>=`), keeping
   identical integer indices.
4. Octaver shared tracker: when both voices are in lockstep (same `mWriteCount`, same countdown, same ring
   content by construction from Reset), compute the period once per update point and hand it to both voices
   at the same sample. If they are **not** in lockstep (e.g. after a live Transpose->Octaver switch, where
   voice 1 resumes on a stale ring), fall back to per-voice tracking until the next Reset. This keeps output
   bit-identical in every state; do not Reset voices on mode change.

## Acceptance
- Bit-identity: nested-loop oracles next to the new kernels (the existing "extract-once WSOLA picks the same
  lag as nested reads" pattern) with exact equality of `mPeriod` and the chosen lag over long renders; full
  Octaver / INSTANT / POLY renders equal to the pre-change output (exact lock); golden "pitch modes and
  characters" and all artifact metrics unchanged.
- Speed check (ratio): worst tracker burst <= 0.35x the reference tracker on the same machine, for Octaver
  and INSTANT; POLY WSOLA burst <= 0.5x.
- Local-only absolute: Octaver and INSTANT worst block < 1333 us at 64 frames @ 48 kHz on the dev box.
  If lag-blocking and sharing cannot reach it, record numbers and escalate: the remaining option (amortizing
  the tracker over callbacks, report P1b) is Needs listen.
- Reported latency unchanged (`Latency()` / `LatencyFor`).

## Proof
Oracles red against a perturbed kernel (lag off by one); ratio checks red against the old kernels.

## Verifier conditions
Sum order per lag unchanged; lockstep detection covers Reset, SetCharacter, sample-rate change and live mode
switch; no allocation on the audio thread; `_WsolaPick` and latency code untouched.

## Result
Commit 60f43550 (amended from 8a288a7d after verify03).
`VoLumPitchShifter.h`: `_EstimatePeriod` fills the scratch by direct index (`b[i0]*1 + b[i1]*0`, so -0 and
non-finite match `_ReadAtDelay`), runs `_LagCorrelations` (SSE2: 8 lags / 4 `__m128d` accumulators per pass,
wider spilled under MSVC; scalar 8-lag path with one `+= a*b` per lag elsewhere) into `mLagCorr`, argmax in
lag order, `rm`/`rp` read from it. `_WsolaScan` + `_WindowCorrelations`: 4 lags per pass on SSE2 with the
squares precomputed in `mCandSq` (same products, x86 does not contract), 8 lags scalar elsewhere; skipped
prefix, argmax in lag order; `_WsolaPick`, timing, latency untouched. `%` replaced by compare-and-wrap in
`_ReadAtDelay` (keeps `%` for a non-finite delay), `mWrite`, dry ring. Octaver: voice 0 logs its period
estimates per block (`mPeriodLog`, sized in `_AllocateScratch`), voice 1 replays them while `mVoicesInStep`
(set by Reset / reconfiguring Configure, cleared by any Transpose block) and `TrackerMatches` (write
position, count, countdown, period, sizes) hold; otherwise each voice tracks alone. Verbatim pre-change
kernels kept as `_EstimatePeriodReference` / `_WsolaScanReference` behind a test-only reference mode.
New `tests/test_volum_pitch_burst.cpp` (vcxproj + CMake): 10 pedal cases + a live mode/character/Reset/
sample-rate switch render equal bit for bit to the reference-kernel render (all OS) and to SHA-256 pinned
from the pre-change build (Windows; macOS prints its hash, pin it from CI later); tracker oracle (period
bits + best lag, 44.1/48/96 kHz, voiced/silent/noise) and splice-scan oracle (pick + every lag's
correlation bits, POLY/DROP, -12/-5/+7, both ranges) at many states; latency pins; timing cases (median of
>= 20 burst blocks, ratio vs reference kernels, local-only 1333 us worst).
- Red proofs: tracker load `p+k+6`->`p+k+7` and scan load `c+j+2`->`c+j+1` -> both oracles, pedal renders,
  golden pitch renders and the extract-once test fail (`evidence/03/red-p1.log`). Always sharing the tracker
  -> "mode switches" differs at sample 29641; live path on the old kernels -> ratios 0.51 / 1.02 / 0.95 /
  1.02 (`red-p2p3.log`). Reverted, green.
- Numbers (Core Ultra 7 155H, default scheduling, full suite run): Octaver burst block median 270 us, worst
  650 us vs old 2449 us (0.11); INSTANT 270 / 560 us vs 1311 us (0.21); POLY splice block 233-246 / 336-564
  us vs 624-642 us (0.37-0.38). Scratch kbench pinned to an LP-E core: tracker 1421 us vs 8046 us, still over
  64 frames there.
- Verify03 fixes: new case "Octaver replays several period updates per host block exactly" (48k/2048,
  44.1k/1024, 48k cycling 37/480/481/2048/64; live shared vs reference, bit-equal). Red with replay forced to
  `values[0]`: first diff at samples 3097 / 2846 / 3097, every other test still green (`red-replay.log`);
  reverted, green. Changelog headline now "much less likely to crackle".
- Full suite exit 0 (1139 cases after verify03, `full-suite-amend.log`); ASan -Filter Pitch green (46); app target builds. An incremental LTCG test
  build once produced an ICE and once a SIGSEGV from a stale object; deleting `build-win/tests/x64/Release/int`
  fixed both.
- Follow-ups: P1b amortization only if LP-E cores matter (Needs listen); `VoLumPitchShifter.h` is ~1340 lines,
  split the kernels into their own header; pin the macOS hash from CI.