# Delay: Analog (Deluxe Memory Man-style)

## Target sound

Warm, slightly squashed BBD-style analog delay with **built-in optical
chorus/vibrato on the wet signal**. Repeats darken progressively (BBD HF
rolloff increases with delay time and per-repeat), and the chorus voice gives
the wet a gentle wobble that distinguishes it instantly from Tape (which uses
wow/flutter on the read tap rather than a separate modulator) and from Digital
(which has no modulation).

Reference gear (target character, not reproduction):

- Electro-Harmonix Deluxe Memory Man (BBD chip MN3005, compander, optical
  chorus/vibrato)
- MXR Carbon Copy (BBD-style, simpler, no chorus)
- Strymon Brigadier (BBD modeling)

## Public reference sources

- *Deluxe Memory Man* schematic and service info (publicly available;
  community-archived). Used for **block-level circuit understanding** only -
  no schematic figures reproduced.
- MN3005 BBD chip datasheet (Panasonic, public PDF).
- AES paper: J. Vladimir et al., *Modeling Bucket-Brigade Device Networks*
  (DAFx proceedings, public).
- Strymon Brigadier manual - parameter philosophy.
- Will Pirkle, *Designing Audio Effect Plugins in C++*, 2nd ed., chorus/BBD
  chapter.

## Topology

```
                    +---- "compander" expander/compressor (companding) ----+
                    |                                                       |
in -+-> [pre-LPF]-->[BBD-modeled delay line w/ progressive HF loss]-->[wet]-+--> [optical chorus]-+
    |    (~5 kHz)            ^                                              |                     |
    |                        |                                              |                     +-> wet*mix --> out
    |                        +---- feedback gain ---- soft saturation <-----+                     |
    |                                                                                            +-> dry*(1-mix) --+
    +-> dry --------------------------------------------------------------------+--------------> +
```

## DSP blocks

1. **Pre-LPF** at ~5 kHz cuts high frequencies that BBD chips can't reproduce
   anyway and reduces clock-aliasing artifacts that real BBDs exhibit.
2. **Compander (model).** Mimics the dbx-style 2:1 compander used in the
   real Memory Man for noise reduction. Implemented as a simple
   peak-detector-driven 2:1 compress on write, 1:2 expand on read. Audibly,
   this produces a soft program-dependent floor that "pumps" subtly with
   loud transients - the Memory-Man "breathing".
3. **BBD-modeled delay line.** Single fractional-delay line with **per-repeat
   one-pole lowpass** in the feedback path whose cutoff drops with delay
   time. At 100 ms time, cutoff ~6 kHz; at 1 s time, cutoff ~2.5 kHz. Each
   feedback pass also rolls off another ~10% to mimic per-repeat darkening.
4. **Soft saturation** in the feedback path. Asymmetric tanh, increasing
   compression at high feedback (`> 0.6`). At extreme feedback, repeats
   self-oscillate into a warm, woofy howl rather than digital clip.
5. **Optical chorus on the wet signal.** Single LFO at ~0.5-0.8 Hz
   (Age-modulated; see Age below). Modulates a short additional delay
   (4-8 ms) added between the BBD line output and the wet bus. Photocell-style
   asymmetric LFO (slightly slower attack than release on the modulation
   waveform). Gives the iconic "Memory Man chorus" wobble.
6. **Tone knob (Analog variant).** Maps to a tilt EQ on the wet signal:
   tone=0.5 = flat, tone=0 = darker (cuts ~3 dB at 4 kHz), tone=1 = brighter
   (boosts ~2 dB at 4 kHz, partially compensating BBD darkness).
7. **Age knob (Analog variant).**
   - Age=0: minimal chorus (~3 ms depth, very subtle), shallow BBD darkness
     curve, mild compander.
   - Age=0.5: classic Memory Man (~6 ms chorus depth, full BBD curve, full
     compander).
   - Age=1.0: heavy chorus (~10 ms depth, slower LFO ~0.4 Hz), aggressive
     BBD darkening (cutoff drops faster), audible compander pumping.

## Parameter ranges and default targets

| User knob | EParam | Range | Default (Analog snapshot) | Curve |
|---|---|---|---|---|
| Time | `kDelayTime` | 1..600 ms (clamped internally for Analog) | 320 ms | linear |
| Feedback | `kDelayFeedback` | 0..1.05 | 0.42 | linear |
| Mix | `kDelayMix` | 0..1 | 0.32 | linear |
| Tone | `kDelayTone` | 0..1 | 0.50 | linear |
| Age | `kDelayAge` | 0..1 | 0.50 | linear |
| Ping-pong toggle | `kDelayPingPong` | bool | off | n/a |

Note: Analog delay times above ~600 ms break the BBD illusion (real BBD chips
can't go that long). The Time knob still allows up to 2000 ms in the EParam
range, but internally Analog mode clamps the BBD line to 600 ms and lets longer
settings roll off severely - this is **intentional vintage degradation**, not
a bug.

## Validation / listening tests

- **Default-state guitar test.** Play a clean arpeggio. Repeats should
  noticeably warm and darken; chorus wobble should be subtle but present.
- **A/B against Digital.** Same Time/Feedback/Mix in Digital vs Analog.
  Analog should sound clearly darker, slightly compressed, and have
  noticeable chorus motion on the wet.
- **Long-time test.** Time at 600 ms, Feedback at 0.6. Repeats should
  cascade into warm, slightly chaotic feedback that doesn't digital-clip.
- **Self-oscillation test.** Time at 300 ms, Feedback at 1.05. Should
  self-oscillate into a warm howl, never harsh.
- **Age sweep.** Age 0 -> 1 should monotonically increase chorus depth and
  HF rolloff steepness.

## Doctest checklist

- No NaN on first block after `SetParams`.
- Mix=0 yields passthrough.
- Age=0 produces less HF rolloff than Age=1 (assert RMS energy in 4-8 kHz
  band on broadband input is higher at Age=0).
- Chorus modulation present at Age>0: assert wet signal has non-zero
  variation at the chorus LFO rate (band-limited variance test).
- Ping-pong on works the same as Digital (cross-channel routing).
