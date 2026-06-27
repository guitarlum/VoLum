#pragma once

// VoLum Pitch pedal engine (PRE, before compressor).
//
// Time-domain GRANULAR pitch shifter (dual-tap delay line with Hann crossfade).
// Chosen over an STFT phase vocoder because, for low guitar strings at low
// latency, a phase vocoder either drifts ("detunes while it rings") or needs
// ~64-85 ms of latency to stay stable. The granular shifter resamples a delay
// line so the pitch is exact (no drift) at ~21 ms latency, which matches how
// drop-tune pedals (DigiTech The Drop, Ola Chug Capo) behave. The tradeoff is
// some amplitude "warble" instead of detuning.
//
// How it works (per voice):
//   - A ring buffer holds recent input.
//   - Two read taps sit grain/2 apart, each Hann-windowed; the windows sum to 1
//     (perfect reconstruction), so there is no amplitude modulation from the
//     windowing itself.
//   - Each output sample, the read pointer advances at the pitch RATIO relative
//     to the write pointer (that is the exact resampling that shifts pitch), and
//     the tap phase increments by (1 - ratio), wrapping over [0, grain). When a
//     tap wraps, its window is ~0 there and the other tap covers the seam.
//   - Latency = grain/2 (group delay). The dry path is delayed by the same so
//     dry/wet stay time-aligned; the host compensates the total via PDC.
//
// Two modes share the engine:
//   Transpose: one voice at 2^(semitones/12), with dry/wet MIX and LEVEL.
//   Octaver:   dry + (-1 octave voice * octDown) + (+1 octave voice * octUp),
//              with a Vintage (gritty/filtered) vs Modern (clean) voicing.
//
// Mono in/out (numChannels == 1; VoLum is mono internally).

