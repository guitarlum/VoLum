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

// Outcome of one block's worth of waiting for a deferred IR swap.
struct DeferredIrSwapStep
{
  bool fire = false; // add or drop the convolver on this block
  bool stillPending = false; // keep waiting
  int waitedBlocks = 0; // updated wait counter
};

// Decide when a lane's IR may change, in either direction, after the user switched
// cab sources.
//
// Both directions pair an IR change with a capture change that loads asynchronously,
// and acting on the IR before its capture arrives is audible either way: dropping
// the IR early exposes a burst of raw, cab-less amp, and adding it early stacks a
// custom IR on top of a baked cab that is still live. Waiting for the replacement
// capture to be staged lets both land on one block, so the lane moves from one
// coherent sound straight to the next.
//
// `maxWaitBlocks` bounds the wait: a capture that fails to load (or never arrives)
// must not strand the lane mid-switch, which would be a worse artifact than the
// seam this avoids. Zero or negative disables deferral entirely.
inline DeferredIrSwapStep StepDeferredIrSwap(bool pending, int waitedBlocks, bool replacementStaged, int maxWaitBlocks)
{
  DeferredIrSwapStep out;
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

// Whether a lane should still convolve its IR this block.
//
// Deferring the convolver teardown alone does not close the gap: the UI turns the
// lane's IR toggle off the instant the user picks a baked cab, and the audio thread
// gates convolution on that toggle, so it stops on the very next block while the
// replacement capture is still loading - the exact burst of cab-less amp the
// deferral exists to prevent. So a pending removal keeps the IR running until
// StepDeferredIrSwap fires, which is the block the replacement goes live.
inline bool IrConvolutionActive(bool toggleOn, bool deferredRemovalPending)
{
  return toggleOn || deferredRemovalPending;
}

} // namespace volum::dsp_staging
