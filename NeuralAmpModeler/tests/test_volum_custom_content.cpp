#include "third_party/doctest.h"

#include <vector>

#include "../VoLumCustomContentMock.h"

// Covers the production (speaker x channel) snap helpers backing the 1.2.0
// BYO-amp builder and main-view focus behavior. Everything else in
// VoLumCustomContentMock.h is throwaway display data.

using volum::custom::SnapChannel;
using volum::custom::SpeakerEnabled;

TEST_CASE("SpeakerEnabled reflects whether a slot has any captures")
{
  REQUIRE(SpeakerEnabled(std::vector<int>{}) == false);
  REQUIRE(SpeakerEnabled(std::vector<int>{1}) == true);
  REQUIRE(SpeakerEnabled(std::vector<int>{2, 3}) == true);
}

TEST_CASE("SnapChannel keeps the current channel when it is still available")
{
  REQUIRE(SnapChannel(std::vector<int>{1, 2, 3}, 2) == 2);
  REQUIRE(SnapChannel(std::vector<int>{1, 2}, 1) == 1);
}

TEST_CASE("SnapChannel falls back to the first available channel")
{
  // Current channel 3 is not available on a {1,2} slot -> snap to first (1).
  REQUIRE(SnapChannel(std::vector<int>{1, 2}, 3) == 1);
  // Current channel 1 not available on a {2,3} slot -> snap to first (2).
  REQUIRE(SnapChannel(std::vector<int>{2, 3}, 1) == 2);
}

TEST_CASE("SnapChannel returns -1 for an empty/disabled speaker slot")
{
  REQUIRE(SnapChannel(std::vector<int>{}, 1) == -1);
  REQUIRE(SnapChannel(std::vector<int>{}, 0) == -1);
}

using volum::custom::AmpSlotChannels;
using volum::custom::AmpSlots;
using volum::custom::CellFileCount;
using volum::custom::CustomAmp;
using volum::custom::FileAssigned;
using volum::custom::FileIsDuplicate;
using volum::custom::HasDuplicate;
using volum::custom::IsDirectSlot;
using volum::custom::kDirectSlot;
using volum::custom::kUnassignedSlot;
using volum::custom::MaxAssignedChannel;
using volum::custom::NormalizeCabName;
using volum::custom::SaveDisabledReason;
using volum::custom::SlotLabel;
using volum::custom::UnassignedCount;

TEST_CASE("IsDirectSlot recognises the amp-only DIRECT slot")
{
  REQUIRE(IsDirectSlot(kDirectSlot) == true);
  REQUIRE(IsDirectSlot(0) == false);
  REQUIRE(IsDirectSlot(kUnassignedSlot) == false);
}

TEST_CASE("FileAssigned requires both a real slot and a real channel")
{
  REQUIRE(FileAssigned({"a.nam", kDirectSlot, 1}) == true);
  REQUIRE(FileAssigned({"b.nam", 0, 2}) == true);
  REQUIRE(FileAssigned({"c.nam", kUnassignedSlot, 0}) == false); // no slot, no channel
  REQUIRE(FileAssigned({"d.nam", 0, 0}) == false); // missing channel
  REQUIRE(FileAssigned({"e.nam", kUnassignedSlot, 2}) == false); // missing slot
}

TEST_CASE("AmpSlots lists DIRECT first then populated cab slots in order")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  const auto slots = AmpSlots(amp);
  REQUIRE(slots == std::vector<int>{kDirectSlot, 0, 1}); // slot 2 (CB3) has no captures
}

TEST_CASE("AmpSlotChannels derives the sparse per-slot channel set")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  REQUIRE(AmpSlotChannels(amp, kDirectSlot) == std::vector<int>{1, 2});
  REQUIRE(AmpSlotChannels(amp, 0) == std::vector<int>{1});
  REQUIRE(AmpSlotChannels(amp, 1) == std::vector<int>{3});
  REQUIRE(AmpSlotChannels(amp, 2).empty());
}

TEST_CASE("SlotLabel maps DIRECT and cab slots to display names")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  REQUIRE(SlotLabel(amp, kDirectSlot) == "DIRECT");
  REQUIRE(SlotLabel(amp, 0) == "G65");
  REQUIRE(SlotLabel(amp, 1) == "V30");
}

TEST_CASE("UnassignedCount counts files still needing a slot/channel")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  REQUIRE(UnassignedCount(amp) == 1);
}

TEST_CASE("Cab rename keeps file links intact (files reference slots, not names)")
{
  CustomAmp amp = volum::custom::MockDemoCustomAmp();
  REQUIRE(AmpSlotChannels(amp, 0) == std::vector<int>{1});
  amp.cabNames[0] = "XYZ"; // rename slot 0
  REQUIRE(SlotLabel(amp, 0) == "XYZ");
  REQUIRE(AmpSlotChannels(amp, 0) == std::vector<int>{1}); // link survives the rename
}

