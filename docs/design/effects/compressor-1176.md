# Compressor: 1176-style FET

## Target sound

A bright, fast, punchy compressor that sits in the front of the signal chain.
It's the "guitar plugin compressor" players reach for when they want **slam
and presence** rather than slow glue. Defaults should give a small, pleasant
amount of gain reduction (~3-4 dB on average) at typical guitar input levels;
cranking Input drives noticeable squash and harmonic colour without ever
sounding mushy.

Reference gear (target character, not reproduction):

- Universal Audio 1176LN (FET feedback compressor, fast attack, audible
  harmonic colour)
- Universal Audio UAD 1176 plugin family (rev A "bluestripe", rev E
  "blackface")

## Public reference sources

Used to inform the design. Content paraphrased; no verbatim reproduction.

- *UREI 1176LN service manual* (Universal Audio archives) - public PDF.
- Universal Audio knowledge base articles on 1176 attack / release behavior
  and "all-buttons-in" mode.
- Steinberg / iZotope / DAW-vendor general-purpose articles on FET compressor
  topology.
- Will Pirkle, *Designing Audio Effect Plugins in C++*, 2nd ed., chapters on
  dynamics processors.
- Udo Zoelzer (ed.), *DAFX: Digital Audio Effects*, 2nd ed., dynamics chapter
  (envelope detection, attack/release shapes, soft-knee curves).

## Topology

```
                                                                +---- mix=1 (fixed) ----+
                                                                |                       v
in ----> [Input gain] ----> [pre-detector FET soft-clip] ----+--+----> [VCA] ----> [Output gain] --> out
                                                             |          ^
                                                             v          |
                                                       [feedback path]  |
                                                        (1176 = FET     |
                                                         in feedback)   |
                                                             |          |
                                                             +-> [envelope detector] (peak, asymmetric)
                                                                  |
                                                                  v
                                                            [program-dependent
                                                             release stage]
                                                                  |
                                                                  v
                                                            [static curve, 4:1]
                                                                  |
                                                                  +-> gain reduction in dB
```

## DSP blocks

1. **Input gain** (user-facing "Input" knob). 0..10 maps to a non-linear curve
   reaching ~+24 dB at 10. The Input knob also drives the FET pre-detector
   harder, so it doubles as a "drive" control - this is the iconic 1176
   technique.
2. **Pre-detector FET soft-clip.** Asymmetric soft saturation modeled as a
   shaped tanh with a small DC bias and slight even-harmonic tilt
   (`y = tanh(a*x + b) - tanh(b)` with a small positive `b`). Adds harmonic
   colour and softens transients before the detector sees them. Audible at
   high Input settings, almost invisible at low Input.
3. **Envelope detector.** Peak follower (single-pole, asymmetric attack and
   release coefficients). Attack range 0.02..1.0 ms (user-mapped to the
   "Attack" knob via an exponential curve - lower index = faster, matching
   1176 hardware where higher knob position = faster attack but we keep
   left=slow / right=fast for VST UX). Release range 50..1100 ms.
4. **Program-dependent release.** The release time stretches when consecutive
   gain-reduction events overlap (modeled as a slower secondary release
   coefficient that blends in based on detector activity). Gives the 1176 its
   characteristic "breathing" decay on dense material.
5. **Static curve.** Fixed 4:1 ratio with a soft knee around the threshold.
   Threshold is implicit (set by detector calibration) so that at default
   Input the average guitar signal triggers ~3-4 dB of GR.
6. **VCA.** Multiplies the dry signal by `10^(GR_dB / 20)`. We do **not** use
   parallel/wet-dry mix - 1176 is "always in series" - mix is locked at 1.0.
7. **Output gain** (user-facing "Output" knob). -20..+20 dB linear in dB.

## Parameter ranges and default targets

| User knob | EParam | Range | Default | Curve |
|---|---|---|---|---|
| Input | `kPreCompAmount` (relabel) | 0..10 | 3.0 | exponential, +0..+24 dB |
| Attack | `kPreCompAttack` | 0.02..1.0 ms | 0.4 ms | exponential |
| Release | `kPreCompRelease` | 50..1100 ms | 250 ms | exponential |
| Output | `kPreCompLevel` (relabel) | -20..+20 dB | 0 dB | linear (dB) |
| (hidden) Ratio | `kPreCompRatio` | locked at 4:1 | 4.0 | n/a |
| (hidden) Mix | `kPreCompMix` | locked at 1.0 | 1.0 | n/a |

The Attack range tightens substantially compared to the previous
implementation (was 0.1..30 ms): real 1176 attack is 0.02..0.8 ms and players
expect that fast end. Release range widens slightly (was 20..800 ms) to give
genuine slow-release territory for sustain-style settings.

## Validation / listening tests

- **Default-state guitar test.** Strum a clean chord at typical input level.
  GR meter should show ~3-4 dB peaks. Tone should brighten and tighten
  slightly without obvious pumping.
- **Slam test.** Input at 8, Attack at 0.05 ms, Release at 100 ms.
  Single-note runs should feel "in your face", with audible harmonic crunch
  on the loudest notes.
- **Sustain test.** Input at 6, Attack at 0.5 ms, Release at 800 ms.
  Sustained chord should hold its level smoothly without breathing artifacts.
- **No-input test.** With no input, output should be silent (no detector
  noise, no DC).
- **Bypass A/B.** Compressor bypassed vs Input=0, Output=0: identical.

## Doctest checklist

- `mix` locked at 1.0 internally regardless of `kPreCompMix` value.
- `ratio` clamps to 4:1 internally regardless of `kPreCompRatio`.
- Attack 0.02 ms produces faster envelope rise than Attack 1.0 ms (assert
  envelope value ratio at known impulse).
- Release 50 ms decays faster than Release 1100 ms.
- FET pre-detector saturation present: feeding a sine at 0.9 amplitude and
  Input=10 produces non-zero THD at 2nd and 3rd harmonics.
- No NaN on first block after `SetParams`.
- Bypass-equivalent sanity: with input below detector floor, output equals
  `Input * inputGain * outputGain` within 1e-3.
