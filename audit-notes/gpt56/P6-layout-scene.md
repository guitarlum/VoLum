## Summary

This subsystem has several strong state-reconstruction paths, but it is not release-safe yet. The highest-severity defect is an editor-lifecycle bug: closing the editor while the tuner is open destroys the only visible way to dismiss it while leaving the DSP tuner flag active, so the plugin continues outputting silence behind a hidden tuner after reopen. I also found four major state/DSP consistency defects: custom SUPPORT amps are loaded but excluded from processing, deleting the active custom MAIN amp leaves its deleted capture playing behind factory UI, and the fresh “atomic” cab-source transaction both publishes the incoming IR before the transaction is armed and commits an incoherent IR/capture pair when the replacement capture fails.

## Findings

### F-P6-1: BLOCKER — Closing the editor with the tuner open leaves the plugin silently muted

**Where:** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:5-20`, `NeuralAmpModeler/NeuralAmpModeler.cpp:665-674`, `NeuralAmpModeler/NeuralAmpModeler.cpp:1204-1211`, `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1086-1090`

**Evidence:**

```5:20:NeuralAmpModeler/VoLumSceneRig.inc.cpp
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
    else
    {
      mTunerDSP.SetActive(false);
      tuner->Hide(true);
    }
  }
}
```

```665:674:NeuralAmpModeler/NeuralAmpModeler.cpp
// Metronome: sum click into output
mMetronomeDSP.Process(outputs, nFrames, static_cast<int>(numChannelsExternalOut));

// Tuner active: silence output so player can tune without hearing amp
if (processingPlan.silenceForTuner)
{
  for (size_t c = 0; c < numChannelsExternalOut; c++)
    std::memset(outputs[c], 0, numFrames * sizeof(iplug::sample));
}
```

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

**Mechanism:** Opening the tuner sets `mTunerDSP` active. That flag directly makes every output buffer zero. The flag is cleared only by toggling/dismissing the live tuner control. `OnUIClose()` does not clear it. The editor teardown destroys the visible tuner control, and the next editor build creates a new tuner control hidden by default, without reconciling its visibility to the still-active DSP flag.

**Trigger:** Open the tuner, then close the VST3/AU editor window (or close/recreate the standalone editor) without dismissing the tuner. Reopen the editor.

**Impact:** Audio remains completely silent with no tuner overlay or visible mute reason. The user must discover that opening and then closing the tuner again restores output.

**Fix sketch:** Treat tuner visibility and tuner muting as one lifecycle-owned state. At minimum deactivate the tuner in `OnUIClose()`. Alternatively persist/reopen the overlay when `mTunerDSP.IsActive()`, but never allow an active mute with a hidden/destroyed control.

**Proposed regression test:** `EditorCloseWhileTunerOpenRestoresAudio` — activate the tuner, invoke the UI-close lifecycle, assert `mTunerDSP.IsActive() == false` and `processingPlan.silenceForTuner == false`; after UI reopen assert the hidden tuner and DSP flag agree.

### F-P6-2: MAJOR — A custom SUPPORT amp is displayed and loaded but never enters the audio graph

**Where:** `NeuralAmpModeler/VoLumAmpMenus.inc.cpp:220-249`, `NeuralAmpModeler/NeuralAmpModeler.cpp:580-598`

**Evidence:**

```220:249:NeuralAmpModeler/VoLumAmpMenus.inc.cpp
void NeuralAmpModeler::_VolumSetSupportCustom(int customIdx)
{
  // ...
  mVolumCustomSupportIdx = customIdx;
  // Clear the factory reference so the SUPPORT lane renders the custom amp's
  // art/cabs (not a factory amp).
  if (GetParam(kSupportAmpIdx)->Int() != -1)
  {
    GetParam(kSupportAmpIdx)->Set(-1.0);
    SendParameterValueFromDelegate(kSupportAmpIdx, GetParam(kSupportAmpIdx)->GetNormalized(), true);
  }
  // ...
  mVolumSupportNeedsLoad.store(true);
}
```

```580:598:NeuralAmpModeler/NeuralAmpModeler.cpp
const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
const bool toneStackActive = GetParam(kEQActive)->Value();
const bool dualAmpActive = GetParam(kDualAmpActive)->Bool();
const bool supportAmpSelected = GetParam(kSupportAmpIdx)->Int() >= 0;
const bool haveSupportModel = supportAmpSelected && (mSupportModel != nullptr);
// ...
const auto processingPlan = volum::MakeProcessingPlan(
  haveMainModel, noiseGateActive, toneStackActive, irActive, mIR != nullptr, GetParam(kPreCompActive)->Bool(),
  preNamActive, havePreNam, GetParam(kDelayActive)->Bool(), GetParam(kReverbActive)->Bool(), mTunerDSP.IsActive(),
  dualAmpActive, haveSupportModel, supportToneStackActive, supportIrActive, mSupportIR != nullptr,
  GetParam(kPrePitchActive)->Bool(), GetParam(kTremoloActive)->Bool());
