#include "third_party/doctest.h"

#include "../VoLumCabStep.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Regression coverage for the S shortcut (cycle cab).
//
// The keyboard carried its own copy of the cab logic, and that copy had no
// custom-amp branch: with a custom amp focused, S wrote the *underlying factory*
// amp's saved cab, rescanned the factory rig folder for channel labels, and never
// touched the custom lane's routing. The stepper relabelled itself under the custom
// amp's name, the row could highlight a slot the amp has no capture for, the audio
// did not change - and the custom amp's persisted gain stage was corrupted for the
// next open. It also stepped straight onto unavailable slots, which a click cannot
// do, since the copy knew nothing about the row's enable state.
//
// The fix routes S through the row's own step, so mouse and keyboard share one
// path. These are the stepping rules that path uses.

using volum::kNumCabRowSlots;
using volum::NextSelectableCab;

namespace
{
constexpr bool kAllAvailable[kNumCabRowSlots] = {true, true, true, true};
} // namespace

TEST_CASE("With every slot available, stepping walks the row and wraps")
{
  CHECK(NextSelectableCab(0, 1, false, kAllAvailable) == 1);
  CHECK(NextSelectableCab(1, 1, false, kAllAvailable) == 2);
  CHECK(NextSelectableCab(3, 1, false, kAllAvailable) == 0);
  CHECK(NextSelectableCab(0, -1, false, kAllAvailable) == 3);
  CHECK(NextSelectableCab(2, -1, false, kAllAvailable) == 1);
}

TEST_CASE("Slots the mouse cannot click are skipped, not landed on")
{
  // A custom amp with a capture in one cab slot only, on a channel with no DIRECT
  // capture: No Cab and two of the three cabs are drawn disabled, and clicking any
  // of them does nothing. Stepping used to land on them anyway, highlighting a slot
  // that plays nothing.
  const bool onlySlot2[kNumCabRowSlots] = {false, false, true, false};

  CHECK(NextSelectableCab(2, 1, false, onlySlot2) == -1); // nowhere else to go
  CHECK(NextSelectableCab(2, -1, false, onlySlot2) == -1);
  CHECK(NextSelectableCab(0, 1, false, onlySlot2) == 2);
  CHECK(NextSelectableCab(3, 1, false, onlySlot2) == 2);

  // No Cab unavailable (this channel has no DIRECT capture) but all three cabs
  // present: forward from the last cab wraps past No Cab to the first.
  const bool noDirect[kNumCabRowSlots] = {false, true, true, true};
  CHECK(NextSelectableCab(3, 1, false, noDirect) == 1);
  CHECK(NextSelectableCab(1, -1, false, noDirect) == 3);
}

TEST_CASE("A row with nothing available refuses the gesture")
{
  const bool none[kNumCabRowSlots] = {false, false, false, false};
  CHECK(NextSelectableCab(0, 1, false, none) == -1);
  CHECK(NextSelectableCab(0, -1, true, none) == -1);
}

TEST_CASE("A step with no direction is not a step")
{
  CHECK(NextSelectableCab(1, 0, false, kAllAvailable) == -1);
  CHECK(NextSelectableCab(1, 0, true, kAllAvailable) == -1);
}

TEST_CASE("With a custom IR active, a single available slot is still worth selecting")
{
  // The IR is the active cab, so the highlighted slot is not really selected;
  // picking it retires the IR, exactly as clicking the highlighted button does.
  // Reporting "nothing to do" here would leave S unable to get out of an IR on an
  // amp with one usable cab.
  const bool onlySlot1[kNumCabRowSlots] = {false, true, false, false};
  CHECK(NextSelectableCab(1, 1, true, onlySlot1) == 1);
  CHECK(NextSelectableCab(1, 1, false, onlySlot1) == -1);
}

TEST_CASE("The keyboard shortcut runs the row's own step, not a second copy of the cab logic")
{
  const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
  auto read = [](const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  };

  const std::string keyboard = read(root / "VoLumKeyboard.inc.cpp");
  const std::string row = read(root / "VoLumSpeakerRow.h");

  // One path: the shortcut asks the row to step, and the row runs the click callback.
  CHECK(keyboard.find("spkCtrl->As<VoLumSpeakerRowControl>()->StepKeyboard(direction)") != std::string::npos);
  CHECK(row.find("bool StepKeyboard(int direction)") != std::string::npos);
  CHECK(row.find("mCallback(next);") != std::string::npos);
  CHECK(row.find("volum::NextSelectableCab(mSelected, direction, mIrCabActive, selectable)") != std::string::npos);

  // And the duplicate that had no custom-amp branch is gone: the shortcut no longer
  // writes a factory cab index or rescans the factory rig folder.
  const auto fn = keyboard.find("bool NeuralAmpModeler::_CycleVoLumKeyboardSpeaker(int direction)");
  REQUIRE(fn != std::string::npos);
  const auto fnEnd = keyboard.find("\n}", fn);
  REQUIRE(fnEnd != std::string::npos);
  const std::string body = keyboard.substr(fn, fnEnd - fn);

  CHECK(body.find("mVolumAmpSettings[mVolumAmpIdx].speakerIdx") == std::string::npos);
  CHECK(body.find("_VolumRefreshChannels()") == std::string::npos);
  CHECK(body.find("GetParam(kSupportSpeakerIdx)->Set(") == std::string::npos);

  // Mouse and keyboard share one definition of "this slot can be chosen".
  CHECK(row.find("bool Selectable(int idx) const") != std::string::npos);
  CHECK(row.find("Contains(x, y) && Selectable(i)") != std::string::npos);
}
