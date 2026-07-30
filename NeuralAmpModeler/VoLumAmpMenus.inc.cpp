// VoLumAmpMenus.inc.cpp: factory reset + preset/support-amp menu + dual-amp focus member functions
// Extracted from NeuralAmpModeler.cpp for file-size hygiene. Tail-#included
// into the NeuralAmpModeler translation unit; not a separate build target.

void NeuralAmpModeler::_VolumResetAmpToFactory()
{
  // Factory baseline == a default-constructed settings scene. Reset the FOCUSED
  // amp's scene (a custom amp keeps its own scene in the content library, so
  // resetting the factory slot would target the wrong owner), apply it to the
  // live params, rediscover channels, and clear any custom IR / recalled preset.
  auto& scene = _VolumActiveScene();
  scene = volum::VoLumAmpSettings{};
  _VolumApplyAmpSettings(scene);
  mVolumNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  // Drop any recalled preset: the bar reads "No Preset" and edits no longer diff.
  _VolumForgetActivePreset();
  // One applier for the whole row - names, enables, IR chip, selection and the
  // channel stepper - instead of re-showing the names and hand-clearing the chip. The
  // hand-written version left No Cab and Custom IR in whatever enable state the
  // pre-reset channel had produced.
  _VolumApplyFocusedLaneCabs();
  if (auto* pGfx = GetUI())
    if (auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
      pb->As<VoLumPresetBarControl>()->SelectName(""); // -> "No Preset", clean
}

void NeuralAmpModeler::_VolumShowPresetMenu()
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* raw = pGfx->GetControlWithTag(kCtrlTagVoLumPresetMenu);
  auto* bar = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar);
  if (!raw || !bar)
    return;
  if (!raw->IsHidden())
  {
    raw->Hide(true);
    return;
  }

  const auto presets = volum::custom::MockPresetsForAmp(mVolumAmpIdx);
  auto* presetBar = bar->As<VoLumPresetBarControl>();
  const bool dirty = presetBar->IsEditDirty();
  const int activePresetIdx = presetBar->ActiveIndex();
  std::vector<VoLumListMenuControl::Row> rows;
  if (presets.empty())
  {
    // No presets to come back from, so the reset-to-factory row is pointless.
    rows.push_back({"No presets yet", -99, false, true}); // dim hint
  }
  else
  {
    // Pinned reset-to-factory row at the top, separated by a divider.
    rows.push_back({"Default (factory settings)", VoLumListMenuControl::kDefault, true, false, true});
    for (int i = 0; i < (int)presets.size(); i++)
      rows.push_back({presets[(size_t)i], i, false, false});
  }
  // When the rig is dirty, offer a one-click save path right in the dropdown:
  // overwrite the active named preset, or (no named preset / on Default) save a
  // new one. Saves opening the Manage panel just to commit a tweak.
  if (dirty)
  {
    if (activePresetIdx >= 0 && activePresetIdx < (int)presets.size())
      rows.push_back({"Overwrite \"" + presets[(size_t)activePresetIdx] + "\"", VoLumListMenuControl::kOverwrite, true,
                      false, true});
    else
      rows.push_back({"Save current as new...", VoLumListMenuControl::kSaveAsNew, true, false, true});
  }
  rows.push_back({"Manage presets...", VoLumListMenuControl::kManage, true, false});

  auto* menu = raw->As<VoLumListMenuControl>();
  const IRECT anchor = bar->GetRECT();
  const float w = std::max(anchor.W(), 220.f);
  const auto bounds = pGfx->GetBounds();
  const float top = anchor.B + 4.f;
  const float h = std::min(VoLumListMenuControl::MenuHeight(rows.size()), std::max(110.f, bounds.B - top - 8.f));
  float l = anchor.L;
  if (l + w > bounds.R - 4.f)
    l = bounds.R - 4.f - w;
  menu->SetMenuRect(IRECT(l, top, l + w, top + h));
  const int selected = bar->As<VoLumPresetBarControl>()->ActiveIndex();
  menu->SetRows(rows, selected);
  menu->Hide(false);
}

