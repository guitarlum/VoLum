No files were modified. Findings below, ranked.

---

## 1. BLOCKER — Tuner stays active when the editor closes, silencing the plugin forever

**WHERE:** `NeuralAmpModeler/NeuralAmpModeler.cpp:1204-1211`, `NeuralAmpModeler/VoLumSceneRig.inc.cpp:5-21`, `NeuralAmpModeler/NeuralAmpModeler.cpp:669-671`

**MECHANISM:** Tuner activation lives entirely in the editor's toggle path:

```5:21:NeuralAmpModeler/VoLumSceneRig.inc.cpp
void NeuralAmpModeler::_ToggleVoLumTuner()
{
  if (auto* pGfx = GetUI())
  {
    auto* tuner = pGfx->GetControlWithTag(kCtrlTagVoLumTuner)->As<VoLumTunerControl>();
    if (tuner->IsHidden())
    {
      mTunerDSP.SetActive(true);
      tuner->Show();
    }
```

`mTunerDSP` is a plugin member that outlives the editor, and `OnUIClose` never touches it:

```1204:1211:NeuralAmpModeler/NeuralAmpModeler.cpp
void NeuralAmpModeler::OnUIClose()
{
  // Save while params are still valid (destructor may run after teardown)
  _VolumSaveCurrentToSettings();
#ifdef APP_API
  _VolumSaveSettingsToFile();
#endif
}
```

An active tuner mutes the output unconditionally — `MakeProcessingPlan(..., mTunerDSP.IsActive(), ...)` sets `plan.silenceForTuner` (`VoLumProcessingPlan.h:51`), consumed at `NeuralAmpModeler.cpp:669-671` ("Tuner active: silence output so player can tune without hearing amp"). On reopen the tuner control is rebuilt hidden (`VoLumLayoutBuild.inc.cpp:1090` `->Hide(true)`), so `_ToggleVoLumTuner` sees `IsHidden() == true` and there is no visual trace of the latched state; the tuner toolbar button is a plain `NAMCircleButtonControl` with no active state at all.

**TRIGGER:** Open the tuner (toolbar button or `T`), then close the plugin window without dismissing the tuner first — closing the editor while the tuner is up is the natural gesture after tuning.

**IMPACT:** The plugin goes completely silent and stays silent for the life of the instance. Reopening the editor shows a normal UI with no indication. The user's only accidental recovery is opening and closing the tuner again.

**CONFIDENCE:** certain.

**FIX SKETCH:** In `OnUIClose`, `mTunerDSP.SetActive(false);`. Note this changes *when* DSP is deactivated, not any voicing — no coefficient, gain, or routing math is touched. Consider also deactivating `mMetronomeDSP` there, or explicitly documenting that a running metronome is intended to survive a window close (see finding 4).

**TEST COVERAGE:** No. `test_volum_dsp_staging.cpp` / the processing-plan doctests cover `silenceForTuner` given `tunerActive`, but nothing exercises the editor-lifecycle transition; `OnUIClose` has no test at all.

---

## 2. MAJOR — Every disabled control in the Settings page is fully operable

