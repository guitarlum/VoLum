## Summary

This control layer is generally defensive about clipping, empty preset banks, disabled speaker slots, and editor-reopen preset dirtiness, but keyboard parity is incomplete. The highest-severity path is the `S` speaker shortcut: unlike the mouse speaker-row path, it neither retires an active Custom IR nor resolves custom-amp cab slots, so it can produce a visually stale selection and can run a baked-cab NAM through the still-active Custom IR. I found four MAJOR and three MINOR defects; all are proved by reachable code paths rather than runtime speculation.

## Findings

### F-P7-1: MAJOR — Keyboard cab cycling leaves Custom IR active and can double-cab the signal

**Where:** `NeuralAmpModeler/VoLumKeyboard.inc.cpp:31-32,189-223`; contrast `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:244-264` and `NeuralAmpModeler/VoLumSpeakerRow.h:262-268`.

**Evidence:**

```cpp
// NeuralAmpModeler/VoLumKeyboard.inc.cpp:31-32
if (key.VK == 's' || key.VK == 'S')
  return _CycleVoLumKeyboardSpeaker(key.S ? -1 : 1);
```

```cpp
// NeuralAmpModeler/VoLumKeyboard.inc.cpp:195-223
if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
{
  const int current = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, kSpeakerCount - 1);
  const int next = (current + direction + kSpeakerCount) % kSpeakerCount;
  GetParam(kSupportSpeakerIdx)->Set(next);
  SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
  mVolumSettingsDirty = true;
  _VolumRefreshSupportChannels();
  mVolumSupportNeedsLoad.store(true);
}
else
{
  const int current = std::clamp(mVolumSpeakerIdx, 0, kSpeakerCount - 1);
  const int next = (current + direction + kSpeakerCount) % kSpeakerCount;
  mVolumSpeakerIdx = next;
  mVolumAmpSettings[mVolumAmpIdx].speakerIdx = next;
  mVolumSettingsDirty = true;
  _VolumRefreshChannels();
  mVolumNeedsLoad.store(true);
}
```

```cpp
// NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:244-264 (mouse path)
const std::string& laneIrId =
  supportFocus ? _VolumActiveScene().supportActiveIrId : _VolumActiveScene().activeIrId;
if (!laneIrId.empty())
{
  // ...
  _VolumClearIR(supportFocus, /*deferToCabSwap=*/captureChanges);
}
```

**Mechanism:** The keyboard path changes the speaker index and requests a NAM reload but never calls `_VolumClearIR`. The scene's `activeIrId`/`supportActiveIrId` and `kIRToggle`/`kSupportIRToggle` therefore remain active. The model loader stages the newly selected baked-cab NAM while the processing plan continues to run the Custom IR convolver. `VoLumSpeakerRowControl::SetSelected()` also leaves `mIrCabActive` untouched, so the chip continues to display the Custom IR.

**Trigger:** Select a Custom IR on MAIN or SUPPORT, expand AMP, then press `S` or `Shift+S`.

**Impact:** The shortcut can produce a baked speaker capture followed by a Custom IR (double cabinet coloration) while the UI still says only the Custom IR is selected. This is a control defect, not a voicing change.

**Fix sketch:** Route keyboard cab cycling through the same lane-aware cab-selection transaction as mouse clicks, including `_VolumClearIR(..., captureChanges)`, capture staging, scene persistence, and full row reconciliation.

**Proposed regression test:** `keyboard_speaker_cycle_retires_active_ir_before_baked_cab_load` — after cycling from an active IR, assert the lane IR id is empty, the lane IR toggle is false/deferred-off as appropriate, the row's IR chip is inactive, and the requested capture is the selected baked cab.

### F-P7-2: MAJOR — Keyboard cab cycling cannot change custom-amp cabs

**Where:** `NeuralAmpModeler/VoLumKeyboard.inc.cpp:189-223`; `NeuralAmpModeler/VoLumSpeakerRow.h:257-269`; load dependency at `NeuralAmpModeler/NeuralAmpModeler.cpp:816-822` and `NeuralAmpModeler/VoLumLoader.inc.cpp:418-452`.

**Evidence:**

```cpp
// NeuralAmpModeler/VoLumSpeakerRow.h:257-269 (mouse path)
for (int i = 0; i < 4; i++)
{
  const bool emptySlot = (i == 0) ? !mNoCabEnabled : mCabNames[i - 1].empty();
  if (mBtnRects[i].Contains(x, y) && !emptySlot && (i != mSelected || mIrCabActive))
  {
    mIrCabActive = false;
    mSelected = i;
    if (mCallback)
      mCallback(i);
```

