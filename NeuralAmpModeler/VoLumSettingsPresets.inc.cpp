// Per-amp named preset bank: hooks, owner sync, save/overwrite/recall, dirty.
// Tail-#included via VoLumSettings.inc.cpp (NOT a separate TU); file-size hygiene.

void NeuralAmpModeler::_VolumInstallPresetHooks()
{
  // Capture: sync live params into the active scene, then hand back a copy so a
  // preset records the complete current rig (incl. the id-based custom refs that
  // live on the scene, not on params).
  volum::custom::PresetCaptureHook() = [this]() -> volum::VoLumAmpSettings {
    _VolumSaveCurrentToSettings();
    return _VolumActiveScene();
  };
  volum::custom::PresetApplyHook() = [this](const volum::VoLumAmpSettings& s) { _VolumApplyRecalledPreset(s); };
  volum::custom::PresetHookOwner() = this;
}

// Claims the process-global preset bridge for this instance, immediately before
// using it.
//
// The bridge exists because the content layer cannot reach live params, but it is a
// single set of globals shared by every instance in the host. Installing the hooks
// once at construction meant the most recently created instance owned capture and
// recall for all of them: saving a preset in the first instance stored the second
// instance's rig, recalling in the first changed the second, and once that instance
// was closed the hooks still held its destroyed `this`. Re-binding per operation
// makes the caller the owner, and the caller is by definition alive.
// Returns this instance's owner key so the caller can pass it explicitly instead
// of reading the ambient global back out. The global is still set for the legacy
// index-based bridge signatures, but nothing in the plugin depends on it.
std::string NeuralAmpModeler::_VolumClaimPresetOps()
{
  _VolumInstallPresetHooks();
  const std::string key = _VolumActiveOwnerKey();
  volum::custom::SetActivePresetOwner(key);
  return key;
}

void NeuralAmpModeler::_VolumRememberActivePreset()
{
  const std::string key = _VolumActiveOwnerKey();
  if (mVolumHasRecalledSnapshot && !mVolumActivePresetId.empty())
  {
    mVolumActivePresetIdByOwner[key] = mVolumActivePresetId;
    mVolumRecalledSnapshotByOwner[key] = mVolumRecalledSnapshot;
  }
  else
  {
    mVolumActivePresetIdByOwner.erase(key);
    mVolumRecalledSnapshotByOwner.erase(key);
  }
}

void NeuralAmpModeler::_VolumForgetActivePreset()
{
  mVolumHasRecalledSnapshot = false;
  mVolumActivePresetId.clear();
  mVolumActivePresetIdByOwner.erase(_VolumActiveOwnerKey());
  mVolumRecalledSnapshotByOwner.erase(_VolumActiveOwnerKey());
}

void NeuralAmpModeler::_VolumSyncPresetOwner()
{
  const std::string key = _VolumActiveOwnerKey();
  volum::custom::SetActivePresetOwner(key);
  // Restore the preset this amp last had selected (if any) so switching back to
  // an amp re-shows its active preset instead of blanking the bar. The id is
  // validated against the live bank in _VolumRefreshPresetBar, so a since-deleted
  // preset simply shows nothing selected.
  auto itId = mVolumActivePresetIdByOwner.find(key);
  if (itId == mVolumActivePresetIdByOwner.end() || itId->second.empty())
  {
    mVolumHasRecalledSnapshot = false;
    mVolumActivePresetId.clear();
    return;
  }
  mVolumActivePresetId = itId->second;

  auto itSnap = mVolumRecalledSnapshotByOwner.find(key);
  if (itSnap != mVolumRecalledSnapshotByOwner.end())
  {
    mVolumRecalledSnapshot = itSnap->second;
    mVolumHasRecalledSnapshot = true;
    return;
  }

  // An id with no snapshot beside it: this selection was read back from
  // volum-settings.json, which stores ids only. Previously both were required, so
  // every amp restored from the file was dropped here and reported no preset - the
  // file remembered the selection and this function threw it away. The baseline the
  // "(unsaved)" marker diffs against is then the preset's own stored content, which
  // is the same choice the DAW-chunk restore path makes.
  if (mVolumCustomMainIdx < 0)
    if (const auto* factory = volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx);
        factory && factory->id == mVolumActivePresetId)
    {
      mVolumRecalledSnapshot = factory->settings;
      mVolumRecalledSnapshotByOwner[key] = factory->settings;
      mVolumHasRecalledSnapshot = true;
      return;
    }

  const auto& banks = volum::content::GlobalContentStore().reg().presetBanks;
  auto itBank = banks.find(key);
  if (itBank != banks.end())
    for (const auto& pr : itBank->second)
      if (pr.id == mVolumActivePresetId)
      {
        mVolumRecalledSnapshot = pr.settings;
        mVolumRecalledSnapshotByOwner[key] = pr.settings;
        mVolumHasRecalledSnapshot = true;
        return;
      }

  // Recorded, but deleted from the bank since. Drop the stale id rather than
  // carrying it to the next launch.
  mVolumHasRecalledSnapshot = false;
  mVolumActivePresetId.clear();
  mVolumActivePresetIdByOwner.erase(key);
}

