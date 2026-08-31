#pragma once

// VoLumChorus.h
//
// POST chorus for VoLum (first POST pedal: Chorus -> Delay -> Reverb -> Tremolo).
//
// Header-only DSP in the VoLum source tree (mirrors VoLumTremolo.h /
// VoLumPitchShifter.h placement) so it builds directly without touching the
// AudioDSPTools submodule. Operates in place on the stereo POST bus.
//
// Four modulated-delay voices built from published literature only - Dattorro,
// "Effect Design Part 2: Delay-Line Modulation and Chorus" (JAES 1997); the
// chorus chapter of Smith, "Physical Audio Signal Processing"; the expired
// US4038898A / US4384505A multi-tap ensemble patents; and the MIT-licensed
// DaisySP multi-tap chorus:
//
//   - CLASSIC  : one short modulated tap (~6.5 ms base), triangle LFO, linear
//                interpolation, no feedback. WIDTH runs the right channel's LFO
//                toward the opposite phase, which stereoizes a historically
//                mono flavour.
//   - WARPED   : a longer base (~17 ms) with a sine LFO and a darker TONE range.
//                Deeper excursion than CLASSIC and a slower rate map, so MIX at
//                1.0 lands on outright vibrato. WIDTH is a small L/R rate offset.
//   - CLEAR    : two interpolated taps a quarter-cycle apart, no feedback, cubic
//                interpolation for a transparent doubling. WIDTH offsets the
//                right channel's base delay.
//   - ENSEMBLE : three taps at 0/120/240 degrees reading one shared per-channel
//                buffer. Denser and less cyclic than a single tap; WIDTH leans
//                the outer taps apart across the stereo field.
//
// Shared behaviour: TONE is a one-pole low-pass on the wet bus (CCW dark), MIX
// is an equal-power dry/wet blend, MIX = 0 is bit-identical passthrough, and
// there is no tempo sync. Reset() clears the lines and is called by the host on
// the bypass edge and by SetParams on a mode change, matching Delay/Reverb.

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace volum
{

inline constexpr int kVoLumChorusModeCount = 4;
inline constexpr int kVoLumChorusModeClassic = 0;
inline constexpr int kVoLumChorusModeWarped = 1;
inline constexpr int kVoLumChorusModeClear = 2;
inline constexpr int kVoLumChorusModeEnsemble = 3;
// Ships bypassed on WARPED: the widest, most obviously "chorus" voice, so the
// first thing a user hears after switching the card on is unambiguous.
inline constexpr int kVoLumChorusModeDefault = kVoLumChorusModeWarped;

inline const char* VoLumChorusModeName(int mode)
{
  switch (mode)
  {
    case kVoLumChorusModeClassic: return "Classic";
    case kVoLumChorusModeWarped: return "Warped";
    case kVoLumChorusModeClear: return "Clear";
    case kVoLumChorusModeEnsemble: return "Ensemble";
    default: return "Warped";
  }
}

class ChorusDSP
{
public:
  enum Mode
  {
    kClassic = kVoLumChorusModeClassic,
    kWarped = kVoLumChorusModeWarped,
    kClear = kVoLumChorusModeClear,
    kEnsemble = kVoLumChorusModeEnsemble,
    kNumModes = kVoLumChorusModeCount,
  };

  void Prepare(double sampleRate, int /*maxBlockSize*/, int /*numChannels*/)
  {
    mSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    _Allocate();
    _RecomputeCoefs();
    Reset();
  }

  // All five knobs are 0..1; the per-mode tables below turn them into Hz and ms
  // so one knob row can drive four voices without a mode-specific control.
  void SetParams(double rate, double depth, double tone, double width, double mix, int mode, double sampleRate)
  {
    if (sampleRate > 0.0 && sampleRate != mSampleRate)
    {
      mSampleRate = sampleRate;
      _Allocate();
      _RecomputeCoefs();
      Reset();
    }
    if (mode < 0 || mode >= kNumModes)
      mode = kVoLumChorusModeDefault;
    if (mode != mMode)
    {
      mMode = mode;
      Reset();
    }
    mRate = _Clamp01(rate);
    mDepthTarget = _Clamp01(depth);
    mTone = _Clamp01(tone);
    mWidth = _Clamp01(width);
    mMixTarget = _Clamp01(mix);
  }

  void Reset()
  {
    std::fill(mBuffer[0].begin(), mBuffer[0].end(), 0.0);
    std::fill(mBuffer[1].begin(), mBuffer[1].end(), 0.0);
    mWrite = 0;
    mPhase = 0.0;
    mPhaseR = 0.0;
    mDepth = mDepthTarget;
    mMix = mMixTarget;
    mToneState[0] = mToneState[1] = 0.0;
  }

  void Process(double** io, int numChannels, int numFrames)
  {
    if (io == nullptr || numChannels <= 0 || numFrames <= 0)
      return;
    if (mBuffer[0].size() < 4)
      return;

    const int chans = std::min(numChannels, 2);
    const ModeTuning& t = _Tuning(mMode);
    const double rateHz = t.rateMinHz * std::pow(t.rateMaxHz / t.rateMinHz, mRate);
    // WARPED spreads the image by letting the right LFO drift a few percent
    // faster than the left, so the two channels wander in and out of phase.
    const double rateHzR = (mMode == kWarped) ? rateHz * (1.0 + 0.08 * mWidth) : rateHz;
    const double phaseInc = rateHz / mSampleRate;
    const double phaseIncR = rateHzR / mSampleRate;
    const double msToSamples = mSampleRate / 1000.0;
    const double baseSamples = t.baseMs * msToSamples;
    const double depthSpanSamples = t.depthMs * msToSamples;
    // CLEAR widens by pushing the right tap pair further back in the line.
    const double clearOffsetSamples = (mMode == kClear) ? mWidth * 2.5 * msToSamples : 0.0;
    const double toneCoef = _OnePoleCoef(t.toneMinHz * std::pow(t.toneMaxHz / t.toneMinHz, mTone));
    const int maxIndex = static_cast<int>(mBuffer[0].size()) - 3;

    for (int s = 0; s < numFrames; ++s)
    {
      mDepth += (mDepthTarget - mDepth) * mSmoothCoef;
      mMix += (mMixTarget - mMix) * mSmoothCoef;
      const double depthSamples = mDepth * depthSpanSamples;
      const double dryGain = std::cos(mMix * M_PI * 0.5);
      const double wetGain = std::sin(mMix * M_PI * 0.5);

      for (int c = 0; c < chans; ++c)
        mBuffer[c][static_cast<size_t>(mWrite)] = _Flush(io[c][s]);

      for (int c = 0; c < chans; ++c)
      {
        const double x = io[c][s];
        const double phase = (c == 0) ? mPhase : mPhaseR;
        double wet = 0.0;

        switch (mMode)
        {
          case kClassic:
          {
            // WIDTH walks the right LFO from in-phase (0) to anti-phase (1).
            const double p = (c == 0) ? phase : phase + 0.5 * mWidth;
            const double d = baseSamples + depthSamples * _Unipolar(_Triangle(p));
            wet = _ReadLinear(c, d, maxIndex);
            break;
          }
          case kWarped:
          {
            const double d = baseSamples + depthSamples * _Unipolar(_Sine(phase));
            wet = _ReadCubic(c, d, maxIndex);
            break;
          }
          case kClear:
          {
            const double offset = (c == 0) ? 0.0 : clearOffsetSamples;
            const double dA = baseSamples + offset + depthSamples * _Unipolar(_Sine(phase));
            const double dB = baseSamples + offset + depthSamples * _Unipolar(_Sine(phase + 0.25));
            wet = 0.5 * (_ReadCubic(c, dA, maxIndex) + _ReadCubic(c, dB, maxIndex));
            break;
          }
          case kEnsemble:
          default:
          {
            // Three taps a third of a cycle apart off one shared line. WIDTH
            // leans the outer taps apart; the weights always sum to 1 so the
            // wet bus cannot gain up as WIDTH opens.
            const double outerNear = (1.0 + mWidth) / 3.0;
            const double outerFar = (1.0 - mWidth) / 3.0;
            const double w0 = (chans == 1) ? (1.0 / 3.0) : ((c == 0) ? outerNear : outerFar);
            const double w2 = (chans == 1) ? (1.0 / 3.0) : ((c == 0) ? outerFar : outerNear);
            const double d0 = baseSamples + depthSamples * _Unipolar(_Sine(phase));
            const double d1 = baseSamples + depthSamples * _Unipolar(_Sine(phase + 1.0 / 3.0));
            const double d2 = baseSamples + depthSamples * _Unipolar(_Sine(phase + 2.0 / 3.0));
            wet = w0 * _ReadCubic(c, d0, maxIndex) + (1.0 / 3.0) * _ReadCubic(c, d1, maxIndex)
                  + w2 * _ReadCubic(c, d2, maxIndex);
            break;
          }
        }

        mToneState[c] += (wet - mToneState[c]) * toneCoef;
        mToneState[c] = _Flush(mToneState[c]);
        wet = mToneState[c];

        // MIX at exactly 0 must not touch the sample at all (the POST bus stays
        // bit-identical while the card is on but fully dry).
        io[c][s] = (wetGain > 0.0) ? (x * dryGain + wet * wetGain) : x;
      }

      mPhase += phaseInc;
      if (mPhase >= 1.0)
        mPhase -= 1.0;
      mPhaseR += phaseIncR;
      if (mPhaseR >= 1.0)
        mPhaseR -= 1.0;
      if (++mWrite >= static_cast<int>(mBuffer[0].size()))
        mWrite = 0;
    }
  }

private:
  struct ModeTuning
  {
    double rateMinHz;
    double rateMaxHz;
    double baseMs;
    double depthMs;
    double toneMinHz;
    double toneMaxHz;
  };

  static const ModeTuning& _Tuning(int mode)
  {
    static const ModeTuning kTuning[kNumModes] = {
      {0.10, 8.0, 6.5, 3.0, 600.0, 12000.0}, // CLASSIC: short, bright, fast
      {0.05, 5.0, 17.0, 7.0, 320.0, 6000.0}, // WARPED: long, dark, slow, deep
      {0.10, 6.0, 9.0, 4.0, 800.0, 16000.0}, // CLEAR: transparent doubling
      {0.08, 3.0, 12.0, 5.0, 600.0, 12000.0}, // ENSEMBLE: dense and slow
    };
    return kTuning[std::clamp(mode, 0, kNumModes - 1)];
  }

  // 64 ms covers the longest base + full excursion + the CLEAR width offset with
  // headroom, so Process never has to grow the line on the audio thread.
  static constexpr double kMaxDelayMs = 64.0;

  void _Allocate()
  {
    const size_t n = static_cast<size_t>(kMaxDelayMs * mSampleRate / 1000.0) + 8;
    for (int c = 0; c < 2; ++c)
      mBuffer[c].assign(n, 0.0);
    mWrite = 0;
  }

  void _RecomputeCoefs()
  {
    // ~15 ms parameter smoothing, matching the tremolo, so depth/mix automation
    // does not step the delay time.
    mSmoothCoef = 1.0 - std::exp(-1.0 / (0.015 * mSampleRate));
  }

  double _OnePoleCoef(double cutoffHz) const
  {
    const double c = 1.0 - std::exp(-2.0 * M_PI * cutoffHz / mSampleRate);
    return std::clamp(c, 0.0, 1.0);
  }

  static double _Clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
  static double _Flush(double v) { return (std::isfinite(v) && std::abs(v) >= 1e-20) ? v : 0.0; }
  static double _Unipolar(double bipolar) { return 0.5 * (bipolar + 1.0); }

  static double _Sine(double phase) { return std::sin(2.0 * M_PI * (phase - std::floor(phase))); }

  static double _Triangle(double phase)
  {
    const double t = phase - std::floor(phase);
    return (t < 0.5) ? (4.0 * t - 1.0) : (3.0 - 4.0 * t);
  }

  // Fractional read `delaySamples` behind the write head. Both readers clamp the
  // delay into the line so a corrupt parameter can never index out of bounds.
  double _ReadLinear(int ch, double delaySamples, int maxIndex) const
  {
    const auto& buf = mBuffer[ch];
    const int size = static_cast<int>(buf.size());
    const double d = std::clamp(delaySamples, 1.0, static_cast<double>(maxIndex));
    const int i0 = static_cast<int>(d);
    const double frac = d - static_cast<double>(i0);
    const int r0 = _WrapRead(mWrite - i0, size);
    const int r1 = _WrapRead(r0 - 1, size);
    return buf[static_cast<size_t>(r0)] * (1.0 - frac) + buf[static_cast<size_t>(r1)] * frac;
  }

  // 4-point Hermite: transparent enough that CLEAR / ENSEMBLE taps do not add
  // the interpolation hiss a linear read leaves on fast modulation.
  double _ReadCubic(int ch, double delaySamples, int maxIndex) const
  {
    const auto& buf = mBuffer[ch];
    const int size = static_cast<int>(buf.size());
    const double d = std::clamp(delaySamples, 2.0, static_cast<double>(maxIndex));
    const int i0 = static_cast<int>(d);
    const double f = d - static_cast<double>(i0);
    const int rm1 = _WrapRead(mWrite - i0 + 1, size);
    const int r0 = _WrapRead(mWrite - i0, size);
    const int r1 = _WrapRead(mWrite - i0 - 1, size);
    const int r2 = _WrapRead(mWrite - i0 - 2, size);
    const double ym1 = buf[static_cast<size_t>(rm1)];
    const double y0 = buf[static_cast<size_t>(r0)];
    const double y1 = buf[static_cast<size_t>(r1)];
    const double y2 = buf[static_cast<size_t>(r2)];
    const double c0 = y0;
    const double c1 = 0.5 * (y1 - ym1);
    const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
    const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + c0;
  }

  static int _WrapRead(int index, int size)
  {
    index %= size;
    return (index < 0) ? index + size : index;
  }

  double mSampleRate = 48000.0;
  int mMode = kVoLumChorusModeDefault;
  double mRate = 0.35;
  double mDepthTarget = 0.45;
  double mDepth = 0.45;
  double mTone = 0.4;
  double mWidth = 0.7;
  double mMixTarget = 0.0;
  double mMix = 0.0;

  double mPhase = 0.0;
  double mPhaseR = 0.0;
  double mSmoothCoef = 0.01;
  double mToneState[2] = {0.0, 0.0};

  std::vector<double> mBuffer[2];
  int mWrite = 0;
};

} // namespace volum
