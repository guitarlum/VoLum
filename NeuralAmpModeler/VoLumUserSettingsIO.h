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

struct DelayModeSnapshot {
  double time = 380.0;
  double feedback = 0.35;
  double mix = 0.28;
};

struct ReverbModeSnapshot {
  double mix = 0.3;
  double decay = 3.0;
  double tone = 4.5;
  double preDelay = 20.0;
  double shimmer = 0.5;
};

struct VoLumEffectSettings {
  bool reverbActive = false;
  int reverbMode = 0;
  ReverbModeSnapshot reverbModes[3];

  bool delayActive = false;
  int delayMode = 1;
  DelayModeSnapshot delayModes[3];
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
    a["preCompActive"] = s.preCompActive;
    a["preCompAmount"] = s.preCompAmount;
    a["preCompRatio"] = s.preCompRatio;
    a["preCompAttack"] = s.preCompAttack;
    a["preCompRelease"] = s.preCompRelease;
    a["preCompMix"] = s.preCompMix;
    a["preCompLevel"] = s.preCompLevel;
    a["preNam1Active"] = s.preNam1Active;
    a["preNam1Capture"] = s.preNam1Capture;
    a["preNam1Gain"] = s.preNam1Gain;
    a["preNam1Bass"] = s.preNam1Bass;
    a["preNam1Mid"] = s.preNam1Mid;
    a["preNam1MidFreq"] = s.preNam1MidFreq;
    a["preNam1Treble"] = s.preNam1Treble;
    a["preNam1Level"] = s.preNam1Level;
    a["preNam2Active"] = s.preNam2Active;
    a["preNam2Capture"] = s.preNam2Capture;
    a["preNam2Gain"] = s.preNam2Gain;
    a["preNam2Bass"] = s.preNam2Bass;
    a["preNam2Mid"] = s.preNam2Mid;
    a["preNam2MidFreq"] = s.preNam2MidFreq;
    a["preNam2Treble"] = s.preNam2Treble;
    a["preNam2Level"] = s.preNam2Level;
    amps[kAmps[i].folderName] = a;
  }
  j["amps"] = amps;

  if (fx) {
    nlohmann::json e;
    e["delayActive"] = fx->delayActive;
    e["delayMode"] = fx->delayMode;
    e["reverbActive"] = fx->reverbActive;
    e["reverbMode"] = fx->reverbMode;
    e["delayModes"] = nlohmann::json::array();
    for (const auto& mode : fx->delayModes)
    {
      e["delayModes"].push_back({
        {"time", mode.time},
        {"feedback", mode.feedback},
        {"mix", mode.mix},
      });
    }
    e["reverbModes"] = nlohmann::json::array();
    for (const auto& mode : fx->reverbModes)
    {
      e["reverbModes"].push_back({
        {"mix", mode.mix},
        {"decay", mode.decay},
        {"tone", mode.tone},
        {"preDelay", mode.preDelay},
        {"shimmer", mode.shimmer},
      });
    }
    j["effects"] = e;
  }

  return j;
}

