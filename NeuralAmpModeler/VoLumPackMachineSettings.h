#pragma once

// VoLum: the machine-settings half of an Everything Pack (standalone only).
//
// The Pack's settings.json is volum-settings.json as the sender had it. Dual Amp
// state lives in a sidecar (volum-dual-amp-settings.json) so that older builds
// never see dual fields in the shared file, so the main file alone carries no Dual
// partner. The Pack carries the sidecar inside settings.json under one additive
// key; an older build ignores it and keeps the receiver's own sidecar.
//
// Restoring onto a running standalone swaps every per-amp scene and the focused
// amp under live params that still describe the outgoing rig. Any live -> scene
// snapshot in that window (a PLAY refresh's dirty check, a param echo) folds the
// outgoing sound into the restored amp's scene, and the next save persists it.
// LiveSceneGate is held from the file read until the restored scene is live.

#include <string>

#if __has_include(<nlohmann/json.hpp>)
  #include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
  #include <json.hpp>
#else
  #error "nlohmann json header not found (expected iPlug Dependencies/Extras layout)"
#endif

namespace volum
{

// UI thread only, like the scenes it guards.
class LiveSceneGate
{
public:
  bool Allows() const { return mHolds == 0; }

  class Hold
  {
  public:
    explicit Hold(LiveSceneGate& gate)
    : mGate(gate)
    {
      ++mGate.mHolds;
    }
    ~Hold() { --mGate.mHolds; }
    Hold(const Hold&) = delete;
    Hold& operator=(const Hold&) = delete;

  private:
    LiveSceneGate& mGate;
  };

private:
  int mHolds = 0;
};

namespace pack
{

inline constexpr const char* kPackDualAmpSettingsKey = "volumDualAmpSettings";

// Export: fold the dual-amp sidecar document into the settings document. An
// unreadable settings document is returned untouched rather than dropped.
inline std::string SettingsWithDualAmp(const std::string& settingsJson, const nlohmann::json& dualAmpSidecar)
{
  nlohmann::json j = nlohmann::json::parse(settingsJson, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object() || !dualAmpSidecar.is_object())
    return settingsJson;
  j[kPackDualAmpSettingsKey] = dualAmpSidecar;
  try
  {
    return j.dump(2);
  }
  catch (const std::exception&)
  {
    return settingsJson;
  }
}

// Import: the sidecar document a Pack's settings carry, or false for a Pack from a
// build that did not carry one (the receiver's own sidecar then stays).
inline bool DualAmpSidecarFromSettings(const std::string& settingsJson, nlohmann::json& sidecar)
{
  const nlohmann::json j = nlohmann::json::parse(settingsJson, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object())
    return false;
  const auto it = j.find(kPackDualAmpSettingsKey);
  if (it == j.end() || !it->is_object() || !it->contains("amps") || !(*it)["amps"].is_object())
    return false;
  sidecar = *it;
  return true;
}

} // namespace pack
} // namespace volum
