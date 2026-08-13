# F12 — Tremolo: deep-research to validate / sharpen the three voices

Status: research-first prompt. Branch: `feature/tremolo-research` (off latest `dev`).
Do not commit to `dev`/`main`. Merge to `dev` only after acceptance criteria + tests/docs/changelog.

The POST **Tremolo** already ships three voices (OPTICAL / BIAS / HARMONIC) with
RATE / DEPTH / SHAPE / MIX, tempo sync, and a HARMONIC X-OVER knob (see changelog
06/26/2026). It was modelled from published descriptions, not from measured
references. This story is to give it the same measured-reference rigor the
transpose engine got: confirm the three voices are honest emulations, fix any that
aren't, and only add controls the research proves earn their place.

## How to run this (mandatory)

Follow the local research process. References, measurement scripts, rendered
WAVs and prototypes are working material and stay in local scratch. Satisfy the
anti-shortcut
checklist (>= 6 primary sources actually read, reference measured before
designing, per-candidate spec with falsifiable metrics, A/B spike vs. reference)
before touching production code.

## Reference targets

- Any owned amp/plugin with a real tremolo, or a hardware unit captured to WAV.
  Ask the user which tremolos they own / can install before assuming. Measure
  black-box from rendered audio and analyze offline.
- Classic references to characterize against: optical (photocell, choppy, e.g.
  Fender brownface/blackface), bias tremolo (smooth, tube-bias, "Bang Bang"),
  harmonic tremolo (band-split anti-phase, brownface). Get measured AM depth,
  waveform shape, and band behavior — not adjectives.

## Research questions to answer with sources + numbers

- **Optical**: real optocell response is asymmetric and non-sinusoidal (slow
  attack / fast release of the LDR). Measure the depth-vs-shape curve of a real
  optical tremolo; is VoLum's "choppy photocell gate" matching that envelope or
  just a steep sine? Quantify.
- **Bias**: confirm the smooth symmetric sine voice and its depth law (perceived
  loudness modulation is not linear in gain). Should DEPTH be perceptual/dB-shaped?
- **Harmonic**: verify the crossover band-split + anti-phase modulation against a
  measured brownface harmonic tremolo (phase relationship, crossover freq region,
  how much the two bands overlap). Is a single X-OVER knob the right control, and
  what default freq does the reference imply?
- **SHAPE / LFO**: is morphing sine→square the right axis, or do references use a
  different skew (ramp/log)? Tie to measurement.
- **Stereo**: references are often mono; confirm VoLum's phase-linked L/R is the
  right call or whether a stereo-phase option is worth it.

## Deliverables

- Local research notes extended with a tremolo section:
  correction framing, per-voice comparison table (measured AM depth / shape /
  band behavior), copyable building blocks with source + license, per-voice spec
  with falsifiable metrics, and reference-measurement results.
- A spike that drives a steady guitar-like input through each candidate voice and
  measures AM depth, modulation-rate accuracy, waveform shape, and (harmonic)
  band phase, A/B vs. the measured reference, rendering WAVs for the ear check.
- A short writeup: which of the three current voices are already faithful, which
  need a fix, and any proposed change — with metrics + WAVs + your pick.

## If/when changes are chosen (implementation)

- Adjust the tremolo DSP in its existing POST file (see AGENTS.md UI file map;
  tremolo lives in the triptych/POST code). Respect RT-safety.
- Re-report latency only if a voice introduces any (most tremolos are zero-latency).
- No new param unless research proves it. If added, append to `EParams` (never
  reorder), wire the full per-amp/preset/user-settings/POST-lock/id-tail
  round-trip, and pin it in the state tests + keyboard step tests.

## Tests (mandatory)

- Tremolo DSP doctests: AM depth bounds per voice, no NaN/Inf, bounded output,
  MIX=0 = passthrough identity, tempo-sync division mapping, HARMONIC band-split
  finite + anti-phase, SHAPE endpoints (sine/square) finite.
- State tests if any param is added.

## Docs / changelog

- Update `docs/user-guide.en.md` + `docs/user-guide.de.md` tremolo paragraph and
  refresh the relevant POST screenshot if the visible UI changes.
- One changelog line in `NeuralAmpModeler/installer/changelog.txt`.

## Out of scope (v1)

- New voices beyond the three. Note candidates (e.g. true pitch-vibrato) as
  follow-ups only if the research says they matter.