TEST_CASE("NormalizeCabName uppercases, strips spaces, caps at 3 chars")
{
  REQUIRE(NormalizeCabName("g65") == "G65");
  REQUIRE(NormalizeCabName("  v 30 ") == "V30");
  REQUIRE(NormalizeCabName("greenback") == "GRE");
  REQUIRE(NormalizeCabName("   ").empty());
}

TEST_CASE("Duplicate (slot,channel) detection flags both colliding files")
{
  CustomAmp amp;
  amp.files = {{"a.nam", 0, 1}, {"b.nam", 0, 1}, {"c.nam", 0, 2}};
  REQUIRE(CellFileCount(amp, 0, 1) == 2);
  REQUIRE(FileIsDuplicate(amp, 0) == true);
  REQUIRE(FileIsDuplicate(amp, 1) == true);
  REQUIRE(FileIsDuplicate(amp, 2) == false);
  REQUIRE(HasDuplicate(amp) == true);
}

TEST_CASE("MaxAssignedChannel ignores unassigned files")
{
  CustomAmp amp;
  amp.files = {{"a.nam", kDirectSlot, 2}, {"b.nam", 0, 5}, {"c.nam", kUnassignedSlot, 0}};
  REQUIRE(MaxAssignedChannel(amp) == 5);
}

TEST_CASE("SaveDisabledReason gates empty, unassigned, and duplicate states")
{
  CustomAmp empty;
  REQUIRE(SaveDisabledReason(empty) == "Add a .nam file");

  CustomAmp unassigned;
  unassigned.files = {{"a.nam", kUnassignedSlot, 0}};
  REQUIRE(SaveDisabledReason(unassigned) == "Assign every file a cab + channel");

  CustomAmp dup;
  dup.files = {{"a.nam", 0, 1}, {"b.nam", 0, 1}};
  REQUIRE(SaveDisabledReason(dup) == "Two files share a cab + channel");

  CustomAmp ok;
  ok.files = {{"a.nam", kDirectSlot, 1}, {"b.nam", 0, 1}};
  REQUIRE(SaveDisabledReason(ok).empty());
}

TEST_CASE("Manifest-derived channels feed SnapChannel consistently")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  // DIRECT has {1,2}; coming from ch3 snaps down to the first available (1).
  REQUIRE(SnapChannel(AmpSlotChannels(amp, kDirectSlot), 3) == 1);
  // An unpopulated slot yields -1 (UI blocks the switch).
  REQUIRE(SnapChannel(AmpSlotChannels(amp, 2), 1) == -1);
}

// ---- session-mutable preset bank (F5 shell persistence) --------------------

TEST_CASE("AddPreset appends and de-duplicates the display name")
{
  const int amp = 9001; // a fresh, otherwise-unused bank
  REQUIRE(volum::custom::MockPresetsForAmp(amp).empty());

  const int i0 = volum::custom::AddPreset(amp, "Crunch");
  const int i1 = volum::custom::AddPreset(amp, "Crunch"); // duplicate name
  REQUIRE(i0 == 0);
  REQUIRE(i1 == 1);

  const auto list = volum::custom::MockPresetsForAmp(amp);
  REQUIRE(list.size() == 2);
  REQUIRE(list[0] == "Crunch");
  REQUIRE(list[1] == "Crunch 2"); // disambiguated

  // empty name falls back to a default label
  volum::custom::AddPreset(amp, "");
  REQUIRE(volum::custom::MockPresetsForAmp(amp)[2] == "Preset");
}

TEST_CASE("RenamePreset and DeletePreset edit the bank in place")
{
  const int amp = 9002;
  volum::custom::AddPreset(amp, "A");
  volum::custom::AddPreset(amp, "B");
  volum::custom::AddPreset(amp, "C");

  volum::custom::RenamePreset(amp, 1, "B2");
  REQUIRE(volum::custom::MockPresetsForAmp(amp)[1] == "B2");

  // empty rename and out-of-range index are ignored
  volum::custom::RenamePreset(amp, 1, "");
  volum::custom::RenamePreset(amp, 99, "X");
  REQUIRE(volum::custom::MockPresetsForAmp(amp)[1] == "B2");

  volum::custom::DeletePreset(amp, 0);
  const auto list = volum::custom::MockPresetsForAmp(amp);
  REQUIRE(list.size() == 2);
  REQUIRE(list[0] == "B2");
  REQUIRE(list[1] == "C");

  volum::custom::DeletePreset(amp, 99); // out of range no-op
  REQUIRE(volum::custom::MockPresetsForAmp(amp).size() == 2);
}

