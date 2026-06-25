#pragma once

// VoLum Pitch pedal engine (PRE, before compressor).
//
// Two modes share one polyphonic pitch engine (Signalsmith Stretch, MIT):
//   Transpose: shift the whole signal by N semitones, dry/wet mix (doubles as a
//              fixed harmonizer when blended).
//   Octaver:   dry + (-1 octave voice * octDown) + (+1 octave voice * octUp),
//              with a Vintage (gritty/filtered) vs Modern (clean) voicing.
//
// Mono in/out (numChannels == 1; VoLum is mono internally). The engine block
// size is exposed as "quality": larger block = smoother but more latency
// (see VoLumPitch::BlockSamplesForQuality). The dry path is delayed by the
// engine latency so dry/wet stay phase-aligned; the host compensates the total
// via PDC (NeuralAmpModeler::_UpdateLatency).

#include "signalsmith-stretch/signalsmith-stretch.h"

#include "../AudioDSPTools/dsp/dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dsp
{
namespace effect
{

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

  // Quality (0..1) -> FFT block size. Larger = smoother low-string tracking but
  // more latency. Discrete so latency only changes in deliberate steps.
  // At 48 kHz the totals are roughly: 512=~11ms, 768=~16ms, 1024=~21ms,
  // 1536=~32ms, 2048=~43ms. Default index 2 (1024, ~21ms).
  static int BlockSamplesForQuality(double quality01)
  {
    static const int kBlocks[] = {512, 768, 1024, 1536, 2048};
    constexpr int n = 5;
    const int idx = static_cast<int>(std::lround(std::clamp(quality01, 0.0, 1.0) * (n - 1)));
    return kBlocks[std::clamp(idx, 0, n - 1)];
  }

  // Reconfigure the engine. Allocates - call OFF the audio thread (OnReset / a
  // guarded reconfigure path), never from ProcessBlock.
  void Configure(double sampleRate, double quality01, int maxBlockSize)
  {
    mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    mMaxBlock = std::max(maxBlockSize, 64);
    const int block = BlockSamplesForQuality(quality01);
    const bool sameConfig = mConfigured && block == mConfiguredBlock && mSampleRate == mConfiguredSampleRate;
    if (!sameConfig)
    {
      for (auto& voice : mVoices)
      {
        voice.configure(1, block, block / 4);
        voice.reset();
      }
      mConfiguredBlock = block;
      mConfiguredSampleRate = mSampleRate;
      mConfigured = true;
      mLatency = mVoices[0].inputLatency() + mVoices[0].outputLatency();
    }
    // Vintage voicing lowpass coefficient (~3.2 kHz one-pole).
    const double fc = 3200.0;
    mVintageLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fc / mSampleRate);
    _AllocateScratch();
  }

  int Latency() const { return mLatency; }
  bool Configured() const { return mConfigured; }

  void Reset()
  {
    for (auto& voice : mVoices)
      voice.reset();
    std::fill(mDryRing.begin(), mDryRing.end(), static_cast<DSP_SAMPLE>(0));
    mDryWrite = 0;
    mVintageLpState = {0.0, 0.0};
  }

  void SetParams(Mode mode, double semitones, double mix01, double octDown01, double octUp01, double dry01,
                 Voicing voicing, double levelDb)
  {
    mMode = mode;
    mSemitones = std::clamp(semitones, -12.0, 12.0);
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

    // Delay dry by engine latency so it stays phase-aligned with the wet voices.
    const size_t ringLen = mDryRing.size();
    const size_t lat = static_cast<size_t>(mLatency);
    for (size_t i = 0; i < numFrames; ++i)
    {
      mDryRing[mDryWrite] = in[i];
      mDryScratch[i] = mDryRing[(mDryWrite + ringLen - lat) % ringLen];
      mDryWrite = (mDryWrite + 1) % ringLen;
    }

    DSP_SAMPLE* inPtr[1] = {in};

    if (mMode == Mode::Transpose)
    {
      mVoices[0].setTransposeSemitones(static_cast<float>(mSemitones));
      DSP_SAMPLE* wetPtr[1] = {mWet0.data()};
      mVoices[0].process(inPtr, static_cast<int>(numFrames), wetPtr, static_cast<int>(numFrames));
      for (size_t i = 0; i < numFrames; ++i)
      {
        const double y = mDryScratch[i] * (1.0 - mMix) + static_cast<double>(mWet0[i]) * mMix;
        mOut[0][i] = static_cast<DSP_SAMPLE>(y * mLevel);
      }
    }
    else // Octaver
    {
      mVoices[0].setTransposeSemitones(-12.0f);
      mVoices[1].setTransposeSemitones(12.0f);
      DSP_SAMPLE* downPtr[1] = {mWet0.data()};
      DSP_SAMPLE* upPtr[1] = {mWet1.data()};
      mVoices[0].process(inPtr, static_cast<int>(numFrames), downPtr, static_cast<int>(numFrames));
      mVoices[1].process(inPtr, static_cast<int>(numFrames), upPtr, static_cast<int>(numFrames));
      for (size_t i = 0; i < numFrames; ++i)
      {
        double down = static_cast<double>(mWet0[i]);
        double up = static_cast<double>(mWet1[i]);
        if (mVoicing == Voicing::Vintage)
        {
          down = _VintageShape(down, 0);
          up = _VintageShape(up, 1);
        }
        const double y = mDryScratch[i] * mDry + down * mOctDown + up * mOctUp;
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

  std::array<signalsmith::stretch::SignalsmithStretch<float>, kNumVoices> mVoices;

  double mSampleRate = 0.0;
  double mConfiguredSampleRate = 0.0;
  int mConfiguredBlock = 0;
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
