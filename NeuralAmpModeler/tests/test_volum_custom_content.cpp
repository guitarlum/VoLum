#include "third_party/doctest.h"

#include <vector>

#include "../VoLumCustomContentApi.h"

// Covers the production (speaker x channel) snap helpers backing the 1.2.0
// BYO-amp builder and main-view focus behavior, plus the registry-backed
// projection/mutation API in VoLumCustomContentApi.h.

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

TEST_CASE("ChannelStepIndex maps a channel number to its stepper position")
{
  using volum::custom::ChannelStepIndex;
  // A Fryette-style amp that only ships channels {3,4}: channel 3 -> row 0, 4 -> row 1.
  REQUIRE(ChannelStepIndex(std::vector<int>{3, 4}, 3) == 0);
  REQUIRE(ChannelStepIndex(std::vector<int>{3, 4}, 4) == 1);
  REQUIRE(ChannelStepIndex(std::vector<int>{1, 2, 3}, 3) == 2);
}

TEST_CASE("ChannelStepIndex defaults to row 0 when the channel is absent/empty")
{
  using volum::custom::ChannelStepIndex;
  REQUIRE(ChannelStepIndex(std::vector<int>{3, 4}, 1) == 0); // not present -> 0
  REQUIRE(ChannelStepIndex(std::vector<int>{}, 2) == 0); // empty slot -> 0
}

TEST_CASE("SnapChannel + ChannelStepIndex preserve a higher gain stage across a cab switch")
{
  using volum::custom::ChannelStepIndex;
  // On channel 4, switch to a cab slot that also covers {3,4}: keep 4 (row 1),
  // do NOT snap back to channel 3 / row 0 (item: cab/IR switch must keep channel).
  const std::vector<int> newSlot{3, 4};
  const int snapped = SnapChannel(newSlot, 4);
  REQUIRE(snapped == 4);
  REQUIRE(ChannelStepIndex(newSlot, snapped) == 1);
}

TEST_CASE("ShortCaptureLabel truncates long custom names to 5 chars + ellipsis")
{
  using volum::custom::ShortCaptureLabel;
  REQUIRE(ShortCaptureLabel("OD") == "OD"); // short names pass through
  REQUIRE(ShortCaptureLabel("BOOST") == "BOOST"); // exactly 5 -> unchanged
  REQUIRE(ShortCaptureLabel("Klon Centaur") == std::string("Klon ") + "\u2026"); // > 5 -> clipped
}

TEST_CASE("HasDirectCapture is true only when a DIRECT (cab-less) capture exists")
{
  using volum::custom::CustomAmp;
  using volum::custom::HasDirectCapture;
  using volum::custom::kDirectSlot;
  // Cab-only amp: no DIRECT capture, so a custom IR has nothing to convolve.
  CustomAmp cabOnly;
  cabOnly.files = {{"a.nam", 0, 1}, {"b.nam", 1, 2}};
  REQUIRE(HasDirectCapture(cabOnly) == false);
  // Amp with a DIRECT capture is IR-eligible.
  CustomAmp withDirect;
  withDirect.files = {{"d.nam", kDirectSlot, 1}, {"b.nam", 0, 1}};
  REQUIRE(HasDirectCapture(withDirect) == true);
  // An unassigned would-be DIRECT file does not count.
  CustomAmp unassigned;
  unassigned.files = {{"d.nam", volum::custom::kUnassignedSlot, 0}};
  REQUIRE(HasDirectCapture(unassigned) == false);
}

TEST_CASE("SlotsForChannel inverts the per-slot channel map (channel-first nav)")
{
  using volum::custom::CustomAmp;
  using volum::custom::kDirectSlot;
  using volum::custom::SlotsForChannel;
  // DIRECT carries ch1 only; cab slot 0 carries ch1+ch2; cab slot 1 carries ch2.
  CustomAmp amp;
  amp.files = {{"d.nam", kDirectSlot, 1}, {"a.nam", 0, 1}, {"b.nam", 0, 2}, {"c.nam", 1, 2}};
  // Channel 1: DIRECT first, then cab slot 0.
  REQUIRE(SlotsForChannel(amp, 1) == std::vector<int>{kDirectSlot, 0});
  // Channel 2: cab slots 0 and 1 (no DIRECT on ch2).
  REQUIRE(SlotsForChannel(amp, 2) == std::vector<int>{0, 1});
  // Channel with no captures.
  REQUIRE(SlotsForChannel(amp, 3).empty());
}

