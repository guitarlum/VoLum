#pragma once

// Shipped, read-only factory named presets. These are deliberately separate
// from the mutable content registry: Pack reset and Manage only own User rows.

#include "VoLumAmpSettingsJson.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace volum
{

inline constexpr const char* kFactoryPresetDisplayName = "Ready";

struct FactoryPreset
{
  std::string id;
  int ampIdx = -1;
  VoLumAmpSettings settings;
};

inline std::string FactoryPresetId(int ampIdx)
{
  return "factory:" + std::to_string(ampIdx) + ":v1";
}

inline int FactoryPresetAmpIndex(const std::string& id)
{
  static constexpr const char* kPrefix = "factory:";
  static constexpr const char* kSuffix = ":v1";
  if (id.rfind(kPrefix, 0) != 0 || id.size() <= 11 || id.substr(id.size() - 3) != kSuffix)
    return -1;
  const std::string number = id.substr(8, id.size() - 11);
  if (number.empty() || !std::all_of(number.begin(), number.end(), [](char c) { return c >= '0' && c <= '9'; }))
    return -1;
  try
  {
    const int idx = std::stoi(number);
    return idx >= 0 && idx < kAmpCount && FactoryPresetId(idx) == id ? idx : -1;
  }
  catch (...)
  {
    return -1;
  }
}

inline bool IsFactoryPresetId(const std::string& id)
{
  return FactoryPresetAmpIndex(id) >= 0;
}

inline bool FactoryPresetCanMutate(const std::string& id)
{
  return !IsFactoryPresetId(id);
}

enum class PresetSaveAction
{
  SaveUserCopy,
  OverwriteUser
};

inline PresetSaveAction SaveActionForActivePreset(const std::string& id)
{
  // Empty id is Default / no named preset — cannot overwrite, must Save As.
  return (id.empty() || IsFactoryPresetId(id)) ? PresetSaveAction::SaveUserCopy : PresetSaveAction::OverwriteUser;
}

inline std::vector<FactoryPreset> DefaultFactoryPresets()
{
  std::vector<FactoryPreset> out;
  out.reserve(kAmpCount);
  for (int i = 0; i < kAmpCount; ++i)
    out.push_back({FactoryPresetId(i), i, VoLumAmpSettings{}});
  return out;
}

// Missing/malformed entries fall back individually to the current shipped
// VoLumAmpSettings defaults, keeping all 15 stable ids available. A later
// release can replace a snapshot in the JSON without changing its identity.
inline std::vector<FactoryPreset> LoadFactoryPresets(const std::filesystem::path& path)
{
  auto out = DefaultFactoryPresets();
  std::ifstream in(path);
  if (!in.good())
    return out;

  try
  {
    nlohmann::json root;
    in >> root;
    if (!root.is_object())
      return out;
    for (auto& preset : out)
    {
      auto it = root.find(preset.id);
      if (it == root.end() || !it->is_object())
        continue;
      const auto settingsIt = it->find("settings");
      if (settingsIt == it->end() || !settingsIt->is_object())
        continue;
      VoLumAmpSettings replacement;
      AmpSettingsFromJson(*settingsIt, replacement);
      preset.settings = std::move(replacement);
    }
  }
  catch (...)
  {
    return DefaultFactoryPresets();
  }
  return out;
}

inline const FactoryPreset* FindFactoryPresetForAmp(const std::vector<FactoryPreset>& presets, int ampIdx)
{
  for (const auto& preset : presets)
    if (preset.ampIdx == ampIdx)
      return &preset;
  return nullptr;
}

inline const FactoryPreset* FindFactoryPresetById(const std::vector<FactoryPreset>& presets, const std::string& id)
{
  for (const auto& preset : presets)
    if (preset.id == id)
      return &preset;
  return nullptr;
}

} // namespace volum
