# Self-review: UI / input layer, `ebb0215..HEAD` (release/1.2.1)

Scope: cab/speaker row, custom-content overlay, keyboard shortcuts, scene/rig UI sync,
numeric entry. Read-only audit; no files edited, no builds run. `dba9ed0` (formatting)
ignored except where reflow hides a pin.

## Verdict

Two of tonight's fixes are wrong in a way a user will hit in the first ten minutes, and
both are pinned by tests that pass with the bug present. **F1**: the double-click guard in
`VoLumCustomOverlay.h` never clears `mOwnsGesture` on mouse-up, so the flag is still `true`
from the click that opened the confirmation dialog — the exact stray double-click the guard
was written to stop still passes it, and "double-click Delete" still deletes the item *and*
recalls an unrelated preset. **F2**: `_HandleVoLumSelectedKnobKey`'s new `IsHidden()` bail
returns `false`, so the arrow key that used to be absorbed by the (hidden) knob now falls
through to the amp-list and channel-stepper handlers below it — one arrow press after a mode
pill click switches the amp or steps the channel, which stages a model load and is audible.
That is a new failure mode created by the fix, and it is worse than the bug it fixed. Both
are small, local, well-understood changes: `mOwnsGesture = false` in an `OnMouseUp` override,
and `return true` instead of `return false`. Everything else I found is medium or below —
notably `_VolumClampSupportFocus()` changing the focused lane without re-deriving the shared
cab row (2 of its 3 call sites), and the identity-based row resolution covering delete and
overwrite but *not* rename. The riskiest change of the night, the `laneFocused` guard in
`_VolumApplyUiSyncPlan`, I could not break: the cache/scene writes stay unconditional, both
per-lane channel steppers are separate controls, and every restore path ends in
`_VolumSyncUiFromState()` → `_VolumApplyFocusedLaneCabs()`. I would not ship without F1 and
F2; the rest can ride.

## Findings

### F1 — `mOwnsGesture` is stale-`true` from the click that opened the dialog, so the guard is inert

- **Severity:** High (the documented repro is unfixed; the consequence is an irreversible
  delete plus an unrelated preset recall)
- **Confidence:** CONFIRMED (traced through `IGraphics::OnMouseDblClick`)
- **Introduced by:** `3dcc1c8` "Stop overlay clicks landing on the wrong row, or on nothing at all"

**Evidence.** The flag is set on every overlay mouse-down and cleared *only* inside the
double-click handler. There is no `OnMouseUp` override in `VoLumCustomOverlayControl`, so
nothing returns it to `false` at the end of a normal click:

```190:193:NeuralAmpModeler/VoLumCustomOverlay.h
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    // Records that this overlay owns the gesture in progress. See OnMouseDblClick.
    mOwnsGesture = true;
```

```284:287:NeuralAmpModeler/VoLumCustomOverlay.h
    const bool ownsGesture = mOwnsGesture;
    mOwnsGesture = false;
    if (!ownsGesture)
      return;
```

```1841:1841:NeuralAmpModeler/VoLumCustomOverlay.h
  bool mOwnsGesture = false; // this overlay received the mouse-down of the current click
```

Now replay the sequence the comment at `VoLumCustomOverlay.h:273-283` describes. The click on
the row's trash icon goes to the overlay (`OnMouseDown` → `mOwnsGesture = true`) and opens the
modal. The modal acts and hides on **mouse-down**:

```95:104:NeuralAmpModeler/VoLumConfirmDialog.h
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (DeleteRect().Contains(x, y))
    {
      auto cb = mOnConfirm;
      Hide(true);
      if (cb)
        cb();
```

Windows then delivers up / dblclick / up. `OnMouseUp` releases the capture, so the dblclick is
hit-tested afresh and — the dialog now hidden — lands on the overlay:

```1186:1201:iPlug2/IGraphics/IGraphics.cpp
  IControl* pControl = GetMouseControl(x, y, true);

  if (pControl)
  {
    ...
      pControl->OnMouseDblClick(x, y, mod);
```

`mOwnsGesture` is still `true` — nobody cleared it since the trash-icon click — so
`if (!ownsGesture) return;` does not fire and the row dispatch runs. The guard only rejects a
double-click when the overlay has never been clicked at all, which is not the failing case.

