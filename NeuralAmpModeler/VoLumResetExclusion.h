#pragma once

// OnReset re-prepares every DSP object ProcessBlock touches (NAM rings, tone
// stacks, POST effect buffers, dual-amp scratch). AUv2 calls it on the main
// thread while render runs (sample rate, max frames, bypass, AudioUnitReset),
// so the two must never overlap. The reset side waits; the audio side never
// does: a block that loses the race is told to skip and outputs silence.
//
// No host call may be made while a reset holds the lock: SetLatency can re-enter
// OnReset on the same thread (VST3 restartComponent), and the mutex is not
// recursive. AAX calls OnReset on the audio thread before ProcessBlock, not from
// inside it, so that path never contends.

#include <mutex>

namespace volum
{

class ResetExclusion
{
public:
  [[nodiscard]] std::unique_lock<std::mutex> BeginReset() { return std::unique_lock<std::mutex>(mMutex); }

  // Audio thread. owns_lock() false means a reset is in flight: skip the block.
  [[nodiscard]] std::unique_lock<std::mutex> TryBeginBlock()
  {
    return std::unique_lock<std::mutex>(mMutex, std::try_to_lock);
  }

private:
  std::mutex mMutex;
};

} // namespace volum
