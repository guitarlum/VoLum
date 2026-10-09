#pragma once

// Audio-thread IR post: trim, then optional one-pole high-pass / low-pass.
// A cut Hz of 0 bypasses that stage. Shared by ProcessBlock and the doctest.

#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"

#include <algorithm>

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

// The AudioDSPTools filters have no public way to drop their history. Zeroing it
// leaves the filter exactly as freshly constructed; it never allocates.
template <typename Filter>
class ClearableFilter : public Filter
{
public:
  void ClearHistory()
  {
    for (auto& h : this->mInputHistory)
      std::fill(h.begin(), h.end(), DSP_SAMPLE(0));
    for (auto& h : this->mOutputHistory)
      std::fill(h.begin(), h.end(), DSP_SAMPLE(0));
  }
};

// One IR lane's cut filters. A cut that is off, a lane that is not running, and
// a different convolver all leave history that would replay into the next
// sample the cut processes, so each of them starts the filters from silence.
class IrShapingLane
{
public:
  // `ir` names the convolver feeding the lane; a new one starts the cuts clean.
  DSP_SAMPLE** Process(DSP_SAMPLE** in, const size_t numChannels, const int nFrames, const double sampleRate,
                       const double trimLin, const double lowHz, const double highHz, const void* ir)
  {
    if (ir != mIr)
    {
      Reset();
      mIr = ir;
    }
    if (!(lowHz > 0.0))
      mLowCut.ClearHistory();
    if (!(highHz > 0.0))
      mHighCut.ClearHistory();
    return ApplyIrShapingLane(in, numChannels, nFrames, sampleRate, trimLin, lowHz, highHz, mLowCut, mHighCut);
  }

  // Every block the lane does not run, and on OnReset.
  void Reset()
  {
    mLowCut.ClearHistory();
    mHighCut.ClearHistory();
    mIr = nullptr;
  }

private:
  ClearableFilter<recursive_linear_filter::HighPass> mLowCut;
  ClearableFilter<recursive_linear_filter::LowPass> mHighCut;
  const void* mIr = nullptr;
};

} // namespace volum
