#pragma once

#include "VoLumTriptychState.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace volum::keyboard
{
constexpr int kTargetCount = 8;

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
  kSupportToneTreble, kSupportOutputLevel, kSupportAmpPan,
};

constexpr std::array<int, 5> kDelayParams = {
  kDelayTime, kDelayFeedback, kDelayMix, kDelayTone, kDelayAge,
};

constexpr std::array<int, 4> kReverbParams = {
  kReverbMix, kReverbDecay, kReverbTone, kReverbPreDelay,
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
  kPreCompAmount, kPreCompAttack, kPreCompRelease, kPreCompLevel,
};

constexpr std::array<int, 4> kPitchTransposeParams = {
  kPrePitchSemitones, kPrePitchMix, kPrePitchLevel, kPrePitchQuality,
};

constexpr std::array<int, 5> kPitchOctaverParams = {
  kPrePitchOctDown, kPrePitchOctUp, kPrePitchDry, kPrePitchLevel, kPrePitchQuality,
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
    case kSupportOutputLevel:
      return fine ? 0.1 : 0.5;
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
    case kSupportToneTreble:
      return fine ? 0.1 : 0.5;
    case kDelayTime:
    case kReverbPreDelay:
    case kPreNam1MidFreq:
    case kPreNam2MidFreq:
    case kPreCompAttack:
    case kPreCompRelease:
      return fine ? 1.0 : 5.0;
    case kPrePitchSemitones:
      return 1.0;
    case kPrePitchMix:
    case kPrePitchOctDown:
    case kPrePitchOctUp:
    case kPrePitchDry:
    case kPrePitchQuality:
      return fine ? 0.01 : 0.05;
    case kPrePitchLevel:
      return fine ? 0.1 : 0.5;
    case kDelayFeedback:
    case kDelayMix:
    case kDelayTone:
    case kDelayAge:
    case kReverbMix:
    case kReverbDecay:
    case kReverbShimmer:
    case kPreCompMix:
      return fine ? 0.01 : 0.05;
    default:
      return fine ? 0.1 : 1.0;
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
} // namespace volum::keyboard
