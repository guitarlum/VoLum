#pragma once

// VoLum plugin parameter enum, extracted from NeuralAmpModeler.h so it can be
// included by test translation units WITHOUT pulling in the IGraphics / iPlug
// plugin headers. This is the SINGLE SOURCE OF TRUTH for `kNumParams`.
//
// Why this matters: `SerializeState` writes the frozen 1.2.2 prefix
// (`kVoLumChunkParamPrefixCount` doubles), and the current-version chunk reader
// (`Unserialization.cpp`, >= 1.2.0 branch) must consume exactly that many before
// it reaches the per-amp selection/scene block. Live `kNumParams` may grow; extra
// EParams overlay from id-tail JSON. The 1.2.0 "VST3/AU state resets to default
// on load" bug was a hand-maintained 71-name reader list drifting from the writer.
//
// EParams order is serialization-sensitive: never reorder or renumber existing
// entries; append new params immediately before `kNumParams`.

enum EParams
{
  // These need to be the first ones because I use their indices to place
  // their rects in the GUI.
  kInputLevel = 0,
  kNoiseGateThreshold,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
  // The rest is fine though.
  kNoiseGateActive,
  kEQActive,
  kIRToggle,
  // Delay (POST)
  kDelayActive,
  kDelayTime,
  kDelayFeedback,
  kDelayMix,
  kDelayMode,
  // Effect staging (delay)
  kDelayTone,
  kDelayAge,
  kDelayPingPong,
  // Reverb (POST)
  kReverbActive,
  kReverbMix,
  kReverbDecay,
  kReverbTone,
  kReverbPreDelay,
  kReverbShimmer,
  kReverbMode,
  // Effect staging (reverb - Oktaverb sub-mode only)
  kReverbSubMode,
  // Reserved legacy boost params. Keep serialized indices stable; PRE captures replaced this planned DSP block.
  kBoostActive,
  kBoostDrive,
  kBoostTone,
  kBoostLevel,
  // PRE pedalboard
  kPreCompActive,
  kPreCompAmount,
  kPreCompRatio,
  kPreCompAttack,
  kPreCompRelease,
  kPreCompMix,
  kPreCompLevel,
  kPreNam1Active,
  kPreNam1Capture,
  kPreNam1Gain,
  kPreNam1Bass,
  kPreNam1Mid,
  kPreNam1MidFreq,
  kPreNam1Treble,
  kPreNam1Level,
  kPreNam2Active,
  kPreNam2Capture,
  kPreNam2Gain,
  kPreNam2Bass,
  kPreNam2Mid,
  kPreNam2MidFreq,
  kPreNam2Treble,
  kPreNam2Level,
  // Input calibration
  kCalibrateInput,
  kInputCalibrationLevel,
  kOutputMode,
  kVoLumAmpeteRig,
  // VoLum: dual-amp and support-lane parameters
  kDualAmpActive,
  kDualAmpRoute,
  kMainAmpPan,
  kSupportAmpIdx,
  kSupportSpeakerIdx,
  kSupportChannelIdx,
  kSupportInputLevel,
  kSupportNoiseGateThreshold,
  kSupportToneBass,
  kSupportToneMid,
  kSupportToneTreble,
  kSupportOutputLevel,
  kSupportNoiseGateActive,
  kSupportEQActive,
  kSupportAmpPan,
  // Per-lane custom IR for the dual-amp SUPPORT lane (mirrors kIRToggle for the
  // MAIN amp). Appended at the end to keep all prior serialized indices stable.
  kSupportIRToggle,
  // PRE Pitch pedal (Transpose / Octaver), at the very front of the PRE chain.
  // Appended at the end to keep all prior serialized indices stable.
  kPrePitchActive,
  kPrePitchMode,
  kPrePitchSemitones,
  kPrePitchMix,
  kPrePitchOctDown,
  kPrePitchOctUp,
  kPrePitchDry,
  kPrePitchVoicing,
  kPrePitchLevel,
  // Tremolo (POST) - third POST pedal, runs last after Reverb. Appended at the
  // end to keep all prior serialized indices stable.
  kTremoloActive,
  kTremoloMode,
  kTremoloRate,
  kTremoloDepth,
  kTremoloShape,
  kTremoloMix,
  kTremoloCrossover,
  kTremoloSync,
  kTremoloDivision,
  // PRE Pitch Transpose CHARACTER (Drop / Fast). Appended at the very end to keep
  // all prior serialized indices stable.
  kPrePitchTransChar,
  // Delay tempo SYNC + DIVISION. Appended at the very end to keep all prior
  // serialized indices stable. When synced, the Time knob is replaced by a
  // tempo-division stepper (mirrors the Tremolo Sync/Division pair).
  kDelaySync,
  kDelayDivision,
  kNumParams
};

// Frozen DAW-chunk param-prefix length (VoLum 1.2.2). SerializeState writes this
// many doubles, and the >= 1.2.0 reader consumes this many, even after kNumParams
// grows. New automatable knobs are real EParams (indices 0–92 stay put) whose
// saved values live in id-tail JSON, not as extra prefix doubles. See
// .scratch/release-1.3.0/issues/07-forward-compatible-chunks.md.
inline constexpr int kVoLumChunkParamPrefixCount = 93;
static_assert(kNumParams >= kVoLumChunkParamPrefixCount,
              "param prefix is the 1.2.2 list; new params append after it");
