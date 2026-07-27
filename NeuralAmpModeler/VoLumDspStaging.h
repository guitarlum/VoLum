#pragma once

#include <string>

namespace volum::dsp_staging
{

// Models how live file paths are published relative to staged DSP assets.
struct LiveStagedPaths
{
  std::string live;
  std::string staged;
};

inline void StagePathOnSuccess(LiveStagedPaths& paths, const std::string& candidatePath)
{
  paths.staged = candidatePath;
}

inline void CommitStagedPathOnApply(LiveStagedPaths& paths)
{
  if (!paths.staged.empty())
    paths.live = paths.staged;
  paths.staged.clear();
}

inline void ClearStagedOnFailure(LiveStagedPaths& paths)
{
  paths.staged.clear();
}

// While a staged asset is pending apply, the live path must not jump ahead of the object.
inline bool LivePathMatchesStagedAsset(const std::string& livePath, const std::string& stagedPath,
                                       const bool hasStagedAsset)
{
  if (!hasStagedAsset)
    return true;
  return livePath == stagedPath;
}

// Outcome of one block's worth of waiting for a deferred IR removal.
struct DeferredIrRemovalStep
{
  bool fire = false; // drop the convolver on this block
  bool stillPending = false; // keep waiting
  int waitedBlocks = 0; // updated wait counter
};

// Decide when to drop a lane's IR convolver after the user switched from a custom
// IR to a baked cab.
//
// Dropping it the moment the user clicks leaves a short burst of raw, cab-less amp,
// because the replacement cab capture loads asynchronously. Waiting for that
// capture to be staged lets both swap on the same block, so the switch is seamless.
//
// `maxWaitBlocks` bounds the wait: a capture that fails to load (or never arrives)
// must not leave the IR convolving indefinitely, which would be a worse artifact
// than the gap this avoids. Zero or negative disables deferral entirely.
inline DeferredIrRemovalStep StepDeferredIrRemoval(bool pending, int waitedBlocks, bool replacementStaged,
                                                   int maxWaitBlocks)
{
  DeferredIrRemovalStep out;
  if (!pending)
    return out;
  const int waited = waitedBlocks + 1;
  if (replacementStaged || waited >= maxWaitBlocks)
  {
    out.fire = true;
    out.waitedBlocks = 0;
    return out;
  }
  out.stillPending = true;
  out.waitedBlocks = waited;
  return out;
}

} // namespace volum::dsp_staging
