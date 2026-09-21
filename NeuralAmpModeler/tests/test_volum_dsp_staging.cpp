#include "third_party/doctest.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"

#include <string>

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

// ---- deferred IR swaps (a cab-source switch is one clean move, both ways) ----

using volum::dsp_staging::StepDeferredIrSwap;

TEST_CASE("A deferred IR removal waits while the replacement capture is still loading")
{
  // Reported symptom: switching from a custom IR to a NAM cab produced a short
  // burst of raw amp. The convolver was dropped immediately while the cab capture
  // was still loading, so those blocks ran with neither cab.
  auto step = StepDeferredIrSwap(/*pending=*/true, /*waitedBlocks=*/0, /*replacementStaged=*/false,
                                 /*maxWaitBlocks=*/750);
  CHECK(step.fire == false);
  CHECK(step.stillPending == true);
  CHECK(step.waitedBlocks == 1);

  step = StepDeferredIrSwap(true, step.waitedBlocks, false, 750);
  CHECK(step.fire == false);
  CHECK(step.waitedBlocks == 2);
}

TEST_CASE("A deferred IR removal fires on the block its replacement capture is staged")
{
  // Both swap together, so the listener never hears the amp without a cab.
  const auto step = StepDeferredIrSwap(/*pending=*/true, /*waitedBlocks=*/9, /*replacementStaged=*/true,
                                       /*maxWaitBlocks=*/750);
  CHECK(step.fire == true);
  CHECK(step.stillPending == false);
  CHECK(step.waitedBlocks == 0); // counter reset for the next swap
}

TEST_CASE("A deferred IR removal gives up at the deadline if no capture ever arrives")
{
  // A capture that fails to load must not leave the IR convolving forever; that
  // would be a worse artifact than the gap the deferral avoids.
  auto step = StepDeferredIrSwap(/*pending=*/true, /*waitedBlocks=*/8, /*replacementStaged=*/false,
                                 /*maxWaitBlocks=*/10);
  CHECK(step.fire == false);
  CHECK(step.waitedBlocks == 9);

  step = StepDeferredIrSwap(true, step.waitedBlocks, false, 10);
  CHECK(step.fire == true);
  CHECK(step.waitedBlocks == 0);
}

