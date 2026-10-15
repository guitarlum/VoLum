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

inline constexpr size_t kFactoryPresetNameMaxBytes = 48;

struct FactoryPreset
{
  std::string id;
  int ampIdx = -1;
  int version = 0; // the N in id "factory:<amp>:v<N>", >= 1
  VoLumAmpSettings settings;
  std::string name;
};

// Only a malformed factory-presets.json entry reaches this; the shipped file names
// every preset.
inline std::string FactoryPresetFallbackName(int version)
{
  return "Factory " + std::to_string(version);
}

// Trimmed, capped on a UTF-8 boundary; empty falls back to "Factory <version>".
inline std::string FactoryPresetNameFromJson(const std::string& raw, int version = 1)
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
  return name.empty() ? FactoryPresetFallbackName(version) : name;
}

inline std::string FactoryPresetId(int ampIdx, int version = 1)
{
  return "factory:" + std::to_string(ampIdx) + ":v" + std::to_string(version);
}

// Accepts only the canonical form FactoryPresetId writes: a factory amp and a
// version >= 1, no sign, no leading zeros, nothing trailing.
inline bool ParseFactoryPresetId(const std::string& id, int& ampIdx, int& version)
{
  static constexpr const char* kPrefix = "factory:";
  if (id.rfind(kPrefix, 0) != 0)
    return false;
  const size_t sep = id.find(":v", 8);
  if (sep == std::string::npos)
    return false;
  const std::string amp = id.substr(8, sep - 8);
  const std::string ver = id.substr(sep + 2);
  const auto digits = [](const std::string& s) {
    return !s.empty() && s.size() <= 4 && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
  };
  if (!digits(amp) || !digits(ver))
    return false;
  const int a = std::stoi(amp);
  const int v = std::stoi(ver);
  if (a < 0 || a >= kAmpCount || v < 1 || FactoryPresetId(a, v) != id)
    return false;
  ampIdx = a;
  version = v;
  return true;
}

inline int FactoryPresetAmpIndex(const std::string& id)
{
  int amp = -1, version = 0;
  return ParseFactoryPresetId(id, amp, version) ? amp : -1;
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

// factory-presets.json is an object keyed by preset id ("factory:<amp>:v<N>"),
// each entry {"name", "settings"}. The bank comes back sorted by amp, then version,
// so an amp's presets keep the order the file numbers them in. Entries with a
// non-canonical id or without a settings object are skipped; an unreadable file
// yields an empty bank. A later release can revoice a snapshot without changing
// its id, which is what PLAY switches and MIDI maps store.
inline std::vector<FactoryPreset> LoadFactoryPresets(const std::filesystem::path& path)
{
  std::vector<FactoryPreset> out;
  std::ifstream in(path);
  if (!in.good())
    return out;

  try
  {
    nlohmann::json root;
    in >> root;
    if (!root.is_object())
      return {};
    for (auto it = root.begin(); it != root.end(); ++it)
    {
      FactoryPreset preset;
      if (!ParseFactoryPresetId(it.key(), preset.ampIdx, preset.version) || !it->is_object())
        continue;
      const auto settingsIt = it->find("settings");
      if (settingsIt == it->end() || !settingsIt->is_object())
        continue;
      preset.id = it.key();
      const auto nameIt = it->find("name");
      preset.name = FactoryPresetNameFromJson(
        nameIt != it->end() && nameIt->is_string() ? nameIt->get<std::string>() : std::string(), preset.version);
      AmpSettingsFromJson(*settingsIt, preset.settings);
      out.push_back(std::move(preset));
    }
  }
  catch (...)
  {
    return {};
  }
  std::sort(out.begin(), out.end(), [](const FactoryPreset& a, const FactoryPreset& b) {
    return a.ampIdx != b.ampIdx ? a.ampIdx < b.ampIdx : a.version < b.version;
  });
  return out;
}

// A missing or unreadable bank still gives every factory amp its v1 id at the
// shipped defaults, so PLAY switches and MIDI maps that store one keep resolving.
inline std::vector<FactoryPreset> FactoryPresetsOrFallback(std::vector<FactoryPreset> bank)
{
  if (!bank.empty())
    return bank;
  for (int amp = 0; amp < kAmpCount; ++amp)
    bank.push_back({FactoryPresetId(amp, 1), amp, 1, VoLumAmpSettings{}, FactoryPresetFallbackName(1)});
  return bank;
}

// One amp's Factory presets, in file order (v1 first). Empty for an amp the file
// gives none.
inline std::vector<const FactoryPreset*> FactoryPresetsForAmp(const std::vector<FactoryPreset>& presets, int ampIdx)
{
  std::vector<const FactoryPreset*> out;
  for (const auto& preset : presets)
    if (preset.ampIdx == ampIdx)
      out.push_back(&preset);
  return out;
}

inline const FactoryPreset* FindFactoryPresetById(const std::vector<FactoryPreset>& presets, const std::string& id)
{
  for (const auto& preset : presets)
    if (preset.id == id)
      return &preset;
  return nullptr;
}

// The preset bar, its < > arrows and the BUILD preset menu index one list: the
// focused amp's Factory presets first, then its User bank.
struct PresetRow
{
  const FactoryPreset* factory = nullptr;
  int userIdx = -1;
};

inline PresetRow PresetRowAt(const std::vector<const FactoryPreset*>& factory, int userCount, int row)
{
  const int factoryCount = static_cast<int>(factory.size());
  if (row >= 0 && row < factoryCount)
    return {factory[static_cast<size_t>(row)], -1};
  if (row >= factoryCount && row - factoryCount < userCount)
    return {nullptr, row - factoryCount};
  return {};
}

inline int FactoryPresetRow(const std::vector<const FactoryPreset*>& factory, const std::string& id)
{
  for (size_t i = 0; i < factory.size(); ++i)
    if (factory[i]->id == id)
      return static_cast<int>(i);
  return -1;
}

inline std::vector<std::string> PresetRowNames(const std::vector<const FactoryPreset*>& factory,
                                               const std::vector<std::string>& users)
{
  std::vector<std::string> names;
  names.reserve(factory.size() + users.size());
  for (const auto* preset : factory)
    names.push_back(preset->name);
  names.insert(names.end(), users.begin(), users.end());
  return names;
}

} // namespace volum