**WHERE:** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1519-1522`; `iPlug2/IGraphics/IGraphics.cpp:1286`; `iPlug2/IGraphics/IControl.h:2191-2194`; `iPlug2/IGraphics/IControl.cpp:762-774`, `819-831`

**MECHANISM:** The build pass ends by granting mouse delivery to disabled controls, globally:

```1519:1522:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
```

That is the only gate iPlug2 has — `IGraphics.cpp:1286` skips a disabled control for mouse events *unless* `GetMouseEventsWhenDisabled()`. With the gate open, the base handlers act because none of them re-check `IsDisabled()`:

- `IEditableTextControl::OnMouseDown` (`IControl.h:2191`) → `GetUI()->CreateTextEntry(...)` unconditionally. This is the parent of `InputLevelControl`, i.e. the input-calibration dBu field disabled at `NeuralAmpModeler.cpp:2208`.
- `ISwitchControlBase::OnMouseDown` (`IControl.cpp:762`) → toggles straight into `SetValue`. This is the "Calibrate input" switch disabled at `NeuralAmpModeler.cpp:2207`.
- `IKnobControlBase::OnMouseDrag` (`IControl.cpp:819`) has no disabled check (its `OnMouseWheel` does), so a greyed knob still tracks the drag.

**TRIGGER:** Open Settings with any bundled rig loaded — VoLum's own rigs carry no `input_level_dbu`, so `canCalibrateInput` is false and both calibration controls are disabled. Click the greyed "Calibrate input" switch, or click the greyed dBu field and type a number.

**IMPACT:** The switch flips and the field accepts a typed value while rendered greyed-out, and the calibration silently does nothing (there is no capture level to align against). The control reads as broken in precisely the way the 1.2.1 explanation text was added to prevent.

**CONFIDENCE:** certain for the code path; the visual dimming is inconsistent too (`InputLevelControl::Draw` fills without `mBlend`, so the field barely dims).

**FIX SKETCH:** Cheapest correct fix is to stop opening the gate for controls that don't need it — restrict the `ForAllControlsFunc` at 1519 to the controls that actually want disabled-state tooltips (match on group/tag), leaving the rest at iPlug2's default. If the global tooltip behaviour must stay, add `if (IsDisabled()) return;` to VoLum's own subclasses (`InputLevelControl::OnMouseDown`, `NAMSwitchControl::OnMouseDown`, `NAMKnobControl::OnMouseDrag`). No audio math involved.

**TEST COVERAGE:** No. `test_volum_input_calibration.cpp` covers the gain arithmetic and `InputCalibrationHelpText`; nothing simulates a click on a disabled control.

---

## 3. MAJOR — Cab-row "No Cab" / "Custom IR" disabled flags leak from a custom amp onto a factory amp

**WHERE:** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:80-129` (sidebar select callback), `NeuralAmpModeler/VoLumSpeakerRow.h:50-68`, `NeuralAmpModeler/VoLumSceneRig.inc.cpp:410-411`, `NeuralAmpModeler/VoLumUiSyncPlan.h:121-122` vs `149-150`

**MECHANISM:** `mNoCabEnabled` / `mIrEnabled` are written in exactly one place, the shared planner applier:

```410:411:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  row->SetNoCabEnabled(plan.noCabEnabled, "No DIRECT capture on this channel");
  row->SetIrEnabled(plan.irEnabled, "Custom IR needs a DIRECT capture");
```

For a custom lane these come from the resolved cab view and can be `false` (`VoLumUiSyncPlan.h:149-150`); for a factory lane the planner always returns `true` ("factory amps always ship a raw DIRECT capture", `:121-122`). But the sidebar amp-select callback is a *parallel* apply path that never runs the planner — it pushes labels only:

```108:110:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
          // Restore the factory cab labels (a custom amp may have overridden them).
          if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
            spk->As<VoLumSpeakerRowControl>()->SetFactoryCabs();
```

`SetFactoryCabs()` (`VoLumSpeakerRow.h:81-87`) writes `mCabNames` and nothing else. `_VolumRestoreFromSettings` → `_VolumApplyAmpSettings` covers `SetSelected` (`VoLumSettingsScene.inc.cpp:297`) and the IR chip (`:303`), and `_VolumRefreshChannels` covers the stepper (`VoLumSceneRig.inc.cpp:71-75`) — so those three are fine. The two enabled flags are the gap, and they gate the clicks: `OnMouseDown` at `VoLumSpeakerRow.h:252` (`if (mIrEnabled && mIrMenuCb)`) and `:261-262` (`const bool emptySlot = (i == 0) ? !mNoCabEnabled : ...`).

**TRIGGER:** Select a custom amp (F6) whose resolved channel has no DIRECT capture, so the row greys "No Cab" and/or the copper "Custom IR" chip. Then click any factory amp in the sidebar (or arrow-key to one — same callback).

**IMPACT:** On a factory amp, which always supports both, the "No Cab" button and/or the "Custom IR" chip stay rendered dead (`VoLumSpeakerRow.h:111-117`, `:178-188`) and clicks are silently swallowed, with the stale tooltip "Custom IR needs a DIRECT capture". Recovery requires a path that runs the planner: re-select a custom amp, toggle dual-amp focus, or reopen the window.

**CONFIDENCE:** certain about the missing writes; likely about reachability, which depends on a custom amp with no DIRECT capture on its resolved channel — the state the planner's own `irEnabled=false` branch exists to describe.

**FIX SKETCH:** Replace lines 108-110 with a call to `_VolumApplyFocusedLaneCabs()` after `_VolumRefreshChannels()`. That routes this path through the same planner as every other apply path and makes the divergence structurally impossible, which is the real lesson of the headline bug.

