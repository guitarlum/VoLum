// PLAY state/application glue. Tail-included into NeuralAmpModeler.cpp.

void NeuralAmpModeler::_VolumSetUiMode(volum::UiMode mode)
{
  const auto transition = volum::ActionForUiModeTransition(mVolumUiMode, mode);
  if (volum::EnteringPlayDropsLocks(mVolumUiMode, mode) && (mVolumPreLocked || mVolumPostLocked))
  {
    // Keep what is sounding: commit the locked live blocks into the scene, then
    // drop the lock so PLAY recalls cannot desync PRE/POST.
    if (mVolumPreLocked)
      _VolumStorePreToCurrentAmp();
    if (mVolumPostLocked)
      _VolumStorePostToCurrentAmp();
    mVolumPreLocked = false;
    mVolumPostLocked = false;
    mVolumPreLockUiDirty = false;
    mVolumPostLockUiDirty = false;
  }
  _VolumClampSupportFocus();
  mVolumUiMode = mode;
  if (auto* pGfx = GetUI())
  {
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumPresetMenu))
      menu->Hide(true);
    if (auto* overlay = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
      overlay->Hide(true);
    if (mode == volum::UiMode::Play)
    {
      _ClearVoLumKnobSelection();
      if (auto* surface = pGfx->GetControlWithTag(kCtrlTagVoLumPlaySurface))
        surface->As<VoLumPlaySurfaceControl>()->ClosePicker();
    }
    pGfx->SetAllControlsDirty();
  }
#ifdef APP_API
  mVolumSettingsDirty = true;
#endif
  DirtyParametersFromUI();
  // Hide/show lives in _VolumRefreshPlaySurface so a restore that only syncs
  // cannot leave PLAY intercepting BUILD clicks. BUILD still needs the layout
  // pass that uncovers the knobs under the overlay.
  _VolumRefreshPlaySurface();
  if (!(mode == volum::UiMode::Play && transition == volum::UiModeTransitionAction::RefreshOnly))
    _UpdateVoLumLayout();
}

bool NeuralAmpModeler::_VolumStepPlaySlot(int dir)
{
  const auto& registry = volum::content::GlobalContentStore().reg();
  const auto slots = volum::BuildPlaySlots(mVolumFactoryPresets, registry);
  const int next = volum::StepAssignedSlot(slots, mVolumLastRecalledPlaySlot, dir);
  if (next < 0)
    return false;
  for (const auto& slot : slots)
  {
    if (slot.slot != next || !slot.valid)
      continue;
    // Same path a rail click takes, so a keyboard step and a click cannot drift.
    if (!VolumRecallSound(slot.sound.ampId, slot.sound.presetId))
      return false;
    mVolumLastRecalledPlaySlot = next;
    _VolumRefreshPlaySurface();
    return true;
  }
  return false;
}

bool NeuralAmpModeler::_VolumTogglePlayBypass(const char* paramName)
{
  if (!paramName)
    return false;
  for (int i = 0; i < kNumParams; ++i)
  {
    if (std::strcmp(GetParam(i)->GetName(), paramName) != 0)
      continue;
    _VolumUserToggleParam(i);
    _VolumRefreshPlaySurface();
    return true;
  }
  return false; // e.g. Chorus before the post-chorus branch is merged
}

void NeuralAmpModeler::_VolumAssignPlaySound(int slot, const volum::SoundChoice& sound)
{
  auto& store = volum::content::GlobalContentStore();
  const auto before = store.reg().midiSoundMap;
  if (!volum::content::AssignMidiSound(store.reg(), slot, sound.ampId, sound.presetId))
    return;
  if (!store.Save())
    store.reg().midiSoundMap = before;
  // Both surfaces, always: the caller can be the PLAY rail or the Settings MIDI
  // tab, and the one that is not on screen must not keep a stale list.
  _VolumRefreshPlaySurface();
  _VolumRefreshMidiSettingsChrome();
}

void NeuralAmpModeler::_VolumClearPlaySound(int slot)
{
  auto& store = volum::content::GlobalContentStore();
  const auto before = store.reg().midiSoundMap;
  if (!volum::content::ClearMidiSound(store.reg(), slot))
    return;
  if (!store.Save())
    store.reg().midiSoundMap = before;
  if (mVolumLastRecalledPlaySlot == slot)
    mVolumLastRecalledPlaySlot = -1;
  _VolumRefreshPlaySurface();
  _VolumRefreshMidiSettingsChrome();
}

void NeuralAmpModeler::_VolumSyncLivePlaySlotFromActivePair()
{
  const int found =
    volum::FindAssignedSlot(volum::BuildPlaySlots(mVolumFactoryPresets, volum::content::GlobalContentStore().reg()),
                            _VolumActiveOwnerKey(), mVolumActivePresetId);
  if (found >= 0)
    mVolumLastRecalledPlaySlot = found;
}