TEST_CASE("AddIR and AddPedal append to their session libraries")
{
  const size_t ir0 = volum::custom::MockIRLibrary().size();
  const int iri = volum::custom::AddIR("Custom 412");
  REQUIRE(volum::custom::MockIRLibrary().size() == ir0 + 1);
  REQUIRE(volum::custom::MockIRLibrary()[(size_t)iri] == "Custom 412");

  const size_t p0 = volum::custom::MockCustomPedals().size();
  const int pi = volum::custom::AddPedal("");
  REQUIRE(volum::custom::MockCustomPedals().size() == p0 + 1);
  REQUIRE(volum::custom::MockCustomPedals()[(size_t)pi] == "Imported pedal"); // empty -> default
}

TEST_CASE("RenameIR and DeleteIR edit the shared IR library in place")
{
  const int idx = volum::custom::AddIR("ToRename");
  volum::custom::RenameIR(idx, "Renamed IR");
  REQUIRE(volum::custom::MockIRLibrary()[(size_t)idx] == "Renamed IR");

  // empty rename + out-of-range are ignored
  volum::custom::RenameIR(idx, "");
  volum::custom::RenameIR(99999, "X");
  REQUIRE(volum::custom::MockIRLibrary()[(size_t)idx] == "Renamed IR");

  const size_t before = volum::custom::MockIRLibrary().size();
  volum::custom::DeleteIR(idx);
  REQUIRE(volum::custom::MockIRLibrary().size() == before - 1);
  volum::custom::DeleteIR(99999); // out of range no-op
  REQUIRE(volum::custom::MockIRLibrary().size() == before - 1);
}

TEST_CASE("RenamePedal and DeletePedal edit the custom-pedal library in place")
{
  const int idx = volum::custom::AddPedal("PedalToRename");
  volum::custom::RenamePedal(idx, "Renamed pedal");
  REQUIRE(volum::custom::MockCustomPedals()[(size_t)idx] == "Renamed pedal");

  volum::custom::RenamePedal(idx, ""); // ignored
  volum::custom::RenamePedal(99999, "X"); // ignored
  REQUIRE(volum::custom::MockCustomPedals()[(size_t)idx] == "Renamed pedal");

  const size_t before = volum::custom::MockCustomPedals().size();
  volum::custom::DeletePedal(idx);
  REQUIRE(volum::custom::MockCustomPedals().size() == before - 1);
  volum::custom::DeletePedal(99999); // out of range no-op
  REQUIRE(volum::custom::MockCustomPedals().size() == before - 1);
}

TEST_CASE("AddCustomAmp stores the assigned art in lockstep with the name list")
{
  const size_t before = volum::custom::MockCustomAmps().size();
  REQUIRE(volum::custom::MockCustomAmpArts().size() == before); // arrays stay aligned

  const int idx = volum::custom::AddCustomAmp("Art amp", 2);
  REQUIRE(volum::custom::MockCustomAmps().size() == before + 1);
  REQUIRE(volum::custom::MockCustomAmpArts().size() == before + 1);
  REQUIRE(volum::custom::CustomAmpArt(idx) == 2);

  // Out-of-range art ids wrap into [0, kNumCustomArts).
  const int idx2 = volum::custom::AddCustomAmp("Wrap amp", volum::custom::kNumCustomArts + 1);
  REQUIRE(volum::custom::CustomAmpArt(idx2) == 1);

  // CustomAmpArt is safe for out-of-range indices.
  REQUIRE(volum::custom::CustomAmpArt(-1) == 0);
  REQUIRE(volum::custom::CustomAmpArt(99999) == 0);
}

TEST_CASE("kNumCustomArts exposes six selectable styles")
{
  REQUIRE(volum::custom::kNumCustomArts == 6);
  const int idx = volum::custom::AddCustomAmp("Star amp", 5);
  REQUIRE(volum::custom::CustomAmpArt(idx) == 5);
}

TEST_CASE("RemoveCustomAmp keeps names and art ids aligned")
{
  const size_t before = volum::custom::MockCustomAmps().size();
  const int a = volum::custom::AddCustomAmp("Keep", 0);
  const int b = volum::custom::AddCustomAmp("Drop", 3);
  REQUIRE(volum::custom::CustomAmpArt(b) == 3);

  volum::custom::RemoveCustomAmp(b);
  REQUIRE(volum::custom::MockCustomAmps().size() == before + 1);
  REQUIRE(volum::custom::MockCustomAmpArts().size() == before + 1);
  REQUIRE(volum::custom::CustomAmpArt(a) == 0); // surviving entry's art intact

  volum::custom::RemoveCustomAmp(99999); // out of range no-op
  REQUIRE(volum::custom::MockCustomAmpArts().size() == volum::custom::MockCustomAmps().size());
}
