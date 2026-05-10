#pragma once

#include <algorithm>
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

// 0.9.3: reverb Mix law switched from additive (`dry + wet*mix`) to equal-power
// crossfade. Hall and Plate also gained a wet-bus trim (kReverbWetTrim = 1.55).
// To preserve perceived wet level on existing presets, remap stored ReverbMix so the
// new wet coefficient matches the old wet contribution at the same playing scene.
//
// Math (oldWet = newWet at the wet bus):
//   Hall / Plate    : oldMix         = sin(newMix * pi/2)         * 1.55
//   Oktaverb        : oldMix * 0.5 = sin(newMix * 0.5  * pi/2)
//
// In each case the right-hand side has Oktaverb's wet baked-in sm.wetGain (1.40 / 1.55)
// already, and the left-hand side preserves the old per-sub-mode cap (mMix * cap) that
// existed pre-equal-power, so the sub-mode-specific trims cancel in the ratio.
inline double RemapReverbMixToEqualPowerV0_9_3(double oldMix, int reverbMode, int reverbSubMode)
{
  if (!std::isfinite(oldMix))
    return oldMix;
  oldMix = std::max(0.0, std::min(1.0, oldMix));
  constexpr double kPi = 3.14159265358979323846;
  constexpr double kHalfPi = kPi * 0.5;
  constexpr double kHallPlateWetTrim = 1.55;

  double cap = 1.0;
  double wetTrim = 1.0;
  if (reverbMode == 2 /* Oktaverb */)
  {
    cap = 0.5;
    wetTrim = 1.0; // Oktaverb wet already bakes sm.wetGain in.
  }
  else
  {
    cap = 1.0;
    wetTrim = kHallPlateWetTrim;
  }

  const double oldWet = oldMix * cap;
  const double targetSin = oldWet / wetTrim;
  if (!std::isfinite(targetSin) || targetSin <= 0.0)
    return 0.0;
  const double sinClamped = std::min(1.0, targetSin);
  const double angle = std::asin(sinClamped);
  const double denom = cap * kHalfPi;
  if (denom <= 0.0)
    return oldMix;
  double newMix = angle / denom;
  return std::max(0.0, std::min(1.0, newMix));
}

inline void MigrateReverbMixToEqualPowerV0_9_3(nlohmann::json& config)
{
  if (!config.contains("ReverbMix") || !config["ReverbMix"].is_number())
    return;

  int reverbMode = 0;
  if (config.contains("ReverbMode") && config["ReverbMode"].is_number())
    reverbMode = static_cast<int>(std::round(config["ReverbMode"].get<double>()));

  int reverbSubMode = 1;
  if (config.contains("ReverbSubMode") && config["ReverbSubMode"].is_number())
    reverbSubMode = static_cast<int>(std::round(config["ReverbSubMode"].get<double>()));

  const double oldMix = config["ReverbMix"].get<double>();
  const double newMix = RemapReverbMixToEqualPowerV0_9_3(oldMix, reverbMode, reverbSubMode);
  config["ReverbMix"] = newMix;
}

} // namespace volum