TEST_CASE("ChannelHasDirect is per-channel, not amp-wide")
{
  using volum::custom::ChannelHasDirect;
  using volum::custom::CustomAmp;
  using volum::custom::HasDirectCapture;
  using volum::custom::kDirectSlot;
  // DIRECT exists only on channel 1; channel 2 is a cab-only stage.
  CustomAmp amp;
  amp.files = {{"d.nam", kDirectSlot, 1}, {"a.nam", 0, 1}, {"b.nam", 0, 2}};
  REQUIRE(HasDirectCapture(amp) == true); // amp-wide: yes
  REQUIRE(ChannelHasDirect(amp, 1) == true); // ch1 has DIRECT -> No Cab / Custom IR ok
  REQUIRE(ChannelHasDirect(amp, 2) == false); // ch2 has no DIRECT -> gate them
  REQUIRE(ChannelHasDirect(amp, 9) == false);
}

TEST_CASE("SnapSlotForChannel keeps current, prefers a real cab, No Cab last")
{
  using volum::custom::CustomAmp;
  using volum::custom::kDirectSlot;
  using volum::custom::kUnassignedSlot;
  using volum::custom::SnapSlotForChannel;
  CustomAmp amp;
  // ch1: DIRECT + cab0 ; ch2: cab0 + cab1 ; ch3: DIRECT only.
  amp.files = {
    {"d1.nam", kDirectSlot, 1}, {"a.nam", 0, 1}, {"b.nam", 0, 2}, {"c.nam", 1, 2}, {"d3.nam", kDirectSlot, 3}};

  // Current slot still valid for the new channel -> keep it.
  REQUIRE(SnapSlotForChannel(amp, 1, 0) == 0);
  REQUIRE(SnapSlotForChannel(amp, 1, kDirectSlot) == kDirectSlot);

  // Current cab (slot 0) is gone on ch... it stays on ch2 (cab0 valid) -> keep.
  REQUIRE(SnapSlotForChannel(amp, 2, 0) == 0);
  // Coming from DIRECT into ch2 (no DIRECT) -> snap to the first real cab.
  REQUIRE(SnapSlotForChannel(amp, 2, kDirectSlot) == 0);

  // ch3 only has DIRECT -> No Cab is the last resort even from a cab.
  REQUIRE(SnapSlotForChannel(amp, 3, 0) == kDirectSlot);

  // Channel with nothing assigned -> no change sentinel.
  REQUIRE(SnapSlotForChannel(amp, 7, 0) == kUnassignedSlot);
}

TEST_CASE("ResolveLaneCabs drives the channel-first cab view")
{
  using volum::custom::CustomAmp;
  using volum::custom::kDirectSlot;
  using volum::custom::ResolveLaneCabs;
  // ch1: DIRECT + cab0 ; ch2: cab0 + cab1 (no DIRECT) ; ch3: DIRECT only.
  CustomAmp amp;
  amp.files = {
    {"d1.nam", kDirectSlot, 1}, {"a.nam", 0, 1}, {"b.nam", 0, 2}, {"c.nam", 1, 2}, {"d3.nam", kDirectSlot, 3}};

  // Stepper lists the amp-wide channel set regardless of the current slot.
  {
    const auto v = ResolveLaneCabs(amp, 0 /*cab0*/, 1 /*ch1*/);
    REQUIRE(v.channels == std::vector<int>{1, 2, 3});
    CHECK(v.channel == 1);
    CHECK(v.channelPos == 0);
    CHECK(v.slot == 0); // current cab0 carries ch1 -> kept
    CHECK(v.selUiIndex == 1);
    CHECK(v.noCabEnabled == true); // ch1 has DIRECT
    CHECK(v.irEnabled == true);
    CHECK(v.cabEnabled[0] == true); // cab0 on ch1
    CHECK(v.cabEnabled[1] == false); // cab1 not on ch1
    CHECK(v.cabEnabled[2] == false);
  }

  // Channel 2 has no DIRECT: No Cab + Custom IR gated off; only cab0/cab1 enabled.
  {
    const auto v = ResolveLaneCabs(amp, 0 /*cab0*/, 2 /*ch2*/);
    CHECK(v.channel == 2);
    CHECK(v.noCabEnabled == false);
    CHECK(v.irEnabled == false);
    CHECK(v.cabEnabled[0] == true);
    CHECK(v.cabEnabled[1] == true);
    CHECK(v.slot == 0); // cab0 carries ch2 -> kept
  }

  // Coming from DIRECT into ch2 (no DIRECT) snaps to the first real cab.
  {
    const auto v = ResolveLaneCabs(amp, kDirectSlot, 2);
    CHECK(v.slot == 0);
    CHECK(v.selUiIndex == 1);
    CHECK(v.noCabEnabled == false);
  }

  // Channel 3 has only DIRECT: a cab slot must snap to No Cab (last resort).
  {
    const auto v = ResolveLaneCabs(amp, 0 /*cab0*/, 3);
    CHECK(v.channel == 3);
    CHECK(v.slot == kDirectSlot);
    CHECK(v.selUiIndex == 0);
    CHECK(v.noCabEnabled == true);
    CHECK(v.cabEnabled[0] == false);
  }

  // A channel that no longer exists snaps to the first available channel.
  {
    const auto v = ResolveLaneCabs(amp, 0, 9 /*gone*/);
    CHECK(v.channel == 1);
    CHECK(v.channelPos == 0);
  }
}

