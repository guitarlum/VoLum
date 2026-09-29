#pragma once

// Audio-thread IR post: trim, then optional one-pole high-pass / low-pass.
// A cut Hz of 0 bypasses that stage. Shared by ProcessBlock and the doctest.

#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"

namespace volum
{

inline DSP_SAMPLE** ApplyIrShapingLane(DSP_SAMPLE** in, const size_t numChannels, const int nFrames,
                                       const double sampleRate, const double trimLin, const double lowHz,
                                       const double highHz, recursive_linear_filter::HighPass& lowCut,
                                       recursive_linear_filter::LowPass& highCut)
{
  if (trimLin != 1.0)
    for (size_t c = 0; c < numChannels; ++c)
      for (int i = 0; i < nFrames; ++i)
        in[c][i] = static_cast<DSP_SAMPLE>(static_cast<double>(in[c][i]) * trimLin);

  DSP_SAMPLE** p = in;
  if (lowHz > 0.0)
  {
    lowCut.SetParams(recursive_linear_filter::HighPassParams(sampleRate, lowHz));
    p = lowCut.Process(p, numChannels, static_cast<size_t>(nFrames));
  }
  if (highHz > 0.0)
  {
    highCut.SetParams(recursive_linear_filter::LowPassParams(sampleRate, highHz));
    p = highCut.Process(p, numChannels, static_cast<size_t>(nFrames));
  }
  return p;
}

} // namespace volum
