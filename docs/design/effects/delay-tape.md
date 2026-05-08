# Delay: Tape

## Target sound

Vintage tape echo with **wow, flutter, saturation, and progressive HF loss**.
Should sound clearly distinct from Digital (no chorus modulation - the motion
is irregular and lives on the read tap, not on a separate post-modulator) and
from Analog Memory-Man (no compander, no smooth chorus). Tape's character is
**chaotic, organic, and aging** - each repeat darker, slightly pitch-wobbly,
and gently saturated.

A 3-way sub-toggle picks the flavour:

- **Studio**: clean Echoplex-style tape, light wow, mild saturation. The
  most usable as an "always-on" delay.
- **Vintage**: Roland Space Echo / hot EP-3 territory. Stronger saturation,
  audible flutter, head-bump-style low-mid resonance.
- **Broken**: heavy wow, occasional dropouts (envelope notches), aggressive
  saturation. Lo-fi character for atmospheric textures.

Reference gear (target character, not reproduction):

- Maestro Echoplex EP-3 (transistor preamp + tape loop, hottest Echoplex)
- Roland Space Echo RE-201 (dedicated wow/flutter motor, head bump,
  preamp coloration)
- Strymon Volante / El Capistan (modeled tape delays)

## Public reference sources

- *Roland RE-201 Space Echo service manual* (publicly available PDF) -
  block-level circuit understanding.
- *Maestro Echoplex EP-3 service notes* (community-archived) - preamp /
  head EQ behaviour.
- Strymon El Capistan / Volante manuals - parameter philosophy.
- Will Pirkle, *Designing Audio Effect Plugins in C++*, 2nd ed., chorus and
  flanger chapter (basis for wow/flutter modulation of the read tap).
- Julius O. Smith, *Physical Audio Signal Processing* - allpass-interpolated
  delay lines, modulated read taps.

## Topology

```
                +---- pre-record EQ (head bump) ---+
                |                                   v
in --> [pre-amp soft drive] -> [WRITE tap on tape line]
                                       |
                                       v
                  [tape line: ring buffer, modulated read tap]
                                       |   ^
                                       |   | wow LFO (0.4-0.8 Hz)
                                       |   | flutter LFO (6-8 Hz)
                                       |   | (sub-mode-dependent depths)
                                       v
                              [READ tap: fractional delay]
                                       |
                                       v
                       [per-repeat HF rolloff (one-pole LP)]
                                       |
                                       v
                       [tape saturation soft-clip]
                                       |
                                       +-> wet bus -+
                                       |            |
                                       v            +--> wet*mix --+
                                  [feedback gain]                  |
                                       |                           +-> mix --> out
                                       +--> back into write tap    |
                                                                   v
in -----------------------------------------------------> dry*(1-mix) -+
```

## DSP blocks

1. **Pre-amp soft drive.** Mild asymmetric saturation modeling tape preamp
   coloration. Stronger in Vintage and Broken sub-modes.
2. **Pre-record head bump (EQ).** Resonant low-mid bump (~80-150 Hz, +2 dB)
   characteristic of tape head response. Adds the iconic "thickness" to
   repeats.
3. **Tape line.** Same fractional-delay ring buffer as Digital, but the
   **read tap is continuously modulated**:
   - **Wow**: slow LFO at 0.4-0.8 Hz, depth 5-15 cents (sub-mode-dependent),
     sine-ish but with a small chaotic noise component.
   - **Flutter**: faster LFO at 6-8 Hz, depth 2-4 cents, slight phase noise.
   - **Per-channel phase decorrelation** (~quarter-cycle offset) so stereo
     gets natural width without a separate stereo modulator.
4. **Per-repeat HF rolloff.** One-pole LP in the feedback path whose cutoff
   drops with each pass. At first repeat, cutoff ~7 kHz. By the 4th pass,
   cutoff ~2 kHz. This is independent of `kDelayTone`.
5. **Tape saturation.** `tanh`-shaped soft clip with asymmetric bias, applied
   in the feedback path. Saturation amount couples to feedback level so
   pushed-feedback echoes "warm up" instead of digital-clipping.
6. **Sub-mode mapping.**
   - **Studio**: wow ~0.4 Hz / 5 cents, flutter ~6 Hz / 2 cents,
     light saturation, head bump +1.5 dB, no dropouts.
   - **Vintage**: wow ~0.6 Hz / 10 cents, flutter ~7 Hz / 3 cents,
     medium saturation, head bump +3 dB, occasional micro-dropouts (rare).
   - **Broken**: wow ~0.5 Hz / 15 cents (more chaotic envelope), flutter
     ~8 Hz / 4 cents, heavy saturation, head bump +4 dB,
     periodic dropouts (envelope notches at low rate).
7. **Tone knob (Tape variant).** Tilt filter on the wet bus. tone=0.5 = flat,
   tone=0 = -6 dB at 6 kHz (extra-dark, useful for ambient repeats),
   tone=1 = +3 dB at 6 kHz (compensates per-repeat dulling, useful for
   slap echoes).
8. **Age knob (Tape variant).** Master scaler that **multiplies all vintage
   intensities together**:
   - Age=0: wow/flutter halved, saturation halved, head bump halved,
     dropouts disabled (even Broken sub-mode behaves like Vintage).
   - Age=0.5: sub-mode default values.
   - Age=1.0: 1.5x wow/flutter, 1.3x saturation, more aggressive
     per-repeat HF roll, more frequent dropouts (Broken only).

## Parameter ranges and default targets

| User knob | EParam | Range | Default (Tape snapshot) | Curve |
|---|---|---|---|---|
| Time | `kDelayTime` | 1..2000 ms | 420 ms | linear |
| Feedback | `kDelayFeedback` | 0..1.05 | 0.45 | linear |
| Mix | `kDelayMix` | 0..1 | 0.30 | linear |
| Tone | `kDelayTone` | 0..1 | 0.45 (slightly dark) | linear |
| Age | `kDelayAge` | 0..1 | 0.50 | linear |
| Sub-mode | `kDelayTapeSubMode` (new) | 0..2 | 1 (Vintage) | enum |
| Ping-pong toggle | `kDelayPingPong` | bool | off | n/a |

Default sub-mode is **Vintage** because that's the most "tape-like" and
matches the user's request to fix "sounds very digital".

## Validation / listening tests

- **A/B against Digital.** Same knob settings, mode toggled. Tape should be
  immediately recognisable as tape: pitched wobble, darker repeats, slight
  saturation crunch.
- **Sub-mode A/B.** Studio vs Vintage vs Broken at identical knob settings
  should sound clearly different in saturation level, wow depth, and
  presence of dropouts.
- **Long feedback test.** Time at 800 ms, Feedback at 0.85. Repeats should
  cascade into warm tape mush, never digital-clip.
- **Self-oscillation test.** Feedback at 1.05. Should produce a warm,
  pitch-wobbly drone that's distinctly tape-flavored.
- **Listen for digital artifacts.** No staircase aliasing, no hard clipping,
  no zipper noise on Time changes.

## Doctest checklist

- No NaN on first block after `SetParams` for any sub-mode value.
- Mix=0 passthrough holds (the modulated read tap doesn't bleed into dry).
- Reset clears state.
- All three sub-modes produce audibly different output for the same input
  (assert RMS difference between sub-mode outputs is non-trivial on a
  fixed-seed input).
- Age=0 produces less wow/flutter than Age=1 (assert pitch-detection on a
  long sine input shows wider deviation at higher Age).
- Wet signal has clear HF rolloff after multiple repeats (RMS in 4-8 kHz
  band of late wet output is much lower than dry).
