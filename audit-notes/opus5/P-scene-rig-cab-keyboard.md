I've read all the named files plus the call graph around them. Here are the findings, ranked.

---

## 1. BLOCKER — the custom-IR DIRECT gate still reads the runtime gain-stage cache during restore, silently deleting the scene's IR

**WHERE:** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:511-525` (gate), reached from `VoLumSettingsScene.inc.cpp:303` (`_VolumApplyActiveIr`) inside `_VolumApplyAmpSettings`.

**MECHANISM:** The 1.2.1 fix hardened `_VolumForceDirectCapture` but not the gate that runs immediately above it in the same function:

```509:512:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  // Channel-first: a custom IR needs a DIRECT capture ON THE CURRENT channel (the
  // raw signal it convolves). An amp may have DIRECT on one channel but not the
  // one in focus, so this is a per-channel gate, not the amp-wide HasDirectCapture.
  const int laneChannel = support ? mVolumCustomSupportChannel : mVolumCustomMainChannel;
```

For MAIN this is the runtime cache, and the restore call graph guarantees it is stale at this point:

- `_VolumSelectCustomAmp` (`VoLumSceneRig.inc.cpp:315-344`) sets `mVolumCustomMainIdx`, calls `_VolumApplyAmpSettings(scene)`, and only *afterwards* calls `_VolumApplyCustomMainCabs`, which is the only thing that writes `mVolumCustomMainSlot/Channel` (`:440-441`).
- `_VolumApplyAmpSettings` restores the persisted *position* (`mVolumChannelIdx = s.channelIdx`) and then, at line 303, runs `_VolumApplyActiveIr(s.activeIrId, false)` → `_VolumSelectIR(idx, false, false)`.

So the gate tests `ChannelHasDirect(amp, <previous amp's stage, or the default 1 at startup>)`. When it fails, the function clears the scene's IR id and returns:

```514:517:NeuralAmpModeler/VoLumSceneRig.inc.cpp
    if (support)
      _VolumActiveScene().supportActiveIrId.clear();
    else
      _VolumActiveScene().activeIrId.clear();
```

`interactive` is `false` on this path, so there is no message box. `_VolumApplyCustomMainCabs` then resolves the row with `irIdPresent == false` and a baked cab takes over. The SUPPORT lane is *not* affected, because `_VolumApplyAmpSettings:247-285` resolves `mVolumCustomSupportChannel` before line 304 — the same asymmetry as the original bug.

Secondary read of the same stale pair, one function down: `_VolumForceDirectCapture:623-625` computes `alreadyDirect` from `mVolumCustomMainSlot`/`mVolumCustomMainChannel`, so on a restore it can wrongly report "already on DIRECT" and skip the capture-load deferral.

**TRIGGER:** Any custom amp whose gain stage 1 has no DIRECT capture — including the very case the code comments call out, an amp whose stages are only 3 and 4 — with a custom IR active. Then either: quit and relaunch the standalone; or select a different amp and come back; or recall a preset saved with the IR while sitting on another channel.

**IMPACT:** The IR is gone. The lane plays the baked cab instead of the user's IR (wrong model), the copper Custom IR chip is dark, and because the cleared `activeIrId` is written back through `_VolumSaveCurrentToSettings` → `GlobalContentStore().Save()`, the reference is destroyed permanently, not just for the session.

**CONFIDENCE:** certain for the code path; likely for the exact permanent-loss outcome (depends on a save happening afterwards, which `OnUIClose`/the destructor always do).

**WOULD A TEST HAVE CAUGHT IT:** No, and the suite actively cements it. `test_volum_ui_sync_plan.cpp` tests the *planner* (`CustomChannelAtStep`, "Custom lane on channel five with a custom IR restores channel five") — but the planner never runs, because the IR id is already cleared before `MakeUiSyncPlan` is reached. The source pin at `test_volum_ui_regressions.cpp:1026-1035` scopes its "must not read the cache" check to the body of `_VolumForceDirectCapture` only, so line 511 is outside it — while lines 974 and 993 *require* the string `!volum::custom::ChannelHasDirect(volum::custom::CustomAmpAt(customLane), laneChannel)`, pinning the buggy expression as intended behavior.

**FIX SKETCH:** Derive the gate's channel the way the fixed function below it already does:

```cpp
const int laneChannel = support
  ? mVolumCustomSupportChannel
  : volum::CustomChannelAtStep(volum::custom::CustomAmpAt(customLane), mVolumChannelIdx);
```

Guard `customLane >= 0` before `CustomAmpAt`, and extend the pin at 1035 to cover `_VolumSelectIR` too. No voicing change — it restores the routing the user saved.

---

## 2. BLOCKER — keyboard `S` (cycle cab) has no custom-amp branch and rescans the factory rig folder

**WHERE:** `NeuralAmpModeler/VoLumKeyboard.inc.cpp:189-227`.

**MECHANISM:** The mouse speaker-row callback branches on the focused custom lane (`VoLumLayoutBuild.inc.cpp:265-279`). The keyboard equivalent does not:

```205:214:NeuralAmpModeler/VoLumKeyboard.inc.cpp
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

With a custom MAIN amp focused this is the exact bug class the 1.2.1 changelog describes for preset recall, reintroduced on the keyboard path:

- `mVolumAmpSettings[mVolumAmpIdx].speakerIdx = next` writes the *underlying factory* amp's saved cab, which the custom amp is only borrowing as an index.
- `_VolumRefreshChannels()` (`VoLumSceneRig.inc.cpp:52-75`) rescans `kAmps[mVolumAmpIdx].folderName`, replaces `mVolumChannelLabels` with the factory amp's labels, pushes them into `kCtrlTagVoLumChannelStep`, and — if the custom amp has more gain stages than the factory folder has channels — resets `mVolumChannelIdx` to 0 (`:64-69`). The next `_VolumSaveCurrentToSettings` persists that factory-derived index as the custom scene's `channelIdx` (`VoLumSettingsLocks.inc.cpp:97`).
- `mVolumCustomMainSlot/Channel` are untouched, so the loader (`NeuralAmpModeler.cpp:821`) resolves the same `.nam` as before and the same-path check at `:869` skips the reload.

The support branch (`:195-204`) is equally unbranched: with a *custom* SUPPORT partner it writes `kSupportSpeakerIdx`, which `_VolumRefreshSupportChannels` ignores for custom amps (`VoLumAmpMenus.inc.cpp:359-368`), and then line 220 forces `row->SetSelected(GetParam(kSupportSpeakerIdx)->Int())` on top of the resolved selection.

**TRIGGER:** Focus a custom amp, make sure the AMP section is expanded, press `S`.

**IMPACT:** The channel stepper switches to the factory amp's channel labels under a custom amp's name; the cab row highlights a slot that may have no capture (drawn as `--`); audio does not change, so the UI and what is playing disagree; and the custom amp's persisted gain-stage position is corrupted, so the *next* open loads the wrong capture. The factory amp's own saved cab is silently changed too.

**CONFIDENCE:** certain that the factory refresh runs and the factory slot is written; likely for the persisted-position corruption (needs the custom amp to have more stages than the factory folder, or a subsequent resolve).

**WOULD A TEST HAVE CAUGHT IT:** No. The only coverage is the source pin `RequireContains(source, "_CycleVoLumKeyboardSpeaker(key.S ? -1 : 1)")` at `test_volum_ui_regressions.cpp:182`, which asserts the call exists and nothing about what it does. There is no headless test of this function, and no `test_volum_dual_amp*.cpp` exists in `NeuralAmpModeler/tests/` at all.

**FIX SKETCH:** Smallest safe change is to make the keyboard reuse the mouse path exactly as the channel stepper already does with `StepKeyboard` — compute `next`, call `spkCtrl->SetSelected(next)` and then invoke the speaker row's own change callback, so the custom/factory branch lives in one place. Failing that, add the two missing branches (`mVolumCustomMainIdx >= 0 && !supportFocus` → set `mVolumSpeakerIdx` and call `_VolumApplyCustomMainCabs(idx, false)`; `mVolumCustomSupportIdx >= 0` → step `mVolumCustomSupportSlot` and call `_VolumApplyCustomMainCabs(idx, true)`) and return early. No voicing change.

---

## 3. MAJOR — leaving a custom amp leaves "No Cab" and "Custom IR" permanently greyed out on the factory amp

**WHERE:** `VoLumLayoutBuild.inc.cpp:108-110` (mouse) and `:1445-1450` (keyboard Up/Down); enable state owned by `VoLumSpeakerRow.h:50-68`.

**MECHANISM:** `SetNoCabEnabled` / `SetIrEnabled` are written in exactly one place, `_VolumApplyUiSyncPlan` (`VoLumSceneRig.inc.cpp:410-411`). Neither amp-select path calls it. The mouse path hand-pokes only the labels:

```108:110:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
          // Restore the factory cab labels (a custom amp may have overridden them).
          if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
            spk->As<VoLumSpeakerRowControl>()->SetFactoryCabs();
```

and the keyboard path does not even do that. Nothing downstream repairs it: `_VolumApplyAmpSettings` only calls `SetSelected` and `_UpdateVoLumLayout`; `_UpdateVoLumLayout` calls `_VolumApplyDualAmpFocus`, which also only calls `SetSelected` (`VoLumAmpMenus.inc.cpp:419-432`).

**TRIGGER:** Focus a custom amp, step to a gain stage that has no DIRECT capture (No Cab and Custom IR grey out, correctly), then select a factory amp — by sidebar click or by Up/Down.

**IMPACT:** On a factory amp, which always ships a raw DIRECT capture, "No Cab" and "Custom IR" render disabled and swallow clicks (`VoLumSpeakerRow.h:249-262` returns early on `!mIrEnabled` / `!mNoCabEnabled`). The user cannot select No Cab or open the IR menu at all until they close and reopen the editor (`OnUIOpen` → `_VolumSyncUiFromState`) or focus a dual lane. On the keyboard path the row additionally still shows the custom amp's `CB1/CB2/CB3` labels under the factory amp's name.

**CONFIDENCE:** certain.

**WOULD A TEST HAVE CAUGHT IT:** No. `test_volum_ui_sync_plan.cpp` proves the planner *computes* `noCabEnabled`/`irEnabled` correctly; nothing tests that the amp-select paths apply it. The pin at `test_volum_ui_regressions.cpp:988-989` only checks the strings `row->SetNoCabEnabled(plan.noCabEnabled` / `row->SetIrEnabled(plan.irEnabled` exist somewhere in the source.

**FIX SKETCH:** Delete the ad-hoc `SetFactoryCabs()` and end both amp-select paths with `_VolumApplyFocusedLaneCabs()`, which already routes the factory lane through the planner and writes names, enables, selection, IR chip and stepper together. Purely UI.

---

## 4. MAJOR — the shared cab row is written for an unfocused lane; preset recall on a custom MAIN amp with a custom SUPPORT partner leaves the row showing SUPPORT

**WHERE:** `VoLumSceneRig.inc.cpp:396-424` (`_VolumApplyUiSyncPlan`, no focus guard) plus the caller at `VoLumSettingsPresets.inc.cpp:145-146`.

**MECHANISM:** Every other function that touches the shared speaker row guards on lane focus — `_VolumSelectIR:578` (`if (support == _VolumSupportFocused())`), `_VolumClearIR:705`, `_VolumForceDirectCapture:597` ("the shared row shows the other lane; don't disturb it"), `_VolumReconcileActiveIr:857`. `_VolumApplyUiSyncPlan` does not: lines 406-424 write `SetFactoryCabs`/`SetCabNames`, `SetNoCabEnabled`, `SetIrEnabled`, `SetIrCab` and `SetSelected` for whatever lane it was handed. `_VolumApplyRecalledPreset` hands it the support lane unconditionally, *after* the main lane:

```141:146:NeuralAmpModeler/VoLumSettingsPresets.inc.cpp
  if (mVolumCustomMainIdx >= 0)
    _VolumApplyCustomMainCabs(mVolumCustomMainIdx, false);
  else
    _VolumRefreshChannels();
  if (mVolumCustomSupportIdx >= 0)
    _VolumApplyCustomMainCabs(mVolumCustomSupportIdx, true);
```

so SUPPORT wins the shared row. The mirror-image gap: when SUPPORT is focused and the support amp is a *factory* amp, `_VolumApplyRecalledPreset` re-applies only MAIN, so the row shows MAIN's cab while SUPPORT is focused.

**TRIGGER:** Dual Amp on, custom MAIN amp with a custom SUPPORT partner, MAIN focused; recall any preset.

**IMPACT:** The cab row shows the SUPPORT amp's cab names, enable gating, selection and IR chip while MAIN is the focused lane. Clicking a cab then edits MAIN (`VoLumLayoutBuild.inc.cpp:242-279` branches on focus) with an index that belongs to the support amp's layout, so the user can land MAIN on a slot with no capture or on the wrong cab.

**CONFIDENCE:** certain for the unguarded write; likely for the persistence of the wrong state, since the only later repair (`_VolumApplyDualAmpFocus`) fixes `SetSelected` and never the names, enables or IR chip.

**FIX SKETCH:** Wrap the control writes in `_VolumApplyUiSyncPlan` in `if (support == _VolumSupportFocused())`, leaving the routing-cache and scene writes at `:426-445` unconditional (the background lane still needs its `.nam` staged); then end `_VolumApplyRecalledPreset` with `_VolumApplyFocusedLaneCabs()`. Purely UI.

---

## 5. MAJOR — `_VolumApplyAmpSettings` pushes a partial, MAIN-only selection outside the planner

**WHERE:** `VoLumSettingsScene.inc.cpp:293-299`.

```293:299:NeuralAmpModeler/VoLumSettingsScene.inc.cpp
  // Update speaker row UI if available
  if (auto* pGfx = GetUI())
  {
    if (auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      spkCtrl->As<VoLumSpeakerRowControl>()->SetSelected(mVolumSpeakerIdx);
    _UpdateVoLumLayout(pGfx);
  }
```

**MECHANISM:** This is the precise anti-pattern the 1.2.1 reopen fix removed from the layout build — a bare `SetSelected` with no cab names, no enables, no IR chip. For a custom lane `mVolumSpeakerIdx` is a raw persisted UI index that `ResolveLaneCabs` has not yet snapped, so it can name a slot the resolved channel does not carry; and when SUPPORT is focused it writes MAIN's index into the shared row. `_VolumApplyAmpSettings` is on every restore path (`_VolumRestoreFromSettings`, `_VolumSelectCustomAmp`, `_VolumApplyRecalledPreset`, `_VolumResetAmpToFactory`).

**TRIGGER:** Any restore of a custom scene, or any restore while SUPPORT is focused.

**IMPACT:** A wrong or flashing cab highlight; on the paths that do not follow up with `_VolumApplyCustomMainCabs` (see finding 4), it persists.

**CONFIDENCE:** likely — most callers self-heal, `_VolumApplyRecalledPreset` with a factory support lane does not.

**WOULD A TEST HAVE CAUGHT IT:** No. The guard at `test_volum_ui_regressions.cpp:1011` is `RequireDoesNotContain(source, "spkRow->SetSelected(mVolumSpeakerIdx);")` — this site uses the variable name `spkCtrl`, so the pin passes on a one-character difference.

**FIX SKETCH:** Drop the `SetSelected` and keep only `_UpdateVoLumLayout(pGfx)`; every caller either already invokes the single applier or should (finding 4). Then tighten the pin to match `SetSelected(mVolumSpeakerIdx)` regardless of receiver name.

---

## 6. MAJOR — Dual Amp with no support amp: the cab row and `S` drive a lane that does not exist

**WHERE:** `VoLumKeyboard.inc.cpp:166-169`; `VoLumAmpMenus.inc.cpp:409-432`.

**MECHANISM:** The keyboard toggle focuses SUPPORT purely on the new Dual Amp value, with no check for a partner:

```166:169:NeuralAmpModeler/VoLumKeyboard.inc.cpp
  const bool next = _VolumUserToggleParam(paramIdx);

  if (paramIdx == kDualAmpActive)
    mVolumDualAmpFocusedSupport = next;
```

`_VolumApplyDualAmpFocus` computes `hasSupportAmp` (`:414-416`) but uses it only for the polarity toggle; the row selection is written unconditionally for a non-custom lane (`:424-431`), and `_VolumRefreshSupportChannels` leaves `mVolumSupportChannelLabels` empty when `supportAmpIdx < 0`, so the stepper renders `---`. The mouse DUAL chip (`VoLumLayoutBuild.inc.cpp:385-388`) does *not* move focus, so the two inputs disagree.

**TRIGGER:** Press `2`, then Space (standalone) or `B` (plugin) with the support amp at "(none)" — the default. Also reachable by switching MAIN to an amp whose scene has no support partner while SUPPORT is focused.

**IMPACT:** The cab row jumps to the phantom lane's V30, the channel stepper reads `---`, and `S` silently edits `kSupportSpeakerIdx` — the highlight moves, nothing is audible, and `mVolumSettingsDirty` is set. The user has to guess that they must pick a support amp from the hero picker first.

**CONFIDENCE:** certain.

**FIX SKETCH:** Gate the focus change on a partner existing — `mVolumDualAmpFocusedSupport = next && (GetParam(kSupportAmpIdx)->Int() >= 0 || mVolumCustomSupportIdx >= 0)` — and use the existing `hasSupportAmp` predicate to skip the `SetSelected` in `_VolumApplyDualAmpFocus` and to make `_CycleVoLumKeyboardSpeaker`'s support branch a no-op. Purely UI/focus.

---

## 7. MINOR — a keyboard-selected knob stays editable after its mode hides it

**WHERE:** `VoLumKeyboard.inc.cpp:445-474`; `NeuralAmpModelerControls.h:106-129`; the mode pickers at `VoLumCoreControls.h:113` and `:326`.

**MECHANISM:** `_SwitchVoLumKeyboardSection` and `_CycleVoLumKeyboardTarget` both call `_ClearVoLumKnobSelection()`, and the sidebar, speaker row and channel stepper all call `ClearVoLumKnobSelection(this)` in `OnMouseDown`. `VoLumModePickerControl` / `VoLumSubModePillControl` do not. `_HandleVoLumSelectedKnobKey` resolves the control with `GetControlWithParamIdx` and `HandleKeyboardInput` never checks `IsHidden()`.

**TRIGGER:** In POST, Tab to Tremolo, Enter, arrow across to CROSSOVER (Harmonic only), then click the mode pill to Optical — `TREMOLO_XOVER` is hidden by `_UpdateVoLumLayout:184`. Same shape for DELAY TIME vs the SYNC/DIVISION swap and for the PITCH transpose/octaver knob split.

**IMPACT:** Up/Down keep editing an invisible parameter and the hint bar names a knob that is not on screen.

**CONFIDENCE:** certain.

**FIX SKETCH:** Add `ClearVoLumKnobSelection(this)` to the two picker controls' `OnMouseDown` (matching the other controls), or bail in `_HandleVoLumSelectedKnobKey` when the resolved control `IsHidden()`.

---

## 8. MINOR — keyboard shortcuts fire while the Settings page covers the window

**WHERE:** `VoLumLayoutBuild.inc.cpp:1392-1393`.

**MECHANISM:** The modal-swallow list is `{kCtrlTagVoLumConfirm, kCtrlTagVoLumCustomOverlay, kCtrlTagVoLumPresetMenu, kCtrlTagVoLumIrMenu, kCtrlTagVoLumPreCaptureMenu, kCtrlTagVoLumSupportAmpMenu}` — it omits `kCtrlTagSettingsBox`, which is attached over the full window bounds (`:1081-1084`).

**TRIGGER:** Press `H` or click the gear, then press `S`, `1`, `2`, `3`, Tab, or Space/`B`.

**IMPACT:** The rig changes behind the settings overlay — `S` cycles the cab (with all of finding 2's consequences if a custom amp is focused), Space/`B` toggles the focused effect or Dual Amp. Separately, `H` only ever calls `HideAnimated(false)`, so it opens the page and can never close it.

**CONFIDENCE:** certain.

**FIX SKETCH:** Add `kCtrlTagSettingsBox` to `kModalTags`; make the `H` handler toggle on `IsHidden()`.

---

## NITs

- `_VolumRefreshChannels` (`VoLumSceneRig.inc.cpp:64-69`) resets an out-of-range channel position to `0` rather than clamping to the last valid one, so deleting a capture drops the user on the first gain stage instead of the nearest surviving one.
- `_VolumApplyActiveIr` (`VoLumSceneRig.inc.cpp:713-728`) leaves `kIRToggle` at 1 for an empty or orphaned id, while `_VolumClearIR:702-704` clears it — the param disagrees with the scene until something else writes it.
- `_VolumMakeUiSyncInput` (`VoLumSceneRig.inc.cpp:375`) fills `in.factoryAmpIdx = mVolumAmpIdx` even for the SUPPORT lane; harmless only because the applier ignores `plan.sidebarFactoryIdx`.
- `_VolumResetAmpToFactory` (`VoLumAmpMenus.inc.cpp:5-35`) is a no-op for PRE/POST while their locks are engaged (the guards in `_VolumApplyAmpSettings:215` and `:290`) and never recomputes `mVolumPreLockUiDirty`/`mVolumPostLockUiDirty` after zeroing the scene, so "Default (factory settings)" keeps the whole PRE/POST chain running and the store-arrow chrome can show clean against a scene it no longer matches.
- `_VolumShowPresetMenu` (`VoLumAmpMenus.inc.cpp:52`) calls `MockPresetsForAmp(mVolumAmpIdx)` without first calling `SetActivePresetOwner`; that function ignores its argument and reads the global owner key (`VoLumCustomContentApi.h:473-476`), so the menu is correct only because every amp-switch path happens to set the owner — one missed path lists another amp's bank.
- `_VolumRefreshSupportChannels` (`VoLumAmpMenus.inc.cpp:383-389`) clamps `kSupportChannelIdx` only inside the in-range-support-amp branch, so with "(none)" a stale out-of-range index survives in the param and gets persisted.

---

**One cross-cutting note on the test strategy:** findings 2, 3, 5 and 6 all live in code whose only guard is a `RequireContains` string pin, and findings 1 and 5 slip past pins that exist specifically to prevent them (one scoped to the wrong function body, one matching a receiver variable name). The planner in `VoLumUiSyncPlan.h` is well covered and, as far as I can tell, correct — every bug above is in a *caller*: either it reads state a later step will overwrite, or it hand-pokes controls the planner owns. A headless fake for the applier (recording `SetCabNames`/`SetNoCabEnabled`/`SetIrEnabled`/`SetSelected`/`SetChannels` calls per lane) would catch 3, 4, 5 and 6 at once, and would be worth more than any further string pins.

I'm checking what state the factory reset function doesn't cover—specifically looking at what a default `VoLumAmpSettings{}` object is missing. I notice that the pre and post lock states aren't being reset when they're locked, which means newly added state could persist through a factory reset.

There's also an issue with the IR shaping: when the factory reset clears the IR by calling `_VolumApplyActiveIr` with a negative index, it sets the flag to remove the IR and clears the cabinet, but it doesn't actually set the kIRToggle parameter to 0. So after the reset, the toggle stays enabled even though the IR data is gone, which creates a mismatch in the processing pipeline. kIRToggle stays stuck at 1 even though the IR is being cleared, creating an asymmetry with `_VolumClearIR` which does reset the toggle to 0. There's a brief window where the toggle doesn't match the actual IR state before the audio thread catches up in the next ProcessBlock, but the real issue is this inconsistency in how the two paths handle the toggle.

Now I'm wondering if `kIRToggle` is a real parameter that gets persisted in the chunk and exposed to the DAW — if it stays at 1 with no actual IR loaded, that could cause problems downstream.

Let me document the findings. The main blocker is that `_VolumSelectIR` reads the runtime gain-stage cache during state restoration, which silently wipes out the scene's custom IR instead of preserving it.

The trigger happens when a custom amp lacks a DIRECT capture on channel 1 or has gain stages that don't include 1 (like Fryette 3/4), combined with an active IR—restarting the app, re-selecting the amp, or recalling a preset all cause the issue. The IR gets dropped silently, the wrong cab plays, and `activeIrId` is cleared and persisted, resulting in permanent loss. I'm confident about the code path itself, and fairly confident about what users will experience. The test suite only covers the planner logic, not this actual scenario.

The real problem is that the `laneChannel` expression is stale and locked in by the test's cache check, which only applies to `_VolumForceDirectCapture`'s scope. The fix is to derive `laneChannel` the same way that function does—using the support channel or falling back to the custom channel at the current step—which restores the user's saved routing without changing the sound. There's also a related stale cache issue affecting `alreadyDirect` in the same function.

There's a second blocker where pressing 'S' to cycle through custom amps rescans the factory rig folder and overwrites the custom lane's channel state. When you focus a custom amp and press S in the AMP section, the channel stepper reverts to showing the factory amp's channels, and `mVolumChannelIdx` gets clamped to a factory-derived index that then persists into the custom scene.

This cascades into a UI/audio mismatch: the factory amp's `speakerIdx` gets overwritten, the cab row selects a slot the custom amp may not have (showing "--"), and the loader keeps playing the old capture since `mVolumCustomMainSlot/Channel` remain untouched. There's also a missing custom-support branch that sets meaningless parameters and forces the row selection off the resolved slot. The fix mirrors the mouse speaker-row callback logic to properly handle the custom amp case. The buttons stay disabled when switching from custom to factory amps because `SetNoCabEnabled` and `SetIrEnabled` only get called by `_VolumApplyUiSyncPlan`, and the sidebar's factory cab restoration skips the re-enable step entirely—the keyboard path doesn't even attempt it. The fix is to ensure both paths trigger the proper UI sync when amp focus changes. The applier doesn't guard against writing the shared cab row for an unfocused lane, so preset recall on a custom MAIN with a custom SUPPORT partner leaves the row showing SUPPORT instead of the focused lane's state. The issue spans two files with inconsistent focus checks — every other function that touches the shared row validates the support lane is focused, but this one skips that guard. The fix is to guard the control writes in `_VolumApplyUiSyncPlan` so they only apply when support is focused, while keeping cache and scene writes unguarded, and then have `_VolumApplyRecalledPreset` call `_VolumApplyFocusedLaneCabs()` at the end to ensure the focused lane gets updated. There's also a separate issue in `_VolumApplyAmpSettings` where it's pushing a partial, MAIN-only cab selection outside the planner in VoLumSettingsScene.inc.cpp. ication layer to handle it (the callers already invoke it). There's a major issue with Dual Amp when there's no support amp: the cab row, channel stepper, and 'S' shortcut all try to drive a lane that doesn't exist. The problem is in the keyboard target toggle and amp menus — when Dual Amp turns on, it sets the focused support flag without checking whether a support amp actually exists, even though the apply function knows to compute this but only uses it for polarity.

When triggered (pressing 2 then Space in standalone or B in plugin with no support amp, or switching MAIN to an amp with no support partner while SUPPORT is focused), the cab row jumps to a phantom lane, the channel stepper shows "---", and the S key silently edits the support speaker index with no audible feedback while the highlight moves. The mouse DUAL chip avoids this by keeping focus on MAIN instead.

The fix is to only focus SUPPORT when a support partner actually exists, and gate the dual amp focus and keyboard speaker cycling logic behind the same condition. There's also a separate issue where a keyboard-selected knob remains editable after its mode hides it, which happens because the mode pickers don't clear the knob selection when they should.

For the second issue, I need to either call the clear selection function in the mode picker and sync toggle controls, or check if the resolved control is hidden before processing keyboard input.

There's also a separate bug where keyboard shortcuts fire behind the Settings overlay—the modal tags list is missing the Settings box control, so shortcuts like '1'/'2'/'3' and others still affect the hidden rig. I need to add that control to the modal tags and make 'h' toggle the settings instead of just opening it.

A few other issues to track: the channel refresh resets out-of-range positions to 0 instead of clamping to the last valid channel, so deleting a capture jumps to the first gain stage rather than staying nearby. The IR applier leaves the toggle at 1 when the IR ID is orphaned while the clear function resets it, creating a brief disagreement between the parameter and scene state. The UI sync input function fills the factory amp index even for the support lane, which only works because the applier ignores that field. And there's an incomplete note about the reset amp to factory function.

When resetting to factory defaults, the PRE/POST chains stay locked and their dirty flags don't get recomputed after zeroing the scene, so the UI shows the factory preset as active while the whole chain keeps running. The preset menu also calls the mock presets function without first setting the active preset owner.

The menu reads the global owner key instead of using its argument, so it only shows the right bank because every amp-switch path happens to set the owner first — missing one path would silently list another amp's presets. There's also a potential race where the preset menu captures the active index at click time but the row label was built when the menu opened, so if something deletes a preset between those moments, the confirmed name and the overwritten index could disagree. Though I'm not sure that's actually reachable since the Manage panel can't be open while the dropdown is open.

Actually, I found a clearer issue: `_VolumSelectCustomAmp` calls `_VolumSaveCurrentToSettings()` only when `mVolumInitComplete` is true, but the keyboard navigation path at VoLumLayoutBuild:1436 already calls it unconditionally, resulting in a double-save that's harmless but redundant.

There's also `_VolumRefreshSupportChannels` that only clamps `kSupportChannelIdx` when the amp index is in range; with "(none)" selected, a stale out-of-range index persists in the parameter and gets saved.

And in `VoLumChannelStepControl::StepKeyboard`, the wrapping behavior means on a single-channel amp, Left/Right navigation fires the callback with the same index repeatedly, causing unnecessary .nam stage updates and dirty marks.

Let me trace through the math: with n=1, `mSelected = (0 + delta + 1) % 1 = 0`, so the callback fires with index 0 every time, triggering `_VolumApplyCustomMainCabs` and `_VolumMarkPresetDirty`. But `_VolumMarkPresetDirty` calls `_VolumRecomputePresetDirty`, which is just an equality check, so it correctly determines the preset is clean. The mouse path has a guard `if (n < 1) return;` that prevents this, but the keyboard path doesn't—though it's harmless either way.

Now checking the F3 claim about the sidebar leaving No Cab/IR disabled: tracing through the sidebar callback from lines 80-129, `_VolumRestoreFromSettings` calls `_VolumApplyAmpSettings` which calls `_UpdateVoLumLayout`...

Checking the keyboard path for cab handling... els match, so the loader pulls the same custom file and applies it correctly. The state loss only happens when the index gets reset during the factory folder refresh, which then persists through the next save-and-resolve cycle.

Now I'll write up the report.