// Per-amp named preset bank: hooks, owner sync, save/overwrite/recall, dirty.
// Tail-#included via VoLumSettings.inc.cpp (NOT a separate TU); file-size hygiene.

void NeuralAmpModeler::_VolumInstallPresetHooks()
{
  // Capture: sync live params into the active scene, then hand back a copy so a
  // preset records the complete current rig (incl. the id-based custom refs that
  // live on the scene, not on params). Registered per instance so a later claim
  // of the process-global pair cannot make this editor persist another scene.
  volum::custom::InstallInstancePresetHooks(
    this,
    [this]() -> volum::VoLumAmpSettings {
      _VolumSaveCurrentToSettings();
      // Locked PRE/POST live on the overlay, not the amp slot. A preset is a
      // snapshot of the sounding rig, so capture must overlay those blocks
      // without mutating the slot the lock is protecting.
      return volum::SoundingPresetScene(
        _VolumActiveScene(), mVolumPreLocked, mVolumLiveLockedPre, mVolumPostLocked, mVolumLiveLockedPost);
    },
    [this](const volum::VoLumAmpSettings& s) { _VolumApplyRecalledPreset(s); });
  volum::custom::PresetHookOwner() = this;
}

// Claims the process-global preset bridge for this instance, immediately before
// using it.
//
// The bridge exists because the content layer cannot reach live params. Capture
// and apply are keyed per instance (see InstallInstancePresetHooks); this claim
// still publishes the process-global pair and the owner key so overlay listing
// and the legacy index-based signatures keep a current claimant. Save, overwrite,
// and recall wrap themselves in PresetOpScope(this) so they never persist or
// apply through whoever last claimed the globals.
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
      mVolumRecalledSnapshot = volum::HealedFactoryPresetSettings(factory->settings);
      mVolumRecalledSnapshotByOwner[key] = mVolumRecalledSnapshot;
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
  const auto* factoryPreset =
    mVolumCustomMainIdx < 0 ? volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx) : nullptr;
  const bool hasFactory = factoryPreset != nullptr;
  std::vector<std::string> names;
  if (hasFactory)
    names.push_back(factoryPreset->name);
  const auto users = volum::custom::PresetsForOwner(_VolumActiveOwnerKey());
  names.insert(names.end(), users.begin(), users.end());
  bar->SetList(names); // clears selection; dirty is preserved then recomputed below

  bool selected = false;
  if (mVolumHasRecalledSnapshot && !mVolumActivePresetId.empty())
  {
    if (hasFactory)
      if (const auto* factory = volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx);
          factory && factory->id == mVolumActivePresetId)
      {
        bar->SelectAt(0, factory->name, true);
        selected = true;
      }
    if (!selected)
    {
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
        selected = true;
      else
        _VolumForgetActivePreset(); // preset was deleted out from under us
    }
  }

  // Always: deleting the selected User preset forgets the id, SetList blanks
  // the bar, and Default must not see a clean "No Preset" over a live sound.
  if (volum::PresetBarNeedsDirtyRecompute(selected))
    _VolumRecomputePresetDirty();
}

int NeuralAmpModeler::_VolumSavePresetAs(const std::string& name)
{
  volum::custom::PresetOpScope op(this);
  _VolumClaimPresetOps();
  const int idx = volum::custom::AddPresetForOwner(_VolumActiveOwnerKey(), name); // captures live via hook
  if (idx < 0)
    return idx;
  // The freshly saved preset becomes the active, clean recalled snapshot.
  mVolumActivePresetId = volum::custom::PresetIdAtForOwner(_VolumActiveOwnerKey(), idx);
  mVolumRecalledSnapshot = volum::SoundingPresetScene(
    _VolumActiveScene(), mVolumPreLocked, mVolumLiveLockedPre, mVolumPostLocked, mVolumLiveLockedPost);
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
  _VolumReassignLivePlaySlotAfterSave();
  return idx;
}

void NeuralAmpModeler::_VolumOverwritePreset(int index)
{
  volum::custom::PresetOpScope op(this);
  _VolumClaimPresetOps();
  if (!volum::custom::OverwritePresetForOwner(_VolumActiveOwnerKey(), index)) // captures live via hook
    return;
  mVolumActivePresetId = volum::custom::PresetIdAtForOwner(_VolumActiveOwnerKey(), index);
  mVolumRecalledSnapshot = volum::SoundingPresetScene(
    _VolumActiveScene(), mVolumPreLocked, mVolumLiveLockedPre, mVolumPostLocked, mVolumLiveLockedPost);
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
  _VolumReassignLivePlaySlotAfterSave();
}

