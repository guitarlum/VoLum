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

// Shown when factory-presets.json gives an amp no name of its own.
inline constexpr const char* kFactoryPresetDisplayName = "Ready";
inline constexpr size_t kFactoryPresetNameMaxBytes = 48;

struct FactoryPreset
{
  std::string id;
  int ampIdx = -1;
  VoLumAmpSettings settings;
  std::string name = kFactoryPresetDisplayName;
};

// Trimmed, capped on a UTF-8 boundary; empty keeps the fallback name.
inline std::string FactoryPresetNameFromJson(const std::string& raw)
{
  size_t b = 0, e = raw.size();
  while (b < e && static_cast<unsigned char>(raw[b]) <= ' ')
    ++b;
  while (e > b && static_cast<unsigned char>(raw[e - 1]) <= ' ')
    --e;
  std::string name = raw.substr(b, e - b);
  if (name.size() > kFactoryPresetNameMaxBytes)
  {
    size_t cut = kFactoryPresetNameMaxBytes;
    while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80)
      --cut;
    name.resize(cut);
  }
  return name.empty() ? std::string(kFactoryPresetDisplayName) : name;
}

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

inline constexpr const char* kSaveDialogNewPresetSeed = "New Preset";

inline std::string SaveDialogSeedName(PresetSaveAction action, const std::string& currentUserName)
{
  if (action == PresetSaveAction::OverwriteUser && !currentUserName.empty())
    return currentUserName;
  return kSaveDialogNewPresetSeed;
}

inline bool SaveDialogOverwritesCurrent(const std::string& typedName, const std::string& currentUserName)
{
  return !currentUserName.empty() && typedName == currentUserName;
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
      if (const auto nameIt = it->find("name"); nameIt != it->end() && nameIt->is_string())
        preset.name = FactoryPresetNameFromJson(nameIt->get<std::string>());
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
