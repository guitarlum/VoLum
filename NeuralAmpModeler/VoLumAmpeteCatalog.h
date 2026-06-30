#pragma once

#include <string>

#include "config.h"
#include "VoLumTremolo.h" // tremolo mode/division constants used in per-amp defaults

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
  0, // Ampete One               -> Dragon curve
  1, // Bad Cat Mini Cat         -> Sierpinski triangle
  2, // Brunetti XL 2            -> Barnsley fern
  14, // Diezel Herbert Mk1       -> Lichtenberg discharge (NEW)
  3, // Fryette Deliverance 120  -> Golden spiral
  4, // H&K TriAmp Mk2           -> Lissajous knot
  5, // Lichtlaerm Prometheus    -> Koch snowflake
  6, // Marshall 2204 1982       -> Fractal tree
  7, // Marshall JMP 2203 1976   -> H-tree
  8, // Marshall JVM 210H OD1    -> Levy C curve
  9, // Orange OD120 1975        -> Mandelbrot zoom
  10, // Orange ORS100 1972       -> Julia set
  11, // Sebago Texas Flood       -> Clifford attractor
  12, // Soldano SLO100           -> Burning Ship fractal
  13, // THC Sunset               -> Pentaflake
};

// Legacy constants for backward compatibility with serialized state (v0.7.14)
inline constexpr int kAmpeteRigCount = 16;
inline constexpr const char* kAmpeteFiles[kAmpeteRigCount] = {
  "AMP-Ampt-1.nam", "AMP-Ampt-2.nam", "AMP-Ampt-3.nam", "AMP-Ampt-4.nam", "G12-Ampt-1.nam", "G12-Ampt-2.nam",
  "G12-Ampt-3.nam", "G12-Ampt-4.nam", "G65-Ampt-1.nam", "G65-Ampt-2.nam", "G65-Ampt-3.nam", "G65-Ampt-4.nam",
  "V30-Ampt-1.nam", "V30-Ampt-2.nam", "V30-Ampt-3.nam", "V30-Ampt-4.nam"};
inline constexpr const char* kAmpeteLabels[kAmpeteRigCount] = {"AMP 1", "AMP 2", "AMP 3", "AMP 4", "G12 1", "G12 2",
                                                               "G12 3", "G12 4", "G65 1", "G65 2", "G65 3", "G65 4",
                                                               "V30 1", "V30 2", "V30 3", "V30 4"};

inline constexpr int kVoLumDelayModeCount = 3;
inline constexpr int kVoLumReverbModeCount = 3;

// Effect-staging delay-mode order: 0=Digital, 1=Analog, 2=Reverse
inline constexpr int kVoLumDelayModeDigital = 0;
inline constexpr int kVoLumDelayModeAnalog = 1;
inline constexpr int kVoLumDelayModeReverse = 2;
// Back-compat alias: pre-v0.9.0 code referenced kVoLumReverseDelayMode by name.
inline constexpr int kVoLumReverseDelayMode = kVoLumDelayModeReverse;

// Reverb-mode order remains: 0=Hall, 1=Plate, 2=Oktaverb
inline constexpr int kVoLumReverbModeHall = 0;
inline constexpr int kVoLumReverbModePlate = 1;
inline constexpr int kVoLumReverbModeOktaverb = 2;
inline constexpr int kVoLumOktaverbSubModeHalo = 0;
inline constexpr int kVoLumOktaverbSubModeShimmer = 1;
inline constexpr int kVoLumOktaverbSubModeBloom = 2;
// Backward-compat alias: old "Dark" name kept so any external callers still build. Slot 0
// is now Halo (dual +-12 in feedback).
inline constexpr int kVoLumOktaverbSubModeDark = kVoLumOktaverbSubModeHalo;

// PRE Pitch pedal mode order: 0=Transpose, 1=Octaver
inline constexpr int kVoLumPitchModeCount = 2;
inline constexpr int kVoLumPitchModeTranspose = 0;
inline constexpr int kVoLumPitchModeOctaver = 1;
// PRE Pitch octaver voicing: 0=Vintage (gritty/filtered), 1=Modern (clean)
inline constexpr int kVoLumPitchVoicingVintage = 0;
inline constexpr int kVoLumPitchVoicingModern = 1;
// PRE Pitch transpose character: 0=Drop (WSOLA period-sync, exact mono, ~17 ms),
// 1=Instant (period-sync, ~8.6 ms, tightest mono feel), 2=Poly (fixed-grain WSOLA,
// no pitch estimate -> polyphonic/chord-capable, ~49 ms; independent replication of
// the a commercial reference transpose family). Drop/Instant are monophonic.
// History: the Pitch feature is unreleased-dev-only, so adding Poly as value 2 has
// no public migration impact (out-of-range stored values still clamp into range).
inline constexpr int kVoLumPitchCharacterDrop = 0;
inline constexpr int kVoLumPitchCharacterInstant = 1;
inline constexpr int kVoLumPitchCharacterPoly = 2;
inline constexpr int kVoLumPitchCharacterCount = 3;

