#pragma once

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "VoLumUpdateState.h"

#if __has_include(<nlohmann/json.hpp>)
  #include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
  #include <json.hpp>
#else
  #error "nlohmann json header not found"
#endif

namespace volum::update
{

constexpr std::int64_t kCheckIntervalSeconds = 24 * 60 * 60;

struct Version
{
  int major = 0;
  int minor = 0;
  int patch = 0;
};

inline bool ParseVersion(const std::string& text, Version& out)
{
  const char* p = text.c_str();
  if (*p == 'v' || *p == 'V')
    ++p;

  auto component = [&p](int& value) {
    if (!std::isdigit(static_cast<unsigned char>(*p)))
      return false;
    int parsed = 0;
    do
    {
      const int digit = *p++ - '0';
      if (parsed > (std::numeric_limits<int>::max() - digit) / 10)
        return false;
      parsed = parsed * 10 + digit;
    } while (std::isdigit(static_cast<unsigned char>(*p)));
    value = parsed;
    return true;
  };

  Version parsed;
  if (!component(parsed.major) || *p++ != '.' || !component(parsed.minor) || *p++ != '.' || !component(parsed.patch)
      || *p != '\0')
    return false;
  out = parsed;
  return true;
}

inline int CompareVersions(const Version& lhs, const Version& rhs)
{
  if (lhs.major != rhs.major)
    return lhs.major < rhs.major ? -1 : 1;
  if (lhs.minor != rhs.minor)
    return lhs.minor < rhs.minor ? -1 : 1;
  if (lhs.patch != rhs.patch)
    return lhs.patch < rhs.patch ? -1 : 1;
  return 0;
}

inline int CompareVersionStrings(const std::string& lhs, const std::string& rhs)
{
  Version a;
  Version b;
  if (!ParseVersion(lhs, a) || !ParseVersion(rhs, b))
    return 0;
  return CompareVersions(a, b);
}

struct Manifest
{
  std::string version;
  std::string notes;
  std::string url;
};

inline bool FakeUpdateRequested()
{
  const char* v = std::getenv("VOLUM_FAKE_UPDATE");
  return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

inline Manifest FakeUpdateManifest()
{
  return {"2.0.0", "configure your own signal chain freely", "https://github.com/guitarlum/VoLum/releases"};
}

inline bool ParseManifest(const std::string& text, Manifest& out)
{
  const auto root = nlohmann::json::parse(text, nullptr, false);
  if (root.is_discarded() || !root.is_object())
    return false;

  try
  {
    const auto schema = root.find("schema");
    const auto stable = root.find("stable");
    if (schema == root.end() || !schema->is_number_integer() || schema->get<std::int64_t>() != 1 || stable == root.end()
        || !stable->is_object())
      return false;

    const auto version = stable->find("version");
    const auto notes = stable->find("notes");
    const auto url = stable->find("url");
    if (version == stable->end() || !version->is_string() || notes == stable->end() || !notes->is_string()
        || url == stable->end() || !url->is_string())
      return false;

    Manifest parsed{version->get<std::string>(), notes->get<std::string>(), url->get<std::string>()};
    Version ignored;
    if (!ParseVersion(parsed.version, ignored) || parsed.url.rfind("https://", 0) != 0)
      return false;
    out = std::move(parsed);
    return true;
  }
  catch (...)
  {
    return false;
  }
}

inline bool ShouldCheck(std::int64_t nowUtc, std::int64_t lastCheckUtc)
{
  if (lastCheckUtc <= 0)
    return true;
  return nowUtc >= lastCheckUtc && nowUtc - lastCheckUtc >= kCheckIntervalSeconds;
}

struct BadgeState
{
  std::string latestKnownVersion;
  std::string lastSeenVersion;
};

inline bool IsUpdateAvailable(const BadgeState& state, const std::string& currentVersion)
{
  return !state.latestKnownVersion.empty() && CompareVersionStrings(state.latestKnownVersion, currentVersion) > 0;
}

inline bool ShouldShowBadge(const BadgeState& state, const std::string& currentVersion)
{
  return state.latestKnownVersion != state.lastSeenVersion && IsUpdateAvailable(state, currentVersion);
}

inline void OnSettingsOpened(BadgeState&)
{
  // Deliberately does not mark the available release as seen.
}

inline void MarkSeen(BadgeState& state)
{
  state.lastSeenVersion = state.latestKnownVersion;
}

// Merge a successful appcast into the sidecar without touching autoCheck /
// lastSeenVersion. A withdrawn or equal-or-older stable clears the cache so
// a yanked 1.3.0 cannot stay advertised forever.
inline void ApplyCheckedManifest(UpdateState& state, const Manifest& manifest, const std::string& currentVersion,
                                 std::int64_t nowUtc)
{
  state.lastCheckUtc = nowUtc;
  if (CompareVersionStrings(manifest.version, currentVersion) > 0)
  {
    state.latestKnownVersion = manifest.version;
    state.latestKnownUrl = manifest.url;
    state.latestKnownNotes = manifest.notes;
    return;
  }
  state.latestKnownVersion.clear();
  state.latestKnownUrl.clear();
  state.latestKnownNotes.clear();
}

struct AsyncResult
{
  std::atomic<bool> complete{false};
  bool succeeded = false;
  Manifest manifest;
};

} // namespace volum::update