inline void VolumUserSettingsFromJson(const nlohmann::json& j, VoLumAmpSettings* ampSettings, int ampCount,
                                      int* lastAmpIdx, VoLumEffectSettings* fx = nullptr, bool* didHeal = nullptr)
{
  bool healed = false;
  auto sanitizeInt = [&](int value, int minValue, int maxValue) {
    const int clamped = std::clamp(value, minValue, maxValue);
    if (clamped != value)
      healed = true;
    return clamped;
  };

  if (lastAmpIdx && j.contains("lastAmpIdx"))
    *lastAmpIdx = sanitizeInt(j["lastAmpIdx"].get<int>(), 0, ampCount - 1);

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
        s.speakerIdx = sanitizeInt(a["speaker"].get<int>(), 0, 3);
      if (a.contains("channel"))
        s.channelIdx = sanitizeInt(a["channel"].get<int>(), 0, 127);
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
      if (a.contains("preCompActive"))
        s.preCompActive = a["preCompActive"].get<bool>();
      if (a.contains("preCompAmount"))
        s.preCompAmount = a["preCompAmount"].get<double>();
      if (a.contains("preCompRatio"))
        s.preCompRatio = a["preCompRatio"].get<double>();
      if (a.contains("preCompAttack"))
        s.preCompAttack = a["preCompAttack"].get<double>();
      if (a.contains("preCompRelease"))
        s.preCompRelease = a["preCompRelease"].get<double>();
      if (a.contains("preCompMix"))
        s.preCompMix = a["preCompMix"].get<double>();
      if (a.contains("preCompLevel"))
        s.preCompLevel = a["preCompLevel"].get<double>();
      if (a.contains("preNam1Active"))
        s.preNam1Active = a["preNam1Active"].get<bool>();
      if (a.contains("preNam1Capture"))
        s.preNam1Capture = a["preNam1Capture"].get<int>();
      if (a.contains("preNam1Gain"))
        s.preNam1Gain = a["preNam1Gain"].get<double>();
      if (a.contains("preNam1Bass"))
        s.preNam1Bass = a["preNam1Bass"].get<double>();
      if (a.contains("preNam1Mid"))
        s.preNam1Mid = a["preNam1Mid"].get<double>();
      if (a.contains("preNam1MidFreq"))
        s.preNam1MidFreq = a["preNam1MidFreq"].get<double>();
      if (a.contains("preNam1Treble"))
        s.preNam1Treble = a["preNam1Treble"].get<double>();
      if (a.contains("preNam1Level"))
        s.preNam1Level = a["preNam1Level"].get<double>();
      if (a.contains("preNam2Active"))
        s.preNam2Active = a["preNam2Active"].get<bool>();
      if (a.contains("preNam2Capture"))
        s.preNam2Capture = a["preNam2Capture"].get<int>();
      if (a.contains("preNam2Gain"))
        s.preNam2Gain = a["preNam2Gain"].get<double>();
      if (a.contains("preNam2Bass"))
        s.preNam2Bass = a["preNam2Bass"].get<double>();
      if (a.contains("preNam2Mid"))
        s.preNam2Mid = a["preNam2Mid"].get<double>();
      if (a.contains("preNam2MidFreq"))
        s.preNam2MidFreq = a["preNam2MidFreq"].get<double>();
      if (a.contains("preNam2Treble"))
        s.preNam2Treble = a["preNam2Treble"].get<double>();
      if (a.contains("preNam2Level"))
        s.preNam2Level = a["preNam2Level"].get<double>();
    }
  }

  if (fx && j.contains("effects") && j["effects"].is_object())
  {
    const auto& e = j["effects"];
    if (e.contains("delayActive")) fx->delayActive = e["delayActive"].get<bool>();
    if (e.contains("delayMode")) fx->delayMode = e["delayMode"].get<int>();
    if (e.contains("reverbActive")) fx->reverbActive = e["reverbActive"].get<bool>();
    if (e.contains("reverbMode")) fx->reverbMode = e["reverbMode"].get<int>();

    if (e.contains("delayModes") && e["delayModes"].is_array())
    {
      const auto& modes = e["delayModes"];
      for (int i = 0; i < 3 && i < static_cast<int>(modes.size()); ++i)
      {
        const auto& mode = modes[i];
        if (mode.contains("time")) fx->delayModes[i].time = mode["time"].get<double>();
        if (mode.contains("feedback")) fx->delayModes[i].feedback = mode["feedback"].get<double>();
        if (mode.contains("mix")) fx->delayModes[i].mix = mode["mix"].get<double>();
      }
    }
    else
    {
      DelayModeSnapshot legacy;
      if (e.contains("delayTime")) legacy.time = e["delayTime"].get<double>();
      if (e.contains("delayFeedback")) legacy.feedback = e["delayFeedback"].get<double>();
      if (e.contains("delayMix")) legacy.mix = e["delayMix"].get<double>();
      for (auto& mode : fx->delayModes)
        mode = legacy;
    }

    if (e.contains("reverbModes") && e["reverbModes"].is_array())
    {
      const auto& modes = e["reverbModes"];
      for (int i = 0; i < 3 && i < static_cast<int>(modes.size()); ++i)
      {
        const auto& mode = modes[i];
        if (mode.contains("mix")) fx->reverbModes[i].mix = mode["mix"].get<double>();
        if (mode.contains("decay")) fx->reverbModes[i].decay = mode["decay"].get<double>();
        if (mode.contains("tone")) fx->reverbModes[i].tone = mode["tone"].get<double>();
        if (mode.contains("preDelay")) fx->reverbModes[i].preDelay = mode["preDelay"].get<double>();
        if (mode.contains("shimmer")) fx->reverbModes[i].shimmer = mode["shimmer"].get<double>();
      }
    }
    else
    {
      ReverbModeSnapshot legacy;
      if (e.contains("reverbMix")) legacy.mix = e["reverbMix"].get<double>();
      if (e.contains("reverbDecay")) legacy.decay = e["reverbDecay"].get<double>();
      if (e.contains("reverbTone")) legacy.tone = e["reverbTone"].get<double>();
      if (e.contains("reverbPreDelay")) legacy.preDelay = e["reverbPreDelay"].get<double>();
      if (e.contains("reverbShimmer")) legacy.shimmer = e["reverbShimmer"].get<double>();
      for (auto& mode : fx->reverbModes)
        mode = legacy;
    }
  }

  if (didHeal)
    *didHeal = healed;
}

} // namespace volum