void NeuralAmpModeler::_VolumHidePreCaptureMenu()
{
  if (auto* pGfx = GetUI())
  {
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumPreCaptureMenu))
      menu->Hide(true);
  }
}

void NeuralAmpModeler::_VolumShowSupportAmpMenu(const IRECT& anchorRect)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  auto* rawCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSupportAmpMenu);
  if (!rawCtrl)
    return;

  auto* menu = rawCtrl->As<VoLumListMenuControl>();
  if (!menu)
    return;

  if (!rawCtrl->IsHidden())
  {
    _VolumHideSupportAmpMenu();
    return;
  }

  // Row 0 = "(none)" so the user can clear the support amp without disabling Dual Amp mode.
  // Then the factory amps, then a "CUSTOM" group (header + custom amps) when any exist.
  std::vector<VoLumListMenuControl::Row> rows;
  rows.push_back({"(none)", VoLumListMenuControl::kNone, false, false});
  for (int i = 0; i < volum::kAmpCount; ++i)
    rows.push_back({volum::kAmps[i].displayName, i, false, false});
  const auto& customAmps = volum::custom::MockCustomAmps();
  if (!customAmps.empty())
  {
    rows.push_back({"CUSTOM", 0, false, false, false, false, /*header=*/true});
    for (int c = 0; c < static_cast<int>(customAmps.size()); ++c)
      rows.push_back({customAmps[(size_t)c], kVolumCustomSupportBase + c, false, false, false, /*group=*/true});
  }

  int selected = VoLumListMenuControl::kNone;
  if (mVolumCustomSupportIdx >= 0 && mVolumCustomSupportIdx < static_cast<int>(customAmps.size()))
    selected = kVolumCustomSupportBase + mVolumCustomSupportIdx;
  else
  {
    const int currentSupportIdx = GetParam(kSupportAmpIdx)->Int();
    if (currentSupportIdx >= 0 && currentSupportIdx < volum::kAmpCount)
      selected = currentSupportIdx;
  }

  const float menuW = std::max(anchorRect.W() * 0.96f, 200.f);
  const float menuH = VoLumListMenuControl::MenuHeight(rows.size());
  const float panelW = static_cast<float>(pGfx->Width());
  const float panelH = static_cast<float>(pGfx->Height());
  const float anchorL = std::min(anchorRect.L, panelW - menuW - 8.f);

  // Prefer to drop the menu BELOW the support hero so the support panel stays clickable to
  // dismiss the dropdown. The list scrolls internally, so cap the visible height to whichever
  // gap (below / above) is larger and let the user scroll the rest.
  const float spaceBelow = panelH - (anchorRect.B + 6.f) - 8.f;
  const float spaceAbove = anchorRect.T - 6.f - 8.f;
  float menuT;
  float visH;
  if (menuH <= spaceBelow)
  {
    menuT = anchorRect.B + 6.f;
    visH = menuH;
  }
  else if (menuH <= spaceAbove)
  {
    menuT = anchorRect.T - 6.f - menuH;
    visH = menuH;
  }
  else if (spaceBelow >= spaceAbove)
  {
    menuT = anchorRect.B + 6.f;
    visH = spaceBelow;
  }
  else
  {
    visH = spaceAbove;
    menuT = anchorRect.T - 6.f - visH;
  }

  const IRECT menuRect(anchorL, menuT, anchorL + menuW, menuT + visH);

  menu->SetMenuRect(menuRect);
  menu->SetRows(rows, selected);
  menu->Hide(false);
}

void NeuralAmpModeler::_VolumHideSupportAmpMenu()
{
  if (auto* pGfx = GetUI())
  {
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumSupportAmpMenu))
      menu->Hide(true);
  }
}

