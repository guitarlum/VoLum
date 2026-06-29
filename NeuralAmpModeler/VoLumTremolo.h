#pragma once

// VoLumTremolo.h
//
// POST tremolo effect for VoLum (third POST pedal, runs last after Reverb).
//
// Header-only DSP in the VoLum source tree (mirrors VoLumMetronomeDSP.h /
// VoLumPitchShifter.h placement) so it builds directly without touching the
// AudioDSPTools submodule. Operates in place on the stereo POST bus.
//
// Three reverse-engineered amp/pedal tremolo voices:
//   - Optical   : LFO-driven gain through an asymmetric photocell envelope (fast
//                 attack, lazy release) so the throb sags down and snaps back -
//                 a clearly non-sine pulse even at slow rates. (Reverb.com "6
//                 Types of Tremolo", Schaller TR-68.)
//   - Bias      : smoothest, symmetric sine amplitude modulation - the
//                 "Bang Bang (My Baby Shot Me Down)" voice. (Strymon Flint paper.)
//   - Harmonic  : dual-band crossover, low/high modulated pi out of phase and
//                 summed; phasey/lush. (Strymon Flint paper, ChromaticIsobar/HyperTremolo.)
//
// Shape morphs the LFO from sine (0) toward square (1). Tempo sync is handled by
// the caller, which converts host/metronome BPM + a musical division into an LFO
// rate in Hz and passes it as `rateHz`.