**TEST COVERAGE:** No, and this is the coverage blind spot worth naming. `test_volum_ui_sync_plan.cpp` tests `MakeUiSyncPlan` in isolation and it returns the *correct* `true/true` for factory lanes; the bug is that nobody calls the applier on this path. The source-string pins in `test_volum_ui_regressions.cpp` only assert that some call exists somewhere in the file.

---

## 4. MAJOR — Metronome toolbar button reads "off" while the metronome is clicking

**WHERE:** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1303-1313`, `NeuralAmpModeler/VoLumTunerMetronomeOverlay.h:179`, `:193`

**MECHANISM:** The button's active state has exactly one writer, the panel's change callback:

```1305:1309:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
        metCtrl->mOnActiveChanged = [pPlugin](bool active) {
          pPlugin->mMetronomeDSP.SetActive(active);
          if (auto* btn = pPlugin->GetUI()->GetControlWithTag(kCtrlTagVoLumMetronomeButton))
            btn->As<VoLumMetronomeButtonControl>()->SetActive(active);
        };
```

The button is constructed with `bool mActive = false;` (`VoLumTunerMetronomeOverlay.h:193`) and dims itself from it (`float alpha = mActive ? 1.f : 0.35f;`, `:179`). Neither `_BuildVoLumLayout` nor `_VolumSyncUiFromState` — "the one place restored backend state becomes visible control state" (`VoLumAmpMenus.inc.cpp:289-292`) — ever pushes `mMetronomeDSP.IsActive()` into it. `mMetronomeDSP` survives editor close (finding 1), and the panel itself gets it right on open (`VoLumSceneRig.inc.cpp:30-31` passes `mMetronomeDSP.IsActive()` into `Show`), which is what makes the toolbar disagreement visible.

**TRIGGER:** Start the metronome, close the plugin window, reopen it.

**IMPACT:** The click is audible but the toolbar metronome button is drawn at 35% alpha, i.e. off. Opening the panel shows ON, so the two surfaces contradict each other. This is the same build-default-vs-apply-pass divergence as the headline 1.2.1 bug, in a control that the sync pass doesn't know about.

**CONFIDENCE:** certain.

**FIX SKETCH:** Add to `_VolumSyncUiFromState()`: push `mMetronomeDSP.IsActive()` into `kCtrlTagVoLumMetronomeButton`. Two lines, no audio impact.

**TEST COVERAGE:** No. `_VolumSyncUiFromState` is only pinned by source-string checks that name the cab row and hero.

---

## 5. MAJOR — The preset bar is clickable straight through the Settings page and the tuner overlay

**WHERE:** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1081-1101`

**MECHANISM:** Attach order is z-order, and the F5 preset bar is attached *after* both modal overlays:

```1081:1101:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
        ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, ...), kCtrlTagSettingsBox)
        ->Hide(true);
      // Tuner overlay (on top of everything)
      {
        auto* tunerCtrl = new VoLumTunerControl(b);
        tunerCtrl->SetDismissAction([pPlugin]() { pPlugin->mTunerDSP.SetActive(false); });
        pGraphics->AttachControl(tunerCtrl, kCtrlTagVoLumTuner)->Hide(true);
      }
      // F5 preset bar — centred in the top header band ...
        pGraphics->AttachControl(
          new VoLumPresetBarControl(presetBarArea, [pPlugin]() { pPlugin->_VolumShowPresetMenu(); }),
          kCtrlTagVoLumPresetBar);
```

Both overlays block the rest of the UI with a full-window backdrop (`VoLumSettingsBackdropControl`, constructed over `GetRECT()` with `mIgnoreMouse = false`), but a backdrop only shields controls *below* it. Nothing hides the preset bar when either overlay opens — `NAMSettingsPageControl::HideAnimated` only walks its own children. The bar occupies a 240×28 rect at the top centre (`:1096-1097`), squarely inside both overlays.

**TRIGGER:** Open Settings (gear or `H`), or open the tuner, then click the preset name / `<` / `>` at the top centre.

**IMPACT:** The preset bar draws on top of the dimmed overlay and stays live: clicking it opens the preset dropdown over the Settings page, and `<` / `>` recall presets underneath an open modal — changing amp, cab, and effect state while the user is looking at a settings dialog. The tuner case is worse, since a preset recall while the tuner has the output silenced gives no audible feedback.

