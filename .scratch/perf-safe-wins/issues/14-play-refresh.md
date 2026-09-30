# 14 PLAY refresh: compare before dirty, build once, field-wise AmpSettingsEqual

Status: resolved
Blocked by: 13
Report: U4 (ui-04, bg-08 a/c, bg-09). Each idle tick in PLAY rebuilds slots twice, choices once, scans 8
params by name, builds two nlohmann JSON trees for the "(unsaved)" test, and dirties the full window.
The same JSON compare runs on every UI param event in BUILD.

## Paths
- `NeuralAmpModeler/VoLumPlayRuntime.inc.cpp` (`_VolumRefreshPlaySurface` ~L164-255)
- `NeuralAmpModeler/VoLumPlaySurface.h` (`SetData` ~L276-326)
- `NeuralAmpModeler/VoLumPlayModel.h` (`BuildPlaySlots`, `BuildSoundChoices`, `PlaySlot` / `SoundChoice`)
- `NeuralAmpModeler/VoLumAmpSettingsJson.h` (`AmpSettingsEqual` ~L113-120)
- `NeuralAmpModeler/VoLumSettingsPresets.inc.cpp` (`_VolumLivePresetDirty` ~L340-360)
- Tests: `test_volum_user_settings_io.cpp` (equivalence), PLAY model tests

## Change
1. `SetData` compares new slots/choices/fx with the current ones (`operator==` on `PlaySlot` /
   `SoundChoice`) and dirties only on a change.
2. Build slots once per tick and pass them to both `SetData` and `SoundIsAssigned`.
3. Index the 8 bypass params directly (`kPrePitchActive`, ... `kTremoloActive`); pin the mapping against
   `kPlayBypassParamNames` with a test.
4. Compute `_VolumLivePresetDirty` once per tick; drop the duplicate `_VolumSaveCurrentToSettings`.
5. Replace `AmpSettingsEqual`'s two JSON trees with a field-wise compare: exact `==` on doubles (what the
   JSON compare does), string compares for the id fields, and the same fields skipped as today.
   Guard it with a doctest that reuses the exhaustive every-field round-trip fixture and asserts
   `NewEqual(a,b) == JsonEqual(a,b)` for a mutation of **every** field; keep the JSON compare in the test as
   the oracle so a new codec field fails the test until the field-wise compare learns it.
Do not cache the dirty bit (that variant is Needs look).

## Acceptance
- Equivalence doctest green; `(unsaved)` and `+` states behave as before (existing PLAY / preset tests).
- PNG diff vs baseline = 0 changed pixels in PLAY (idle, after a recall, after a knob change).
- `_VolumRefreshPlaySurface` time per tick logged before/after (steady_clock scope, removed before commit).

## Proof
Equivalence doctest red against a field-wise compare with one field deliberately left out; SetData
"unchanged input does not dirty" red against the old `SetData`.

## Verifier conditions
Field list covers every `VoLumAmpSettings` field the JSON codec writes; PLAY still repaints for its
animation through `Tick()` (only the extra `SetData` dirty goes away).

## Result (2026-09-29, round 2)
- 1: `SetData` takes prebuilt slots/choices and dirties only via `AssignIfChanged` (explicit `operator==` on
  `PlaySlot`/`SoundChoice`; C++17 for the mac plugin build) or a scroll/press-state change.
  2: refresh builds slots once, `SoundIsAssigned` on the same vector, owner key once.
  3: `kPlayBypassParams` indexed; pinned against each `InitBool` name in `NeuralAmpModeler.cpp`.
  4: `_VolumLivePresetDirty` already ran once per tick; the duplicate `_VolumSaveCurrentToSettings` is OnIdle's own
  save in `NeuralAmpModeler.cpp` (not in this ticket's paths; would reorder OnIdle) -> left, follow-up.
  5: field-wise `AmpSettingsEqual`; oracle doctest mutates every codec key and snapshot field from default and
  from the exhaustive fixture (both directions), asserts agreement and that each mutation is seen.
- Red: `postChorusMix` left out -> oracle case red on exactly that key; `AssignIfChanged` always-true and old
  unconditional `SetDirty` -> both PLAY cases red (`evidence/14/red.log`). One source lock
  (`test_volum_ui_regressions` "two views of one Sound map") moved from `VoLumPlaySurface.h` to the runtime.
  Green: suite 1193 passed, 1 skipped.
- Pixels: 13 states vs `baseline-r2/` 0 changed (`evidence/14/png-diff.log`).
- Probe: `_VolumRefreshPlaySurface` median 436-528 us / 1825 allocs -> 53 us / 15 allocs per tick.

- Coordinator note (2026-09-30): resolved without change item 4 (duplicate OnIdle save in NeuralAmpModeler.cpp,
  outside this ticket's paths); carried to 1.3.1. Verifier nit taken: field-count lock + per-field mutation
  test on SoundChoice / PlaySlot operator== in test_volum_play.cpp.

## Parked (owner, 2026-09-26 22:00)
Budget: lean plan. Not in this loop; stays specified for a later effort.
