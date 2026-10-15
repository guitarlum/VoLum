#pragma once

// Migrations an instance runs over a library an older VoLum wrote. Both are
// idempotent and lossless: nothing the old version stored is dropped before its
// migrated copy is on disk, and anything that cannot be migrated yet is left
// exactly as it was so the next launch tries again.

#include <cmath>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "VoLumContentStore.h"

namespace volum::content
{

// This instance's live scene for one custom amp, seeded on first touch from a
// pre-1.3.0 library's shared scene, otherwise default-constructed (the amp's
// factory-default settings).
//
// The library entry is copied, never moved. In a DAW the migrated copy is only
// durable once the host saves the project, which VoLum cannot observe; erasing
// the source here made "open an old project, quit without saving" lose the scene.
// The instance's own entry shadows the library's from then on, so the copy can
// never pull pre-upgrade knobs over later edits.
inline VoLumAmpSettings& InstanceCustomScene(std::map<std::string, VoLumAmpSettings>& instanceScenes,
                                             const std::map<std::string, VoLumAmpSettings>& legacyScenes,
                                             const std::string& ampId)
{
  const auto existing = instanceScenes.find(ampId);
  if (existing != instanceScenes.end())
    return existing->second;
  auto& scene = instanceScenes[ampId];
  const auto legacy = legacyScenes.find(ampId);
  if (legacy != legacyScenes.end())
    scene = legacy->second;
  return scene;
}

// Broadband L2 norm of the IR at `file`; false when it cannot be read or holds
// no samples.
using IrEnergyMeter = std::function<bool(const std::filesystem::path& file, double& l2)>;

struct IrTrimMigrationResult
{
  std::vector<std::pair<std::string, double>> calibrated; // name, trim dB stored this pass
  std::vector<std::string> pendingNames; // unreadable: left for the next pass
  bool backedUp = false;
  bool saved = false;
};

// One-time migration for IRs imported before 1.2.1 (no stored trim): measure the
// .wav's broadband energy and auto-normalize so they stop landing ~18 dB quieter
// than the baked stock cabs.
//
// An IR that cannot be read stays uncalibrated, and RegistryToJson keeps it that
// way on disk, so it is measured once it is readable again (a library restored
// from a backup, a sync that has not finished, a drive not mounted yet). Storing
// 0 dB for it instead marked it calibrated for good: the IR came back ~18 dB
// quiet and no later launch looked at it again.
inline IrTrimMigrationResult MigrateIrTrims(ContentStore& store, const IrEnergyMeter& measure)
{
  IrTrimMigrationResult result;
  auto& irs = store.reg().irs;
  bool anyPending = false;
  for (const auto& ir : irs)
    anyPending = anyPending || !ir.trimCalibrated;
  if (!anyPending)
    return result;

  // This rewrites the user's library in place and cannot be undone by going back
  // to 1.2.0 (a v2 build that re-saves drops trimDb/lowCutHz/highCutHz), so keep
  // a one-time pre-migration copy alongside it. Taken before the first attempt,
  // not the first success: a pass that measures nothing still lets other saves
  // rewrite the file before a later pass gets to it.
  result.backedUp = store.BackupBeforeMigration("1.2.1");

  for (auto& ir : irs)
  {
    if (ir.trimCalibrated)
      continue;
    const auto abs = store.ResolveStored(ir.file);
    double l2 = 0.0;
    if (abs.empty() || !measure(abs, l2) || !std::isfinite(l2))
    {
      result.pendingNames.push_back(ir.name);
      continue;
    }
    ir.trimDb = AutoNormalizeIrTrimDb(l2);
    ir.trimCalibrated = true;
    result.calibrated.emplace_back(ir.name, ir.trimDb);
  }
  if (!result.calibrated.empty())
    result.saved = store.Save();
  return result;
}

} // namespace volum::content
