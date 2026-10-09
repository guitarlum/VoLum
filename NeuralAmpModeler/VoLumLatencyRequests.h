#pragma once

#include <atomic>
#include <cstdint>

#include "VoLumParams.h"

namespace volum
{

// Every param _ReportedLatencySamples reads (PRE Pitch mode and CHARACTER change the
// granular engine's latency). A change to any of them asks for a recompute;
// test_volum_reset_exclusion.cpp checks the list against that function.
inline bool ParamAffectsReportedLatency(int paramIdx)
{
  switch (paramIdx)
  {
    case kDualAmpActive:
    case kPreNam1Active:
    case kPreNam1Capture:
    case kPreNam2Active:
    case kPreNam2Capture:
    case kPrePitchActive:
    case kPrePitchMode:
    case kPrePitchTransChar: return true;
    default: return false;
  }
}

// Any thread (the audio thread included) asks for a host latency recompute by
// bumping a generation; only the main thread services it. The generation is read
// before the inputs, so a request that lands during a recompute stays pending and
// is serviced again: no request is ever lost, and nothing the main thread writes
// can hide one.
class LatencyRecomputeRequests
{
public:
  void Request() { mRequested.fetch_add(1, std::memory_order_acq_rel); }
  [[nodiscard]] bool Pending() const { return mRequested.load(std::memory_order_acquire) != mHandled; }

  // Main thread. Recomputes once if anything is pending; returns whether it did.
  template <typename Compute, typename Apply>
  bool Service(Compute&& compute, Apply&& apply)
  {
    const std::uint32_t generation = mRequested.load(std::memory_order_acquire);
    if (generation == mHandled)
      return false;
    apply(compute());
    mHandled = generation;
    return true;
  }

private:
  std::atomic<std::uint32_t> mRequested{0};
  std::uint32_t mHandled = 0;
};

} // namespace volum
