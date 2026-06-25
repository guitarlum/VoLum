# F1 — Pitch pedal: Transpose + Octaver (PRE, before compressor)

Status: design locked, ready to implement. Branch: `feature/transpose-octaver-pedal` (off `feature/1.2.0`).
Do not commit to `dev`/`main`. Merge to `dev` only after acceptance criteria + tests/docs/changelog.

One PRE pedal, two modes (Transpose / Octaver), inserted at the very front of the PRE chain
(before the compressor). UI/UX brainstormed and approved via the mockup
`ui-mockup/pre-pitch-pedal-mockup.html` (open `?only=mixed|collapsed|motifs`); reference captures in
`ui-mockup/_capture/pp-mixed-helix.png`, `pp-collapsed-helix.png`, `pp-motifs2.png`.

## Decisions (locked)

- One combined card, not two. The mode switch maps onto the existing POST architecture, so combining
  is the natural fit, not a compromise.
- Engine: Signalsmith Stretch (polyphonic, ~5–10 ms, MIT, header-only C++11, formant-preserving).
  One shared engine drives both modes. Chosen over mono granular (chords mush) and STFT (too much latency).
- Scope: ship BOTH modes in v1 (they share the engine; marginal cost is small).
- Art: "helix" motif (user pick) — port from the mockup `pitch_helix` into `VoLumTriptychMotifs.h`.

## Research basis (verified)

- NDSP Rabea ships Transpose ("constant ±12 semitones", replaces signal) and Octaver (Chaos Bed:
  Vintage/Modern + Direct + Oct Up + Oct Dwn, layers octaves under dry) as separate devices.
- Octaver lineage: mono analog (OC-2 divider, gritty, chords mush) vs poly digital (POG / OC-5, organ-like, chord-safe).
- Pitch algorithms: phase vocoder 30–100+ ms (unusable live); time-domain granular/PSOLA ~10–12 ms (mono);
  Signalsmith Stretch 5–10 ms polyphonic. `akela91/LiveDSP` proves the live-guitar pitch use case on this exact stack.

## UI / UX

Layout — mixed-width PRE strip, NO window resize (PLUG_WIDTH stays 900; expanded PRE strip stays
`kSectionExpandedW = 430`):

- 4 cards left→right = Pitch → Comp → NAM 1 → NAM 2, matching DSP order.
- Pitch + Comp are slim utility cards (~64px). NAM 1 / NAM 2 grow to ~125px (basically current size).
- 3 chain connectors. Generalize `ComputePreCards()` in
  [NeuralAmpModeler/VoLumTriptychLayout.h](NeuralAmpModeler/VoLumTriptychLayout.h) from the hardcoded
  3-equal (`cardW = .../3.f`) to N cards with per-card flex widths (slim = fixed, NAM = grow).
- Collapsed PRE quiet strip: 3 → 4 stacked slots (PITCH/COMP/NAM 1/NAM 2), each with motif thumbnail.
  Helix must stay legible at ~18px (verified in mockup).

Mode switch — mirror POST reverb exactly (no new control patterns):

- Primary picker `VoLumModePickerControl` (vertical, right of knobs): `TRANSPOSE` / `OCTAVER`
  → bound to `kPrePitchMode`. Same control as reverb `HALL/PLATE/OKTAVERB`.
- Secondary sub-pill `VoLumSubModePillControl` (horizontal, below knobs, shown only in Octaver mode):
  `VINTAGE` / `MODERN` → bound to `kPrePitchOctVoicing`. Same control as reverb Oktaverb sub-pill.
- Per-mode knob snapshot save/restore on mode change, mirroring `OnParamChange` for `kReverbMode`
  (see `VoLumSettings.inc.cpp`).

Per-mode knob sets (bottom knob row, replace-mode groups like `REVERB_KNOBS`):

- Transpose: `SEMI` (`kPrePitchSemitones`), `MIX` (`kPrePitchMix`, default 100%), `LEVEL` (`kPrePitchLevel`).
  MIX exposed so a +7 shift blended with dry doubles as a fixed harmonizer.
