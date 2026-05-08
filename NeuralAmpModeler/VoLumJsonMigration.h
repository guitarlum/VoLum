#pragma once

#include <cmath>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
#include <json.hpp>
#else
#error "nlohmann json header not found (expected iPlug Dependencies/Extras layout)"
#endif

namespace volum
{

inline bool RenameJsonKeyIfPresent(nlohmann::json& j, const char* from, const char* to)
{
  auto existing = j.find(from);
  if (existing == j.end())
    return false;

  j[to] = *existing;
  j.erase(existing);
  return true;
}

// v0.9.0/effect-staging migration applied to the unserialized chunk JSON dict for any
// pre-0.9.0 chunk.
// Public so tests can verify the mapping table without going through the IByteChunk path.
//
// Old delay-mode order: 0=Tape, 1=Digital, 2=PingPong, 3=Reverse
// New delay-mode order: 0=Digital, 1=Analog, 2=Reverse
// Mapping:
//   old 0 (Tape)     -> new 0 (Digital; Tape removed from staging)
//   old 1 (Digital)  -> new 0 (Digital)
//   old 2 (PingPong) -> new 0 (Digital) + DelayPingPong = true
//   old 3 (Reverse)  -> new 2 (Reverse)
// Old reverb modes 0/1/2 (Hall/Plate/Oktaverb) keep their indices.
inline void MigrateDelayReverbToV0_9_0(nlohmann::json& config)
{
  bool wasInPingPong = false;
  if (config.contains("DelayMode") && config["DelayMode"].is_number())
  {
    const int oldMode = static_cast<int>(std::round(config["DelayMode"].get<double>()));
    int newMode = oldMode;
    switch (oldMode)
    {
      case 0: newMode = 0; break;
      case 1: newMode = 0; break;
      case 2: newMode = 0; wasInPingPong = true; break;
      case 3: newMode = 2; break;
      default: newMode = 0; break;
    }
    config["DelayMode"] = static_cast<double>(newMode);
  }

  // Add neutral defaults for the new EParams. Pre-0.9.0 chunks couldn't have stored these
  // names, so unconditional overwrite is safe and avoids a probing branch per key.
  config["DelayTone"] = 0.5;
  config["DelayAge"] = 0.0;
  config["DelayPingPong"] = wasInPingPong ? 1.0 : 0.0;
  config["ReverbSubMode"] = 0.0;
}

inline int RemapLegacyOktaverbSubModeToV0_9_1(int oldSubMode)
{
  switch (oldSubMode)
  {
    case 2: return 0; // old Oct+Sub -> slot 0 (now Halo, was Dark in 0.9.1)
    case 0:
    case 1:
    default: return 1; // old Oct / Oct+5th -> new Shimmer
  }
}

inline void MigrateOktaverbSubModeToV0_9_1(nlohmann::json& config)
{
  if (!config.contains("ReverbSubMode") || !config["ReverbSubMode"].is_number())
  {
    config["ReverbSubMode"] = 1.0;
    return;
  }

  const int oldSubMode = static_cast<int>(std::round(config["ReverbSubMode"].get<double>()));
  config["ReverbSubMode"] = static_cast<double>(RemapLegacyOktaverbSubModeToV0_9_1(oldSubMode));
}

} // namespace volum
