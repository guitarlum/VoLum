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
inline constexpr int kVoLumReverbModeCount = 4; // v0.9.0: added TremVerb
// v4 heals v3 files saved by an early buggy v2->v3 migration that left Analog/Tape ages at 0.0
// instead of the design-guide 0.5 (the pre-load step seeded the wrong-slot defaults for legacy
// reads). When a v3 file presents the buggy pattern (Analog.age == Tape.age == 0.0), v3->v4
// resets those two ages to per-slot defaults. Other fields are left intact.
inline constexpr int kVoLumUserSettingsVersion = 4;

// v0.9.0 delay-mode order: 0=Digital, 1=Analog, 2=Tape, 3=Reverse
inline constexpr int kVoLumDelayModeDigital = 0;
inline constexpr int kVoLumDelayModeAnalog = 1;
inline constexpr int kVoLumDelayModeTape = 2;
inline constexpr int kVoLumDelayModeReverse = 3;
// Back-compat alias: pre-v0.9.0 code referenced kVoLumReverseDelayMode by name.
inline constexpr int kVoLumReverseDelayMode = kVoLumDelayModeReverse;

// v0.9.0 reverb-mode order: 0=Hall, 1=Plate, 2=Oktaverb, 3=TremVerb
inline constexpr int kVoLumReverbModeHall = 0;
inline constexpr int kVoLumReverbModePlate = 1;
inline constexpr int kVoLumReverbModeOktaverb = 2;
inline constexpr int kVoLumReverbModeTremVerb = 3;

struct DelayModeSnapshot {
  double time = 380.0;
  double feedback = 0.35;
  double mix = 0.28;
  // Iteration 2 additions (v0.9.0):
  double tone = 0.5;       // 0..1, per-mode DSP curve
  double age = 0.0;        // 0..1, per-mode DSP meaning
  bool pingPong = false;   // ignored when mode == Reverse
  int tapeSubMode = 1;     // 0=Studio,1=Vintage,2=Broken (Tape only; ignored elsewhere)
};

struct ReverbModeSnapshot {
  double mix = 0.3;
  double decay = 3.0;
  double tone = 4.5;
  double preDelay = 20.0;
  double shimmer = 0.5;
  // Iteration 2 additions (v0.9.0):
  int subMode = 0;       // per-mode meaning (Hall/Plate/Oktaverb); ignored for TremVerb
  double tremRate = 4.0; // Hz, only used by TremVerb mode
};

struct VoLumEffectSettings {
  bool reverbActive = false;
  int reverbMode = 0;
  ReverbModeSnapshot reverbModes[kVoLumReverbModeCount] = {
    // Hall: stronger defaults per design guide (mix ~0.32, decay ~3.5s, tone slightly bright)
    ReverbModeSnapshot{0.32, 3.5, 5.5, 30.0, 0.0, /*subMode*/ 1, /*tremRate*/ 4.0},
    // Plate: brighter default tone, faster modulation, Steel sub-mode default
    ReverbModeSnapshot{0.30, 1.8, 6.0, 15.0, 0.0, /*subMode*/ 0, /*tremRate*/ 4.0},
    // Oktaverb: musical-but-not-cranked shimmer (curve retuned)
    ReverbModeSnapshot{0.40, 5.0, 5.5, 30.0, 0.40, /*subMode*/ 0, /*tremRate*/ 4.0},
    // TremVerb (new): short plate base, photocell trem on wet (shimmer = trem depth)
    ReverbModeSnapshot{0.32, 1.5, 5.5, 12.0, 0.55, /*subMode*/ 0, /*tremRate*/ 4.0},
  };

