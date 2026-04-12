#pragma once

#include <algorithm>

#include "VoLumAmpeteCatalog.h"

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
#include <json.hpp>
#else
#error "nlohmann json header not found (expected iPlug Dependencies/Extras layout)"
#endif

#if !VOLUM_AMPETE_PRODUCT
#error VoLumUserSettingsIO is only used when VOLUM_AMPETE_PRODUCT is enabled
#endif

namespace volum
{

inline nlohmann::json VolumUserSettingsToJson(const VoLumAmpSettings* ampSettings, int ampCount, int lastAmpIdx)
{
  nlohmann::json j;
  j["version"] = 1;
  j["lastAmpIdx"] = lastAmpIdx;

  nlohmann::json amps = nlohmann::json::object();
  for (int i = 0; i < ampCount; ++i)
  {
    const auto& s = ampSettings[i];
    nlohmann::json a;
    a["speaker"] = s.speakerIdx;
    a["channel"] = s.channelIdx;
    a["input"] = s.inputLevel;
    a["gate"] = s.gateThreshold;
    a["bass"] = s.toneBass;
    a["mid"] = s.toneMid;
    a["treble"] = s.toneTreble;
    a["output"] = s.outputLevel;
    a["noiseGate"] = s.noiseGateActive;
    a["eq"] = s.eqActive;
    amps[kAmps[i].folderName] = a;
  }
  j["amps"] = amps;
  return j;
}

inline void VolumUserSettingsFromJson(const nlohmann::json& j, VoLumAmpSettings* ampSettings, int ampCount,
                                      int* lastAmpIdx)
{
  if (lastAmpIdx && j.contains("lastAmpIdx"))
    *lastAmpIdx = std::clamp(j["lastAmpIdx"].get<int>(), 0, ampCount - 1);

  if (!j.contains("amps") || !j["amps"].is_object())
    return;

  for (int i = 0; i < ampCount; ++i)
  {
    const char* key = kAmps[i].folderName;
    if (!j["amps"].contains(key))
      continue;

    const auto& a = j["amps"][key];
    auto& s = ampSettings[i];
    if (a.contains("speaker"))
      s.speakerIdx = a["speaker"].get<int>();
    if (a.contains("channel"))
      s.channelIdx = a["channel"].get<int>();
    if (a.contains("input"))
      s.inputLevel = a["input"].get<double>();
    if (a.contains("gate"))
      s.gateThreshold = a["gate"].get<double>();
    if (a.contains("bass"))
      s.toneBass = a["bass"].get<double>();
    if (a.contains("mid"))
      s.toneMid = a["mid"].get<double>();
    if (a.contains("treble"))
      s.toneTreble = a["treble"].get<double>();
    if (a.contains("output"))
      s.outputLevel = a["output"].get<double>();
    if (a.contains("noiseGate"))
      s.noiseGateActive = a["noiseGate"].get<bool>();
    if (a.contains("eq"))
      s.eqActive = a["eq"].get<bool>();
  }
}

} // namespace volum