```cpp
// NeuralAmpModeler/VoLumKeyboard.inc.cpp:194-213
constexpr int kSpeakerCount = 4;
// ...
const int next = (current + direction + kSpeakerCount) % kSpeakerCount;
// Updates factory speaker state only; no custom slot is changed.
GetParam(kSupportSpeakerIdx)->Set(next);
// ...
mVolumSpeakerIdx = next;
_VolumRefreshChannels();
mVolumNeedsLoad.store(true);
```

```cpp
// NeuralAmpModeler/NeuralAmpModeler.cpp:816-822
if (mVolumCustomMainIdx >= 0)
{
  const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
  const std::string rel = volum::content::CaptureFileFor(
    amp, mVolumCustomMainSlot, mVolumCustomMainChannel);
```

**Mechanism:** Mouse clicks reject unavailable custom slots and the callback maps row index to `mVolumCustomMainSlot`/`mVolumCustomSupportSlot`. The keyboard implementation always cycles a four-item factory index, calls the factory channel refresh, and only pushes that index into the row. Custom model loading, however, reads the unchanged custom `(slot, channel)`, so the visible selection changes while the requested audio capture does not. It can also select an empty/disabled custom cab because the shortcut never checks `mCabNames`, `mNoCabEnabled`, or the lane resolver.

**Trigger:** Focus a custom MAIN or custom SUPPORT amp and press `S`; sparse custom amps make the failure especially obvious.

**Impact:** The keyboard-advertised cab shortcut is nonfunctional for custom amps and can highlight a cab that has no capture while audio remains on the old slot.

**Fix sketch:** Replace the hard-coded four-state cycle with a helper that enumerates enabled row choices for the focused lane and applies the same custom slot/channel resolver used by the mouse callback.

**Proposed regression test:** `keyboard_speaker_cycle_updates_custom_lane_slot` — with a sparse custom amp, assert `S` advances to the next enabled slot, updates the persisted custom slot, requests that slot's capture, and never selects a disabled row.

### F-P7-3: MAJOR — Keyboard lane focus leaves speaker-row state from the previous lane

**Where:** `NeuralAmpModeler/VoLumAmpMenus.inc.cpp:400-431`; trigger path `NeuralAmpModeler/VoLumKeyboard.inc.cpp:104-128`; full-sync path `NeuralAmpModeler/VoLumAmpMenus.inc.cpp:256-286`.

**Evidence:**

```cpp
// NeuralAmpModeler/VoLumKeyboard.inc.cpp:104-128
case EVoLumSection::AMP:
{
  mVolumFocusedEffect = EVoLumEffectFocus::AMP;
  mVolumDualAmpFocusedSupport =
    GetParam(kDualAmpActive)->Bool() ? !mVolumDualAmpFocusedSupport : false;
  break;
}
// ...
_UpdateVoLumLayout();
```

```cpp
// NeuralAmpModeler/VoLumAmpMenus.inc.cpp:419-431
if (auto* spkRow = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
{
  const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane < 0)
  {
    auto* row = spkRow->As<VoLumSpeakerRowControl>();
    const int focusedSpeakerIdx =
      supportFocus ? std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3) : mVolumSpeakerIdx;
    row->SetSelected(focusedSpeakerIdx);
  }
}
```

```cpp
// NeuralAmpModeler/VoLumAmpMenus.inc.cpp:268-286 (complete path)
const bool supportFocus = GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport;
const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
if (customLane >= 0)
{
  _VolumApplyCustomMainCabs(customLane, supportFocus);
  return;
}
// ...
const volum::custom::CustomAmp unusedAmp;
_VolumApplyUiSyncPlan(
  volum::MakeUiSyncPlan(_VolumMakeUiSyncInput(supportFocus, unusedAmp)), supportFocus);
```

**Mechanism:** Mouse lane focus explicitly calls `_VolumApplyFocusedLaneCabs()`. Keyboard focus only calls `_UpdateVoLumLayout()`, which reaches `_VolumApplyDualAmpFocus()`. That function updates only `mSelected` for factory lanes and does nothing at all to the row for custom lanes. It does not update cab names, enabled slots, the Custom IR chip/name, or the lane's channel-stepper state.

**Trigger:** Enable Dual Amp and give MAIN/SUPPORT different cab states (especially a Custom IR or custom amp), then use Tab/Left/Right keyboard navigation to switch lane focus.

**Impact:** The shared row can show and hit-test the previous lane's labels and disabled slots while edits are applied to the newly focused lane. A stale `mIrCabActive` can also suppress every baked-cab selection highlight.

**Fix sketch:** Make every lane-focus transition call `_VolumApplyFocusedLaneCabs()`, or make `_VolumApplyDualAmpFocus()` delegate to the complete UI sync planner instead of partially setting one field.