#include "../AudioDSPTools/dsp/dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dsp
{
namespace effect
{

// Single-voice granular pitch shifter over a ring buffer. Exact-pitch (the read
// pointer resamples the delay line); the dual Hann taps overlap-add seamlessly.
class GranularVoice
{
public:
  void Configure(int grainSamples, int maxBlockSize)
  {
    mGrain = std::max(grainSamples, 64);
    const size_t need = static_cast<size_t>(mGrain) + static_cast<size_t>(std::max(maxBlockSize, 64)) + 8;
    if (mBuf.size() < need)
      mBuf.assign(need, 0.0);
    Reset();
  }

  void Reset()
  {
    std::fill(mBuf.begin(), mBuf.end(), 0.0);
    mWrite = 0;
    mPhase = 0.0;
  }

  // ratio = output_freq / input_freq (2^(semitones/12)).
  void SetRatio(double ratio) { mInc = 1.0 - std::clamp(ratio, 0.25, 4.0); }

  int Latency() const { return mGrain / 2; }

  void Process(const DSP_SAMPLE* in, DSP_SAMPLE* out, size_t numFrames)
  {
    if (mBuf.empty())
    {
      std::copy(in, in + numFrames, out);
      return;
    }
    const size_t sz = mBuf.size();
    const double g = static_cast<double>(mGrain);
    const double twoPiOverG = 2.0 * 3.14159265358979323846 / g;
    for (size_t i = 0; i < numFrames; ++i)
    {
      mBuf[mWrite] = static_cast<double>(in[i]);
      const double p1 = mPhase;
      double p2 = mPhase + g * 0.5;
      if (p2 >= g)
        p2 -= g;
      const double w1 = 0.5 * (1.0 - std::cos(twoPiOverG * p1));
      const double w2 = 0.5 * (1.0 - std::cos(twoPiOverG * p2));
      out[i] = static_cast<DSP_SAMPLE>(_ReadAt(p1) * w1 + _ReadAt(p2) * w2);
      mPhase += mInc;
      while (mPhase >= g)
        mPhase -= g;
      while (mPhase < 0.0)
        mPhase += g;
      mWrite = (mWrite + 1) % sz;
    }
  }

private:
  double _ReadAt(double delay) const
  {
    const double bufLen = static_cast<double>(mBuf.size());
    double rp = static_cast<double>(mWrite) - delay;
    while (rp < 0.0)
      rp += bufLen;
    const double fl = std::floor(rp);
    const size_t i0 = static_cast<size_t>(fl) % mBuf.size();
    const size_t i1 = (i0 + 1) % mBuf.size();
    const double frac = rp - fl;
    return mBuf[i0] * (1.0 - frac) + mBuf[i1] * frac;
  }

  int mGrain = 0;
  std::vector<double> mBuf;
  size_t mWrite = 0;
  double mPhase = 0.0;
  double mInc = 0.0; // 1 - ratio
};

class VoLumPitch
{
public:
  enum class Mode
  {
    Transpose = 0,
    Octaver = 1
  };
  enum class Voicing
  {
    Vintage = 0,
    Modern = 1
  };

  static constexpr int kNumVoices = 2; // octaver uses both (down/up); transpose uses voice 0.

  // Grain length as a fraction of the sample rate. grain/2 is the reported
  // latency (~21 ms at 48 kHz), matching the fast end of real drop-tune pedals
  // while giving enough overlap to keep low strings stable.
  static int GrainSamplesFor(double sampleRate)
  {
    int g = static_cast<int>(std::lround(sampleRate * 0.045));
    if (g % 2 != 0)
      ++g;
    return std::max(g, 256);
  }

  // Allocates - call OFF the audio thread (OnReset), never from ProcessBlock.
  void Configure(double sampleRate, int maxBlockSize)
  {
    mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    mMaxBlock = std::max(maxBlockSize, 64);
    const int grain = GrainSamplesFor(mSampleRate);
    const bool sameConfig = mConfigured && grain == mConfiguredGrain && mSampleRate == mConfiguredSampleRate;
    if (!sameConfig)
    {
      for (auto& voice : mVoices)
        voice.Configure(grain, mMaxBlock);
      mConfiguredGrain = grain;
      mConfiguredSampleRate = mSampleRate;
      mConfigured = true;
      mLatency = mVoices[0].Latency();
    }
    const double fc = 3200.0;
    mVintageLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fc / mSampleRate);
    _AllocateScratch();
  }

  int Latency() const { return mLatency; }
  bool Configured() const { return mConfigured; }

  void Reset()
  {
    for (auto& voice : mVoices)
      voice.Reset();
    std::fill(mDryRing.begin(), mDryRing.end(), static_cast<DSP_SAMPLE>(0));
    mDryWrite = 0;
    mVintageLpState = {0.0, 0.0};
  }

  void SetParams(Mode mode, double semitones, double mix01, double octDown01, double octUp01, double dry01,
                 Voicing voicing, double levelDb)
  {
    mMode = mode;
    mSemitones = std::clamp(semitones, -24.0, 24.0);
    mMix = std::clamp(mix01, 0.0, 1.0);
    mOctDown = std::clamp(octDown01, 0.0, 1.0);
    mOctUp = std::clamp(octUp01, 0.0, 1.0);
    mDry = std::clamp(dry01, 0.0, 1.0);
    mVoicing = voicing;
    const double clampedDb = std::clamp(levelDb, -20.0, 20.0);
    mLevel = std::pow(10.0, clampedDb / 20.0);
  }

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, size_t numChannels, size_t numFrames)
  {
    _PrepareIO(numChannels, numFrames);

    if (!mConfigured || numChannels == 0)
    {
      for (size_t c = 0; c < numChannels; ++c)
        std::copy(inputs[c], inputs[c] + numFrames, mOut[c].begin());
      return _Pointers(numChannels);
    }

    DSP_SAMPLE* in = inputs[0];

    // Delay dry by engine latency so it stays time-aligned with the wet voices.
    const size_t ringLen = mDryRing.size();
    const size_t lat = static_cast<size_t>(mLatency);
    for (size_t i = 0; i < numFrames; ++i)
    {
      mDryRing[mDryWrite] = in[i];
      mDryScratch[i] = mDryRing[(mDryWrite + ringLen - lat) % ringLen];
      mDryWrite = (mDryWrite + 1) % ringLen;
    }

    if (mMode == Mode::Transpose)
    {
      mVoices[0].SetRatio(std::pow(2.0, mSemitones / 12.0));
      mVoices[0].Process(in, mWet0.data(), numFrames);
      for (size_t i = 0; i < numFrames; ++i)
      {
        const double y = mDryScratch[i] * (1.0 - mMix) + static_cast<double>(mWet0[i]) * mMix;
        mOut[0][i] = static_cast<DSP_SAMPLE>(y * mLevel);
      }
    }
    else // Octaver
    {
      mVoices[0].SetRatio(0.5);
      mVoices[1].SetRatio(2.0);
      mVoices[0].Process(in, mWet0.data(), numFrames);
      mVoices[1].Process(in, mWet1.data(), numFrames);
      for (size_t i = 0; i < numFrames; ++i)
      {
        double down = static_cast<double>(mWet0[i]);
        double up = static_cast<double>(mWet1[i]);
        if (mVoicing == Voicing::Vintage)
        {
          down = _VintageShape(down, 0);
          up = _VintageShape(up, 1);
        }
        const double wet = down * mOctDown + up * mOctUp;
        const double y = mDryScratch[i] * mDry + wet;
        mOut[0][i] = static_cast<DSP_SAMPLE>(y * mLevel);
      }
    }

    for (size_t c = 0; c < numChannels; ++c)
      for (size_t i = 0; i < numFrames; ++i)
        if (!std::isfinite(static_cast<double>(mOut[c][i])))
          mOut[c][i] = static_cast<DSP_SAMPLE>(0);

    return _Pointers(numChannels);
  }

private:
  double _VintageShape(double x, int idx)
  {
    // Asymmetric soft grit (emulates analog octave-divider character) + lowpass.
    const double driven = std::tanh(x * 1.8);
    mVintageLpState[idx] = mVintageLpCoeff * driven + (1.0 - mVintageLpCoeff) * mVintageLpState[idx];
    return mVintageLpState[idx];
  }

  void _AllocateScratch()
  {
    const size_t cap = static_cast<size_t>(mMaxBlock);
    if (mWet0.size() < cap)
    {
      mWet0.assign(cap, static_cast<DSP_SAMPLE>(0));
      mWet1.assign(cap, static_cast<DSP_SAMPLE>(0));
      mDryScratch.assign(cap, static_cast<DSP_SAMPLE>(0));
    }
    const size_t ringNeed = static_cast<size_t>(mLatency) + cap + 1;
    if (mDryRing.size() < ringNeed)
    {
      mDryRing.assign(ringNeed, static_cast<DSP_SAMPLE>(0));
      mDryWrite = 0;
    }
    for (size_t c = 0; c < kMaxChannels; ++c)
      if (mOut[c].size() < cap)
        mOut[c].assign(cap, static_cast<DSP_SAMPLE>(0));
  }

  void _PrepareIO(size_t numChannels, size_t numFrames)
  {
    if (static_cast<int>(numFrames) > mMaxBlock || mWet0.size() < numFrames)
    {
      mMaxBlock = std::max(mMaxBlock, static_cast<int>(numFrames));
      for (auto& voice : mVoices)
        voice.Configure(mConfiguredGrain > 0 ? mConfiguredGrain : GrainSamplesFor(mSampleRate), mMaxBlock);
      _AllocateScratch();
    }
    (void)numChannels;
  }

  DSP_SAMPLE** _Pointers(size_t numChannels)
  {
    for (size_t c = 0; c < numChannels && c < kMaxChannels; ++c)
      mOutPtrs[c] = mOut[c].data();
    return mOutPtrs.data();
  }

  static constexpr size_t kMaxChannels = 2;

  std::array<GranularVoice, kNumVoices> mVoices;

  double mSampleRate = 0.0;
  double mConfiguredSampleRate = 0.0;
  int mConfiguredGrain = 0;
  int mMaxBlock = 0;
  int mLatency = 0;
  bool mConfigured = false;

  Mode mMode = Mode::Transpose;
  Voicing mVoicing = Voicing::Modern;
  double mSemitones = 0.0;
  double mMix = 1.0;
  double mOctDown = 0.0;
  double mOctUp = 0.0;
  double mDry = 1.0;
  double mLevel = 1.0;

  double mVintageLpCoeff = 0.3;
  std::array<double, kNumVoices> mVintageLpState{0.0, 0.0};

  std::vector<DSP_SAMPLE> mWet0, mWet1, mDryScratch, mDryRing;
  size_t mDryWrite = 0;
  std::array<std::vector<DSP_SAMPLE>, kMaxChannels> mOut;
  std::array<DSP_SAMPLE*, kMaxChannels> mOutPtrs{nullptr, nullptr};
};

} // namespace effect
} // namespace dsp
