// Gear -> Settings: Export Pack... / Import Pack...
//
// The rules are in VoLumPack.h and the chrome is in VoLumPackOverlay.h. This file
// is the seam between them: file dialogs, the machine-settings document, and the
// live rig. Nothing here decides what a Pack contains or what a verb means.
//
// Tail-included by NeuralAmpModeler.cpp.

namespace
{
// Test harness only: VOLUM_PACK_SAVE_PATH / VOLUM_PACK_OPEN_PATH stand in for the
// native Save / Open dialog, which nothing can drive on a locked workstation
// (e2e-standalone-win.ps1 -Scenario pack). Unset or empty, the dialog runs.
std::string VolumPackDialogOverride(const char* name)
{
#if defined(OS_WIN)
  const std::wstring wideName(name, name + std::strlen(name));
  wchar_t buf[2048] = {};
  const DWORD len = GetEnvironmentVariableW(wideName.c_str(), buf, 2048);
  if (len == 0 || len >= 2048)
    return {};
  return volum::content::PathToUtf8(std::filesystem::path(buf));
#else
  const char* v = std::getenv(name);
  return v ? std::string(v) : std::string();
#endif
}
} // namespace

std::string NeuralAmpModeler::_VolumExportPack(const volum::pack::ExportSelection& selection)
{
  auto& store = volum::content::GlobalContentStore();
  store.EnsureLoaded();
  const auto plan = volum::pack::BuildExportPlan(store.reg(), selection);
  if (plan.Empty())
    return "Nothing selected to export.";

  // Only the standalone has a machine-settings document, and only an Everything
  // Pack carries one. A plugin exporting Everything still packs the whole library
  // - it just has no machine to describe.
  std::string settingsJson;
#if defined(APP_API)
  // Debounced idle writes may still be pending; Pack must see the live knobs.
  _VolumSaveCurrentToSettings();
  if (mVolumSettingsDirty || VolumDebounceFor(this).settings.isDirty())
  {
    mVolumSettingsDirty = false;
    VolumDebounceFor(this).settings.markWritten(VolumWriteNowMs());
    _VolumSaveSettingsToFile();
  }
  if (mVolumCalibrationDefaultsDirty || VolumDebounceFor(this).calibration.isDirty())
  {
    mVolumCalibrationDefaultsDirty = false;
    VolumDebounceFor(this).calibration.markWritten(VolumWriteNowMs());
    _VolumSaveCalibrationDefaults();
  }
  if (plan.includeSettings)
  {
    const auto path = volum::VolumUserSettingsFilePath();
    if (!path.empty() && volum::pack::ReadWholeFile(path, settingsJson))
      settingsJson = volum::pack::SettingsWithDualAmp(
        settingsJson, volum::VolumDualAmpUserSettingsToJson(mVolumAmpSettings.data(), volum::kAmpCount));
  }
#endif

  WDL_String fileName, dir;
  const std::string harnessPath = VolumPackDialogOverride("VOLUM_PACK_SAVE_PATH");
  if (!harnessPath.empty())
    fileName.Set(harnessPath.c_str());
  else
  {
    fileName.Set(plan.job == volum::pack::Job::Everything ? "VoLum library.volumpack" : "VoLum pack.volumpack");
    // The macOS panel swallows the mouse-up, which would leave this press captured.
    GetUI()->ReleaseMouseCapture();
    GetUI()->PromptForFile(fileName, dir, EFileAction::Save, "volumpack");
  }
  if (fileName.GetLength() == 0)
    return {}; // cancelled: not a failure

  std::string error;
  auto out = volum::content::PathFromUtf8(fileName.Get());
  if (out.extension() != ".volumpack")
    out += ".volumpack";
  if (!volum::pack::WritePack(store, plan, settingsJson, out, &error))
  {
    if (error.empty())
      error = "Could not write the Pack.";
    VOLUM_LOG("pack", "export failed: " + error);
    return error;
  }
  VOLUM_LOG("pack", std::string("export wrote ") + volum::pack::JobName(plan.job)
                      + " Pack: " + std::to_string(plan.ampIds.size()) + " amps, " + std::to_string(plan.irIds.size())
                      + " IRs, " + std::to_string(plan.pedalIds.size()) + " pedals, "
                      + std::to_string(plan.presetIds.size()) + " presets");
  return {};
}

