// VoLumSceneRig.inc.cpp: tuner/metronome toggles + channel/PRE-capture/custom-amp/IR rig member functions
// Extracted from NeuralAmpModeler.cpp for file-size hygiene. Tail-#included
// into the NeuralAmpModeler translation unit; not a separate build target.

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

void NeuralAmpModeler::_ToggleVoLumMetronomePanel()
{
  if (auto* pGfx = GetUI())
  {
    auto* panel = pGfx->GetControlWithTag(kCtrlTagVoLumMetronome)->As<VoLumMetronomeControl>();
    if (panel->IsHidden())
    {
      panel->Show(
        mMetronomeDSP.IsActive(), mMetronomeDSP.GetBPM(), mMetronomeDSP.GetVolume(), mMetronomeDSP.GetTimeSig());
    }
    else
    {
      panel->Hide(true);
    }
  }
}

void NeuralAmpModeler::_VolumRefreshChannels()
{
  if (mVolumRigsRoot.empty())
    return;

  if (mVolumSpeakerIdx < 0 || mVolumSpeakerIdx >= 4)
  {
    mVolumSpeakerIdx = std::clamp(mVolumSpeakerIdx, 0, 3);
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = mVolumSpeakerIdx;
    mVolumSettingsDirty = true;
  }

  auto channels =
    volum::DiscoverChannels(volum::content::PathFromUtf8(mVolumRigsRoot), volum::kAmps[mVolumAmpIdx].folderName,
                            volum::kSpeakerPrefixes[mVolumSpeakerIdx]);

  mVolumChannelFiles.clear();
  mVolumChannelLabels.clear();
  for (auto& ch : channels)
  {
    mVolumChannelFiles.push_back(std::move(ch.filename));
    mVolumChannelLabels.push_back(std::move(ch.label));
  }

  if (mVolumChannelIdx < 0 || mVolumChannelIdx >= (int)mVolumChannelFiles.size())
  {
    mVolumChannelIdx = 0;
    mVolumAmpSettings[mVolumAmpIdx].channelIdx = mVolumChannelIdx;
    mVolumSettingsDirty = true;
  }

  if (auto* pGfx = GetUI())
  {
    if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumChannelStep))
      stepper->As<VoLumChannelStepControl>()->SetChannels(mVolumChannelLabels, mVolumChannelIdx);
  }
}

void NeuralAmpModeler::_VolumRefreshPrePedalCaptures()
{
  mVolumPreCaptureFiles.clear();
  mVolumPreCaptureLabels.clear();
  mVolumPreCaptureShortLabels.clear();
  mVolumPreCaptureGroups.clear();

  auto addMockCaptures = [&]() {
    struct MockCapture
    {
      const char* label;
      const char* shortLabel;
      volum::PrePedalCaptureGroup group;
    };
    const MockCapture captures[] = {
      {"Klon - Gold Horse", "Klon", volum::PrePedalCaptureGroup::Klon},
      {"TS - Green Drive", "TS", volum::PrePedalCaptureGroup::TsBoost},
      {"Fuzz - Velvet Doom", "Fuzz", volum::PrePedalCaptureGroup::Fuzz},
      {"Nuke - Petty Push", "Nuke", volum::PrePedalCaptureGroup::Fuzz},
      {"Boost - Clean Lift", "Boost", volum::PrePedalCaptureGroup::TsBoost},
    };
    for (const auto& capture : captures)
    {
      mVolumPreCaptureFiles.emplace_back();
      mVolumPreCaptureLabels.emplace_back(capture.label);
      mVolumPreCaptureShortLabels.emplace_back(capture.shortLabel);
      mVolumPreCaptureGroups.emplace_back(capture.group);
    }
  };

  if (mVolumRigsRoot.empty())
  {
    addMockCaptures();
    return;
  }

  const auto captures = volum::DiscoverPrePedalCaptures(volum::content::PathFromUtf8(mVolumRigsRoot));
  for (const auto& capture : captures)
  {
    mVolumPreCaptureFiles.push_back(capture.filename);
    mVolumPreCaptureLabels.push_back(capture.label);
    mVolumPreCaptureShortLabels.push_back(capture.shortLabel);
    mVolumPreCaptureGroups.push_back(capture.group);
  }

  if (mVolumPreCaptureLabels.empty())
    addMockCaptures();
}

const char* NeuralAmpModeler::_VolumGetPreCaptureLabel(int captureIdx) const
{
  // Custom imported pedals (F8) live at stable indices >= kCustomPedalIndexBase,
  // outside the contiguous factory label vector. Resolve their name via a stable
  // scratch buffer so we can hand back a const char*.
  if (captureIdx >= volum::content::kCustomPedalIndexBase)
  {
    mVolumPreCaptureLabelScratch = volum::custom::PedalNameByLegacy(captureIdx);
    return mVolumPreCaptureLabelScratch.empty() ? "Click to change" : mVolumPreCaptureLabelScratch.c_str();
  }
  if (captureIdx <= 0 || captureIdx > static_cast<int>(mVolumPreCaptureLabels.size()))
    return "Click to change";
  return mVolumPreCaptureLabels[static_cast<size_t>(captureIdx - 1)].c_str();
}