void NeuralAmpModeler::_VolumSetSupportAmp(int ampIdx)
{
  const int clamped = std::clamp(ampIdx, -1, volum::kAmpCount - 1);
  // Picking a factory support amp (or "(none)") always clears a custom support partner.
  const bool hadCustom = mVolumCustomSupportIdx >= 0;
  mVolumCustomSupportIdx = -1;
  if (!hadCustom && GetParam(kSupportAmpIdx)->Int() == clamped)
    return;

  GetParam(kSupportAmpIdx)->Set(clamped);
  SendParameterValueFromDelegate(kSupportAmpIdx, GetParam(kSupportAmpIdx)->GetNormalized(), true);
  _VolumActiveScene().supportCustomId.clear(); // factory partner: drop any custom ref
  mVolumSupportNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
  _VolumRefreshSupportChannels();
  _VolumApplyFocusedLaneCabs();
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumSetSupportCustom(int customIdx)
{
  const auto& names = volum::custom::MockCustomAmps();
  if (customIdx < 0 || customIdx >= static_cast<int>(names.size()))
    return;

  mVolumCustomSupportIdx = customIdx;
  // Clear the factory reference so the SUPPORT lane renders the custom amp's
  // art/cabs (not a factory amp).
  if (GetParam(kSupportAmpIdx)->Int() != -1)
  {
    GetParam(kSupportAmpIdx)->Set(-1.0);
    SendParameterValueFromDelegate(kSupportAmpIdx, GetParam(kSupportAmpIdx)->GetNormalized(), true);
  }
  // Persist the custom partner by stable id and resolve a default (slot, channel)
  // so the SUPPORT lane produces sound immediately (F6 dual amp DSP).
  _VolumActiveScene().supportCustomId = volum::custom::CustomAmpIdAt(customIdx);
  {
    const auto amp = volum::custom::CustomAmpAt(customIdx);
    int s = volum::custom::kDirectSlot, c = 1;
    if (volum::content::DefaultCaptureSelection(amp, s, c))
    {
      mVolumCustomSupportSlot = s;
      mVolumCustomSupportChannel = c;
    }
    // Persist the freshly resolved cab/channel so it round-trips like MAIN.
    _VolumActiveScene().supportCustomSlot = mVolumCustomSupportSlot;
    _VolumActiveScene().supportCustomChannel = mVolumCustomSupportChannel;
  }
  mVolumSupportNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
  _VolumApplyFocusedLaneCabs();
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumApplyFocusedLaneCabs()
{
  // Reconcile the shared cabinet row + channel stepper with the focused lane.
  // Custom lanes (MAIN or the custom SUPPORT partner) show their own named cabs;
  // factory lanes restore the stock G12/G65/V30 labels and that lane's channels.
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl)
    return;

  const bool supportFocus = GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport;
  const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane >= 0)
  {
    _VolumApplyCustomMainCabs(customLane, supportFocus);
    return;
  }

  // Factory lane: discover this lane's channels first (that also clamps a stale
  // channel index), then let the shared planner decide what the row shows. Going
  // through the same planner as custom lanes is what stopped a factory amp with
  // an active custom IR from coming back as "No Cab" on editor reopen.
  if (supportFocus)
    _VolumRefreshSupportChannels();
  else
    _VolumRefreshChannels();

  const volum::custom::CustomAmp unusedAmp; // factory lane: planner ignores it
  _VolumApplyUiSyncPlan(volum::MakeUiSyncPlan(_VolumMakeUiSyncInput(supportFocus, unusedAmp)), supportFocus);
}

