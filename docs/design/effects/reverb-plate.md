# Reverb: Plate

## Target sound

Dense, metallic-but-musical plate reverb with **brighter top end**, **more
visible motion in the tail**, and **denser early reflections** than the
existing implementation. The user's complaints are:

1. "A bit metallic" - too much resonant ringing on the dark side.
2. "Moves maybe too slow" - LFO modulation depth/rate too gentle.
3. "Sounds too much like a delay" - early reflections too sparse, too
   slap-like.
4. "Too dark" - default tone curve sits below comfortable plate brightness.

This iteration directly addresses each complaint and adds a **3-way sub-toggle**
with three distinct plate flavours, inspired by Valhalla Plate's mode set.

A 3-way sub-toggle:

- **Steel** (default): neutral, slightly dark plate. Default destination,
  works for everything, doesn't fight a midrangey guitar.
- **Brass**: brighter, sharper attack, more upfront. Great for funk,
  single-note leads, percussive chords.
- **Copper**: high modal density, dark, chamber-like. Directly addresses
  the "too metallic" complaint when the user wants lush instead of
  plate-y. Sounds closer to a small reverb chamber than a steel plate.

Reference gear (target character, not reproduction):

- EMT 140 (the canonical steel plate)
- EMT 240 (gold plate, but Valhalla's blog notes Costello "kinda hated"
  this one - we don't model it)
- Lawson Audio plate
- Valhalla Plate (12 modes; we sample three distinct destinations)

## Public reference sources

- Jon Dattorro, *Effect Design Part 1: Reverberator and Other Filters*,
  JAES 1997 (the foundational plate algorithm reference; public PDF).
- Sean Costello, [*ValhallaPlate: The Reverb Modes*](https://valhalladsp.com/2015/11/08/valhallaplate-the-reverb-modes/)
  (informs sub-mode philosophy, **not** algorithm details).
- Sean Costello, [*Chambers vs Plates*](https://valhalladsp.com/2015/11/07/chambers-versus-plates/)
  (informs Copper sub-mode).
- Will Pirkle, *Designing Audio Effect Plugins in C++*, 2nd ed., plate /
  tank reverb chapter.

## Topology

```
in --> [pre-delay] --> [pre-LP (input damping)] --> [4-stage input diffusion (allpass chain)]
                                                                |
                                                                v
        +---- TANK HALF A: -> [decay-AP] -> [delay] -> [LP damping] ----+
        |                                                                |
        +<--- decay gain <--- [LP damping] <- [delay] <- [decay-AP] <----+
                                              ^
                                              |
                                              + TANK HALF B: same structure, cross-fed
                                                with modulation phase offset
                                                (kicks per-sub-mode)
                                                                |
                                                                v
                          [output tap mixing -> L/R taps from both halves]
                                                                |
                                                                v
                              [output tone tilt (kReverbTone, per sub-mode)]
                                                                |
                                                                v
                                wet bus -> mix*wet + (1-mix)*dry --> out
```

## DSP blocks

1. **Pre-delay** (existing `mPreDelayBuf`).
2. **Input damping LP** (existing).
3. **4-stage input diffusion** (existing `mInputAPBuf` / `mInputAPLen`).
   Densified by adding a small randomization to the existing AP lengths
   per sub-mode (Brass = shorter, sharper attack; Copper = longer with more
   diffusion = chamber feel; Steel = balanced default).
4. **Densified early reflections.** New: a short pre-tank early-reflection
   tap (~3-5 short taps in 5-25 ms range) summed into the tank input.
   Directly fixes "sounds too much like a delay" because the wet now has
   a recognisable plate "splat" before the tail develops, instead of a
   solitary discrete echo.
5. **Tank halves** (existing `mTank[2]` Dattorro structure). Per-sub-mode
   tuning of:
   - decay-AP coefficient (higher in Copper for density, moderate in Steel,
     lower with more transient bite in Brass)
   - delay-line lengths (sub-mode-scaled around base values)
   - LP damping cutoff target (Steel = 5 kHz baseline, Brass = 8 kHz,
     Copper = 4 kHz with extra diffusion to compensate)
6. **Modulation.** Existing `mPlateLfoPhase`. New: faster default LFO rate
   (~1.2-1.8 Hz vs old ~0.6 Hz) and ~1.5x deeper depth, applied to the tank
   delay-line read taps. Directly fixes "moves maybe too slow".
7. **Tone-knob curve.** Re-mapped so default (tone=0.5) sits brighter than
   the previous mapping. Top end retained, dark end pulled in:
   - tone=0.0 -> LP cutoff at 1.8 kHz
   - tone=0.5 -> LP cutoff at 6 kHz (was ~4 kHz before)
   - tone=1.0 -> LP cutoff at 11 kHz
8. **Sub-mode summary**:

| Sub-mode | Decay-AP coefficient | Tank delay scaling | LP cutoff target | Modulation depth | Early-reflection density |
|---|---|---|---|---|---|
| Steel (default) | 0.55 | 1.00x | 6 kHz | 1.0x | 4 taps |
| Brass | 0.45 (less diffusion = sharper attack) | 0.80x (faster onset) | 8 kHz | 0.8x | 6 taps (denser splat) |
| Copper | 0.70 (more diffusion = denser) | 1.30x (longer) | 4 kHz | 1.5x | 8 taps (chamber-ish) |

## Parameter ranges and default targets

| User knob | EParam | Range | Default (Plate snapshot) | Curve |
|---|---|---|---|---|
| Mix | `kReverbMix` | 0..1 | 0.30 | linear |
| Decay | `kReverbDecay` | 0.3..6 s | 1.8 s | linear |
| Tone | `kReverbTone` | 0..1 | **0.60** (brighter default than before) | per-mode mapping |
| Pre-delay | `kReverbPreDelay` | 0..150 ms | 15 ms | linear |
| Shimmer | `kReverbShimmer` | hidden in Plate | 0 | n/a |
| Sub-mode | `kReverbSubMode` | 0..2 | 0 (Steel) | enum |

## Validation / listening tests

- **"Not metallic" check.** Default Steel sub-mode, sustained chord. Tail
  should sound like a real plate - dense, slightly bright, alive - not
  ringy or pinged.
- **"Moves more" check.** Hold a single note. Tail should clearly modulate
  in pitch / amplitude over the decay; old version sounded too static.
- **"Not slap-delay" check.** Single percussive impulse should produce a
  dense splat that quickly decorrelates from the dry signal, not a
  recognisable single early echo.
- **Sub-mode A/B.** Steel/Brass/Copper at identical Decay/Mix/Tone should
  sound clearly different in attack character and density.
- **Default brightness.** Out-of-the-box Plate at default tone should sound
  "right" without dialing Tone up.

## Doctest checklist

- No NaN on first block after `SetParams` for any sub-mode.
- Mix=0 passthrough.
- Reset clears state.
- Modulation present: assert non-zero variation in wet output at the plate
  LFO rate over a sustained input.
- Sub-modes produce different responses (RMS difference at +500 ms after
  impulse meaningfully different).
- Tone curve maps as expected: assert HF content increases monotonically
  with tone knob and crosses expected thresholds at tone=0.5.
- Default-snapshot HF energy is higher than existing baseline (validates
  "less dark default" requirement).
