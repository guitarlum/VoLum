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

inline constexpr int kVoLumDelayModeCount = 3;
inline constexpr int kVoLumReverbModeCount = 3;
// v5 stores independent Oktaverb knob snapshots per Halo / Shimmer / Bloom sub-mode.
// Slot 0 was originally "Dark" in 0.9.1 and became "Halo" (dual +-12 in feedback) in
// 0.9.2; the index stays stable so existing presets / settings keep loading.
// v6 adds per-main-amp POST live values (delay + reverb knob positions, modes, and
// active toggles) under each amps[<folderName>] entry. Legacy v<6 settings are
// reset to factory POST defaults per amp on first load (postValid flipped to true
// at end of load) so brand-new amps don't inherit stale tweaks from a single global
// POST scene.
inline constexpr int kVoLumUserSettingsVersion = 6;

// Effect-staging delay-mode order: 0=Digital, 1=Analog, 2=Reverse
inline constexpr int kVoLumDelayModeDigital = 0;
inline constexpr int kVoLumDelayModeAnalog = 1;
inline constexpr int kVoLumDelayModeReverse = 2;
// Back-compat alias: pre-v0.9.0 code referenced kVoLumReverseDelayMode by name.
inline constexpr int kVoLumReverseDelayMode = kVoLumDelayModeReverse;

// Reverb-mode order remains: 0=Hall, 1=Plate, 2=Oktaverb
inline constexpr int kVoLumReverbModeHall = 0;
inline constexpr int kVoLumReverbModePlate = 1;
inline constexpr int kVoLumReverbModeOktaverb = 2;
inline constexpr int kVoLumOktaverbSubModeHalo = 0;
inline constexpr int kVoLumOktaverbSubModeShimmer = 1;
inline constexpr int kVoLumOktaverbSubModeBloom = 2;
// Backward-compat alias: old "Dark" name kept so any external callers still build. Slot 0
// is now Halo (dual +-12 in feedback); see Reverb.cpp GetOktaverbSubMode.
inline constexpr int kVoLumOktaverbSubModeDark = kVoLumOktaverbSubModeHalo;

inline int RemapLegacyOktaverbSubModeToV4Settings(int oldSubMode)
{
  switch (oldSubMode)
  {
    case 2: return kVoLumOktaverbSubModeHalo; // old Oct+Sub -> slot 0 (now Halo)
    case 0:
    case 1:
    default: return kVoLumOktaverbSubModeShimmer; // old Oct / Oct+5th -> Shimmer
  }
}

struct DelayModeSnapshot {
  double time = 380.0;
  double feedback = 0.35;
  double mix = 0.28;
  // Iteration 2 additions (v0.9.0):
  double tone = 0.5;       // 0..1, per-mode DSP curve
  double age = 0.0;        // 0..1, per-mode DSP meaning
  bool pingPong = false;   // ignored when mode == Reverse
};

struct ReverbModeSnapshot {
  double mix = 0.3;
  double decay = 3.0;
  double tone = 4.5;
  double preDelay = 20.0;
  double shimmer = 0.5;
  int subMode = kVoLumOktaverbSubModeShimmer; // Oktaverb only: 0=Halo, 1=Shimmer, 2=Bloom
};

struct OktaverbSubModeSnapshot {
  double mix = 0.40;
  double decay = 5.0;
  double tone = 5.5;
  double preDelay = 30.0;
  double shimmer = 0.65;
};

struct VoLumEffectSettings {
  bool reverbActive = false;
  int reverbMode = 0;
  ReverbModeSnapshot reverbModes[kVoLumReverbModeCount] = {
    // Hall: the good Cathedral-ish recipe from iteration 2, exposed simply as Hall.
    ReverbModeSnapshot{0.20, 2.5, 5.0, 30.0, 0.0, /*subMode*/ 0},
    // Plate: restored dev/original plate behaviour and defaults.
    ReverbModeSnapshot{0.25, 2.5, 4.5, 20.0, 0.0, /*subMode*/ 0},
    // Oktaverb: high-quality pitch-reverb with Halo / Shimmer / Bloom voices.
    ReverbModeSnapshot{0.40, 5.0, 5.5, 30.0, 0.75, /*subMode*/ kVoLumOktaverbSubModeShimmer},
  };

