#pragma once

// VoLum NaN/Inf guard.
//
// One bad NAM capture, a runaway IIR, or a denormal-stuck stage can emit NaN
// or +/-Inf into the main signal bus. Without scrubbing, the poison sample
// flows into the noise gate, tone stack, IR convolver, delay ring, and reverb
// FDN tank, and stays there forever because each of those stages reads its
// own state at every block. This header centralizes the in-place scrub and
// the "anything bad happened in this block?" return value the audio thread
// uses to decide whether to also Reset() the POST effects.
//
// RT-safe: no allocation, no exceptions, no logging.

#include <cmath>
#include <cstddef>

namespace volum
{

template <typename Sample>
inline bool ScrubNonFiniteInPlace(Sample* buf, std::size_t numFrames)
{
  if (buf == nullptr)
    return false;
  bool foundNonFinite = false;
  for (std::size_t i = 0; i < numFrames; ++i)
  {
    if (!std::isfinite(static_cast<double>(buf[i])))
    {
      buf[i] = Sample{0};
      foundNonFinite = true;
    }
  }
  return foundNonFinite;
}

template <typename Sample>
inline bool ScrubNonFiniteInPlace(Sample** channels, std::size_t numChannels, std::size_t numFrames)
{
  if (channels == nullptr)
    return false;
  bool foundNonFinite = false;
  for (std::size_t c = 0; c < numChannels; ++c)
  {
    if (ScrubNonFiniteInPlace(channels[c], numFrames))
      foundNonFinite = true;
  }
  return foundNonFinite;
}

} // namespace volum