TEST_CASE("ResolveLaneCabs on an amp with no assigned files is inert")
{
  using volum::custom::CustomAmp;
  using volum::custom::ResolveLaneCabs;
  CustomAmp amp; // no files
  const auto v = ResolveLaneCabs(amp, volum::custom::kDirectSlot, 1);
  CHECK(v.channels.empty());
  CHECK(v.channel == 1);
  CHECK(v.noCabEnabled == false);
  CHECK(v.irEnabled == false);
  CHECK(v.cabEnabled[0] == false);
}

TEST_CASE("Custom preset recall keeps channel five DIRECT routing and custom IR")
{
  using volum::custom::AssignedChannels;
  using volum::custom::CustomAmp;
  using volum::custom::CustomNamFile;
  using volum::custom::kDirectSlot;
  using volum::custom::ResolveLaneCabs;

  CustomAmp amp;
  amp.id = "amp_issue_22";
  amp.name = "Five-channel direct amp";
  for (int channel = 1; channel <= 5; ++channel)
  {
    CustomNamFile file;
    file.file = "Direct-" + std::to_string(channel) + ".nam";
    file.slot = kDirectSlot;
    file.channel = channel;
    file.storedPath = "amps/direct-" + std::to_string(channel) + ".nam";
    amp.files.push_back(file);
  }

  volum::VoLumAmpSettings saved;
  saved.speakerIdx = 0; // DIRECT / custom-IR row
  saved.channelIdx = 4; // position of gain-stage channel 5
  saved.activeIrId = "ir_issue_22";

  volum::VoLumAmpSettings recalled;
  REQUIRE_FALSE(volum::AmpSettingsFromJson(volum::AmpSettingsToJson(saved), recalled));
  const auto channels = AssignedChannels(amp);
  REQUIRE(channels.size() == 5);
  const int recalledChannel = channels[(size_t)recalled.channelIdx];
  const auto view = ResolveLaneCabs(amp, kDirectSlot, recalledChannel);

  CHECK(view.channel == 5);
  CHECK(view.slot == kDirectSlot);
  CHECK(view.noCabEnabled);
  CHECK(view.irEnabled);
  CHECK(recalled.activeIrId == "ir_issue_22");
  CHECK(volum::content::CaptureFileFor(amp, view.slot, view.channel) == "amps/direct-5.nam");
}

TEST_CASE("ClampName caps a long name and never splits a UTF-8 glyph")
{
  using volum::custom::ClampName;
  REQUIRE(ClampName("short", 24) == "short"); // already fits
  REQUIRE(ClampName("0123456789abcdef", 8) == "01234567"); // hard byte cap
  // A 3-byte euro sign straddling the cap is dropped whole, not split.
  const std::string withEuro = std::string("abc") + "\xE2\x82\xAC" + "xyz"; // "abc<euro>xyz"
  REQUIRE(ClampName(withEuro, 4) == "abc"); // cap at 4 lands mid-euro -> back off to "abc"
  REQUIRE(ClampName(withEuro, 6) == std::string("abc") + "\xE2\x82\xAC"); // cap includes whole euro
}

using volum::custom::AmpSlotChannels;
using volum::custom::AmpSlots;
using volum::custom::AssignedChannels;
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

TEST_CASE("A cab name capped mid-glyph never yields invalid UTF-8")
{
  // Regression: the cap counted bytes, so a four-byte glyph was cut after three
  // and the fragment went into the builder draft and then into
  // volum-content.json, where nlohmann::json::dump() throws on invalid UTF-8 -
  // taking the plug-in down while saving the amp rather than rejecting the name.
  const std::string grinning = "\xF0\x9F\x98\x80"; // U+1F600, four bytes
  const std::string euro = "\xE2\x82\xAC"; // U+20AC, three bytes

  // The whole glyph survives, or none of it does; never three of its four bytes.
  const std::string oneEmoji = NormalizeCabName(grinning);
  CHECK(oneEmoji == grinning);
  CHECK(volum::custom::Utf8Prefix(oneEmoji, oneEmoji.size()) == oneEmoji);

  // Three characters means three characters, not three bytes.
  CHECK(NormalizeCabName(grinning + grinning + grinning + grinning) == grinning + grinning + grinning);
  CHECK(NormalizeCabName(euro + euro + euro + euro) == euro + euro + euro);

  // Mixed input still counts characters, and ASCII is still uppercased.
  CHECK(NormalizeCabName("a" + euro + "b" + "c") == "A" + euro + "B");

  // Input that is already malformed stops the scan instead of copying a fragment.
  CHECK(NormalizeCabName(std::string("AB") + "\xF0\x9F\x98") == "AB");
  CHECK(NormalizeCabName("\x80\x80\x80").empty());
}

