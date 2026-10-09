#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace volum
{

// Latency in samples of each NAM lane's live model, or kNoModel when the lane has none.
struct LaneLatencies
{
  static constexpr int kNoModel = -1;

  int main = kNoModel;
  int support = kNoModel;
  int pre[2] = {kNoModel, kNoModel};
};

// The live model pointers belong to whoever holds the reset exclusion (the audio
// thread in ProcessBlock, OnReset otherwise); every other thread reads their
// latencies from here. All four lanes share one atomic word so a reader never mixes
// lanes from two different publishes.
class LiveLatencySnapshot
{
public:
  static constexpr int kMaxLatency = 0xFFFE;

  void Publish(const LaneLatencies& lanes) { mPacked.store(Pack(lanes), std::memory_order_release); }
  [[nodiscard]] LaneLatencies Read() const { return Unpack(mPacked.load(std::memory_order_acquire)); }

private:
  static constexpr std::uint64_t kNoModelBits = 0xFFFF;

  static std::uint64_t PackLane(int latency)
  {
    return latency < 0 ? kNoModelBits : static_cast<std::uint64_t>(std::min(latency, kMaxLatency));
  }
  static int UnpackLane(std::uint64_t bits)
  {
    bits &= kNoModelBits;
    return bits == kNoModelBits ? LaneLatencies::kNoModel : static_cast<int>(bits);
  }
  static std::uint64_t Pack(const LaneLatencies& lanes)
  {
    return PackLane(lanes.main) | (PackLane(lanes.support) << 16) | (PackLane(lanes.pre[0]) << 32)
           | (PackLane(lanes.pre[1]) << 48);
  }
  static LaneLatencies Unpack(std::uint64_t packed)
  {
    LaneLatencies lanes;
    lanes.main = UnpackLane(packed);
    lanes.support = UnpackLane(packed >> 16);
    lanes.pre[0] = UnpackLane(packed >> 32);
    lanes.pre[1] = UnpackLane(packed >> 48);
    return lanes;
  }

  std::atomic<std::uint64_t> mPacked{Pack(LaneLatencies{})};
};

} // namespace volum
