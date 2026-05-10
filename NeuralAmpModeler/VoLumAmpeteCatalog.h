#pragma once

#include "config.h"

namespace volum
{
struct AmpInfo
{
  const char* folderName;
  const char* displayName;
};

inline constexpr int kAmpCount = 15;
inline constexpr AmpInfo kAmps[kAmpCount] = {
  {"Ampete One", "Ampete One"},
  {"Bad Cat mini Cat", "Bad Cat Mini Cat"},
  {"Brunetti XL 2", "Brunetti XL 2"},
  {"Diezel Herbert Mk1", "Diezel Herbert Mk1"},
  {"Fryette Deliverance 120", "Fryette Deliv. 120"},
  {"H&K TriAmp Mk2", "H&K TriAmp Mk2"},
  {"Lichtlaerm Prometheus", "Lichtlaerm Prom."},
  {"Marshall 2204 1982", "Marshall 2204"},
  {"Marshall JMP 2203 1976", "Marshall JMP 2203"},
  {"Marshall JVM 210H OD1", "Marshall JVM"},
  {"Orange OD120 1975", "Orange OD120"},
  {"Orange ORS100 1972", "Orange ORS100"},
  {"Sebago Texas Flood", "Sebago Texas Fl."},
  {"Soldano SLO100", "Soldano SLO100"},
  {"THC Sunset", "THC Sunset"},
};

inline constexpr const char* kSpeakerPrefixes[4] = {"AMP", "G12", "G65", "V30"};

// Per-amp fractal art variant. Each value selects a `case N` branch in the
// fractal art switches in VoLumFractalArt.h (Hero, Sidebar mini, Strip mini).
// Cases 0..13 are the original 14 fractals; new amps add new cases (e.g. 14 = Lichtenberg, Diezel Herbert).
// Keep the order in sync with kAmps so existing amps preserve their visual identity.
inline constexpr int kAmpFractalCase[kAmpCount] = {
  0,   // Ampete One               -> Dragon curve
  1,   // Bad Cat Mini Cat         -> Sierpinski triangle
  2,   // Brunetti XL 2            -> Barnsley fern
  14,  // Diezel Herbert Mk1       -> Lichtenberg discharge (NEW)
  3,   // Fryette Deliverance 120  -> Golden spiral
  4,   // H&K TriAmp Mk2           -> Lissajous knot
  5,   // Lichtlaerm Prometheus    -> Koch snowflake
  6,   // Marshall 2204 1982       -> Fractal tree
  7,   // Marshall JMP 2203 1976   -> H-tree
  8,   // Marshall JVM 210H OD1    -> Levy C curve
  9,   // Orange OD120 1975        -> Mandelbrot zoom
  10,  // Orange ORS100 1972       -> Julia set
  11,  // Sebago Texas Flood       -> Clifford attractor
  12,  // Soldano SLO100           -> Burning Ship fractal
  13,  // THC Sunset               -> Pentaflake
};

// Legacy constants for backward compatibility with serialized state (v0.7.14)
inline constexpr int kAmpeteRigCount = 16;
inline constexpr const char* kAmpeteFiles[kAmpeteRigCount] = {"AMP-Ampt-1.nam", "AMP-Ampt-2.nam", "AMP-Ampt-3.nam",
  "AMP-Ampt-4.nam", "G12-Ampt-1.nam", "G12-Ampt-2.nam", "G12-Ampt-3.nam", "G12-Ampt-4.nam", "G65-Ampt-1.nam",
  "G65-Ampt-2.nam", "G65-Ampt-3.nam", "G65-Ampt-4.nam", "V30-Ampt-1.nam", "V30-Ampt-2.nam", "V30-Ampt-3.nam",
  "V30-Ampt-4.nam"};
inline constexpr const char* kAmpeteLabels[kAmpeteRigCount] = {"AMP 1", "AMP 2", "AMP 3", "AMP 4", "G12 1", "G12 2",
  "G12 3", "G12 4", "G65 1", "G65 2", "G65 3", "G65 4", "V30 1", "V30 2", "V30 3", "V30 4"};

#if VOLUM_AMPETE_PRODUCT
struct VoLumAmpSettings
{
  int speakerIdx = 3;
  int channelIdx = 0;
  double inputLevel = 0.0;
  double gateThreshold = -80.0;
  double toneBass = 5.0;
  double toneMid = 5.0;
  double toneTreble = 5.0;
  double outputLevel = 0.0;
  bool noiseGateActive = true;
  bool eqActive = true;
  bool preCompActive = false;
  double preCompAmount = 3.0;
  double preCompRatio = 4.0;
  double preCompAttack = 4.0;
  double preCompRelease = 120.0;
  double preCompMix = 1.0;
  double preCompLevel = 0.0;
  bool preNam1Active = false;
  int preNam1Capture = 0;
  double preNam1Gain = 0.0;
  double preNam1Bass = 5.0;
  double preNam1Mid = 5.0;
  double preNam1MidFreq = 650.0;
  double preNam1Treble = 5.0;
  double preNam1Level = 0.0;
  bool preNam2Active = false;
  int preNam2Capture = 0;
  double preNam2Gain = 0.0;
  double preNam2Bass = 5.0;
  double preNam2Mid = 5.0;
  double preNam2MidFreq = 650.0;
  double preNam2Treble = 5.0;
  double preNam2Level = 0.0;
  bool dualAmpActive = false;
  int dualAmpRoute = 2; // 0=STACK, 1=L/R, 2=CUSTOM. UI no longer exposes the picker — Custom honours per-lane PAN.
  double mainAmpPan = 0.0;
  int supportAmpIdx = -1;
  int supportSpeakerIdx = 3;
  int supportChannelIdx = 0;
  double supportInputLevel = 0.0;
  double supportGateThreshold = -80.0;
  double supportToneBass = 5.0;
  double supportToneMid = 5.0;
  double supportToneTreble = 5.0;
  double supportOutputLevel = 0.0;
  bool supportNoiseGateActive = true;
  bool supportEqActive = true;
  double supportAmpPan = 0.0;
  bool supportPolarityInvert = true;

  // Per-amp POST live values (delay + reverb). Mirror PRE per-amp persistence: each
  // main amp keeps its own delay / reverb knob positions, mode selections, and
  // active-toggles, so switching amps swaps the POST scene the same way it swaps PRE.
  // These are the LIVE EParam values; the per-mode-snapshot map (DelayModeSnapshot /
  // ReverbModeSnapshot etc.) remains global to the session and only kicks in on a
  // mode change within the current amp.
  // Defaults match the pre-per-amp EParam defaults so brand-new amps behave the same
  // as the prior global POST defaults.
  bool postValid = false; // false = legacy / never-saved; restore should leave EParams alone.
  bool postDelayActive = false;
  double postDelayTime = 380.0;
  double postDelayFeedback = 0.35;
  double postDelayMix = 0.28;
  int postDelayMode = 0; // 0=Digital, 1=Analog, 2=Reverse
  double postDelayTone = 0.5;
  double postDelayAge = 0.0;
  bool postDelayPingPong = false;
  bool postReverbActive = false;
  double postReverbMix = 0.32;
  double postReverbDecay = 3.5;
  double postReverbTone = 5.5;
  double postReverbPreDelay = 30.0;
  double postReverbShimmer = 0.0;
  int postReverbMode = 0; // 0=Hall, 1=Plate, 2=Oktaverb
  int postReverbSubMode = 1; // Oktaverb sub-mode (0=Halo, 1=Shimmer, 2=Bloom)
};
#endif
} // namespace volum