**CONFIDENCE:** certain for the z-order and the absence of any hide; likely for the exact visual overlap, which I derived from the geometry rather than a screenshot.

**FIX SKETCH:** Attach the preset bar before `kCtrlTagSettingsBox` (it has no dependency on either overlay), or hide it in `HideAnimated`/`VoLumTunerControl::Show`. Attach-order is the smaller change and fixes both overlays at once. Note the metronome panel (attached at `:1313`) is correctly above the bar.

**TEST COVERAGE:** No. No test asserts relative z-order or overlay input capture.

---

## 6. MAJOR — Opening the editor can silently drop an IR, dirty the preset, and send a parameter change to the host

**WHERE:** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:416-417` and `673-711`, reached from `NeuralAmpModeler.cpp:1154`

**MECHANISM:** `OnUIOpen` ends with `_VolumSyncUiFromState()` → `_VolumApplyFocusedLaneCabs()` → the planner, and the applier acts on `clearOrphanedIr` with a full state mutation:

```416:417:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  if (plan.clearOrphanedIr)
    _VolumClearIR(support, /*deferToCabSwap=*/true);
```

`_VolumClearIR` is not a UI-only reconcile. It clears the scene's `activeIrId`, then:

```703:710:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  GetParam(toggle)->Set(0.0);
  SendParameterValueFromDelegate(toggle, GetParam(toggle)->GetNormalized(), true);
  ...
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
```

`plan.clearOrphanedIr` is true whenever the scene stores an IR id that no longer resolves in the library (`VoLumUiSyncPlan.h:133`, `:167`) — the deliberate design, but the applier runs it as a side effect of *opening a window*.

**TRIGGER:** Save a preset with a custom IR active, delete or move that IR file, then open the plugin window. Also fires for a custom lane whose resolved channel lost its DIRECT capture.

**IMPACT:** Merely opening the editor mutates saved state: the preset is marked dirty with no user action, and `kIRToggle` is reported to the host from `OnUIOpen`. In a DAW that treats delegate parameter changes as edits, this dirties the project; with automation in write mode it can write an automation point. The user never asked for anything.

**CONFIDENCE:** certain for the code path; likely for the host-side automation consequence, which is host-dependent.

**FIX SKETCH:** Split the applier: on the editor-open path, reconcile the *chip* (`SetIrCab(false, "")`) and defer the destructive clear to the next real user edit or to a load-time migration that runs once outside `OnUIOpen`. Minimum viable change: pass a flag so `_VolumSyncUiFromState`'s call site skips `_VolumMarkPresetDirty()` / `SendParameterValueFromDelegate`. Do not change the deferred-removal DSP timing (`deferToCabSwap`) — that guards against a cab-less burst.

**TEST COVERAGE:** Partly, in the wrong place. `test_volum_ui_sync_plan.cpp` verifies the pure `clearOrphanedIr` flag; no test covers the applier's side effects or asserts that opening an editor leaves the preset clean.

---

## 7. MINOR — Input-calibration controls keep a stale enabled/greyed state when no model is loaded

**WHERE:** `NeuralAmpModeler/NeuralAmpModeler.cpp:1145-1148`, `2188-2215`, `1001-1008`

**MECHANISM:** Every calibration-availability write lives behind two null guards:

```1145:1148:NeuralAmpModeler/NeuralAmpModeler.cpp
  if (mModel != nullptr)
  {
    _UpdateControlsFromModel();
  }
```

and `_UpdateControlsFromModel` itself returns at `:2190` on a null model, so `SetDisabled(!canCalibrateInput)` (`:2207-2208`) and `SetInputCalibrationAvailable` (`:2209`) never run. The build-time state disagrees: the help label is constructed with `InputCalibrationHelpText(false)` (`NeuralAmpModelerControls.h:1140`), i.e. "This model has no capture level", while the switch at `:1135` is attached with no `SetDisabled(true)` — enabled. The clear path is worse: `mModelCleared` → `ClearModelInfo()` (`NeuralAmpModeler.cpp:1005`) only blanks the sample-rate row (`NeuralAmpModelerControls.h:735-739`), leaving the previous model's help text and enabled state in place.

**TRIGGER:** Open Settings with no model loaded (missing rigs folder, failed load), or load a model that carries `input_level_dbu`, then clear it.

**IMPACT:** In the no-model case the switch and dBu field look live while the help line says there is nothing to calibrate against. After a clear, the help line still reads "Aligns your interface level to the capture" for a model that is gone. Combined with finding 2, the controls are also operable.

**CONFIDENCE:** certain.

**FIX SKETCH:** In `OnUIOpen`, drop the outer `if (mModel != nullptr)` and let `_UpdateControlsFromModel` handle null by pushing `available = false` (disable both controls, set the help text) instead of returning early. Have `ClearModelInfo` do the same.

**TEST COVERAGE:** No. `test_volum_input_calibration.cpp` covers `InputCalibrationHelpText(bool)` and the gain math, not who calls it or when.

---

## 8. MINOR — Latency readout keeps a stale value after the model is cleared

**WHERE:** `NeuralAmpModeler/NeuralAmpModelerControls.h:735-739`

**MECHANISM:** `ClearModelInfo` clears one of the three populated rows:

```735:739:NeuralAmpModeler/NeuralAmpModelerControls.h
  void ClearModelInfo()
  {
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.sampleRate))->SetStr("");
    mHasInfo = false;
  };