```

**Mechanism:** Selecting a custom SUPPORT amp deliberately sets the factory `kSupportAmpIdx` to `-1` and identifies the lane through `mVolumCustomSupportIdx`. The audio-side `haveSupportModel` gate ignores that member and requires `kSupportAmpIdx >= 0`. Therefore `haveSupportModel` is false for every custom SUPPORT selection even after its model has loaded, and `MakeProcessingPlan` disables the support model/dual lane.

**Trigger:** Enable Dual Amp, focus SUPPORT, and choose any custom amp from the SUPPORT picker.

**Impact:** The hero, cab row, channel stepper, and loader all behave as though the custom SUPPORT amp is active, but the output contains only MAIN. The advertised custom SUPPORT path is unusable and the UI materially misreports the audio graph.

**Fix sketch:** Define support presence from either source: a valid factory support index **or** a valid custom support index, together with a loaded `mSupportModel`. Keep the factory/custom identity distinction only in the resolver/loader.

**Proposed regression test:** `CustomSupportModelParticipatesInProcessingPlan` — set `kSupportAmpIdx = -1`, `mVolumCustomSupportIdx >= 0`, Dual Amp on, and provide `mSupportModel`; assert `runSupportModel` and `runDualAmp` are true.

### F-P6-3: MAJOR — Deleting the active custom MAIN amp leaves its deleted capture playing behind factory UI

**Where:** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:155-200`

**Evidence:**

```163:200:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
auto doDelete = [this, customIdx]() {
  volum::custom::RemoveCustomAmp(customIdx);
  auto* pGfx2 = GetUI();
  if (!pGfx2)
    return;
  // ... sidebar and hero are changed to the factory amp ...
  if (mVolumCustomMainIdx == customIdx)
    mVolumCustomMainIdx = -1;
  else if (mVolumCustomMainIdx > customIdx)
    --mVolumCustomMainIdx;
  // ...
  if (mVolumCustomSupportIdx == customIdx)
    mVolumCustomSupportIdx = -1;
  else if (mVolumCustomSupportIdx > customIdx)
    --mVolumCustomSupportIdx;
  _VolumSyncPresetOwner();
  _VolumRefreshPresetBar();
};
```

**Mechanism:** The delete callback removes the content and switches the identity/UI members back to the current factory index, but it never restores that factory amp's settings, rediscovers its channels/cabs, or sets `mVolumNeedsLoad`. The live `mModel` is not removed either. Consequently the last custom `.nam` remains the active DSP model while the hero and preset owner say the factory amp is selected. Deleting an active SUPPORT custom amp similarly clears only its index and performs no runtime teardown/reload.

**Trigger:** Select a custom amp as MAIN, click its delete affordance, and confirm deletion while it is still selected.

**Impact:** The UI claims a factory amp is active while audio continues using the now-deleted custom capture; speaker/channel UI can also remain from the custom manifest. State only converges after a later action happens to request another load.

**Fix sketch:** Before/after removal, detect whether the deleted ID owns MAIN or SUPPORT. For active MAIN, perform the same complete factory transition as a normal factory selection (`restore settings`, refresh/sync cabs and channels, request model load, update layout). For active SUPPORT, explicitly select “none” or another valid partner and remove/reload its model. Do this by stable ID, not only the mutable vector index.

**Proposed regression test:** `DeletingFocusedCustomMainAtomicallyFallsBackToFactory` — delete the active custom MAIN and assert `mVolumCustomMainIdx == -1`, a factory load is requested, factory cab/channel state is applied, and the live/staged model path no longer identifies the deleted capture.