  bool delayActive = false;
  int delayMode = kVoLumDelayModeDigital;
  // Default snapshots per design guide.
  DelayModeSnapshot delayModes[kVoLumDelayModeCount] = {
    // Digital: clean, defaults flat tone, no age
    DelayModeSnapshot{380.0, 0.35, 0.28, 0.50, 0.00, false, 1},
    // Analog (Memory Man): warmer, slightly more feedback, age=0.5 for chorus depth
    DelayModeSnapshot{320.0, 0.42, 0.32, 0.50, 0.50, false, 1},
    // Tape: vintage default, sub-mode = Vintage, slightly dark tone, age=0.5
    DelayModeSnapshot{420.0, 0.45, 0.30, 0.45, 0.50, false, 1},
    // Reverse: half-sine fade default
    DelayModeSnapshot{600.0, 0.30, 0.40, 0.50, 0.50, false, 1},
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
        {"tone", mode.tone},
        {"age", mode.age},
        {"pingPong", mode.pingPong},
        {"tapeSubMode", mode.tapeSubMode},
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
        {"tremRate", mode.tremRate},
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
    loadBool(e, "reverbActive", fx->reverbActive, defaults.reverbActive);

    // Settings v3 (VoLum 0.9.0) introduces new delay mode order
    //   {Digital, Analog, Tape, Reverse} (was {Tape, Digital, PingPong, Reverse}).
    // and a 4-entry reverb mode list (added TremVerb at index 3).
    const bool legacyDelayLayout = settingsVersion < 3;
    const bool legacyReverbLayout = settingsVersion < 3;

    int rawDelayMode = defaults.delayMode;
    loadInt(e, "delayMode", rawDelayMode, 0, kVoLumDelayModeCount - 1, defaults.delayMode);
    int rawReverbMode = defaults.reverbMode;
    // Old reverb only had 3 modes; allow up to current count for forward-compat.
    loadInt(e, "reverbMode", rawReverbMode, 0, kVoLumReverbModeCount - 1, defaults.reverbMode);

    // Parse delay mode snapshots into a temporary array using the layout described in the JSON.
    // Initialise to current-schema defaults so missing slots (e.g. legacy 3-entry arrays) keep
    // their iteration-2 defaults instead of zero-initialising to {380, 0.35, 0.28}.
    DelayModeSnapshot rawDelayModes[kVoLumDelayModeCount];
    for (int i = 0; i < kVoLumDelayModeCount; ++i)
      rawDelayModes[i] = defaults.delayModes[i];

    if (e.contains("delayModes") && e["delayModes"].is_array())
    {
      const auto& modes = e["delayModes"];
      for (int i = 0; i < kVoLumDelayModeCount && i < static_cast<int>(modes.size()); ++i)
      {
        const auto& mode = modes[i];
        loadDouble(mode, "time", rawDelayModes[i].time, 10.0, 2000.0, defaults.delayModes[i].time);
        loadDouble(mode, "feedback", rawDelayModes[i].feedback, 0.0, 0.99, defaults.delayModes[i].feedback);
        loadDouble(mode, "mix", rawDelayModes[i].mix, 0.0, 1.0, defaults.delayModes[i].mix);
        // v3 fields - default to neutral if absent (legacy rows only).
        loadDouble(mode, "tone", rawDelayModes[i].tone, 0.0, 1.0, 0.5);
        loadDouble(mode, "age", rawDelayModes[i].age, 0.0, 1.0, 0.0);
        loadBool(mode, "pingPong", rawDelayModes[i].pingPong, false);
        loadInt(mode, "tapeSubMode", rawDelayModes[i].tapeSubMode, 0, 2, 1);
      }
    }
    else
    {
      // Legacy flat fields: pre-mode-snapshot schema. Fill non-Reverse slots with these values;
      // Reverse keeps its iteration-2 default.
      DelayModeSnapshot legacy;
      loadDouble(e, "delayTime", legacy.time, 10.0, 2000.0, DelayModeSnapshot{}.time);
      loadDouble(e, "delayFeedback", legacy.feedback, 0.0, 0.99, DelayModeSnapshot{}.feedback);
      loadDouble(e, "delayMix", legacy.mix, 0.0, 1.0, DelayModeSnapshot{}.mix);
      for (int i = 0; i < kVoLumReverseDelayMode; ++i)
      {
        rawDelayModes[i].time = legacy.time;
        rawDelayModes[i].feedback = legacy.feedback;
        rawDelayModes[i].mix = legacy.mix;
      }
      // rawDelayModes[kVoLumReverseDelayMode] keeps defaults.delayModes[Reverse].
    }

    if (legacyDelayLayout)
    {
      // Old order: 0=Tape, 1=Digital, 2=PingPong, 3=Reverse.
      // New order: 0=Digital, 1=Analog, 2=Tape, 3=Reverse.
      // Map old snapshots to new slots; fold old PingPong snapshot into Digital with pingPong=true.
      // v2 files only authored {time, feedback, mix}, so we discard the v3 fields from
      // rawDelayModes (which were just the wrong-slot defaults from the pre-load step) and
      // re-apply the design-guide defaults for the destination NEW slot. That way the user's
      // saved time/feedback/mix carry over, but new fields (age, tone, pingPong, tapeSubMode)
      // pick up the correct iteration-2 defaults for the slot they end up in.
      auto carryLegacyCore = [](DelayModeSnapshot& dst, const DelayModeSnapshot& src) {
        dst.time = src.time;
        dst.feedback = src.feedback;
        dst.mix = src.mix;
      };

      const DelayModeSnapshot oldTape = rawDelayModes[0];
      const DelayModeSnapshot oldDigital = rawDelayModes[1];
      const DelayModeSnapshot oldPingPong = rawDelayModes[2];
      const DelayModeSnapshot oldReverse = rawDelayModes[3];

      DelayModeSnapshot newDigital = defaults.delayModes[kVoLumDelayModeDigital];
      DelayModeSnapshot newAnalog = defaults.delayModes[kVoLumDelayModeAnalog];
      DelayModeSnapshot newTape = defaults.delayModes[kVoLumDelayModeTape];
      DelayModeSnapshot newReverse = defaults.delayModes[kVoLumDelayModeReverse];
      carryLegacyCore(newDigital, oldDigital);
      carryLegacyCore(newTape, oldTape);
      carryLegacyCore(newReverse, oldReverse);

      // Only fold the old PingPong snapshot into Digital when the user was actively in
      // PingPong mode at save time. Otherwise the user's old Digital snapshot wins for new
      // Digital, and the old PingPong snapshot is discarded (PingPong is no longer a mode).
      if (rawDelayMode == 2)
      {
        carryLegacyCore(newDigital, oldPingPong);
        newDigital.pingPong = true;
      }

      fx->delayModes[kVoLumDelayModeDigital] = newDigital;
      fx->delayModes[kVoLumDelayModeAnalog] = newAnalog;
      fx->delayModes[kVoLumDelayModeTape] = newTape;
      fx->delayModes[kVoLumDelayModeReverse] = newReverse;

      // Remap the active mode index.
      switch (rawDelayMode)
      {
        case 0: fx->delayMode = kVoLumDelayModeTape; break;
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

      // v3 -> v4 heal: an early version of the v2->v3 migration left Analog/Tape ages at 0.0
      // because the pre-load step seeded each raw slot with the WRONG-slot defaults. Detect
      // the buggy pattern (Analog.age and Tape.age both exactly 0.0) and reset those two
      // slots' ages to the per-slot design-guide defaults. v3 files written intentionally
      // with both ages at 0 are vanishingly unlikely (Analog/Tape sound dry without age).
      if (settingsVersion < 4)
      {
        const double analogAge = fx->delayModes[kVoLumDelayModeAnalog].age;
        const double tapeAge = fx->delayModes[kVoLumDelayModeTape].age;
        if (analogAge == 0.0 && tapeAge == 0.0)
        {
          fx->delayModes[kVoLumDelayModeAnalog].age = defaults.delayModes[kVoLumDelayModeAnalog].age;
          fx->delayModes[kVoLumDelayModeTape].age = defaults.delayModes[kVoLumDelayModeTape].age;
          healed = true;
        }
      }
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
          // v3 fields
          loadInt(mode, "subMode", fx->reverbModes[i].subMode, 0, 2, defaults.reverbModes[i].subMode);
          loadDouble(mode, "tremRate", fx->reverbModes[i].tremRate, 0.5, 10.0, defaults.reverbModes[i].tremRate);
        }
        else
        {
          // Modes beyond what the legacy file held (e.g. TremVerb) start from defaults.
          fx->reverbModes[i] = defaults.reverbModes[i];
          if (legacyReverbLayout)
            healed = true;
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

    fx->reverbMode = std::clamp(rawReverbMode, 0, kVoLumReverbModeCount - 1);
  }

  if (didHeal)
    *didHeal = healed;
}

} // namespace volum
