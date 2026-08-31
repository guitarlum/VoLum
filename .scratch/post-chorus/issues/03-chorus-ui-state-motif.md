# Chorus params, POST card, Throat motif, state

Status: resolved
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

## Result

Seven params appended after index 92 (`kNumParams` 93 -> 100);
`kVoLumChunkParamPrefixCount` stays 93, so the chunk prefix width is unchanged
and 1.2.2 chunks still load. Saved values ride in the id-tail JSON as `cho` per
amp and `lockedPostChorus` for the live POST lock (schema 6). A 1.2.2-shaped
chunk with no chorus keys loads with the card bypassed on WARPED.

POST is four equal peers (`ComputePostCards` plus a third connector); Throat
motif in `VoLumTriptychMotifs.h`; quiet-strip label `CHORUS`; mode picker
CLASSIC / WARPED / CLEAR / ENSEMBLE via `DrawVoLumSelection`; knob row RATE
DEPTH TONE WIDTH MIX. Per-mode knob memory mirrors Delay/Reverb/Tremolo.
Keyboard cycling covers all four cards.

UI judged in the standalone (`run-app-win.ps1`) against
`.scratch/release-1.3.0/chorus-motif/index.html`: four cards fit at 900x600 with
no knob-row clipping, and the wireframe hyperboloid with its gold mouth ellipse
and gold dot matches the mock's THROAT panel. Captured in
`docs/user-guide-chorus.png`.

### Revert-fail proof

**Pass I - integration points, DSP left intact** (5 failed):

| Revert | Test that failed |
| --- | --- |
| `plan.runChorus = false` | `Chorus runs first in the POST chain and is wired to its own params` |
| id-tail drops the per-amp `cho` key | `Id tail round-trips per-amp + locked POST chorus settings` |
| id-tail drops `lockedPostChorus` | (same) |
| POST dirty compare ignores chorus | `POST dirty compare includes chorus params` |
| user settings stop writing `postChorusActive` | `Preset/scene path (AmpSettingsToJson) round-trips 1.2.0 fields struct-direct (non-circular)` |
| `kPostSlots` back to three cards | `POST carries a fourth Chorus card wired to the Throat motif` |

**Pass II - param-order and keyboard claims** (4 failed):

| Revert | Test that failed |
| --- | --- |
| `kVoLumChunkParamPrefixCount = kNumParams` | `Chunk param prefix is frozen at the 1.2.2 count`, `EParam: Chorus params appended past the frozen chunk prefix`, `EParam: total count is stable` |
| chorus knobs dropped from the keyboard step table | `Keyboard step: chorus knobs are all 0..1, so 0.05 normal / 0.01 fine` |
