# Delay: Digital

## Target sound

Pristine, time-accurate repeats with no audible HF loss between repeats. The
"reference" delay - what users compare other modes against. Should feel
**clean, present, and stereo-capable** (via the global ping-pong toggle).
Defaults at modest mix and feedback so it works as a useful slap/short echo
without dialing.

Reference gear (target character):

- Boss DD-3 / DD-5 (clean digital delay)
- TC Electronic 2290 (early studio digital delay)
- Strymon Timeline "Digital" mode

## Public reference sources

- Strymon Timeline manual (publicly downloadable PDF) - parameter behaviour
  philosophy.
- Julius O. Smith, *Physical Audio Signal Processing* (CCRMA, online) -
  fractional-delay line design, allpass interpolators.
- Udo Zoelzer (ed.), *DAFX: Digital Audio Effects*, 2nd ed., delay-line
  chapter.

## Topology

```
            +-------- pingPong stereo cross-feed -------+
            |                                           |
in_L ---+---|---> [delay line L] ---> [feedback gain] --+--> wet_L --+
        |   |                                           |            +-> mix --> out_L
        |   +-> ... write tap                                        |
        |                                                            +-> dry --+
        |
in_R ---+---+---> [delay line R] ---> [feedback gain] --+--> wet_R --+
            |                                           |            +-> mix --> out_R
            +-------- pingPong stereo cross-feed -------+            +-> dry --+
```

When ping-pong is **off**, channels are independent. When ping-pong is **on**,
each channel's delay output feeds back into the **other** channel's input,
producing the classic L-R-L-R bouncing. Repeats decay equally; only stereo
position alternates.

## DSP blocks

1. **Fractional-delay ring buffer** per channel, sized to the maximum delay
   time at the current sample rate (the existing implementation already does
   this).
2. **Linear interpolation** at the read tap; smoothing of `mTimeMs` changes
   so knob-twiddling doesn't pitch-shift audibly.
3. **Feedback gain** clamped at 1.05 (current implementation already
   permits a small runaway region for self-oscillation experimentation).
4. **Per-mode tone tilt** (the new `kDelayTone` knob). For Digital, tone
   maps a single one-pole tilt filter applied **on the wet signal** (not
   per-repeat) so that the first repeat already has the user's tone but
   subsequent repeats don't darken further. Tone curve: at 0.0, -6 dB shelf
   above 4 kHz; at 0.5, flat; at 1.0, +3 dB shelf above 4 kHz.
5. **Age (digital grit) processing** for `kDelayAge`:
   - At Age=0, signal is bit-perfect.
   - At Age>0, a subtle pre-delay-line **bit-crusher** kicks in (16 bits down
     to ~10 bits) plus a small noise floor (-80 dB at Age=0.5, -65 dB at
     Age=1.0) gated to follow signal envelope. Models a 1980s A/D bottleneck.
6. **Ping-pong routing** (when `kDelayPingPong` is true): the feedback path
   takes the opposite-channel output before mixing back into its own write
   tap. Hidden when mode = Reverse.

## Parameter ranges and default targets

| User knob | EParam | Range | Default (Digital snapshot) | Curve |
|---|---|---|---|---|
| Time | `kDelayTime` | 1..2000 ms | 380 ms | linear |
| Feedback | `kDelayFeedback` | 0..1.05 | 0.35 | linear |
| Mix | `kDelayMix` | 0..1 | 0.28 | linear |
| Tone | `kDelayTone` (new) | 0..1 | 0.50 (flat) | linear |
| Age | `kDelayAge` (new) | 0..1 | 0.00 (clean) | linear |
| Ping-pong toggle | `kDelayPingPong` (new) | bool | off | n/a |

## Validation / listening tests

- **Mix=0 passthrough.** Output equals input bit-for-bit.
- **Single-tap.** With one short impulse, expect repeats at exactly Time-ms
  spacing for several iterations (no drift).
- **Ping-pong stereo image.** With ping-pong on, mono input yields equal-amp
  alternating L/R repeats.
- **Tone neutrality.** Tone=0.5 produces measurably flat magnitude response
  on the wet signal (within 0.3 dB across 200 Hz - 8 kHz).
- **Age=0 cleanliness.** Wet output bit-perfect (no crusher quantization, no
  noise) at Age=0 across a 1-second null test.

## Doctest checklist

- No NaN on first block after `SetParams`.
- Mix=0 yields output==input.
- Reset clears state (existing test pattern).
- Age=0 produces zero noise floor in silent input.
- Ping-pong on produces L-R alternation (cross-channel correlation < 0
  for the first repeat).