```

`currentLatency` and `latencyDetail` (`:768`, `:773`) keep whatever `SetCurrentLatency` last wrote. Since the plugin's PDC depends on the loaded model's own latency (`NeuralAmpModeler.cpp:638-641` reads `mModel->GetLatency()`), the headline sample count goes stale.

**TRIGGER:** Load a model, open Settings to see the latency, clear the model, reopen Settings.

**IMPACT:** "Plugin latency: X ms (N samples)" for a model that is no longer loaded, next to a blank sample-rate row. Wrong number presented with the same authority as a right one — the specific failure mode the file header of `VoLumLatencyReport.h` argues against.

**CONFIDENCE:** certain.

**FIX SKETCH:** Blank both latency rows in `ClearModelInfo`, or call `_UpdateLatency()` on the `mModelCleared` branch at `NeuralAmpModeler.cpp:1001-1008`.

**TEST COVERAGE:** No. `test_volum_latency_report.cpp` doctests `FormatLatencyLines` (which is correct); nothing covers label lifecycle.

---

## 9. MINOR — The 1.2.1 latency detail line can still clip, and clips first on Windows

**WHERE:** `NeuralAmpModeler/NeuralAmpModelerControls.h:1066-1068` and `766-773`, `NeuralAmpModeler/VoLumLatencyReport.h:100`, `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:26-34`

**MECHANISM:** The model-info column is a fixed fraction of the panel: `metaRow` is 64 px tall (`:1066`) and `modelColW = metaRow.W() * 0.46f` (`:1067`). At the default 900×600 window that is a ~326 px wide box. The longest detail string is the standalone no-driver case:

```100:100:NeuralAmpModeler/VoLumLatencyReport.h
    std::snprintf(detail, sizeof(detail), "buffer %d; driver reports none, real round trip is higher", r.bufferFrames);
```

That is 57 characters with a 3-digit buffer, drawn at 12 px `EAlign::Near` into the ~326 px row created at `:773`. Two things make the margin thinner than it looks: a 4-digit buffer ("buffer 1024") adds a character, and on Windows the font names are deliberately remapped one weight heavier —

```26:34:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
#ifdef OS_WIN
    // NanoVG/GL2 on Windows renders these small Josefin caps thinner than macOS/Metal,
    // so load one weight heavier there to match the macOS readability.
    pGraphics->LoadFont("Josefin-Sans", JOSEFINSANS_BOLD_FN);
```

so the "Josefin-Sans" detail line is actually a bold face on the primary platform, several percent wider than what a macOS eyeball check would show. Vertically the block needs 20 + 14 + 14 + 14 = 62 px of the 64 px available, so there is no room to add a line later.

**TRIGGER:** Standalone on Windows with a driver that reports no latency (WASAPI), at a 1024-frame buffer, Settings open.

**IMPACT:** The tail of the caveat line is cut off — the same failure the two-line split was introduced to fix, one string longer. The 1.2.0 bug this replaced also clipped its own closing bracket, so the box boundary does clip in practice.

**CONFIDENCE:** likely. The arithmetic puts the string within a few percent of the box width; I derived glyph advances from font metrics rather than measuring a render, so the exact platform and buffer size where it tips over is uncertain. Worth one screenshot on Windows/WASAPI at 1024 frames before the release.

**FIX SKETCH:** Shorten the no-driver detail to fit with margin ("buffer %d; driver reports none — true round trip is higher" is no better; drop the buffer, which already appears in the headline case, or use "driver reports no latency; real round trip is higher"). Do not widen `modelColW` — the About block on the right needs its four lines.

**TEST COVERAGE:** No, and it is not really testable as written. `test_volum_latency_report.cpp` asserts wording, which cannot see a pixel width. A cheap guard is a doctest asserting `detail.size()` stays under a documented character budget.

---

## 10. MINOR — Custom IR chip label byte-truncates UTF-8 and overflows its text area

**WHERE:** `NeuralAmpModeler/VoLumSpeakerRow.h:304-309`, `:203-205`

**MECHANISM:**

```304:309:NeuralAmpModeler/VoLumSpeakerRow.h
  std::string TruncatedIr() const
  {
    if (mIrName.size() <= 12)
      return mIrName;
    return mIrName.substr(0, 11) + "\u2026";
  }
