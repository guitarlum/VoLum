#pragma once

// The Factory bank the plugin ships, read from the repo's rigs/ tree the way the
// plugin reads it from the installed one.

#include "../VoLumFactoryPresets.h"

#include <filesystem>
#include <vector>

namespace volum_test
{
inline std::filesystem::path ShippedFactoryPresetsPath()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "rigs" / "factory-presets.json";
}

inline std::vector<volum::FactoryPreset> ShippedFactoryPresets()
{
  return volum::LoadFactoryPresets(ShippedFactoryPresetsPath());
}

inline volum::FactoryPreset* FindMutableFactoryPreset(std::vector<volum::FactoryPreset>& bank, const std::string& id)
{
  for (auto& preset : bank)
    if (preset.id == id)
      return &preset;
  return nullptr;
}
} // namespace volum_test
