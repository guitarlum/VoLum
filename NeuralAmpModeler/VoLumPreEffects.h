#pragma once

#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"
#include "../AudioDSPTools/dsp/dsp.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dsp
{
namespace effect
{

class VoLumPreEq
{
public:
  void Reset(double sampleRate, int maxBlockSize)
  {
    (void) maxBlockSize;
    mSampleRate = sampleRate;
    _Refresh();
  }

  void SetParams(double bass, double mid, double midFrequency, double treble)
  {
    mBass = std::clamp(bass, 0.0, 10.0);
    mMid = std::clamp(mid, 0.0, 10.0);
    mMidFrequency = std::clamp(midFrequency, 150.0, 2500.0);
    mTreble = std::clamp(treble, 0.0, 10.0);
    _Refresh();
  }

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, size_t numChannels, size_t numFrames)
  {
    DSP_SAMPLE** bass = mBassFilter.Process(inputs, numChannels, numFrames);
    DSP_SAMPLE** mid = mMidFilter.Process(bass, numChannels, numFrames);
    return mTrebleFilter.Process(mid, numChannels, numFrames);
  }

private:
  void _Refresh()
  {
    if (mSampleRate <= 0.0)
      return;

    mBassFilter.SetParams(recursive_linear_filter::BiquadParams(mSampleRate, 150.0, 0.707, 4.0 * (mBass - 5.0)));
    mMidFilter.SetParams(recursive_linear_filter::BiquadParams(mSampleRate, mMidFrequency,
                                                               mMid < 5.0 ? 1.5 : 0.7,
                                                               3.0 * (mMid - 5.0)));
    mTrebleFilter.SetParams(recursive_linear_filter::BiquadParams(mSampleRate, 1800.0, 0.707, 2.0 * (mTreble - 5.0)));
  }

  double mSampleRate = 0.0;
  double mBass = 5.0;
  double mMid = 5.0;
  double mMidFrequency = 650.0;
  double mTreble = 5.0;

  recursive_linear_filter::LowShelf mBassFilter;
  recursive_linear_filter::Peaking mMidFilter;
  recursive_linear_filter::HighShelf mTrebleFilter;
};

// 1176-style FET feedback compressor. See docs/design/effects/compressor-1176.md.
//
// Topology:
//   in -> [input gain w/ FET soft-clip] -+-> [VCA] -> [output gain] -> out
//                                        |             ^
//                                        +-> peak detector (asymmetric attack/release,
//                                            program-dependent release blend)
//                                              -> static curve (fixed 4:1 ratio with soft knee)
//
// API: SetParams keeps the historical 7-arg signature (amount, ratio, attack, release, mix,
// level, sampleRate) for state/API compatibility. Internally:
//   - `ratio` is ignored (locked at 4:1).
//   - `attackMs` is accepted in 0.02..30 ms range; the new EParam range is 0.02..1.0 ms but
//      we accept the wider legacy range for back-compat with old preset chunks.
//   - `releaseMs` accepted in 20..1100 ms.
//   - `mix` still functional so dry-passthrough tests work; UI locks it at 1.0.
class VoLumCompressor : public DSP
{
public:
  static constexpr double kFixedRatio = 4.0;
  static constexpr double kUnityOutputCalibrationDb = -5.0;

  void SetParams(double amount, double ratio, double attackMs, double releaseMs, double mix, double levelDb,
                 double sampleRate)
  {
    (void) ratio; // 1176-style: ratio fixed at 4:1 internally; API arg retained for state compat.
    mAmount = std::clamp(amount, 0.0, 10.0);
    mAttackMs = std::clamp(attackMs, 0.02, 30.0);
    mReleaseMs = std::clamp(releaseMs, 20.0, 1100.0);
    mMix = std::clamp(mix, 0.0, 1.0);
    // Output=0 dB should feel like a calibrated compressor unity point, not the raw
    // 1176-style make-up gain. The user-facing knob remains centered at 0 dB.
    mLevel = std::pow(10.0, (std::clamp(levelDb, -20.0, 20.0) + kUnityOutputCalibrationDb) / 20.0);
    if (mSampleRate != sampleRate)
    {
      mSampleRate = sampleRate;
      mEnvelope = 0.0;
      mEnvelopeSlow = 0.0;
    }
  }

  void Reset()
  {
    mEnvelope = 0.0;
    mEnvelopeSlow = 0.0;
  }

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const size_t numChannels, const size_t numFrames) override
  {
    _PrepareBuffers(numChannels, numFrames);

    if (mSampleRate <= 0.0)
      mSampleRate = 48000.0;

    // Input drive: 0..10 -> +0..+24 dB pre-detector gain, exponentially.
    const double inputDriveDb = mAmount * 2.4;
    const double inputDriveGain = std::pow(10.0, inputDriveDb / 20.0);

    // Threshold scales mildly with input drive so we still see meaningful GR at low drive.
    const double thresholdDb = -22.0 + (10.0 - mAmount) * 0.6; // ~-22 dB at amount=10, ~-16 dB at amount=0
    const double softKneeDb = 6.0;
    const double makeupDb = mAmount * 0.55; // gentle automatic make-up so output stays loud
    const double makeup = std::pow(10.0, makeupDb / 20.0);

    // Asymmetric envelope coefficients.
    const double attack = std::exp(-1.0 / ((mAttackMs / 1000.0) * mSampleRate));
    const double releaseFast = std::exp(-1.0 / ((mReleaseMs / 1000.0) * mSampleRate));
    // Program-dependent release: a slower secondary stage that blends in for sustained GR.
    const double releaseSlow = std::exp(-1.0 / ((mReleaseMs * 4.0 / 1000.0) * mSampleRate));

    // FET-style asymmetric pre-detector saturation. y = tanh(a*x + b) - tanh(b)
    // with small positive `b` adds a touch of even-order harmonics and asymmetric clip.
    constexpr double kFetA = 1.0;
    constexpr double kFetB = 0.06;
    const double fetTanhB = std::tanh(kFetB);

    auto fet = [&](double x) {
      // Drive scales the saturation amount; at high drive, character becomes clearly audible.
      const double drive = 1.0 + (inputDriveGain - 1.0) * 0.5;
      return std::tanh(kFetA * drive * x + kFetB) - fetTanhB;
    };

    for (size_t s = 0; s < numFrames; ++s)
    {
      // Detect on the louder channel after pre-detector saturation, mirroring 1176 behavior
      // where the FET sees the program-shaped signal.
      double detector = 0.0;
      for (size_t c = 0; c < numChannels; ++c)
      {
        const double driven = fet(static_cast<double>(inputs[c][s]) * inputDriveGain);
        detector = std::max(detector, std::abs(driven));
      }

      // Asymmetric envelope follower (peak).
      const double targetEnv = detector;
      const double coeff = targetEnv > mEnvelope ? attack : releaseFast;
      mEnvelope = coeff * mEnvelope + (1.0 - coeff) * targetEnv;

      // Slow envelope tracks more conservatively for program-dependent release.
      const double slowCoeff = targetEnv > mEnvelopeSlow ? attack : releaseSlow;
      mEnvelopeSlow = slowCoeff * mEnvelopeSlow + (1.0 - slowCoeff) * targetEnv;

      // Use the larger of fast and slow envelopes for the gain-reduction calculation.
      // When the slow envelope is heavily charged from sustained material, release feels
      // gentler than the fast one alone.
      const double envForGr = std::max(mEnvelope, mEnvelopeSlow);
      const double envDb = 20.0 * std::log10(std::max(envForGr, 1.0e-9));

      double gainDb = 0.0;
      const double over = envDb - thresholdDb;
      if (over > softKneeDb * 0.5)
      {
        // Above knee
        gainDb = -(over - over / kFixedRatio);
      }
      else if (over > -softKneeDb * 0.5)
      {
        // Inside soft knee: quadratic interpolation
        const double kneePos = (over + softKneeDb * 0.5) / softKneeDb; // 0..1
        const double slope = 1.0 - 1.0 / kFixedRatio;
        const double kneeOver = over;
        gainDb = -slope * (kneeOver + softKneeDb * 0.5) * (kneeOver + softKneeDb * 0.5) / (2.0 * softKneeDb);
        (void) kneePos;
      }
      // Otherwise no compression (below knee region).

      const double gain = std::pow(10.0, gainDb / 20.0) * makeup;

      for (size_t c = 0; c < numChannels; ++c)
      {
        const double dry = static_cast<double>(inputs[c][s]);
        // Wet path: drive into FET, then apply VCA gain, then output level.
        const double driven = fet(dry * inputDriveGain);
        const double wet = driven * gain * mLevel;
        double out = dry * (1.0 - mMix) + wet * mMix;
        if (!std::isfinite(out))
          out = 0.0;
        mOutputs[c][s] = static_cast<DSP_SAMPLE>(out);
      }
    }

    return _GetPointers();
  }

private:
  double mSampleRate = 0.0;
  double mAmount = 3.0;
  double mAttackMs = 0.4;
  double mReleaseMs = 250.0;
  double mMix = 1.0;
  double mLevel = 1.0;
  double mEnvelope = 0.0;
  double mEnvelopeSlow = 0.0;
};

} // namespace effect
} // namespace dsp
