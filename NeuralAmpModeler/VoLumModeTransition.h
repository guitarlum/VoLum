#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>

namespace volum
{
enum class ModeSnapshotTarget : unsigned
{
  PrePitch = 0,
  Delay,
  Reverb,
  Oktaverb,
  Tremolo,
  Chorus,
  Count
};

constexpr unsigned ModeSnapshotBit(ModeSnapshotTarget target)
{
  return 1u << static_cast<unsigned>(target);
}

struct PendingModeSnapshotBatch
{
  unsigned mask = 0;
  std::array<int, static_cast<std::size_t>(ModeSnapshotTarget::Count)> requestedModes{};

  bool Has(ModeSnapshotTarget target) const { return (mask & ModeSnapshotBit(target)) != 0; }
  int Requested(ModeSnapshotTarget target) const { return requestedModes[static_cast<std::size_t>(target)]; }
};

// Audio-thread producer / main-thread consumer handoff. A repeated UI click +
// host echo can enqueue the same target twice; the tracked-mode comparison in
// ApplyModeSnapshotTransition makes the second transaction a no-op.
class PendingModeSnapshotChanges
{
public:
  void Request(ModeSnapshotTarget target, int requestedMode) noexcept
  {
    mRequestedModes[static_cast<std::size_t>(target)].store(requestedMode, std::memory_order_relaxed);
    mMask.fetch_or(ModeSnapshotBit(target), std::memory_order_release);
  }

  PendingModeSnapshotBatch Take() noexcept
  {
    PendingModeSnapshotBatch batch;
    batch.mask = mMask.exchange(0, std::memory_order_acquire);
    for (std::size_t i = 0; i < batch.requestedModes.size(); ++i)
      if (batch.mask & (1u << static_cast<unsigned>(i)))
        batch.requestedModes[i] = mRequestedModes[i].load(std::memory_order_relaxed);
    return batch;
  }

  void Discard(unsigned mask) noexcept { mMask.fetch_and(~mask, std::memory_order_acq_rel); }

private:
  std::atomic<unsigned> mMask{0};
  std::array<std::atomic<int>, static_cast<std::size_t>(ModeSnapshotTarget::Count)> mRequestedModes{};
};

// Apply one per-mode snapshot transaction. The caller decides whether this
// parameter notification represents a real user/host transition; restores pass
// false so their already-decoded snapshots cannot be overwritten.
template <typename SaveOutgoing, typename RestoreIncoming>
bool ApplyModeSnapshotTransition(int requestedMode, int modeCount, bool transitionAllowed, int& trackedMode,
                                 SaveOutgoing&& saveOutgoing, RestoreIncoming&& restoreIncoming)
{
  if (!transitionAllowed || modeCount <= 0)
    return false;

  const int oldMode = std::clamp(trackedMode, 0, modeCount - 1);
  const int newMode = std::clamp(requestedMode, 0, modeCount - 1);
  if (oldMode == newMode)
    return false;

  saveOutgoing(oldMode);
  trackedMode = newMode;
  restoreIncoming(newMode);
  return true;
}
} // namespace volum