### F-P6-4: MAJOR — Incoming IR is published to the audio thread before the atomic switch is armed

**Where:** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:538-565`, `NeuralAmpModeler/NeuralAmpModeler.cpp:1750-1755`, `NeuralAmpModeler/NeuralAmpModeler.cpp:2068-2085`

**Evidence:**

```538:565:NeuralAmpModeler/VoLumSceneRig.inc.cpp
WDL_String p(absUtf8.c_str());
const dsp::wav::LoadReturnCode loadRc = _StageIR(p, support);
// ...
const int lane = support ? 1 : 0;
(support ? mVolumDeferredRemoveSupportIR : mVolumDeferredRemoveIR).store(false);
// ...
const bool captureLoading = _VolumForceDirectCapture(support);
// ...
(support ? mVolumDeferredApplySupportIrBlocks : mVolumDeferredApplyIrBlocks).store(0);
(support ? mVolumDeferredApplySupportIR : mVolumDeferredApplyIR).store(captureLoading);
mVolumIrShapingPushPending[lane] = captureLoading;
```

```2068:2085:NeuralAmpModeler/NeuralAmpModeler.cpp
{
  // Publish the staged IR (and its path) under the staging mutex so the audio thread
  // sees a fully-constructed object or none at all.
  std::lock_guard<std::mutex> lock(mStagingMutex);
  auto& stagedSlot = support ? mStagedSupportIR : mStagedIR;
  // ...
  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    stagedSlot = std::move(stagedIR);
    volum::dsp_staging::StagePathOnSuccess(pathPair, irPath);
  }
}
```

```1750:1755:NeuralAmpModeler/NeuralAmpModeler.cpp
if (mStagedIR != nullptr && !holdMainIr)
{
  mIR = std::move(mStagedIR);
  mStagedIR = nullptr;
  volum::dsp_staging::CommitStagedPathOnApply(mIRPaths);
}
```

**Mechanism:** `_StageIR()` publishes `mStagedIR` under `mStagingMutex` and returns. Only afterwards does `_VolumSelectIR()` force the DIRECT capture and set `mVolumDeferredApplyIR`. The audio thread can acquire the mutex in that interval, observe `mStagedIR != nullptr` while the hold flag is still false, and commit the incoming IR over the outgoing baked-cab capture. The same ordering allows an IR-to-IR switch to expose the new convolver before its scene ID and trim/cut atomics are updated.

**Trigger:** Select a custom IR while audio is running, especially at a small host block size or when the UI thread is preempted immediately after `_StageIR()` publishes.

**Impact:** The newly advertised “atomic” baked-cab → IR move is racy: a block (or more) can process baked cab + incoming IR, and IR-to-IR changes can briefly use the previous IR's shaping. This is exactly the audible half-applied combination the transaction is intended to prevent.

**Fix sketch:** Arm and publish the whole transaction under one synchronization boundary. The audio thread must not be able to see the staged IR until the corresponding apply/remove state, counter, target capture generation, and shaping payload are all ready. A single transaction/generation object is safer than independent relaxed atomics.

**Proposed regression test:** `IncomingIrCannotCommitBeforeDirectCaptureTransaction` — instrument a barrier immediately after staged-IR publication, run one `_ApplyDSPStaging()` block, and assert the live IR remains unchanged until the matching DIRECT model is staged; repeat with different per-IR shaping and assert no new-IR/old-shaping state is observable.

### F-P6-5: MAJOR — A failed replacement capture still commits the IR against the wrong amp capture

**Where:** `NeuralAmpModeler/VoLumDspStaging.h:59-75`, `NeuralAmpModeler/VoLumLoader.inc.cpp:170-176`, `NeuralAmpModeler/NeuralAmpModeler.cpp:1812-1823`

**Evidence:**

```59:75:NeuralAmpModeler/VoLumDspStaging.h
// `maxWaitBlocks` bounds the wait: a capture that fails to load (or never arrives)
// must not strand the lane mid-switch, which would be a worse artifact than the
// seam this avoids. Zero or negative disables deferral entirely.
inline DeferredIrSwapStep StepDeferredIrSwap(bool pending, int waitedBlocks, bool replacementStaged, int maxWaitBlocks)
{
  // ...
  if (replacementStaged || waited >= maxWaitBlocks)
  {
    out.fire = true;
    out.waitedBlocks = 0;
    return out;
  }
  out.stillPending = true;
  // ...
}
```

```170:176:NeuralAmpModeler/VoLumLoader.inc.cpp
if (!result.error.empty())
{
  // Keep the last known-good model for uninterrupted audio, but tell the
  // main/UI thread to make the fallback explicit in the footer.
  mVolumMainLoadFailed.store(true);
  VOLUM_LOG("model", "MAIN load FAILED " + result.path + " : " + result.error);
  continue;
}
```

```1812:1823:NeuralAmpModeler/NeuralAmpModeler.cpp
const bool mainStaged = mStagedModel != nullptr;
const bool supportStaged = mStagedSupportModel != nullptr;
// ...
// The addition waits the other way round: the staged IR stays parked until this
// block, so the caller must leave it where it is.
holdMainIr = step(mVolumDeferredApplyIR, mVolumDeferredApplyIrBlocks, mainStaged).stillPending;
holdSupportIr = step(mVolumDeferredApplySupportIR, mVolumDeferredApplySupportIrBlocks, supportStaged).stillPending;
```

**Mechanism:** A baked-cab → custom-IR move requires a DIRECT capture load. On MAIN load failure, the loader intentionally keeps the outgoing baked-cab model and never stages the DIRECT replacement. The deferred stepper nevertheless fires after its block deadline, releases the staged IR, and clears the pending state. The IR then becomes live on top of the retained baked-cab model. The scene/UI already says Custom IR/No Cab. SUPPORT failure is worse: that loader removes the support model, after which the timeout can release an IR with no support amp model feeding it.

**Trigger:** Select a custom IR when the lane's DIRECT `.nam` is missing, unreadable, corrupt, or fails parser/reset after import (for example, delete the copied DIRECT file externally before selecting the IR).

**Impact:** The transaction permanently lands in a half-applied state: MAIN sounds like baked cab + custom IR while UI says raw amp + custom IR; SUPPORT can become silent. The timeout converts a load error into persistent wrong routing rather than rolling back.

**Fix sketch:** Associate loader success/failure with the cab-source transaction generation. On explicit failure, cancel/rollback the incoming IR (and its UI/scene/toggle) or restore the prior complete source; use the deadline only for a genuinely lost completion signal, not as authorization to commit an unmatched pair.

**Proposed regression test:** `DirectCaptureFailureRollsBackIncomingIr` — start from a baked cab, stage an IR, return a failed DIRECT model result, advance beyond `maxWaitBlocks`, and assert the old model and old cab source remain coherent, the incoming IR is not committed, and UI/scene state is rolled back.

### F-P6-6: MINOR — Reopened gate/EQ controls lose their disabled state

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:1141-1155`, `NeuralAmpModeler/NeuralAmpModeler.cpp:1480-1540`, `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:497-524`