- Octaver: `OCT DOWN` (`kPrePitchOctDown`), `OCT UP` (`kPrePitchOctUp`), `DRY` (`kPrePitchDry`),
  `LEVEL` (`kPrePitchLevel`) + Vintage/Modern sub-pill.

UI files to touch: `VoLumTriptychState.h` (new `PRE_PITCH` focus, ordered before `COMP`),
`VoLumTriptych.h` (`kPreSlots` → 4, `slotCount`, `kEffectFocusCount`), `VoLumTriptychLayout.h`
(mixed-width geometry), `VoLumPedalCardControl.h` (Pitch card identity/footer + current-mode display),
`VoLumTriptychMotifs.h` (helix), `NeuralAmpModeler.cpp` (`_AttachVoLumGraphics` cards/connector/knob
groups/pickers, `_UpdateVoLumLayout` show/hide/focus, keyboard cycle `targets[]`), `VoLumKeyboardModel.h`
(`kTargetCount` 7→8, param lists), `NeuralAmpModeler.h` (members + ctrl tags).
Update golden geometry in `test_volum_ui_regressions.cpp`.

## DSP design

Insertion: top of `_VolumProcessPreChain` in
[NeuralAmpModeler/VoLumProcessBlock.inc.cpp](NeuralAmpModeler/VoLumProcessBlock.inc.cpp), BEFORE the
`runPreComp` block. Signal is mono (`kNumChannelsInternal = 1`).

New effect class `VoLumPitch` (new file `VoLumPitchShifter.h`, or extend `VoLumPreEffects.h`), wrapping
Signalsmith Stretch:

- Transpose: 1 voice, ratio `2^(semi/12)`; output = dry*(1-mix) + wet*mix, then Level.
- Octaver: dry*Dry + downVoice(ratio 0.5)*OctDown + upVoice(ratio 2.0)*OctUp, then Level.
  - Vintage: waveshaper grit + lowpass on the octave voices (emulate OC-2 character).
  - Modern: clean poly voices.
