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


namespace volum
{

// v5 stores independent Oktaverb knob snapshots per Halo / Shimmer / Bloom sub-mode.
// Slot 0 was originally "Dark" in 0.9.1 and became "Halo" (dual +-12 in feedback) in
// 0.9.2; the index stays stable so existing presets / settings keep loading.
// v6 adds per-main-amp POST live values (delay + reverb knob positions, modes, and
// active toggles) under each amps[<folderName>] entry. Legacy v<6 settings are
// restored to factory POST defaults per amp on first load so brand-new amps don't
// inherit stale tweaks from a single global POST scene.
//
// PRE/POST lock fields (`preLocked`, `postLocked`, `liveLockedPre`, `liveLockedPost`)
// were introduced in VoLum 1.0.1. They are purely additive optional keys: older
// readers ignore unknown fields, and the version reader (`VolumUserSettingsFromJson`)
// is forward-tolerant, so a settings file written by a newer build with `version > 6`
// is still loaded by this reader without triggering a destructive legacy migration.
// Bumping `kVoLumUserSettingsVersion` is therefore reserved for changes that
// fundamentally alter how existing keys are interpreted, NOT for additive fields.
inline constexpr int kVoLumUserSettingsVersion = 6;

// See VoLumAmpeteCatalog.h for delay/reverb mode constants and snapshot structs.

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

struct VoLumEffectSettings
{
  bool reverbActive = false;
  int reverbMode = 0;
  ReverbModeSnapshot reverbModes[kVoLumReverbModeCount] = {
    // Hall: the good Cathedral-ish recipe from iteration 2, exposed simply as Hall.
    ReverbModeSnapshot{0.20, 2.5, 5.0, 30.0, 0.0, /*subMode*/ 0},
    // Plate: restored dev/original plate behaviour and defaults.
    ReverbModeSnapshot{0.20, 2.5, 4.5, 20.0, 0.0, /*subMode*/ 0},
    // Oktaverb: high-quality pitch-reverb with Halo / Shimmer / Bloom voices.
    ReverbModeSnapshot{0.30, 6.0, 6.0, 30.0, 0.70, /*subMode*/ kVoLumOktaverbSubModeShimmer},
  };

  // Defaults are placeholders until final voicing values are chosen by ear.
  // Per-mode default knob snapshots. Halo wants a brighter tone than the old Dark
  // since the +12 voice carries the body; Shimmer / Bloom unchanged.
  OktaverbSubModeSnapshot oktaverbSubModes[3] = {
    // Halo (was Dark in 0.9.1): bright tone, moderate decay, midway intensity.
    OktaverbSubModeSnapshot{0.30, 5.5, 6.0, 25.0, 0.65},
    // Shimmer
    OktaverbSubModeSnapshot{0.30, 6.0, 6.0, 30.0, 0.70},
    // Bloom
    OktaverbSubModeSnapshot{0.30, 5.5, 5.5, 20.0, 0.75},
  };

