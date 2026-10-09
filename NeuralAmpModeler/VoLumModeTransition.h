#pragma once

#include <algorithm>
#include <atomic>
#include <utility>

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

// Audio-thread producer / main-thread consumer handoff. A repeated UI click +
// host echo can set the same bit twice; the main thread reads the current param
// value when it consumes the bit, so no mode value is shared across threads.
class PendingModeSnapshotChanges
{
public:
  void Request(ModeSnapshotTarget target) noexcept { mMask.fetch_or(ModeSnapshotBit(target), std::memory_order_release); }

  unsigned Take() noexcept { return mMask.exchange(0, std::memory_order_acquire); }

  void Discard(unsigned mask) noexcept { mMask.fetch_and(~mask, std::memory_order_acq_rel); }

private:
  std::atomic<unsigned> mMask{0};
};

enum class PendingModeAction
{
  Apply,
  Retry,
  Drop
};

enum class PendingModeResult
{
  NoRequest,
  Applied,
  Unchanged,
  Retry,
  Dropped
};

// A regular knob save must use the remembered mode even when the mode parameter
// already contains a not-yet-applied host request. Only the main-thread
// transition below may change rememberedMode.
template <typename SaveSnapshot>
void SaveTrackedModeSnapshot(int modeCount, int rememberedMode, SaveSnapshot&& saveSnapshot)
{
  if (modeCount <= 0)
    return;
  saveSnapshot(std::clamp(rememberedMode, 0, modeCount - 1));
}

inline PendingModeAction OktaverbPendingModeAction(bool oktaverbSelected, bool restoreInProgress)
{
  if (!oktaverbSelected)
    return PendingModeAction::Drop;
  return restoreInProgress ? PendingModeAction::Retry : PendingModeAction::Apply;
}

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

// Main-thread half of the handoff. currentParamMode is sampled by the consumer,
// not carried by the audio-thread request. Drop is used for an Oktaverb request
// observed while another Reverb mode is selected; Retry is reserved for a
// temporary restore guard.
template <typename SaveOutgoing, typename RestoreIncoming>
PendingModeResult ApplyPendingModeSnapshotChange(bool hasRequest, int currentParamMode, int modeCount,
                                                 PendingModeAction action, int& rememberedMode,
                                                 SaveOutgoing&& saveOutgoing, RestoreIncoming&& restoreIncoming)
{
  if (!hasRequest)
    return PendingModeResult::NoRequest;
  if (action == PendingModeAction::Retry)
    return PendingModeResult::Retry;
  if (action == PendingModeAction::Drop)
    return PendingModeResult::Dropped;

  return ApplyModeSnapshotTransition(
           currentParamMode, modeCount, true, rememberedMode, std::forward<SaveOutgoing>(saveOutgoing),
           std::forward<RestoreIncoming>(restoreIncoming))
           ? PendingModeResult::Applied
           : PendingModeResult::Unchanged;
}

// A parent Reverb-mode restore can rewrite the visible sub-mode parameter before
// this nested transition runs. Reassert the applied sub-mode after restoring its
// knobs so parameter, editor and remembered snapshot cannot disagree.
template <typename SaveOutgoing, typename RestoreIncoming, typename SetCurrentMode>
PendingModeResult ApplyPendingNestedModeSnapshotChange(
  bool hasRequest, int currentParamMode, int modeCount, PendingModeAction action, int& rememberedMode,
  SaveOutgoing&& saveOutgoing, RestoreIncoming&& restoreIncoming, SetCurrentMode&& setCurrentMode)
{
  const auto result = ApplyPendingModeSnapshotChange(
    hasRequest, currentParamMode, modeCount, action, rememberedMode, std::forward<SaveOutgoing>(saveOutgoing),
    std::forward<RestoreIncoming>(restoreIncoming));
  if (result == PendingModeResult::Applied)
    setCurrentMode(std::clamp(rememberedMode, 0, modeCount - 1));
  return result;
}
} // namespace volum
