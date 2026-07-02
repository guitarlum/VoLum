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
  auto itSnap = mVolumRecalledSnapshotByOwner.find(key);
  if (itId != mVolumActivePresetIdByOwner.end() && !itId->second.empty()
      && itSnap != mVolumRecalledSnapshotByOwner.end())
  {
    mVolumActivePresetId = itId->second;
    mVolumRecalledSnapshot = itSnap->second;
    mVolumHasRecalledSnapshot = true;
  }
  else
  {
    mVolumHasRecalledSnapshot = false;
    mVolumActivePresetId.clear();
  }
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
  bar->SetList(volum::custom::MockPresetsForAmp(mVolumAmpIdx)); // clears selection + dirty

  if (mVolumHasRecalledSnapshot && !mVolumActivePresetId.empty())
  {
    const auto& banks = volum::content::GlobalContentStore().reg().presetBanks;
    auto it = banks.find(_VolumActiveOwnerKey());
    bool found = false;
    if (it != banks.end())
      for (const auto& pr : it->second)
        if (pr.id == mVolumActivePresetId)
        {
          bar->SelectName(pr.name.c_str());
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
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  const int idx = volum::custom::AddPreset(mVolumAmpIdx, name); // captures live via hook
  if (idx < 0)
    return idx;
  // The freshly saved preset becomes the active, clean recalled snapshot.
  mVolumActivePresetId = volum::custom::PresetIdAt(idx);
  mVolumRecalledSnapshot = _VolumActiveScene(); // hook already synced live -> scene
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
  return idx;
}

void NeuralAmpModeler::_VolumOverwritePreset(int index)
{
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  volum::custom::OverwritePreset(mVolumAmpIdx, index); // captures live via hook
  mVolumActivePresetId = volum::custom::PresetIdAt(index);
  mVolumRecalledSnapshot = _VolumActiveScene();
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
}

void NeuralAmpModeler::_VolumRecallPreset(int index)
{
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  mVolumActivePresetId = volum::custom::PresetIdAt(index);
  volum::custom::RecallPreset(mVolumAmpIdx, index); // -> apply hook -> _VolumApplyRecalledPreset
  _VolumRefreshPresetBar();
}

void NeuralAmpModeler::_VolumApplyRecalledPreset(const volum::VoLumAmpSettings& s)
{
  _VolumActiveScene() = s; // make the live scene equal the preset
  _VolumApplyAmpSettings(_VolumActiveScene());
  _VolumRefreshChannels();
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