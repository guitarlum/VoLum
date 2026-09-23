#pragma once

// Which PLAY arts move. Pure (no IGraphics), doctested in test_volum_art_anim.cpp.
//
// One row per factory fractal case, index == case. `animate` is the only switch
// an art needs: false and PLAY never creates that art's animator, so it draws
// the static cached art exactly as before. Turning one art on or off (or
// reverting it) is this one line plus that art's own file.

#include <cstdint>

namespace volumart
{
enum class ArtClass : uint8_t
{
  Additive, // full art cached, extras added on top
  Split, // art cut into cached passes, the moving element drawn live in between
  Transform // cached layer(s) drawn under an affine transform
};

struct ArtSpec
{
  int fractalCase;
  int ampIdx; // volum::kAmps index
  const char* amp; // volum::kAmps[ampIdx].displayName
  const char* motif;
  const char* file; // under NeuralAmpModeler/art/
  ArtClass kind;
  bool animate;
};

inline constexpr int kFactoryArtCount = 15;

// clang-format off
inline constexpr ArtSpec kArtSpecs[kFactoryArtCount] = {
  {0,  0,  "Ampete One",         "Dragon Nebula",    "VoLumArtAmpeteOne.h",     ArtClass::Additive,  true},
  {1,  1,  "Bad Cat Mini Cat",   "Eyes in the Dark", "VoLumArtBadCat.h",        ArtClass::Split,     true},
  {2,  2,  "Brunetti XL 2",      "Verdant Fern",     "VoLumArtBrunetti.h",      ArtClass::Transform, true},
  {3,  4,  "Fryette Deliv. 120", "Spiral Galaxy",    "VoLumArtFryette.h",       ArtClass::Split,     true},
  {4,  5,  "H&K TriAmp Mk2",     "Lissajous Nebula", "VoLumArtHkTriamp.h",      ArtClass::Additive,  true},
  {5,  6,  "Lichtlaerm Prom.",   "Koch Deep",        "VoLumArtLichtlaerm.h",    ArtClass::Additive,  true},
  {6,  7,  "Marshall 2204",      "Windswept Glow",   "VoLumArtMarshall2204.h",  ArtClass::Split,     false},
  {7,  8,  "Marshall JMP 2203",  "Depth Triforce",   "VoLumArtJmp2203.h",       ArtClass::Split,     false},
  {8,  9,  "Marshall JVM",       "Levy Nebula",      "VoLumArtJvm210.h",        ArtClass::Additive,  false},
  {9,  10, "Orange OD120",       "Ember Bulb",       "VoLumArtOrangeOd120.h",   ArtClass::Additive,  false},
  {10, 11, "Orange ORS100",      "Julia Nebula",     "VoLumArtOrangeOrs100.h",  ArtClass::Additive,  false},
  {11, 12, "Sebago Texas Fl.",   "Clifford Nebula",  "VoLumArtSebago.h",        ArtClass::Additive,  false},
  {12, 13, "Soldano SLO100",     "Beacon Sweep",     "VoLumArtSoldano.h",       ArtClass::Split,     true},
  {13, 14, "THC Sunset",         "Dark Sun",         "VoLumArtThcSunset.h",     ArtClass::Split,     false},
  {14, 3,  "Diezel Herbert Mk1", "Lichtenberg Glow", "VoLumArtDiezelHerbert.h", ArtClass::Split,     false},
};
// clang-format on

// Custom-amp arts share one generic slow turn + pulse (art/VoLumArtCustom.h).
inline constexpr bool kCustomArtAnimate = true;

constexpr bool ArtAnimates(int fractalCase)
{
  return fractalCase >= 0 && fractalCase < kFactoryArtCount && kArtSpecs[fractalCase].animate;
}
} // namespace volumart