TEST_CASE("Utf8Prefix cuts only on character boundaries")
{
  using volum::custom::Utf8Prefix;
  const std::string euro = "\xE2\x82\xAC";

  CHECK(Utf8Prefix("abcdef", 3) == "abc");
  CHECK(Utf8Prefix("abcdef", 99) == "abcdef");
  CHECK(Utf8Prefix("abcdef", 0).empty());

  // A budget that lands inside the euro sign excludes it entirely.
  CHECK(Utf8Prefix("ab" + euro, 3) == "ab");
  CHECK(Utf8Prefix("ab" + euro, 4) == "ab");
  CHECK(Utf8Prefix("ab" + euro, 5) == "ab" + euro);

  // A truncated sequence at the end of the input is never emitted.
  CHECK(Utf8Prefix(std::string("ab") + "\xE2\x82", 99) == "ab");
}

TEST_CASE("The UTF-8 scanner rejects every ill-formed sequence, not only split ones")
{
  using volum::custom::NormalizeCabName;
  using volum::custom::Utf8Prefix;

  // The point of the scanner is that whatever it emits can be written to
  // volum-content.json, and nlohmann's dump() throws on every ill-formed sequence -
  // not only on one cut in half. Reading the lead byte's length and stopping there
  // let three whole families through, so a registry could still be written that
  // could never be written again.

  // A lead byte followed by something that is not a continuation byte. Plausible
  // from a name that was byte-truncated by an older build and then edited.
  CHECK(Utf8Prefix(std::string("A") + "\xE2" + "bc", 99) == "A");
  CHECK(NormalizeCabName(std::string("A") + "\xE2" + "bc") == "A");
  CHECK(Utf8Prefix(std::string("A") + "\xF0\x9F" + "z", 99) == "A");

  // Overlong encodings: the right shape for a two- or three-byte sequence, encoding
  // a value that must be spelled shorter. "\xC0\x80" is the classic NUL smuggle.
  CHECK(Utf8Prefix(std::string("A") + "\xC0\x80", 99) == "A");
  CHECK(Utf8Prefix(std::string("A") + "\xE0\x80\xAF", 99) == "A");

  // Above U+10FFFF, and the surrogate range, which is not encodable in UTF-8.
  CHECK(Utf8Prefix(std::string("A") + "\xF4\x90\x80\x80", 99) == "A");
  CHECK(Utf8Prefix(std::string("A") + "\xED\xA0\x80", 99) == "A");

  // The shapes that are legal stay legal: 1-, 2-, 3- and 4-byte sequences, and the
  // boundary values at each end of each range.
  const std::string cases[] = {
    "A", "\xC2\x80", "\xDF\xBF", "\xE0\xA0\x80", "\xEF\xBF\xBF", "\xF0\x90\x80\x80", "\xF4\x8F\xBF\xBF"};
  for (const auto& ok : cases)
  {
    CAPTURE(ok);
    CHECK(Utf8Prefix(ok, 99) == ok);
  }
}

