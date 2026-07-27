#include "third_party/doctest.h"

#include <string>
#include <vector>

#include "../VoLumUiSyncPlan.h"

// Covers VoLumUiSyncPlan.h: the pure mapping from restored backend state onto the
// cab row / channel stepper / sidebar state the editor should show on reopen.
//
// Two 1.2.1 field reports drive most of these cases:
//   * "custom IR shows No Cab after closing and reopening the window"
//   * "custom amp on channel 5 comes back on channel 1 after an app restart"
// Both were UI resync failures with correct data on disk, so they are expressible
// here without IGraphics.

using volum::CustomChannelAtStep;
using volum::DirectChannelForIr;
using volum::MakeUiSyncPlan;
using volum::UiSyncInput;
using volum::custom::CustomAmp;
using volum::custom::CustomNamFile;
using volum::custom::kDirectSlot;

namespace
{
// The reporter's amp: DIRECT captures on gain stages 1 and 5 only, so the
// stepper has two entries and channel 5 sits at position 1.
CustomAmp MakeChannelOneAndFiveAmp()
{
  CustomAmp amp;
  amp.id = "amp_mjvm";
  amp.name = "MJVM";
  amp.files = {{"AMP-MJVM-1.nam", kDirectSlot, 1}, {"AMP-MJVM-5.nam", kDirectSlot, 5}};
  return amp;
}

// An amp where DIRECT exists on channel 1 only, so channel 2 cannot host an IR.
CustomAmp MakeMixedDirectAmp()
{
  CustomAmp amp;
  amp.id = "amp_mixed";
  amp.name = "Mixed";
  amp.cabNames = {"G12", "V30", "CB3"};
  amp.files = {{"AMP-1.nam", kDirectSlot, 1}, {"G12-1.nam", 0, 1}, {"G12-2.nam", 0, 2}, {"V30-2.nam", 1, 2}};
  return amp;
}
} // namespace

// ---- CustomChannelAtStep: the channel-5 regression -------------------------

TEST_CASE("CustomChannelAtStep maps a persisted stepper position to its gain stage")
{
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  // Position 0 is gain stage 1, position 1 is gain stage 5. Reading position as
  // if it were the gain stage is exactly the 1.2.1 "channel 5 becomes 1" bug.
  CHECK(CustomChannelAtStep(amp, 0) == 1);
  CHECK(CustomChannelAtStep(amp, 1) == 5);
}

TEST_CASE("CustomChannelAtStep clamps an out-of-range position instead of indexing past the end")
{
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  CHECK(CustomChannelAtStep(amp, -3) == 1);
  CHECK(CustomChannelAtStep(amp, 99) == 5);
}

TEST_CASE("CustomChannelAtStep falls back to gain stage 1 for an amp with no captures")
{
  CHECK(CustomChannelAtStep(CustomAmp{}, 0) == 1);
  CHECK(CustomChannelAtStep(CustomAmp{}, 4) == 1);
}

// ---- DirectChannelForIr ----------------------------------------------------

TEST_CASE("DirectChannelForIr keeps the current gain stage when DIRECT covers it")
{
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  // Selecting an IR while already on channel 5 must not snap back to 1.
  CHECK(DirectChannelForIr(amp, 5) == 5);
  CHECK(DirectChannelForIr(amp, 1) == 1);
}

TEST_CASE("DirectChannelForIr snaps to the first DIRECT stage when the current one has none")
{
  const CustomAmp amp = MakeMixedDirectAmp(); // DIRECT on channel 1 only
  CHECK(DirectChannelForIr(amp, 2) == 1);
}

TEST_CASE("DirectChannelForIr reports -1 when the amp has no DIRECT capture at all")
{
  CustomAmp amp;
  amp.files = {{"G12-1.nam", 0, 1}};
  CHECK(DirectChannelForIr(amp, 1) == -1);
}

// ---- Factory lane: the "No Cab after reopen" regression --------------------

TEST_CASE("Factory lane with an active custom IR keeps the IR chip on editor reopen")
{
  // The reported bug: an IR forces the lane onto DIRECT (speakerIdx 0), the
  // reopen path pushed only SetSelected(0), and the teal "No Cab" button lit up
  // because nothing restored the copper IR chip.
  UiSyncInput in;
  in.factoryAmpIdx = 4;
  in.factorySpeakerIdx = 0; // forced to DIRECT by the IR
  in.factoryChannelLabels = {"1", "2"};
  in.factoryChannelIdx = 1;
  in.irResolved = true;
  in.irName = "Marshall 4x12 / Royer";

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.irCabActive == true);
  CHECK(plan.irName == "Marshall 4x12 / Royer");
  CHECK(plan.cabSelectedIndex == 0);
  CHECK(plan.sidebarFactoryIdx == 4);
  CHECK(plan.sidebarCustomIdx == -1);
  CHECK(plan.channelSelectedPos == 1);
  CHECK(plan.useFactoryCabNames == true);
  CHECK(plan.clearOrphanedIr == false);
}

