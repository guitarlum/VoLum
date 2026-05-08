# Delay: Reverse

## Target sound

Captures successive segments of audio and plays each one backwards into the
wet bus. Best with sustained chords, ambient passages, or single notes -
fast strumming gets chewed into a wash. Defaults should produce a recognisable
"swelling backwards" effect at moderate mix without any dialing.

The Reverse mode keeps the existing **double-buffer capture/playback**
topology (one buffer captures the next segment while the other plays the
previous segment backwards). The new improvements are **per-mode tone** and
the **Age knob repurposed as fade-shape softness** so the bloom envelope is
adjustable from sharp/triangle to smooth/sin^2.

Reference gear (target character):

- DigiTech PDS 8000 Reverse (early dedicated reverse pedal)
- Strymon Volante "Studio" reverse mode
- Eventide H910 / H3000 reverse algorithms

## Public reference sources

- Strymon Volante manual - reverse mode philosophy.
- Will Pirkle, *Designing Audio Effect Plugins in C++*, 2nd ed., delay
  segment buffer technique.
- Public DAFx tutorial papers on reverse-playback envelope shaping.

## Topology

```
in -> [WRITE: capture buffer A]      [PLAYBACK: read buffer B in reverse]
        (size = Time ms)                  (read in reverse with fade-shape envelope)
                |                                |
                +---- when buffer A fills,       +---> wet *= mix
                      swap roles -->                            |
                                                                +-> mix --> out
                                                                |
in -----------------------------------------------------> dry*(1-mix) -+
```

## DSP blocks

1. **Capture/playback double buffer.** Existing implementation (see
   `_PrepareReverseBuffers` and `_ProcessReverse` in
   [`AudioDSPTools/dsp/Delay.cpp`](../../../AudioDSPTools/dsp/Delay.cpp)).
2. **Fade-shape envelope** applied during playback to avoid clicks at segment
   boundaries. Existing implementation uses a triangular envelope. The new
   Age knob shapes this:
   - Age=0: sharp triangular fade-in/out (existing behaviour). Most
     percussive bloom.
   - Age=0.5: half-sine (`sin(pi * t / segLen)`). Smoother bloom.
   - Age=1.0: `sin^2(pi * t / segLen)`. Very soft, swelling pad-like bloom.
3. **Feedback** is applied to the playback output on its way back into the
   capture write tap (existing behaviour).
4. **Tone knob (Reverse variant).** One-pole tilt filter on the wet bus.
   tone=0.5 = flat, tone=0 = darker, tone=1 = brighter. Useful because
   reverse blooms can read very airy and a slight darkening grounds them.

## Parameter ranges and default targets

| User knob | EParam | Range | Default (Reverse snapshot) | Curve |
|---|---|---|---|---|
| Time | `kDelayTime` | 50..2000 ms (segment length) | 600 ms | linear |
| Feedback | `kDelayFeedback` | 0..0.85 (clamped from global 1.05 because reverse self-feedback gets messy) | 0.30 | linear |
| Mix | `kDelayMix` | 0..1 | 0.40 | linear |
| Tone | `kDelayTone` | 0..1 | 0.50 | linear |
| Age | `kDelayAge` | 0..1 | 0.50 (half-sine fade) | linear |
| Ping-pong toggle | `kDelayPingPong` | hidden | n/a (disabled in Reverse) | n/a |

The user-facing **Time knob means "segment length"** in Reverse mode. A 600 ms
segment plays back as a 600 ms reverse swell.

Feedback is internally clamped at 0.85 even though the global EParam goes to
1.05, because Reverse-feedback above ~0.85 produces unmusical layered chaos
quickly. Users dialing in 1.0 will perceive the clamp as "feedback maxes out
near the end".

## Validation / listening tests

- **Default-state test.** Strum one chord. After ~600 ms, hear the chord
  swell back in, fading from quiet to loud and tail-stopping cleanly.
- **Age sweep.** At Age=0 the bloom feels percussive and triangular;
  at Age=1 it feels smooth and pad-like.
- **Tone test.** Tone=0 makes blooms warm and dark; tone=1 makes them airy.
- **Feedback test.** Feedback at 0.7 produces audible secondary blooms
  layered over the primary.

## Doctest checklist

- No NaN on first block after `SetParams`.
- Mix=0 passthrough.
- Reset clears state and produces silence on first block of silent input
  after reset.
- Reverse playback present: feed an asymmetric ramp (linearly increasing
  signal) into reverse mode at full mix, expect output amplitude profile to
  decrease over the playback segment then re-increase after segment swap.
- Age extremes produce different envelope shapes: assert peak position of
  envelope differs between Age=0 and Age=1 on a known impulse.
- Ping-pong is ignored in Reverse mode (output of two identical calls with
  ping-pong on vs off should be identical).