TEST_CASE("A pill label truncated mid-glyph never yields invalid UTF-8")
{
  using volum::custom::ShortCaptureLabel;
  const std::string euro = "\xE2\x82\xAC";
  const std::string ellipsis = "\u2026";

  // Cap 5 lands inside the second euro sign; it is dropped rather than split.
  CHECK(ShortCaptureLabel(euro + euro + euro) == euro + ellipsis);
  // "abc" is 3 bytes and the euro needs 3 more, so the euro does not fit at all.
  CHECK(ShortCaptureLabel("abc" + euro) == "abc" + ellipsis);
  // Here it fits exactly, so it is kept whole.
  CHECK(ShortCaptureLabel("ab" + euro + "cd") == "ab" + euro + ellipsis);
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

TEST_CASE("AssignedChannels returns only the present channels, sorted and deduped")
{
  // A Fryette-style amp with only gain stages 3 and 4 (across two cab slots).
  CustomAmp amp;
  amp.files = {{"a.nam", 0, 4}, {"b.nam", 1, 3}, {"c.nam", kDirectSlot, 3}, {"d.nam", kUnassignedSlot, 0}};
  const std::vector<int> chs = AssignedChannels(amp);
  REQUIRE(chs.size() == 2);
  CHECK(chs[0] == 3);
  CHECK(chs[1] == 4);
}

TEST_CASE("AssignedChannels is empty when nothing is assigned")
{
  CustomAmp amp;
  amp.files = {{"a.nam", kUnassignedSlot, 0}};
  CHECK(AssignedChannels(amp).empty());
}

TEST_CASE("SaveDisabledReason gates name, empty, unassigned, and duplicate states")
{
  // Name gate first: empty and the builder default both read as unnamed.
  CustomAmp unnamed;
  unnamed.files = {{"a.nam", kDirectSlot, 1}};
  REQUIRE(SaveDisabledReason(unnamed) == "Name your amp");
  CustomAmp defName;
  defName.name = "New custom amp";
  defName.files = {{"a.nam", kDirectSlot, 1}};
  REQUIRE(SaveDisabledReason(defName) == "Name your amp");

  CustomAmp empty;
  empty.name = "My amp";
  REQUIRE(SaveDisabledReason(empty) == "Add a .nam file");

  CustomAmp unassigned;
  unassigned.name = "My amp";
  unassigned.files = {{"a.nam", kUnassignedSlot, 0}};
  REQUIRE(SaveDisabledReason(unassigned) == "Assign every file a cab + channel");

  CustomAmp dup;
  dup.name = "My amp";
  dup.files = {{"a.nam", 0, 1}, {"b.nam", 0, 1}};
  REQUIRE(SaveDisabledReason(dup) == "Two files share a cab + channel");

  CustomAmp ok;
  ok.name = "My amp";
  ok.files = {{"a.nam", kDirectSlot, 1}, {"b.nam", 0, 1}};
  REQUIRE(SaveDisabledReason(ok).empty());
}

TEST_CASE("ParseNamFileName auto-fills slot/channel/cab from the factory convention")
{
  using volum::custom::kDirectSlot;
  using volum::custom::ParseNamFileName;

  auto g65 = ParseNamFileName("G65-2204-3.nam");
  REQUIRE(g65.matched);
  REQUIRE(g65.slot == 1);
  REQUIRE(g65.channel == 3);
  REQUIRE(g65.cabName == "G65");

  auto v30 = ParseNamFileName("/some/dir/V30-JCM800-2.nam");
  REQUIRE(v30.matched);
  REQUIRE(v30.slot == 2);
  REQUIRE(v30.channel == 2);

  auto g12 = ParseNamFileName("G12-Plexi-1.nam");
  REQUIRE(g12.matched);
  REQUIRE(g12.slot == 0);

  auto direct = ParseNamFileName("AMP-Ampt-1.nam");
  REQUIRE(direct.matched);
  REQUIRE(direct.slot == kDirectSlot);
  REQUIRE(direct.cabName.empty()); // DIRECT slot keeps its fixed label

  // Unrecognized prefix -> unmatched (left for manual assignment).
  auto weird = ParseNamFileName("MyCapture.nam");
  REQUIRE_FALSE(weird.matched);

  // Recognized prefix but non-numeric channel -> matched slot, channel stays 0.
  auto noCh = ParseNamFileName("G65-foo-bar.nam");
  REQUIRE(noCh.matched);
  REQUIRE(noCh.channel == 0);
}

TEST_CASE("Name uniqueness is case-insensitive within a content type")
{
  using namespace volum::custom;
  std::vector<std::string> list = {"Greenback", "Mesa 4x12"};
  REQUIRE(NameExistsCI(list, "greenback"));
  REQUIRE(NameExistsCI(list, "GREENBACK"));
  REQUIRE_FALSE(NameExistsCI(list, "Greenbac"));
  // exceptIdx lets a row keep its own name on rename.
  REQUIRE_FALSE(NameExistsCI(list, "Greenback", 0));
  REQUIRE(NameExistsCI(list, "Mesa 4x12", 0));
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
  // Preset banks are keyed by the active owner key (not the ampIdx arg), so pin a
  // fresh owner for test isolation.
  volum::custom::SetActivePresetOwner("test:add-dedup");
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
  volum::custom::SetActivePresetOwner("test:rename-delete");
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

TEST_CASE("Imported IR/pedal keep the exact source filename alongside the display name")
{
  // The display name defaults to the filename minus extension; the exact
  // filename is stored separately so renaming never loses provenance.
  const int iri = volum::custom::AddIR("Mesa OS", "Mesa_OS_4x12.wav");
  REQUIRE(volum::custom::IRFileAt(iri) == "Mesa_OS_4x12.wav");
  volum::custom::RenameIR(iri, "Renamed cab");
  REQUIRE(volum::custom::MockIRLibrary()[(size_t)iri] == "Renamed cab");
  REQUIRE(volum::custom::IRFileAt(iri) == "Mesa_OS_4x12.wav"); // filename unchanged by rename

  const int pi = volum::custom::AddPedal("Klon", "klon_clone.nam");
  REQUIRE(volum::custom::PedalFileAt(pi) == "klon_clone.nam");

  // Delete keeps the parallel filename store aligned (no off-by-one).
  REQUIRE(volum::custom::MockIRLibrary().size() == volum::custom::MockIRFiles().size());
  volum::custom::DeleteIR(iri);
  REQUIRE(volum::custom::MockIRLibrary().size() == volum::custom::MockIRFiles().size());
  REQUIRE(volum::custom::MockCustomPedals().size() == volum::custom::MockPedalFiles().size());
}

TEST_CASE("IRIdAt / IRIndexById round-trip and orphan-fallback to -1")
{
  using namespace volum::custom;
  const int idx = AddIR("Resolve me", "ir/ir_x__Resolve.wav");
  const std::string id = IRIdAt(idx);
  REQUIRE_FALSE(id.empty());
  CHECK(id.rfind("ir_", 0) == 0);
  CHECK(IRIndexById(id) == idx); // round-trips back to the same index
  CHECK(IRIndexById("ir_missing") == -1); // orphaned id -> -1 (baked-cab fallback)
  CHECK(IRIndexById("") == -1);
  CHECK(IRIdAt(99999).empty()); // out of range
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

TEST_CASE("Imported pedals get stable, monotonic, non-reused PRE-capture indices")
{
  using namespace volum::custom;
  const int base = volum::content::kCustomPedalIndexBase;

  const int i0 = AddPedal("Ped A", "pedals/a.nam");
  const int idxA = PedalLegacyIndexAt(i0);
  CHECK(idxA >= base);

  const int i1 = AddPedal("Ped B", "pedals/b.nam");
  const int idxB = PedalLegacyIndexAt(i1);
  CHECK(idxB == idxA + 1); // monotonic

  // Resolution by stable index round-trips name + stored path.
  CHECK(PedalNameByLegacy(idxA) == "Ped A");
  CHECK(PedalStoredPathByLegacy(idxB) == "pedals/b.nam");

  // Deleting A leaves B's index intact and never reuses A's index.
  DeletePedal(i0);
  CHECK(PedalNameByLegacy(idxB) == "Ped B");
  CHECK(PedalNameByLegacy(idxA).empty()); // A's index is now orphaned
  const int i2 = AddPedal("Ped C", "pedals/c.nam");
  CHECK(PedalLegacyIndexAt(i2) == idxB + 1); // monotonic, skips A's freed index
}

TEST_CASE("AddPedal refuses once the PRE-capture index pool is exhausted")
{
  using namespace volum::custom;
  auto& reg = volum::content::GlobalContentStore().reg();
  const int savedNext = reg.nextPedalIndex;

  // Last usable slot: minting here must succeed and consume kCustomPedalIndexMax.
  reg.nextPedalIndex = volum::content::kCustomPedalIndexMax;
  const size_t before = MockCustomPedals().size();
  const int last = AddPedal("Last slot", "pedals/last.nam");
  CHECK(last >= 0);
  CHECK(PedalLegacyIndexAt(last) == volum::content::kCustomPedalIndexMax);
  CHECK(MockCustomPedals().size() == before + 1);

  // Pool is now full (nextPedalIndex past max): further adds are rejected with -1
  // instead of clamping to a colliding legacy index that would alias captures.
  const size_t afterFull = MockCustomPedals().size();
  CHECK(AddPedal("Overflow", "pedals/of.nam") == -1);
  CHECK(MockCustomPedals().size() == afterFull); // nothing appended

  DeletePedal(last); // keep the shared library tidy for later cases
  reg.nextPedalIndex = savedNext;
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

TEST_CASE("ManifestHasFile makes re-importing the same .nam a no-op (case-insensitive)")
{
  using namespace volum::custom;
  CustomAmp amp;
  amp.files = {{"AMP-OD12-1.nam", kDirectSlot, 1}, {"G12-OD12-2.nam", 0, 2}};

  REQUIRE(ManifestHasFile(amp, "AMP-OD12-1.nam"));
  REQUIRE(ManifestHasFile(amp, "amp-od12-1.NAM")); // case-insensitive
  REQUIRE_FALSE(ManifestHasFile(amp, "AMP-OD12-9.nam")); // not present yet
  REQUIRE_FALSE(ManifestHasFile(CustomAmp{}, "anything.nam")); // empty manifest
}

TEST_CASE("UpdateCustomAmp edits an entry in place instead of appending a duplicate")
{
  using namespace volum::custom;
  const int idx = AddCustomAmp("Edit me", 1);
  const size_t before = MockCustomAmps().size();

  CustomAmp draft = CustomAmpAt(idx);
  draft.name = "Edited name";
  draft.art = 3;
  draft.cabNames = {"AA", "BB", "CC"};
  draft.files = {{"x.nam", kDirectSlot, 1}, {"y.nam", 0, 2}};

  const int outIdx = UpdateCustomAmp(idx, draft);
  REQUIRE(outIdx == idx); // same slot, no new row
  REQUIRE(MockCustomAmps().size() == before); // list did not grow
  REQUIRE(MockCustomAmps()[(size_t)idx] == "Edited name");
  REQUIRE(CustomAmpArt(idx) == 3);
  REQUIRE(MockCustomAmpCabs()[(size_t)idx][0] == "AA");
  REQUIRE(MockCustomAmpFiles()[(size_t)idx].size() == 2);

  // An amp may keep its own name (self-collision is allowed)...
  REQUIRE(UpdateCustomAmp(idx, draft) == idx);
  REQUIRE(MockCustomAmps()[(size_t)idx] == "Edited name");

  // ...but renaming onto *another* amp's name is de-duplicated.
  const int other = AddCustomAmp("Sibling", 0);
  CustomAmp clash = CustomAmpAt(idx);
  clash.name = "Sibling";
  UpdateCustomAmp(idx, clash);
  REQUIRE(MockCustomAmps()[(size_t)idx] != MockCustomAmps()[(size_t)other]);

  REQUIRE(UpdateCustomAmp(-1, draft) == -1); // out of range is a no-op
}

TEST_CASE("kNumCustomArts exposes six selectable styles")
{
  REQUIRE(volum::custom::kNumCustomArts == 6);
  const int idx = volum::custom::AddCustomAmp("Star amp", 5);
  REQUIRE(volum::custom::CustomAmpArt(idx) == 5);
}

// ---- F5 preset capture/recall hooks + owner routing -------------------------

TEST_CASE("AddPreset captures live settings via the plugin hook")
{
  using namespace volum::custom;
  SetActivePresetOwner("test:f5-capture");

  // The plugin installs a capture hook returning the current live scene; emulate
  // it here so the bridge stores a real snapshot rather than defaults.
  volum::VoLumAmpSettings live;
  live.toneBass = 8.25;
  live.activeIrId = "ir_live";
  live.preNam1Capture = 70;
  PresetCaptureHook() = [&live]() { return live; };

  const int i = AddPreset(0, "Snapshot");
  REQUIRE(i == 0);
  const std::string id = PresetIdAt(i);
  REQUIRE_FALSE(id.empty());

  const auto& bank = volum::content::GlobalContentStore().reg().presetBanks.at("test:f5-capture");
  REQUIRE(bank.size() == 1);
  CHECK(volum::AmpSettingsEqual(bank[0].settings, live));

  PresetCaptureHook() = nullptr; // don't leak the hook into other cases
}

TEST_CASE("OverwritePreset replaces the stored snapshot with the live capture")
{
  using namespace volum::custom;
  SetActivePresetOwner("test:f5-overwrite");

  volum::VoLumAmpSettings first;
  first.toneMid = 3.0;
  PresetCaptureHook() = [&first]() { return first; };
  const int i = AddPreset(0, "P");
  REQUIRE(i == 0);

  volum::VoLumAmpSettings second;
  second.toneMid = 9.0;
  PresetCaptureHook() = [&second]() { return second; };
  OverwritePreset(0, i);

  const auto& bank = volum::content::GlobalContentStore().reg().presetBanks.at("test:f5-overwrite");
  CHECK(volum::AmpSettingsEqual(bank[0].settings, second));

  PresetCaptureHook() = nullptr;
}

TEST_CASE("RecallPreset feeds the stored snapshot to the apply hook")
{
  using namespace volum::custom;
  SetActivePresetOwner("test:f5-recall");

  volum::VoLumAmpSettings stored;
  stored.toneTreble = 6.5;
  stored.supportCustomId = "amp_partner";
  PresetCaptureHook() = [&stored]() { return stored; };
  const int i = AddPreset(0, "Recall me");

  volum::VoLumAmpSettings applied;
  bool called = false;
  PresetApplyHook() = [&](const volum::VoLumAmpSettings& s) {
    applied = s;
    called = true;
  };
  RecallPreset(0, i);
  REQUIRE(called);
  CHECK(volum::AmpSettingsEqual(applied, stored));

  // Out-of-range recall is a safe no-op (hook not re-invoked).
  called = false;
  RecallPreset(0, 99);
  CHECK_FALSE(called);

  PresetCaptureHook() = nullptr;
  PresetApplyHook() = nullptr;
}

TEST_CASE("Preset banks are isolated by active owner key (factory vs custom amp)")
{
  using namespace volum::custom;
  SetActivePresetOwner("test:f5-ownerA");
  AddPreset(0, "OnlyA");
  SetActivePresetOwner("test:f5-ownerB");
  CHECK(MockPresetsForAmp(0).empty()); // a different owner sees an empty bank
  AddPreset(0, "OnlyB");

  SetActivePresetOwner("test:f5-ownerA");
  const auto a = MockPresetsForAmp(0);
  REQUIRE(a.size() == 1);
  CHECK(a[0] == "OnlyA");

  SetActivePresetOwner("test:f5-ownerB");
  const auto b = MockPresetsForAmp(0);
  REQUIRE(b.size() == 1);
  CHECK(b[0] == "OnlyB");
}

// Two plugin instances in one host share this bridge, and its capture/apply hooks
// each hold a raw `this`. They used to be installed once per instance at
// construction, so the newest instance owned preset capture and recall for every
// other one: saving a preset in the first instance stored the second instance's rig,
// recalling in the first changed the second, and closing that instance left the
// globals pointing at a destroyed object. Every operation now claims the bridge
// first, and a departing instance clears the hooks if they are still its own.
TEST_CASE("The newest claim owns the preset hooks, and a departing owner clears them")
{
  using namespace volum::custom;

  // Stand-ins for two plugin instances. Only their addresses matter.
  int instanceA = 0;
  int instanceB = 0;

  volum::VoLumAmpSettings sceneA;
  sceneA.toneBass = 1.5;
  volum::VoLumAmpSettings sceneB;
  sceneB.toneBass = 9.5;

  auto claim = [](const void* owner, const volum::VoLumAmpSettings& scene) {
    PresetCaptureHook() = [&scene]() { return scene; };
    PresetApplyHook() = [](const volum::VoLumAmpSettings&) {};
    PresetHookOwner() = owner;
  };

  SetActivePresetOwner("test:f5-two-instances");

  // B was created last, but A is the one operating, so A claims and A's scene is
  // what gets stored. Before the fix this stored B's.
  claim(&instanceB, sceneB);
  claim(&instanceA, sceneA);
  const int i = AddPreset(0, "From A");
  REQUIRE(i >= 0);
  const auto& bank = volum::content::GlobalContentStore().reg().presetBanks.at("test:f5-two-instances");
  CHECK(bank[(size_t)i].settings.toneBass == doctest::Approx(1.5));

  // B closing must not disturb hooks A owns.
  ClearPresetHooksIfOwnedBy(&instanceB);
  CHECK(PresetHookOwner() == &instanceA);
  REQUIRE(static_cast<bool>(PresetCaptureHook()));
  CHECK(PresetCaptureHook()().toneBass == doctest::Approx(1.5));

  // A closing does clear them, so nothing is left pointing at a dead instance.
  ClearPresetHooksIfOwnedBy(&instanceA);
  CHECK(PresetHookOwner() == nullptr);
  CHECK_FALSE(static_cast<bool>(PresetCaptureHook()));
  CHECK_FALSE(static_cast<bool>(PresetApplyHook()));

  // And a bridge with no hooks stores defaults rather than dereferencing anything.
  const int j = AddPreset(0, "No hooks");
  REQUIRE(j >= 0);
  CHECK(volum::AmpSettingsEqual(bank[(size_t)j].settings, volum::VoLumAmpSettings{}));
}

// Destructive confirmations name an item but used to remember only its row. The
// library is process-global, so a second plugin editor can remove an earlier row
// while a prompt is open, after which the remembered position belongs to a
// different item. These are the lookups the overlay and the builder now use instead;
// with them, "delete Foo" can only ever delete Foo.
TEST_CASE("An item's identity survives rows shifting underneath it")
{
  using namespace volum::custom;

  SetActivePresetOwner("test:stale-index");
  AddPreset(0, "First");
  AddPreset(0, "Second");
  AddPreset(0, "Third");

  const std::string secondId = PresetIdAt(1);
  const std::string thirdId = PresetIdAt(2);
  REQUIRE_FALSE(secondId.empty());
  REQUIRE(PresetIndexById(thirdId) == 2);

  // The row the prompt was opened on now holds a different preset.
  DeletePreset(0, 0);
  CHECK(PresetIdAt(1) == thirdId);

  // Resolving by id follows the item; resolving by the remembered row would have
  // hit "Third".
  CHECK(PresetIndexById(secondId) == 0);
  CHECK(PresetIndexById(thirdId) == 1);

  // An id that is gone resolves to nothing, so the caller can say so instead of
  // acting on a neighbour.
  CHECK(PresetIndexById("preset_does_not_exist") == -1);
  CHECK(PresetIndexById("") == -1);
}

TEST_CASE("IRs, pedals and custom amps resolve by id the same way")
{
  using namespace volum::custom;

  const int irA = AddIR("StaleA.wav", "ir/a.wav");
  const int irB = AddIR("StaleB.wav", "ir/b.wav");
  REQUIRE(irB == irA + 1);
  const std::string irBId = IRIdAt(irB);
  REQUIRE_FALSE(irBId.empty());
  DeleteIR(irA);
  CHECK(IRIndexById(irBId) == irA); // shifted down by one, still the same IR
  CHECK(IRIndexById("ir_gone") == -1);

  const int pedA = AddPedal("PedA.nam", "pedals/a.nam");
  const int pedB = AddPedal("PedB.nam", "pedals/b.nam");
  REQUIRE(pedA >= 0);
  REQUIRE(pedB >= 0);
  const std::string pedBId = PedalIdAt(pedB);
  REQUIRE_FALSE(pedBId.empty());
  DeletePedal(pedA);
  CHECK(PedalIndexById(pedBId) == pedB - 1);
  CHECK(PedalIndexById("pedal_gone") == -1);

  const int ampA = AddCustomAmp("StaleAmpA", 0);
  const int ampB = AddCustomAmp("StaleAmpB", 1);
  const std::string ampBId = CustomAmpIdAt(ampB);
  REQUIRE_FALSE(ampBId.empty());
  RemoveCustomAmp(ampA);
  CHECK(CustomAmpIndexById(ampBId) == ampB - 1);
  CHECK(CustomAmpIndexById("amp_gone") == -1);
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