**Repro.** Manage → Presets at the default window size → single-click the trash icon on any
row → double-click "Delete" in the confirmation. Expected: one delete. Actual: the delete plus
the primary action of whatever row sits under the vanished Delete button (row five at the
default size) — preset recalled / IR selected / pedal loaded — and Manage closes.

**Suggested fix.** Clear the flag when the gesture ends, not when a double-click consumes it:
add `void OnMouseUp(float, float, const IMouseMod&) override { mOwnsGesture = false; }` and
keep the read in `OnMouseDblClick`. (Set it in `OnMouseDown` *after* the early-outs too, so a
click the overlay ignores does not claim the gesture.) The pin in
`test_volum_ui_regressions.cpp:1195-1227` cannot see this — see *Weak pins* below.

### F2 — the first arrow key after the selected knob is hidden switches the amp / steps the channel

- **Severity:** High (unrequested amp switch or channel change; both stage a model load and are
  immediately audible)
- **Confidence:** CONFIRMED
- **Introduced by:** `bb01a44` "Fix custom-amp cab state, lane focus and keyboard cab cycling"

**Evidence.** The new bail-out returns `false`:

```455:460:NeuralAmpModeler/VoLumKeyboard.inc.cpp
      if (pControl->IsHidden())
      {
        mVolumSelectedKnobParamIdx = kNoParameter;
        _UpdateVoLumKeyboardFocusHint();
        return false;
      }
```

`false` means "not handled", and the key-handler chain continues. Up/Down land on amp-list
navigation:

```1422:1426:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
    if (_HandleVoLumSelectedKnobKey(key))
      return true;

    if (key.VK == kVK_UP || key.VK == kVK_DOWN)
    {
```

Left/Right land on the channel stepper. Note that the guard there is already defeated,
because the bail-out cleared `mVolumSelectedKnobParamIdx` two lines earlier:

```1500:1519:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
    if (key.VK == kVK_LEFT || key.VK == kVK_RIGHT)
    {
      if (mVolumSelectedKnobParamIdx != kNoParameter)
        return false;
      ...
            stepper->As<VoLumChannelStepControl>()->StepKeyboard(delta);
```

Before `bb01a44` the hidden knob still received the key (`pKnob->HandleKeyboardInput(key)`
returned `true`), so the press was absorbed: the old bug was "the arrow edits an off-screen
parameter". The new bug is "the arrow reloads a different amp". The upstream handler cannot
save it either — `_HandleVoLumKeyboardFocusKey` returns `false` for Up/Down/Left/Right while a
knob is selected (`VoLumKeyboard.inc.cpp:37-38`).

**Repro.** POST → Tremolo → select the RATE knob with Enter → click the SYNC pill (RATE is
hidden, DIVISION takes its slot; the pill does not clear the selection, as the new comment
says) → press Down. Expected: nothing. Actual: VoLum switches to the next amp in the sidebar.
Press Left/Right instead and the amp's channel steps and a new capture loads.

**Suggested fix.** `return true` — the press is consumed by dropping the selection, which is
the semantics the comment describes ("Drop the selection instead"). Same for the sibling
`return false` at the end of the function (`VoLumKeyboard.inc.cpp:480-481`), which has the
identical fall-through, though that path is only reachable if the param has no knob.

### F3 — `_VolumClampSupportFocus()` changes the focused lane without re-deriving the shared cab row