const char* NeuralAmpModeler::_VolumGetPreCaptureShortLabel(int captureIdx, const char* fallback) const
{
  if (captureIdx >= volum::content::kCustomPedalIndexBase)
  {
    // Custom pedal captures carry the user's full import name; the Amp-view quiet
    // slot pill is tiny, so cap it like the factory short labels (curated 3-5
    // chars) instead of letting a long name overflow into the next pill.
    const std::string full = volum::custom::PedalNameByLegacy(captureIdx);
    if (full.empty())
      return fallback;
    mVolumPreCaptureLabelScratch = volum::custom::ShortCaptureLabel(full);
    return mVolumPreCaptureLabelScratch.c_str();
  }
  if (captureIdx <= 0 || captureIdx > static_cast<int>(mVolumPreCaptureShortLabels.size()))
    return fallback;
  return mVolumPreCaptureShortLabels[static_cast<size_t>(captureIdx - 1)].c_str();
}

int NeuralAmpModeler::_VolumGetPreCaptureCount() const
{
  return static_cast<int>(mVolumPreCaptureLabels.size()) + 1; // zero is EMPTY
}

std::string NeuralAmpModeler::_VolumGetPreCaptureFilename(int captureIdx) const
{
  if (captureIdx <= 0 || captureIdx > static_cast<int>(mVolumPreCaptureFiles.size()))
    return {};
  return mVolumPreCaptureFiles[static_cast<size_t>(captureIdx - 1)];
}

std::string NeuralAmpModeler::_VolumGetPreCaptureLoadPath(int captureIdx) const
{
  if (captureIdx >= volum::content::kCustomPedalIndexBase)
  {
    const std::string rel = volum::custom::PedalStoredPathByLegacy(captureIdx);
    if (rel.empty())
      return {};
    return volum::content::PathToUtf8(volum::content::GlobalContentStore().ResolveStored(rel));
  }
  const std::string fn = _VolumGetPreCaptureFilename(captureIdx);
  if (fn.empty() || mVolumRigsRoot.empty())
    return {};
  return volum::content::PathToUtf8(volum::content::PathFromUtf8(mVolumRigsRoot) / "PrePedals" / fn);
}

void NeuralAmpModeler::_VolumSetPreNamCapture(int slot, int captureIdx)
{
  if (slot < 0 || slot >= 2)
    return;

  const int paramIdx = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  // Custom imported pedals use stable indices up to the param max (127), so clamp
  // to the full param range rather than the factory count.
  const int next = std::clamp(captureIdx, 0, volum::kPreCaptureMaxParamIndex);
  if (GetParam(paramIdx)->Int() == next)
    return;

  GetParam(paramIdx)->Set(next);
  SendParameterValueFromDelegate(paramIdx, GetParam(paramIdx)->GetNormalized(), true);
  mVolumPreNeedsLoad[slot].store(true);
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumShowPreCaptureMenu(int slot, const IRECT& anchorRect)
{
  if (slot < 0 || slot >= 2)
    return;

  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  auto* rawCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumPreCaptureMenu);
  if (!rawCtrl)
    return;

  auto* menu = rawCtrl->As<VoLumPreCaptureMenuControl>();
  if (!rawCtrl->IsHidden() && menu && menu->GetSlot() == slot)
  {
    _VolumHidePreCaptureMenu();
    return;
  }

  const int captureCount = std::max(1, _VolumGetPreCaptureCount());
  std::vector<VoLumPreCaptureMenuItem> items;
  items.reserve(static_cast<size_t>(captureCount + 4));
  items.push_back({_VolumGetPreCaptureLabel(0), 0, false, volum::PrePedalCaptureGroup::None});
  volum::PrePedalCaptureGroup lastGroup = volum::PrePedalCaptureGroup::None;
  for (int i = 1; i < captureCount; ++i)
  {
    const auto group = (i - 1 < static_cast<int>(mVolumPreCaptureGroups.size()))
                         ? mVolumPreCaptureGroups[static_cast<size_t>(i - 1)]
                         : volum::PrePedalCaptureGroup::None;
    if (group != lastGroup)
    {
      items.push_back({volum::PrePedalCaptureGroupLabel(group), 0, true, group});
      lastGroup = group;
    }
    items.push_back({_VolumGetPreCaptureLabel(i), i, false, group});
  }

  // F8: CUSTOM group lists imported pedals; a single "Manage custom pedals..."
  // row opens the shared Manage panel (CRUD + import via file dialog).
  // captureIdx -1 keeps these rows from ever matching a real selected capture
  // (which is always >= 0), so they never render with a false selection dot.
  items.push_back({"CUSTOM", -1, true, volum::PrePedalCaptureGroup::None});
  {
    const auto& peds = volum::content::GlobalContentStore().reg().pedals;
    for (const auto& p : peds)
      items.push_back({p.name, p.legacyIndex, false, volum::PrePedalCaptureGroup::None, PreMenuAction::None, true});
  }
  items.push_back(
    {"Manage custom pedals...", -1, false, volum::PrePedalCaptureGroup::None, PreMenuAction::Manage, false});

  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  // Unclamped so a custom-pedal selection (index >= kCustomPedalIndexBase) marks
  // the right row instead of a factory row.
  const int selected = GetParam(captureParam)->Int();
  const float menuW = std::max(anchorRect.W() * 0.88f, 180.f);
  const float fullH = VoLumPreCaptureMenuControl::MenuHeight(items);
  // Cap the menu to the window: drop below the anchor when it fits, otherwise
  // grow upward, and finally cap to the usable window height so a long imported
  // pedal list never spills off-screen - the menu scrolls internally instead.
  const IRECT uiBounds = GetUI()->GetBounds();
  const float top = anchorRect.B + 6.f;
  const float spaceBelow = uiBounds.B - 6.f - top;
  const float maxOnScreen = uiBounds.H() - 12.f;
  float menuT, menuH;
  if (fullH <= spaceBelow)
  {
    menuT = top;
    menuH = fullH;
  }
  else
  {
    menuH = std::min(fullH, maxOnScreen);
    menuT = std::max(uiBounds.T + 6.f, uiBounds.B - 6.f - menuH);
  }
  const IRECT menuRect(anchorRect.L, menuT, anchorRect.L + menuW, menuT + menuH);

  menu->SetTargetAndDrawRECTs(menuRect);
  menu->SetItems(slot, items, selected);
  menu->Hide(false);
}

