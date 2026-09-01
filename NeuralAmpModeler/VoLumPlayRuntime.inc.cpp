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
    if (auto* surface = pGfx->GetControlWithTag(kCtrlTagVoLumPlaySurface))
      surface->Hide(mode != volum::UiMode::Play);
    if (auto* toggle = pGfx->GetControlWithTag(kCtrlTagVoLumModeToggle))
      toggle->As<VoLumModeToggleControl>()->SetMode(mode);
    if (auto* preset = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
      preset->Hide(mode == volum::UiMode::Play);
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumPresetMenu))
      menu->Hide(true);
    if (auto* overlay = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
      overlay->Hide(true);
    pGfx->SetAllControlsDirty();
  }
#ifdef APP_API
  mVolumSettingsDirty = true;
#endif
  if (mode == volum::UiMode::Play && transition == volum::UiModeTransitionAction::RefreshOnly)
    _VolumRefreshPlaySurface();
  else
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
  // LIVE follows the Sound: the program number that now holds the recalled
  // assignment is the one the highlight has to sit on.
  if (mVolumLastRecalledPlaySlot == slotA)
    mVolumLastRecalledPlaySlot = slotB;
  else if (mVolumLastRecalledPlaySlot == slotB)
    mVolumLastRecalledPlaySlot = slotA;
  _VolumRefreshPlaySurface();
  _VolumRefreshMidiSettingsChrome();
}

void NeuralAmpModeler::_VolumRefreshPlaySurface()
{
  if (mVolumUiMode != volum::UiMode::Play)
    return;
  auto* pGfx = GetUI();
  if (!pGfx)
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

  // A PRE NAM slot with no capture loaded is armed but silent, so the board
  // greys it out the way BUILD leaves the row unnamed. Everything else on the
  // board always has something to bypass.
  std::array<bool, VoLumPlaySurfaceControl::FxCount> fxAvailable{};
  fxAvailable.fill(true);
  fxAvailable[VoLumPlaySurfaceControl::Nam1] = mPreModel[0] != nullptr;
  fxAvailable[VoLumPlaySurfaceControl::Nam2] = mPreModel[1] != nullptr;

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
  raw->As<VoLumPlaySurfaceControl>()->SetInPeak(mVolumPlayInPeak.load(std::memory_order_relaxed));
  raw->As<VoLumPlaySurfaceControl>()->SetPickerGroups(&mVolumPlayPickerGroups);
}

void NeuralAmpModeler::_VolumAddHeardPlaySound()
{
  auto finish = [this]() {
    auto& store = volum::content::GlobalContentStore();
    const int slot = volum::content::FirstFreeMidiSoundSlot(store.reg());
    if (!volum::AddHeardMarksLive(slot, mVolumActivePresetId.empty()))
      return;
    _VolumAssignPlaySound(slot, {_VolumActiveOwnerKey(), mVolumActivePresetId, {}, {}, false, 0, false});
    mVolumLastRecalledPlaySlot = slot;
    _VolumRefreshPlaySurface();
  };
  if (volum::AddHeardNeedsSaveAs(
        volum::SaveActionForActivePreset(mVolumActivePresetId), _VolumLivePresetDirty(), mVolumActivePresetId.empty()))
  {
    _VolumPromptSaveAs(finish);
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

  if (const auto* factory = volum::FindFactoryPresetById(mVolumFactoryPresets, presetId))
  {
    mVolumActivePresetId = factory->id;
    _VolumApplyRecalledPreset(factory->settings);
  }
  else
  {
    const auto bank = volum::content::GlobalContentStore().reg().presetBanks.find(ampId);
    if (bank == volum::content::GlobalContentStore().reg().presetBanks.end())
      return false;
    const auto preset =
      std::find_if(bank->second.begin(), bank->second.end(), [&](const auto& item) { return item.id == presetId; });
    if (preset == bank->second.end())
      return false;
    mVolumActivePresetId = preset->id;
    _VolumApplyRecalledPreset(preset->settings);
  }

  _VolumSyncUiFromState();
  _VolumRefreshPresetBar();
  _UpdateVoLumLayout();
  _VolumRefreshPlaySurface();
  return true;
}
