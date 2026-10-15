# Which chorus voice families fit VoLum POST?

Type: research
Status: resolved

## Question

What 2–3 chorus voice families should VoLum offer in POST, matching the existing Delay/Tremolo pattern (a mode picker, a short shared knob row, no extra pages)?

Use only public literature, expired-or-citable patents, and open-source implementations with a compatible license. Write about **signal-flow families** (for example: mono modulated delay, stereo quadrature, dimension-style fixed-phase), not products.

Deliver: 2–3 named VoLum-side voice options, the knobs each needs, CPU/latency notes, and which one should be the default. This answer is what [Where does Chorus sit in POST, and which voices?](08-chorus-placement-and-voices.md) will pick from.

Do not put third-party product or vendor names in this ticket’s answer.

## Answer

Use one shared five-knob row: **RATE · DEPTH · TONE · WIDTH · MIX**.

- **Clear** — Two smooth, phase-offset stereo delay taps, transparent interpolation, no feedback. Low CPU, no extra reported latency. **Default.**
- **Warm** — Two triangle-modulated taps with linear interpolation, wet low-pass and modest fixed feedback for darker motion. Low CPU, no extra reported latency.
- **Ensemble** — Three phase-spaced taps per channel from shared delay buffers for denser, less cyclic movement. Medium-low CPU, no extra reported latency.

**Default Clear:** preserves guitar transients and high end, stays cheap beside NAM, covers always-on use.

Public sources (signal-flow only): Dattorro, “Effect Design, Part 2: Delay-Line Modulation and Chorus,” *JAES* 45(10), 1997; Julius O. Smith, chorus chapter (CCRMA PASP); expired US4038898A (N voices at 360°/N spacing); expired US4384505A (three-delay ensemble); MIT-licensed multi-tap chorus in DaisySP.

**Cut from the card:** exposed feedback, base delay, waveform, voice count, phase, per-channel rates, extra filters, randomness, tempo sync.
