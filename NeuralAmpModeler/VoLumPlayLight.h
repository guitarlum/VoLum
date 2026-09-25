#pragma once

// PLAY art illumination. Silence draws the art exactly as BUILD does: no veil,
// nothing on top. Playing adds bloom and a frame corona on top of that, and
// reaches its full look at a normal guitar level (~-12 dBFS), not near 0 dBFS.
// PLAY meters share BUILD's -70..0 dB window (NAMMeterControl).
//
// The smoothed `energy` and the `attack` envelope are the motion inputs for the
// PLAY art animation as well, so glow and motion always agree.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace volum
{

inline constexpr float kPlayMeterDbMin = -70.0f;
inline constexpr float kPlayMeterDbMax = -0.01f;
// Energy window on the dB-norm: 0 at or below -55 dBFS (interface and pickup
// noise stay under it), 1 from -18 dBFS up. A rolled-back volume pot or a chord
// ringing out still moves the art.
inline constexpr float kPlayEnergyFloorNorm = 0.2143f;
inline constexpr float kPlayEnergyFullNorm = 0.7429f;
// Lamp release per 60 Hz tick (~1 s), so a decaying chord holds the motion.
inline constexpr float kPlayLampRelease = 0.025f;
// Attack: how far the input leads the lamp, which lags a pick by design.
inline constexpr float kPlayAttackRiseMin = 0.04f;
inline constexpr float kPlayAttackRiseSpan = 0.16f;
// Per 60 Hz tick: exp(-(1/60) / 0.12 s).
inline constexpr float kPlayAttackDecay = 0.87f;
// Linear gain the bloom adds to the art at full glow.
inline constexpr float kPlayBloomGain = 0.45f;

inline float MeterNormFromDb(float db)
{
  return std::clamp((db - kPlayMeterDbMin) / (kPlayMeterDbMax - kPlayMeterDbMin), 0.f, 1.f);
}

inline float MeterNormFromLinear(float linearPeak)
{
  const float peak = std::max(linearPeak, 1e-9f);
  return MeterNormFromDb(20.f * std::log10(peak));
}

inline float PlayIdlePulse(float phase)
{
  return 0.5f + 0.5f * std::sin(phase);
}

// One-pole toward the dB-norm. Attack ~200 ms / release ~1 s at 60 Hz.
inline float PlayLampFollow(float current, float target, float attack = 0.15f, float release = kPlayLampRelease)
{
  const float coeff = target > current ? attack : release;
  return current + (target - current) * coeff;
}

inline float PlaySmoothstep(float x)
{
  x = std::clamp(x, 0.f, 1.f);
  return x * x * (3.f - 2.f * x);
}

// 0..1 sustained level from the smoothed lamp; exactly 0 below the floor.
inline float PlayLightEnergy(float lampNorm)
{
  return PlaySmoothstep((lampNorm - kPlayEnergyFloorNorm) / (kPlayEnergyFullNorm - kPlayEnergyFloorNorm));
}

inline float PlayAttackTarget(float inNorm, float lampNorm)
{
  if (inNorm < kPlayEnergyFloorNorm)
    return 0.f;
  return std::clamp((inNorm - lampNorm - kPlayAttackRiseMin) / kPlayAttackRiseSpan, 0.f, 1.f);
}

// The output may carry a reverb or delay tail for up to ~3 s after the input
// stops, never longer, and never on its own: the metronome click or a stray
// output level cannot wake the art.
inline constexpr float kPlayTailHoldStep = 1.f / 180.f;

// All zero is silence, which is the rest state BUILD shows.
struct PlayLight
{
  float lamp = 0.f; // smoothed input, dB-norm
  float energy = 0.f; // 0..1 sustained level
  float attack = 0.f; // 0..1 pick envelope, ~120 ms decay
  float outLamp = 0.f; // smoothed output, dB-norm
  float tailHold = 0.f; // how much of the output tail may still count
};

// One 60 Hz tick. inNorm / outNorm are the last block's input / output peaks on
// the dB-norm; the lamps follow a smoothed envelope, never the raw ladder sample.
// Picks and the sustained level come from the input (the player's dynamics; the
// output depends on amp gain and the Output knob). The output only extends it.
inline PlayLight AdvancePlayLight(const PlayLight& prev, float inNorm, float outNorm = 0.f)
{
  const float in = std::clamp(inNorm, 0.f, 1.f);
  PlayLight next;
  next.attack = std::max(PlayAttackTarget(in, prev.lamp), prev.attack * kPlayAttackDecay);
  if (next.attack < 1e-3f)
    next.attack = 0.f;
  next.lamp = PlayLampFollow(prev.lamp, in);
  const float inEnergy = PlayLightEnergy(next.lamp);
  next.outLamp = PlayLampFollow(prev.outLamp, std::clamp(outNorm, 0.f, 1.f));
  next.tailHold = std::max(inEnergy, prev.tailHold - kPlayTailHoldStep);
  if (next.tailHold < 1e-3f)
    next.tailHold = 0.f;
  next.energy = std::max(inEnergy, std::min(PlayLightEnergy(next.outLamp), next.tailHold));
  return next;
}

// Sustained energy plus a short lift on each pick.
inline float PlayGlowAmount(const PlayLight& light)
{
  return std::clamp(light.energy + 0.35f * light.attack * (1.f - light.energy), 0.f, 1.f);
}

// Weight for the additive re-draw of the art. EBlend::Add on NanoVG adds
// src * w^2 over the opaque framebuffer, so this is the square root of the gain.
inline float PlayBloomWeight(float glow)
{
  return std::sqrt(kPlayBloomGain * std::clamp(glow, 0.f, 1.f));
}

inline float PlayCoronaOpacity(float glow, float idlePulse)
{
  const float g = std::clamp(glow, 0.f, 1.f);
  return 0.05f + 0.03f * std::clamp(idlePulse, 0.f, 1.f) * (1.f - g) + 0.40f * g;
}

// Debug-only VOLUM_PLAY_FAKE_PEAK: "-12" or "-12dB" is dBFS, "0.83" is the
// dB-norm itself. Anything else (or no variable) leaves the live input alone.
inline bool ParsePlayFakePeak(const char* text, float& norm)
{
  if (!text || !*text)
    return false;
  char* end = nullptr;
  const float v = std::strtof(text, &end);
  if (end == text)
    return false;
  const bool db = (end[0] == 'd' || end[0] == 'D') && (end[1] == 'b' || end[1] == 'B') && end[2] == '\0';
  if (!db && *end != '\0')
    return false;
  if (db || v < 0.f)
  {
    norm = MeterNormFromDb(v);
    return true;
  }
  if (v > 1.f)
    return false;
  norm = v;
  return true;
}

} // namespace volum
