# Digital Delay

Digital delay is the clean staging baseline.

Visible controls:

- `Time`, `Feedback`, `Mix`.
- `Tone`: wet-repeat brightness tilt.
- `Grit`: subtle bit depth/noise character on repeats.
- `Ping-Pong`: stereo cross-feed toggle for alternating repeats.

`Ping-Pong` is a toggle, not a separate mode. With stereo input it alternates repeats via opposite-channel feedback; with mono duplicated to both outputs (VoLum's normal path) it writes dry signal only into the right delay line so the first repeat lands on the right and later taps bounce L/R. Switching Delay mode or Ping-Pong clears the delay buffers so old tails do not bleed across modes.
