# Reverb: Hall

## Target sound

Lush, large, **musical** hall reverb. The "always-useful" reverb mode -
players reach for it for ambient passages, ballads, and big stage tones.
Defaults should sound **immediately significant** at the default Mix and Decay
- the previous version required cranked Mix/Decay to register meaningfully,
which we explicitly fix.

A 3-way sub-toggle picks the size/character:

- **Studio**: small/intimate, slightly bright, ~1.2 s decay center. Good
  for tighter mixes and rhythm guitar.
- **Concert** (default): medium hall, balanced, ~2.5 s decay center.
  The "main" hall.
- **Cathedral**: large, long-tail, lightly chorused. ~5+ s decay. For
  ambient and atmospheric work.

Reference gear (target character):

- Lexicon 480L "Random Hall"
- Bricasti M7
- Valhalla Room / Vintage Verb hall settings
- Eventide H3000 hall

## Public reference sources

- Manfred Schroeder, *Natural Sounding Artificial Reverberation*, JAES 1962
  (foundational paper, public).
- John Stautner & Miller Puckette, *Designing Multi-Channel Reverberators*
  (Computer Music Journal 1982) - FDN structure.
- Julius O. Smith, *Physical Audio Signal Processing* (CCRMA, online) - FDN
  with feedback matrix, Hadamard topology.
- Sean Costello, Valhalla DSP blog - philosophical posts on hall reverb
  modulation and decay.
- Will Pirkle, *Designing Audio Effect Plugins in C++*, 2nd ed., FDN reverb
  chapter.

## Topology

```
in --> [pre-LP (input damping)] -> [pre-delay] --+
                                                  |
                                                  v
                  +-------------------- 8-line FDN ----------------------+
                  |                                                       |
                  | [DL_0] -+                                  [DL_4] -+  |
                  | [DL_1] -+                                  [DL_5] -+  |
                  | [DL_2] -+--> [Hadamard 8x8] -> [LP per line] --+----  |
                  | [DL_3] -+      orthogonal      (damping)       |   .  |
                  |                feedback matrix                 |  +-+ |
                  |                                                v  | | |
                  |                                          [decay   | | |
                  |                                           gain]   | | |
                  |                                                |  | | |
                  |                                                +<-+ | |
                  +------------------------------------------------+----+ |
                                                                          |
                  + line-output mixing (sub-mode-dependent matrix) -------+
                  |
                  v
        [output tone tilt (kReverbTone)]
                  |
                  v
        wet bus -> mix*wet + (1-mix)*dry --> out
```

## DSP blocks

1. **Input pre-LP.** One-pole low-pass damping the input that hits the FDN.
   Models room air absorption.
2. **Pre-delay.** Shared global pre-delay (existing `mPreDelayBuf`).
3. **8-line FDN with Hadamard feedback matrix** (existing
   `_ProcessHall`). Delay line lengths are prime / mutually-incommensurate
   to maximise modal density.
4. **Per-line damping LP** (existing `mHallLPState`). Cutoff is a function
   of `kReverbTone` and the sub-mode's brightness target.
5. **Sub-mode mapping** (new):
   - **Studio**: shorter delay line lengths (×0.5), brighter target tone
     (LP cutoff +1.5 octaves), modulation depth halved.
   - **Concert** (default): existing line lengths and modulation behaviour.
   - **Cathedral**: longer delay line lengths (×1.7), darker target tone
     (LP cutoff -0.5 octaves), modulation depth ×1.5, and an additional
     **chorused tail** via a slow detune LFO on a subset of lines.
6. **Tone-knob curve compression on the dark side.** The previous tone curve
   gave too much range below tone=0.5 (sounded muffled fast). New mapping:
   - tone=0.0 -> LP cutoff at 1.5 kHz (was much darker before).
   - tone=0.5 -> LP cutoff at 5 kHz.
   - tone=1.0 -> LP cutoff at 10 kHz (existing top end).
   - Curve: exponential, biased so half the knob range covers the
     comfortable 3-8 kHz span.
7. **Modulation.** Each delay line read tap is modulated by a slow LFO
   (~0.3-0.7 Hz, slight phase offset per line) to break up modal ringing
   and keep the tail moving. Sub-mode scales depth.
8. **Output mixing.** Hall outputs are summed to L and R via a
   line-to-channel mix matrix that gives natural width without
   over-stereoising the mix.

## Parameter ranges and default targets

| User knob | EParam | Range | Default (Hall snapshot) | Curve |
|---|---|---|---|---|
| Mix | `kReverbMix` | 0..1 | **0.32** (was 0.20-ish; nudged up) | linear |
| Decay | `kReverbDecay` | 0.5..10 s | **3.5 s** (was 3.0; nudged up) | linear |
| Tone | `kReverbTone` | 0..1 | 0.55 (slightly bright of flat) | per-mode mapping |
| Pre-delay | `kReverbPreDelay` | 0..200 ms | 30 ms | linear |
| Shimmer | `kReverbShimmer` | hidden in Hall (only used by Oktaverb / repurposed in TremVerb) | 0 | n/a |
| Sub-mode | `kReverbSubMode` (new) | 0..2 | 1 (Concert) | enum |

The user feedback explicitly asked for **stronger Hall defaults** because
"it needs high mix and decay to sound significant". We bump Mix to ~0.32 and
Decay to ~3.5 s.

The Tone knob's **lower-side range is compressed** so the dark end is less
extreme. Upper end stays close to existing behaviour (user said "upper is
fine").

## Validation / listening tests

- **Default-state test.** Strum a chord. Reverb should be **immediately
  audible and significant** without dialing Mix/Decay up.
- **Sub-mode A/B.** Studio vs Concert vs Cathedral on the same input should
  feel like three clearly different room sizes - tightness vs main vs vast.
- **Tone-knob lower-side test.** Tone=0.2 should sound dark but not
  muffled-mush dark. The previous behaviour was too aggressive on the
  dark side.
- **Modulation test.** Sustained chord should have audible motion in the
  tail - not static comb-filter ringing.
- **Long-decay test.** Cathedral, Decay=8s, Mix=0.5. Tail should bloom
  smoothly and decay over many seconds without metallic artifacts.

## Doctest checklist

- No NaN on first block after `SetParams` for any sub-mode.
- Mix=0 passthrough.
- Reset clears state.
- Sub-modes produce different responses to a known impulse: assert RMS
  energy at +1 s after impulse is meaningfully different across sub-modes.
- Tone curve compresses dark side: assert HF content of wet output at
  tone=0.2 is **higher** than what the previous mapping produced (regression
  test against a recorded baseline or compute against expected cutoff).
- Hall energy at default snapshot is significantly above existing
  `default mix=0.20` baseline (validates "stronger defaults" requirement).