- Allocate 2 Signalsmith instances (covers octaver's 2 voices; transpose uses 1). Set ratio per block.
- Dry-path delay line = shifter reported latency, so dry blends phase-aligned with wet (avoid comb filtering).
- Latency: add Signalsmith latency to `preLatency` in `_UpdateLatency()` when Pitch active; call
  `_UpdateLatency()` on active/mode/semitone changes. Consider dual-amp merge compensation.
- RT-safety (per `neural-amp-modeler-native.mdc`): pre-reserve all buffers in `OnReset` (no audio-thread
  alloc), `Reset()` the engine on active→inactive edge and on mode change, scrub non-finite via
  `volum::ScrubNonFiniteInPlace`, final `SoftSafetyClip` already on the bus.
- CPU: 2 instances + NAM is heavy — wire into existing Lite mode (reduce voices/quality or disable).

`ProcessingPlan` ([VoLumProcessingPlan.h](NeuralAmpModeler/VoLumProcessingPlan.h)): add `runPrePitch`
(+ pass `kPrePitchActive` through `MakeProcessingPlan`). Update `test_volum_processing_plan.cpp`.

Vendoring: add `signalsmith-stretch.h` (+ its `dsp/`) under a third-party include dir; register include
path in `NeuralAmpModeler/projects/NeuralAmpModeler-*.vcxproj` and the CMake/test build. Note MIT license
in changelog. RISK: Signalsmith is primarily Clang-tested — compile under MSVC early (see Risks).

## Parameters / state

Append to `EParams` AFTER `kSupportIRToggle`, before `kNumParams` (never reorder; keep stable `GetName()`):

| Param | Type / InitEnum | Range / values | Default | Keyboard step |
|---|---|---|---|---|
| `kPrePitchActive` | bool | on/off | off | toggle (Space) |
| `kPrePitchMode` | enum | Transpose, Octaver | Transpose | — |
| `kPrePitchSemitones` | int-ish double | −12 … +12 | 0 | 1 semitone |
| `kPrePitchMix` | % | 0 … 100 | 100 | match other % knobs |
| `kPrePitchOctDown` | % | 0 … 100 | 0 | match |
| `kPrePitchOctUp` | % | 0 … 100 | 0 | match |
| `kPrePitchDry` | % | 0 … 100 | 100 | match |
| `kPrePitchOctVoicing` | enum | Vintage, Modern | Modern | — |
| `kPrePitchLevel` | dB | −20 … +20 | 0 | match other level knobs |

`InitEnum` enum-value constants go in `VoLumAmpeteCatalog.h` next to the delay/reverb mode constants.

Per-amp storage: PRE comp/NAM settings are stored per amp (`kPrePedalPerAmpSettingsBytes`,
`VoLumChunkLayout.h`). Pitch settings must be stored per amp too → binary per-amp block grows →
chunk version bump + new `ChunkUses…`/migration helper in `VoLumChunkVersion.h` +
version branch in `Unserialization.cpp` (never rewrite old readers; old chunks → Pitch defaults =
bypassed). Also update `VoLumAmpSettings`, `VoLumChunkCodec.h`, `VoLumUserSettingsIO.h`,
`VoLumSettings.inc.cpp` (per-amp dirty/lock snapshot + store-to-amp).

State tests: `test_eparam_order.cpp` (pin new indices), `test_keyboard_steps.cpp` (semitone + level
steps), `test_volum_chunk_version.cpp` + `test_volum_chunk_codec.cpp` (round-trip + migration),
`test_volum_user_settings_io.cpp`.

## Tests (mandatory)

- DSP doctest (`test_volum_pitch.cpp`, register in both vcxproj + tests/CMakeLists.txt):
  bypass identity (Active off = bitwise/▲ passthrough), transpose Mix=0 = dry, octaver all-levels-0-but-dry
  = dry, no NaN/Inf, bounded output, ratio correctness (sine in → expected dominant freq), latency reported,
  both Vintage/Modern voicings finite.
- `test_volum_processing_plan.cpp`: `runPrePitch` gating.
- `test_volum_bypass_identity.cpp`: include Pitch in the chain bypass check.
- `test_volum_ui_regressions.cpp`: mixed-width card geometry (slim Pitch/Comp, wide NAM, 3 connectors) +
  4-slot collapsed strip.
- NaN guard / master safety already covered; extend if needed.

## Docs / changelog

- Update `docs/user-guide.en.md` + `docs/user-guide.de.md` PRE section (new Pitch pedal, both modes, knobs).
- Refresh `docs/user-guide-pre.png`; add a Pitch-focused screenshot if helpful.
- Add one line to `NeuralAmpModeler/installer/changelog.txt` (and note Signalsmith MIT vendor).
- Update the UI file map table in `AGENTS.md` if a new `VoLumPitchShifter.h` file is added.

## Implementation order (de-risk first)

1. Vendor Signalsmith; compile a trivial use under MSVC + Clang to confirm toolchain (BLOCKER gate).
2. `VoLumPitch` wrapper (single mono voice) + doctests (ratio, finite, latency).
3. Params + `runPrePitch` + DSP insert (Transpose only) + latency + dry-align + bypass identity.
4. Octaver voices (down/up/dry) + Vintage/Modern voicing.
5. UI: mixed-width layout + helix motif + mode picker + sub-pill + knob groups + collapsed slot + golden tests.
6. Per-amp state + chunk version + migration + state tests.
7. Docs EN/DE + screenshots + changelog.

## Risks

- MSVC compatibility of Signalsmith Stretch (Clang-primary) — gate item #1; fallback = custom granular voice.
- CPU with 2 shifter voices + NAM (mitigate via Lite mode).
- Dry/wet phase alignment in octaver (delay dry by shifter latency).
- Added PDC latency may surprise users (surface in settings latency readout, already wired).

## Out of scope (v1)

- −2 octave voice, fine/cents detune knob, polyphonic Range/Lowest targeting (OC-5 style). Note as follow-ups.