struct DelayModeSnapshot
{
  double time = 320.0;
  double feedback = 0.35;
  double mix = 0.28;
  double tone = 0.5;
  double age = 0.0;
  bool pingPong = false;
};

struct ReverbModeSnapshot
{
  double mix = 0.3;
  double decay = 3.0;
  double tone = 4.5;
  double preDelay = 20.0;
  double shimmer = 0.5;
  int subMode = kVoLumOktaverbSubModeShimmer;
};

struct OktaverbSubModeSnapshot
{
  double mix = 0.40;
  double decay = 5.0;
  double tone = 5.5;
  double preDelay = 30.0;
  double shimmer = 0.65;
};

// Per-pitch-mode knob memory (Transpose / Octaver). Only the SHARED knobs need
// memory: semitones (Transpose) and octDown/octUp (Octaver) are already
// mode-exclusive params and never collide.
struct PitchModeSnapshot
{
  double mix = 1.0; // 0..1 wet/dry
  double dry = 1.0; // 0..1 dry blend
  double level = 0.0; // dB output trim
  int voicing = kVoLumPitchVoicingModern; // 0=Vintage, 1=Modern
};

// Per-tremolo-mode knob memory (Optical / Bias / Harmonic). Mirrors the
// reverb/delay snapshots so switching tremolo mode recalls that mode's last
// knobs. depth is the KNOB value (0..1); the audible floor is applied at the
// param->DSP boundary, not here.
struct TremoloModeSnapshot
{
  double rate = 3.0; // Hz
  double depth = 0.85; // 0..1 knob
  double shape = 0.0; // 0..1 (sine -> square)
  double mix = 0.60; // 0..1
  double crossover = 800.0; // Hz (Harmonic band split)
};

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
  // PRE Pitch pedal (Transpose / Octaver), stored per amp like the rest of PRE.
  bool prePitchActive = false;
  int prePitchMode = 0; // 0=Transpose, 1=Octaver
  double prePitchSemitones = 0.0;
  double prePitchMix = 1.0;
  double prePitchOctDown = 0.8; // ship audible: a sub-octave blend so enabling the octaver does something
  double prePitchOctUp = 0.0;
  double prePitchDry = 1.0;
  int prePitchVoicing = 1; // 0=Vintage, 1=Modern
  double prePitchLevel = 0.0;
  int prePitchTransChar = 1; // 0=Drop, 1=Instant (default; transpose engine character)
  // Per-mode knob memory for the shared Mix/Dry/Level/Voicing knobs so switching
  // Transpose<->Octaver recalls that mode's last blend (mirrors postTremoloModes).
  PitchModeSnapshot prePitchModes[kVoLumPitchModeCount] = {
    PitchModeSnapshot{1.0, 1.0, 0.0, kVoLumPitchVoicingModern}, // Transpose
    PitchModeSnapshot{1.0, 1.0, 0.0, kVoLumPitchVoicingModern}, // Octaver
  };
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

  // Per-amp POST values (delay + reverb). Mirror PRE per-amp persistence: each
  // main amp keeps both live EParam values and hidden per-mode snapshots, so mode
  // changes made inside POST survive amp switches too.
  // Defaults match the pre-per-amp EParam defaults so brand-new amps behave the same
  // as the prior global POST defaults.
  bool postValid = false; // false = legacy / never-saved; restore initializes factory POST defaults.
  bool postDelayActive = false;
  double postDelayTime = 320.0;
  double postDelayFeedback = 0.35;
  double postDelayMix = 0.28;
  int postDelayMode = 0; // 0=Digital, 1=Analog, 2=Reverse
  double postDelayTone = 0.5;
  double postDelayAge = 0.0;
  bool postDelayPingPong = false;
  // Tempo sync: when on, the Time knob is replaced by a musical DIVISION locked
  // to the host/metronome BPM (mirrors postTremoloSync / postTremoloDivision).
  bool postDelaySync = false;
  int postDelayDivision = kVoLumTremoloDivisionDefault; // 1/8
  bool postReverbActive = false;
  double postReverbMix = 0.20;
  double postReverbDecay = 2.5;
  double postReverbTone = 5.0;
  double postReverbPreDelay = 30.0;
  double postReverbShimmer = 0.0;
  int postReverbMode = 0; // 0=Hall, 1=Plate, 2=Oktaverb
  int postReverbSubMode = 1; // Oktaverb sub-mode (0=Halo, 1=Shimmer, 2=Bloom)
  DelayModeSnapshot postDelayModes[kVoLumDelayModeCount] = {
    DelayModeSnapshot{320.0, 0.35, 0.28, 0.50, 0.00, false},
    DelayModeSnapshot{320.0, 0.42, 0.32, 0.50, 0.50, false},
    DelayModeSnapshot{600.0, 0.30, 0.32, 0.50, 0.00, false},
  };
  ReverbModeSnapshot postReverbModes[kVoLumReverbModeCount] = {
    ReverbModeSnapshot{0.20, 2.5, 5.0, 30.0, 0.0, 0},
    ReverbModeSnapshot{0.20, 2.5, 4.5, 20.0, 0.0, 0},
    ReverbModeSnapshot{0.30, 6.0, 6.0, 30.0, 0.70, kVoLumOktaverbSubModeShimmer},
  };
  OktaverbSubModeSnapshot postOktaverbSubModes[3] = {
    OktaverbSubModeSnapshot{0.30, 5.5, 6.0, 25.0, 0.65},
    OktaverbSubModeSnapshot{0.30, 6.0, 6.0, 30.0, 0.70},
    OktaverbSubModeSnapshot{0.30, 5.5, 5.5, 20.0, 0.75},
  };

  // Per-amp POST Tremolo (third POST pedal). The scalar fields are the LIVE
  // selected-mode values; postTremoloModes[] keeps each mode's last knobs so
  // switching mode recalls that voice (mirrors postReverbModes / postDelayModes).
  // Defaults: slow deep Optical throb at a moderate wet blend.
  bool postTremoloActive = false;
  int postTremoloMode = kVoLumTremoloModeOptical;
  double postTremoloRate = 3.0; // Hz (free-running rate)
  double postTremoloDepth = 0.85; // 0..1
  double postTremoloShape = 0.0; // 0..1 (sine -> square)
  double postTremoloMix = 0.60; // 0..1
  double postTremoloCrossover = 800.0; // Hz (Harmonic band split)
  bool postTremoloSync = false;
  int postTremoloDivision = kVoLumTremoloDivisionDefault; // 1/8
  TremoloModeSnapshot postTremoloModes[kVoLumTremoloModeCount] = {
    TremoloModeSnapshot{3.0, 0.85, 0.0, 0.60, 800.0}, // Optical
    TremoloModeSnapshot{3.0, 0.85, 0.0, 0.60, 800.0}, // Bias
    TremoloModeSnapshot{3.0, 0.85, 0.0, 0.60, 800.0}, // Harmonic
  };

  // 1.2.0 BYO custom-content references (additive, id-based). Both default to
  // empty = "no custom content". They are NOT written into the fixed per-amp
  // chunk block (which keeps a stable byte layout); they travel in the JSON
  // settings/preset/registry codecs and in the append-only id chunk tail.
  // activeIrId: when non-empty, the amp's DIRECT capture is run through this
  // custom IR cab instead of a baked cab (see VoLumProcessingPlan / spec 3.2).
  std::string activeIrId;
  // supportActiveIrId: the same, for the dual-amp SUPPORT lane. The SUPPORT lane
  // owns its own convolver (mSupportIR), so its custom IR is independent of the
  // MAIN lane's activeIrId (per-lane custom IR). Lives on the focused MAIN scene
  // alongside the other dual-amp partner fields (supportCustomId, etc.).
  std::string supportActiveIrId;
  // supportCustomId: when non-empty, the dual-amp SUPPORT partner is the custom
  // amp with this id (takes precedence over the factory supportAmpIdx).
  std::string supportCustomId;
  // Custom SUPPORT partner cab + gain stage. The MAIN custom lane reuses
  // speakerIdx/channelIdx; the SUPPORT lane needs its own so a custom support
  // amp's chosen cab + channel survive preset save and session/DAW recall.
  // supportCustomSlot: kDirectSlot(-1) / 0..2 cab slot, or kUnassignedSlot(-2)
  // == "never saved" (restore then falls back to the default capture). Only
  // meaningful while supportCustomId is non-empty.
  int supportCustomSlot = -2;
  int supportCustomChannel = 0; // gain stage (>= 1); 0 == not saved
};
} // namespace volum
