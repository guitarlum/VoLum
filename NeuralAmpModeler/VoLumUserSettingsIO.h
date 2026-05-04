#pragma once

#include <algorithm>
#include <cmath>

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

inline constexpr int kVoLumDelayModeCount = 4;
inline constexpr int kVoLumReverbModeCount = 3;
inline constexpr int kVoLumReverseDelayMode = 3;
inline constexpr int kVoLumUserSettingsVersion = 2;

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
  ReverbModeSnapshot reverbModes[kVoLumReverbModeCount];

  bool delayActive = false;
  int delayMode = 1;
  DelayModeSnapshot delayModes[kVoLumDelayModeCount] = {
    DelayModeSnapshot{},
    DelayModeSnapshot{},
    DelayModeSnapshot{},
    DelayModeSnapshot{600.0, 0.35, 0.28},
  };
};

inline void WriteDualAmpUserSettings(nlohmann::json& a, const VoLumAmpSettings& s)
{
  a["dualAmpActive"] = s.dualAmpActive;
  a["dualAmpRoute"] = s.dualAmpRoute;
  a["mainAmpPan"] = s.mainAmpPan;
  a["supportAmp"] = s.supportAmpIdx;
  a["supportSpeaker"] = s.supportSpeakerIdx;
  a["supportChannel"] = s.supportChannelIdx;
  a["supportInput"] = s.supportInputLevel;
  a["supportGate"] = s.supportGateThreshold;
  a["supportBass"] = s.supportToneBass;
  a["supportMid"] = s.supportToneMid;
  a["supportTreble"] = s.supportToneTreble;
  a["supportOutput"] = s.supportOutputLevel;
  a["supportNoiseGate"] = s.supportNoiseGateActive;
  a["supportEq"] = s.supportEqActive;
  a["supportPan"] = s.supportAmpPan;
  a["supportPolarityInvert"] = s.supportPolarityInvert;
}

inline bool HasDualAmpUserSettings(const nlohmann::json& j)
{
  if (!j.contains("amps") || !j["amps"].is_object())
    return false;

  static constexpr const char* kDualKeys[] = {
    "dualAmpActive", "dualAmpRoute", "mainAmpPan", "supportAmp", "supportSpeaker", "supportChannel",
    "supportInput", "supportGate", "supportBass", "supportMid", "supportTreble", "supportOutput",
    "supportNoiseGate", "supportEq", "supportPan", "supportPolarityInvert",
  };

  for (const auto& item : j["amps"].items())
  {
    if (!item.value().is_object())
      continue;
    for (const char* key : kDualKeys)
      if (item.value().contains(key))
        return true;
  }
  return false;
}

inline nlohmann::json VolumDualAmpUserSettingsToJson(const VoLumAmpSettings* ampSettings, int ampCount)
{
  nlohmann::json j;
  j["version"] = kVoLumUserSettingsVersion;

  nlohmann::json amps = nlohmann::json::object();
  for (int i = 0; i < ampCount; ++i)
  {
    nlohmann::json a;
    WriteDualAmpUserSettings(a, ampSettings[i]);
    amps[kAmps[i].folderName] = a;
  }
  j["amps"] = amps;
  return j;
}