void NeuralAmpModeler::_VolumRefreshPresetBar()
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar);
  if (!pb)
    return;
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  auto* bar = pb->As<VoLumPresetBarControl>();
  // Owner-explicit for the User rows: with two editors open, the ambient owner key
  // belongs to whichever one last switched amps, so reading "the active bank"
  // through it could show another instance's presets in this bar. The shipped
  // Ready row is not a library item, so it is prepended here.
  const bool hasFactory = mVolumCustomMainIdx < 0
    && volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx) != nullptr;
  std::vector<std::string> names;
  if (hasFactory)
    names.push_back(volum::kFactoryPresetDisplayName);
  const auto users = volum::custom::PresetsForOwner(_VolumActiveOwnerKey());
  names.insert(names.end(), users.begin(), users.end());
  bar->SetList(names); // clears selection + dirty

  if (mVolumHasRecalledSnapshot && !mVolumActivePresetId.empty())
  {
    if (hasFactory)
      if (const auto* factory = volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx);
          factory && factory->id == mVolumActivePresetId)
      {
        bar->SelectAt(0, volum::kFactoryPresetDisplayName, true);
        _VolumRecomputePresetDirty();
        return;
      }
    const auto& banks = volum::content::GlobalContentStore().reg().presetBanks;
    auto it = banks.find(_VolumActiveOwnerKey());
    bool found = false;
    if (it != banks.end())
      for (int i = 0; i < static_cast<int>(it->second.size()); ++i)
        if (const auto& pr = it->second[static_cast<size_t>(i)]; pr.id == mVolumActivePresetId)
        {
          bar->SelectAt(i + (hasFactory ? 1 : 0), pr.name.c_str(), false);
          found = true;
          break;
        }
    if (found)
      _VolumRecomputePresetDirty();
    else
      _VolumForgetActivePreset(); // preset was deleted out from under us
  }
}

int NeuralAmpModeler::_VolumSavePresetAs(const std::string& name)
{
  _VolumClaimPresetOps();
  const int idx = volum::custom::AddPresetForOwner(_VolumActiveOwnerKey(), name); // captures live via hook
  if (idx < 0)
    return idx;
  // The freshly saved preset becomes the active, clean recalled snapshot.
  mVolumActivePresetId = volum::custom::PresetIdAtForOwner(_VolumActiveOwnerKey(), idx);
  mVolumRecalledSnapshot = _VolumActiveScene(); // hook already synced live -> scene
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
  return idx;
}

void NeuralAmpModeler::_VolumOverwritePreset(int index)
{
  _VolumClaimPresetOps();
  volum::custom::OverwritePresetForOwner(_VolumActiveOwnerKey(), index); // captures live via hook
  mVolumActivePresetId = volum::custom::PresetIdAtForOwner(_VolumActiveOwnerKey(), index);
  mVolumRecalledSnapshot = _VolumActiveScene();
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
}

void NeuralAmpModeler::_VolumRecallPreset(int index)
{
  const bool hasFactory = mVolumCustomMainIdx < 0
    && volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx) != nullptr;
  if (hasFactory && index == 0)
  {
    _VolumRecallFactoryPreset();
    return;
  }
  _VolumRecallUserPreset(index - (hasFactory ? 1 : 0));
}