void NeuralAmpModeler::_VolumShowManageCustomPedals(int preSlot)
{
  if (auto* pGfx = GetUI())
    if (auto* ov = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
      ov->As<VoLumCustomOverlayControl>()->ShowManage(
        VoLumCustomOverlayControl::ManageKind::Pedals, 0, nullptr, preSlot);
}

void NeuralAmpModeler::_VolumMarkPresetDirty()
{
  // Rig edits that bypass the kUI param hook (cab/channel/IR/polarity changes set
  // members or send kDelegate) funnel here. The "(unsaved)" flag is an equality
  // test against the recalled snapshot, so an A/B edit back to the preset clears
  // it; with no recalled preset there is nothing to diverge from.
  _VolumRecomputePresetDirty();
}

void NeuralAmpModeler::_VolumSelectCustomAmp(int customIdx)
{
  const auto& names = volum::custom::MockCustomAmps();
  if (customIdx < 0 || customIdx >= (int)names.size())
    return;

  // Scene isolation (F6): snapshot the outgoing lane (factory slot or the prior
  // custom scene, via the redirect in _VolumSaveCurrentToSettings), then make the
  // new custom amp active and apply its own scene (defaults on first focus).
  if (mVolumInitComplete)
    _VolumSaveCurrentToSettings();
  mVolumCustomMainIdx = customIdx;
  // Point the preset bank at this custom amp's owner key and drop any recalled
  // preset carried over from the previous amp.
  _VolumSyncPresetOwner();
  const std::string ampId = volum::custom::CustomAmpIdAt(customIdx);
  if (!ampId.empty())
    _VolumApplyAmpSettings(_VolumCustomScene(ampId));

  auto* pGfx = GetUI();
  if (!pGfx)
  {
    _VolumApplyCustomMainCabs(customIdx);
    return;
  }
  if (auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
  {
    auto* hero = heroCtrl->As<VoLumHeroImageControl>();
    hero->SetCustomArt(true, volum::custom::CustomAmpArt(customIdx));
    hero->SetName(names[(size_t)customIdx].c_str());
  }
  if (auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
    if (mVolumExpandedSection == EVoLumSection::AMP)
      nameCtrl->As<VoLumSubRowTextControl>()->SetName(names[(size_t)customIdx].c_str(), true);
  _VolumRefreshPresetBar(); // this custom amp's preset bank
  if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
    al->As<VoLumAmpListControl>()->SetCustomSelected(customIdx);
  // Make the shared cabinet row + channel stepper reflect this custom amp.
  _VolumApplyCustomMainCabs(customIdx);
}

// Build the pure planner's input for one lane from live backend state. The only
// subtlety is the channel: MAIN persists the stepper POSITION (mVolumChannelIdx)
// while SUPPORT persists the gain-stage NUMBER, so SUPPORT is converted to a
// position here and the planner then speaks one language for both lanes.
volum::UiSyncInput NeuralAmpModeler::_VolumMakeUiSyncInput(bool support, const volum::custom::CustomAmp& customAmp)
{
  volum::UiSyncInput in;
  in.supportFocused = support;

  const int customLane = support ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane >= 0)
  {
    in.customAmp = &customAmp;
    in.customAmpIdx = customLane;
    if (support)
    {
      in.customSlot = mVolumCustomSupportSlot;
      in.customChannelPos =
        volum::custom::ChannelStepIndex(volum::custom::AssignedChannels(customAmp), mVolumCustomSupportChannel);
    }
    else
    {
      in.customSlot = (mVolumSpeakerIdx == 0) ? volum::custom::kDirectSlot : (mVolumSpeakerIdx - 1);
      in.customChannelPos = mVolumChannelIdx;
    }
  }
  else
  {
    in.factoryAmpIdx = mVolumAmpIdx;
    in.factorySpeakerIdx = support ? std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3) : mVolumSpeakerIdx;
    in.factoryChannelIdx = support ? GetParam(kSupportChannelIdx)->Int() : mVolumChannelIdx;
    in.factoryChannelLabels = support ? mVolumSupportChannelLabels : mVolumChannelLabels;
  }

  const std::string& laneIrId = support ? _VolumActiveScene().supportActiveIrId : _VolumActiveScene().activeIrId;
  in.irIdPresent = !laneIrId.empty();
  const int irIdx = in.irIdPresent ? volum::custom::IRIndexById(laneIrId) : -1;
  const auto& irNames = volum::custom::MockIRLibrary();
  if (irIdx >= 0 && irIdx < static_cast<int>(irNames.size()))
  {
    in.irResolved = true;
    in.irName = irNames[static_cast<size_t>(irIdx)];
  }
  return in;
}

// Apply one resolved plan to the shared cab row + channel stepper. Everything the
// editor shows for a lane is written here, so a freshly built control never keeps
// a constructor default.
void NeuralAmpModeler::_VolumApplyUiSyncPlan(const volum::UiSyncPlan& plan, bool support)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl)
    return;
  auto* row = spkCtrl->As<VoLumSpeakerRowControl>();

  // The cab row is one control shared by both lanes, showing whichever is focused.
  // Every other function that writes it checks that first (_VolumSelectIR,
  // _VolumClearIR, _VolumForceDirectCapture, _VolumReconcileActiveIr); this one did
  // not. Reconciling the background lane therefore left the row describing that lane
  // - recalling a preset on a custom MAIN amp with a custom SUPPORT partner ended
  // with SUPPORT's cab names, enables, selection and IR chip on screen while MAIN was
  // focused, and a click on a cab then edited MAIN with an index belonging to the
  // support amp's layout. The routing caches and scene writes below stay
  // unconditional: the background lane still has to stage its own .nam.
  const bool laneFocused = (support == _VolumSupportFocused());

  if (laneFocused)
  {
    if (plan.useFactoryCabNames)
      row->SetFactoryCabs();
    else
      row->SetCabNames(plan.cabNames[0], plan.cabNames[1], plan.cabNames[2]);
    row->SetNoCabEnabled(plan.noCabEnabled, "No DIRECT capture on this channel");
    row->SetIrEnabled(plan.irEnabled, "Custom IR needs a DIRECT capture");
  }

  // The resolved channel cannot host this lane's stored IR; drop it so a real cab
  // takes over. Unconditional, because it repairs state and not just the display -
  // and _VolumClearIR guards its own row writes on focus. That cab's capture is
  // staged below, so the removal rides along with it.
  if (plan.clearOrphanedIr)
    _VolumClearIR(support, /*deferToCabSwap=*/true);

  if (laneFocused)
  {
    row->SetIrCab(plan.irCabActive, plan.irName.c_str());
    row->SetSelected(plan.cabSelectedIndex);
  }

  const int stepperTag = support ? kCtrlTagVoLumSupportChannelStep : kCtrlTagVoLumChannelStep;
  if (auto* stepper = pGfx->GetControlWithTag(stepperTag))
    stepper->As<VoLumChannelStepControl>()->SetChannels(plan.channelLabels, plan.channelSelectedPos);

  if (plan.sidebarCustomIdx < 0)
    return; // factory lane: routing caches are already the source of truth

  // Custom lane: record the resolved (slot, channel) and stage that lane's .nam.
  if (support)
  {
    mVolumCustomSupportSlot = plan.customSlot;
    mVolumCustomSupportChannel = plan.customChannel;
    _VolumActiveScene().supportCustomSlot = plan.customSlot;
    _VolumActiveScene().supportCustomChannel = plan.customChannel;
    mVolumSupportNeedsLoad.store(true);
  }
  else
  {
    mVolumCustomMainSlot = plan.customSlot;
    mVolumCustomMainChannel = plan.customChannel;
    mVolumSpeakerIdx = plan.cabSelectedIndex;
    mVolumChannelIdx = plan.channelSelectedPos;
    mVolumNeedsLoad.store(true);
  }
}

