# F11 — Octaver: deep-research a copyable, high-confidence engine

Status: research-first prompt. Branch: `feature/octaver-engine` (off latest `dev`).
Do not commit to `dev`/`main`. Merge to `dev` only after acceptance criteria + tests/docs/changelog.

The PRE Pitch **Octaver** mode currently reuses the transpose granular voices at
fixed ±12 ratios (down/up) blended with dry, plus a VINTAGE/MODERN voicing pill.
It works, but it was never given the same measured-reference treatment the
transpose engine got. This story is to do that properly and ship the best octaver
we can defend with numbers — not to add knobs for their own sake.

## How to run this (mandatory)

Follow the local research process. Every reference, measurement script, rendered
WAV, and prototype is working material and stays in local scratch. Satisfy
the anti-shortcut checklist (>= 6 primary sources actually read, reference
measured before designing, per-candidate spec with falsifiable metrics, A/B spike
beats/matches reference) before writing any production code. "Googled 4 times" is
an explicit failure mode.

## Reference targets

- An owned octaver of the polyphonic-digital lineage, measured black-box from
  rendered audio. Availability is a local question; ask before assuming.

## Research questions to answer with sources + numbers

- Mono analog divider (flip-flop octave-down, gritty, chord-mush) vs. poly
  digital (FFT/organ-like, chord-safe): which lineage does VoLum's octaver want
  to emulate, and does the VINTAGE/MODERN pill map onto that split honestly?
- For octave-DOWN specifically: is a time-domain period-doubling / waveform
  re-synthesis cleaner than our granular ±12, and at what latency/cents cost?
- Tracking on chords vs. single notes: quantify how the reference degrades on a
  power chord (it will) and decide VoLum's honest scope (mono-accurate, chord-ok).
- VINTAGE grit: is the current waveshaper+lowpass defensible vs. a measured analog
  octaver's harmonic profile? Get the reference's THD/harmonic tilt, don't guess.

## Deliverables

- Local research notes extended with an octaver section: correction framing,
  comparison table, copyable building blocks (each with source + license),
  per-candidate spec with expected latency/cents/artifact numbers, and the
  reference measurement results.
- A local spike that A/Bs each candidate octaver vs. the measured reference on
  guitar-like test signals
  (additive tone with decay + pick transient, single notes AND a power chord)
  and renders WAVs for the ear check.
- A 2-3 option writeup for the human (metrics + WAVs + your pick), same format as
  the transpose options.

## If/when an engine is chosen (implementation)

- Promote the winner into `VoLumPitchShifter.h` (octaver path), keep the spike
  local. Respect RT-safety (no audio-thread alloc; pre-reserve in Configure/Reset;
  `ScrubNonFiniteInPlace`; existing `SoftSafetyClip`).
- Report per-engine latency to the host via `_UpdateLatency()` like DROP/FAST do
  if the chosen octaver changes the latency story.
- No new param unless the research proves it earns its place. If a param is added,
  append to `EParams` (never reorder), wire the full per-amp/preset/user-settings/
  PRE-lock/id-tail round-trip, and pin it in `test_eparam_order.cpp` +
  `test_volum_chunk_codec.cpp` + keyboard step tests.

## Tests (mandatory)

- `test_volum_pitch.cpp`: octave-down/up ratio correctness (dominant freq), no
  NaN/Inf, bounded output, dry-mix=passthrough identity, both VINTAGE/MODERN
  finite, sustain no-drift, and latency reported. Add a chord-input bound check.
- State tests if any param is added (see above).

## Docs / changelog

- Update `docs/user-guide.en.md` + `docs/user-guide.de.md` octaver paragraph and
  refresh `docs/user-guide-pitch-octaver.png` if the visible UI changes.
- One changelog line in `NeuralAmpModeler/installer/changelog.txt`.

## Out of scope (v1)

- Polyphonic Range/Lowest-note targeting, −2 octave voice. Note as
  follow-ups only if the research says they matter.