**Evidence:**

```1480:1540:NeuralAmpModeler/NeuralAmpModeler.cpp
switch (paramIdx)
{
  case kNoiseGateActive:
    pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active);
    break;
  // ...
  case kSupportNoiseGateActive:
    if (auto* c = pGraphics->GetControlWithParamIdx(kSupportNoiseGateThreshold))
      c->SetDisabled(!active);
    break;
  case kSupportEQActive:
    // support tone controls disabled here
    break;
  case kEQActive:
    // main tone controls disabled here
    break;
}
```

```1141:1155:NeuralAmpModeler/NeuralAmpModeler.cpp
void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();
  if (mModel != nullptr)
    _UpdateControlsFromModel();
  _UpdateLatency();
  _VolumRestoreSessionSelection();
  _VolumSyncUiFromState();
}
```

**Mechanism:** Fresh knob controls are constructed enabled. Their disabled state is applied only in `OnParamChangeUI` when the corresponding gate/EQ active parameter changes. The layout build contains no initial `SetDisabled` pass, `_UpdateVoLumLayout()` intentionally contains no disabling, and `OnUIOpen()` only refreshes model metadata plus cab/identity state. Therefore an already-off gate or EQ does not replay the UI-only disabled property onto newly created controls.

**Trigger:** Turn MAIN or SUPPORT Noise Gate/EQ off, close the editor, then reopen it without changing that parameter.

