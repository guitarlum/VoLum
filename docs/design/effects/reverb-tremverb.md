# Reverb: TremVerb

## Target sound

A short plate/room reverb with **photocell-style optical tremolo modulating
the wet signal only**. The dry signal passes through clean while the reverb
tail throbs in volume - the iconic Strymon Flint sound and the classic
amp-style "reverb plus tremolo" combination, but with the tremolo on the
wet only (so dry guitar transients hit hard while the wet reverb sways).

This is a **new mode** added at index 3 of the reverb mode picker.

## Reference gear (target character, not reproduction)

- Strymon Flint (3 reverb types × 3 tremolo types; we model the small-room
  - photocell-tremolo combination)
- Vintage Fender amp reverb + tremolo combinations (Twin Reverb, Deluxe
Reverb)
- Universal Audio Galaxy Tape Echo with tremolo overlay

## Public reference sources

- Strymon Flint manual (publicly downloadable PDF) - parameter philosophy
for TYPE x VOICE pedals.
- Public papers on photocell-vs-bias-vs-harmonic tremolo topology
(e.g., trémolo blog posts on the Premier Guitar / GuitarPlayer sites,
cited by URL only - not reproduced).
- Will Pirkle, *Designing Audio Effect Plugins in C++*, 2nd ed., tremolo
chapter (LFO shapes).
- Dattorro 1997 (used as base reverb topology for the short plate).

## Topology

```
in --> [pre-delay] -> [pre-LP (input damping)] -> [input diffusion (3 APs)]
                                                          |
                                                          v
                          +-- TANK A: [decay-AP] -> [delay] -> [LP damp] --+
                          |                                                |
                          +< -- decay gain <- [LP damp] <- [delay] <- [decay-AP] <- TANK B
                                              ^                              ^
                                              | mod                          | mod
                                                                             |
                                                                             v
                                              [output mixing -> wet bus]
                                                                             |
                                                                             v
                                            [photocell trem VCA on wet only]
                                                                             |  ^
                                                                             |  |
                                                                          tremolo LFO
                                                                          (rate=kReverbTremRate,
                                                                           depth=kReverbShimmer
                                                                                 repurposed)
                                                                             |
                                                                             v
                                                  [output tone tilt (kReverbTone)]
                                                                             |
                                                                             v
                                            mix*wet + (1-mix)*dry --> out
```

## DSP blocks

1. **Pre-delay** (existing, shared).
2. **Input diffusion (3 APs).** Smaller diffusion stage than the full Plate
  - we want a short, dense room rather than a big plate.
3. **Tank halves**, Dattorro-derived but with shorter delay-line lengths
  to give a target decay range of 1.0-2.5 s (vs Plate's 0.3-6 s and Hall's
   0.5-10 s).
4. **Light tank modulation.** ~0.6 Hz at small depth, kept light because
  the tremolo will dominate the perceived motion.
5. **Photocell tremolo VCA on wet bus only (new).**
  - LFO shape: smooth quasi-sinusoid with **slight asymmetric duty**
   (rise slower than fall, modeling the photocell's response curve).
   We use a parameterised `0.5*(1 + sin(...))` shape with an asymmetric
   skew applied via `tanh(k * (raw - 0.5)) + 0.5`.
  - Frequency: `kReverbTremRate`, range 0.5..10 Hz, default 4 Hz.
  - Depth: `kReverbShimmer` repurposed as "Trem Depth", range 0..1,
  default 0.55. At depth=0 there's no modulation (effectively bypass
  to tremolo, full wet through). At depth=1 the wet signal modulates
  between full and silent.
  - Routing: applied **only** to the wet bus before the wet/dry mix. Dry
  is untouched.
6. **Tone-knob curve.** Plate-like mapping (compress dark side, retain
  bright).

## Parameter ranges and default targets


| User knob                                                 | EParam                  | Range                                        | Default (TremVerb snapshot) | Curve            |
| --------------------------------------------------------- | ----------------------- | -------------------------------------------- | --------------------------- | ---------------- |
| Mix                                                       | `kReverbMix`            | 0..1                                         | 0.32                        | linear           |
| Decay                                                     | `kReverbDecay`          | 0.5..2.5 s (clamped internally for TremVerb) | 1.5 s                       | linear           |
| Tone                                                      | `kReverbTone`           | 0..1                                         | 0.55                        | per-mode mapping |
| Pre-delay                                                 | `kReverbPreDelay`       | 0..150 ms                                    | 12 ms                       | linear           |
| **Trem Depth** (Shimmer knob, relabeled when in TremVerb) | `kReverbShimmer`        | 0..1                                         | 0.55                        | linear           |
| **Trem Rate**                                             | `kReverbTremRate` (new) | 0.5..10 Hz                                   | 4.0 Hz                      | exponential      |
| Sub-mode                                                  | `kReverbSubMode`        | hidden in TremVerb (no sub-modes)            | n/a                         | n/a              |


The Decay range is **clamped internally** at 2.5 s even though the EParam
goes to 10 - longer decays in TremVerb wash out the tremolo motion. Settings
above 2.5 s read as "near 2.5 s".

## Validation / listening tests

- **Default-state test.** Strum a chord. Dry hits hard and clean; the reverb
tail throbs at ~4 Hz with comfortable depth.
- **Wet-only modulation test.** Mix=1.0, Trem Depth=1.0. Wet signal
modulates fully while dry would have been silent. Mix=0.0 produces
unmodulated dry only.
- **Trem Rate sweep.** 0.5 Hz feels slow-and-spooky, 4 Hz is the comfortable
default, 10 Hz is fast and almost ring-mod-ish on the wet.
- **Trem Depth at 0.** Effect should sound like a clean short plate with
no modulation.
- **Asymmetric duty audible.** Compared to a pure sine VCA, the photocell
shape should give a slightly "bouncy" feel rather than perfectly smooth
rise-fall symmetry.

## Doctest checklist

- No NaN on first block after `SetParams`.
- Mix=0 passthrough (dry only, no tremolo).
- Reset clears state.
- Tremolo modulation present at depth>0: assert non-zero RMS variation in
wet output at the tremolo LFO rate.
- Trem Depth=0 produces stable wet output (no envelope variation at LFO
rate).
- Wet-only routing verified: dry signal envelope is unaffected by tremolo
rate/depth (assert dry component of output equals input * (1 - mix)
for any tremolo settings).
- Decay clamp: setting `kReverbDecay = 5.0` produces internal decay
saturating near 2.5 s (assert via tail energy at +3 s after impulse).

