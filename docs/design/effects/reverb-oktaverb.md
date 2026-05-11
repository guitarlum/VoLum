# Oktaverb

Oktaverb keeps the reworked pitch-bloom idea, with three distinct sub-modes that now match the shipped UI labels.

Visible controls:

- `Mix`, `Decay`, `Tone`, `Pre-Dly`, `Intensity`.
- Sub-mode pill: `HALO`, `SHIMMER`, `BLOOM`.

Mode behavior:

- `HALO`: octave-up and octave-down voices in the feedback loop for a dense dual-pitch wash with body.
- `SHIMMER`: octave-up feedback for a bright climbing tail.
- `BLOOM`: slow wet fade-in for pad-like swells.

The pitch voice uses a pitched pre-delay and small per-line detune motion so the shimmer blooms after the reverb onset instead of sitting statically on top. Switching Oktaverb sub-mode resets the reverb tail so the previous voice does not bleed into the next one.
