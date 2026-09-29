#pragma once

#include "VoLumSettingsFileIO.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace volum::update
{

struct UpdateState
{
  std::int64_t lastCheckUtc = 0;
  std::string lastSeenVersion;
  std::string latestKnownVersion;
  std::string latestKnownUrl;
  std::string latestKnownNotes;
  bool autoCheck = true;
};

inline UpdateState LoadUpdateState(const std::filesystem::path& path)
{
  UpdateState state;
  if (path.empty())
    return state;

  try
  {
    std::ifstream in(path, std::ios::binary);
    if (!in)
      return state;
    const auto root = nlohmann::json::parse(in, nullptr, false);
    if (!root.is_object())
      return state;

    auto readString = [&root](const char* key, std::string& value) {
      const auto it = root.find(key);
      if (it != root.end() && it->is_string())
        value = it->get<std::string>();
    };
    const auto lastCheck = root.find("lastCheckUtc");
    if (lastCheck != root.end() && lastCheck->is_number_integer())
      state.lastCheckUtc = lastCheck->get<std::int64_t>();
    const auto autoCheck = root.find("autoCheck");
    if (autoCheck != root.end() && autoCheck->is_boolean())
      state.autoCheck = autoCheck->get<bool>();
    readString("lastSeenVersion", state.lastSeenVersion);
    readString("latestKnownVersion", state.latestKnownVersion);
    readString("latestKnownUrl", state.latestKnownUrl);
    readString("latestKnownNotes", state.latestKnownNotes);
  }
  catch (...)
  {
    return {};
  }
  return state;
}

inline bool SaveUpdateState(const std::filesystem::path& path, const UpdateState& state)
{
  const nlohmann::json root = {
    {"lastCheckUtc", state.lastCheckUtc},
    {"lastSeenVersion", state.lastSeenVersion},
    {"latestKnownVersion", state.latestKnownVersion},
    {"latestKnownUrl", state.latestKnownUrl},
    {"latestKnownNotes", state.latestKnownNotes},
    {"autoCheck", state.autoCheck},
  };
  std::error_code ec;
  return volum::WriteJsonAtomically(path, root, ec);
}

} // namespace volum::update
