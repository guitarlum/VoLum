# Reverb: Oktaverb (Shimmer)

## Target sound

Lush ambient pad reverb with **upper-octave pitched feedback** and an
optional **fifth-up voice** that gives it noticeable forward motion.
The user's reference is the GFI Specular Tempus shimmer mode, which adds
"prob a fifth, has more pre-delay on the oktave, and comes with an octave up
shift. It moves more."

The previous implementation pitched **down** an octave (sub-octave
"Oktaverb"), which is unusual - common shimmer-pedal culture expects octave
**up**. This iteration replaces the down-shift with up-shift as the primary
voice while preserving the old behaviour as an opt-in sub-mode for users who
liked it.

A 3-way sub-toggle:

- **Oct** (default): octave-up only. Classic shimmer.
- **Oct + Fifth**: octave-up plus a fifth-up voice (+7 semitones) blended
  in. The "moves more" Specular-Tempus-flavoured destination.
- **Oct + SubOct**: octave-up plus the original octave-**down** voice
  preserved from the previous implementation. Wider, more orchestral.

## Reference gear (target character)

- GFI System Specular Tempus (shimmer + reverb pedal)
- Strymon BigSky "Shimmer" mode
- Eventide Blackhole + crystals plug-in (reference for "deep ambient
  shimmer")
- Valhalla Shimmer

## Public reference sources

- Sean Costello, [*ValhallaShimmer Tips and Tricks: Bloom*](https://valhalladsp.com/2017/03/27/valhallashimmer-tips-and-tricks-bloom/)
  (philosophy of pitched feedback in a reverb).
- Eventide Blackhole product copy (goal description only, not algorithm).
- Strymon BigSky manual - parameter philosophy.
- Julius O. Smith, *Spectral Audio Signal Processing* (CCRMA, online) -
  granular pitch shifting.
- Public DAFx tutorials on "shimmer reverb" (granular pitch in feedback
  loop).

## Topology

```
in --> [pre-delay] -> [input damping LP] -> [8-line FDN tank (shared with Hall)]
                                                           |
                                                           v
                                            +---- read taps from FDN (8 lines)
                                            |
                                            v
                                  [pitched pre-delay (~40-120 ms)]
                                            |
                                            +---- octave-up grain pitch shifter (per line)
                                            |
                                            +---- fifth-up grain pitch shifter (per line, sub-mode-dependent)
                                            |
                                            +---- octave-down grain pitch shifter (per line, sub-mode-dependent)
                                            |
                                            v
                            [pitched bus, modulated detune ~3-6 cents at 0.3-0.5 Hz]
                                            |
                                            v
                       [shimmer level scaler (kReverbShimmer, retuned curve)]
                                            |
                                            v
                  [pitched bus] + [hall bus] -> out tap mixing -> [tone tilt] -> wet bus
                                                                                       |
                                                                                       v
                                                              wet*mix + (1-mix)*dry --> out
```

## DSP blocks

1. **Shared 8-line FDN base.** Same structure as Hall - pre-delay, input LP,
   FDN with damping. Provides the underlying "lush bed" for the shimmer to
   live on.
2. **Pitched pre-delay (new).** A dedicated short delay line (40-120 ms,
   sub-mode-dependent) sits between the FDN read taps and the pitch
   shifters. This makes the pitched bloom arrive **after** the dry reverb
   onset, mimicking the user's "more pre-delay on the oktave" Specular
   Tempus observation.
3. **Octave-up grain pitch shifter (new).** Replaces `_PitchDownOctaveTick`.
   Implementation: short-grain time-domain pitch shifter (~40 ms grain,
   ~50% overlap, Hann window, ratio 2.0). One shifter per FDN feedback
   line. Output sums to a pitched bus.
4. **Fifth-up grain pitch shifter (new).** Same topology as octave-up but
   ratio = 1.4983 (≈ 7 semitones). Active only in **Oct + Fifth** sub-mode,
   blended at 0.7x of the octave-up level for a non-dominant supporting
   voice.
5. **Octave-down grain pitch shifter (preserved).** The existing
   `_PitchDownOctaveTick` is preserved and only routed when the **Oct +
   SubOct** sub-mode is active.
6. **Detune motion.** Each pitched line gets a small per-line LFO at
   0.3-0.5 Hz, ±3-6 cents detune. Independent phase per line.
   Directly fixes "It moves more".
7. **Shimmer level scaler with retuned curve.** The user complained "needs
   cranked Shimmer to register". The new mapping for `kReverbShimmer`
   (0..1):
   - 0.0: dry reverb only (no pitched feedback).
   - 0.30: comfortably present pitched bloom (was effectively
     "needs to be 0.7+" with old mapping).
   - 0.70: previous "cranked" level - heavy shimmer.
   - 1.00: extreme, near-runaway pitched feedback.
   Curve: exponential, biased toward perceptual loudness (`shimmer^1.5`
   or similar).
8. **Tone-knob curve** for Oktaverb: same retuned curve as Hall (compress
   the dark side, retain the bright top).

## Sub-mode summary

| Sub-mode | Octave-up | Fifth-up | Octave-down (sub) | Pitched pre-delay |
|---|---|---|---|---|
| Oct (default) | 1.0x | off | off | 60 ms |
| Oct + Fifth | 1.0x | 0.7x | off | 80 ms (slightly longer for movement) |
| Oct + SubOct | 0.8x | off | 0.6x (preserves old sub-octave for nostalgia) | 50 ms |

## Parameter ranges and default targets

| User knob | EParam | Range | Default (Oktaverb snapshot) | Curve |
|---|---|---|---|---|
| Mix | `kReverbMix` | 0..1 | 0.40 | linear |
| Decay | `kReverbDecay` | 1..10 s | 5.0 s | linear |
| Tone | `kReverbTone` | 0..1 | 0.55 | per-mode mapping |
| Pre-delay | `kReverbPreDelay` | 0..200 ms | 30 ms | linear |
| Shimmer | `kReverbShimmer` | 0..1 | **0.40** (was 0.5; with retuned curve, this is musical without being cranked) | exponential |
| Sub-mode | `kReverbSubMode` | 0..2 | 0 (Oct) | enum |

## Validation / listening tests

- **Default-state test.** Strum a sustained chord. Should hear the chord
  followed by an upper-octave bloom that fades in slightly after the dry
  signal (because of the pitched pre-delay) and modulates as it sustains.
- **Shimmer curve test.** At Shimmer=0.3, pitched bloom is clearly audible.
  Old behaviour required ~0.7 to feel similar - regression-check this.
- **Oct + Fifth sub-mode.** Same chord, sub-mode = Oct + Fifth. Should hear
  the octave AND a layered fifth - significantly more harmonic motion and
  forward-feel than Oct-only.
- **Oct + SubOct sub-mode.** Sub-octave voice preserved - users who liked
  the old Oktaverb get a path back to that sound.
- **Motion check.** Sustained note's pitched bloom should clearly drift /
  detune over time (not sit static).

## Doctest checklist

- No NaN on first block after `SetParams` for any sub-mode.
- Mix=0 passthrough.
- Reset clears state.
- Octave-up content present: feed a 220 Hz sine, expect non-trivial energy
  near 440 Hz in the wet output (in Oct sub-mode, with Shimmer > 0).
- Fifth content present: in Oct+Fifth sub-mode, expect non-trivial energy
  near ~329 Hz (220 × 1.4983) in the wet output.
- Sub-octave content present: in Oct+SubOct, expect energy near 110 Hz.
- Shimmer curve regression: at Shimmer=0.30, pitched-bus RMS energy is
  meaningfully greater than what the old linear mapping produced
  (validates the "musical at 0.3" requirement).
- Pitched pre-delay measurable: assert pitched-bus onset lags dry-reverb
  onset by ≥ pre-delay ms on impulse input.