volum::pack::PackContents NeuralAmpModeler::_VolumPickPack()
{
  WDL_String fileName, dir;
  const std::string harnessPath = VolumPackDialogOverride("VOLUM_PACK_OPEN_PATH");
  if (!harnessPath.empty())
    fileName.Set(harnessPath.c_str());
  else
  {
    // The macOS panel swallows the mouse-up, which would leave this press captured.
    GetUI()->ReleaseMouseCapture();
    GetUI()->PromptForFile(fileName, dir, EFileAction::Open, "volumpack");
  }
  if (fileName.GetLength() == 0)
    return volum::pack::PackContents{}; // cancelled: empty error, so the modal closes quietly
  auto pack = volum::pack::OpenPack(volum::content::PathFromUtf8(fileName.Get()));
  if (pack.ok)
  {
    mVolumOpenedPackDualAmp.Open(pack.settingsJson);
    VOLUM_LOG("pack", std::string("opened ") + volum::pack::PackSummaryLine(pack));
    for (const auto& skipped : pack.skipped)
      VOLUM_LOG("pack", "skipped on open: " + skipped);
  }
  else
    VOLUM_LOG("pack", std::string("open refused: ") + pack.error
                        + (pack.detail.empty() ? std::string() : " (" + pack.detail + ")"));
  return pack;
}

std::vector<std::string> NeuralAmpModeler::_VolumSoundingLibraryIds() const
{
  const auto rig = _VolumSnapshotSoundingRig();
  std::vector<std::string> ids;
  auto add = [&ids](const std::string& id) {
    if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end())
      ids.push_back(id);
  };
  add(rig.mainCustomAmpId);
  if (rig.dualAmpActive)
    add(rig.supportCustomAmpId);
  add(rig.activeIrId);
  if (rig.dualAmpActive)
    add(rig.supportActiveIrId);
  // A PRE slot holds a capture index, not an id; map it back so the preview can
  // talk about the pedal the user recognises.
  const auto& reg = volum::content::GlobalContentStore().reg();
  for (int slot = 0; slot < 2; ++slot)
    for (const auto& p : reg.pedals)
      if (p.legacyIndex == rig.preCapture[slot])
        add(p.id);
  add(rig.recalledPresetId);
  return ids;
}

