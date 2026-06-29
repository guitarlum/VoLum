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

// Value-equality over the full serialized settings. Used by the F5 preset
// "(unsaved)" indicator: the live scene is dirty iff it differs from the
// recalled snapshot, so an A/B edit that lands back on the preset clears the
// flag. Comparing the canonical JSON keeps this in lock-step with the codec
// (any field the codec round-trips participates in equality automatically).
inline bool AmpSettingsEqual(const VoLumAmpSettings& a, const VoLumAmpSettings& b)
{
  return AmpSettingsToJson(a) == AmpSettingsToJson(b);
}

} // namespace volum