void NeuralAmpModeler::_VolumRecallPreset(int index)
{
  const bool hasFactory =
    mVolumCustomMainIdx < 0 && volum::FindFactoryPresetForAmp(mVolumFactoryPresets, mVolumAmpIdx) != nullptr;
  if (hasFactory && index == 0)
  {
    _VolumRecallFactoryPreset();
    return;
  }
  _VolumRecallUserPreset(index - (hasFactory ? 1 : 0));
}

void NeuralAmpModeler::_VolumRecallUserPreset(int index)
{
  volum::custom::PresetOpScope op(this);
  _VolumClaimPresetOps();
  mVolumActivePresetId = volum::custom::PresetIdAtForOwner(_VolumActiveOwnerKey(), index);
  if (!volum::custom::RecallPresetForOwner(_VolumActiveOwnerKey(), index)) // -> apply hook -> _VolumApplyRecalledPreset
    return;
  _VolumSyncLivePlaySlotFromActivePair();
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

void NeuralAmpModeler::_VolumSetMidiRecallCc(int cc)
{
  mVolumMidiRecallCc.store(volum::ClampMidiRecallCc(cc));
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
  auto* raw = pGfx->GetControlWithTag(kCtrlTagSettingsBox);
  if (!raw)
    return;
  auto* page = raw->As<NAMSettingsPageControl>();
  page->SetMidiChannel(mVolumMidiChannel.load());
  page->SetMidiRecallCc(mVolumMidiRecallCc.load());
  // Rebuilt from the live registry every time, never cached: the map is machine
  // global, so another instance or the PLAY rail can have changed it since the
  // panel was last opened.
  page->SetMidiSoundMap(mVolumFactoryPresets, volum::content::GlobalContentStore().reg(), mVolumLastRecalledPlaySlot,
                        _VolumActiveOwnerKey(), mVolumActivePresetId);
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
  _VolumSyncLivePlaySlotFromActivePair();
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
  bar->SetDirtyState(_VolumLivePresetDirty());
}

bool NeuralAmpModeler::_VolumLivePresetDirty()
{
  _VolumSaveCurrentToSettings();
  const auto sounding = volum::SoundingPresetScene(
    _VolumActiveScene(), mVolumPreLocked, mVolumLiveLockedPre, mVolumPostLocked, mVolumLiveLockedPost);
  return volum::LivePresetDirty(mVolumHasRecalledSnapshot, sounding, mVolumRecalledSnapshot);
}

void NeuralAmpModeler::_VolumPromptSaveAs(std::function<void()> after)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* raw = pGfx->GetControlWithTag(kCtrlTagVoLumNameDialog);
  if (!raw)
    return;
  const auto action = volum::SaveActionForActivePreset(mVolumActivePresetId);
  const std::string ownerKey = _VolumClaimPresetOps();
  std::string currentName;
  std::string currentId;
  if (action == volum::PresetSaveAction::OverwriteUser)
  {
    const int idx = volum::custom::PresetIndexByIdForOwner(ownerKey, mVolumActivePresetId);
    const auto users = volum::custom::PresetsForOwner(ownerKey);
    if (idx >= 0 && idx < static_cast<int>(users.size()))
    {
      currentName = users[static_cast<size_t>(idx)];
      currentId = mVolumActivePresetId;
    }
  }
  const std::string seed = volum::SaveDialogSeedName(action, currentName);
  VOLUM_LOG("preset", "save dialog open (" + std::string(currentId.empty() ? "new" : "may update") + ")");
  raw->As<VoLumNameDialogControl>()->Show(
    "Save preset", "Name this User preset.", seed, currentName,
    [this, after, currentName, currentId](const std::string& name) {
      // The overwrite target is looked up by id now, not by an index remembered
      // when the dialog opened: the bank can be edited or reordered in between.
      const int overwriteIdx = volum::name_dialog::Overwrites(name, currentName)
                                 ? volum::custom::PresetIndexByIdForOwner(_VolumActiveOwnerKey(), currentId)
                                 : -1;
      bool ok = false;
      if (overwriteIdx >= 0)
      {
        _VolumOverwritePreset(overwriteIdx);
        ok = true;
      }
      else
        ok = _VolumSavePresetAs(name) >= 0;
      VOLUM_LOG("preset", std::string("save dialog commit: ") + (overwriteIdx >= 0 ? "updated '" : "saved '") + name
                            + "'" + (ok ? "" : " (refused)"));
      if (!ok)
        return;
      if (after)
        after();
    },
    []() { VOLUM_LOG("preset", "save dialog cancelled: nothing written"); });
}

bool NeuralAmpModeler::_VolumHandleSaveShortcut()
{
  _VolumPromptSaveAs();
  return true;
}