void NeuralAmpModeler::_VolumRefreshCustomAmpSidebar()
{
  // The sidebar list and the hero keep their own copy of each custom amp's name and
  // art, taken when the row was last built. An import that replaces an amp under the
  // same id changes neither the id nor the row index, so nothing else rebuilds them.
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  const auto& names = volum::custom::MockCustomAmps();
  const int main = mVolumCustomMainIdx;
  const bool mainValid = main >= 0 && main < static_cast<int>(names.size());
  if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
  {
    auto* list = al->As<VoLumAmpListControl>();
    list->SetCustomAmps(names, volum::custom::MockCustomAmpArts());
    if (mainValid)
      list->SetCustomSelected(main);
  }
  if (!mainValid)
    return;
  if (auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
  {
    auto* hero = heroCtrl->As<VoLumHeroImageControl>();
    hero->SetCustomArt(true, volum::custom::CustomAmpArt(main));
    hero->SetName(names[static_cast<size_t>(main)].c_str());
  }
  if (auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
    if (mVolumExpandedSection == EVoLumSection::AMP)
      nameCtrl->As<VoLumSubRowTextControl>()->SetName(names[static_cast<size_t>(main)].c_str(), true);
}

void NeuralAmpModeler::_VolumReloadReplacedLibraryIds(const std::vector<std::string>& ids)
{
  // A confirmed replace keeps every lane where it is and reloads the payload
  // behind the same id. Bouncing to the delete fallback first would be an audible
  // detour to a sound nobody asked for (see VoLumRigRepair.h).
  const auto& reg = volum::content::GlobalContentStore().reg();
  for (const auto& id : ids)
  {
    volum::rig::LibraryKind kind = volum::rig::LibraryKind::CustomAmp;
    bool known = false;
    for (const auto& a : reg.amps)
      if (a.id == id)
      {
        kind = volum::rig::LibraryKind::CustomAmp;
        known = true;
      }
    for (const auto& ir : reg.irs)
      if (ir.id == id)
      {
        kind = volum::rig::LibraryKind::IR;
        known = true;
      }
    for (const auto& p : reg.pedals)
      if (p.id == id)
      {
        kind = volum::rig::LibraryKind::Pedal;
        known = true;
      }
    if (!known)
      continue;
    _VolumPlanLibraryReplace(kind, id, id);
    _VolumApplyPendingRigRepair();
  }
}

std::string NeuralAmpModeler::_VolumImportPack(const volum::pack::PackContents& pack, volum::pack::ImportVerb verb,
                                               bool alsoSettings)
{
  auto& store = volum::content::GlobalContentStore();
  store.EnsureLoaded();

#if defined(APP_API)
  const bool standalone = true;
  const auto settingsPath = volum::VolumUserSettingsFilePath();
#else
  const bool standalone = false;
  const std::filesystem::path settingsPath;
#endif

  // The Dual Amp sidecar travels inside the Pack's settings. It goes into ApplyPack
  // so it is staged before the library commit; writing it afterwards left a window
  // in which the library was new and the sidecar still the old rig's.
  const volum::pack::MachineSidecar* dualAmpSidecar = nullptr;
#if defined(APP_API)
  volum::pack::MachineSidecar packDualAmp;
  if (alsoSettings && !pack.settingsJson.empty()
      && mVolumOpenedPackDualAmp.For(pack.settingsJson, packDualAmp.document))
  {
    packDualAmp.path = volum::VolumDualAmpSettingsFilePath();
    if (packDualAmp.path.empty())
    {
      VOLUM_LOG("pack", "dual-amp settings not restored: no settings folder");
      return "The machine settings could not be written - the import was not applied.";
    }
    dualAmpSidecar = &packDualAmp;
  }
#endif

  const auto result =
    volum::pack::ApplyPack(store, pack, verb, alsoSettings, standalone, settingsPath, 4000, nullptr, dualAmpSidecar);
  static const char* kVerbLog[3] = {"overwrite", "add", "reset"};
  for (const auto& notice : result.notices)
    VOLUM_LOG("pack", notice);
  VOLUM_LOG("pack", std::string("import ") + kVerbLog[(int)verb] + (alsoSettings ? " +settings" : "") + ": "
                      + (result.ok ? std::string("applied, ") + std::to_string(result.replacedItemCount) + " replaced"
                                   : "failed: " + result.error));
  // A settings-file failure still committed the library. Reload replaced captures
  // before reporting that error, or the rig keeps playing the bytes just overwritten.
  if (result.libraryCommitted)
  {
    _VolumMigrateIrTrims();
    _VolumRepairRigForMissingContent(); // Reset can delete an id this rig was playing
    _VolumRefreshCustomAmpSidebar(); // names and art the import replaced, amps it added or removed
    _VolumReloadReplacedLibraryIds(result.replacedIds);
    _VolumReconcileActiveIr();
    _VolumPushIrShaping(false);
    _VolumPushIrShaping(true);
    _VolumSyncPresetOwner();
    _VolumRefreshPresetBar();
    _VolumRefreshMidiSettingsChrome();
    _VolumRefreshPlaySurface();
    _VolumSyncUiFromState();
  }
  if (!result.ok)
    return result.error.empty() ? std::string("The Pack could not be imported.") : result.error;

#if defined(APP_API)
  // An Everything import with the box ticked has just replaced the machine
  // settings file. Read it back and put it on the live rig the way a launch does
  // (constructor + OnUIOpen). Reading alone left the old knobs live on the newly
  // focused amp, and the next settings save wrote them over its restored scene
  // and dropped its preset. Nothing outgoing is snapshotted: the file replaced it.
  if (alsoSettings && !pack.settingsJson.empty())
  {
    {
      // The read swaps every scene under the outgoing live params, and a load in
      // PLAY refreshes the surface, whose dirty check snapshots live into the
      // active scene. No snapshot until the restored scene is live.
      volum::LiveSceneGate::Hold restoring(mVolumLiveSceneGate);
      _VolumLoadSettingsFromFile();
      _VolumSelectFactoryAmp(mVolumAmpIdx, /*snapshotOutgoing=*/false);
      _VolumApplyLiveLockSnapshots();
    }
    _VolumRefreshPrePedalCaptures();
    _VolumRefreshSupportChannels();
    mVolumDidRestorePresetSelection = false;
    _VolumRestoreSessionSelection();
    _VolumSyncUiFromState();
  }
#endif
  return {};
}