TEST_CASE("Factory lane without an IR restores its persisted cab selection")
{
  UiSyncInput in;
  in.factoryAmpIdx = 2;
  in.factorySpeakerIdx = 3; // V30
  in.factoryChannelLabels = {"1", "2", "3"};
  in.factoryChannelIdx = 2;

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.cabSelectedIndex == 3);
  CHECK(plan.irCabActive == false);
  CHECK(plan.irName.empty());
  CHECK(plan.channelSelectedPos == 2);
}

TEST_CASE("Factory lane treats an orphaned IR id as no IR")
{
  // The id was persisted but the IR is missing on this machine; the caller
  // resolves that to irResolved=false and the chip must stay off.
  UiSyncInput in;
  in.factorySpeakerIdx = 1;
  in.irResolved = false;
  in.irName = "deleted.wav";

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.irCabActive == false);
  CHECK(plan.cabSelectedIndex == 1);
}

TEST_CASE("Factory lane clears an IR id that no longer names anything")
{
  // Found by the end-to-end harness: hiding the chip was not enough, the dead id
  // stayed in the scene and was written back on every save.
  UiSyncInput in;
  in.factorySpeakerIdx = 2;
  in.irIdPresent = true;
  in.irResolved = false;
  in.irName = "deleted.wav";

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.clearOrphanedIr == true);
  CHECK(plan.irCabActive == false);
  CHECK(plan.irName.empty());
  // Clearing the reference must not disturb the cab the lane is actually on.
  CHECK(plan.cabSelectedIndex == 2);
}

TEST_CASE("Factory lane keeps a resolvable IR id")
{
  UiSyncInput in;
  in.factorySpeakerIdx = 0;
  in.irIdPresent = true;
  in.irResolved = true;
  in.irName = "greenback.wav";

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.clearOrphanedIr == false);
  CHECK(plan.irCabActive == true);
  CHECK(plan.irName == "greenback.wav");
}

TEST_CASE("Factory lane clamps a corrupt speaker index and channel position")
{
  UiSyncInput in;
  in.factorySpeakerIdx = 99;
  in.factoryChannelLabels = {"1", "2"};
  in.factoryChannelIdx = 7;
  CHECK(MakeUiSyncPlan(in).cabSelectedIndex == 3);
  CHECK(MakeUiSyncPlan(in).channelSelectedPos == 1);

  in.factorySpeakerIdx = -4;
  in.factoryChannelIdx = -2;
  CHECK(MakeUiSyncPlan(in).cabSelectedIndex == 0);
  CHECK(MakeUiSyncPlan(in).channelSelectedPos == 0);
}

TEST_CASE("Factory lane with no discovered channels reports position zero")
{
  UiSyncInput in;
  in.factoryChannelLabels.clear();
  in.factoryChannelIdx = 3;
  CHECK(MakeUiSyncPlan(in).channelSelectedPos == 0);
}

// ---- Custom lane: the "channel 5 resets to 1" regression -------------------

TEST_CASE("Custom lane on channel five with a custom IR restores channel five, not channel one")
{
  // Standalone relaunch: the scene persisted channelIdx=1 (position of gain
  // stage 5) and an activeIrId. Deriving the stage from the runtime cache
  // instead of the persisted position is what produced channel 1.
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 0;
  in.customSlot = kDirectSlot;
  in.customChannelPos = 1; // persisted position of gain stage 5
  in.irResolved = true;
  in.irName = "Marshall 4x12 / Royer";

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.customChannel == 5);
  CHECK(plan.channelSelectedPos == 1);
  CHECK(plan.channelLabels == std::vector<std::string>{"1", "5"});
  CHECK(plan.irCabActive == true);
  CHECK(plan.clearOrphanedIr == false);
  CHECK(plan.customSlot == kDirectSlot);
  CHECK(plan.cabSelectedIndex == 0); // DIRECT row, with the IR chip lit
  CHECK(plan.sidebarCustomIdx == 0);
  CHECK(plan.sidebarFactoryIdx == -1);
}

TEST_CASE("Custom lane on channel one is unchanged by the channel-five fix")
{
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 0;
  in.customSlot = kDirectSlot;
  in.customChannelPos = 0;
  in.irResolved = true;

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.customChannel == 1);
  CHECK(plan.channelSelectedPos == 0);
  CHECK(plan.irCabActive == true);
}