// The one place restored backend state becomes visible control state. Called
// after every restore path (editor open, DAW chunk load, session re-focus) so a
// rebuilt editor never shows a constructor default in place of real state.
void NeuralAmpModeler::_VolumSyncUiFromState()
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  // Sidebar + hero follow the MAIN lane's amp identity regardless of which lane
  // the cab row is showing.
  if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
  {
    auto* list = al->As<VoLumAmpListControl>();
    if (mVolumCustomMainIdx >= 0)
      list->SetCustomSelected(mVolumCustomMainIdx);
    else
      list->SetSelected(mVolumAmpIdx);
  }

  const char* ampName = _VolumMainAmpDisplayName();
  if (auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
  {
    auto* hero = heroCtrl->As<VoLumHeroImageControl>();
    if (mVolumCustomMainIdx >= 0)
      hero->SetCustomArt(true, volum::custom::CustomAmpArt(mVolumCustomMainIdx));
    else
    {
      char ph[4] = {volum::kAmps[mVolumAmpIdx].displayName[0], (char)('0' + (mVolumAmpIdx % 10)), 0, 0};
      hero->SetCustomArt(false, 0);
      hero->SetPlaceholder(ph, mVolumAmpIdx);
    }
    hero->SetName(ampName);
  }
  if (auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
    if (mVolumExpandedSection == EVoLumSection::AMP)
      nameCtrl->As<VoLumSubRowTextControl>()->SetName(ampName, mVolumCustomMainIdx >= 0);

  // Cab row + channel stepper for the focused lane.
  _VolumApplyFocusedLaneCabs();
}

void NeuralAmpModeler::_VolumReflectLaneIrChip(bool support)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl)
    return;
  auto* row = spkCtrl->As<VoLumSpeakerRowControl>();
  const std::string irId = support ? _VolumActiveScene().supportActiveIrId : _VolumActiveScene().activeIrId;
  const int irIdx = irId.empty() ? -1 : volum::custom::IRIndexById(irId);
  const auto& irNames = volum::custom::MockIRLibrary();
  if (irIdx >= 0 && irIdx < static_cast<int>(irNames.size()))
    row->SetIrCab(true, irNames[static_cast<size_t>(irIdx)].c_str());
  else
    row->SetIrCab(false, "");
}

void NeuralAmpModeler::_VolumRefreshSupportChannels()
{
  mVolumSupportChannelFiles.clear();
  mVolumSupportChannelLabels.clear();

  // Custom support amp: channels come from the custom amp's manifest, not the
  // factory rig folders. Channel-first: the stepper lists the amp-WIDE gain
  // stages; the resolver keeps the lane's saved (slot, channel) and snaps the slot
  // to one that carries the channel (item: custom support amp forgets its cab /
  // channel on recall).
  if (mVolumCustomSupportIdx >= 0)
  {
    const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
    const volum::custom::LaneCabView v =
      volum::custom::ResolveLaneCabs(amp, mVolumCustomSupportSlot, mVolumCustomSupportChannel);
    mVolumCustomSupportSlot = v.slot;
    mVolumCustomSupportChannel = v.channel;
    _VolumSetCustomChannelStepper(mVolumCustomSupportIdx, /*supportLane=*/true, v.channel);
    return;
  }

  const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
  if (supportAmpIdx >= 0 && supportAmpIdx < volum::kAmpCount && !mVolumRigsRoot.empty())
  {
    const int speakerIdx = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3);
    auto channels =
      volum::DiscoverChannels(volum::content::PathFromUtf8(mVolumRigsRoot), volum::kAmps[supportAmpIdx].folderName,
                              volum::kSpeakerPrefixes[speakerIdx]);
    for (auto& ch : channels)
    {
      mVolumSupportChannelFiles.push_back(std::move(ch.filename));
      mVolumSupportChannelLabels.push_back(std::move(ch.label));
    }

    int channelIdx = std::clamp(
      GetParam(kSupportChannelIdx)->Int(), 0, std::max(0, static_cast<int>(mVolumSupportChannelFiles.size()) - 1));
    if (channelIdx != GetParam(kSupportChannelIdx)->Int())
    {
      GetParam(kSupportChannelIdx)->Set(channelIdx);
      SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
    }
  }

  if (auto* pGfx = GetUI())
  {
    if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumSupportChannelStep))
      stepper->As<VoLumChannelStepControl>()->SetChannels(
        mVolumSupportChannelLabels, GetParam(kSupportChannelIdx)->Int());
  }
}