TEST_CASE("Nothing happens when no IR removal is pending")
{
  const auto step = StepDeferredIrSwap(/*pending=*/false, /*waitedBlocks=*/0, /*replacementStaged=*/true,
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
  using volum::dsp_staging::StepDeferredIrSwap;

  bool pending = true; // user just picked a baked cab
  int waited = 0;
  for (int block = 0; block < 5; ++block)
  {
    const auto step = StepDeferredIrSwap(pending, waited, /*replacementStaged=*/false, /*maxWaitBlocks=*/750);
    pending = step.stillPending;
    waited = step.waitedBlocks;
    CHECK(IrConvolutionActive(/*toggleOn=*/false, pending)); // still cabbed
  }

  const auto swap = StepDeferredIrSwap(pending, waited, /*replacementStaged=*/true, /*maxWaitBlocks=*/750);
  CHECK(swap.fire); // convolver dropped on the same block the capture goes live
  CHECK_FALSE(IrConvolutionActive(false, swap.stillPending));
}

TEST_CASE("The same wait holds a newly picked IR back until its DIRECT capture lands")
{
  // The other direction, reported as "from stock cab to custom IR has a weird volume
  // jump": the IR was staged and applied at once while the baked-cab capture it
  // replaces was still live, so for the length of the load the lane ran cab plus IR.
  // The staged IR now parks until the DIRECT capture is staged and both go live
  // together.
  bool held = false;
  int waited = 0;
  for (int block = 0; block < 4; ++block)
  {
    const auto step = StepDeferredIrSwap(/*pending=*/true, waited, /*replacementStaged=*/false,
                                         /*maxWaitBlocks=*/750);
    held = step.stillPending;
    waited = step.waitedBlocks;
    CHECK(held); // IR parked: the lane is still on the baked cab, alone
    CHECK_FALSE(step.fire);
  }

  const auto swap = StepDeferredIrSwap(/*pending=*/true, waited, /*replacementStaged=*/true, /*maxWaitBlocks=*/750);
  CHECK(swap.fire); // DIRECT capture and IR go live on the same block
  CHECK_FALSE(swap.stillPending);
}

TEST_CASE("A held IR is released at the deadline rather than never convolving")
{
  // If the DIRECT capture never arrives, the user still asked for this IR. Releasing
  // it late is recoverable; parking it forever silently ignores the choice.
  const auto step = StepDeferredIrSwap(/*pending=*/true, /*waitedBlocks=*/9, /*replacementStaged=*/false,
                                       /*maxWaitBlocks=*/10);
  CHECK(step.fire);
  CHECK_FALSE(step.stillPending);
}

TEST_CASE("A non-positive deadline disables deferral so the removal is immediate")
{
  // Degenerate configuration guard: never leave a pending removal that can only be
  // resolved by a capture that may not come.
  const auto step = StepDeferredIrSwap(/*pending=*/true, /*waitedBlocks=*/0, /*replacementStaged=*/false,
                                       /*maxWaitBlocks=*/0);
  CHECK(step.fire == true);
}

namespace
{
struct Counted
{
  static int live;
  Counted() { ++live; }
  ~Counted() { --live; }
};
int Counted::live = 0;
} // namespace

TEST_CASE("Publishing a staged model retires the live one instead of destroying it")
{
  // Revert of T1-1: `live = std::move(staged)` runs ~ResamplingNAM in ProcessBlock.
  Counted::live = 0;
  auto live = std::make_unique<Counted>();
  auto staged = std::make_unique<Counted>();
  std::vector<std::unique_ptr<Counted>> graveyard;
  graveyard.reserve(volum::dsp_staging::kDspGraveyardCapacity);

  volum::dsp_staging::PublishStagedModel(live, staged, graveyard);

  CHECK(staged == nullptr);
  CHECK(live != nullptr);
  CHECK(graveyard.size() == 1);
  CHECK(Counted::live == 2);

  graveyard.clear();
  CHECK(Counted::live == 1);
  live.reset();
  CHECK(Counted::live == 0);
}

TEST_CASE("Staging an incoming model retires a not-yet-applied predecessor")
{
  Counted::live = 0;
  auto staged = std::make_unique<Counted>();
  auto incoming = std::make_unique<Counted>();
  std::vector<std::unique_ptr<Counted>> graveyard;
  graveyard.reserve(volum::dsp_staging::kDspGraveyardCapacity);

  volum::dsp_staging::StageIncomingModel(staged, incoming, graveyard);

  CHECK(incoming == nullptr);
  CHECK(staged != nullptr);
  CHECK(graveyard.size() == 1);
  CHECK(Counted::live == 2);

  graveyard.clear();
  CHECK(Counted::live == 1);
}

TEST_CASE("A full graveyard last-resorts to destroy rather than reallocating")
{
  Counted::live = 0;
  std::vector<std::unique_ptr<Counted>> graveyard;
  graveyard.reserve(1);
  auto first = std::make_unique<Counted>();
  auto second = std::make_unique<Counted>();
  CHECK(Counted::live == 2);

  // Retiring moves ownership; it must not destroy. That is the whole point - the
  // caller is the audio thread.
  volum::dsp_staging::RetireToGraveyard(first, graveyard);
  CHECK(first == nullptr);
  CHECK(graveyard.size() == 1);
  CHECK(Counted::live == 2);

  // At capacity there is no room left, and growing the vector would allocate in
  // the callback - the thing the graveyard exists to avoid. Destroying here is
  // the lesser evil, and only reachable if OnIdle has not run for 16 swaps.
  volum::dsp_staging::RetireToGraveyard(second, graveyard);
  CHECK(second == nullptr);
  CHECK(graveyard.size() == 1);
  CHECK(graveyard.capacity() == 1); // no reallocation
  CHECK(Counted::live == 1);
}

TEST_CASE("A loader result whose rate or block is stale is dropped for reload")
{
  using volum::dsp_staging::DecideLoaderResult;
  using volum::dsp_staging::LoaderResultAction;

  // Revert of T1-2: the drain used to Reset/prewarm this result on the audio thread.
  CHECK(DecideLoaderResult(true, false, false, true, false) == LoaderResultAction::RetireAndReload);
  CHECK(DecideLoaderResult(true, false, false, false, false) == LoaderResultAction::Stage);
  CHECK(DecideLoaderResult(true, true, false, true, false) == LoaderResultAction::Retire);
  CHECK(DecideLoaderResult(true, false, true, false, false) == LoaderResultAction::Retire);
  CHECK(DecideLoaderResult(false, false, false, false, true) == LoaderResultAction::Ignore);
  CHECK(DecideLoaderResult(false, false, false, false, false) == LoaderResultAction::Ignore);
}

TEST_CASE("A published NAM path copies into a fixed buffer without needing WDL")
{
  char pending[volum::dsp_staging::kRtPathCapacity]{};
  volum::dsp_staging::CopyPathNoAlloc(pending, sizeof(pending), "C:/rigs/Ampete One/AMP-Ampt-1.nam");
  CHECK(std::string(pending) == "C:/rigs/Ampete One/AMP-Ampt-1.nam");

  volum::dsp_staging::RtPublishedPath slot;
  CHECK_FALSE(slot.dirty.load());
  volum::dsp_staging::PublishPathNoAlloc(slot, pending);
  CHECK(slot.dirty.load());
  CHECK(std::string(slot.text) == std::string(pending));

  slot.dirty.store(false);
  volum::dsp_staging::PublishPathNoAlloc(slot, nullptr);
  CHECK(slot.dirty.load());
  CHECK(slot.text[0] == '\0');
}
