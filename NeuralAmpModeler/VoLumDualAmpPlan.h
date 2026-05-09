#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace volum
{

// Support lane defaults to unity gain so it matches the main amp's loudness when picked.
// Earlier builds shipped -6 dB as a "safety headroom" default, but users found it confusing
// and harder to A/B against the main amp; per-NAM-model loudness differences are the only
// remaining source of imbalance and are corrected with the user-facing OUTPUT knob.
inline constexpr double kDualAmpDefaultLaneLevelDb = 0.0;

enum class DualAmpRoute
{
  Stack = 0,
  LeftRight = 1,
  Custom = 2,
};

struct DualAmpPanGains
{
  double mainLeft = 1.0;
  double mainRight = 1.0;
  double supportLeft = 1.0;
  double supportRight = 1.0;
};

struct DualAmpLatencyCompensation
{
  int mainDelaySamples = 0;
  int supportDelaySamples = 0;
};

inline double ClampPan(double pan)
{
  return std::clamp(pan, -1.0, 1.0);
}

inline DualAmpRoute ClampDualAmpRoute(int route)
{
  switch (route)
  {
    case 0: return DualAmpRoute::Stack;
    case 1: return DualAmpRoute::LeftRight;
    case 2: return DualAmpRoute::Custom;
    default: return DualAmpRoute::Stack;
  }
}

inline DualAmpLatencyCompensation MakeDualAmpLatencyCompensation(int mainLatencySamples, int supportLatencySamples)
{
  const int mainLatency = std::max(0, mainLatencySamples);
  const int supportLatency = std::max(0, supportLatencySamples);
  const int targetLatency = std::max(mainLatency, supportLatency);
  return {targetLatency - mainLatency, targetLatency - supportLatency};
}

inline void ConstantPowerPan(double pan, double& left, double& right)
{
  const double clamped = ClampPan(pan);
  const double t = (clamped + 1.0) * 0.5;
  constexpr double kHalfPi = 1.57079632679489661923;
  left = std::cos(t * kHalfPi);
  right = std::sin(t * kHalfPi);
}

inline DualAmpPanGains MakeDualAmpPanGains(DualAmpRoute route, double mainPan, double supportPan)
{
  DualAmpPanGains gains;
  switch (route)
  {
    case DualAmpRoute::Stack:
      ConstantPowerPan(0.0, gains.mainLeft, gains.mainRight);
      ConstantPowerPan(0.0, gains.supportLeft, gains.supportRight);
      break;
    case DualAmpRoute::LeftRight:
      ConstantPowerPan(-1.0, gains.mainLeft, gains.mainRight);
      ConstantPowerPan(1.0, gains.supportLeft, gains.supportRight);
      break;
    case DualAmpRoute::Custom:
      ConstantPowerPan(mainPan, gains.mainLeft, gains.mainRight);
      ConstantPowerPan(supportPan, gains.supportLeft, gains.supportRight);
      break;
  }
  return gains;
}

template<typename Sample>
// Merge dual-amp main/support lanes to stereo. Identical behavior in standalone and DAW:
// no clamp here. Final-bus bounding is the master safety stage at the end of ProcessBlock
// (see VoLumMasterSafety.h); clamping mid-chain before Delay/Reverb would have been
// ineffective and asymmetric.
inline void MergeDualAmpToStereo(const Sample* mainMono, const Sample* supportMono, Sample* const* outputs,
                                 std::size_t nFrames, std::size_t nChansOut, double mainLevel, double supportLevel,
                                 const DualAmpPanGains& panGains, bool /*appApi*/)
{
  if (nChansOut == 0)
    return;

  for (std::size_t s = 0; s < nFrames; ++s)
  {
    const double main = static_cast<double>(mainMono[s]) * mainLevel;
    const double support = static_cast<double>(supportMono[s]) * supportLevel;
    const double left = main * panGains.mainLeft + support * panGains.supportLeft;
    const double right = main * panGains.mainRight + support * panGains.supportRight;

    for (std::size_t c = 0; c < nChansOut; ++c)
    {
      const double y = (c == 0) ? left : right;
      outputs[c][s] = static_cast<Sample>(y);
    }
  }
}

template<typename Sample>
class DualAmpDelayLine
{
public:
  void Reset()
  {
    mState.clear();
    mDelaySamples = 0;
    mWriteIndex = 0;
  }

  const Sample* Process(const Sample* input, Sample* output, std::size_t nFrames, int delaySamples)
  {
    if (delaySamples <= 0)
    {
      Reset();
      return input;
    }

    if (mDelaySamples != delaySamples)
    {
      mState.assign(static_cast<std::size_t>(delaySamples), static_cast<Sample>(0));
      mDelaySamples = delaySamples;
      mWriteIndex = 0;
    }

    for (std::size_t s = 0; s < nFrames; ++s)
    {
      output[s] = mState[mWriteIndex];
      mState[mWriteIndex] = input[s];
      mWriteIndex = (mWriteIndex + 1) % mState.size();
    }

    return output;
  }

private:
  std::vector<Sample> mState;
  int mDelaySamples = 0;
  std::size_t mWriteIndex = 0;
};

} // namespace volum
