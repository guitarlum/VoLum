#pragma once

#include "VoLumParams.h"
#include "VoLumTriptychState.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace volum::keyboard
{
constexpr int kTargetCount = 10;

constexpr int TargetIndex(EVoLumEffectFocus focus, bool supportAmp)
{
  switch (focus)
  {
    case EVoLumEffectFocus::AMP: return supportAmp ? 1 : 0;
    case EVoLumEffectFocus::COMP: return 2;
    case EVoLumEffectFocus::PRE_NAM1: return 3;
    case EVoLumEffectFocus::PRE_NAM2: return 4;
    case EVoLumEffectFocus::DELAY: return 5;
    case EVoLumEffectFocus::REVERB: return 6;
    case EVoLumEffectFocus::PITCH: return 7;
    case EVoLumEffectFocus::TREMOLO: return 8;
    case EVoLumEffectFocus::CHORUS: return 9;
  }
  return 0;
}

constexpr std::array<int, 6> kMainAmpMonoParams = {
  kInputLevel, kNoiseGateThreshold, kToneBass, kToneMid, kToneTreble, kOutputLevel,
};

constexpr std::array<int, 7> kMainAmpDualParams = {
  kInputLevel, kNoiseGateThreshold, kToneBass, kToneMid, kToneTreble, kOutputLevel, kMainAmpPan,
};

constexpr std::array<int, 7> kSupportAmpParams = {
  kSupportInputLevel, kSupportNoiseGateThreshold, kSupportToneBass, kSupportToneMid,
  kSupportToneTreble, kSupportOutputLevel,        kSupportAmpPan,
};

constexpr std::array<int, 5> kDelayParams = {
  kDelayTime, kDelayFeedback, kDelayMix, kDelayTone, kDelayAge,
};

// TEMPO SYNC hides TIME and puts DIVISION in its slot. Keyboard remember / Left-Right
// have to walk this list or Enter lands on a control that is not on screen.
constexpr std::array<int, 4> kDelaySyncedParams = {
  kDelayFeedback,
  kDelayMix,
  kDelayTone,
  kDelayAge,
};

constexpr std::array<int, 4> kReverbParams = {
  kReverbMix,
  kReverbDecay,
  kReverbTone,
  kReverbPreDelay,
};

constexpr std::array<int, 5> kOktaverbParams = {
  kReverbMix, kReverbDecay, kReverbTone, kReverbPreDelay, kReverbShimmer,
};

constexpr std::array<int, 6> kPreNam1Params = {
  kPreNam1Gain, kPreNam1Bass, kPreNam1Mid, kPreNam1MidFreq, kPreNam1Treble, kPreNam1Level,
};

constexpr std::array<int, 6> kPreNam2Params = {
  kPreNam2Gain, kPreNam2Bass, kPreNam2Mid, kPreNam2MidFreq, kPreNam2Treble, kPreNam2Level,
};

constexpr std::array<int, 4> kCompParams = {
  kPreCompAmount,
  kPreCompAttack,
  kPreCompRelease,
  kPreCompLevel,
};

constexpr std::array<int, 4> kTremoloParams = {
  kTremoloRate,
  kTremoloDepth,
  kTremoloShape,
  kTremoloMix,
};

// Harmonic mode exposes the band-split CROSSOVER knob too.
constexpr std::array<int, 5> kTremoloHarmonicParams = {
  kTremoloRate, kTremoloDepth, kTremoloShape, kTremoloMix, kTremoloCrossover,
};

constexpr std::array<int, 3> kTremoloSyncedParams = {
  kTremoloDepth,
  kTremoloShape,
  kTremoloMix,
};

constexpr std::array<int, 4> kTremoloHarmonicSyncedParams = {
  kTremoloDepth,
  kTremoloShape,
  kTremoloMix,
  kTremoloCrossover,
};

inline int DefaultDelayKnob(bool tempoSync)
{
  return tempoSync ? kDelayFeedback : kDelayTime;
}

inline int DefaultTremoloKnob(bool tempoSync)
{
  return tempoSync ? kTremoloDepth : kTremoloRate;
}

constexpr std::array<int, 5> kChorusParams = {
  kChorusRate, kChorusDepth, kChorusTone, kChorusWidth, kChorusMix,
};

constexpr std::array<int, 3> kPitchTransposeParams = {
  kPrePitchSemitones,
  kPrePitchMix,
  kPrePitchLevel,
};

constexpr std::array<int, 4> kPitchOctaverParams = {
  kPrePitchOctDown,
  kPrePitchOctUp,
  kPrePitchDry,
  kPrePitchLevel,
};

inline double StepForParam(int paramIdx, bool fine)
{
  switch (paramIdx)
  {
    case kInputLevel:
    case kOutputLevel:
    case kPreCompLevel:
    case kPreNam1Gain:
    case kPreNam1Level:
    case kPreNam2Gain:
    case kPreNam2Level:
    case kSupportInputLevel:
    case kSupportOutputLevel: return fine ? 0.1 : 0.5;
    case kToneBass:
    case kToneMid:
    case kToneTreble:
    case kReverbTone:
    case kBoostTone:
    case kBoostDrive:
    case kPreNam1Bass:
    case kPreNam1Mid:
    case kPreNam1Treble:
    case kPreNam2Bass:
    case kPreNam2Mid:
    case kPreNam2Treble:
    case kSupportToneBass:
    case kSupportToneMid:
    case kSupportToneTreble: return fine ? 0.1 : 0.5;
    case kDelayTime:
    case kReverbPreDelay:
    case kPreNam1MidFreq:
    case kPreNam2MidFreq:
    case kPreCompAttack:
    case kPreCompRelease:
    case kTremoloCrossover: return fine ? 1.0 : 5.0;
    case kTremoloRate: return fine ? 0.1 : 0.5;
    case kPrePitchSemitones: return 1.0;
    case kPrePitchMix:
    case kPrePitchOctDown:
    case kPrePitchOctUp:
    case kPrePitchDry: return fine ? 0.01 : 0.05;
    case kPrePitchLevel: return fine ? 0.1 : 0.5;
    case kDelayFeedback:
    case kDelayMix:
    case kDelayTone:
    case kDelayAge:
    case kReverbMix:
    case kReverbDecay:
    case kReverbShimmer:
    case kPreCompMix:
    case kTremoloDepth:
    case kTremoloShape:
    case kTremoloMix:
    case kChorusRate:
    case kChorusDepth:
    case kChorusTone:
    case kChorusWidth:
    case kChorusMix:
    case kMainAmpPan:
    case kSupportAmpPan: return fine ? 0.01 : 0.05;
    default: return fine ? 0.1 : 1.0;
  }
}

struct WheelAccumulator
{
  int OnDelta(double delta)
  {
    if (delta == 0.0)
      return 0;

    if (mAccum != 0.0 && ((delta > 0.0) != (mAccum > 0.0)))
      mAccum = 0.0;

    mAccum += delta;

    if (mAccum >= 1.0 || mAccum <= -1.0)
    {
      const int steps = static_cast<int>(mAccum);
      mAccum -= static_cast<double>(steps);
      return steps;
    }

    return 0;
  }

  void Reset() { mAccum = 0.0; }

  double ResidualForTests() const { return mAccum; }

private:
  double mAccum = 0.0;
};

template <size_t N>
inline bool Contains(const std::array<int, N>& params, int paramIdx)
{
  return std::find(params.begin(), params.end(), paramIdx) != params.end();
}

// Windows VK codes (same values as iPlug kVK_*). Kept numeric so this header
// stays free of IGraphics.
inline constexpr int kKeyReturn = 0x0D;
inline constexpr int kKeyEscape = 0x1B;
inline constexpr int kKeyLeft = 0x25;
inline constexpr int kKeyUp = 0x26;
inline constexpr int kKeyRight = 0x27;
inline constexpr int kKeyDown = 0x28;

enum class OverlayId
{
  None = 0,
  Metronome,
  Tuner,
  NameDialog,
  Confirm,
  Custom,
  Pack,
  Settings,
  Dropdown,
};

struct OverlayStack
{
  bool exactEntry = false;
  bool textEntry = false;
  bool metronome = false;
  bool tuner = false;
  bool nameDialog = false;
  bool confirm = false;
  bool custom = false;
  bool pack = false;
  bool settings = false;
  bool settingsMidiArmed = false;
  bool dropdown = false;
  bool knobSelected = false;
};

enum class KeyKind
{
  Escape,
  HotkeyH,
  HotkeyT,
  HotkeyM,
  Arrow,
  Enter,
  Other,
};

enum class KeyConsumer
{
  PassToTextEntry,
  CancelExactEntry,
  CloseOverlay,
  PeelSettingsMidi,
  OverlayNav,
  Swallow,
  FallThrough,
  Knob,
  Rig,
};

inline OverlayId TopOverlay(const OverlayStack& s)
{
  // Attach order is z-order. Last attached sits on top and must peel first.
  if (s.metronome)
    return OverlayId::Metronome;
  if (s.tuner)
    return OverlayId::Tuner;
  if (s.nameDialog)
    return OverlayId::NameDialog;
  if (s.confirm)
    return OverlayId::Confirm;
  if (s.custom)
    return OverlayId::Custom;
  if (s.pack)
    return OverlayId::Pack;
  if (s.settings)
    return OverlayId::Settings;
  if (s.dropdown)
    return OverlayId::Dropdown;
  return OverlayId::None;
}

inline KeyKind ClassifyVk(int vk)
{
  if (vk == kKeyEscape)
    return KeyKind::Escape;
  if (vk == 'h' || vk == 'H')
    return KeyKind::HotkeyH;
  if (vk == 't' || vk == 'T')
    return KeyKind::HotkeyT;
  if (vk == 'm' || vk == 'M')
    return KeyKind::HotkeyM;
  if (vk == kKeyUp || vk == kKeyDown || vk == kKeyLeft || vk == kKeyRight)
    return KeyKind::Arrow;
  if (vk == kKeyReturn)
    return KeyKind::Enter;
  return KeyKind::Other;
}

// One function answers "who gets this key". A visible overlay blocks every
// global hotkey except Escape, which closes the topmost overlay. Repeating that
// condition at each call site is how H/T/M leaked through Manage and the tuner.
inline KeyConsumer RouteKey(const OverlayStack& s, KeyKind kind)
{
  if (kind == KeyKind::Escape && s.exactEntry)
    return KeyConsumer::CancelExactEntry;
  if (s.textEntry)
    return KeyConsumer::PassToTextEntry;

  const OverlayId top = TopOverlay(s);

  if (kind == KeyKind::Escape)
  {
    if (top == OverlayId::Settings && s.settingsMidiArmed)
      return KeyConsumer::PeelSettingsMidi;
    if (top != OverlayId::None)
      return KeyConsumer::CloseOverlay;
    if (s.knobSelected)
      return KeyConsumer::Knob;
    return KeyConsumer::Rig;
  }

  if (top == OverlayId::None)
  {
    if (kind == KeyKind::HotkeyH || kind == KeyKind::HotkeyT || kind == KeyKind::HotkeyM)
      return KeyConsumer::Rig;
    if (s.knobSelected)
      return KeyConsumer::Knob;
    return KeyConsumer::Rig;
  }

  if (kind == KeyKind::HotkeyH && (top == OverlayId::Settings || top == OverlayId::Pack))
    return KeyConsumer::CloseOverlay;
  if (kind == KeyKind::Arrow && top == OverlayId::Custom)
    return KeyConsumer::OverlayNav;
  if (kind == KeyKind::Enter && top == OverlayId::Confirm)
    return KeyConsumer::FallThrough;
  return KeyConsumer::Swallow;
}

inline bool SelectedKnobConsumesKind(KeyKind kind, bool innerHandled)
{
  if (kind == KeyKind::Arrow)
    return true;
  return innerHandled;
}

inline bool HideDisarmsMidiSubscreen(bool hiding, bool onList)
{
  return hiding && !onList;
}
} // namespace volum::keyboard
