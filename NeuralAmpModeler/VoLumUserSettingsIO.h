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

struct VoLumEffectSettings {
  bool delayActive = false;
  double delayTime = 380.0;
  double delayFeedback = 0.35;
  double delayMix = 0.28;
  int delayMode = 1;
  bool reverbActive = false;
  double reverbMix = 0.5;
  double reverbDecay = 3.0;
  double reverbTone = 6.0;
  int reverbMode = 0;
};

inline nlohmann::json VolumUserSettingsToJson(const VoLumAmpSettings* ampSettings, int ampCount, int lastAmpIdx,
                                               const VoLumEffectSettings* fx = nullptr)
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

  if (fx) {
    nlohmann::json e;
    e["delayActive"] = fx->delayActive;
    e["delayTime"] = fx->delayTime;
    e["delayFeedback"] = fx->delayFeedback;
    e["delayMix"] = fx->delayMix;
    e["delayMode"] = fx->delayMode;
    e["reverbActive"] = fx->reverbActive;
    e["reverbMix"] = fx->reverbMix;
    e["reverbDecay"] = fx->reverbDecay;
    e["reverbTone"] = fx->reverbTone;
    e["reverbMode"] = fx->reverbMode;
    j["effects"] = e;
  }

  return j;
}

inline void VolumUserSettingsFromJson(const nlohmann::json& j, VoLumAmpSettings* ampSettings, int ampCount,
                                      int* lastAmpIdx, VoLumEffectSettings* fx = nullptr)
{
  if (lastAmpIdx && j.contains("lastAmpIdx"))
    *lastAmpIdx = std::clamp(j["lastAmpIdx"].get<int>(), 0, ampCount - 1);

  if (j.contains("amps") && j["amps"].is_object())
  {
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

  if (fx && j.contains("effects") && j["effects"].is_object())
  {
    const auto& e = j["effects"];
    if (e.contains("delayActive")) fx->delayActive = e["delayActive"].get<bool>();
    if (e.contains("delayTime")) fx->delayTime = e["delayTime"].get<double>();
    if (e.contains("delayFeedback")) fx->delayFeedback = e["delayFeedback"].get<double>();
    if (e.contains("delayMix")) fx->delayMix = e["delayMix"].get<double>();
    if (e.contains("delayMode")) fx->delayMode = e["delayMode"].get<int>();
    if (e.contains("reverbActive")) fx->reverbActive = e["reverbActive"].get<bool>();
    if (e.contains("reverbMix")) fx->reverbMix = e["reverbMix"].get<double>();
    if (e.contains("reverbDecay")) fx->reverbDecay = e["reverbDecay"].get<double>();
    if (e.contains("reverbTone")) fx->reverbTone = e["reverbTone"].get<double>();
    if (e.contains("reverbMode")) fx->reverbMode = e["reverbMode"].get<int>();
  }
}

} // namespace volum
