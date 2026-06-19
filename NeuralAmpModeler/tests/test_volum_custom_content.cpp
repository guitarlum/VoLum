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