inline nlohmann::json VolumUserSettingsToJson(const VoLumAmpSettings* ampSettings, int ampCount, int lastAmpIdx,
                                               const VoLumEffectSettings* fx = nullptr, bool includeDualAmp = true)
{
  nlohmann::json j;
  j["version"] = kVoLumUserSettingsVersion;
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
    if (includeDualAmp)
      WriteDualAmpUserSettings(a, s);
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
  auto loadInt = [&](const nlohmann::json& obj, const char* key, int& target, int minValue, int maxValue, int defaultValue) {
    if (!obj.contains(key))
      return;
    const auto& field = obj[key];
    if (!field.is_number_integer())
    {
      target = defaultValue;
      healed = true;
      return;
    }
    const auto value = field.get<long long>();
    if (value < minValue || value > maxValue)
    {
      target = defaultValue;
      healed = true;
      return;
    }
    target = static_cast<int>(value);
  };
  auto loadDouble = [&](const nlohmann::json& obj, const char* key, double& target, double minValue, double maxValue,
                        double defaultValue) {
    if (!obj.contains(key))
      return;
    const auto& field = obj[key];
    if (!field.is_number())
    {
      target = defaultValue;
      healed = true;
      return;
    }
    const double value = field.get<double>();
    if (!std::isfinite(value) || value < minValue || value > maxValue)
    {
      target = defaultValue;
      healed = true;
      return;
    }
    target = value;
  };
  auto loadBool = [&](const nlohmann::json& obj, const char* key, bool& target, bool defaultValue) {
    if (!obj.contains(key))
      return;
    const auto& field = obj[key];
    if (!field.is_boolean())
    {
      target = defaultValue;
      healed = true;
      return;
    }
    target = field.get<bool>();
  };

  int settingsVersion = 1;
  if (j.contains("version"))
    loadInt(j, "version", settingsVersion, 1, kVoLumUserSettingsVersion, 1);
  const bool resetLegacyPreCaptureSelections = settingsVersion < kVoLumUserSettingsVersion;

  if (lastAmpIdx && j.contains("lastAmpIdx"))
  {
    const int defaultLastAmpIdx = 0;
    loadInt(j, "lastAmpIdx", *lastAmpIdx, 0, ampCount - 1, defaultLastAmpIdx);
  }

  if (j.contains("amps") && j["amps"].is_object())
  {
    bool resetAmpSettings = false;
    for (int i = 0; i < ampCount; ++i)
    {
      const char* key = kAmps[i].folderName;
      if (!j["amps"].contains(key))
        continue;

      const auto& a = j["amps"][key];
      if (a.contains("channel") && a["channel"].is_number_integer() && a["channel"].get<long long>() < 0)
      {
        resetAmpSettings = true;
        break;
      }
    }

    if (resetAmpSettings)
    {
      for (int i = 0; i < ampCount; ++i)
        ampSettings[i] = VoLumAmpSettings{};
      healed = true;
    }
    else
    {
      for (int i = 0; i < ampCount; ++i)
      {
        const char* key = kAmps[i].folderName;
        if (!j["amps"].contains(key))
          continue;

        const auto& a = j["amps"][key];
        auto& s = ampSettings[i];
        const VoLumAmpSettings defaults;
        loadInt(a, "speaker", s.speakerIdx, 0, 3, defaults.speakerIdx);
        loadInt(a, "channel", s.channelIdx, 0, 127, defaults.channelIdx);
        loadDouble(a, "input", s.inputLevel, -20.0, 20.0, defaults.inputLevel);
        loadDouble(a, "gate", s.gateThreshold, -100.0, 0.0, defaults.gateThreshold);
        loadDouble(a, "bass", s.toneBass, 0.0, 10.0, defaults.toneBass);
        loadDouble(a, "mid", s.toneMid, 0.0, 10.0, defaults.toneMid);
        loadDouble(a, "treble", s.toneTreble, 0.0, 10.0, defaults.toneTreble);
        loadDouble(a, "output", s.outputLevel, -40.0, 10.0, defaults.outputLevel);
        loadBool(a, "noiseGate", s.noiseGateActive, defaults.noiseGateActive);
        loadBool(a, "eq", s.eqActive, defaults.eqActive);
        loadBool(a, "preCompActive", s.preCompActive, defaults.preCompActive);
        loadDouble(a, "preCompAmount", s.preCompAmount, 0.0, 10.0, defaults.preCompAmount);
        loadDouble(a, "preCompRatio", s.preCompRatio, 1.0, 20.0, defaults.preCompRatio);
        loadDouble(a, "preCompAttack", s.preCompAttack, 0.1, 30.0, defaults.preCompAttack);
        loadDouble(a, "preCompRelease", s.preCompRelease, 20.0, 800.0, defaults.preCompRelease);
        loadDouble(a, "preCompMix", s.preCompMix, 0.0, 1.0, defaults.preCompMix);
        loadDouble(a, "preCompLevel", s.preCompLevel, -20.0, 20.0, defaults.preCompLevel);
        loadBool(a, "preNam1Active", s.preNam1Active, defaults.preNam1Active);
        loadInt(a, "preNam1Capture", s.preNam1Capture, 0, 127, defaults.preNam1Capture);
        if (resetLegacyPreCaptureSelections && a.contains("preNam1Capture") && s.preNam1Capture != defaults.preNam1Capture)
        {
          s.preNam1Capture = defaults.preNam1Capture;
          healed = true;
        }
        loadDouble(a, "preNam1Gain", s.preNam1Gain, -20.0, 20.0, defaults.preNam1Gain);
        loadDouble(a, "preNam1Bass", s.preNam1Bass, 0.0, 10.0, defaults.preNam1Bass);
        loadDouble(a, "preNam1Mid", s.preNam1Mid, 0.0, 10.0, defaults.preNam1Mid);
        loadDouble(a, "preNam1MidFreq", s.preNam1MidFreq, 150.0, 2500.0, defaults.preNam1MidFreq);
        loadDouble(a, "preNam1Treble", s.preNam1Treble, 0.0, 10.0, defaults.preNam1Treble);
        loadDouble(a, "preNam1Level", s.preNam1Level, -20.0, 20.0, defaults.preNam1Level);
        loadBool(a, "preNam2Active", s.preNam2Active, defaults.preNam2Active);
        loadInt(a, "preNam2Capture", s.preNam2Capture, 0, 127, defaults.preNam2Capture);
        if (resetLegacyPreCaptureSelections && a.contains("preNam2Capture") && s.preNam2Capture != defaults.preNam2Capture)
        {
          s.preNam2Capture = defaults.preNam2Capture;
          healed = true;
        }
        loadDouble(a, "preNam2Gain", s.preNam2Gain, -20.0, 20.0, defaults.preNam2Gain);
        loadDouble(a, "preNam2Bass", s.preNam2Bass, 0.0, 10.0, defaults.preNam2Bass);
        loadDouble(a, "preNam2Mid", s.preNam2Mid, 0.0, 10.0, defaults.preNam2Mid);
        loadDouble(a, "preNam2MidFreq", s.preNam2MidFreq, 150.0, 2500.0, defaults.preNam2MidFreq);
        loadDouble(a, "preNam2Treble", s.preNam2Treble, 0.0, 10.0, defaults.preNam2Treble);
        loadDouble(a, "preNam2Level", s.preNam2Level, -20.0, 20.0, defaults.preNam2Level);
        loadBool(a, "dualAmpActive", s.dualAmpActive, defaults.dualAmpActive);
        loadInt(a, "dualAmpRoute", s.dualAmpRoute, 0, 2, defaults.dualAmpRoute);
        loadDouble(a, "mainAmpPan", s.mainAmpPan, -1.0, 1.0, defaults.mainAmpPan);
        loadInt(a, "supportAmp", s.supportAmpIdx, -1, ampCount - 1, defaults.supportAmpIdx);
        loadInt(a, "supportSpeaker", s.supportSpeakerIdx, 0, 3, defaults.supportSpeakerIdx);
        loadInt(a, "supportChannel", s.supportChannelIdx, 0, 127, defaults.supportChannelIdx);
        loadDouble(a, "supportInput", s.supportInputLevel, -20.0, 20.0, defaults.supportInputLevel);
        loadDouble(a, "supportGate", s.supportGateThreshold, -100.0, 0.0, defaults.supportGateThreshold);
        loadDouble(a, "supportBass", s.supportToneBass, 0.0, 10.0, defaults.supportToneBass);
        loadDouble(a, "supportMid", s.supportToneMid, 0.0, 10.0, defaults.supportToneMid);
        loadDouble(a, "supportTreble", s.supportToneTreble, 0.0, 10.0, defaults.supportToneTreble);
        loadDouble(a, "supportOutput", s.supportOutputLevel, -40.0, 10.0, defaults.supportOutputLevel);
        loadBool(a, "supportNoiseGate", s.supportNoiseGateActive, defaults.supportNoiseGateActive);
        loadBool(a, "supportEq", s.supportEqActive, defaults.supportEqActive);
        loadDouble(a, "supportPan", s.supportAmpPan, -1.0, 1.0, defaults.supportAmpPan);
        loadBool(a, "supportPolarityInvert", s.supportPolarityInvert, defaults.supportPolarityInvert);
      }
    }
  }

  if (fx && j.contains("effects") && j["effects"].is_object())
  {
    const auto& e = j["effects"];
    const VoLumEffectSettings defaults;
    loadBool(e, "delayActive", fx->delayActive, defaults.delayActive);
    loadInt(e, "delayMode", fx->delayMode, 0, kVoLumDelayModeCount - 1, defaults.delayMode);
    loadBool(e, "reverbActive", fx->reverbActive, defaults.reverbActive);
    loadInt(e, "reverbMode", fx->reverbMode, 0, kVoLumReverbModeCount - 1, defaults.reverbMode);

    if (e.contains("delayModes") && e["delayModes"].is_array())
    {
      const auto& modes = e["delayModes"];
      for (int i = 0; i < kVoLumDelayModeCount && i < static_cast<int>(modes.size()); ++i)
      {
        const auto& mode = modes[i];
        loadDouble(mode, "time", fx->delayModes[i].time, 10.0, 2000.0, defaults.delayModes[i].time);
        loadDouble(mode, "feedback", fx->delayModes[i].feedback, 0.0, 0.99, defaults.delayModes[i].feedback);
        loadDouble(mode, "mix", fx->delayModes[i].mix, 0.0, 1.0, defaults.delayModes[i].mix);
      }
    }
    else
    {
      DelayModeSnapshot legacy;
      loadDouble(e, "delayTime", legacy.time, 10.0, 2000.0, DelayModeSnapshot{}.time);
      loadDouble(e, "delayFeedback", legacy.feedback, 0.0, 0.99, DelayModeSnapshot{}.feedback);
      loadDouble(e, "delayMix", legacy.mix, 0.0, 1.0, DelayModeSnapshot{}.mix);
      for (int i = 0; i < kVoLumReverseDelayMode; ++i)
        fx->delayModes[i] = legacy;
    }

    if (e.contains("reverbModes") && e["reverbModes"].is_array())
    {
      const auto& modes = e["reverbModes"];
      for (int i = 0; i < kVoLumReverbModeCount && i < static_cast<int>(modes.size()); ++i)
      {
        const auto& mode = modes[i];
        loadDouble(mode, "mix", fx->reverbModes[i].mix, 0.0, 1.0, defaults.reverbModes[i].mix);
        loadDouble(mode, "decay", fx->reverbModes[i].decay, 0.1, 10.0, defaults.reverbModes[i].decay);
        loadDouble(mode, "tone", fx->reverbModes[i].tone, 0.0, 10.0, defaults.reverbModes[i].tone);
        loadDouble(mode, "preDelay", fx->reverbModes[i].preDelay, 0.0, 80.0, defaults.reverbModes[i].preDelay);
        loadDouble(mode, "shimmer", fx->reverbModes[i].shimmer, 0.0, 1.0, defaults.reverbModes[i].shimmer);
      }
    }
    else
    {
      ReverbModeSnapshot legacy;
      loadDouble(e, "reverbMix", legacy.mix, 0.0, 1.0, ReverbModeSnapshot{}.mix);
      loadDouble(e, "reverbDecay", legacy.decay, 0.1, 10.0, ReverbModeSnapshot{}.decay);
      loadDouble(e, "reverbTone", legacy.tone, 0.0, 10.0, ReverbModeSnapshot{}.tone);
      loadDouble(e, "reverbPreDelay", legacy.preDelay, 0.0, 80.0, ReverbModeSnapshot{}.preDelay);
      loadDouble(e, "reverbShimmer", legacy.shimmer, 0.0, 1.0, ReverbModeSnapshot{}.shimmer);
      for (auto& mode : fx->reverbModes)
        mode = legacy;
    }
  }

  if (didHeal)
    *didHeal = healed;
}

} // namespace volum
