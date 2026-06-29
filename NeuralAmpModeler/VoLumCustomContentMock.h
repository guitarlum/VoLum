#pragma once

// VoLum 1.2.0 custom-content session API (F5-F8).
//
// Historically this header held an in-memory mock. It is now a thin *projection
// bridge* over the real backend (VoLumContentStore.h): the UI keeps calling the
// same index-based volum::custom::* functions, but reads now project the live
// registry and writes mutate it (id-backed) and persist via the process-global
// ContentStore.
//
//   - Getters (MockCustomAmps, MockIRLibrary, ...) rebuild a cached projection
//     of the registry each call and return a reference to it. Callers read them
//     immediately (never hold across a mutation), so the rebuild-per-call cache
//     is safe.
//   - Mutators (Add*/Update*/Remove*/Rename*/Delete*/*Preset) edit the registry
//     through stable opaque ids and call Save(). In unit tests the global store
//     has an empty base dir, so Save() is a no-op and the real user content
//     directory is never touched.
//
// The pure manifest model + (speaker x channel) helpers live in
// VoLumCustomModel.h (included here and by the store). Preset banks are keyed by
// FactoryOwnerKey(ampIdx) so the int-keyed shell API maps onto the registry's
// string-keyed banks.

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>

#include "VoLumContentStore.h"
#include "VoLumCustomModel.h"

