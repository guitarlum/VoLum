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

class VoLumCompressor : public DSP
{
public:
  void SetParams(double amount, double ratio, double attackMs, double releaseMs, double mix, double levelDb,
                 double sampleRate)
  {
    mAmount = std::clamp(amount, 0.0, 10.0);
    mRatio = std::clamp(ratio, 1.0, 20.0);
    mAttackMs = std::clamp(attackMs, 0.1, 30.0);
    mReleaseMs = std::clamp(releaseMs, 20.0, 800.0);
    mMix = std::clamp(mix, 0.0, 1.0);
    mLevel = std::pow(10.0, std::clamp(levelDb, -20.0, 20.0) / 20.0);
    if (mSampleRate != sampleRate)
    {
      mSampleRate = sampleRate;
      mEnvelope = 0.0;
    }
  }

  void Reset()
  {
    mEnvelope = 0.0;
  }

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const size_t numChannels, const size_t numFrames) override
  {
    _PrepareBuffers(numChannels, numFrames);

    if (mSampleRate <= 0.0)
      mSampleRate = 48000.0;

    const double thresholdDb = -28.0 + (10.0 - mAmount) * 1.8; // -10..-28 dB
    const double attack = std::exp(-1.0 / ((mAttackMs / 1000.0) * mSampleRate));
    const double release = std::exp(-1.0 / ((mReleaseMs / 1000.0) * mSampleRate));
    const double makeup = std::pow(10.0, (mAmount * 0.45) / 20.0);

    for (size_t s = 0; s < numFrames; ++s)
    {
      double detector = 0.0;
      for (size_t c = 0; c < numChannels; ++c)
        detector = std::max(detector, std::abs(static_cast<double>(inputs[c][s])));

      const double coeff = detector > mEnvelope ? attack : release;
      mEnvelope = coeff * mEnvelope + (1.0 - coeff) * detector;

      const double envDb = 20.0 * std::log10(std::max(mEnvelope, 1.0e-9));
      double gainDb = 0.0;
      if (envDb > thresholdDb)
      {
        const double over = envDb - thresholdDb;
        gainDb = -(over - over / mRatio);
      }

      const double gain = std::pow(10.0, gainDb / 20.0) * makeup * mLevel;
      for (size_t c = 0; c < numChannels; ++c)
      {
        const double dry = static_cast<double>(inputs[c][s]);
        const double wet = dry * gain;
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
  double mRatio = 4.0;
  double mAttackMs = 4.0;
  double mReleaseMs = 120.0;
  double mMix = 1.0;
  double mLevel = 1.0;
  double mEnvelope = 0.0;
};

} // namespace effect
} // namespace dsp
