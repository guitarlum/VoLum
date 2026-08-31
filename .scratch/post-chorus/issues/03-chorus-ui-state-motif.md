# Chorus params, POST card, Throat motif, state

Status: ready-for-agent
Blocked by: 01, 02

## Goal

Fourth POST card, automatable `EParams` (indices after 92), id-tail JSON overlay, per-amp + POST lock + user-settings, Throat motif.

## Params (append before `kNumParams`)

`kChorusActive, kChorusMode, kChorusRate, kChorusDepth, kChorusTone, kChorusWidth, kChorusMix`

Mode labels in UI: **CLASSIC / WARPED / CLEAR / ENSEMBLE**. Default mode WARPED, default bypassed.

Saved values live in id-tail JSON (`cho` / `lockedPostChorus`), **not** as extra prefix doubles. On load: apply the 93 prefix, then overlay JSON onto these params and per-amp scenes.

Extend `VoLumAmpSettings`, `WriteAmpCoreBlock` / POST block helpers, `PreBlockEquals`/`PostBlockEquals` as needed, `test_eparam_order.cpp`, `test_keyboard_steps.cpp`, exhaustive user-settings pin, chunk codec + state round-trip.

## UI

POST strip goes from three equal peers to four (PRE already fits four). Visual order matches the bus. Tab/arrow cycling reaches all four. Mode picker uses `DrawVoLumSelection`. Knob row RATE · DEPTH · TONE · WIDTH · MIX. No extra per-mode knob. No reorder.

Throat motif: copy construction from `.scratch/release-1.3.0/chorus-motif/index.html` into `VoLumTriptychMotifs.h`. Quiet label **CHORUS**. Gold/glow only when `min(w,h) > 40`.

Launch standalone (`run-app-win.ps1`) and iterate until the card matches the mock (Throat selected). Four cards must fit at 900×600 without clipping the knob row.

## Tests (must fail with this ticket reverted)

- `kNumParams` grew; prefix writer still emits 93.
- Chunk JSON round-trips Chorus; a 1.2.2-shaped chunk loads with Chorus at defaults.
- Four POST cards + four-mode picker source/layout pins.
- POST lock + Store include Chorus.

## Done when

UI judged in the standalone app. Revert-fail proven. Changelog + user guides are ticket 04.