void NeuralAmpModeler::_VolumReassignLivePlaySlotAfterSave()
{
  if (mVolumLastRecalledPlaySlot < 0 || mVolumActivePresetId.empty())
    return;
  if (!volum::content::MidiSoundAtSlot(volum::content::GlobalContentStore().reg(), mVolumLastRecalledPlaySlot))
    return;
  _VolumAssignPlaySound(
    mVolumLastRecalledPlaySlot, {_VolumActiveOwnerKey(), mVolumActivePresetId, {}, {}, false, 0, false});
}

void NeuralAmpModeler::_VolumInsertPlaySound(int fromSlot, int beforeSlot)
{
  auto& store = volum::content::GlobalContentStore();
  const auto before = store.reg().midiSoundMap;
  if (!volum::content::InsertMidiSoundAmongAssigned(store.reg(), fromSlot, beforeSlot))
    return;
  if (!store.Save())
  {
    store.reg().midiSoundMap = before;
    return;
  }
  mVolumLastRecalledPlaySlot =
    volum::content::FollowLiveSlotAfterReorder(before, store.reg().midiSoundMap, mVolumLastRecalledPlaySlot);
  _VolumRefreshPlaySurface();
  _VolumRefreshMidiSettingsChrome();
}

void NeuralAmpModeler::_VolumSwapPlaySounds(int slotA, int slotB)
{
  auto& store = volum::content::GlobalContentStore();
  const auto before = store.reg().midiSoundMap;
  if (!volum::content::SwapMidiSoundSlots(store.reg(), slotA, slotB))
    return;
  if (!store.Save())
  {
    store.reg().midiSoundMap = before;
    return;
  }
  mVolumLastRecalledPlaySlot =
    volum::content::FollowLiveSlotAfterReorder(before, store.reg().midiSoundMap, mVolumLastRecalledPlaySlot);
  _VolumRefreshPlaySurface();
  _VolumRefreshMidiSettingsChrome();
}

void NeuralAmpModeler::_VolumRefreshPlaySurface()
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  // Visibility is derived from mVolumUiMode on every call, including BUILD: the
  // early return used to skip Hide, so a host restore into BUILD left PLAY shown
  // at full-window bounds and it swallowed every click. Skip Hide when already
  // matching - this function also runs every idle tick while PLAY is up.
  const auto mode = mVolumUiMode;
  const auto chrome = volum::PlayChromeForUiMode(mode);
  if (auto* surface = pGfx->GetControlWithTag(kCtrlTagVoLumPlaySurface))
  {
    if (surface->IsHidden() != chrome.hidePlaySurface)
      surface->Hide(chrome.hidePlaySurface);
  }
  if (auto* plate = pGfx->GetControlWithTag(kCtrlTagVoLumHeaderPlate))
  {
    if (plate->IsHidden() != chrome.hideHeaderPlate)
      plate->Hide(mode == volum::UiMode::Play);
  }
  if (auto* toggle = pGfx->GetControlWithTag(kCtrlTagVoLumModeToggle))
    toggle->As<VoLumModeToggleControl>()->SetMode(mode);
  if (auto* preset = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
  {
    if (preset->IsHidden() != chrome.hidePresetBar)
      preset->Hide(chrome.hidePresetBar);
  }

  if (mVolumUiMode != volum::UiMode::Play)
    return;
  auto* raw = pGfx->GetControlWithTag(kCtrlTagVoLumPlaySurface);
  if (!raw)
    return;

  auto paramBool = [this](const char* name) {
    for (int i = 0; i < kNumParams; ++i)
      if (std::strcmp(GetParam(i)->GetName(), name) == 0)
        return GetParam(i)->Bool();
    return false;
  };
  std::array<bool, VoLumPlaySurfaceControl::FxCount> fx{};
  for (size_t i = 0; i < fx.size(); ++i)
    fx[i] = paramBool(volum::kPlayBypassParamNames[i]);

  // NAM wells are available when a capture is assigned, even if bypass has
  // unloaded the model. Empty slots stay veiled and ignore click / 3 / 4.
  std::array<bool, VoLumPlaySurfaceControl::FxCount> fxAvailable{};
  fxAvailable.fill(true);
  fxAvailable[VoLumPlaySurfaceControl::Nam1] = volum::PlayNamCaptureAssigned(GetParam(kPreNam1Capture)->Int());
  fxAvailable[VoLumPlaySurfaceControl::Nam2] = volum::PlayNamCaptureAssigned(GetParam(kPreNam2Capture)->Int());

  const bool dual = GetParam(kDualAmpActive)->Bool() && _VolumHasSupportAmp();
  std::string supportName;
  int supportArt = 0;
  bool supportCustom = false;
  if (mVolumCustomSupportIdx >= 0)
  {
    const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
    supportName = amp.name;
    supportArt = amp.art;
    supportCustom = true;
  }
  else
  {
    const int idx = GetParam(kSupportAmpIdx)->Int();
    if (idx >= 0 && idx < volum::kAmpCount)
    {
      supportName = volum::kAmps[idx].displayName;
      supportArt = idx;
    }
  }

  const bool dirty = _VolumLivePresetDirty();
  raw->As<VoLumPlaySurfaceControl>()->SetData(
    mVolumFactoryPresets, volum::content::GlobalContentStore().reg(), _VolumActiveOwnerKey(), mVolumActivePresetId,
    mVolumLastRecalledPlaySlot, _VolumMainAmpDisplayName(),
    mVolumCustomMainIdx >= 0 ? volum::custom::CustomAmpArt(mVolumCustomMainIdx) : mVolumAmpIdx,
    mVolumCustomMainIdx >= 0, dual, supportName, supportArt, supportCustom, fx, fxAvailable,
    mVolumMidiChannel.load(std::memory_order_relaxed), dirty,
    _VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), "NAM 1"),
    _VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), "NAM 2"));
  raw->As<VoLumPlaySurfaceControl>()->SetPlusAddsHeard(volum::PlayPlusAddsHeard(
    dirty, volum::SaveActionForActivePreset(mVolumActivePresetId) == volum::PresetSaveAction::SaveUserCopy,
    volum::SoundIsAssigned(volum::BuildPlaySlots(mVolumFactoryPresets, volum::content::GlobalContentStore().reg()),
                           _VolumActiveOwnerKey(), mVolumActivePresetId)));
  raw->As<VoLumPlaySurfaceControl>()->SetAnimateArt(mVolumAnimatePlayArt.load());
  raw->As<VoLumPlaySurfaceControl>()->SetInPeak(mVolumPlayInPeak.load(std::memory_order_relaxed));
  raw->As<VoLumPlaySurfaceControl>()->SetOutPeak(mVolumPlayOutPeak.load(std::memory_order_relaxed));
  raw->As<VoLumPlaySurfaceControl>()->SetPickerGroups(&mVolumPlayPickerGroups);
}

