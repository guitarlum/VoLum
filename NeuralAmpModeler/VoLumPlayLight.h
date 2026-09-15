#pragma once

// PLAY art illumination: idle lamp pulse when quiet, IN-meter brightness when
// playing, frame corona only. No full-panel gold wash.
// PLAY meters share BUILD's −70..0 dB window (NAMMeterControl).

#include <algorithm>
#include <cmath>

namespace volum
{

inline constexpr float kPlayMeterDbMin = -70.0f;
inline constexpr float kPlayMeterDbMax = -0.01f;
// Old 0.03 linear floor was −30.5 dB; same point on the shared dB-norm.
inline constexpr float kPlayPlayingFloorNorm = 0.56f;

inline float MeterNormFromLinear(float linearPeak)
{
  const float peak = std::max(linearPeak, 1e-9f);
  const float db = 20.f * std::log10(peak);
  return std::clamp((db - kPlayMeterDbMin) / (kPlayMeterDbMax - kPlayMeterDbMin), 0.f, 1.f);
}

inline float PlayIdlePulse(float phase)
{
  return 0.5f + 0.5f * std::sin(phase);
}

// One-pole toward the dB-norm. Attack ~200 ms / release ~600 ms at 60 Hz.
inline float PlayLampFollow(float current, float target, float attack = 0.15f, float release = 0.04f)
{
  const float coeff = target > current ? attack : release;
  return current + (target - current) * coeff;
}

// inPeak is 0..1 dB-norm (same window as BUILD). Lamps follow a smoothed
// envelope, not the raw ladder sample. Blend across the playing floor so
// brightness does not step down when a quiet note starts.
inline float PlayArtBrightness(float inPeak, float idlePulse, float playingFloor = kPlayPlayingFloorNorm)
{
  const float peak = std::clamp(inPeak, 0.f, 1.f);
  const float idle = 0.36f + 0.28f * idlePulse;
  const float playing = 0.64f + 0.36f * peak;
  const float blend = std::clamp((peak - playingFloor) / 0.08f, 0.f, 1.f);
  return idle + (playing - idle) * blend;
}

inline float PlayCoronaOpacity(float brightness)
{
  return 0.035f + 0.11f * std::clamp(brightness, 0.f, 1.f);
}

} // namespace volum
