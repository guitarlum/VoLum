#pragma once

#include <algorithm>

namespace volum
{
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
