#pragma once

// PLAY art motion input. Pure (no IGraphics), doctested in test_volum_art_anim.cpp.
//
// The PLAY light (VoLumPlayLight.h) already turns the input into a smoothed
// `energy` and a pick `attack`; motion reads the same two numbers so glow and
// motion always agree. On top of them this adds what an animation needs and a
// light does not: a motion clock that only runs while sounding, and the age of
// the last few picks so an art can spawn an event per pick.
//
// Rest rule: ArtMotion::IsRest() means silence. An animator must then draw its
// static art (the BUILD hero) within 2/255 per channel, for any clock value.

#include "../VoLumPlayLight.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace volumart
{
inline constexpr float kNoOnset = 1e9f;
// Pick-spawned effects must be gone by this age; IsRest waits for it.
inline constexpr float kMaxEventAge = 1.5f;
inline constexpr int kOnsetSlots = 4;
// A pick is the attack envelope crossing this, at most once per refractory time.
inline constexpr float kOnsetAttack = 0.4f;
inline constexpr float kOnsetRearm = 0.2f;
inline constexpr float kOnsetRejump = 0.3f;
inline constexpr float kOnsetRefractory = 0.09f;
// Motion seconds per wall second: kClockRateFloor at the lightest touch, 1 at full.
inline constexpr float kClockRateFloor = 0.35f;
inline constexpr float kMaxMotionDt = 0.05f;

struct ArtMotion
{
  double clock = 0.0; // motion seconds; advances only while sounding
  float energy = 0.f; // 0..1 sustained level (PlayLight::energy)
  float pick = 0.f; // 0..1 attack envelope, ~120 ms decay (PlayLight::attack)
  float bloom = 0.f; // additive weight of the PLAY bloom on the base art (PlayBloomWeight); 0 at rest
  float onsetAge[kOnsetSlots] = {kNoOnset, kNoOnset, kNoOnset, kNoOnset}; // s since the last picks, newest first
  uint32_t onsets = 0; // running pick count: per-pick variation = Hash01(onsets * 747796405u + seed)
  uint32_t seed = 0; // fractal case 0..14, or 100 + custom style

  bool IsRest() const { return energy <= 0.f && pick <= 0.f && onsetAge[0] >= kMaxEventAge; }
  // 0 at rest, 1 once anything is sounding: the weight a crossfade-to-live gives the live element.
  float Wake() const;
};

struct ArtMotionState
{
  ArtMotion m;
  float prevAttack = 0.f;
  float sinceOnset = kNoOnset;
  bool armed = true;
};

inline float Smoothstep(float e0, float e1, float x)
{
  if (e1 == e0)
    return x < e0 ? 0.f : 1.f;
  const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

inline float ArtMotion::Wake() const
{
  return Smoothstep(0.f, 0.25f, std::max(energy, pick));
}

inline float Hash01(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return static_cast<float>(x >> 8) * (1.f / 16777216.f);
}

// Smooth value noise in [-0.5, 0.5]; one independent curve per seed.
inline float Noise1(float x, uint32_t seed)
{
  const float fl = std::floor(x);
  const float f = x - fl;
  const int32_t i = static_cast<int32_t>(fl);
  const float a = Hash01(static_cast<uint32_t>(i) * 2654435761u ^ (seed * 97u + 13u));
  const float b = Hash01(static_cast<uint32_t>(i + 1) * 2654435761u ^ (seed * 97u + 13u));
  const float s = f * f * (3.f - 2.f * f);
  return a + (b - a) * s - 0.5f;
}

inline ArtMotion RestMotion(uint32_t seed)
{
  ArtMotion m;
  m.seed = seed;
  return m;
}

// One PLAY tick. energy / attack / bloom come from the PLAY light; dt is wall
// seconds since the last tick (clamped, so a stalled frame cannot fling the clock).
inline void AdvanceArtMotion(ArtMotionState& s, float energy, float attack, float bloom, float dt)
{
  dt = std::clamp(dt, 0.f, kMaxMotionDt);
  ArtMotion& m = s.m;
  m.energy = std::clamp(energy, 0.f, 1.f);
  m.pick = std::clamp(attack, 0.f, 1.f);
  m.bloom = std::max(bloom, 0.f);
  for (float& age : m.onsetAge)
    age = std::min(age + dt, kNoOnset);
  s.sinceOnset = std::min(s.sinceOnset + dt, kNoOnset);
  // Re-arm once the envelope has fallen away, or when a new pick lifts it again
  // while it is still decaying from the last one.
  if (m.pick < kOnsetRearm || m.pick - s.prevAttack >= kOnsetRejump)
    s.armed = true;
  if (s.armed && m.pick >= kOnsetAttack && s.sinceOnset >= kOnsetRefractory)
  {
    for (int k = kOnsetSlots - 1; k > 0; --k)
      m.onsetAge[k] = m.onsetAge[k - 1];
    m.onsetAge[0] = 0.f;
    ++m.onsets;
    s.sinceOnset = 0.f;
    s.armed = false;
  }
  s.prevAttack = m.pick;
  const float a = std::max(m.energy, m.pick);
  if (a > 0.f)
    m.clock += static_cast<double>(dt) * (kClockRateFloor + (1.f - kClockRateFloor) * a);
}

// VOLUM_ART_ANIM_DEBUG=<art>[:<energy>|off[:<clock>|run[:<pick>]]]
//   art    0..14 = factory amp index (kAmps order), c0..c5 = custom art style
//   energy 0..1, or off = the static layer (the toggle-off path)
//   clock  motion seconds (default 0), or run = advance live
//   pick   0..1 (default 0); > 0 also stamps one pick 0.05 s ago
struct ArtAnimDebug
{
  bool on = false;
  bool legacy = false;
  bool custom = false;
  int art = 0;
  float energy = 0.f;
  double clock = 0.0;
  bool runClock = false;
  float pick = 0.f;
};

inline ArtAnimDebug ParseArtAnimDebug(const char* env)
{
  ArtAnimDebug d;
  if (!env || !*env)
    return d;
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%s", env);
  char* fields[4] = {nullptr, nullptr, nullptr, nullptr};
  int n = 0;
  for (char* p = buf;;)
  {
    if (n == 4)
      return d; // a fifth field
    fields[n++] = p;
    char* colon = std::strchr(p, ':');
    if (!colon)
      break;
    *colon = '\0';
    p = colon + 1;
  }
  auto number = [](const char* text, double& out) {
    if (!text || !*text)
      return false;
    char* end = nullptr;
    out = std::strtod(text, &end);
    return end != text && *end == '\0' && std::isfinite(out);
  };
  const char* art = fields[0];
  if (art[0] == 'c' || art[0] == 'C')
  {
    d.custom = true;
    ++art;
  }
  double v = 0.0;
  if (!number(art, v) || v < 0.0 || v != std::floor(v))
    return d;
  d.art = d.custom ? static_cast<int>(std::min(v, 5.0)) : static_cast<int>(std::min(v, 14.0));
  if (n >= 2)
  {
    if (std::strcmp(fields[1], "off") == 0)
      d.legacy = true;
    else if (number(fields[1], v))
      d.energy = static_cast<float>(std::clamp(v, 0.0, 1.0));
    else
      return d;
  }
  if (n >= 3)
  {
    if (std::strcmp(fields[2], "run") == 0)
      d.runClock = true;
    else if (number(fields[2], v) && v >= 0.0)
      d.clock = v;
    else
      return d;
  }
  if (n >= 4)
  {
    if (!number(fields[3], v))
      return d;
    d.pick = static_cast<float>(std::clamp(v, 0.0, 1.0));
  }
  d.on = true;
  return d;
}

// The PLAY light a debug frame stands for, so glow, corona and bloom match the motion.
inline volum::PlayLight DebugPlayLight(const ArtAnimDebug& d)
{
  volum::PlayLight light;
  light.energy = d.energy;
  light.attack = d.pick;
  light.lamp = d.energy > 0.f
                 ? volum::kPlayEnergyFloorNorm + d.energy * (volum::kPlayEnergyFullNorm - volum::kPlayEnergyFloorNorm)
                 : 0.f;
  return light;
}

inline uint32_t ArtMotionSeed(bool custom, int caseOrStyle)
{
  return custom ? 100u + static_cast<uint32_t>(caseOrStyle) : static_cast<uint32_t>(caseOrStyle);
}

// The frame a debug spec describes. runClock is the live clock (only read when d.runClock).
inline ArtMotion DebugArtMotion(const ArtAnimDebug& d, uint32_t seed, double runClock)
{
  ArtMotion m = RestMotion(seed);
  m.energy = d.energy;
  m.pick = d.pick;
  m.clock = d.runClock ? runClock : d.clock;
  m.bloom = m.energy > 0.f || m.pick > 0.f ? volum::PlayBloomWeight(volum::PlayGlowAmount(DebugPlayLight(d))) : 0.f;
  if (d.pick > 0.f)
  {
    m.onsetAge[0] = 0.05f;
    m.onsets = 1;
  }
  return m;
}

// Live frame: what AdvanceArtMotion produced, snapped to the exact rest frame in
// silence so a stale bloom rounding or clock can never leak into the static art.
inline ArtMotion LiveArtMotion(const ArtMotionState& s, uint32_t seed)
{
  if (s.m.IsRest())
  {
    ArtMotion rest = RestMotion(seed);
    rest.clock = s.m.clock;
    rest.onsets = s.m.onsets;
    return rest;
  }
  ArtMotion m = s.m;
  m.seed = seed;
  return m;
}
} // namespace volumart
