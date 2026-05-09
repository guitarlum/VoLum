#pragma once

#include <cstddef>

namespace volum::process_io
{

// Collapse external inputs to mono with per-channel gain. When appApi is false (DAW build),
// gain is divided by nChansIn so stereo sources do not sum to double level.
template<typename Sample>
inline void MixExternalInputsToMono(Sample* const* inputs, std::size_t nFrames, std::size_t nChansIn, double baseGain,
                                    bool appApi, Sample* monoOut)
{
  double gain = baseGain;
  if (!appApi && nChansIn > 0)
    gain /= static_cast<double>(nChansIn);

  for (std::size_t c = 0; c < nChansIn; ++c)
  {
    for (std::size_t s = 0; s < nFrames; ++s)
    {
      const double contrib = gain * static_cast<double>(inputs[c][s]);
      if (c == 0)
        monoOut[s] = static_cast<Sample>(contrib);
      else
        monoOut[s] = static_cast<Sample>(static_cast<double>(monoOut[s]) + contrib);
    }
  }
}

// Broadcast internal mono to all output channels with output gain. Identical behavior in
// standalone and DAW: no clamp here. Final-bus bounding is the master safety stage at the
// end of ProcessBlock (see VoLumMasterSafety.h); clamping mid-chain before Delay/Reverb
// would have been ineffective and asymmetric.
template<typename Sample>
inline void ApplyOutputGainBroadcast(const Sample* monoIn, Sample* const* outputs, std::size_t nFrames,
                                     std::size_t nChansOut, double outGain, bool /*appApi*/)
{
  for (std::size_t cout = 0; cout < nChansOut; ++cout)
  {
    for (std::size_t s = 0; s < nFrames; ++s)
    {
      const double y = outGain * static_cast<double>(monoIn[s]);
      outputs[cout][s] = static_cast<Sample>(y);
    }
  }
}

template<typename Sample>
inline void ClearBuffers(Sample* const* outputs, std::size_t nFrames, std::size_t nChansOut)
{
  for (std::size_t c = 0; c < nChansOut; ++c)
    for (std::size_t s = 0; s < nFrames; ++s)
      outputs[c][s] = static_cast<Sample>(0);
}

} // namespace volum::process_io