void NeuralAmpModeler::_VolumApplyCustomMainCabs(int customIdx, bool supportLane)
{
  if (GetUI() == nullptr)
    return;
  const auto amp = volum::custom::CustomAmpAt(customIdx);
  _VolumApplyUiSyncPlan(volum::MakeUiSyncPlan(_VolumMakeUiSyncInput(supportLane, amp)), supportLane);
}

void NeuralAmpModeler::_VolumSetCustomChannelStepper(int customIdx, bool supportLane, int channel)
{
  // Channel-first: the stepper lists the amp-WIDE gain-stage set (so every channel
  // is reachable regardless of the current cab), with `channel` selected.
  const auto amp = volum::custom::CustomAmpAt(customIdx);
  const auto channels = volum::custom::AssignedChannels(amp);
  std::vector<std::string> labels;
  for (int c : channels)
    labels.push_back(std::to_string(c));
  if (labels.empty())
    labels.push_back("1"); // amp with no assigned files yet
  const int sel = volum::custom::ChannelStepIndex(channels, channel);
  // The MAIN and SUPPORT lanes have separate channel steppers; drive whichever
  // belongs to the focused lane.
  const int tag = supportLane ? kCtrlTagVoLumSupportChannelStep : kCtrlTagVoLumChannelStep;
  if (auto* pGfx = GetUI())
    if (auto* stepper = pGfx->GetControlWithTag(tag))
      stepper->As<VoLumChannelStepControl>()->SetChannels(labels, sel);
}

// This instance's live scene for one custom amp. Lazily seeded from a pre-1.3.0
// library's shared customScenes so upgrading does not reset the knobs, and from
// the amp's factory-default settings otherwise (first focus).
//
// Draining the migration entry rather than copying it is deliberate: the library
// stops writing that map, so leaving it in place would let a later focus of the
// same amp pull stale pre-upgrade knobs over what the user has since done.
volum::VoLumAmpSettings& NeuralAmpModeler::_VolumCustomScene(const std::string& ampId)
{
  const auto existing = mVolumCustomScenes.find(ampId);
  if (existing != mVolumCustomScenes.end())
    return existing->second;

  auto& legacy = volum::content::GlobalContentStore().reg().legacyCustomScenes;
  const auto migrated = legacy.find(ampId);
  if (migrated != legacy.end())
  {
    auto& scene = mVolumCustomScenes[ampId];
    scene = migrated->second;
    legacy.erase(migrated);
    return scene;
  }
  return mVolumCustomScenes[ampId];
}

volum::VoLumAmpSettings& NeuralAmpModeler::_VolumActiveScene()
{
  if (mVolumCustomMainIdx >= 0)
  {
    const std::string id = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
    if (!id.empty())
      return _VolumCustomScene(id);
  }
  return mVolumAmpSettings[mVolumAmpIdx];
}