- **Severity:** Medium (the shared row describes a lane that is not focused — the precise
  condition the `laneFocused` guard was added to prevent — and a click on it then edits the
  focused lane with an index from the other lane's layout)
- **Confidence:** CONFIRMED for the code paths; LIKELY for how often a user reaches them
- **Introduced by:** `bb01a44`

**Evidence.** The clamp silently flips the flag that every row write is now conditioned on:

```397:401:NeuralAmpModeler/VoLumAmpMenus.inc.cpp
void NeuralAmpModeler::_VolumClampSupportFocus()
{
  if (mVolumDualAmpFocusedSupport && !_VolumHasSupportAmp())
    mVolumDualAmpFocusedSupport = false;
}
```

It has three call sites and they disagree about the follow-up. The hero/mouse path gets it
right — clamp, then re-derive:

```378:380:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
      _VolumClampSupportFocus();
      ...
      _VolumApplyFocusedLaneCabs();
```

The keyboard dual-amp toggle does not:

```174:179:NeuralAmpModeler/VoLumKeyboard.inc.cpp
    mVolumDualAmpFocusedSupport = next;
    _VolumClampSupportFocus();
  }

  _UpdateVoLumLayout();
  _UpdateVoLumKeyboardFocusHint();
```

and neither does `_VolumApplyDualAmpFocus()`, which is where the clamp does most of its work
(it runs from `_UpdateVoLumLayout`, i.e. after everything). It only pushes a selection index,
and only for a factory lane:

```409:437:NeuralAmpModeler/VoLumAmpMenus.inc.cpp
  _VolumClampSupportFocus();
  ...
    const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
    if (customLane < 0)
    {
      ...
      row->SetSelected(focusedSpeakerIdx);
    }
```

So when the clamp fires, the row keeps the *unfocused* lane's cab names, `noCabEnabled`,
`irEnabled` and IR chip. The ordering makes it concrete: on a MAIN amp switch,
`_VolumApplyFocusedLaneCabs()` runs while `mVolumDualAmpFocusedSupport` is still `true`
(resolving the row for a SUPPORT lane the new scene does not have — phantom cab, `---`
stepper), and only the later `_UpdateVoLumLayout()` → `_VolumApplyDualAmpFocus()` clamps focus
back to MAIN. Nothing re-derives after that.

**Repro.** Amp A: dual amp on, focus SUPPORT, factory partner selected. Amp B's scene has no
support partner. Switch A → B (sidebar or Up/Down). The cab row keeps amp A's SUPPORT reading
while the hint bar and the arrow keys address MAIN; clicking a cab writes MAIN with that
index.

**Suggested fix.** Make the clamp own its consequence — have `_VolumClampSupportFocus()`
return whether it changed the flag and call `_VolumApplyFocusedLaneCabs()` when it did (it does
not call `_UpdateVoLumLayout`, so there is no re-entrancy), then drop the now-redundant call at
`VoLumLayoutBuild.inc.cpp:380`.

### F4 — rename still resolves the row by index; the identity fix covers delete and overwrite only

- **Severity:** Medium (renames the wrong item across two editors; recoverable, unlike delete)
- **Confidence:** CONFIRMED
- **Introduced by:** `5d6beb1` "Make preset and library operations act on the item you named"
  (incomplete rather than wrong)

**Evidence.** Delete and overwrite both re-resolve at confirm time
(`VoLumCustomOverlay.h:848` and `:872-881`, via `RowIdAt` / `RowIndexById` at `:526-544`).
The rename path does not — it applies `mSel` straight through when the text entry completes,
and `mItems` has not been reloaded either:

```344:356:NeuralAmpModeler/VoLumCustomOverlay.h
      case TextTarget::RenameItem:
        if (mSel >= 0)
        {
          if (!s.empty() && NameTaken(s, mSel))
            SetNameError(s);
          else
          {
            mError.clear();
            ApplyRename(mSel, s);
```

`ApplyRename` forwards the index to index-keyed API calls
(`VoLumCustomOverlay.h:546-555` → `RenameIR`/`RenamePedal`/`RenamePreset`, all
`bank[(size_t)idx].name = name`, e.g. `VoLumCustomContentApi.h:599-611`). The library is
process-global and the text entry does not block other instances, so the same window that
motivated `RowIdAt` applies: a deletion above `mSel` while the field is open makes the rename
land on a different item. `NameTaken(s, mSel)` inherits the same staleness, so the
uniqueness check can exempt the wrong row.

**Repro.** Two plugin instances, Manage → Presets. Instance A: click the pen on "Foo",
type a new name, leave the field open. Instance B: delete a preset above Foo. Instance A:
press Enter. The preset that shifted into Foo's row is renamed.

**Suggested fix.** Capture `RowIdAt(mSel)` in `StartTextEntry` for `TextTarget::RenameItem`
and resolve it with `RowIndexById` at completion, with the same "no longer in your library"
bail-out the delete path uses. Extend the pin at
`test_volum_ui_regressions.cpp:143-168` with `RequireDoesNotContain(overlay, "ApplyRename(mSel, s);")`.

### F5 — the hidden-knob bail-out drops the index but not the selection

- **Severity:** Low
- **Confidence:** CONFIRMED for the code path; LIKELY for the visible effect
- **Introduced by:** `bb01a44`

**Evidence.** The bail-out (quoted in F2) does two of the four things
`_ClearVoLumKnobSelection()` does:

```406:423:NeuralAmpModeler/VoLumKeyboard.inc.cpp
void NeuralAmpModeler::_ClearVoLumKnobSelection()
{
  mVolumSelectedKnobParamIdx = kNoParameter;
  mVolumSelectedKnobHintText.clear();
  ...
      if (auto* pKnob = dynamic_cast<NAMKnobControl*>(pControl))
        pKnob->SetSelectedForKeyboard(false);
  ...
    _HideVoLumExactEntry();
```

The hidden knob therefore keeps `SetSelectedForKeyboard(true)` and comes back wearing the
keyboard selection ring when its mode is re-selected, while arrows no longer address it; and
the exact-value panel, if open, is left up with nothing driving it. Both self-heal on the next
`_SelectVoLumKnob` (it rewrites the flag on every knob) or a click outside the panel, and the
pre-existing Escape path at `VoLumKeyboard.inc.cpp:466-470` has the same omission, which is
presumably where the pattern was copied from. `mVolumSelectedKnobHintText` is also left stale
but is read nowhere else, so it is harmless.

**Repro.** Select the Tremolo RATE knob, click SYNC, press an arrow (see F2), click SYNC back.
RATE is drawn as keyboard-selected but does not respond to arrows until re-selected.

**Suggested fix.** Call `_ClearVoLumKnobSelection()` in the bail-out instead of hand-clearing
two fields, and `return true` (F2).

On the second half of the question: `IsHidden()` is the right test here. iPlug2 has no control
hierarchy — `_HideControlGroup` sets `mHide` on each member individually — so there is no
"hidden by a parent" case where it lies, and `GetControlWithParamIdx` returns the knob rather
than the co-registered `VoLumParamValueControl` (otherwise the `dynamic_cast<NAMKnobControl*>`
below it would already have broken arrow editing entirely).

### F6 — `Utf8SequenceLength` trusts the lead byte, so invalid UTF-8 still reaches the registry

- **Severity:** Low (the fix does not deliver its stated invariant; practical exposure needs a
  hand-edited or corrupted registry)
- **Confidence:** CONFIRMED
- **Introduced by:** `79e57ca` "Make the custom content library survive its own failure modes"

**Evidence.** The decoder reads the lead byte's length and never checks that the following
bytes are continuation bytes:

```92:118:NeuralAmpModeler/VoLumCustomModel.h
inline std::size_t Utf8SequenceLength(unsigned char lead)
{
  ...
  return 0; // continuation byte or invalid lead
}
...
    if (len == 0 || end + len > s.size() || end + len > maxBytes)
      break;
```

So `"A\xE2bc"` (a 3-byte lead followed by ASCII) is copied through whole by both `Utf8Prefix`
and `NormalizeCabName` (`:127-158`, same `len == 0 || i + len > in.size()` test), as are
overlong forms (`\xC0\x80`) and out-of-range 4-byte sequences (`\xF4\x90...`). The stated
reason for the change is that "`dump()` throws — so naming a cabinet with an emoji could take
the plug-in down while saving the amp" (`VoLumCustomModel.h:123-126`), and nlohmann throws on
all of those, not just on split sequences. The tests cover the two easy shapes only — a lone
continuation run and a truncated tail
(`test_volum_custom_content.cpp:382-383`, `:401`) — neither of which exercises a lead byte
followed by a non-continuation byte.

**Suggested fix.** Validate the trailing bytes (`(b & 0xC0) == 0x80` for each) and reject
overlongs / `> U+10FFFF` / surrogates, then add the cases above to
`test_volum_custom_content.cpp`. No call site truncates by bytes any more — I checked; there
is no remaining `substr(0, n)` on a display or persisted name in `NeuralAmpModeler/`.

Two adjacent nits, neither a regression: `ClampName(s, maxChars)` and
`ShortCaptureLabel(name, maxChars)` compare `s.size()` (bytes) against `maxChars` and then pass
it to `Utf8Prefix` as a *byte* budget (`VoLumCustomModel.h:537-553`), so the parameter name
lies and a multi-byte name is cut shorter than the caller intends — but that matches the
pre-fix behaviour, minus the corruption. And `ShortCaptureLabel` appends an ellipsis to a
2-character CJK name that would have fitted.

### F7 — `_UpdateVoLumLayout` decides lane visibility before `_VolumApplyDualAmpFocus` clamps it

- **Severity:** Low (one stale layout pass; self-heals on the next interaction)
- **Confidence:** CONFIRMED
- **Introduced by:** `bb01a44`

**Evidence.** The lane groups are hidden from a snapshot taken seven lines before the clamp
runs:

```194:201:NeuralAmpModeler/VoLumLayoutRuntime.inc.cpp
    const bool supportFocusNow = dualActiveNow && mVolumDualAmpFocusedSupport;
    _HideControlGroup(pGfx, "MAIN_LANE_TOGGLES", !ampExpanded || supportFocusNow);
    _HideControlGroup(pGfx, "SUPPORT_LANE_TOGGLES", !ampExpanded || !supportFocusNow);
    ...
    _VolumApplyDualAmpFocus();
```

When the clamp fires, the pass leaves SUPPORT's toggles on screen while focus, the hint bar
and the arrow keys have already moved to MAIN.

**Suggested fix.** Call `_VolumClampSupportFocus()` at the top of `_UpdateVoLumLayout` (it is
cheap and idempotent), or move the `_VolumApplyDualAmpFocus()` call above the group-visibility
block.

### F8 — a rejected exact-value entry closes the box silently, and enum/bool text still snaps to the minimum

- **Severity:** Low
- **Confidence:** CONFIRMED for the enum branch (dead today); LIKELY for the silence being read
  as a bug
- **Introduced by:** `3dcc1c8`

**Evidence.** `ParseNumericEntry` itself is solid — I could not get a wrong number out of it
(see *Reviewed and found correct*). Two seams around it:

```115:119:NeuralAmpModeler/VoLumExactEntry.h
    double typed = 0.0;
    if (!volum::ParseNumericEntry(str ? str : "", typed))
      return;
```

The panel has already been hidden by `mEditing = false; Hide(true);` at the top of the
function, so a rejected entry is indistinguishable from Escape. That is the intended
"mistyping cancels" behaviour and much better than moving the knob to zero, but there is no
signal that the text was refused; a user who typed `1,2,3` will conclude the box is broken
rather than that they mistyped.

And the branch above it reintroduces the exact defect the commit removed, for the one param
type it forwards to iPlug2:

```109:113:NeuralAmpModeler/VoLumExactEntry.h
    if (pParam->Type() == IParam::kTypeEnum || pParam->Type() == IParam::kTypeBool)
    {
      SetValueFromUserInput(pParam->ToNormalized(pParam->StringToValue(str ? str : "")), 0);
```

`IParam::StringToValue` returns 0 for an enum/bool string it does not recognise, so a typo
there *does* move the parameter, to its minimum. Harmless today — only `NAMKnobControl` params
reach this control, as the comment says — but it is a trap for whoever wires the first list
param, which is precisely the case the branch was added for.

**Suggested fix.** For the silent rejection: keep the panel open with a short inline "not a
number" state, or leave as-is and treat this as a documented decision. For the enum branch:
compare against `pParam->GetDisplayText(i)` / `NDisplayTexts()` and bail out when nothing
matches, instead of trusting `StringToValue`'s 0.

## Reviewed and found correct

- **`_VolumApplyUiSyncPlan`'s `laneFocused` guard (item 1) — correct and complete.** Every
  caller reaches it through `_VolumApplyCustomMainCabs` / `_VolumApplyFocusedLaneCabs`, the
  cache and scene writes stay unconditional
  (`VoLumSceneRig.inc.cpp:448-463`), and the reads that matter come back from the caches, not
  from the control. The one place that reads a control back as authoritative is already
  focus-guarded (`VoLumSceneRig.inc.cpp:906-909`, `uiActive` only for the MAIN lane). The
  channel-stepper write at `:441-442` is deliberately *outside* the guard and that is right:
  MAIN and SUPPORT own separate stepper controls
  (`kCtrlTagVoLumChannelStep` / `kCtrlTagVoLumSupportChannelStep`), so writing the background
  lane's stepper cannot disturb the visible one. Editor open and chunk restore both end in
  `_VolumSyncUiFromState()` → `_VolumApplyFocusedLaneCabs()`
  (`VoLumAmpMenus.inc.cpp:283-320`), and `_VolumApplyRecalledPreset` reconciles both lanes and
  then the focused one (`VoLumSettingsPresets.inc.cpp:169`). The residual hole is F3, and it is
  in the clamp, not in the guard.
- **`_VolumSelectIR` / `_VolumForceDirectCapture` channel derivation (item 2) — correct for
  both lanes and both directions.** MAIN persists the stepper *position* and SUPPORT persists
  the gain-stage *number*; the new code speaks each lane's language
  (`VoLumSceneRig.inc.cpp:541`, `:628-672`, documented at `:347-350`). I checked that
  `mVolumChannelIdx` is maintained at every site that can move MAIN's channel before an IR
  pick — the stepper callback clamps and assigns it *before* re-deriving
  (`VoLumLayoutBuild.inc.cpp:551-552`), `_VolumForceDirectCapture` recomputes it from
  `ChannelStepIndex` (`VoLumSceneRig.inc.cpp:672`), the cab-slot path does the same (`:950`),
  `_VolumApplyUiSyncPlan` writes back the resolved position (`:461`), and the chunk reader sets
  it before any refresh (`Unserialization.cpp:637`). `CustomChannelAtStep` clamps, so even an
  out-of-range value cannot index badly. No live-use regression.
- **`StepKeyboard` + `VoLumCabStep.h` (item 3) — mouse/keyboard parity holds.** `Selectable()`
  is the same predicate the click path uses (`VoLumSpeakerRow.h:342-349`, used at `:260` and
  `:288`), so the keyboard cannot reach a slot the mouse cannot, or vice versa. Old behaviour is
  preserved on factory amps: the planner sets `noCabEnabled = true` and factory names are never
  empty (`VoLumUiSyncPlan.h:119-122`, `VoLumSpeakerRow.h:83-89`), so all four slots stay
  selectable and stepping still wraps 4-wide exactly like the deleted `% kSpeakerCount`. Zero
  selectable slots and "one selectable slot, already selected" both return `-1` and the shortcut
  reports unhandled; with the IR chip active the same slot is returned so the step retires the
  IR, matching a click on the highlighted button (`VoLumCabStep.h:30-39`). Non-contiguous
  captures are handled by `Selectable` reading the resolved per-channel names. And it fires
  `mCallback(next)` — the same callback the mouse fires — so the change persists, stages the
  `.nam` and is audible.
- **`_VolumHasSupportAmp()` (item 4) — correct.** It covers both a factory partner and a custom
  one (`VoLumAmpMenus.inc.cpp:391-395`) and correctly replaces the open-coded copy it was
  extracted from. Focus cannot survive on a nonexistent SUPPORT lane through preset recall or
  chunk restore, because `_VolumApplyDualAmpFocus()` runs from `_UpdateVoLumLayout` after every
  interaction and clamps before its own early-out. It does not lose user intent: the flag is
  re-set from scratch on the next explicit focus action, and picking a support amp afterwards
  goes through `_VolumSetSupportAmp` / `_VolumSetSupportCustom`, which both re-derive
  (`VoLumAmpMenus.inc.cpp:207`, `:243`). The defect is only the missing row resync — F3.
- **Settings page `H` / `Esc` (item 6) — no double-handling.** The open-page branch returns
  `true` on `H`, so `_HandleVoLumKeyboardFocusKey`'s `H` (which would reopen it) is never
  reached, and it returns `false` for every other key so the rig shortcuts cannot edit the amp
  behind the overlay (`VoLumLayoutBuild.inc.cpp:1378-1393`). `Esc` cannot double-fire: text
  entry is checked first (`:1355-1356`), the transient-surface dismissal above runs on a tag
  list that does not include `kCtrlTagSettingsBox` (`:1360-1376`), and the exact-entry cancel is
  ahead of both (`:1342-1353`). `T`/`M` are likewise unreachable while the page is up.
- **Overlay action-code layout (item 7) — sound, with room to spare.** Families are
  `1 << 20 + n * (1 << 16)` (`VoLumOverlayActionCodes.h:24-47`), the highest family base is
  1,572,864 — nowhere near `INT_MAX` — and every fixed code in the overlay is below 100
  (`VoLumCustomOverlay.h:385-410`), so no fixed code can fall in a family range and no
  reachable row index can cross into the next family. The decode arithmetic matches the encode
  (`base <= code < base + kActionStride`, `VoLumCustomOverlay.h:783-811`, `:984-999`).
- **Import failure handling (item 7) — leaves nothing behind.** `MintId` is pure — it reads the
  registry to avoid a collision and does not record anything
  (`VoLumContentStore.h:454-463`) — `ImportFileCopy` only copies
  (`:853-870`), `AddIR` pops its entry back off when `Save()` fails
  (`VoLumCustomContentApi.h:272-286`), and each failure branch calls
  `store.RemoveStoredFile(rel)`, which no-ops safely on an unresolvable or missing path
  (`VoLumContentStore.h:878-885`). So a failed import leaves no copied payload and no registry
  row. (Unrelated and pre-existing: a pedal import that fails because the slots are full sets
  `mError` and then has it wiped by the `else mError.clear();` at
  `VoLumCustomOverlay.h:703-704`, because that path pushes to no bucket. Not from tonight.)
- **`volum::ParseNumericEntry` (item 8) — I could not make it return a wrong number.**
  `"3dB"`, `"3 dB"`, `" -12.5 dB "`, `"50%"` on a non-percent param and `"1e3"` all parse to the
  number and leave the clamping to the caller; `""`, `"abc"`, `"-"`, `"1.2.3"`, `"5-3"`,
  `"1,5,5"`, `"1,234,567"` and `"3 4"` are all rejected because `strtod` stops early and the
  tail contains a non-space, non-unit character (`VoLumNumericEntry.h:66-75`); `"nan"`, `"inf"`
  and `"1e999"` are caught by the `std::isfinite` check (`:77-79`); `"0x10"` parses as 16, which
  is a hex literal a user cannot type by accident and is then clamped like any other number.
  Out-of-range values are clamped rather than rejected (`VoLumExactEntry.h:119`), which matches
  the pre-change behaviour and the knob's own drag limits. Passing `kNoValIdx` to
  `CreateTextEntry` (`:163`) is the right lever: it leaves `mEdParam` null so iPlug2 hands back
  the raw string instead of an already-`atof`'d value. One caveat I could not rule out by
  reading: `strtod` honours `LC_NUMERIC`, so under a host that calls `setlocale(LC_ALL, "")` on
  a comma-decimal system, both `"1.5"` and the comma-rewritten `"1,5"` would be refused. The
  same locale would already make `GetDisplay` print `1,5`, so this is self-consistent in
  practice — SPECULATIVE, and not worth changing before the release.

## Weak pins

Flagging these because they read as coverage for tonight's fixes and are not:

- `test_volum_ui_regressions.cpp:1195-1227` ("A double-click the overlay did not begin cannot
  run a row's action") asserts only that the strings `mOwnsGesture = true;`,
  `const bool ownsGesture = mOwnsGesture;`, `mOwnsGesture = false;` and `if (!ownsGesture)`
  appear in the header, and that the guard precedes the row dispatch. All five hold with **F1**
  present — the test cannot see *when* the flag is cleared, which is the entire bug. Its own
  comment admits why ("The overlay needs a real graphics host to instantiate, so this pins the
  wiring"). Either extract the gesture-ownership rule into a testable helper the way
  `VoLumCabStep.h` and `VoLumOverlayActionCodes.h` were extracted, or at minimum pin
  `RequireContains(overlay, "void OnMouseUp(")`.
- `test_volum_ui_regressions.cpp:1145-1160` pins `ApplyDelete(now);` / `mOverwritePreset(now);`
  and forbids the `idx` spellings, but says nothing about rename — **F4** passes it.
- `test_volum_ui_regressions.cpp:355-360` / `:1018-1022`
  (`RequireDoesNotContain(source, "SetSelected(mVolumSpeakerIdx);")`) is dodged by a local: the
  live push in `_VolumApplyDualAmpFocus` reads
  `row->SetSelected(focusedSpeakerIdx);` with `focusedSpeakerIdx = ... : mVolumSpeakerIdx`
  (`VoLumAmpMenus.inc.cpp:434-436`). That one is *correct* — it is behind
  `if (customLane < 0)`, so it only ever runs for a factory lane, where `mVolumSpeakerIdx` is
  the persisted speaker index — but the pin would not have caught it if it were not, and its
  comment claims the property globally.
- Nothing pins `_VolumClampSupportFocus` / `_VolumHasSupportAmp` (F3), the `laneFocused` guard
  itself, or the `IsHidden()` bail-out (F2, F5). Those four changes ship untested; grep across
  `NeuralAmpModeler/tests` finds no reference to any of them.
