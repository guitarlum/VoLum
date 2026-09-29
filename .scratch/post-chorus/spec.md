# POST Chorus (and the chunk freeze it waits on)

Locked map: `.scratch/release-1.3.0/map.md`. Do not reopen product calls.

## Outcome

1. **Chunk freeze first.** A 1.2.2 plugin opening a 1.3.0 project keeps the 1.2.2-era sounding rig. New instance state lives in id-tail JSON, never as extra prefix doubles or extra binary-tail bytes. This lands **before** any `EParams` are appended.
2. **Chorus** is the fourth POST card: Chorus → Delay → Reverb → Tremolo. Modes CLASSIC / WARPED / CLEAR / ENSEMBLE (default WARPED, card bypassed). Knobs RATE · DEPTH · TONE · WIDTH · MIX. Motif: one Throat wormhole for all four modes.

## Locked decisions (do not invent)

- [How does a current build read a future DAW chunk?](../release-1.3.0/issues/07-forward-compatible-chunks.md)
- [Where does Chorus sit in POST, and which voices?](../release-1.3.0/issues/08-chorus-placement-and-voices.md)
- [Which chorus voice families fit VoLum POST?](../release-1.3.0/issues/09-chorus-voice-families.md) — knob row, CLEAR, ENSEMBLE. Warm / default Clear do **not** ship.
- [What Chorus card motif ships with the four modes?](../release-1.3.0/issues/14-chorus-card-motif.md)

## Serial order

Ticket `01` (chunk freeze) must be **resolved** before tickets `02`/`03` append Chorus `EParams` or JSON keys. `04` last.

## File ownership

- Freeze: `Unserialization.cpp`, `NeuralAmpModeler.cpp` (`SerializeState`), `VoLumParams.h` / `VoLumChunkIdTail.h` / `VoLumChunkLayout.h`, `test_volum_state_roundtrip.cpp`, `test_volum_chunk_codec.cpp`, `test_volum_ui_regressions.cpp` (the live-`kNumParams` source pin).
- DSP: new `NeuralAmpModeler/VoLumChorus.h` (header-only, same placement as `VoLumTremolo.h`). Do **not** add Chorus to the AudioDSPTools submodule.
- UI: `VoLumTriptych.h` / `VoLumTriptychLayout.h` / `VoLumTriptychMotifs.h` / `VoLumPedalCardControl.h` / `VoLumLayoutBuild.inc.cpp` / `VoLumLayoutRuntime.inc.cpp` / `VoLumProcessBlock.inc.cpp` / `VoLumSettings*.inc.cpp` / `VoLumAmpeteCatalog.h` (scene fields).
- Params: append Chorus `EParams` **after** the freeze, immediately before `kNumParams`. Indices 0–92 stay put.

## Id-tail JSON keys this spec owns

After freeze, Chorus per-amp state in the id tail (schema bump informational only):

```json
"cho": {
  "active": false,
  "mode": 1,
  "rate": 0.35,
  "depth": 0.45,
  "tone": 0.4,
  "width": 0.7,
  "mix": 0.5,
  "modes": [ /* 4 snapshots, one per mode */ ]
}
```

Mode enum: `0=CLASSIC, 1=WARPED, 2=CLEAR, 3=ENSEMBLE`. Default mode WARPED (`1`), default `active` false.

Also `lockedPostChorus` for the live POST-lock snapshot, mirroring tremolo.

Do **not** add MIDI channel or PLAY mode keys here — those belong to midi-control / play-vs-build.

## Chorus DSP binding (copyable, public)

Header-only. No donor/product names in comments or UI. No local measurement notes in the repo.

Shared: stereo delay lines, interpolated reads, RATE/DEPTH map per mode, TONE = one-pole LPF on the wet bus (CCW dark), WIDTH always spreads the wet, MIX equal-power dry/wet. No tempo sync. `Reset()` on active→inactive and on mode change (same contract as Delay/Tremolo). MIX=0 is bit-identical passthrough. Output finite and bounded.

| Mode | Signal flow |
| --- | --- |
| **CLASSIC** | Short modulated delay (~5–8 ms base). Triangle LFO. Linear interpolation. No feedback. WIDTH = opposite-phase LFO on R (stereoizer on a historically-mono flavor). RATE ~0.1–8 Hz. |
| **WARPED** | Longer base (~12–22 ms). Sine LFO. Darker TONE range. MIX at 1.0 is 100% wet (vibrato). WIDTH = small L/R rate offset. RATE maps slower/deeper than CLASSIC. |
| **CLEAR** | Two interpolated taps, ~90° LFO phase, no feedback, transparent interpolation. WIDTH = L/R delay offset. |
| **ENSEMBLE** | Three taps at 0°/120°/240° from shared delay buffers (expired US4038898A / US4384505A family; DaisySP MIT multi-tap chorus is the open-source shape). Denser, less cyclic. |

Public literature only: Dattorro JAES 1997 Part 2; JOS PASP chorus chapter; those two expired patents; MIT DaisySP multi-tap chorus.

## Motif

Copy Throat from `.scratch/release-1.3.0/chorus-motif/index.html` into `DrawEffectMotif`: near mouth large teal ellipse, far mouth small gold ellipse + core, latitude ellipses, straight generators with ~0.55 rad twist. Same drawing at Quiet ~20 px and card size; gold/glow only when `min(w,h) > 40`. Quiet label stays **CHORUS**. One glyph for all four modes.

## Tests (gate)

- Freeze: writer prefix stays 93 even when a test pretends `kNumParams` is larger. A 1.2.2-shaped reader loads extra JSON keys with selection intact and `pos` at `size-4`. A 1.3.0-shaped reader loads a 1.2.2-shaped chunk. pluginval is not this coverage.
- DSP: every mode; MIX=0 passthrough; bounded; no NaN; `Reset()` on bypass edge.
- UI: four POST cards; four-mode picker; tab/arrow reaches Chorus.
- State: per-amp + POST lock + chunk JSON + user-settings round-trip (extend the exhaustive `VoLumAmpSettings` pin).

Prove each new test fails with the freeze/chorus fix reverted, then passes. Windows suite.

## Docs

Changelog one line per user-visible change (freeze is internal — skip unless user-facing). EN/DE user guides: POST Chorus paragraph + four modes. Refresh `docs/user-guide-post.png` and add a chorus shot if the POST strip visibly grew to four cards. Screenshot recipes: `.scratch/release-1.3.0/chorus-motif/index.html` is throwaway; product shots are the standalone app.

## Out of scope

POST reorder. Per-mode motif variants. NAMCore. AudioDSPTools except the Unicode WAV work already landed.