const char* NeuralAmpModeler::_VolumMainAmpDisplayName() const
{
  if (mVolumCustomMainIdx >= 0)
  {
    const auto& customAmps = volum::custom::MockCustomAmps();
    if (mVolumCustomMainIdx < static_cast<int>(customAmps.size()))
      return customAmps[(size_t)mVolumCustomMainIdx].c_str();
  }
  return volum::kAmps[mVolumAmpIdx].displayName;
}

void NeuralAmpModeler::_VolumSelectIR(int irIdx, bool support, bool interactive)
{
  const std::string id = volum::custom::IRIdAt(irIdx);
  const std::string rel = volum::custom::IRFileAt(irIdx);
  if (id.empty() || rel.empty())
    return;
  // A custom IR convolves the amp's DIRECT (raw) capture. A custom amp with no
  // DIRECT capture has nothing to feed the IR, so refuse the selection (the cab
  // row already greys the button out; this guards the menu/dialog/restore paths).
  const int customLane = support ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane >= 0)
  {
    // Channel-first: a custom IR needs a DIRECT capture ON THE CURRENT channel (the
    // raw signal it convolves). An amp may have DIRECT on one channel but not the
    // one in focus, so this is a per-channel gate, not the amp-wide HasDirectCapture.
    //
    // MAIN's channel comes from the persisted stepper POSITION, for the same reason
    // _VolumForceDirectCapture below does it: mVolumCustomMainChannel is a runtime
    // cache still holding the *previous* amp's stage while a restore is in flight,
    // and every restore path reaches this gate before anything refreshes it. Reading
    // it here cleared the scene's IR id whenever that stale stage happened to lack a
    // DIRECT capture - and because the cleared id is written straight back out on the
    // next save, the user's IR reference was destroyed, not merely ignored for the
    // session. SUPPORT persists its gain stage directly, so it can be read as-is.
    const auto amp = volum::custom::CustomAmpAt(customLane);
    const int laneChannel = support ? mVolumCustomSupportChannel : volum::CustomChannelAtStep(amp, mVolumChannelIdx);
    if (!volum::custom::ChannelHasDirect(amp, laneChannel))
    {
      if (support)
        _VolumActiveScene().supportActiveIrId.clear();
      else
        _VolumActiveScene().activeIrId.clear();
      if (interactive)
        if (auto* pGfx = GetUI())
          _ShowMessageBox(pGfx,
                          "This channel has no DIRECT capture, so a custom IR has no raw signal to "
                          "convolve.\n\nSwitch to a channel with a DIRECT (AMP-/DI-) capture to use a custom IR.",
                          "Impulse Response", EMsgBoxType::kMB_OK);
      return;
    }
  }
  const auto abs = volum::content::GlobalContentStore().ResolveStored(rel);
  const std::string absUtf8 = volum::content::PathToUtf8(abs);
  // VoLum: reject unreasonably large IRs before decoding the whole file (the
  // convolver only uses the first ~8192 samples). Interactive guard with a clear
  // message; the restore path validated size at import time.
  std::string sizeWhy;
  if (!volum::IrFileSizeAcceptable(absUtf8, sizeWhy))
  {
    if (auto* pGfx = GetUI())
      _ShowMessageBox(pGfx, sizeWhy.c_str(), "Impulse Response", EMsgBoxType::kMB_OK);
    return;
  }
  WDL_String p(absUtf8.c_str());
  const dsp::wav::LoadReturnCode loadRc = _StageIR(p, support);
  if (loadRc != dsp::wav::LoadReturnCode::SUCCESS)
  {
    // VoLum: surface why the IR did not activate instead of failing silently.
    if (auto* pGfx = GetUI())
    {
      const std::string msg =
        "VoLum could not load this impulse response.\n\n" + dsp::wav::GetMsgForLoadReturnCode(loadRc);
      _ShowMessageBox(pGfx, msg.c_str(), "Impulse Response", EMsgBoxType::kMB_OK);
    }
    return;
  }
  // Picking an IR again while a cab swap is still waiting cancels that swap. The
  // removal drops the staged IR as well as the live one, so letting it fire now
  // would silently throw away the IR just chosen.
  const int lane = support ? 1 : 0;
  (support ? mVolumDeferredRemoveSupportIR : mVolumDeferredRemoveIR).store(false);
  // A custom IR replaces the baked cab: force this lane's amp onto its DIRECT /
  // No-Cab capture so the IR convolves the raw amp (not amp + baked cab).
  const bool captureLoading = _VolumForceDirectCapture(support);
  // That capture loads asynchronously. Convolving the IR before it arrives would
  // stack the IR on the baked cab the lane is still playing - an audible jump on
  // the way in, mirroring the cab-less gap on the way out - so the staged IR waits
  // for it, and its trim and cuts wait with it.
  (support ? mVolumDeferredApplySupportIrBlocks : mVolumDeferredApplyIrBlocks).store(0);
  (support ? mVolumDeferredApplySupportIR : mVolumDeferredApplyIR).store(captureLoading);
  mVolumIrShapingPushPending[lane] = captureLoading;
  const int toggle = support ? kSupportIRToggle : kIRToggle;
  if (support)
    _VolumActiveScene().supportActiveIrId = id;
  else
    _VolumActiveScene().activeIrId = id;
  if (!captureLoading)
    _VolumPushIrShaping(support); // apply this IR's trim + low/high cut on the lane
  GetParam(toggle)->Set(1.0);
  SendParameterValueFromDelegate(toggle, GetParam(toggle)->GetNormalized(), true);
  // The shared speaker row shows the focused lane's IR; only update it when this
  // lane is the one on screen (a background restore of the other lane must not
  // overwrite the visible chip).
  if (support == _VolumSupportFocused())
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      {
        const auto& names = volum::custom::MockIRLibrary();
        if (irIdx >= 0 && irIdx < (int)names.size())
          spk->As<VoLumSpeakerRowControl>()->SetIrCab(true, names[(size_t)irIdx].c_str());
      }
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
}