bool NeuralAmpModeler::_VolumHasSupportAmp()
{
  const int factory = GetParam(kSupportAmpIdx)->Int();
  if (factory >= 0 && factory < volum::kAmpCount)
    return true;
  // An index the library no longer contains is not a lane: a custom support amp
  // deleted from another instance leaves this one holding a stale index, and
  // treating it as present focuses a lane with no amp behind it.
  return mVolumCustomSupportIdx >= 0
         && mVolumCustomSupportIdx < static_cast<int>(volum::custom::MockCustomAmps().size());
}

void NeuralAmpModeler::_VolumClampSupportFocus()
{
  if (!mVolumDualAmpFocusedSupport || _VolumHasSupportAmp())
    return;

  mVolumDualAmpFocusedSupport = false;

  // Moving focus is only half the job. The cab row is shared by both lanes and every
  // write to it is now conditioned on which lane is focused, so a clamp that only
  // flipped the flag left the row still describing SUPPORT while MAIN was focused -
  // the exact state that guard exists to prevent, and a click on a cab then edited
  // MAIN with an index from the support amp's layout. Re-derive here so no caller
  // has to remember: _VolumApplyFocusedLaneCabs does not call _UpdateVoLumLayout,
  // so there is no re-entrancy back into this.
  _VolumApplyFocusedLaneCabs();
}

void NeuralAmpModeler::_VolumApplyDualAmpFocus()
{
  // Focus can go stale rather than only being set wrongly: switching MAIN to an amp
  // whose scene carries no support partner drops the partner while SUPPORT is still
  // focused. Repaired here because this runs from _UpdateVoLumLayout, i.e. after
  // every interaction, and before the early-out below so it also holds headless.
  _VolumClampSupportFocus();

  // Sync the speaker row to the focused lane (cab selection is per-amp). Lane belonging on the
  // SUPPORT amp-row knobs is conveyed by their teal pointer dot + teal value text — set once at
  // attach time, no per-frame retoggling needed here.
  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  const bool dualActive = GetParam(kDualAmpActive)->Bool();
  const bool supportFocus = dualActive && mVolumDualAmpFocusedSupport;
  const bool showPanKnobs = dualActive && mVolumExpandedSection == EVoLumSection::AMP;
  // Polarity belongs to the SUPPORT lane whenever it has an amp - a factory amp
  // or a custom support partner.
  const bool showSupportPolarity = showPanKnobs && _VolumHasSupportAmp();

  if (auto* spkRow = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
  {
    // A focused custom lane (MAIN or custom SUPPORT) manages its own cab
    // selection in _VolumApplyFocusedLaneCabs; don't fight it here. Factory
    // lanes track their per-amp speaker index.
    const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
    if (customLane < 0)
    {
      auto* row = spkRow->As<VoLumSpeakerRowControl>();
      const int focusedSpeakerIdx =
        supportFocus ? std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3) : mVolumSpeakerIdx;
      row->SetSelected(focusedSpeakerIdx);
    }
  }

  // PAN knobs follow Dual Amp: shown only in dual mode, and their slot tracks the hero geometry.
  if (auto* hero = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
  {
    auto* heroCtrl = hero->As<VoLumHeroImageControl>();
    if (auto* mainPanGrp = pGfx)
    {
      mainPanGrp->ForControlInGroup("MAIN_PAN_KNOB", [&](IControl* c) {
        c->SetTargetAndDrawRECTs(heroCtrl->GetMainPanKnobSlot());
        c->Hide(!showPanKnobs);
      });
      mainPanGrp->ForControlInGroup("SUPPORT_PAN_KNOB", [&](IControl* c) {
        c->SetTargetAndDrawRECTs(heroCtrl->GetSupportPanKnobSlot());
        c->Hide(!showPanKnobs);
      });
      mainPanGrp->ForControlInGroup("SUPPORT_POLARITY_TOGGLE", [&](IControl* c) {
        c->SetTargetAndDrawRECTs(heroCtrl->GetSupportPolarityToggleSlot());
        c->Hide(!showSupportPolarity);
      });
    }
  }
}
