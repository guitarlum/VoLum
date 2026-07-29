#include "third_party/doctest.h"
#include "../VoLumDspStaging.h"

TEST_CASE("DSP staging keeps live path until staged asset is applied")
{
  volum::dsp_staging::LiveStagedPaths paths;
  paths.live = "C:/assets/old.nam";

  volum::dsp_staging::StagePathOnSuccess(paths, "C:/assets/new.nam");
  CHECK(paths.live == "C:/assets/old.nam");
  CHECK(paths.staged == "C:/assets/new.nam");
  CHECK_FALSE(volum::dsp_staging::LivePathMatchesStagedAsset(paths.live, paths.staged, true));

  volum::dsp_staging::CommitStagedPathOnApply(paths);
  CHECK(paths.live == "C:/assets/new.nam");
  CHECK(paths.staged.empty());
  CHECK(volum::dsp_staging::LivePathMatchesStagedAsset(paths.live, paths.staged, false));
}

TEST_CASE("DSP staging failure clears staged path without mutating live path")
{
  volum::dsp_staging::LiveStagedPaths paths;
  paths.live = "C:/assets/keep.nam";

  volum::dsp_staging::StagePathOnSuccess(paths, "C:/assets/bad.nam");
  volum::dsp_staging::ClearStagedOnFailure(paths);

  CHECK(paths.live == "C:/assets/keep.nam");
  CHECK(paths.staged.empty());
}

// ---- deferred IR removal (custom IR -> baked cab has no audible gap) ---------

using volum::dsp_staging::StepDeferredIrRemoval;

TEST_CASE("A deferred IR removal waits while the replacement capture is still loading")
{
  // Reported symptom: switching from a custom IR to a NAM cab produced a short
  // burst of raw amp. The convolver was dropped immediately while the cab capture
  // was still loading, so those blocks ran with neither cab.
  auto step = StepDeferredIrRemoval(/*pending=*/true, /*waitedBlocks=*/0, /*replacementStaged=*/false,
                                    /*maxWaitBlocks=*/750);
  CHECK(step.fire == false);
  CHECK(step.stillPending == true);
  CHECK(step.waitedBlocks == 1);

  step = StepDeferredIrRemoval(true, step.waitedBlocks, false, 750);
  CHECK(step.fire == false);
  CHECK(step.waitedBlocks == 2);
}

TEST_CASE("A deferred IR removal fires on the block its replacement capture is staged")
{
  // Both swap together, so the listener never hears the amp without a cab.
  const auto step = StepDeferredIrRemoval(/*pending=*/true, /*waitedBlocks=*/9, /*replacementStaged=*/true,
                                          /*maxWaitBlocks=*/750);
  CHECK(step.fire == true);
  CHECK(step.stillPending == false);
  CHECK(step.waitedBlocks == 0); // counter reset for the next swap
}

TEST_CASE("A deferred IR removal gives up at the deadline if no capture ever arrives")
{
  // A capture that fails to load must not leave the IR convolving forever; that
  // would be a worse artifact than the gap the deferral avoids.
  auto step = StepDeferredIrRemoval(/*pending=*/true, /*waitedBlocks=*/8, /*replacementStaged=*/false,
                                    /*maxWaitBlocks=*/10);
  CHECK(step.fire == false);
  CHECK(step.waitedBlocks == 9);

  step = StepDeferredIrRemoval(true, step.waitedBlocks, false, 10);
  CHECK(step.fire == true);
  CHECK(step.waitedBlocks == 0);
}

TEST_CASE("Nothing happens when no IR removal is pending")
{
  const auto step = StepDeferredIrRemoval(/*pending=*/false, /*waitedBlocks=*/0, /*replacementStaged=*/true,
                                          /*maxWaitBlocks=*/750);
  CHECK(step.fire == false);
  CHECK(step.stillPending == false);
  CHECK(step.waitedBlocks == 0);
}

TEST_CASE("A pending removal keeps the IR convolving even though its toggle is off")
{
  // The 1.2.1 fix deferred the convolver teardown but not the toggle, and the audio
  // thread gates convolution on the toggle - so the cab-less burst survived the fix
  // untouched. Reported again after 1.2.1: "still the no cab noise when switching
  // from custom cab to stock cab".
  using volum::dsp_staging::IrConvolutionActive;

  // The instant the user picks a baked cab: toggle off, removal pending, capture
  // still loading. The lane must keep convolving.
  CHECK(IrConvolutionActive(/*toggleOn=*/false, /*deferredRemovalPending=*/true));

  // Once the removal fires the convolver is gone; nothing keeps it alive.
  CHECK_FALSE(IrConvolutionActive(false, false));

  // A normally active IR is unaffected either way.
  CHECK(IrConvolutionActive(true, false));
  CHECK(IrConvolutionActive(true, true));
}

TEST_CASE("The deferral holds the IR for exactly the blocks the replacement needs")
{
  // End to end over the two helpers: convolution must stay on for every block of the
  // wait and stop on the block the replacement is staged - no gap, no overlap.
  using volum::dsp_staging::IrConvolutionActive;
  using volum::dsp_staging::StepDeferredIrRemoval;

  bool pending = true; // user just picked a baked cab
  int waited = 0;
  for (int block = 0; block < 5; ++block)
  {
    const auto step = StepDeferredIrRemoval(pending, waited, /*replacementStaged=*/false, /*maxWaitBlocks=*/750);
    pending = step.stillPending;
    waited = step.waitedBlocks;
    CHECK(IrConvolutionActive(/*toggleOn=*/false, pending)); // still cabbed
  }

  const auto swap = StepDeferredIrRemoval(pending, waited, /*replacementStaged=*/true, /*maxWaitBlocks=*/750);
  CHECK(swap.fire); // convolver dropped on the same block the capture goes live
  CHECK_FALSE(IrConvolutionActive(false, swap.stillPending));
}

TEST_CASE("A non-positive deadline disables deferral so the removal is immediate")
{
  // Degenerate configuration guard: never leave a pending removal that can only be
  // resolved by a capture that may not come.
  const auto step = StepDeferredIrRemoval(/*pending=*/true, /*waitedBlocks=*/0, /*replacementStaged=*/false,
                                          /*maxWaitBlocks=*/0);
  CHECK(step.fire == true);
}
