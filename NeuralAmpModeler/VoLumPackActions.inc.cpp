// Gear -> Settings: Export Pack... / Import Pack...
//
// The rules are in VoLumPack.h and the chrome is in VoLumPackOverlay.h. This file
// is the seam between them: file dialogs, the machine-settings document, and the
// live rig. Nothing here decides what a Pack contains or what a verb means.
//
// Tail-included by NeuralAmpModeler.cpp.

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
  if (plan.includeSettings)
  {
    const auto path = volum::VolumUserSettingsFilePath();
    if (!path.empty())
      volum::pack::ReadWholeFile(path, settingsJson);
  }
#endif

  WDL_String fileName, dir;
  fileName.Set(plan.job == volum::pack::Job::Everything ? "VoLum library.volumpack" : "VoLum pack.volumpack");
  GetUI()->PromptForFile(fileName, dir, EFileAction::Save, "volumpack");
  if (fileName.GetLength() == 0)
    return {}; // cancelled: not a failure

  std::string error;
  auto out = volum::content::PathFromUtf8(fileName.Get());
  if (out.extension() != ".volumpack")
    out += ".volumpack";
  if (!volum::pack::WritePack(store, plan, settingsJson, out, &error))
    return error.empty() ? std::string("Could not write the Pack.") : error;
  return {};
}

volum::pack::PackContents NeuralAmpModeler::_VolumPickPack()
{
  WDL_String fileName, dir;
  GetUI()->PromptForFile(fileName, dir, EFileAction::Open, "volumpack");
  if (fileName.GetLength() == 0)
    return volum::pack::PackContents{}; // cancelled: empty error, so the modal closes quietly
  return volum::pack::OpenPack(volum::content::PathFromUtf8(fileName.Get()));
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

  const auto result = volum::pack::ApplyPack(store, pack, verb, alsoSettings, standalone, settingsPath);
  if (!result.ok)
    return result.error.empty() ? std::string("The Pack could not be imported.") : result.error;

  // The catalog changed under the live rig. Re-derive everything that reads it,
  // in the same order the Manage panel's own change hook uses.
  _VolumMigrateIrTrims();
  _VolumRepairRigForMissingContent(); // Reset can delete an id this rig was playing
  _VolumReloadReplacedLibraryIds(result.replacedIds);
  _VolumReconcileActiveIr();
  _VolumPushIrShaping(false);
  _VolumPushIrShaping(true);
  _VolumSyncPresetOwner();
  _VolumRefreshPresetBar();
  _VolumSyncUiFromState();

#if defined(APP_API)
  // An Everything import with the box ticked has just replaced the machine
  // settings file; read it back so this window matches what it will restore next
  // launch instead of overwriting it on the next knob move.
  if (alsoSettings && !pack.settingsJson.empty())
    _VolumLoadSettingsFromFile();
#endif
  return {};
}