**Proposed regression test:** `keyboard_lane_focus_fully_reconciles_shared_cab_row` — switch from MAIN with Custom IR to custom SUPPORT and assert cab labels, enabled flags, selected cab, IR chip/name, and channel-stepper selection all equal the SUPPORT plan.

### F-P7-4: MAJOR — Next preset skips index zero when no preset is selected

**Where:** `NeuralAmpModeler/VoLumPresetBar.h:195-205`; reachable after `SetList()`/`SelectName("")` at lines 45-66 and factory reset at `NeuralAmpModeler/VoLumAmpMenus.inc.cpp:21-33`.

**Evidence:**

```cpp
// NeuralAmpModeler/VoLumPresetBar.h:195-205
void Step(int dir)
{
  if (mList.empty())
    return;
  const int n = (int)mList.size();
  const int idx = ((mIdx < 0 ? 0 : mIdx) + dir % n + n) % n;
  if (mRecall)
  {
    mRecall(idx);
    return;
  }
```

```cpp
// NeuralAmpModeler/VoLumPresetBar.h:45-51
mList = names;
mIdx = -1;
mName.clear();
mEmpty = true;
mDirtyEdit = false;
```

**Mechanism:** With `mIdx == -1`, the expression substitutes `0` before applying `dir`. Therefore `Step(+1)` returns index `1`, not index `0` (for any bank with at least two presets). `Step(-1)` correctly wraps to the last item, so the two directions are asymmetrical.

**Trigger:** Have at least two presets, clear the current selection (for example via `Default (factory settings)` or deletion of the active preset), then click `>`.

**Impact:** The control recalls the second preset even though the first preset is the expected forward starting point. This is an immediately audible wrong-selection action.

**Fix sketch:** Handle `mIdx < 0` explicitly: forward starts at `0`, backward starts at `n - 1`; only apply modular stepping when an item is already active.

**Proposed regression test:** `preset_next_from_no_selection_recalls_first_item` — with `mIdx == -1` and three names, assert `Step(+1)` recalls `0` and `Step(-1)` recalls `2`.

### F-P7-5: MINOR — Empty-bank arrows are drawn disabled but still open the preset menu

**Where:** `NeuralAmpModeler/VoLumPresetBar.h:44-53,123-131,160-174`.

**Evidence:**

```cpp
// NeuralAmpModeler/VoLumPresetBar.h:44
// Set the active amp's preset bank (mock). Empty list => "(unsaved)" + inert arrows.
```

```cpp
// NeuralAmpModeler/VoLumPresetBar.h:128-131
const IText arrow(15.f, mList.empty() ? VoLumColors::CREAM_DIM : VoLumColors::GOLD, ...);
g.DrawText(arrow, "<", PrevRect());
g.DrawText(arrow, ">", NextRect());
```

```cpp
// NeuralAmpModeler/VoLumPresetBar.h:160-173
if (!mList.empty() && PrevRect().Contains(x, y))
{
  Step(-1);
  return;
}
if (!mList.empty() && NextRect().Contains(x, y))
{
  Step(1);
  return;
}
if (mOpen)
  mOpen();
```

**Mechanism:** When the list is empty, both arrow conditions are skipped and execution falls through to `mOpen()`. Thus the visually disabled arrow hit areas behave like the center of the bar.

**Trigger:** On an amp with no presets, click either dim `<` or `>` glyph.

**Impact:** A control that is explicitly documented and rendered as inert opens a dropdown instead. This is small but makes hit behavior disagree with the visual affordance.

**Fix sketch:** If the click is in an arrow rect, always consume it; call `Step()` only when the list is non-empty.

**Proposed regression test:** `empty_preset_arrows_are_inert` — click each arrow with an empty list and assert neither recall nor open callback fires; clicking the center should still fire open.

### F-P7-6: MINOR — Custom IR truncation can emit invalid UTF-8

**Where:** `NeuralAmpModeler/VoLumSpeakerRow.h:203-205,304-309`.

**Evidence:**

```cpp
// NeuralAmpModeler/VoLumSpeakerRow.h:203-205
std::string label = mIrCabActive && !mIrName.empty() ? TruncatedIr() : "Custom IR";
g.DrawText(..., label.c_str(), ...);
```

```cpp
// NeuralAmpModeler/VoLumSpeakerRow.h:304-309
std::string TruncatedIr() const
{
  if (mIrName.size() <= 12)
    return mIrName;
  return mIrName.substr(0, 11) + "\u2026";
}
```

**Mechanism:** `std::string::size()` and `substr()` count bytes, not Unicode code points. If byte 11 is inside a multibyte character, the prefix ends with an orphaned UTF-8 lead byte. For example, a name beginning with ten ASCII bytes followed by `é` is split after the first byte of `é`.

