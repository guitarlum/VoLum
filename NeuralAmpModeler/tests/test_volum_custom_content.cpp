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

using volum::custom::AmpSpeakerChannels;
using volum::custom::AmpSpeakers;
using volum::custom::CustomAmp;
using volum::custom::FileAssigned;
using volum::custom::IsDirectSpeaker;
using volum::custom::kDirectSpeaker;
using volum::custom::UnassignedCount;

TEST_CASE("IsDirectSpeaker treats empty and DIRECT as the amp-only speaker")
{
  REQUIRE(IsDirectSpeaker("") == true);
  REQUIRE(IsDirectSpeaker(kDirectSpeaker) == true);
  REQUIRE(IsDirectSpeaker("G65 4x12") == false);
}

TEST_CASE("FileAssigned requires both a speaker and a real channel")
{
  REQUIRE(FileAssigned({"a.nam", kDirectSpeaker, 1}) == true);
  REQUIRE(FileAssigned({"b.nam", "G65 4x12", 2}) == true);
  REQUIRE(FileAssigned({"c.nam", "", 0}) == false); // no speaker, no channel
  REQUIRE(FileAssigned({"d.nam", "G65 4x12", 0}) == false); // missing channel
}

TEST_CASE("AmpSpeakers lists DIRECT first then first-seen cabs, ignoring unassigned")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  const auto speakers = AmpSpeakers(amp);
  REQUIRE(speakers.size() == 3);
  REQUIRE(speakers[0] == kDirectSpeaker); // DIRECT normalized + sorted first
  REQUIRE(speakers[1] == "G65 4x12");
  REQUIRE(speakers[2] == "V30 2x12");
}

TEST_CASE("AmpSpeakerChannels derives the sparse per-speaker channel set")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  REQUIRE(AmpSpeakerChannels(amp, kDirectSpeaker) == std::vector<int>{1, 2});
  REQUIRE(AmpSpeakerChannels(amp, "G65 4x12") == std::vector<int>{1});
  REQUIRE(AmpSpeakerChannels(amp, "V30 2x12") == std::vector<int>{3});
  REQUIRE(AmpSpeakerChannels(amp, "missing").empty());
}

TEST_CASE("UnassignedCount counts files still needing a speaker/channel")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  REQUIRE(UnassignedCount(amp) == 1);
}

TEST_CASE("Manifest-derived channels feed SnapChannel consistently")
{
  const CustomAmp amp = volum::custom::MockDemoCustomAmp();
  // DIRECT has {1,2}; coming from ch3 snaps down to the first available (1).
  REQUIRE(SnapChannel(AmpSpeakerChannels(amp, kDirectSpeaker), 3) == 1);
  // An unpopulated speaker yields -1 (UI blocks the switch).
  REQUIRE(SnapChannel(AmpSpeakerChannels(amp, "missing"), 1) == -1);
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