// Switch the given lane's amp onto its DIRECT (No-Cab) capture so a custom IR
// convolves the raw amp. Custom amps without a DIRECT capture keep their current
// cab (best effort - there is no raw signal to convolve). Returns true when a
// capture load was queued: the caller holds the IR back until it lands, so the
// lane never convolves a custom IR over the baked cab it is leaving.
bool NeuralAmpModeler::_VolumForceDirectCapture(bool support)
{
  const bool laneFocused = (support == _VolumSupportFocused());
  auto selectRow0 = [this, laneFocused]() {
    if (!laneFocused)
      return; // the shared row shows the other lane; don't disturb it
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
        spk->As<VoLumSpeakerRowControl>()->SetSelected(0);
  };
  const int customLane = support ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane >= 0)
  {
    const auto amp = volum::custom::CustomAmpAt(customLane);
    // Keep the current gain stage if DIRECT also covers it (item: selecting a
    // custom IR while already on a DIRECT channel must not snap back to 1).
    //
    // MAIN must derive that stage from the persisted stepper POSITION, not from
    // mVolumCustomMainChannel: that member is a runtime cache still sitting at its
    // default (1) during a restore, and reading it here is what made a saved
    // channel 5 come back as channel 1 whenever the scene also carried an IR.
    // SUPPORT persists its gain stage directly, so it can be read as-is.
    const int curCh = support ? mVolumCustomSupportChannel : volum::CustomChannelAtStep(amp, mVolumChannelIdx);
    const int snapped = volum::DirectChannelForIr(amp, curCh);
    if (snapped < 0)
      return false; // no raw capture to convolve; leave the current cab in place
    // Already on the DIRECT capture this IR needs (switching between two custom IRs,
    // or an IR picked while the lane is on No Cab): nothing loads, so nothing waits.
    const bool alreadyDirect =
      support ? (mVolumCustomSupportSlot == volum::custom::kDirectSlot && mVolumCustomSupportChannel == snapped)
              : (mVolumCustomMainSlot == volum::custom::kDirectSlot && mVolumCustomMainChannel == snapped);
    if (support)
    {
      mVolumCustomSupportSlot = volum::custom::kDirectSlot;
      mVolumCustomSupportChannel = snapped;
      _VolumActiveScene().supportCustomSlot = volum::custom::kDirectSlot;
      _VolumActiveScene().supportCustomChannel = snapped;
      if (laneFocused)
        _VolumSetCustomChannelStepper(customLane, true, snapped);
      mVolumSupportNeedsLoad.store(true);
    }
    else
    {
      mVolumCustomMainSlot = volum::custom::kDirectSlot;
      mVolumCustomMainChannel = snapped;
      mVolumSpeakerIdx = 0;
      mVolumChannelIdx = volum::custom::ChannelStepIndex(volum::custom::AssignedChannels(amp), snapped);
      _VolumSetCustomChannelStepper(customLane, false, snapped);
      mVolumNeedsLoad.store(true);
    }
    selectRow0();
    return !alreadyDirect;
  }
  if (support)
  {
    if (GetParam(kSupportSpeakerIdx)->Int() != 0)
    {
      GetParam(kSupportSpeakerIdx)->Set(0.0);
      SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
      _VolumRefreshSupportChannels();
      selectRow0();
      mVolumSupportNeedsLoad.store(true);
      return true;
    }
    return false;
  }
  if (mVolumSpeakerIdx != 0)
  {
    mVolumSpeakerIdx = 0;
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = 0;
    _VolumRefreshChannels();
    selectRow0();
    mVolumNeedsLoad.store(true);
    return true;
  }
  return false;
}

void NeuralAmpModeler::_VolumClearIR(bool support, bool deferToCabSwap)
{
  // The audio thread drops the lane's convolver in _ApplyDSPStaging. When a baked
  // cab is taking over, defer that to the block its capture goes live: the .nam
  // loads asynchronously, so dropping the IR now would expose a short burst of raw,
  // cab-less amp. Paths with no replacement coming (the IR menu's "None", which
  // leaves the lane on the DIRECT capture it already has) clear immediately.
  const int lane = support ? 1 : 0;
  // An IR that was itself still waiting for its capture is being retired before it
  // ever convolved; that wait is moot now.
  (support ? mVolumDeferredApplySupportIR : mVolumDeferredApplyIR).store(false);
  if (deferToCabSwap)
  {
    (support ? mVolumDeferredRemoveSupportIrBlocks : mVolumDeferredRemoveIrBlocks).store(0);
    (support ? mVolumDeferredRemoveSupportIR : mVolumDeferredRemoveIR).store(true);
  }
  else
    (support ? mShouldRemoveSupportIR : mShouldRemoveIR) = true;
  if (support)
    _VolumActiveScene().supportActiveIrId.clear();
  else
    _VolumActiveScene().activeIrId.clear();
  // The deferred lane goes on convolving until the swap block, so its trim and cuts
  // have to stay with it; resetting them now would drop a shaped IR to unity
  // mid-note, trading the cab-less gap for a level jump. _VolumFlushDeferredIrShaping
  // pushes the reset once the removal has fired.
  mVolumIrShapingPushPending[lane] = deferToCabSwap;
  if (!deferToCabSwap)
    _VolumPushIrShaping(support); // no active IR -> reset the lane to unity / no cuts
  const int toggle = support ? kSupportIRToggle : kIRToggle;
  GetParam(toggle)->Set(0.0);
  SendParameterValueFromDelegate(toggle, GetParam(toggle)->GetNormalized(), true);
  if (support == _VolumSupportFocused())
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
        spk->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
}

