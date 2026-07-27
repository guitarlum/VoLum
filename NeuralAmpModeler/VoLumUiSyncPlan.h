#pragma once

// VoLumUiSyncPlan.h: pure mapping from restored backend state to the cab row /
// channel stepper / sidebar state the editor should show.
//
// Why this exists: the editor rebuilds every control from scratch on each open,
// starting from constructor defaults (the cab row starts on V30). Through 1.2.0
// the reopen path only pushed `SetSelected(speakerIdx)` for factory lanes, so a
// lane whose cab is a custom IR came back showing "No Cab" - the IR forces
// speakerIdx 0 and nothing restored the IR chip. Custom amps avoided it only
// because their restore happened to run the full cab-resolve path.
//
// Everything here is pure and headless so those decisions are doctested without
// IGraphics, mirroring how VoLumProcessingPlan.h covers signal routing.

#include "VoLumCustomModel.h"

#include <algorithm>
#include <string>
#include <vector>

namespace volum
{

// Gain stage a custom lane is on, derived from the persisted stepper POSITION.
//
// Two different numbers are easy to confuse here: `channelIdx` is the 0-based
// position within the amp-wide channel set and is what gets persisted, while the
// gain stage (1, 5, ...) is a runtime cache. Right after a restore the cache is
// still at its default, so the position is the only trustworthy source.
inline int CustomChannelAtStep(const custom::CustomAmp& amp, int channelPos)
{
  const auto chans = custom::AssignedChannels(amp);
  if (chans.empty())
    return 1;
  const int i = std::clamp(channelPos, 0, static_cast<int>(chans.size()) - 1);
  return chans[static_cast<size_t>(i)];
}

// Gain stage a lane lands on when a custom IR forces it onto its DIRECT capture,
// or -1 when the amp has no DIRECT capture at all (nothing to convolve).
inline int DirectChannelForIr(const custom::CustomAmp& amp, int currentChannel)
{
  const auto direct = custom::AmpSlotChannels(amp, custom::kDirectSlot);
  if (direct.empty())
    return -1;
  return custom::SnapChannel(direct, currentChannel);
}

// Backend state for the focused lane, as it stands after settings/chunk restore.
struct UiSyncInput
{
  // Focused lane is the SUPPORT lane rather than MAIN.
  bool supportFocused = false;

  // Custom lane. Null means the focused lane is a factory amp.
  const custom::CustomAmp* customAmp = nullptr;
  int customAmpIdx = -1;
  int customSlot = custom::kDirectSlot;
  // Persisted stepper position. Authoritative on restore; see CustomChannelAtStep.
  int customChannelPos = 0;

  // Factory lane.
  int factoryAmpIdx = 0;
  int factorySpeakerIdx = 0;
  int factoryChannelIdx = 0;
  std::vector<std::string> factoryChannelLabels;

  // Focused lane's custom IR, already resolved against the library by the caller.
  // `irIdPresent` means the scene stores an id at all; `irResolved` additionally
  // means that id still names an IR that exists on this machine. They differ when
  // the IR was deleted, in which case the dangling id still needs cleaning up.
  bool irIdPresent = false;
  bool irResolved = false;
  std::string irName;
};

// Control state the editor should apply. A field means the same thing whether the
// lane is factory or custom, so the applier stays free of branches.
struct UiSyncPlan
{
  // Cab row.
  int cabSelectedIndex = 0; // 0 = No Cab, 1..3 = cab slot + 1
  bool irCabActive = false; // copper Custom IR button is the active cab
  std::string irName;
  bool noCabEnabled = true;
  bool irEnabled = true;
  bool useFactoryCabNames = true;
  std::string cabNames[custom::kNumCabSlots];

  // Channel stepper.
  std::vector<std::string> channelLabels;
  int channelSelectedPos = 0;

  // Sidebar selection: exactly one of these is >= 0.
  int sidebarFactoryIdx = -1;
  int sidebarCustomIdx = -1;

  // The lane carries an IR id but the resolved channel has no DIRECT capture to
  // convolve, so the caller must drop the orphaned IR and let a real cab take over.
  bool clearOrphanedIr = false;

  // Resolved custom-lane routing, so the applier can write the runtime caches
  // back without repeating the resolve. Meaningless for factory lanes.
  int customChannel = 1;
  int customSlot = custom::kDirectSlot;
};

// Map restored backend state onto the control state the editor should show.
inline UiSyncPlan MakeUiSyncPlan(const UiSyncInput& in)
{
  UiSyncPlan plan;
  plan.irName = in.irName;

  if (in.customAmp == nullptr)
  {
    // Factory lane: the speaker index is persisted directly and the channel list
    // is discovered from the rig folders by the caller.
    plan.cabSelectedIndex = std::clamp(in.factorySpeakerIdx, 0, custom::kNumCabSlots);
    plan.useFactoryCabNames = true;
    plan.noCabEnabled = true; // factory amps always ship a raw DIRECT capture
    plan.irEnabled = true;
    plan.channelLabels = in.factoryChannelLabels;
    plan.channelSelectedPos =
      in.factoryChannelLabels.empty()
        ? 0
        : std::clamp(in.factoryChannelIdx, 0, static_cast<int>(in.factoryChannelLabels.size()) - 1);
    plan.sidebarFactoryIdx = in.factoryAmpIdx;
    plan.irCabActive = in.irResolved;
    return plan;
  }

  // Custom lane: resolve channel-first from the persisted stepper position, never
  // from the runtime gain-stage cache (stale immediately after a restore).
  const custom::CustomAmp& amp = *in.customAmp;
  const int channel = CustomChannelAtStep(amp, in.customChannelPos);
  const custom::LaneCabView v = custom::ResolveLaneCabs(amp, in.customSlot, channel);

  plan.cabSelectedIndex = v.selUiIndex;
  plan.useFactoryCabNames = false;
  for (int s = 0; s < custom::kNumCabSlots; ++s)
    plan.cabNames[s] = v.cabEnabled[static_cast<size_t>(s)] ? amp.cabNames[static_cast<size_t>(s)] : std::string();
  plan.noCabEnabled = v.noCabEnabled;
  plan.irEnabled = v.irEnabled;

  plan.channelLabels.reserve(v.channels.size());
  for (int c : v.channels)
    plan.channelLabels.push_back(std::to_string(c));
  if (plan.channelLabels.empty())
    plan.channelLabels.push_back("1"); // amp with no assigned captures yet
  plan.channelSelectedPos = v.channelPos;

  plan.sidebarCustomIdx = in.customAmpIdx;
  plan.customChannel = v.channel;
  plan.customSlot = v.slot;

  // A custom IR convolves the DIRECT capture. If the resolved channel has none,
  // any stored IR is orphaned for this routing and must be dropped - including an
  // id that no longer resolves, so the dangling reference does not survive.
  plan.clearOrphanedIr = (in.irIdPresent || in.irResolved) && !v.irEnabled;
  plan.irCabActive = in.irResolved && v.irEnabled;
  if (plan.clearOrphanedIr)
    plan.irName.clear();

  return plan;
}

} // namespace volum
