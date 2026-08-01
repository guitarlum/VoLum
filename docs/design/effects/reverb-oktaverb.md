# Oktaverb

Oktaverb keeps the reworked pitch-bloom idea, with three distinct sub-modes that now match the shipped UI labels.

Visible controls:

- `Mix`, `Decay`, `Tone`, `Pre-Dly`, `Intensity`.
- Sub-mode pill: `HALO`, `SHIMMER`, `BLOOM`.

Mode behavior:

- `HALO`: octave-up and octave-down voices in the feedback loop for a dense dual-pitch wash with body.
- `SHIMMER`: octave-up feedback for a bright climbing tail.
- `BLOOM`: slow wet fade-in for pad-like swells.

The pitch voice uses small per-line detune motion so the shimmer blooms after the reverb onset instead of sitting statically on top. Switching Oktaverb sub-mode resets the reverb tail so the previous voice does not bleed into the next one.

## Structure

The same feedback delay network as Hall, and since 1.2.1 the same input stage: the
shared allpass diffuser and per-channel early field described in
[reverb-hall.md](reverb-hall.md). Oktaverb never had Hall's silent start - its pitch path
reads its grain buffer at a one-sample delay, so there was always something at the
output - but it did inject the undiffused input into all eight lines identically, which
is the other half of what made the reflections discrete.