void NeuralAmpModeler::_VolumApplyActiveIr(const std::string& irId, bool support)
{
  const int idx = volum::custom::IRIndexById(irId);
  if (idx < 0)
  {
    // Empty or orphaned id (the IR was deleted / is missing on this machine):
    // drop the convolver so the baked cab takes over. No UI when headless.
    (support ? mShouldRemoveSupportIR : mShouldRemoveIR) = true;
    if (support == _VolumSupportFocused())
      if (auto* pGfx = GetUI())
        if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
          spk->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
    return;
  }
  _VolumSelectIR(idx, support, /*interactive=*/false);
}

// VoLum 1.2.1 per-IR shaping ----------------------------------------------------

// Copy the currently-active IR's library settings (trim + low/high cut) into the
// given lane's audio-thread atomics. With no active IR the lane resets to unity
// gain and no cuts, so the DSP is inert whenever runIR is false. Off-audio-thread.
void NeuralAmpModeler::_VolumPushIrShaping(bool support)
{
  const std::string irId = support ? _VolumActiveScene().supportActiveIrId : _VolumActiveScene().activeIrId;
  const volum::custom::IRShaping s = volum::custom::IRShapingById(irId); // defaults when empty/missing
  const double trimLin = DBToAmp(s.trimDb);
  if (support)
  {
    mSupportIrTrimLin.store(trimLin, std::memory_order_relaxed);
    mSupportIrLowCutHz.store(s.lowCutHz, std::memory_order_relaxed);
    mSupportIrHighCutHz.store(s.highCutHz, std::memory_order_relaxed);
  }
  else
  {
    mIrTrimLin.store(trimLin, std::memory_order_relaxed);
    mIrLowCutHz.store(s.lowCutHz, std::memory_order_relaxed);
    mIrHighCutHz.store(s.highCutHz, std::memory_order_relaxed);
  }
}

// Audio thread: apply trim (in place) then the optional low-cut (high-pass) and
// high-cut (low-pass) one-pole filters to one IR lane. SetParams only assigns
// coefficients (no allocation, no history reset), so per-block reconfig is safe
// and tracks live edits without zipper noise. A cut Hz of 0 bypasses that stage.
iplug::sample** NeuralAmpModeler::_VolumApplyIrShaping(iplug::sample** in, const size_t numChannels, const int nFrames,
                                                       const double sampleRate, const bool support)
{
  const double trim = (support ? mSupportIrTrimLin : mIrTrimLin).load(std::memory_order_relaxed);
  if (trim != 1.0)
    for (size_t c = 0; c < numChannels; ++c)
      for (int i = 0; i < nFrames; ++i)
        in[c][i] = static_cast<iplug::sample>(static_cast<double>(in[c][i]) * trim);

  iplug::sample** p = in;
  const double lowHz = (support ? mSupportIrLowCutHz : mIrLowCutHz).load(std::memory_order_relaxed);
  if (lowHz > 0.0)
  {
    auto& f = support ? mSupportIrLowCut : mIrLowCut;
    f.SetParams(recursive_linear_filter::HighPassParams(sampleRate, lowHz));
    p = f.Process(p, numChannels, nFrames);
  }
  const double highHz = (support ? mSupportIrHighCutHz : mIrHighCutHz).load(std::memory_order_relaxed);
  if (highHz > 0.0)
  {
    auto& f = support ? mSupportIrHighCut : mIrHighCut;
    f.SetParams(recursive_linear_filter::LowPassParams(sampleRate, highHz));
    p = f.Process(p, numChannels, nFrames);
  }
  return p;
}

// One-time migration for IRs imported before 1.2.1 (no stored trim): measure the
// .wav's broadband energy and auto-normalize so they stop landing ~18 dB quieter
// than the baked stock cabs. Persists so it only runs once. Missing/unreadable
// files are marked calibrated at unity so we do not retry them every launch.
void NeuralAmpModeler::_VolumMigrateIrTrims()
{
  auto& irs = volum::content::GlobalContentStore().reg().irs;
  bool changed = false;
  for (auto& ir : irs)
  {
    if (ir.trimCalibrated)
      continue;
    double trimDb = 0.0;
    const auto abs = volum::content::GlobalContentStore().ResolveStored(ir.file);
    const std::string absUtf8 = volum::content::PathToUtf8(abs);
    std::vector<float> audio;
    double fileSr = 0.0;
    if (!abs.empty() && dsp::wav::Load(absUtf8.c_str(), audio, fileSr) == dsp::wav::LoadReturnCode::SUCCESS
        && !audio.empty())
    {
      double sumSq = 0.0;
      for (float v : audio)
        sumSq += static_cast<double>(v) * static_cast<double>(v);
      trimDb = volum::content::AutoNormalizeIrTrimDb(std::sqrt(sumSq));
    }
    ir.trimDb = trimDb;
    ir.trimCalibrated = true;
    changed = true;
    VOLUM_LOG("migrate", "IR '" + ir.name + "' auto-normalized to " + std::to_string(trimDb) + " dB");
  }
  if (!changed)
    return;
  // This rewrites the user's library in place and cannot be undone by going back
  // to 1.2.0 (a v2 build that re-saves drops trimDb/lowCutHz/highCutHz), so keep
  // a one-time pre-migration copy alongside it.
  const bool backedUp = volum::content::GlobalContentStore().BackupBeforeMigration("1.2.1");
  volum::content::GlobalContentStore().Save();
  VOLUM_LOG(
    "migrate",
    std::string("IR trim migration saved; pre-migration backup ")
      + (backedUp ? "written to "
                      + volum::content::PathToUtf8(volum::content::GlobalContentStore().MigrationBackupPath("1.2.1"))
                  : "unavailable"));
}

