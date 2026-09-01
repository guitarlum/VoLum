#pragma once

// PLAY art illumination: idle lamp pulse when quiet, IN-meter brightness when
// playing, frame corona only. No full-panel gold wash.

#include <algorithm>
#include <cmath>

namespace volum
{

inline float PlayIdlePulse(float phase)
{
  return 0.5f + 0.5f * std::sin(phase);
}

// inPeak is 0..1 from the existing input meter sender. Below the noise floor
// the stage breathes; above it the lamps follow the guitar.
inline float PlayArtBrightness(float inPeak, float idlePulse, float playingFloor = 0.03f)
{
  const float peak = std::clamp(inPeak, 0.f, 1.f);
  if (peak < playingFloor)
    return 0.36f + 0.16f * idlePulse;
  return 0.48f + 0.52f * peak;
}

inline float PlayCoronaOpacity(float brightness)
{
  return 0.035f + 0.11f * std::clamp(brightness, 0.f, 1.f);
}

} // namespace volum