void NeuralAmpModeler::_VolumAddHeardPlaySound()
{
  auto finish = [this]() {
    if (volum::SoundIsAssigned(volum::BuildPlaySlots(mVolumFactoryPresets, volum::content::GlobalContentStore().reg()),
                               _VolumActiveOwnerKey(), mVolumActivePresetId))
    {
      _VolumRefreshPlaySurface();
      return;
    }
    auto& store = volum::content::GlobalContentStore();
    const int slot = volum::content::FirstFreeMidiSoundSlot(store.reg());
    if (!volum::AddHeardMarksLive(slot, mVolumActivePresetId.empty()))
    {
      if (auto* gfx = GetUI())
        if (auto* surface = gfx->GetControlWithTag(kCtrlTagVoLumPlaySurface))
          surface->As<VoLumPlaySurfaceControl>()->OpenReplacePicker();
      return;
    }
    _VolumAssignPlaySound(slot, {_VolumActiveOwnerKey(), mVolumActivePresetId, {}, {}, false, 0, false});
    mVolumLastRecalledPlaySlot = slot;
    _VolumRefreshPlaySurface();
  };
  if (volum::AddHeardNeedsSaveAs(
        volum::SaveActionForActivePreset(mVolumActivePresetId), _VolumLivePresetDirty(), mVolumActivePresetId.empty()))
  {
    _VolumPromptSaveAs(finish, volum::SaveOrigin::AddSound);
    return;
  }
  finish();
}

void NeuralAmpModeler::_VolumFocusBuildEffect(int focus)
{
  const auto f = static_cast<EVoLumEffectFocus>(focus);
  mVolumExpandedSection = volum::SectionForEffectFocus(f);
  mVolumFocusedEffect = f;
  _VolumSetUiMode(volum::UiMode::Build);
}

bool NeuralAmpModeler::VolumRecallSound(const std::string& ampId, const std::string& presetId)
{
  volum::SoundChoice resolved;
  if (!volum::ResolveSound(mVolumFactoryPresets, volum::content::GlobalContentStore().reg(), ampId, presetId, resolved))
    return false;
  const auto settings =
    volum::ResolveSoundSettings(mVolumFactoryPresets, volum::content::GlobalContentStore().reg(), ampId, presetId);
  if (!settings)
    return false;

  if (mVolumInitComplete)
    _VolumSaveCurrentToSettings();

  int factoryAmp = -1;
  if (ampId.rfind("factory:", 0) == 0)
  {
    try
    {
      factoryAmp = std::stoi(ampId.substr(8));
    }
    catch (...)
    {
      return false;
    }
    if (factoryAmp < 0 || factoryAmp >= volum::kAmpCount || volum::content::FactoryOwnerKey(factoryAmp) != ampId)
      return false;
    mVolumAmpIdx = factoryAmp;
    mVolumCustomMainIdx = -1;
    _VolumRestoreFromSettings(factoryAmp);
    _VolumRefreshChannels();
    mVolumNeedsLoad.store(true);
    _VolumSyncPresetOwner();
  }
  else
  {
    const int custom = volum::custom::CustomAmpIndexById(ampId);
    if (custom < 0)
      return false;
    _VolumSelectCustomAmp(custom);
  }

  mVolumActivePresetId = resolved.presetId;
  _VolumApplyRecalledPreset(*settings);
  _VolumSyncLivePlaySlotFromActivePair();

  _VolumSyncUiFromState();
  _VolumRefreshPresetBar();
  _UpdateVoLumLayout();
  _VolumRefreshPlaySurface();
  return true;
}