TEST_CASE("Custom lane drops an IR whose resolved channel has no DIRECT capture")
{
  // Channel 2 of this amp has cabs but no DIRECT, so a stored IR has no raw
  // signal to convolve and must be cleared rather than shown as active.
  const CustomAmp amp = MakeMixedDirectAmp();
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 3;
  in.customSlot = 0; // cab slot 0 carries channel 2
  in.customChannelPos = 1; // gain stage 2
  in.irIdPresent = true;
  in.irResolved = true;
  in.irName = "orphan.wav";

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.customChannel == 2);
  CHECK(plan.irEnabled == false);
  CHECK(plan.noCabEnabled == false);
  CHECK(plan.clearOrphanedIr == true);
  CHECK(plan.irCabActive == false);
  CHECK(plan.irName.empty());
}

TEST_CASE("Custom lane clears a dangling IR id even when it no longer resolves")
{
  // The IR was deleted from the library, so it cannot light the chip, but the
  // stale id must still be scrubbed off a channel that has no DIRECT capture.
  const CustomAmp amp = MakeMixedDirectAmp();
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 0;
  in.customSlot = 0;
  in.customChannelPos = 1; // gain stage 2: no DIRECT
  in.irIdPresent = true;
  in.irResolved = false;

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.clearOrphanedIr == true);
  CHECK(plan.irCabActive == false);
}

TEST_CASE("Custom lane keeps a stored IR when the resolved channel does have DIRECT")
{
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 0;
  in.customChannelPos = 1; // gain stage 5, which has DIRECT
  in.irIdPresent = true;
  in.irResolved = true;
  in.irName = "keep.wav";

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.clearOrphanedIr == false);
  CHECK(plan.irCabActive == true);
  CHECK(plan.irName == "keep.wav");
}

TEST_CASE("Custom lane blanks cab names for slots that do not carry the resolved channel")
{
  const CustomAmp amp = MakeMixedDirectAmp();
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 0;
  in.customSlot = 0;
  in.customChannelPos = 0; // gain stage 1: DIRECT + cab slot 0 only

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.useFactoryCabNames == false);
  CHECK(plan.cabNames[0] == "G12"); // carries channel 1
  CHECK(plan.cabNames[1].empty()); // V30 is channel 2 only
  CHECK(plan.cabNames[2].empty()); // never assigned
  CHECK(plan.noCabEnabled == true); // channel 1 has DIRECT
  CHECK(plan.irEnabled == true);
}

TEST_CASE("Custom lane with no assigned captures degrades to a single placeholder channel")
{
  const CustomAmp amp; // nothing assigned
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 1;
  in.customChannelPos = 2;

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.channelLabels == std::vector<std::string>{"1"});
  CHECK(plan.channelSelectedPos == 0);
  CHECK(plan.noCabEnabled == false);
  CHECK(plan.irEnabled == false);
  CHECK(plan.sidebarCustomIdx == 1);
}

TEST_CASE("Custom lane keeps a real cab selected when no IR is active")
{
  const CustomAmp amp = MakeMixedDirectAmp();
  UiSyncInput in;
  in.customAmp = &amp;
  in.customAmpIdx = 0;
  in.customSlot = 1; // V30
  in.customChannelPos = 1; // gain stage 2, which V30 carries

  const auto plan = MakeUiSyncPlan(in);
  CHECK(plan.customSlot == 1);
  CHECK(plan.cabSelectedIndex == 2); // slot + 1
  CHECK(plan.irCabActive == false);
  CHECK(plan.clearOrphanedIr == false);
}

TEST_CASE("Sidebar selection is exclusive between factory and custom lanes")
{
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  UiSyncInput custom;
  custom.customAmp = &amp;
  custom.customAmpIdx = 7;
  custom.factoryAmpIdx = 3; // must be ignored while a custom amp is focused
  const auto customPlan = MakeUiSyncPlan(custom);
  CHECK(customPlan.sidebarCustomIdx == 7);
  CHECK(customPlan.sidebarFactoryIdx == -1);

  UiSyncInput factory;
  factory.factoryAmpIdx = 3;
  const auto factoryPlan = MakeUiSyncPlan(factory);
  CHECK(factoryPlan.sidebarFactoryIdx == 3);
  CHECK(factoryPlan.sidebarCustomIdx == -1);
}

TEST_CASE("Support focus does not change how the plan is resolved")
{
  // The shared cab row renders whichever lane is focused; the mapping itself is
  // lane-agnostic, so the same inputs must produce the same plan.
  const CustomAmp amp = MakeChannelOneAndFiveAmp();
  UiSyncInput main;
  main.customAmp = &amp;
  main.customAmpIdx = 0;
  main.customChannelPos = 1;
  main.irResolved = true;

  UiSyncInput support = main;
  support.supportFocused = true;

  const auto mainPlan = MakeUiSyncPlan(main);
  const auto supportPlan = MakeUiSyncPlan(support);
  CHECK(mainPlan.customChannel == supportPlan.customChannel);
  CHECK(mainPlan.cabSelectedIndex == supportPlan.cabSelectedIndex);
  CHECK(mainPlan.irCabActive == supportPlan.irCabActive);
}