  bool delayActive = false;
  int delayMode = kVoLumDelayModeDigital;
  // Default snapshots per design guide.
  DelayModeSnapshot delayModes[kVoLumDelayModeCount] = {
    // Digital: clean, defaults flat tone, no age
    DelayModeSnapshot{320.0, 0.35, 0.28, 0.50, 0.00, false},
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
  // 1.2.0: custom SUPPORT partner binding + its own cab/channel (additive; older
  // readers ignore). Lets a custom support amp's cab + gain stage round-trip
  // through presets, the dual-amp settings sidecar, and DAW scenes.
  a["supportCustomId"] = s.supportCustomId;
  a["supportCustomSlot"] = s.supportCustomSlot;
  a["supportCustomChannel"] = s.supportCustomChannel;
}

inline bool HasDualAmpUserSettings(const nlohmann::json& j)
{
  if (!j.contains("amps") || !j["amps"].is_object())
    return false;

  static constexpr const char* kDualKeys[] = {
    "dualAmpActive",    "dualAmpRoute",   "mainAmpPan",    "supportAmp",
    "supportSpeaker",   "supportChannel", "supportInput",  "supportGate",
    "supportBass",      "supportMid",     "supportTreble", "supportOutput",
    "supportNoiseGate", "supportEq",      "supportPan",    "supportPolarityInvert",
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

inline nlohmann::json DelayModeSnapshotsToJson(const DelayModeSnapshot* modes, int modeCount)
{
  nlohmann::json out = nlohmann::json::array();
  for (int i = 0; i < modeCount; ++i)
  {
    out.push_back({
      {"time", modes[i].time},
      {"feedback", modes[i].feedback},
      {"mix", modes[i].mix},
      {"tone", modes[i].tone},
      {"age", modes[i].age},
      {"pingPong", modes[i].pingPong},
    });
  }
  return out;
}

inline nlohmann::json ReverbModeSnapshotsToJson(const ReverbModeSnapshot* modes, int modeCount)
{
  nlohmann::json out = nlohmann::json::array();
  for (int i = 0; i < modeCount; ++i)
  {
    out.push_back({
      {"mix", modes[i].mix},
      {"decay", modes[i].decay},
      {"tone", modes[i].tone},
      {"preDelay", modes[i].preDelay},
      {"shimmer", modes[i].shimmer},
      {"subMode", modes[i].subMode},
    });
  }
  return out;
}

inline nlohmann::json OktaverbSubModeSnapshotsToJson(const OktaverbSubModeSnapshot* subModes, int subModeCount)
{
  nlohmann::json out = nlohmann::json::array();
  for (int i = 0; i < subModeCount; ++i)
  {
    out.push_back({
      {"mix", subModes[i].mix},
      {"decay", subModes[i].decay},
      {"tone", subModes[i].tone},
      {"preDelay", subModes[i].preDelay},
      {"shimmer", subModes[i].shimmer},
    });
  }
  return out;
}

// Serialize just the PRE block of a VoLumAmpSettings (compressor + two PRE NAM
// pedals). Used for the standalone live-lock snapshot so we can persist the live
// PRE state independently of any per-amp slot when the user has PRE locked.
namespace detail
{
inline bool JsonGetClampedInt(const nlohmann::json& obj, const char* key, int& target, int minValue, int maxValue)
{
  if (!obj.contains(key) || !obj[key].is_number_integer())
    return false;
  const long long v = obj[key].get<long long>();
  if (v < minValue || v > maxValue)
    return false;
  target = static_cast<int>(v);
  return true;
}
inline bool JsonGetClampedDouble(const nlohmann::json& obj, const char* key, double& target, double minValue,
                                 double maxValue)
{
  if (!obj.contains(key) || !obj[key].is_number())
    return false;
  const double v = obj[key].get<double>();
  if (!std::isfinite(v) || v < minValue || v > maxValue)
    return false;
  target = v;
  return true;
}
inline bool JsonGetBool(const nlohmann::json& obj, const char* key, bool& target)
{
  if (!obj.contains(key) || !obj[key].is_boolean())
    return false;
  target = obj[key].get<bool>();
  return true;
}
} // namespace detail

// Read a PRE block snapshot from JSON. Out-of-range / missing fields fall back
// to the defaults already in `out`. Returns true if the object had at least one
// recognized PRE key (so callers can tell a snapshot was present vs absent).
inline bool PreBlockFromJson(const nlohmann::json& o, VoLumAmpSettings& out)
{
  const VoLumAmpSettings defaults;
  bool any = false;
  any |= detail::JsonGetBool(o, "preCompActive", out.preCompActive);
  any |= detail::JsonGetClampedDouble(o, "preCompAmount", out.preCompAmount, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "preCompRatio", out.preCompRatio, 1.0, 20.0);
  any |= detail::JsonGetClampedDouble(o, "preCompAttack", out.preCompAttack, 0.1, 30.0);
  any |= detail::JsonGetClampedDouble(o, "preCompRelease", out.preCompRelease, 20.0, 800.0);
  any |= detail::JsonGetClampedDouble(o, "preCompMix", out.preCompMix, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "preCompLevel", out.preCompLevel, -20.0, 20.0);
  any |= detail::JsonGetBool(o, "preNam1Active", out.preNam1Active);
  any |= detail::JsonGetClampedInt(o, "preNam1Capture", out.preNam1Capture, 0, 127);
  any |= detail::JsonGetClampedDouble(o, "preNam1Gain", out.preNam1Gain, -20.0, 20.0);
  any |= detail::JsonGetClampedDouble(o, "preNam1Bass", out.preNam1Bass, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "preNam1Mid", out.preNam1Mid, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "preNam1MidFreq", out.preNam1MidFreq, 150.0, 2500.0);
  any |= detail::JsonGetClampedDouble(o, "preNam1Treble", out.preNam1Treble, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "preNam1Level", out.preNam1Level, -20.0, 20.0);
  any |= detail::JsonGetBool(o, "preNam2Active", out.preNam2Active);
  any |= detail::JsonGetClampedInt(o, "preNam2Capture", out.preNam2Capture, 0, 127);
  any |= detail::JsonGetClampedDouble(o, "preNam2Gain", out.preNam2Gain, -20.0, 20.0);
  any |= detail::JsonGetClampedDouble(o, "preNam2Bass", out.preNam2Bass, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "preNam2Mid", out.preNam2Mid, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "preNam2MidFreq", out.preNam2MidFreq, 150.0, 2500.0);
  any |= detail::JsonGetClampedDouble(o, "preNam2Treble", out.preNam2Treble, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "preNam2Level", out.preNam2Level, -20.0, 20.0);
  any |= detail::JsonGetBool(o, "prePitchActive", out.prePitchActive);
  any |= detail::JsonGetClampedInt(o, "prePitchMode", out.prePitchMode, 0, 1);
  any |= detail::JsonGetClampedDouble(o, "prePitchSemitones", out.prePitchSemitones, -12.0, 7.0);
  any |= detail::JsonGetClampedDouble(o, "prePitchMix", out.prePitchMix, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "prePitchOctDown", out.prePitchOctDown, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "prePitchOctUp", out.prePitchOctUp, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "prePitchDry", out.prePitchDry, 0.0, 1.0);
  any |= detail::JsonGetClampedInt(o, "prePitchVoicing", out.prePitchVoicing, 0, 1);
  any |= detail::JsonGetClampedDouble(o, "prePitchLevel", out.prePitchLevel, -20.0, 20.0);
  any |=
    detail::JsonGetClampedInt(o, "prePitchTransChar", out.prePitchTransChar, 0, volum::kVoLumPitchCharacterCount - 1);
  return any;
}

inline bool PostBlockFromJson(const nlohmann::json& o, VoLumAmpSettings& out)
{
  const VoLumAmpSettings defaults;
  bool any = false;
  any |= detail::JsonGetBool(o, "postValid", out.postValid);
  any |= detail::JsonGetBool(o, "postDelayActive", out.postDelayActive);
  any |= detail::JsonGetClampedDouble(o, "postDelayTime", out.postDelayTime, 10.0, 2000.0);
  any |= detail::JsonGetClampedDouble(o, "postDelayFeedback", out.postDelayFeedback, 0.0, 0.99);
  any |= detail::JsonGetClampedDouble(o, "postDelayMix", out.postDelayMix, 0.0, 1.0);
  any |= detail::JsonGetClampedInt(o, "postDelayMode", out.postDelayMode, 0, kVoLumDelayModeCount - 1);
  any |= detail::JsonGetClampedDouble(o, "postDelayTone", out.postDelayTone, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "postDelayAge", out.postDelayAge, 0.0, 1.0);
  any |= detail::JsonGetBool(o, "postDelayPingPong", out.postDelayPingPong);
  any |= detail::JsonGetBool(o, "postReverbActive", out.postReverbActive);
  any |= detail::JsonGetClampedDouble(o, "postReverbMix", out.postReverbMix, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "postReverbDecay", out.postReverbDecay, 0.1, 10.0);
  any |= detail::JsonGetClampedDouble(o, "postReverbTone", out.postReverbTone, 0.0, 10.0);
  any |= detail::JsonGetClampedDouble(o, "postReverbPreDelay", out.postReverbPreDelay, 0.0, 200.0);
  any |= detail::JsonGetClampedDouble(o, "postReverbShimmer", out.postReverbShimmer, 0.0, 1.0);
  any |= detail::JsonGetClampedInt(o, "postReverbMode", out.postReverbMode, 0, kVoLumReverbModeCount - 1);
  any |= detail::JsonGetClampedInt(o, "postReverbSubMode", out.postReverbSubMode, 0, 2);
  any |= detail::JsonGetBool(o, "postTremoloActive", out.postTremoloActive);
  any |= detail::JsonGetClampedInt(o, "postTremoloMode", out.postTremoloMode, 0, kVoLumTremoloModeCount - 1);
  any |= detail::JsonGetClampedDouble(o, "postTremoloRate", out.postTremoloRate, 0.1, 20.0);
  any |= detail::JsonGetClampedDouble(o, "postTremoloDepth", out.postTremoloDepth, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "postTremoloShape", out.postTremoloShape, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "postTremoloMix", out.postTremoloMix, 0.0, 1.0);
  any |= detail::JsonGetClampedDouble(o, "postTremoloCrossover", out.postTremoloCrossover, 200.0, 2000.0);
  any |= detail::JsonGetBool(o, "postTremoloSync", out.postTremoloSync);
  any |=
    detail::JsonGetClampedInt(o, "postTremoloDivision", out.postTremoloDivision, 0, kVoLumTremoloDivisionCount - 1);
  if (o.contains("postDelayModes") && o["postDelayModes"].is_array())
  {
    const auto& modes = o["postDelayModes"];
    for (int i = 0; i < kVoLumDelayModeCount && i < static_cast<int>(modes.size()); ++i)
    {
      const auto& m = modes[i];
      auto& dst = out.postDelayModes[i];
      detail::JsonGetClampedDouble(m, "time", dst.time, 10.0, 2000.0);
      detail::JsonGetClampedDouble(m, "feedback", dst.feedback, 0.0, 0.99);
      detail::JsonGetClampedDouble(m, "mix", dst.mix, 0.0, 1.0);
      detail::JsonGetClampedDouble(m, "tone", dst.tone, 0.0, 1.0);
      detail::JsonGetClampedDouble(m, "age", dst.age, 0.0, 1.0);
      detail::JsonGetBool(m, "pingPong", dst.pingPong);
    }
    any = true;
  }
  if (o.contains("postReverbModes") && o["postReverbModes"].is_array())
  {
    const auto& modes = o["postReverbModes"];
    for (int i = 0; i < kVoLumReverbModeCount && i < static_cast<int>(modes.size()); ++i)
    {
      const auto& m = modes[i];
      auto& dst = out.postReverbModes[i];
      detail::JsonGetClampedDouble(m, "mix", dst.mix, 0.0, 1.0);
      detail::JsonGetClampedDouble(m, "decay", dst.decay, 0.1, 10.0);
      detail::JsonGetClampedDouble(m, "tone", dst.tone, 0.0, 10.0);
      detail::JsonGetClampedDouble(m, "preDelay", dst.preDelay, 0.0, 200.0);
      detail::JsonGetClampedDouble(m, "shimmer", dst.shimmer, 0.0, 1.0);
      detail::JsonGetClampedInt(m, "subMode", dst.subMode, 0, 2);
    }
    any = true;
  }
  if (o.contains("postOktaverbSubModes") && o["postOktaverbSubModes"].is_array())
  {
    const auto& subs = o["postOktaverbSubModes"];
    for (int i = 0; i < 3 && i < static_cast<int>(subs.size()); ++i)
    {
      const auto& m = subs[i];
      auto& dst = out.postOktaverbSubModes[i];
      detail::JsonGetClampedDouble(m, "mix", dst.mix, 0.0, 1.0);
      detail::JsonGetClampedDouble(m, "decay", dst.decay, 0.1, 10.0);
      detail::JsonGetClampedDouble(m, "tone", dst.tone, 0.0, 10.0);
      detail::JsonGetClampedDouble(m, "preDelay", dst.preDelay, 0.0, 200.0);
      detail::JsonGetClampedDouble(m, "shimmer", dst.shimmer, 0.0, 1.0);
    }
    any = true;
  }
  (void)defaults;
  return any;
}

inline nlohmann::json PreBlockToJson(const VoLumAmpSettings& s)
{
  nlohmann::json o;
  o["preCompActive"] = s.preCompActive;
  o["preCompAmount"] = s.preCompAmount;
  o["preCompRatio"] = s.preCompRatio;
  o["preCompAttack"] = s.preCompAttack;
  o["preCompRelease"] = s.preCompRelease;
  o["preCompMix"] = s.preCompMix;
  o["preCompLevel"] = s.preCompLevel;
  o["preNam1Active"] = s.preNam1Active;
  o["preNam1Capture"] = s.preNam1Capture;
  o["preNam1Gain"] = s.preNam1Gain;
  o["preNam1Bass"] = s.preNam1Bass;
  o["preNam1Mid"] = s.preNam1Mid;
  o["preNam1MidFreq"] = s.preNam1MidFreq;
  o["preNam1Treble"] = s.preNam1Treble;
  o["preNam1Level"] = s.preNam1Level;
  o["preNam2Active"] = s.preNam2Active;
  o["preNam2Capture"] = s.preNam2Capture;
  o["preNam2Gain"] = s.preNam2Gain;
  o["preNam2Bass"] = s.preNam2Bass;
  o["preNam2Mid"] = s.preNam2Mid;
  o["preNam2MidFreq"] = s.preNam2MidFreq;
  o["preNam2Treble"] = s.preNam2Treble;
  o["preNam2Level"] = s.preNam2Level;
  o["prePitchActive"] = s.prePitchActive;
  o["prePitchMode"] = s.prePitchMode;
  o["prePitchSemitones"] = s.prePitchSemitones;
  o["prePitchMix"] = s.prePitchMix;
  o["prePitchOctDown"] = s.prePitchOctDown;
  o["prePitchOctUp"] = s.prePitchOctUp;
  o["prePitchDry"] = s.prePitchDry;
  o["prePitchVoicing"] = s.prePitchVoicing;
  o["prePitchLevel"] = s.prePitchLevel;
  o["prePitchTransChar"] = s.prePitchTransChar;
  return o;
}

inline nlohmann::json PostBlockToJson(const VoLumAmpSettings& s)
{
  nlohmann::json o;
  o["postValid"] = s.postValid;
  o["postDelayActive"] = s.postDelayActive;
  o["postDelayTime"] = s.postDelayTime;
  o["postDelayFeedback"] = s.postDelayFeedback;
  o["postDelayMix"] = s.postDelayMix;
  o["postDelayMode"] = s.postDelayMode;
  o["postDelayTone"] = s.postDelayTone;
  o["postDelayAge"] = s.postDelayAge;
  o["postDelayPingPong"] = s.postDelayPingPong;
  o["postReverbActive"] = s.postReverbActive;
  o["postReverbMix"] = s.postReverbMix;
  o["postReverbDecay"] = s.postReverbDecay;
  o["postReverbTone"] = s.postReverbTone;
  o["postReverbPreDelay"] = s.postReverbPreDelay;
  o["postReverbShimmer"] = s.postReverbShimmer;
  o["postReverbMode"] = s.postReverbMode;
  o["postReverbSubMode"] = s.postReverbSubMode;
  o["postTremoloActive"] = s.postTremoloActive;
  o["postTremoloMode"] = s.postTremoloMode;
  o["postTremoloRate"] = s.postTremoloRate;
  o["postTremoloDepth"] = s.postTremoloDepth;
  o["postTremoloShape"] = s.postTremoloShape;
  o["postTremoloMix"] = s.postTremoloMix;
  o["postTremoloCrossover"] = s.postTremoloCrossover;
  o["postTremoloSync"] = s.postTremoloSync;
  o["postTremoloDivision"] = s.postTremoloDivision;
  o["postDelayModes"] = DelayModeSnapshotsToJson(s.postDelayModes, kVoLumDelayModeCount);
  o["postReverbModes"] = ReverbModeSnapshotsToJson(s.postReverbModes, kVoLumReverbModeCount);
  o["postOktaverbSubModes"] = OktaverbSubModeSnapshotsToJson(s.postOktaverbSubModes, 3);
  return o;
}

inline nlohmann::json VolumUserSettingsToJson(const VoLumAmpSettings* ampSettings, int ampCount, int lastAmpIdx,
                                              const VoLumEffectSettings* fx = nullptr, bool includeDualAmp = true,
                                              bool preLocked = false, bool postLocked = false,
                                              const VoLumAmpSettings* liveLockedPre = nullptr,
                                              const VoLumAmpSettings* liveLockedPost = nullptr, bool liteMode = false)
{
  nlohmann::json j;
  j["version"] = kVoLumUserSettingsVersion;
  j["lastAmpIdx"] = lastAmpIdx;
  j["preLocked"] = preLocked;
  j["postLocked"] = postLocked;
  // VoLum 1.2.0: machine-global A2 Lite mode (false = Full, default). Additive
  // optional key; older readers ignore it, so no version bump.
  j["liteMode"] = liteMode;
  // Live lock snapshots: persisted independently of per-amp slots so that
  // reopening the app with a lock still on restores the exact live PRE/POST
  // the user was hearing, without ever mutating any amp's stored scene.
  if (preLocked && liveLockedPre)
    j["liveLockedPre"] = PreBlockToJson(*liveLockedPre);
  if (postLocked && liveLockedPost)
    j["liveLockedPost"] = PostBlockToJson(*liveLockedPost);

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
    // VoLum 1.2.0: persist the per-amp custom IR selection (opaque content-store
    // id; empty == baked cab) so a factory amp's chosen IR survives a standalone
    // restart. Additive optional key; older readers ignore it.
    a["activeIrId"] = s.activeIrId;
    // VoLum 1.2.0: per-lane support-amp custom IR (additive; empty == baked cab).
    a["supportActiveIrId"] = s.supportActiveIrId;
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
    a["prePitchActive"] = s.prePitchActive;
    a["prePitchMode"] = s.prePitchMode;
    a["prePitchSemitones"] = s.prePitchSemitones;
    a["prePitchMix"] = s.prePitchMix;
    a["prePitchOctDown"] = s.prePitchOctDown;
    a["prePitchOctUp"] = s.prePitchOctUp;
    a["prePitchDry"] = s.prePitchDry;
    a["prePitchVoicing"] = s.prePitchVoicing;
    a["prePitchLevel"] = s.prePitchLevel;
    a["prePitchTransChar"] = s.prePitchTransChar;
    if (includeDualAmp)
      WriteDualAmpUserSettings(a, s);

    // POST per-amp live values (v6+). postValid distinguishes a real per-amp scene
    // from a legacy / never-saved slot; restore initializes postValid=false slots to
    // factory POST defaults instead of inheriting the previously selected amp.
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
    a["postTremoloActive"] = s.postTremoloActive;
    a["postTremoloMode"] = s.postTremoloMode;
    a["postTremoloRate"] = s.postTremoloRate;
    a["postTremoloDepth"] = s.postTremoloDepth;
    a["postTremoloShape"] = s.postTremoloShape;
    a["postTremoloMix"] = s.postTremoloMix;
    a["postTremoloCrossover"] = s.postTremoloCrossover;
    a["postTremoloSync"] = s.postTremoloSync;
    a["postTremoloDivision"] = s.postTremoloDivision;
    a["postDelayModes"] = DelayModeSnapshotsToJson(s.postDelayModes, kVoLumDelayModeCount);
    a["postReverbModes"] = ReverbModeSnapshotsToJson(s.postReverbModes, kVoLumReverbModeCount);
    a["postOktaverbSubModes"] = OktaverbSubModeSnapshotsToJson(s.postOktaverbSubModes, 3);

    amps[kAmps[i].folderName] = a;
  }
  j["amps"] = amps;

  if (fx)
  {
    nlohmann::json e;
    e["delayActive"] = fx->delayActive;
    e["delayMode"] = fx->delayMode;
    e["reverbActive"] = fx->reverbActive;
    e["reverbMode"] = fx->reverbMode;
    e["delayModes"] = DelayModeSnapshotsToJson(fx->delayModes, kVoLumDelayModeCount);
    e["reverbModes"] = ReverbModeSnapshotsToJson(fx->reverbModes, kVoLumReverbModeCount);
    e["oktaverbSubModes"] = OktaverbSubModeSnapshotsToJson(fx->oktaverbSubModes, 3);
    j["effects"] = e;
  }

  return j;
}

inline void VolumUserSettingsFromJson(const nlohmann::json& j, VoLumAmpSettings* ampSettings, int ampCount,
                                      int* lastAmpIdx, VoLumEffectSettings* fx = nullptr, bool* didHeal = nullptr,
                                      bool* preLocked = nullptr, bool* postLocked = nullptr,
                                      VoLumAmpSettings* liveLockedPre = nullptr,
                                      VoLumAmpSettings* liveLockedPost = nullptr, bool* haveLiveLockedPre = nullptr,
                                      bool* haveLiveLockedPost = nullptr, bool* liteMode = nullptr)
{
  bool healed = false;
  auto loadInt = [&](const nlohmann::json& obj, const char* key, int& target, int minValue, int maxValue,
                     int defaultValue) {
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
  if (j.contains("version") && j["version"].is_number_integer())
  {
    // Forward-tolerant: a settings file written by a newer build can contain
    // version > kVoLumUserSettingsVersion. Treat it as "current schema, ignore
    // unknown fields" rather than as a corrupt v1 file that needs migration.
    // This prevents A/B downgrades from blowing away the user's tweaks.
    const long long rawVersion = j["version"].get<long long>();
    if (rawVersion >= kVoLumUserSettingsVersion)
      settingsVersion = kVoLumUserSettingsVersion;
    else if (rawVersion >= 1)
      settingsVersion = static_cast<int>(rawVersion);
    else
    {
      settingsVersion = 1;
      healed = true;
    }
  }
  else if (j.contains("version"))
  {
    settingsVersion = 1;
    healed = true;
  }
  const bool resetLegacyPreCaptureSelections = settingsVersion < 3;

  if (lastAmpIdx && j.contains("lastAmpIdx"))
  {
    const int defaultLastAmpIdx = 0;
    loadInt(j, "lastAmpIdx", *lastAmpIdx, 0, ampCount - 1, defaultLastAmpIdx);
  }

  // PRE/POST lock flags: optional, no version gate (older files omit them, older
  // readers ignore them).
  if (preLocked)
  {
    *preLocked = false;
    loadBool(j, "preLocked", *preLocked, false);
  }
  if (postLocked)
  {
    *postLocked = false;
    loadBool(j, "postLocked", *postLocked, false);
  }

  // VoLum 1.2.0: machine-global A2 Lite mode. Optional, no version gate; default
  // Full (false) when the key is absent (older files / fresh installs).
  if (liteMode)
  {
    *liteMode = false;
    loadBool(j, "liteMode", *liteMode, false);
  }

  // Live lock snapshots: stored independently of per-amp slots so that the
  // exact PRE/POST live state at shutdown comes back at startup without ever
  // mutating any amp's stored scene. Optional; absent on older files.
  if (haveLiveLockedPre)
    *haveLiveLockedPre = false;
  if (haveLiveLockedPost)
    *haveLiveLockedPost = false;
  if (liveLockedPre && j.contains("liveLockedPre") && j["liveLockedPre"].is_object())
  {
    const bool any = PreBlockFromJson(j["liveLockedPre"], *liveLockedPre);
    if (haveLiveLockedPre)
      *haveLiveLockedPre = any;
  }
  if (liveLockedPost && j.contains("liveLockedPost") && j["liveLockedPost"].is_object())
  {
    const bool any = PostBlockFromJson(j["liveLockedPost"], *liveLockedPost);
    if (haveLiveLockedPost)
      *haveLiveLockedPost = any;
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
        // VoLum 1.2.0: per-amp custom IR id (additive; absent on older files ->
        // keep the default empty id == baked cab).
        if (a.contains("activeIrId") && a["activeIrId"].is_string())
          s.activeIrId = a["activeIrId"].get<std::string>();
        if (a.contains("supportActiveIrId") && a["supportActiveIrId"].is_string())
          s.supportActiveIrId = a["supportActiveIrId"].get<std::string>();
        loadBool(a, "preCompActive", s.preCompActive, defaults.preCompActive);
        loadDouble(a, "preCompAmount", s.preCompAmount, 0.0, 10.0, defaults.preCompAmount);
        loadDouble(a, "preCompRatio", s.preCompRatio, 1.0, 20.0, defaults.preCompRatio);
        loadDouble(a, "preCompAttack", s.preCompAttack, 0.1, 30.0, defaults.preCompAttack);
        loadDouble(a, "preCompRelease", s.preCompRelease, 20.0, 800.0, defaults.preCompRelease);
        loadDouble(a, "preCompMix", s.preCompMix, 0.0, 1.0, defaults.preCompMix);
        loadDouble(a, "preCompLevel", s.preCompLevel, -20.0, 20.0, defaults.preCompLevel);
        loadBool(a, "preNam1Active", s.preNam1Active, defaults.preNam1Active);
        loadInt(a, "preNam1Capture", s.preNam1Capture, 0, 127, defaults.preNam1Capture);
        if (resetLegacyPreCaptureSelections && a.contains("preNam1Capture")
            && s.preNam1Capture != defaults.preNam1Capture)
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
        if (resetLegacyPreCaptureSelections && a.contains("preNam2Capture")
            && s.preNam2Capture != defaults.preNam2Capture)
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
        loadBool(a, "prePitchActive", s.prePitchActive, defaults.prePitchActive);
        loadInt(a, "prePitchMode", s.prePitchMode, 0, 1, defaults.prePitchMode);
        loadDouble(a, "prePitchSemitones", s.prePitchSemitones, -12.0, 7.0, defaults.prePitchSemitones);
        loadDouble(a, "prePitchMix", s.prePitchMix, 0.0, 1.0, defaults.prePitchMix);
        loadDouble(a, "prePitchOctDown", s.prePitchOctDown, 0.0, 1.0, defaults.prePitchOctDown);
        loadDouble(a, "prePitchOctUp", s.prePitchOctUp, 0.0, 1.0, defaults.prePitchOctUp);
        loadDouble(a, "prePitchDry", s.prePitchDry, 0.0, 1.0, defaults.prePitchDry);
        loadInt(a, "prePitchVoicing", s.prePitchVoicing, 0, 1, defaults.prePitchVoicing);
        loadDouble(a, "prePitchLevel", s.prePitchLevel, -20.0, 20.0, defaults.prePitchLevel);
        loadInt(a, "prePitchTransChar", s.prePitchTransChar, 0, volum::kVoLumPitchCharacterCount - 1,
                defaults.prePitchTransChar);
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
        // 1.2.0: custom SUPPORT partner id + its own cab/channel (additive; absent
        // on older files -> keep defaults == no custom support partner).
        if (a.contains("supportCustomId") && a["supportCustomId"].is_string())
          s.supportCustomId = a["supportCustomId"].get<std::string>();
        loadInt(a, "supportCustomSlot", s.supportCustomSlot, -2, 2, defaults.supportCustomSlot);
        loadInt(a, "supportCustomChannel", s.supportCustomChannel, 0, 8, defaults.supportCustomChannel);

        // v6+ per-amp POST live values. On legacy (v<6) settings the keys are absent
        // and the struct defaults remain in place; postValid stays false so amp restore
        // can initialize a fresh factory POST scene instead of copying the previously
        // selected amp's POST settings.
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
        loadBool(a, "postTremoloActive", s.postTremoloActive, defaults.postTremoloActive);
        loadInt(a, "postTremoloMode", s.postTremoloMode, 0, kVoLumTremoloModeCount - 1, defaults.postTremoloMode);
        loadDouble(a, "postTremoloRate", s.postTremoloRate, 0.1, 20.0, defaults.postTremoloRate);
        loadDouble(a, "postTremoloDepth", s.postTremoloDepth, 0.0, 1.0, defaults.postTremoloDepth);
        loadDouble(a, "postTremoloShape", s.postTremoloShape, 0.0, 1.0, defaults.postTremoloShape);
        loadDouble(a, "postTremoloMix", s.postTremoloMix, 0.0, 1.0, defaults.postTremoloMix);
        loadDouble(a, "postTremoloCrossover", s.postTremoloCrossover, 200.0, 2000.0, defaults.postTremoloCrossover);
        loadBool(a, "postTremoloSync", s.postTremoloSync, defaults.postTremoloSync);
        loadInt(a, "postTremoloDivision", s.postTremoloDivision, 0, kVoLumTremoloDivisionCount - 1,
                defaults.postTremoloDivision);
        if (a.contains("postDelayModes") && a["postDelayModes"].is_array())
        {
          const auto& modes = a["postDelayModes"];
          for (int modeIdx = 0; modeIdx < kVoLumDelayModeCount && modeIdx < static_cast<int>(modes.size()); ++modeIdx)
          {
            const auto& mode = modes[modeIdx];
            auto& dst = s.postDelayModes[modeIdx];
            const auto& def = defaults.postDelayModes[modeIdx];
            loadDouble(mode, "time", dst.time, 10.0, 2000.0, def.time);
            loadDouble(mode, "feedback", dst.feedback, 0.0, 0.99, def.feedback);
            loadDouble(mode, "mix", dst.mix, 0.0, 1.0, def.mix);
            loadDouble(mode, "tone", dst.tone, 0.0, 1.0, def.tone);
            loadDouble(mode, "age", dst.age, 0.0, 1.0, def.age);
            loadBool(mode, "pingPong", dst.pingPong, def.pingPong);
          }
        }
        else if (a.contains("postDelayModes"))
        {
          healed = true;
        }
        if (a.contains("postReverbModes") && a["postReverbModes"].is_array())
        {
          const auto& modes = a["postReverbModes"];
          for (int modeIdx = 0; modeIdx < kVoLumReverbModeCount && modeIdx < static_cast<int>(modes.size()); ++modeIdx)
          {
            const auto& mode = modes[modeIdx];
            auto& dst = s.postReverbModes[modeIdx];
            const auto& def = defaults.postReverbModes[modeIdx];
            loadDouble(mode, "mix", dst.mix, 0.0, 1.0, def.mix);
            loadDouble(mode, "decay", dst.decay, 0.1, 10.0, def.decay);
            loadDouble(mode, "tone", dst.tone, 0.0, 10.0, def.tone);
            loadDouble(mode, "preDelay", dst.preDelay, 0.0, 200.0, def.preDelay);
            loadDouble(mode, "shimmer", dst.shimmer, 0.0, 1.0, def.shimmer);
            loadInt(mode, "subMode", dst.subMode, 0, 2, def.subMode);
          }
        }
        else if (a.contains("postReverbModes"))
        {
          healed = true;
        }
        if (a.contains("postOktaverbSubModes") && a["postOktaverbSubModes"].is_array())
        {
          const auto& subModes = a["postOktaverbSubModes"];
          for (int subIdx = 0; subIdx < 3 && subIdx < static_cast<int>(subModes.size()); ++subIdx)
          {
            const auto& sub = subModes[subIdx];
            auto& dst = s.postOktaverbSubModes[subIdx];
            const auto& def = defaults.postOktaverbSubModes[subIdx];
            loadDouble(sub, "mix", dst.mix, 0.0, 1.0, def.mix);
            loadDouble(sub, "decay", dst.decay, 0.1, 10.0, def.decay);
            loadDouble(sub, "tone", dst.tone, 0.0, 10.0, def.tone);
            loadDouble(sub, "preDelay", dst.preDelay, 0.0, 200.0, def.preDelay);
            loadDouble(sub, "shimmer", dst.shimmer, 0.0, 1.0, def.shimmer);
          }
        }
        else if (a.contains("postOktaverbSubModes"))
        {
          healed = true;
        }
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
          loadDouble(
            mode, "preDelay", fx->oktaverbSubModes[i].preDelay, 0.0, 200.0, defaults.oktaverbSubModes[i].preDelay);
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
