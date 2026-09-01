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
  CHECK(volum::UiModeFromMachineSettings(true, standalone, volum::UiMode::Build) == volum::UiMode::Play);
  CHECK(volum::UiModeFromMachineSettings(false, standalone, volum::UiMode::Build) == volum::UiMode::Build);
  CHECK(volum::UiModeFromMachineSettings(false, standalone, volum::UiMode::Play) == volum::UiMode::Play);
  nlohmann::json midi = {{"midiCh", 7}};
  CHECK(volum::MidiChannelFromJson(midi) == 7);
  CHECK(volum::MidiChannelFromJson(nlohmann::json::object(), 3) == 3);
  CHECK(volum::MidiChannelFromJson({{"midiCh", 99}}) == 16);
  CHECK(volum::MidiChannelFromMachineSettings(true, midi, 0) == 7);
  CHECK(volum::MidiChannelFromMachineSettings(false, midi, 0) == 0);
  CHECK(volum::MidiChannelFromMachineSettings(false, midi, 12) == 12);
  CHECK(volum::ActionForUiModeTransition(volum::UiMode::Build, volum::UiMode::Play)
        == volum::UiModeTransitionAction::RefreshOnly);
}

TEST_CASE("PLAY stomps own exactly the eight performance bypass parameters")
{
  const std::array<std::string, 8> expected = {"PrePitchActive", "PreCompActive", "PreNam1Active", "PreNam2Active",
                                               "ChorusActive",   "DelayActive",   "ReverbActive",  "TremoloActive"};
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

TEST_CASE("PLAY arrows step only the slots a Program Change could actually recall")
{
  const auto factory = volum::DefaultFactoryPresets();
  volum::content::Registry registry;

  // Nothing assigned: the arrows have nowhere to go and must say so rather than
  // silently recalling slot 0.
  CHECK(volum::StepAssignedSlot(volum::BuildPlaySlots(factory, registry), -1, 1) == -1);
  CHECK(volum::StepAssignedSlot(volum::BuildPlaySlots(factory, registry), 4, -1) == -1);

  volum::content::AssignMidiSound(registry, 2, "factory:7", "factory:7:v1");
  volum::content::AssignMidiSound(registry, 5, "missing-amp", "missing-preset"); // hole that reads red
  volum::content::AssignMidiSound(registry, 9, "factory:3", "factory:3:v1");
  const auto slots = volum::BuildPlaySlots(factory, registry);

  // Nothing recalled yet: down starts at the top of the rail, up at the bottom.
  CHECK(volum::StepAssignedSlot(slots, -1, 1) == 2);
  CHECK(volum::StepAssignedSlot(slots, -1, -1) == 9);

  // 5 is assigned but unresolvable, so stepping jumps straight over it.
  CHECK(volum::StepAssignedSlot(slots, 2, 1) == 9);
  CHECK(volum::StepAssignedSlot(slots, 9, -1) == 2);

  // Wrap at both ends: the rail is short and holding Down means "the next one".
  CHECK(volum::StepAssignedSlot(slots, 9, 1) == 2);
  CHECK(volum::StepAssignedSlot(slots, 2, -1) == 9);

  // A slot that is not reachable any more (cleared, or gone invalid while live)
  // hands off to the neighbour in the direction of travel, not back to the start.
  CHECK(volum::StepAssignedSlot(slots, 5, 1) == 9);
  CHECK(volum::StepAssignedSlot(slots, 5, -1) == 2);
  CHECK(volum::StepAssignedSlot(slots, 40, 1) == 2); // past the end, so wrap
  CHECK(volum::StepAssignedSlot(slots, 0, -1) == 9); // before the start, so wrap

  // One reachable slot: stepping is a no-op that still resolves to that slot,
  // never to -1, so the key stays consumed instead of falling through to BUILD.
  volum::content::Registry one;
  volum::content::AssignMidiSound(one, 11, "factory:7", "factory:7:v1");
  const auto single = volum::BuildPlaySlots(factory, one);
  CHECK(volum::StepAssignedSlot(single, 11, 1) == 11);
  CHECK(volum::StepAssignedSlot(single, 11, -1) == 11);
}

TEST_CASE("SwapMidiSoundSlots exchanges program numbers and can move onto a hole")
{
  volum::content::Registry registry;
  REQUIRE(volum::content::AssignMidiSound(registry, 2, "factory:7", "factory:7:v1"));
  REQUIRE(volum::content::AssignMidiSound(registry, 9, "factory:3", "factory:3:v1"));

  CHECK_FALSE(volum::content::SwapMidiSoundSlots(registry, -1, 2));
  CHECK_FALSE(volum::content::SwapMidiSoundSlots(registry, 2, 128));
  CHECK(volum::content::SwapMidiSoundSlots(registry, 2, 2));
  CHECK(registry.midiSoundMap.at(2).ampId == "factory:7");

  REQUIRE(volum::content::SwapMidiSoundSlots(registry, 2, 9));
  CHECK(registry.midiSoundMap.at(2).ampId == "factory:3");
  CHECK(registry.midiSoundMap.at(9).ampId == "factory:7");

  REQUIRE(volum::content::SwapMidiSoundSlots(registry, 9, 4)); // 4 is empty: move, do not copy
  CHECK(registry.midiSoundMap.count(9) == 0);
  CHECK(registry.midiSoundMap.at(4).ampId == "factory:7");
  CHECK(registry.midiSoundMap.at(2).ampId == "factory:3");
}

TEST_CASE("MIDI map mutation edges preserve assignments holes bounds and first-free order")
{
  using namespace volum::content;
  Registry registry;

  SUBCASE("empty-empty and same-slot swaps are successful no-ops")
  {
    CHECK(SwapMidiSoundSlots(registry, 4, 11));
    CHECK(registry.midiSoundMap.empty());

    REQUIRE(AssignMidiSound(registry, 4, "factory:7", "factory:7:v1"));
    const auto before = RegistryToJson(registry);
    CHECK(SwapMidiSoundSlots(registry, 4, 4));
    CHECK(RegistryToJson(registry) == before);
  }

  SUBCASE("both positions are range-checked without mutating the map")
  {
    REQUIRE(AssignMidiSound(registry, 6, "factory:7", "factory:7:v1"));
    const auto before = RegistryToJson(registry);
    CHECK_FALSE(SwapMidiSoundSlots(registry, -1, 6));
    CHECK_FALSE(SwapMidiSoundSlots(registry, 6, -1));
    CHECK_FALSE(SwapMidiSoundSlots(registry, 128, 6));
    CHECK_FALSE(SwapMidiSoundSlots(registry, 6, 128));
    CHECK(RegistryToJson(registry) == before);

    CHECK_FALSE(AssignMidiSound(registry, -1, "factory:2", "factory:2:v1"));
    CHECK_FALSE(AssignMidiSound(registry, 128, "factory:2", "factory:2:v1"));
    CHECK(RegistryToJson(registry) == before);
  }

  SUBCASE("assigning an occupied slot replaces exactly that Sound")
  {
    REQUIRE(AssignMidiSound(registry, 3, "factory:1", "factory:1:v1"));
    REQUIRE(AssignMidiSound(registry, 8, "factory:2", "factory:2:v1"));
    REQUIRE(AssignMidiSound(registry, 3, "factory:9", "factory:9:v1"));
    REQUIRE(registry.midiSoundMap.size() == 2);
    CHECK(registry.midiSoundMap.at(3).ampId == "factory:9");
    CHECK(registry.midiSoundMap.at(3).presetId == "factory:9:v1");
    CHECK(registry.midiSoundMap.at(8).ampId == "factory:2");
  }

  SUBCASE("occupied slots exchange both ids and a hole move frees the old lowest slot")
  {
    REQUIRE(AssignMidiSound(registry, 0, "factory:1", "factory:1:v1"));
    REQUIRE(AssignMidiSound(registry, 1, "factory:2", "factory:2:v1"));
    CHECK(FirstFreeMidiSoundSlot(registry) == 2);

    REQUIRE(SwapMidiSoundSlots(registry, 0, 1));
    CHECK(registry.midiSoundMap.at(0).ampId == "factory:2");
    CHECK(registry.midiSoundMap.at(0).presetId == "factory:2:v1");
    CHECK(registry.midiSoundMap.at(1).ampId == "factory:1");
    CHECK(registry.midiSoundMap.at(1).presetId == "factory:1:v1");
    CHECK(FirstFreeMidiSoundSlot(registry) == 2);

    REQUIRE(SwapMidiSoundSlots(registry, 0, 9));
    CHECK(registry.midiSoundMap.count(0) == 0);
    CHECK(registry.midiSoundMap.at(9).ampId == "factory:2");
    CHECK(registry.midiSoundMap.at(1).ampId == "factory:1");
    CHECK(FirstFreeMidiSoundSlot(registry) == 0);
  }
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

TEST_CASE("Two slots with the same Sound: last recalled PC wins")
{
  volum::PlaySlot a;
  a.slot = 2;
  a.valid = true;
  a.sound.ampId = "factory:7";
  a.sound.presetId = "factory:7:v1";
  volum::PlaySlot b = a;
  b.slot = 9;
  CHECK_FALSE(volum::IsLastRecalledSlot(a, 9, "factory:7", "factory:7:v1"));
  CHECK(volum::IsLastRecalledSlot(b, 9, "factory:7", "factory:7:v1"));
  CHECK(volum::IsLastRecalledSlot(a, 2, "factory:7", "factory:7:v1"));
}

TEST_CASE("Save As from a Factory PLAY origin drops LIVE on that slot")
{
  volum::PlaySlot origin;
  origin.slot = 4;
  origin.valid = true;
  origin.sound.ampId = "factory:7";
  origin.sound.presetId = "factory:7:v1";
  CHECK(volum::IsLastRecalledSlot(origin, 4, "factory:7", "factory:7:v1"));
  CHECK_FALSE(volum::IsLastRecalledSlot(origin, 4, "factory:7", "preset_lead"));
  CHECK(volum::PlayPlusAddsHeard(false, false, false));
  volum::content::Registry registry;
  volum::content::AssignMidiSound(registry, 4, "factory:7", "factory:7:v1");
  CHECK(volum::content::FirstFreeMidiSoundSlot(registry) == 0);
}

TEST_CASE("Default with no snapshot dirties against factory settings")
{
  volum::VoLumAmpSettings live;
  CHECK_FALSE(volum::LivePresetDirty(false, live, {}));
  live.toneBass = 8.0;
  CHECK(volum::LivePresetDirty(false, live, {}));
  volum::VoLumAmpSettings recalled = live;
  CHECK_FALSE(volum::LivePresetDirty(true, live, recalled));
  live.toneMid = 2.0;
  CHECK(volum::LivePresetDirty(true, live, recalled));
}
