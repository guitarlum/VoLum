#pragma once

// The resampling wrapper every loaded NAM runs inside (main, support, PRE slots).
// Keep it free of the plugin class: test_volum_golden_render.cpp renders through it.

// ResamplingContainer uses iplug::PI and an unqualified DEFAULT_BLOCK_SIZE
// without including either; a conforming (two-phase) compile needs both first.
// In the plugin DEFAULT_BLOCK_SIZE is already IPlugMidi.h's macro.
#include "../iPlug2/IPlug/IPlugConstants.h"
#ifndef DEFAULT_BLOCK_SIZE
namespace dsp
{
using iplug::DEFAULT_BLOCK_SIZE;
}
#endif
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"
#include "../NeuralAmpModelerCore/NAM/slimmable.h"

#include "VoLumDspStagingWdl.h"

#include <cmath>
#include <functional>
#include <memory>

// Get the sample rate of a NAM model.
// Sometimes, the model doesn't know its own sample rate; this wrapper guesses 48k based on the way that most
// people have used NAM in the past.
inline double GetNAMSampleRate(const std::unique_ptr<nam::DSP>& model)
{
  // Some models are from when we didn't have sample rate in the model.
  // For those, this wraps with the assumption that they're 48k models, which is probably true.
  const double assumedSampleRate = 48000.0;
  const double reportedEncapsulatedSampleRate = model->GetExpectedSampleRate();
  const double encapsulatedSampleRate =
    reportedEncapsulatedSampleRate <= 0.0 ? assumedSampleRate : reportedEncapsulatedSampleRate;
  return encapsulatedSampleRate;
};

class ResamplingNAM : public nam::DSP
{
public:
  // Resampling wrapper around the NAM models
  ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated, const double expected_sample_rate)
  : nam::DSP(1, 1, expected_sample_rate)
  , mEncapsulated(std::move(encapsulated))
  , mResampler(GetNAMSampleRate(mEncapsulated))
  {
    // Assign the encapsulated object's processing function to this object's member so that the resampler can use it:
    auto ProcessBlockFunc = [&](NAM_SAMPLE** input, NAM_SAMPLE** output, int numFrames) {
      mEncapsulated->process(input, output, numFrames);
    };
    mBlockProcessFunc = ProcessBlockFunc;

    // Get the other information from the encapsulated NAM so that we can tell the outside world about what we're
    // holding.
    if (mEncapsulated->HasLoudness())
    {
      SetLoudness(mEncapsulated->GetLoudness());
    }
    if (mEncapsulated->HasInputLevel())
    {
      SetInputLevel(mEncapsulated->GetInputLevel());
    }
    if (mEncapsulated->HasOutputLevel())
    {
      SetOutputLevel(mEncapsulated->GetOutputLevel());
    }

    // NOTE: prewarm samples doesn't mean anything--we can prewarm the encapsulated model as it likes and be good to
    // go.
    // _prewarm_samples = 0;

    // And be ready
    int maxBlockSize = 2048; // Conservative
    Reset(expected_sample_rate, maxBlockSize);
  };

  ~ResamplingNAM() = default;

  void prewarm() override { mEncapsulated->prewarm(); };

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    // Hosts grow the callback without OnReset. Throw would unwind ProcessBlock
    // (and leak the host FP env); allocate is also forbidden here. The model
    // stays Reset at the host block (see NamResetBlockSize), so a bigger block
    // is split rather than growing the model's buffers.
    volum::dsp_staging::ProcessNamInChunks(
      num_frames, mMaxExternalBlockSize, input[0], output[0], [this](NAM_SAMPLE** in, NAM_SAMPLE** out, int n) {
        if (!NeedToResample())
          mEncapsulated->process(in, out, n);
        else
          mResampler.ProcessBlock(in, out, n, mBlockProcessFunc);
      });
  };

  void process(NAM_SAMPLE* input, NAM_SAMPLE* output, const int num_frames)
  {
    NAM_SAMPLE* inputPtrs[1] = {input};
    NAM_SAMPLE* outputPtrs[1] = {output};
    process(inputPtrs, outputPtrs, num_frames);
  };

  int GetLatency() const { return NeedToResample() ? mResampler.GetLatency() : 0; };

  void Reset(const double sampleRate, const int maxBlockSize) override
  {
    mExpectedSampleRate = sampleRate;
    mMaxExternalBlockSize = maxBlockSize;
    mResampler.Reset(sampleRate, maxBlockSize);

    // Allocations in the encapsulated model (HACK)
    // Stolen some code from the resampler; it'd be nice to have these exposed as methods? :)
    const double mUpRatio = sampleRate / GetEncapsulatedSampleRate();
    const auto maxEncapsulatedBlockSize = static_cast<int>(std::ceil(static_cast<double>(maxBlockSize) / mUpRatio));
    mEncapsulated->ResetAndPrewarm(sampleRate, maxEncapsulatedBlockSize);
  };

  // So that we can let the world know if we're resampling (useful for debugging)
  double GetEncapsulatedSampleRate() const { return GetNAMSampleRate(mEncapsulated); };

  // VoLum: if the encapsulated model is a slimmable container (A2), select its
  // Lite (val < 0.5) or Full (val >= 0.5) slice. Plain (non-slimmable) models
  // no-op gracefully. The container prepares the inactive slice under its own
  // mutex; call this off the audio thread (loader thread / staging), not in
  // ProcessBlock.
  void SetSlimmableSize(const double val)
  {
    if (auto* slim = dynamic_cast<nam::SlimmableModel*>(mEncapsulated.get()))
      slim->SetSlimmableSize(val);
  };

private:
  bool NeedToResample() const { return GetExpectedSampleRate() != GetEncapsulatedSampleRate(); };
  // The encapsulated NAM
  std::unique_ptr<nam::DSP> mEncapsulated;

  // The resampling wrapper
  dsp::ResamplingContainer<NAM_SAMPLE, 1, 12> mResampler;

  // Used to check that we don't get too large a block to process.
  int mMaxExternalBlockSize = 0;

  // This function is defined to conform to the interface expected by the iPlug2 resampler.
  std::function<void(NAM_SAMPLE**, NAM_SAMPLE**, int)> mBlockProcessFunc;
};