void NeuralAmpModeler::_VolumReconcileActiveIr()
{
  auto& scene = _VolumActiveScene();
  // SUPPORT lane: an orphaned id drops the support convolver. The IR had forced
  // the lane onto its DIRECT (cab-less) capture, so recover to a real cab too -
  // mirroring the MAIN _VolumFallbackToAvailableCab - instead of leaving the
  // support lane on the bare raw amp.
  if (!scene.supportActiveIrId.empty() && volum::custom::IRIndexById(scene.supportActiveIrId) < 0)
  {
    _VolumClearIR(true);
    if (mVolumCustomSupportIdx >= 0)
    {
      const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
      const auto slots = volum::custom::AmpSlots(amp);
      int chosen = volum::custom::kDirectSlot;
      for (int s : slots) // prefer a real cab over DIRECT
        if (s != volum::custom::kDirectSlot)
        {
          chosen = s;
          break;
        }
      if (chosen == volum::custom::kDirectSlot && !slots.empty())
        chosen = slots.front();
      const auto chs = volum::custom::AmpSlotChannels(amp, chosen);
      mVolumCustomSupportSlot = chosen;
      mVolumCustomSupportChannel = chs.empty() ? 1 : volum::custom::SnapChannel(chs, mVolumCustomSupportChannel);
      scene.supportCustomSlot = mVolumCustomSupportSlot;
      scene.supportCustomChannel = mVolumCustomSupportChannel;
      mVolumSupportNeedsLoad.store(true);
      if (_VolumSupportFocused())
        _VolumApplyFocusedLaneCabs();
    }
    else if (GetParam(kSupportAmpIdx)->Int() >= 0)
    {
      GetParam(kSupportSpeakerIdx)->Set(1.0); // first baked cab
      SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
      _VolumRefreshSupportChannels();
      mVolumSupportNeedsLoad.store(true);
    }
  }

  const std::string id = scene.activeIrId;
  // Only trust the visible IR chip as a signal for the MAIN lane when MAIN is
  // the focused lane (the shared row otherwise shows the support lane).
  bool uiActive = false;
  if (!_VolumSupportFocused())
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
        uiActive = spk->As<VoLumSpeakerRowControl>()->IsIrCabActive();
  // Nothing was using a custom IR.
  if (id.empty() && !uiActive)
    return;
  // The active IR still resolves: leave the live state untouched.
  if (!id.empty() && volum::custom::IRIndexById(id) >= 0)
    return;
  // Orphaned (deleted on this machine): recover to a real cab.
  _VolumFallbackToAvailableCab();
}

void NeuralAmpModeler::_VolumFallbackToAvailableCab()
{
  mShouldRemoveIR = true; // audio thread drops mIR in _ApplyDSPStaging
  _VolumActiveScene().activeIrId.clear();
  GetParam(kIRToggle)->Set(0.0);
  SendParameterValueFromDelegate(kIRToggle, GetParam(kIRToggle)->GetNormalized(), true);
  auto* pGfx = GetUI();
  auto* row = pGfx ? pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow) : nullptr;
  if (mVolumCustomMainIdx >= 0)
  {
    const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
    const auto slots = volum::custom::AmpSlots(amp);
    int chosenSlot = volum::custom::kDirectSlot, sel = 0;
    for (int s : slots) // prefer a real cab over DIRECT
      if (s != volum::custom::kDirectSlot)
      {
        chosenSlot = s;
        sel = s + 1;
        break;
      }
    if (sel == 0 && !slots.empty())
    {
      chosenSlot = slots.front();
      sel = (chosenSlot == volum::custom::kDirectSlot) ? 0 : chosenSlot + 1;
    }
    const auto chs = volum::custom::AmpSlotChannels(amp, chosenSlot);
    const int ch = chs.empty() ? 1 : chs.front();
    mVolumCustomMainSlot = chosenSlot;
    mVolumCustomMainChannel = ch;
    mVolumSpeakerIdx = sel;
    mVolumChannelIdx = volum::custom::ChannelStepIndex(volum::custom::AssignedChannels(amp), ch);
    _VolumSetCustomChannelStepper(mVolumCustomMainIdx, false, ch);
    if (row)
    {
      row->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
      row->As<VoLumSpeakerRowControl>()->SetSelected(sel);
    }
    mVolumNeedsLoad.store(true);
  }
  else
  {
    // Factory amps always ship No Cab + 3 baked cabs; prefer the first real cab.
    const int sel = 1;
    mVolumSpeakerIdx = sel;
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = sel;
    _VolumRefreshChannels();
    if (row)
    {
      row->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
      row->As<VoLumSpeakerRowControl>()->SetSelected(sel);
    }
    mVolumNeedsLoad.store(true);
  }
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
}