  // Defaults are placeholders until final voicing values are chosen by ear.
  // Per-mode default knob snapshots. Halo wants a brighter tone than the old Dark
  // since the +12 voice carries the body; Shimmer / Bloom unchanged.
  OktaverbSubModeSnapshot oktaverbSubModes[3] = {
    // Halo (was Dark in 0.9.1): bright tone, moderate decay, midway intensity.
    OktaverbSubModeSnapshot{0.40, 5.5, 6.0, 25.0, 0.55},
    // Shimmer
    OktaverbSubModeSnapshot{0.40, 6.0, 6.0, 30.0, 0.75},
    // Bloom
    OktaverbSubModeSnapshot{0.42, 5.5, 5.5, 20.0, 0.60},
  };

  bool delayActive = false;
  int delayMode = kVoLumDelayModeDigital;
  // Default snapshots per design guide.
  DelayModeSnapshot delayModes[kVoLumDelayModeCount] = {
    // Digital: clean, defaults flat tone, no age
    DelayModeSnapshot{380.0, 0.35, 0.28, 0.50, 0.00, false},
    // Analog (Memory Man): warmer, slightly more feedback, age=0.5 for chorus depth
    DelayModeSnapshot{320.0, 0.42, 0.32, 0.50, 0.50, false},
    // Reverse: restored dev core; Bloom defaults to 0 so the old edge fade remains the
    // baseline. Mix lowered from 0.40 to 0.32 alongside the reverse blend-law fix so a
    // fresh patch sits at the same level as Digital / Analog at the same Mix.
    DelayModeSnapshot{600.0, 0.30, 0.32, 0.50, 0.00, false},
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

    // POST per-amp live values (v6+). postValid distinguishes "real per-amp scene"
    // from "legacy slot defaulted at load time" so the loader can no-op restore on
    // postValid=false (currently always true after a save round-trip).
    a["postValid"] = s.postValid;
    a["postDelayActive"] = s.postDelayActive;
    a["postDelayTime"] = s.postDelayTime;
    a["postDelayFeedback"] = s.postDelayFeedback;
    a["postDelayMix"] = s.postDelayMix;
    a["postDelayMode"] = s.postDelayMode;
    a["postDelayTone"] = s.postDelayTone;
    a["postDelayAge"] = s.postDelayAge;
    a["postDelayPingPong"] = s.postDelayPingPong;
    a["postReverbActive"] = s.postReverbActive;
    a["postReverbMix"] = s.postReverbMix;
    a["postReverbDecay"] = s.postReverbDecay;
    a["postReverbTone"] = s.postReverbTone;
    a["postReverbPreDelay"] = s.postReverbPreDelay;
    a["postReverbShimmer"] = s.postReverbShimmer;
    a["postReverbMode"] = s.postReverbMode;
    a["postReverbSubMode"] = s.postReverbSubMode;

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
        {"tone", mode.tone},
        {"age", mode.age},
        {"pingPong", mode.pingPong},
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
        {"subMode", mode.subMode},
      });
    }
    e["oktaverbSubModes"] = nlohmann::json::array();
    for (const auto& sub : fx->oktaverbSubModes)
    {
      e["oktaverbSubModes"].push_back({
        {"mix", sub.mix},
        {"decay", sub.decay},
        {"tone", sub.tone},
        {"preDelay", sub.preDelay},
        {"shimmer", sub.shimmer},
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
  const bool resetLegacyPreCaptureSelections = settingsVersion < 3;

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

        // v6+ per-amp POST live values. On legacy (v<6) settings the keys are absent
        // and the struct defaults remain in place; postValid stays false so the
        // loader does not clobber the active EParams. Once written to disk by a
        // post-v6 build, these round-trip with postValid=true and the loader
        // restores them on amp switch.
        loadBool(a, "postValid", s.postValid, defaults.postValid);
        loadBool(a, "postDelayActive", s.postDelayActive, defaults.postDelayActive);
        loadDouble(a, "postDelayTime", s.postDelayTime, 10.0, 2000.0, defaults.postDelayTime);
        loadDouble(a, "postDelayFeedback", s.postDelayFeedback, 0.0, 0.99, defaults.postDelayFeedback);
        loadDouble(a, "postDelayMix", s.postDelayMix, 0.0, 1.0, defaults.postDelayMix);
        loadInt(a, "postDelayMode", s.postDelayMode, 0, kVoLumDelayModeCount - 1, defaults.postDelayMode);
        loadDouble(a, "postDelayTone", s.postDelayTone, 0.0, 1.0, defaults.postDelayTone);
        loadDouble(a, "postDelayAge", s.postDelayAge, 0.0, 1.0, defaults.postDelayAge);
        loadBool(a, "postDelayPingPong", s.postDelayPingPong, defaults.postDelayPingPong);
        loadBool(a, "postReverbActive", s.postReverbActive, defaults.postReverbActive);
        loadDouble(a, "postReverbMix", s.postReverbMix, 0.0, 1.0, defaults.postReverbMix);
        loadDouble(a, "postReverbDecay", s.postReverbDecay, 0.1, 10.0, defaults.postReverbDecay);
        loadDouble(a, "postReverbTone", s.postReverbTone, 0.0, 10.0, defaults.postReverbTone);
        loadDouble(a, "postReverbPreDelay", s.postReverbPreDelay, 0.0, 200.0, defaults.postReverbPreDelay);
        loadDouble(a, "postReverbShimmer", s.postReverbShimmer, 0.0, 1.0, defaults.postReverbShimmer);
        loadInt(a, "postReverbMode", s.postReverbMode, 0, kVoLumReverbModeCount - 1, defaults.postReverbMode);
        loadInt(a, "postReverbSubMode", s.postReverbSubMode, 0, 2, defaults.postReverbSubMode);
      }
    }
  }

  if (fx && j.contains("effects") && j["effects"].is_object())
  {
    const auto& e = j["effects"];
    const VoLumEffectSettings defaults;
    loadBool(e, "delayActive", fx->delayActive, defaults.delayActive);
    loadBool(e, "reverbActive", fx->reverbActive, defaults.reverbActive);

    // Settings v3 (effect-staging) introduces the smaller delay mode order
    //   {Digital, Analog, Reverse} (was {Tape, Digital, PingPong, Reverse}).
    const bool legacyDelayLayout = settingsVersion < 3;
    const bool legacyOktaverbSubModes = settingsVersion < 4;

    int rawDelayMode = defaults.delayMode;
    // Legacy settings may contain the old Reverse index (3), so allow that until we remap.
    loadInt(e, "delayMode", rawDelayMode, 0, legacyDelayLayout ? 3 : kVoLumDelayModeCount - 1, defaults.delayMode);
    int rawReverbMode = defaults.reverbMode;
    loadInt(e, "reverbMode", rawReverbMode, 0, kVoLumReverbModeCount - 1, defaults.reverbMode);

    DelayModeSnapshot rawDelayModes[kVoLumDelayModeCount];
    for (int i = 0; i < kVoLumDelayModeCount; ++i)
      rawDelayModes[i] = defaults.delayModes[i];

    DelayModeSnapshot legacyOldModes[4] = {
      defaults.delayModes[kVoLumDelayModeDigital], // old Tape slot, remapped below
      defaults.delayModes[kVoLumDelayModeDigital], // old Digital
      defaults.delayModes[kVoLumDelayModeDigital], // old PingPong
      defaults.delayModes[kVoLumDelayModeReverse], // old Reverse
    };

    if (e.contains("delayModes") && e["delayModes"].is_array())
    {
      const auto& modes = e["delayModes"];
      const int maxModes = legacyDelayLayout ? 4 : kVoLumDelayModeCount;
      for (int i = 0; i < maxModes && i < static_cast<int>(modes.size()); ++i)
      {
        const auto& mode = modes[i];
        DelayModeSnapshot& dst = legacyDelayLayout ? legacyOldModes[i] : rawDelayModes[i];
        const DelayModeSnapshot& def = legacyDelayLayout ? legacyOldModes[i] : defaults.delayModes[i];
        loadDouble(mode, "time", dst.time, 10.0, 2000.0, def.time);
        loadDouble(mode, "feedback", dst.feedback, 0.0, 0.99, def.feedback);
        loadDouble(mode, "mix", dst.mix, 0.0, 1.0, def.mix);
        loadDouble(mode, "tone", dst.tone, 0.0, 1.0, def.tone);
        loadDouble(mode, "age", dst.age, 0.0, 1.0, def.age);
        loadBool(mode, "pingPong", dst.pingPong, def.pingPong);
      }
    }
    else
    {
      // Legacy flat fields: pre-mode-snapshot schema. Fill Digital/Analog with these values;
      // Reverse keeps its staging default.
      DelayModeSnapshot legacy;
      loadDouble(e, "delayTime", legacy.time, 10.0, 2000.0, DelayModeSnapshot{}.time);
      loadDouble(e, "delayFeedback", legacy.feedback, 0.0, 0.99, DelayModeSnapshot{}.feedback);
      loadDouble(e, "delayMix", legacy.mix, 0.0, 1.0, DelayModeSnapshot{}.mix);
      for (int i = 0; i < kVoLumDelayModeReverse; ++i)
      {
        rawDelayModes[i].time = legacy.time;
        rawDelayModes[i].feedback = legacy.feedback;
        rawDelayModes[i].mix = legacy.mix;
      }
      for (int i = 0; i < 3; ++i)
        legacyOldModes[i] = legacy;
    }

    if (legacyDelayLayout)
    {
      // Old order: 0=Tape, 1=Digital, 2=PingPong, 3=Reverse.
      // New order: 0=Digital, 1=Analog, 2=Reverse. Old Tape is removed and falls back to
      // Digital; old PingPong folds into Digital with pingPong=true.
      auto carryLegacyCore = [](DelayModeSnapshot& dst, const DelayModeSnapshot& src) {
        dst.time = src.time;
        dst.feedback = src.feedback;
        dst.mix = src.mix;
      };

      const DelayModeSnapshot oldTape = legacyOldModes[0];
      const DelayModeSnapshot oldDigital = legacyOldModes[1];
      const DelayModeSnapshot oldPingPong = legacyOldModes[2];
      const DelayModeSnapshot oldReverse = legacyOldModes[3];

      DelayModeSnapshot newDigital = defaults.delayModes[kVoLumDelayModeDigital];
      DelayModeSnapshot newAnalog = defaults.delayModes[kVoLumDelayModeAnalog];
      DelayModeSnapshot newReverse = defaults.delayModes[kVoLumDelayModeReverse];
      carryLegacyCore(newDigital, oldDigital);
      carryLegacyCore(newReverse, oldReverse);

      if (rawDelayMode == 2)
      {
        carryLegacyCore(newDigital, oldPingPong);
        newDigital.pingPong = true;
      }
      else if (rawDelayMode == 0)
      {
        // Tape is not part of staging. Preserve the user's old Tape timing as Digital
        // only when Tape was the active mode; otherwise old Digital remains the source.
        carryLegacyCore(newDigital, oldTape);
      }

      fx->delayModes[kVoLumDelayModeDigital] = newDigital;
      fx->delayModes[kVoLumDelayModeAnalog] = newAnalog;
      fx->delayModes[kVoLumDelayModeReverse] = newReverse;

      switch (rawDelayMode)
      {
        case 0: fx->delayMode = kVoLumDelayModeDigital; break; // old Tape -> Digital
        case 1: fx->delayMode = kVoLumDelayModeDigital; break;
        case 2: fx->delayMode = kVoLumDelayModeDigital; break; // PingPong -> Digital + pingPong=true
        case 3: fx->delayMode = kVoLumDelayModeReverse; break;
        default: fx->delayMode = defaults.delayMode; break;
      }

      healed = true;
    }
    else
    {
      for (int i = 0; i < kVoLumDelayModeCount; ++i)
        fx->delayModes[i] = rawDelayModes[i];
      fx->delayMode = std::clamp(rawDelayMode, 0, kVoLumDelayModeCount - 1);
    }

    // Reverb mode snapshots
    if (e.contains("reverbModes") && e["reverbModes"].is_array())
    {
      const auto& modes = e["reverbModes"];
      const int avail = static_cast<int>(modes.size());
      for (int i = 0; i < kVoLumReverbModeCount; ++i)
      {
        if (i < avail)
        {
          const auto& mode = modes[i];
          loadDouble(mode, "mix", fx->reverbModes[i].mix, 0.0, 1.0, defaults.reverbModes[i].mix);
          loadDouble(mode, "decay", fx->reverbModes[i].decay, 0.1, 10.0, defaults.reverbModes[i].decay);
          loadDouble(mode, "tone", fx->reverbModes[i].tone, 0.0, 10.0, defaults.reverbModes[i].tone);
          loadDouble(mode, "preDelay", fx->reverbModes[i].preDelay, 0.0, 200.0, defaults.reverbModes[i].preDelay);
          loadDouble(mode, "shimmer", fx->reverbModes[i].shimmer, 0.0, 1.0, defaults.reverbModes[i].shimmer);
          loadInt(mode, "subMode", fx->reverbModes[i].subMode, 0, 2, defaults.reverbModes[i].subMode);
          if (legacyOktaverbSubModes && i == kVoLumReverbModeOktaverb)
          {
            fx->reverbModes[i].subMode = RemapLegacyOktaverbSubModeToV4Settings(fx->reverbModes[i].subMode);
            healed = true;
          }
        }
        else
        {
          fx->reverbModes[i] = defaults.reverbModes[i];
        }
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
      for (int i = 0; i < kVoLumReverbModeCount; ++i)
        fx->reverbModes[i] = legacy;
    }

    bool loadedOktaverbSubModes = false;
    if (e.contains("oktaverbSubModes") && e["oktaverbSubModes"].is_array())
    {
      const auto& modes = e["oktaverbSubModes"];
      const int avail = static_cast<int>(modes.size());
      for (int i = 0; i < 3; ++i)
      {
        if (i < avail)
        {
          const auto& mode = modes[i];
          loadDouble(mode, "mix", fx->oktaverbSubModes[i].mix, 0.0, 1.0, defaults.oktaverbSubModes[i].mix);
          loadDouble(mode, "decay", fx->oktaverbSubModes[i].decay, 0.1, 10.0, defaults.oktaverbSubModes[i].decay);
          loadDouble(mode, "tone", fx->oktaverbSubModes[i].tone, 0.0, 10.0, defaults.oktaverbSubModes[i].tone);
          loadDouble(mode, "preDelay", fx->oktaverbSubModes[i].preDelay, 0.0, 200.0, defaults.oktaverbSubModes[i].preDelay);
          loadDouble(mode, "shimmer", fx->oktaverbSubModes[i].shimmer, 0.0, 1.0, defaults.oktaverbSubModes[i].shimmer);
        }
        else
        {
          fx->oktaverbSubModes[i] = defaults.oktaverbSubModes[i];
        }
      }
      loadedOktaverbSubModes = true;
    }

    if (!loadedOktaverbSubModes)
    {
      const auto& okt = fx->reverbModes[kVoLumReverbModeOktaverb];
      for (auto& sub : fx->oktaverbSubModes)
      {
        sub.mix = okt.mix;
        sub.decay = okt.decay;
        sub.tone = okt.tone;
        sub.preDelay = okt.preDelay;
        sub.shimmer = okt.shimmer;
      }
    }

    fx->reverbMode = std::clamp(rawReverbMode, 0, kVoLumReverbModeCount - 1);
  }

  if (didHeal)
    *didHeal = healed;
}

} // namespace volum