**Trigger:** Import/select an IR with a long non-ASCII name whose UTF-8 sequence crosses byte 11 (for example `abcdefghijéxx`).

**Impact:** The active Custom IR chip receives malformed UTF-8 and can display a replacement glyph, omit text, or behave backend-dependently.

**Fix sketch:** Reuse a UTF-8-safe truncation helper that backs up to a code-point boundary before appending the ellipsis.

**Proposed regression test:** `speaker_ir_label_truncation_preserves_utf8` — truncate names with 2-, 3-, and 4-byte characters across the boundary and assert the result is valid UTF-8, ends with one ellipsis, and has no partial code point.

### F-P7-7: MINOR — Calibration text entry sends the same parameter edit twice

**Where:** `NeuralAmpModeler/NeuralAmpModelerControls.h:1290-1295`; pinned iPlug2 behavior in `IGraphics/IControl.cpp:173-220` and `IGraphics/IControl.h:2181-2200`.

**Evidence:**

```cpp
// NeuralAmpModeler/NeuralAmpModelerControls.h:1290-1295
void SetValueFromUserInput(double normalizedValue, int valIdx) override
{
  IControl::SetValueFromUserInput(normalizedValue, valIdx);
  const std::string s = ConvertToString(normalizedValue);
  OnTextEntryCompletion(s.c_str(), valIdx);
}
```

At the pinned iPlug2 revision, `IControl::SetValueFromUserInput()` sets the value and calls `SetDirty(true)`, which sends the parameter to the delegate. The inherited `IEditableTextControl::OnTextEntryCompletion()` then calls `SetDirty(true)` again. The second call is not display-only.

**Mechanism:** Completing a valid calibration text edit enters this override once. The explicit base `SetValueFromUserInput()` sends the new normalized value; the subsequent inherited completion handler sends the identical value again while updating the string.

**Trigger:** In Settings, type a new Input calibration value and confirm it.

**Impact:** Hosts and `OnParamChangeUI` observe two identical UI edits for one user action. This can create redundant automation points and repeats calibration-default dirty processing.

**Fix sketch:** After the single `IControl::SetValueFromUserInput()` call, update the displayed string with `SetStr()` and `SetDirty(false)` instead of invoking `OnTextEntryCompletion()`.

**Proposed regression test:** `input_calibration_text_entry_sends_one_ui_parameter_edit` — instrument the delegate, confirm one text edit, and assert exactly one `SendParameterValueFromUI` for `kInputCalibrationLevel`.

## Voicing observations (report only)

None. No filter curves, effect tuning, drive, envelope, or mix-law changes are proposed. F-P7-1 concerns an unintended control path that composes two cabinet stages, not intended voicing.

## Areas read and found clean

- `NeuralAmpModeler/NeuralAmpModelerControls.h` — read in full. Knob natural/normalized conversion, clamping, wheel accumulation, fine stepping, double-click default behavior, disabled checks, settings visibility, model-info formatting, and control callback ownership were otherwise coherent. The legacy `NAMFileBrowserControl` has no production instantiation in the VoLum layout.
- `NeuralAmpModeler/VoLumCoreControls.h` — read in full. Current mode-picker and sub-mode-pill call sites all provide at least two labels, mapped enum values match their serialization denominators, hover state clears on mouse-out, and selected-slot rounding is correct for those call sites.
- `NeuralAmpModeler/VoLumPedalCardControl.h` — read in full. Placeholder clicks are swallowed, PRE capture menus are only opened from focused PRE NAM cards, cached artwork is invalidated on bypass/motif changes, and footer text is measured and clipped.
- `NeuralAmpModeler/VoLumSpeakerRow.h` — read in full. Draw and hit rectangles share the same stored geometry, mouse selection rejects unavailable DIRECT/cab slots, mouse selection clears the visible IR state before callback, and menu callbacks are synchronous/non-owning.
- `NeuralAmpModeler/VoLumPresetBar.h` — read in full. Empty-list drawing itself is safe, active deletion falls back to `No Preset`, long labels are clipped, save-as trims empty names, and dirty state is explicitly separated from selection.
- `NeuralAmpModeler/VoLumAmpMenus.inc.cpp` — read in full. Preset row codes are independent of conditional row positions, selected checkmarks use item codes rather than visual indices, support custom rows use a collision-free code base, menu height is capped, and support factory/custom selection validates indices.
- `NeuralAmpModeler/VoLumControls.h` — read in full; it is an include-only umbrella with no behavior.
- Preset edge paths traced beyond the assigned files: backend preset IDs make dirty state survive editor close/reopen; deleted active IDs are validated away; normal save/rename paths prevent exact duplicate names, so no duplicate-name finding is reported.
