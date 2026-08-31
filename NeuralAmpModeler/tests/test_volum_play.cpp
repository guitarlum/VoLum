#include "third_party/doctest.h"

#include "VoLumChunkIdTail.h"
#include "VoLumPlayModel.h"

TEST_CASE("PLAY mode defaults to BUILD and round-trips valid values")
{
  CHECK(volum::UiModeFromString("") == volum::UiMode::Build);
  CHECK(volum::UiModeFromString("future") == volum::UiMode::Build);
  CHECK(volum::UiModeFromString("build") == volum::UiMode::Build);
  CHECK(volum::UiModeFromString("play") == volum::UiMode::Play);
  CHECK(std::string(volum::UiModeToString(volum::UiMode::Play)) == "play");

  volum::ChunkIdTail tail;
  CHECK(tail.uiMode == "build");
  tail.uiMode = "play";
  CHECK(volum::IdTailFromJson(volum::IdTailToJson(tail)).uiMode == "play");
  nlohmann::json oldTail = nlohmann::json::object();
  CHECK(volum::IdTailFromJson(oldTail).uiMode == "build");
  nlohmann::json standalone = {{"volumUiMode", "play"}};
  CHECK(volum::UiModeFromJson(standalone, "volumUiMode") == volum::UiMode::Play);
  CHECK(volum::UiModeFromJson(nlohmann::json::object(), "volumUiMode") == volum::UiMode::Build);
  CHECK(volum::ActionForUiModeTransition(volum::UiMode::Build, volum::UiMode::Play)
        == volum::UiModeTransitionAction::RefreshOnly);
}

TEST_CASE("PLAY stomps own exactly the eight performance bypass parameters")
{
  const std::array<std::string, 8> expected = {
    "PrePitchActive", "PreCompActive", "PreNam1Active", "PreNam2Active",
    "ChorusActive",   "DelayActive",   "ReverbActive", "TremoloActive"};
  for (size_t i = 0; i < expected.size(); ++i)
    CHECK(volum::kPlayBypassParamNames[i] == expected[i]);
}

TEST_CASE("PLAY bypass edits make a recalled snapshot dirty")
{
  volum::VoLumAmpSettings recalled;
  auto live = recalled;
  live.preCompActive = !recalled.preCompActive;
  CHECK(volum::IsPlaySnapshotDirty(true, live, recalled));
  CHECK_FALSE(volum::IsPlaySnapshotDirty(false, live, recalled));
}

TEST_CASE("midiSoundMap is ordered, replaceable, clearable, and persistent")
{
  volum::content::Registry registry;
  CHECK(volum::content::FirstFreeMidiSoundSlot(registry) == 0);
  CHECK(volum::content::AssignMidiSound(registry, 12, "factory:2", "factory:2:v1"));
  CHECK(volum::content::AssignMidiSound(registry, 3, "factory:1", "factory:1:v1"));
  CHECK(volum::content::AssignMidiSound(registry, 12, "factory:4", "factory:4:v1"));
  REQUIRE(registry.midiSoundMap.size() == 2);
  CHECK(registry.midiSoundMap.count(3) == 1);
  CHECK(registry.midiSoundMap.count(12) == 1);
  CHECK(registry.midiSoundMap.at(12).ampId == "factory:4"); // reassigned, not appended

  const auto restored = volum::content::RegistryFromJson(volum::content::RegistryToJson(registry));
  REQUIRE(restored.midiSoundMap.size() == 2);
  CHECK(restored.midiSoundMap.count(3) == 1);
  CHECK(restored.midiSoundMap.at(12).presetId == "factory:4:v1");

  CHECK(volum::content::ClearMidiSound(registry, 3));
  CHECK_FALSE(volum::content::ClearMidiSound(registry, 3));
  CHECK(volum::content::FirstFreeMidiSoundSlot(registry) == 0);
}

TEST_CASE("PLAY slot helper distinguishes empty assigned and invalid slots in PC order")
{
  const auto factory = volum::DefaultFactoryPresets();
  volum::content::Registry registry;
  CHECK(volum::BuildPlaySlots(factory, registry).empty());

  volum::content::AssignMidiSound(registry, 9, "missing-amp", "missing-preset");
  volum::content::AssignMidiSound(registry, 2, "factory:7", "factory:7:v1");
  const auto slots = volum::BuildPlaySlots(factory, registry);
  REQUIRE(slots.size() == 2);
  CHECK(slots[0].slot == 2);
  CHECK(slots[0].valid);
  CHECK(slots[0].sound.presetName == "Ready");
  CHECK(slots[1].slot == 9);
  CHECK_FALSE(slots[1].valid);
  CHECK(slots[1].sound.presetName == "Invalid slot");
}

TEST_CASE("Last-recalled highlight survives dirty edits but not another origin")
{
  volum::PlaySlot slot;
  slot.slot = 5;
  slot.valid = true;
  slot.sound.ampId = "factory:7";
  slot.sound.presetId = "factory:7:v1";

  // Dirty is intentionally absent from this predicate: stomping keeps origin.
  CHECK(volum::IsLastRecalledSlot(slot, 5, "factory:7", "factory:7:v1"));
  CHECK_FALSE(volum::IsLastRecalledSlot(slot, 6, "factory:7", "factory:7:v1"));
  CHECK_FALSE(volum::IsLastRecalledSlot(slot, 5, "factory:8", "factory:8:v1"));
}