namespace volum
{
namespace custom
{

// Convenience accessor for the process-global content store.
inline content::ContentStore& Store()
{
  return content::GlobalContentStore();
}

// A demo custom amp used as a test fixture for the validation/coverage helpers
// (SaveDisabledReason, AmpSlots, etc.): a partial-rig NAM with DIRECT + two named
// cabs, sparse channel coverage, and one deliberately unassigned file.
inline CustomAmp MockDemoCustomAmp()
{
  CustomAmp a;
  a.id = "demo-plexi";
  a.name = "My Plexi A/B";
  a.art = 0;
  a.cabNames = {"G65", "V30", "CB3"};
  a.files = {
    {"AMP-Plexi-direct-1.nam", kDirectSlot, 1},
    {"AMP-Plexi-direct-2.nam", kDirectSlot, 2},
    {"G65-Plexi-Ch1.nam", 0, 1},
    {"V30-Plexi-Ch3.nam", 1, 3},
    {"dump-take7.nam", kUnassignedSlot, 0}, // needs assignment
  };
  return a;
}

// ---------------------------------------------------------------------------
// Custom amps (F6) - projected from the registry
// ---------------------------------------------------------------------------

inline std::vector<std::string>& MockCustomAmps()
{
  static std::vector<std::string> v;
  v.clear();
  for (const auto& a : Store().reg().amps)
    v.push_back(a.name);
  return v;
}

inline std::vector<int>& MockCustomAmpArts()
{
  static std::vector<int> v;
  v.clear();
  for (const auto& a : Store().reg().amps)
    v.push_back(a.art);
  return v;
}

inline std::vector<std::array<std::string, kNumCabSlots>>& MockCustomAmpCabs()
{
  static std::vector<std::array<std::string, kNumCabSlots>> v;
  v.clear();
  for (const auto& a : Store().reg().amps)
    v.push_back(a.cabNames);
  return v;
}

inline std::vector<std::vector<CustomNamFile>>& MockCustomAmpFiles()
{
  static std::vector<std::vector<CustomNamFile>> v;
  v.clear();
  for (const auto& a : Store().reg().amps)
    v.push_back(a.files);
  return v;
}

inline int CustomAmpArt(int idx)
{
  const auto& amps = Store().reg().amps;
  return (idx >= 0 && idx < (int)amps.size()) ? amps[(size_t)idx].art : 0;
}

// Stable opaque id for a custom amp index ("" when out of range). Used to bridge
// the index-based UI to the id-keyed registry (scenes, support refs, presets).
inline std::string CustomAmpIdAt(int idx)
{
  const auto& amps = Store().reg().amps;
  return (idx >= 0 && idx < (int)amps.size()) ? amps[(size_t)idx].id : std::string();
}

// Index of a custom amp by id (-1 when not found).
inline int CustomAmpIndexById(const std::string& id)
{
  if (id.empty())
    return -1;
  const auto& amps = Store().reg().amps;
  for (int i = 0; i < (int)amps.size(); ++i)
    if (amps[(size_t)i].id == id)
      return i;
  return -1;
}

inline CustomAmp CustomAmpAt(int idx)
{
  const auto& amps = Store().reg().amps;
  if (idx < 0 || idx >= (int)amps.size())
    return CustomAmp{};
  return amps[(size_t)idx];
}

// Add a custom amp from a full builder draft, de-duplicating the display name and
// minting a stable id. Returns its index.
inline int AddCustomAmp(const CustomAmp& amp)
{
  auto& reg = Store().reg();
  CustomAmp a = amp;
  NormalizeAmpCabNames(a);
  const std::string base = a.name.empty() ? "New custom amp" : a.name;
  std::string unique = base;
  int suffix = 2;
  auto clashes = [&](const std::string& n) {
    for (const auto& e : reg.amps)
      if (e.name == n)
        return true;
    return false;
  };
  while (clashes(unique))
    unique = base + " " + std::to_string(suffix++);
  a.name = unique;
  a.art = ((a.art % kNumCustomArts) + kNumCustomArts) % kNumCustomArts;
  if (a.id.empty())
    a.id = content::MintId(reg, "amp");
  reg.amps.push_back(std::move(a));
  Store().Save();
  return (int)reg.amps.size() - 1;
}

// Update an existing custom amp in place (preserving its id), keeping the display
// name unique against the *other* amps. Returns the index, or -1 if out of range.
inline int UpdateCustomAmp(int idx, const CustomAmp& amp)
{
  auto& reg = Store().reg();
  if (idx < 0 || idx >= (int)reg.amps.size())
    return -1;
  CustomAmp a = amp;
  a.id = reg.amps[(size_t)idx].id; // identity is immutable across edits
  NormalizeAmpCabNames(a);
  const std::string base = a.name.empty() ? "New custom amp" : a.name;
  std::string unique = base;
  int suffix = 2;
  auto clashesWithOther = [&](const std::string& n) {
    for (int i = 0; i < (int)reg.amps.size(); ++i)
      if (i != idx && reg.amps[(size_t)i].name == n)
        return true;
    return false;
  };
  while (clashesWithOther(unique))
    unique = base + " " + std::to_string(suffix++);
  a.name = unique;
  a.art = ((a.art % kNumCustomArts) + kNumCustomArts) % kNumCustomArts;
  reg.amps[(size_t)idx] = std::move(a);
  Store().Save();
  return idx;
}

// Convenience overload (name + art only) used by light callers/tests.
inline int AddCustomAmp(const std::string& name, int art = 0)
{
  CustomAmp a;
  a.name = name;
  a.art = art;
  return AddCustomAmp(a);
}

inline void RemoveCustomAmp(int idx)
{
  auto& reg = Store().reg();
  if (idx < 0 || idx >= (int)reg.amps.size())
    return;
  Store().RemoveCustomAmp(reg.amps[(size_t)idx].id);
  Store().Save();
}

// ---------------------------------------------------------------------------
// IR library (F7) - projected from the registry
// ---------------------------------------------------------------------------

inline std::vector<std::string>& MockIRLibrary()
{
  static std::vector<std::string> v;
  v.clear();
  for (const auto& ir : Store().reg().irs)
    v.push_back(ir.name);
  return v;
}

inline std::vector<std::string>& MockIRFiles()
{
  static std::vector<std::string> v;
  v.clear();
  for (const auto& ir : Store().reg().irs)
    v.push_back(ir.file);
  return v;
}

inline std::string IRFileAt(int idx)
{
  const auto& irs = Store().reg().irs;
  return (idx >= 0 && idx < (int)irs.size()) ? irs[(size_t)idx].file : std::string();
}

inline std::string IRIdAt(int idx)
{
  const auto& irs = Store().reg().irs;
  return (idx >= 0 && idx < (int)irs.size()) ? irs[(size_t)idx].id : std::string();
}

inline int IRIndexById(const std::string& id)
{
  if (id.empty())
    return -1;
  const auto& irs = Store().reg().irs;
  for (int i = 0; i < (int)irs.size(); ++i)
    if (irs[(size_t)i].id == id)
      return i;
  return -1;
}

// Append a custom IR to the global library. `file` is the stored registry-
// relative path (the plugin copies the source in first); in tests it is just a
// filename. Returns its index.
inline int AddIR(const std::string& name, const std::string& file = "")
{
  auto& reg = Store().reg();
  content::IRItem it;
  it.id = content::MintId(reg, "ir");
  it.name = name.empty() ? "Imported IR" : name;
  it.file = file;
  reg.irs.push_back(std::move(it));
  Store().Save();
  return (int)reg.irs.size() - 1;
}

inline void RenameIR(int idx, const std::string& name)
{
  auto& irs = Store().reg().irs;
  if (idx >= 0 && idx < (int)irs.size() && !name.empty())
  {
    irs[(size_t)idx].name = name;
    Store().Save();
  }
}

inline void DeleteIR(int idx)
{
  auto& irs = Store().reg().irs;
  if (idx >= 0 && idx < (int)irs.size())
  {
    Store().RemoveIR(irs[(size_t)idx].id);
    Store().Save();
  }
}

// ---------------------------------------------------------------------------
// Imported pedal captures (F8) - projected from the registry
// ---------------------------------------------------------------------------

inline std::vector<std::string>& MockCustomPedals()
{
  static std::vector<std::string> v;
  v.clear();
  for (const auto& p : Store().reg().pedals)
    v.push_back(p.name);
  return v;
}

inline std::vector<std::string>& MockPedalFiles()
{
  static std::vector<std::string> v;
  v.clear();
  for (const auto& p : Store().reg().pedals)
    v.push_back(p.file);
  return v;
}

inline std::string PedalFileAt(int idx)
{
  const auto& peds = Store().reg().pedals;
  return (idx >= 0 && idx < (int)peds.size()) ? peds[(size_t)idx].file : std::string();
}

// Stable PRE-capture index assigned to an imported pedal (0 when out of range).
inline int PedalLegacyIndexAt(int idx)
{
  const auto& peds = Store().reg().pedals;
  return (idx >= 0 && idx < (int)peds.size()) ? peds[(size_t)idx].legacyIndex : 0;
}

inline std::string PedalIdAt(int idx)
{
  const auto& peds = Store().reg().pedals;
  return (idx >= 0 && idx < (int)peds.size()) ? peds[(size_t)idx].id : std::string();
}

// Resolve an imported pedal by its stable PRE-capture legacy index (the value a
// scene/preset/param stores). Returns "" when no pedal owns that index.
inline std::string PedalNameByLegacy(int legacyIndex)
{
  for (const auto& p : Store().reg().pedals)
    if (p.legacyIndex == legacyIndex)
      return p.name;
  return {};
}

inline std::string PedalStoredPathByLegacy(int legacyIndex)
{
  for (const auto& p : Store().reg().pedals)
    if (p.legacyIndex == legacyIndex)
      return p.file;
  return {};
}

// Appends an imported pedal and returns its list index, or -1 when the finite
// PRE-capture index pool (kCustomPedalIndexBase..kCustomPedalIndexMax) is
// exhausted. We refuse rather than clamp to kCustomPedalIndexMax, which would
// alias the new pedal's captures onto an existing one. Callers must handle -1.
inline int AddPedal(const std::string& name, const std::string& file = "", const std::string& group = "")
{
  auto& reg = Store().reg();
  if (reg.nextPedalIndex > content::kCustomPedalIndexMax)
    return -1;
  content::PedalItem it;
  it.id = content::MintId(reg, "pedal");
  it.name = name.empty() ? "Imported pedal" : name;
  it.group = group;
  it.file = file;
  it.legacyIndex = reg.nextPedalIndex;
  reg.nextPedalIndex = it.legacyIndex + 1;
  reg.pedals.push_back(std::move(it));
  Store().Save();
  return (int)reg.pedals.size() - 1;
}

inline void RenamePedal(int idx, const std::string& name)
{
  auto& peds = Store().reg().pedals;
  if (idx >= 0 && idx < (int)peds.size() && !name.empty())
  {
    peds[(size_t)idx].name = name;
    Store().Save();
  }
}

inline void DeletePedal(int idx)
{
  auto& peds = Store().reg().pedals;
  if (idx >= 0 && idx < (int)peds.size())
  {
    Store().RemovePedal(peds[(size_t)idx].id);
    Store().Save();
  }
}

// ---------------------------------------------------------------------------
// Per-amp named presets (F5) - projected from the registry's preset banks
// ---------------------------------------------------------------------------
//
// A preset bank is keyed by the *owning amp* (factory:<idx> or a custom amp id),
// not by the factory index the index-based UI passes around. Since every preset
// surface (header bar + Manage panel) only ever acts on the currently focused
// amp, the plugin publishes that amp's owner key here on each switch; the bridge
// reads it instead of deriving a factory key from the ampIdx argument (which is
// always the underlying factory slot, even while a custom amp is focused).
inline std::string& ActivePresetOwnerKey()
{
  static std::string key = content::FactoryOwnerKey(0);
  return key;
}
inline void SetActivePresetOwner(const std::string& key)
{
  ActivePresetOwnerKey() = key;
}

// The plugin installs these so registry preset ops capture/apply the *real* live
// VoLumAmpSettings (the bridge has no access to live params). Unset in unit tests
// (presets then carry default settings, which the round-trip tests still cover).
using PresetSettingsCapture = std::function<VoLumAmpSettings()>;
using PresetSettingsApply = std::function<void(const VoLumAmpSettings&)>;
inline PresetSettingsCapture& PresetCaptureHook()
{
  static PresetSettingsCapture h;
  return h;
}
inline PresetSettingsApply& PresetApplyHook()
{
  static PresetSettingsApply h;
  return h;
}

inline std::vector<std::string> MockPresetsForAmp(int /*ampIdx*/)
{
  const auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  std::vector<std::string> names;
  if (it != banks.end())
    for (const auto& pr : it->second)
      names.push_back(pr.name);
  return names;
}

// Capture the current live settings (via the plugin hook) into a new named
// preset, de-duplicating the display name. Returns its index in the bank.
inline int AddPreset(int /*ampIdx*/, const std::string& name)
{
  auto& reg = Store().reg();
  auto& bank = reg.presetBanks[ActivePresetOwnerKey()];
  const std::string fallback = name.empty() ? "Preset" : name;
  std::string unique = fallback;
  int suffix = 2;
  auto clashes = [&](const std::string& n) {
    for (const auto& pr : bank)
      if (pr.name == n)
        return true;
    return false;
  };
  while (clashes(unique))
    unique = fallback + " " + std::to_string(suffix++);
  content::Preset pr;
  pr.id = content::MintId(reg, "preset");
  pr.name = unique;
  if (PresetCaptureHook())
    pr.settings = PresetCaptureHook()();
  bank.push_back(std::move(pr));
  Store().Save();
  return (int)bank.size() - 1;
}

// Overwrite an existing preset's snapshot with the current live settings.
inline void OverwritePreset(int /*ampIdx*/, int idx)
{
  auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  if (it == banks.end() || idx < 0 || idx >= (int)it->second.size())
    return;
  if (PresetCaptureHook())
    it->second[(size_t)idx].settings = PresetCaptureHook()();
  Store().Save();
}

// Recall a preset: apply its stored snapshot to the live chain (via the plugin
// hook). No-op (other than selection) in unit tests where no hook is installed.
inline void RecallPreset(int /*ampIdx*/, int idx)
{
  auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  if (it == banks.end() || idx < 0 || idx >= (int)it->second.size())
    return;
  if (PresetApplyHook())
    PresetApplyHook()(it->second[(size_t)idx].settings);
}

inline std::string PresetIdAt(int idx)
{
  const auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  if (it == banks.end() || idx < 0 || idx >= (int)it->second.size())
    return {};
  return it->second[(size_t)idx].id;
}

inline void RenamePreset(int /*ampIdx*/, int idx, const std::string& name)
{
  auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  if (it == banks.end())
    return;
  auto& bank = it->second;
  if (idx >= 0 && idx < (int)bank.size() && !name.empty())
  {
    bank[(size_t)idx].name = name;
    Store().Save();
  }
}

inline void DeletePreset(int /*ampIdx*/, int idx)
{
  auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  if (it == banks.end())
    return;
  auto& bank = it->second;
  if (idx >= 0 && idx < (int)bank.size())
  {
    bank.erase(bank.begin() + idx);
    if (bank.empty())
      banks.erase(it);
    Store().Save();
  }
}

// ---------------------------------------------------------------------------
// Name-uniqueness helpers (delegate to the pure NameExistsCI over projections)
// ---------------------------------------------------------------------------

inline bool IRNameExists(const std::string& name, int exceptIdx = -1)
{
  return NameExistsCI(MockIRLibrary(), name, exceptIdx);
}

inline bool PedalNameExists(const std::string& name, int exceptIdx = -1)
{
  return NameExistsCI(MockCustomPedals(), name, exceptIdx);
}

inline bool PresetNameExists(int ampIdx, const std::string& name, int exceptIdx = -1)
{
  return NameExistsCI(MockPresetsForAmp(ampIdx), name, exceptIdx);
}

} // namespace custom
} // namespace volum