**Impact:** The reopened controls look and behave enabled even though the DSP block is off, contradicting the state shown before close and the live toggle.

**Fix sketch:** Add a single attach/reopen UI-state synchronization routine that reapplies all non-parameter control properties, including gate/EQ disabled state, after the full editor tree exists.

**Proposed regression test:** `EditorReopenReappliesGateEqDisablement` — restore all four active flags false into a freshly built editor and assert the corresponding threshold/tone controls are disabled before any new parameter event.

### F-P6-7: MINOR — Hero title ellipsis can return malformed UTF-8

**Where:** `NeuralAmpModeler/VoLumHero.h:417-440`

**Evidence:**

```417:440:NeuralAmpModeler/VoLumHero.h
static std::string FitTextToWidth(IGraphics& g, const IText& text, const char* s, float maxW)
{
  // ...
  while (str.size() > 1)
  {
    str.pop_back();
    // Avoid leaving a dangling UTF-8 lead/continuation byte.
    while (!str.empty() && (static_cast<unsigned char>(str.back()) & 0xC0) == 0x80)
      str.pop_back();
    const std::string cand = str + "\u2026";
    // ...
    if (mr.W() <= maxW)
      return cand;
  }
  return str + "\u2026";
}
```

**Mechanism:** When `pop_back()` removes the final continuation byte of a multi-byte character, the loop removes the remaining continuation bytes but stops with the character's lead byte still in `str`. If that candidate now fits, it returns the orphaned lead byte followed by an ellipsis, which is invalid UTF-8.

**Trigger:** Give a custom amp a long non-ASCII name (for example containing `ö`, `Δ`, CJK, or emoji) whose title width requires truncation at that character boundary.

**Impact:** The MAIN/SUPPORT title can show a replacement glyph, missing text, or backend-dependent rendering corruption for valid user-entered names.

**Fix sketch:** Truncate by Unicode code point/grapheme boundary, or after removing continuation bytes also remove the corresponding lead byte before measuring the candidate.

**Proposed regression test:** `HeroEllipsisNeverSplitsUtf8CodePoint` — run the truncation helper across every byte-boundary-sized width for representative 2/3/4-byte names and assert the returned string is valid UTF-8 and ends only at a complete code point.

## Voicing observations (report only)

None. No filter curves, drive amounts, envelope times, mix laws, or effect-tuning changes are proposed.

## Areas read and found clean

- `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp` — read all 1,524 lines. Apart from findings above, control tags/groups, attach order, transient-menu null checks, minimum logical geometry, and scaled corner-resizer construction were internally consistent.
- `NeuralAmpModeler/VoLumLayoutRuntime.inc.cpp` — read all 406 lines. PRE/AMP/POST visibility teardown, mode-conditional groups, pedal-card relayout, and hidden hit-target handling were consistent.
- `NeuralAmpModeler/VoLumSceneRig.inc.cpp` — read all 944 lines. Factory/custom channel clamping, focused-lane cab planning, IR chip reflection, and pre-capture menu bounds were clean outside the IR transaction findings.
- `NeuralAmpModeler/VoLumUiSyncPlan.h` — read all 175 lines. Factory/custom plan mapping, restored custom channel-position handling, IR resolution flags, and cab enablement were coherent.
- `NeuralAmpModeler/VoLumHero.h` — read all 538 lines. Hero cache invalidation, DPI rescale invalidation, lane hit testing, dual/support geometry, and art/name switching were clean apart from UTF-8 truncation.
- `NeuralAmpModeler/VoLumTriptychState.h` — read all 4 lines; enum membership and ordering were consistent with the assigned call sites.
- `NeuralAmpModeler/NeuralAmpModeler.cpp` — read the editor open/close paths and all of `OnParamChange` / `OnParamChangeUI`, plus the audio staging and processing call sites required to prove the tuner, custom SUPPORT, and atomic-switch mechanisms.
- `NeuralAmpModeler/VoLumAmpMenus.inc.cpp`, `VoLumDspStaging.h`, `VoLumLoader.inc.cpp`, `VoLumProcessingPlan.h`, and the relevant `VoLumTunerMetronomeOverlay.h` call sites were read narrowly to close the concrete execution paths above.