void NeuralAmpModeler::_VolumRecallUserPreset(int index)
{
  _VolumClaimPresetOps();
  mVolumActivePresetId = volum::custom::PresetIdAtForOwner(_VolumActiveOwnerKey(), index);
  volum::custom::RecallPresetForOwner(_VolumActiveOwnerKey(), index); // -> apply hook -> _VolumApplyRecalledPreset
  _VolumRefreshPresetBar();
  if (GetUI())
    _VolumSyncUiFromState();
  else
    mVolumUiSyncPending.store(true);
}

bool NeuralAmpModeler::_VolumRecallSound(const std::string& ampId, const std::string& presetId)
{
  return VolumRecallSound(ampId, presetId);
}

void NeuralAmpModeler::_VolumSetMidiChannel(int channel)
{
  mVolumMidiChannel.store(std::clamp(channel, 0, volum::kMidiChannelCount));
#ifdef APP_API
  mVolumSettingsDirty = true;
#endif
  DirtyParametersFromUI();
  _VolumRefreshMidiSettingsChrome();
}

void NeuralAmpModeler::_VolumRefreshMidiSettingsChrome()
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  // Settings shows the channel only; PLAY owns the Sound assignment list, so
  // there is no row/choice model to rebuild here.
  if (auto* raw = pGfx->GetControlWithTag(kCtrlTagSettingsBox))
    raw->As<NAMSettingsPageControl>()->SetMidiChannel(mVolumMidiChannel.load());
}

void NeuralAmpModeler::_VolumRecallFactoryPreset()
{
  if (mVolumCustomMainIdx >= 0)
    return;
  const auto* preset = volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx);
  if (!preset)
    return;
  mVolumActivePresetId = preset->id;
  _VolumApplyRecalledPreset(preset->settings);
  _VolumRefreshPresetBar();
}

void NeuralAmpModeler::_VolumApplyRecalledPreset(const volum::VoLumAmpSettings& s)
{
  _VolumActiveScene() = s; // make the live scene equal the preset
  _VolumApplyAmpSettings(_VolumActiveScene());
  // Refresh the cab row / channel stepper for the FOCUSED amp. When a custom amp
  // is focused, the factory _VolumRefreshChannels() would rescan the underlying
  // factory rig folder and clobber the custom lane's cab names, channel stepper,
  // and (critically) mVolumCustomMainSlot/Channel that the .nam loader reads -
  // so recalling a second preset after a factory<->custom round-trip loaded the
  // wrong capture and desynced the UI. Mirror _VolumSelectCustomAmp instead.
  if (mVolumCustomMainIdx >= 0)
    _VolumApplyCustomMainCabs(mVolumCustomMainIdx, false);
  else
    _VolumRefreshChannels();
  if (mVolumCustomSupportIdx >= 0)
    _VolumApplyCustomMainCabs(mVolumCustomSupportIdx, true);
  // Both lanes have now staged their own capture; the shared cab row must end up
  // describing the focused one. Without this, whichever lane was reconciled last won
  // the row - and the SUPPORT branch above runs last whenever a custom partner is
  // loaded. The mirror gap was a factory support amp, which is not reconciled here at
  // all, leaving MAIN's cab on screen while SUPPORT was focused.
  _VolumApplyFocusedLaneCabs();
  // Re-derive the scene from the now-live params so the retained baseline matches
  // exactly what _VolumRecomputePresetDirty() will read back (avoids a spurious
  // "(unsaved)" right after recall from param normalization).
  _VolumSaveCurrentToSettings();
  mVolumRecalledSnapshot = _VolumActiveScene();
  mVolumHasRecalledSnapshot = true;
  mVolumNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
}

void NeuralAmpModeler::_VolumRecomputePresetDirty()
{
  if (!mVolumInitComplete)
    return;
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar);
  if (!pb)
    return;
  auto* bar = pb->As<VoLumPresetBarControl>();
  if (!mVolumHasRecalledSnapshot)
  {
    bar->SetDirtyState(false);
    return;
  }
  _VolumSaveCurrentToSettings();
  const bool dirty = !volum::AmpSettingsEqual(_VolumActiveScene(), mVolumRecalledSnapshot);
  bar->SetDirtyState(dirty);
}