#include <algorithm>
#include <cmath>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace volum
{

inline constexpr int kVoLumTremoloModeCount = 3;
inline constexpr int kVoLumTremoloModeOptical = 0;
inline constexpr int kVoLumTremoloModeBias = 1;
inline constexpr int kVoLumTremoloModeHarmonic = 2;

// Musical divisions for tempo sync, ordered slow -> fast.
inline constexpr int kVoLumTremoloDivisionCount = 8;
inline constexpr int kVoLumTremoloDivisionDefault = 4; // 1/8

inline const char* VoLumTremoloDivisionName(int division)
{
  switch (division)
  {
    case 0: return "1/2";
    case 1: return "1/4";
    case 2: return "1/4.";
    case 3: return "1/4T";
    case 4: return "1/8";
    case 5: return "1/8.";
    case 6: return "1/8T";
    case 7: return "1/16";
    default: return "1/8";
  }
}

// LFO frequency (Hz) for a tempo division at a given BPM. The multiplier is
// relative to a quarter note (= one LFO cycle per beat). Dotted = x1.5 duration
// (slower), triplet = x2/3 duration (faster).
inline double VoLumTremoloSyncRateHz(double bpm, int division)
{
  static const double kQuarterMultiplier[kVoLumTremoloDivisionCount] = {
    0.5, // 1/2
    1.0, // 1/4
    1.0 / 1.5, // 1/4. (dotted quarter)
    1.5, // 1/4T (quarter triplet)
    2.0, // 1/8
    2.0 / 1.5, // 1/8. (dotted eighth)
    3.0, // 1/8T (eighth triplet)
    4.0, // 1/16
  };
  if (division < 0 || division >= kVoLumTremoloDivisionCount)
    division = kVoLumTremoloDivisionDefault;
  if (bpm < 1.0)
    bpm = 120.0;
  const double quartersPerSecond = bpm / 60.0;
  return quartersPerSecond * kQuarterMultiplier[division];
}

class TremoloDSP
{
public:
  enum Mode
  {
    kOptical = kVoLumTremoloModeOptical,
    kBias = kVoLumTremoloModeBias,
    kHarmonic = kVoLumTremoloModeHarmonic,
    kNumModes = kVoLumTremoloModeCount,
  };

  void Prepare(double sampleRate, int /*maxBlockSize*/, int /*numChannels*/)
  {
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    _RecomputeCoefs();
    Reset();
  }

  // rateHz       : effective LFO rate in Hz (caller resolves tempo sync to Hz).
  // depth        : 0..1 modulation depth (1 = full attenuation at the trough).
  // shape        : 0..1 sine -> square morph.
  // mix          : 0..1 dry/wet (1 = fully modulated).
  // crossoverHz  : Harmonic-mode band split frequency.
  // mode         : Optical / Bias / Harmonic.
  void SetParams(double rateHz, double depth, double shape, double mix, double crossoverHz, int mode, double sampleRate)
  {
    if (sampleRate > 0.0 && sampleRate != mSampleRate)
    {
      mSampleRate = sampleRate;
      _RecomputeCoefs();
    }
    mRateHz = std::clamp(rateHz, 0.01, 40.0);
    mDepthTarget = std::clamp(depth, 0.0, 1.0);
    mShape = std::clamp(shape, 0.0, 1.0);
    mMixTarget = std::clamp(mix, 0.0, 1.0);
    const double newCrossover = std::clamp(crossoverHz, 50.0, 8000.0);
    if (newCrossover != mCrossoverHz)
    {
      mCrossoverHz = newCrossover;
      mXoverCoef = _OnePoleCoef(mCrossoverHz);
    }
    if (mode < 0 || mode >= kNumModes)
      mode = kBias;
    mMode = mode;
  }

  void Reset()
  {
    mPhase = 0.0;
    mDepth = mDepthTarget;
    mMix = mMixTarget;
    mOptGainState[0] = mOptGainState[1] = 1.0;
    mLp1[0] = mLp1[1] = mLp2[0] = mLp2[1] = 0.0;
  }

  // In-place stereo (or mono) amplitude modulation on the POST bus.
  void Process(double** io, int numChannels, int numFrames)
  {
    if (io == nullptr || numChannels <= 0 || numFrames <= 0)
      return;
    const int chans = std::min(numChannels, 2);
    const double phaseInc = mRateHz / mSampleRate;

    for (int s = 0; s < numFrames; ++s)
    {
      mDepth += (mDepthTarget - mDepth) * mSmoothCoef;
      mMix += (mMixTarget - mMix) * mSmoothCoef;

      const double gPrimary = _ModGain(mPhase, mDepth);
      const double gAnti = _ModGain(mPhase + 0.5, mDepth);

      for (int c = 0; c < chans; ++c)
      {
        const double x = io[c][s];

        // Always advance the crossover filter so switching into Harmonic is
        // click-free (the band split is warm before it is heard).
        mLp1[c] += (x - mLp1[c]) * mXoverCoef;
        mLp2[c] += (mLp1[c] - mLp2[c]) * mXoverCoef;
        mLp1[c] = _Flush(mLp1[c]);
        mLp2[c] = _Flush(mLp2[c]);

        // Always advance the optical envelope so switching into Optical is smooth.
        const double optTarget = gPrimary;
        const double optCoef = (optTarget > mOptGainState[c]) ? mOptAttackCoef : mOptReleaseCoef;
        mOptGainState[c] += (optTarget - mOptGainState[c]) * optCoef;

        double wet;
        switch (mMode)
        {
          case kHarmonic:
          {
            const double low = mLp2[c];
            const double high = x - low;
            wet = low * gPrimary + high * gAnti;
            break;
          }
          case kOptical: wet = x * mOptGainState[c]; break;
          case kBias:
          default: wet = x * gPrimary; break;
        }

        io[c][s] = x * (1.0 - mMix) + wet * mMix;
      }

      mPhase += phaseInc;
      if (mPhase >= 1.0)
        mPhase -= 1.0;
    }
  }

private:
  double _OnePoleCoef(double cutoffHz) const
  {
    const double c = 1.0 - std::exp(-2.0 * M_PI * cutoffHz / mSampleRate);
    return std::clamp(c, 0.0, 1.0);
  }

  void _RecomputeCoefs()
  {
    // ~15 ms parameter smoothing to kill zipper on depth/mix automation.
    mSmoothCoef = 1.0 - std::exp(-1.0 / (0.015 * mSampleRate));
    // Photocell-style asymmetry: fast brighten, lazy fade. The release lag must
    // be a meaningful fraction of the LFO period (tens of ms) or the optical
    // throb collapses back onto the symmetric Bias sine at slow rates - i.e.
    // switching Optical<->Bias becomes inaudible. ~3 ms attack / ~28 ms release
    // keeps the classic asymmetric "sag down, snap up" pulse at default rates
    // while naturally shedding depth as the rate climbs (authentic optical).
    mOptAttackCoef = 1.0 - std::exp(-1.0 / (0.003 * mSampleRate));
    mOptReleaseCoef = 1.0 - std::exp(-1.0 / (0.028 * mSampleRate));
    mXoverCoef = _OnePoleCoef(mCrossoverHz);
  }

  static double _Flush(double v) { return (std::abs(v) < 1e-20) ? 0.0 : v; }

  // Shaped unipolar gain in [1-depth, 1]. phase wraps to [0,1).
  double _ModGain(double phase, double depth) const
  {
    phase -= std::floor(phase);
    const double bipolar = std::sin(2.0 * M_PI * phase); // -1..1
    double shaped;
    if (mShape <= 1e-6)
    {
      shaped = bipolar;
    }
    else
    {
      // tanh waveshaping pushes the sine toward a square as shape -> 1.
      const double drive = 1.0 + mShape * mShape * 40.0;
      shaped = std::tanh(bipolar * drive) / std::tanh(drive);
    }
    const double unipolar = 0.5 * (shaped + 1.0); // 0..1
    return 1.0 - depth * (1.0 - unipolar);
  }

  double mSampleRate = 48000.0;
  double mRateHz = 5.0;
  double mDepth = 0.0;
  double mDepthTarget = 0.0;
  double mShape = 0.0;
  double mMix = 1.0;
  double mMixTarget = 1.0;
  double mCrossoverHz = 800.0;
  int mMode = kBias;

  double mPhase = 0.0;
  double mSmoothCoef = 0.01;
  double mXoverCoef = 0.1;
  double mOptAttackCoef = 0.3;
  double mOptReleaseCoef = 0.05;

  double mOptGainState[2] = {1.0, 1.0};
  double mLp1[2] = {0.0, 0.0};
  double mLp2[2] = {0.0, 0.0};
};

} // namespace volum
