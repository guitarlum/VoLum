#pragma once

// Full VoLumAmpSettings <-> JSON codec.
//
// volum-settings.json has its own (heavily tested) per-amp serializer inside
// VoLumUserSettingsIO.h that is intentionally left untouched. This header gives
// a reusable codec for the 1.2.0 content store: preset snapshots and isolated
// per-custom-amp scenes both need to round-trip a *complete* VoLumAmpSettings
// (core tone + PRE + dual-amp + POST + the new id-based custom-content refs).
//
// It composes the existing PRE/POST/dual helpers from VoLumUserSettingsIO.h and
// only adds the small main-amp core block + the two id strings, so the field
// list stays in lock-step with the rest of the settings code.

#include <cstddef>
#include <map>
#include <string>

#include "VoLumUserSettingsIO.h"

#if __has_include(<nlohmann/json.hpp>)
  #include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
  #include <json.hpp>
#else
  #error "nlohmann json header not found (expected iPlug Dependencies/Extras layout)"
#endif

namespace volum
{

// WriteAmpCoreBlock / ReadAmpCoreBlock now live in VoLumUserSettingsIO.h next to
// the other block writers/readers (single source of truth for the field list);
// they are visible here via that include.

inline void ReadDualAmpBlock(const nlohmann::json& a, VoLumAmpSettings& s)
{
  const VoLumAmpSettings d;
  detail::JsonGetBool(a, "dualAmpActive", s.dualAmpActive);
  detail::JsonGetClampedInt(a, "dualAmpRoute", s.dualAmpRoute, 0, 2);
  detail::JsonGetClampedDouble(a, "mainAmpPan", s.mainAmpPan, -1.0, 1.0);
  detail::JsonGetClampedInt(a, "supportAmp", s.supportAmpIdx, -1, kAmpCount - 1);
  detail::JsonGetClampedInt(a, "supportSpeaker", s.supportSpeakerIdx, 0, 3);
  detail::JsonGetClampedInt(a, "supportChannel", s.supportChannelIdx, 0, 127);
  detail::JsonGetClampedDouble(a, "supportInput", s.supportInputLevel, -20.0, 20.0);
  detail::JsonGetClampedDouble(a, "supportGate", s.supportGateThreshold, -100.0, 0.0);
  detail::JsonGetClampedDouble(a, "supportBass", s.supportToneBass, 0.0, 10.0);
  detail::JsonGetClampedDouble(a, "supportMid", s.supportToneMid, 0.0, 10.0);
  detail::JsonGetClampedDouble(a, "supportTreble", s.supportToneTreble, 0.0, 10.0);
  detail::JsonGetClampedDouble(a, "supportOutput", s.supportOutputLevel, -40.0, 10.0);
  detail::JsonGetBool(a, "supportNoiseGate", s.supportNoiseGateActive);
  detail::JsonGetBool(a, "supportEq", s.supportEqActive);
  detail::JsonGetClampedDouble(a, "supportPan", s.supportAmpPan, -1.0, 1.0);
  detail::JsonGetBool(a, "supportPolarityInvert", s.supportPolarityInvert);
  // 1.2.0: custom SUPPORT partner binding + its own cab/channel.
  if (a.contains("supportCustomId") && a["supportCustomId"].is_string())
    s.supportCustomId = a["supportCustomId"].get<std::string>();
  detail::JsonGetClampedInt(a, "supportCustomSlot", s.supportCustomSlot, -2, 2);
  detail::JsonGetClampedInt(a, "supportCustomChannel", s.supportCustomChannel, 0, 8);
  (void)d;
}

inline nlohmann::json AmpSettingsToJson(const VoLumAmpSettings& s)
{
  nlohmann::json a;
  WriteAmpCoreBlock(a, s);
  a.update(PreBlockToJson(s));
  WriteDualAmpUserSettings(a, s);
  a.update(PostBlockToJson(s));
  a["activeIrId"] = s.activeIrId;
  a["supportActiveIrId"] = s.supportActiveIrId;
  // supportCustomId + custom support cab/channel are written by
  // WriteDualAmpUserSettings above (single source of truth).
  return a;
}

// Reads into `s` (already holding defaults). Out-of-range / wrong-type fields
// fall back to defaults. Returns true if anything was healed.
inline bool AmpSettingsFromJson(const nlohmann::json& a, VoLumAmpSettings& s)
{
  if (!a.is_object())
    return false;
  bool healed = ReadAmpCoreBlock(a, s);
  PreBlockFromJson(a, s);
  ReadDualAmpBlock(a, s);
  PostBlockFromJson(a, s);
  if (a.contains("activeIrId") && a["activeIrId"].is_string())
    s.activeIrId = a["activeIrId"].get<std::string>();
  if (a.contains("supportActiveIrId") && a["supportActiveIrId"].is_string())
    s.supportActiveIrId = a["supportActiveIrId"].get<std::string>();
  // supportCustomId + custom support cab/channel are read by ReadDualAmpBlock.
  return healed;
}

// The apply path heals !postValid to true before snapshotting (factory POST
// defaults, then _VolumSavePostToSlot). Reconstructors that copy the shipped
// bank as the dirty baseline must apply the same heal; the bank keeps
// postValid=false as the "never written" sentinel for restore.
inline VoLumAmpSettings HealedFactoryPresetSettings(VoLumAmpSettings s)
{
  s.postValid = true;
  return s;
}

namespace detail
{
template <typename Snapshot, std::size_t N, typename Same>
bool SnapshotsEqual(const Snapshot (&a)[N], const Snapshot (&b)[N], Same same)
{
  for (std::size_t i = 0; i < N; ++i)
    if (!same(a[i], b[i]))
      return false;
  return true;
}
} // namespace detail

// Value-equality over the sounding settings. Used by the F5 preset
// "(unsaved)" indicator: the live scene is dirty iff it differs from the
// recalled snapshot, so an A/B edit that lands back on the preset clears the
// flag. It compares exactly the fields AmpSettingsToJson writes, block by
// block, with the exact double == the JSON tree compare it replaces used (that
// compare built two nlohmann trees on every PLAY tick and UI param event). A
// codec field missing here fails the "agrees with the JSON tree compare" case in
// test_volum_user_settings_io.cpp, which keeps the JSON compare as its oracle.
//
// postValid is excluded: it is a restore sentinel ("POST was never written"),
// not a knob. Live save always stamps it true; a shipped Factory snapshot may
// not. Including it made every relaunched Factory preset read as (unsaved).
inline bool AmpSettingsEqual(const VoLumAmpSettings& a, const VoLumAmpSettings& b)
{
  // WriteAmpCoreBlock
  if (!(a.speakerIdx == b.speakerIdx && a.channelIdx == b.channelIdx && a.inputLevel == b.inputLevel
        && a.gateThreshold == b.gateThreshold && a.toneBass == b.toneBass && a.toneMid == b.toneMid
        && a.toneTreble == b.toneTreble && a.outputLevel == b.outputLevel && a.noiseGateActive == b.noiseGateActive
        && a.eqActive == b.eqActive))
    return false;
  // PreBlockToJson
  if (!(a.preCompActive == b.preCompActive && a.preCompAmount == b.preCompAmount && a.preCompRatio == b.preCompRatio
        && a.preCompAttack == b.preCompAttack && a.preCompRelease == b.preCompRelease && a.preCompMix == b.preCompMix
        && a.preCompLevel == b.preCompLevel))
    return false;
  if (!(a.preNam1Active == b.preNam1Active && a.preNam1Capture == b.preNam1Capture && a.preNam1Gain == b.preNam1Gain
        && a.preNam1Bass == b.preNam1Bass && a.preNam1Mid == b.preNam1Mid && a.preNam1MidFreq == b.preNam1MidFreq
        && a.preNam1Treble == b.preNam1Treble && a.preNam1Level == b.preNam1Level))
    return false;
  if (!(a.preNam2Active == b.preNam2Active && a.preNam2Capture == b.preNam2Capture && a.preNam2Gain == b.preNam2Gain
        && a.preNam2Bass == b.preNam2Bass && a.preNam2Mid == b.preNam2Mid && a.preNam2MidFreq == b.preNam2MidFreq
        && a.preNam2Treble == b.preNam2Treble && a.preNam2Level == b.preNam2Level))
    return false;
  if (!(a.prePitchActive == b.prePitchActive && a.prePitchMode == b.prePitchMode
        && a.prePitchSemitones == b.prePitchSemitones && a.prePitchMix == b.prePitchMix
        && a.prePitchOctDown == b.prePitchOctDown && a.prePitchOctUp == b.prePitchOctUp
        && a.prePitchDry == b.prePitchDry && a.prePitchVoicing == b.prePitchVoicing
        && a.prePitchLevel == b.prePitchLevel && a.prePitchTransChar == b.prePitchTransChar))
    return false;
  if (!detail::SnapshotsEqual(
        a.prePitchModes, b.prePitchModes, [](const PitchModeSnapshot& x, const PitchModeSnapshot& y) {
          return x.mix == y.mix && x.dry == y.dry && x.level == y.level && x.voicing == y.voicing;
        }))
    return false;
  // WriteDualAmpUserSettings
  if (!(a.dualAmpActive == b.dualAmpActive && a.dualAmpRoute == b.dualAmpRoute && a.mainAmpPan == b.mainAmpPan
        && a.supportAmpIdx == b.supportAmpIdx && a.supportSpeakerIdx == b.supportSpeakerIdx
        && a.supportChannelIdx == b.supportChannelIdx && a.supportInputLevel == b.supportInputLevel
        && a.supportGateThreshold == b.supportGateThreshold && a.supportToneBass == b.supportToneBass
        && a.supportToneMid == b.supportToneMid && a.supportToneTreble == b.supportToneTreble
        && a.supportOutputLevel == b.supportOutputLevel && a.supportNoiseGateActive == b.supportNoiseGateActive
        && a.supportEqActive == b.supportEqActive && a.supportAmpPan == b.supportAmpPan
        && a.supportPolarityInvert == b.supportPolarityInvert && a.supportCustomId == b.supportCustomId
        && a.supportCustomSlot == b.supportCustomSlot && a.supportCustomChannel == b.supportCustomChannel))
    return false;
  // PostBlockToJson, without postValid
  if (!(a.postDelayActive == b.postDelayActive && a.postDelayTime == b.postDelayTime
        && a.postDelayFeedback == b.postDelayFeedback && a.postDelayMix == b.postDelayMix
        && a.postDelayMode == b.postDelayMode && a.postDelayTone == b.postDelayTone && a.postDelayAge == b.postDelayAge
        && a.postDelayPingPong == b.postDelayPingPong && a.postDelaySync == b.postDelaySync
        && a.postDelayDivision == b.postDelayDivision))
    return false;
  if (!(a.postReverbActive == b.postReverbActive && a.postReverbMix == b.postReverbMix
        && a.postReverbDecay == b.postReverbDecay && a.postReverbTone == b.postReverbTone
        && a.postReverbPreDelay == b.postReverbPreDelay && a.postReverbShimmer == b.postReverbShimmer
        && a.postReverbMode == b.postReverbMode && a.postReverbSubMode == b.postReverbSubMode))
    return false;
  if (!(a.postTremoloActive == b.postTremoloActive && a.postTremoloMode == b.postTremoloMode
        && a.postTremoloRate == b.postTremoloRate && a.postTremoloDepth == b.postTremoloDepth
        && a.postTremoloShape == b.postTremoloShape && a.postTremoloMix == b.postTremoloMix
        && a.postTremoloCrossover == b.postTremoloCrossover && a.postTremoloSync == b.postTremoloSync
        && a.postTremoloDivision == b.postTremoloDivision))
    return false;
  if (!(a.postChorusActive == b.postChorusActive && a.postChorusMode == b.postChorusMode
        && a.postChorusRate == b.postChorusRate && a.postChorusDepth == b.postChorusDepth
        && a.postChorusTone == b.postChorusTone && a.postChorusWidth == b.postChorusWidth
        && a.postChorusMix == b.postChorusMix))
    return false;
  if (!detail::SnapshotsEqual(
        a.postDelayModes, b.postDelayModes, [](const DelayModeSnapshot& x, const DelayModeSnapshot& y) {
          return x.time == y.time && x.feedback == y.feedback && x.mix == y.mix && x.tone == y.tone && x.age == y.age
                 && x.pingPong == y.pingPong;
        }))
    return false;
  if (!detail::SnapshotsEqual(
        a.postReverbModes, b.postReverbModes, [](const ReverbModeSnapshot& x, const ReverbModeSnapshot& y) {
          return x.mix == y.mix && x.decay == y.decay && x.tone == y.tone && x.preDelay == y.preDelay
                 && x.shimmer == y.shimmer && x.subMode == y.subMode;
        }))
    return false;
  if (!detail::SnapshotsEqual(a.postOktaverbSubModes, b.postOktaverbSubModes,
                              [](const OktaverbSubModeSnapshot& x, const OktaverbSubModeSnapshot& y) {
                                return x.mix == y.mix && x.decay == y.decay && x.tone == y.tone
                                       && x.preDelay == y.preDelay && x.shimmer == y.shimmer;
                              }))
    return false;
  if (!detail::SnapshotsEqual(
        a.postTremoloModes, b.postTremoloModes, [](const TremoloModeSnapshot& x, const TremoloModeSnapshot& y) {
          return x.rate == y.rate && x.depth == y.depth && x.shape == y.shape && x.mix == y.mix
                 && x.crossover == y.crossover;
        }))
    return false;
  if (!detail::SnapshotsEqual(
        a.postChorusModes, b.postChorusModes, [](const ChorusModeSnapshot& x, const ChorusModeSnapshot& y) {
          return x.rate == y.rate && x.depth == y.depth && x.tone == y.tone && x.width == y.width && x.mix == y.mix;
        }))
    return false;
  // AmpSettingsToJson's own id fields
  return a.activeIrId == b.activeIrId && a.supportActiveIrId == b.supportActiveIrId;
}

// SetList blanks the selection. It must not force the dirty bit off:
// Manage-delete of the selected User preset forgets the id, Refresh calls
// SetList, and Default would then discard the live sound the confirm promised
// to keep.
inline bool PresetBarSetListPreservesDirty()
{
  return true;
}

// Even with nothing selected: Forget after Manage-delete leaves the live
// sound, and LivePresetDirty(false, live, {}) is the bit Save As / Default need.
inline bool PresetBarNeedsDirtyRecompute(bool /*hasSelectedPreset*/)
{
  return true;
}

// ---------------------------------------------------------------------------
// Per-instance custom-amp scenes (1.3.0)
// ---------------------------------------------------------------------------
//
// Until 1.3.0 a focused custom amp's live knobs lived in the shared content
// library ("customScenes"), which made every catalog write a rewrite of every
// other instance's sounding rig: saving a preset in one DAW track could move the
// knobs on another. Custom amps behave like factory amps now - the scene belongs
// to the VoLum instance, so it travels in the DAW chunk (id tail) and, for the
// standalone, in volum-settings.json.
//
// Keyed by custom-amp library id. One codec for both carriers so a scene written
// by the standalone and one restored from a project cannot drift apart.
inline nlohmann::json CustomScenesToJson(const std::map<std::string, VoLumAmpSettings>& byAmpId)
{
  nlohmann::json j = nlohmann::json::object();
  for (const auto& entry : byAmpId)
    if (!entry.first.empty())
      j[entry.first] = AmpSettingsToJson(entry.second);
  return j;
}

// Tolerant: a wrong-typed entry is dropped rather than failing the load, because
// the cost of refusing is every other scene in the file.
inline std::map<std::string, VoLumAmpSettings> CustomScenesFromJson(const nlohmann::json& j)
{
  std::map<std::string, VoLumAmpSettings> out;
  if (!j.is_object())
    return out;
  for (auto it = j.begin(); it != j.end(); ++it)
  {
    if (it.key().empty() || !it.value().is_object())
      continue;
    VoLumAmpSettings s;
    AmpSettingsFromJson(it.value(), s);
    out[it.key()] = s;
  }
  return out;
}

} // namespace volum