```

`substr(0, 11)` counts bytes, so an IR name with any multi-byte character can be cut mid-codepoint. The result is drawn centred at 11 px into `IRECT(btn.L + 22.f, ..., btn.R - 4.f)` — 78 px of the 104 px button (`:204-205`) — and the ≤12-char early return is right at that limit for capital-heavy names, in the Windows-bold "Josefin-Bold" face.

**TRIGGER:** Activate a custom IR whose filename contains a non-ASCII character (umlaut, accent), or one that is exactly 12 wide characters.

**IMPACT:** A replacement glyph or garbled tail in the copper chip, or a 12-character name overflowing centred into the IR glyph at `btn.L + 9..22`.

**CONFIDENCE:** likely for the overflow (font-metric dependent), certain for the UTF-8 split.

**FIX SKETCH:** Truncate on a codepoint boundary (back off while `(byte & 0xC0) == 0x80`) and lower the threshold to 10. `NormalizeCabName` has the same byte-wise assumption.

**TEST COVERAGE:** No; `TruncatedIr` is private and untested.

---

## NITs

- `VoLumModePickerControl::Draw:123` recovers the selected index with truncation, `int(GetValue() * (mModes.size() - 1))`, while `OnMouseOver:161` clamps — and `mModes.size() == 1` divides by zero at `:149`/`:153`. All current pickers have ≥2 modes, so this is latent only.
- `VoLumSpeakerRow.h:100-102` centres a hardcoded 360 px of buttons in `mRECT`; any layout change that narrows the row below 360 px silently overflows both edges rather than reflowing.
- `IsPostBlockParam` (`NeuralAmpModeler.cpp:1418-1440`) omits every tremolo param plus `kDelaySync`/`kDelayDivision`, so `_VolumRefreshPrePostLockChrome` misses those edits — harmless in practice because `OnIdle:905-917` recomputes `_VolumIsPostDirty()` live and dirties the triptych, but the two lists should agree.
- `VoLumLayoutBuild.inc.cpp:119-120` computes `preActive` without `kPrePitchActive`, diverging from `_UpdateVoLumLayout`; no impact today only because `VoLumTriptychControl::SetState` discards both flags (`VoLumTriptych.h:112-113`) — dead parameters that invite a real bug later.
- `SetModelInfo` (`NeuralAmpModelerControls.h:780-790`) streams the sample rate as a bare `double`, so any value above 6 significant digits would render in scientific notation; unreachable at real sample rates.
- `_VolumRefreshChannels` (`VoLumSceneRig.inc.cpp:42-43`) returns early on an empty `mVolumRigsRoot`, leaving the channel stepper showing the previous amp's labels when the rigs folder is missing.

---

**Checked and cleared** (so the parent knows the ground is covered): the triptych resets every hit-test rect at the top of `Draw` (`VoLumTriptych.h:76-87`), so no stale header/lock/store zones; `kEffectFocusCount` matches the enum, so the motif-layer array cannot be indexed out of bounds; `mSlotMotifLayers` is keyed and invalidated on variant and bypass changes; the planner's `clearOrphanedIr` runs *before* the `sidebarCustomIdx < 0` early return, so factory lanes are not skipped; `mVolumAmpIdx` is clamped in `Unserialization.cpp` before any `volum::kAmps[]` access; the cab row correctly gates its own clicks on `mIrEnabled`/`mNoCabEnabled`; the channel stepper and IR chip *are* synced on the sidebar amp-switch path; and `PLUG_HOST_RESIZE 0` with an aspect-locked corner resizer rules out the off-aspect overlap class entirely.