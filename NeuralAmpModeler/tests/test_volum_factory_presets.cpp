#include "third_party/doctest.h"

#include "VoLumCustomContentApi.h"
#include "VoLumFactoryPresets.h"
#include "VoLumPlayModel.h"

#include <filesystem>
#include <fstream>

namespace
{
struct GlobalStoreReset
{
  GlobalStoreReset()
  {
    auto& store = volum::content::GlobalContentStore();
    store.SetBaseDir({});
    store.reg() = {};
    volum::custom::SetActivePresetOwner(volum::content::FactoryOwnerKey(0));
    volum::custom::PresetCaptureHook() = nullptr;
    volum::custom::PresetApplyHook() = nullptr;
    volum::custom::PresetHooksByInstance().clear();
  }
  ~GlobalStoreReset()
  {
    volum::custom::PresetCaptureHook() = nullptr;
    volum::custom::PresetApplyHook() = nullptr;
    volum::custom::PresetHookOwner() = nullptr;
    volum::custom::PresetHooksByInstance().clear();
  }
};
} // namespace

TEST_CASE("Factory bank exposes one stable Ready preset for every factory amp")
{
  const auto bank = volum::DefaultFactoryPresets();
  REQUIRE(bank.size() == volum::kAmpCount);
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    CAPTURE(i);
    CHECK(bank[(size_t)i].id == "factory:" + std::to_string(i) + ":v1");
    CHECK(bank[(size_t)i].ampIdx == i);
    CHECK(volum::IsFactoryPresetId(bank[(size_t)i].id));
  }
}

TEST_CASE("Factory rows are immutable and custom amps never gain one")
{
  const auto bank = volum::DefaultFactoryPresets();
  CHECK_FALSE(volum::FactoryPresetCanMutate(bank[0].id));
  CHECK(volum::FactoryPresetCanMutate("preset_user"));
  CHECK(volum::FindFactoryPresetForAmp(bank, 4) != nullptr);
  CHECK(volum::FindFactoryPresetForAmp(bank, volum::kAmpCount) == nullptr);
}

TEST_CASE("Dirty Factory Save creates a User preset and leaves Factory unchanged")
{
  GlobalStoreReset reset;
  auto& store = volum::content::GlobalContentStore();
  const auto factory = volum::DefaultFactoryPresets();
  const auto before = factory[3].settings;
  CHECK(volum::SaveActionForActivePreset(factory[3].id) == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset("") == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset("preset_user") == volum::PresetSaveAction::OverwriteUser);

  volum::custom::SetActivePresetOwner(volum::content::FactoryOwnerKey(3));
  volum::custom::PresetCaptureHook() = [] {
    volum::VoLumAmpSettings changed;
    changed.toneBass = 8.5;
    return changed;
  };
  const int index = volum::custom::AddPreset(3, "Ready edit");

  REQUIRE(index == 0);
  const auto& user = store.reg().presetBanks.at("factory:3")[0];
  CHECK(user.id.rfind("preset_", 0) == 0);
  CHECK(user.settings.toneBass == doctest::Approx(8.5));
  CHECK(volum::AmpSettingsEqual(factory[3].settings, before));
}

TEST_CASE("Factory snapshot file can revoice a preset without changing its id")
{
  const auto temp = std::filesystem::temp_directory_path() / "volum-factory-presets-test.json";
  volum::VoLumAmpSettings revoiced;
  revoiced.toneMid = 7.25;
  nlohmann::json root;
  root["factory:6:v1"] = {{"name", "Ready"}, {"settings", volum::AmpSettingsToJson(revoiced)}};
  {
    std::ofstream out(temp);
    out << root.dump(2);
  }

  const auto bank = volum::LoadFactoryPresets(temp);
  std::error_code ec;
  std::filesystem::remove(temp, ec);
  REQUIRE(bank.size() == volum::kAmpCount);
  REQUIRE(bank[6].id == "factory:6:v1");
  CHECK(bank[6].settings.toneMid == doctest::Approx(7.25));
}

// Default (_VolumResetAmpToFactory) assigns VoLumAmpSettings{}, and a shipped
// Ready whose snapshot is "settings": {} inherits it, so every one of them has to
// make a pedal audible the moment it is switched on.
TEST_CASE("Fresh scene, Default and an empty Ready snapshot switch pedals on audibly")
{
  const auto temp = std::filesystem::temp_directory_path() / "volum-factory-pedal-defaults-test.json";
  nlohmann::json root;
  root["factory:0:v1"] = {{"name", "Ready"}, {"settings", nlohmann::json::object()}};
  {
    std::ofstream out(temp);
    out << root.dump(2);
  }
  const auto loaded = volum::LoadFactoryPresets(temp);
  std::error_code ec;
  std::filesystem::remove(temp, ec);
  REQUIRE(loaded.size() == volum::kAmpCount);
  const auto builtIn = volum::DefaultFactoryPresets();
  const volum::VoLumAmpSettings fresh;
  const auto healedReady = volum::HealedFactoryPresetSettings(loaded[0].settings);

  const auto ensemble = volum::kVoLumChorusModeDefaults[volum::kVoLumChorusModeEnsemble];
  const volum::VoLumAmpSettings* scenes[] = {&fresh, &builtIn[0].settings, &loaded[0].settings, &healedReady};
  for (const auto* s : scenes)
  {
    CAPTURE(s - scenes[0]);
    CHECK_FALSE(s->prePitchActive);
    CHECK(s->prePitchMode == volum::kVoLumPitchModeOctaver);
    CHECK(s->prePitchSemitones == doctest::Approx(-2.0));
    // The Octaver's own knobs keep their factory blend.
    CHECK(s->prePitchMix == doctest::Approx(1.0));
    CHECK(s->prePitchOctDown == doctest::Approx(0.8));
    CHECK(s->prePitchOctUp == doctest::Approx(0.0));
    CHECK(s->prePitchDry == doctest::Approx(1.0));
    CHECK(s->prePitchVoicing == volum::kVoLumPitchVoicingModern);
    CHECK(s->prePitchLevel == doctest::Approx(0.0));

    CHECK_FALSE(s->postChorusActive);
    CHECK(s->postChorusMode == volum::kVoLumChorusModeEnsemble);
    CHECK(s->postChorusRate == doctest::Approx(ensemble.rate));
    CHECK(s->postChorusDepth == doctest::Approx(ensemble.depth));
    CHECK(s->postChorusTone == doctest::Approx(ensemble.tone));
    CHECK(s->postChorusWidth == doctest::Approx(ensemble.width));
    CHECK(s->postChorusMix == doctest::Approx(ensemble.mix));

    // PRE NAM slots stay EMPTY: no capture is chosen for the user.
    CHECK_FALSE(s->preNam1Active);
    CHECK_FALSE(s->preNam2Active);
    CHECK(s->preNam1Capture == 0);
    CHECK(s->preNam2Capture == 0);
  }

  // The live POST working copy seeds the same voice.
  CHECK(volum::VoLumEffectSettings{}.chorusMode == volum::kVoLumChorusModeEnsemble);
}

TEST_CASE("A factory preset shows the name its snapshot file gives it")
{
  const auto temp = std::filesystem::temp_directory_path() / "volum-factory-preset-names-test.json";
  nlohmann::json root;
  root["factory:12:v1"] = {{"name", "  Texas Crunch  "}, {"settings", nlohmann::json::object()}};
  root["factory:3:v1"] = {{"name", ""}, {"settings", nlohmann::json::object()}};
  root["factory:4:v1"] = {{"name", std::string(80, 'x')}, {"settings", nlohmann::json::object()}};
  {
    std::ofstream out(temp);
    out << root.dump(2);
  }
  const auto bank = volum::LoadFactoryPresets(temp);
  std::error_code ec;
  std::filesystem::remove(temp, ec);
  REQUIRE(bank.size() == volum::kAmpCount);
  CHECK(bank[12].name == "Texas Crunch");
  CHECK(bank[12].id == "factory:12:v1"); // the id PLAY slots and MIDI maps point at is unchanged
  CHECK(bank[3].name == volum::kFactoryPresetDisplayName);
  CHECK(bank[0].name == volum::kFactoryPresetDisplayName); // absent from the file
  CHECK(bank[4].name.size() == volum::kFactoryPresetNameMaxBytes);

  volum::content::Registry registry;
  const auto sounds = volum::BuildSoundChoices(bank, registry);
  CHECK(sounds[12].presetName == "Texas Crunch");
  volum::SoundChoice resolved;
  REQUIRE(volum::ResolveSound(bank, registry, volum::content::FactoryOwnerKey(12), "factory:12:v1", resolved));
  CHECK(resolved.presetName == "Texas Crunch");
}

TEST_CASE("Factory Sounds are available to PLAY without seeding midiSoundMap")
{
  volum::content::Registry registry;
  const auto sounds = volum::BuildSoundChoices(volum::DefaultFactoryPresets(), registry);
  REQUIRE(sounds.size() == volum::kAmpCount);
  CHECK(sounds[0].presetName == "Ready");
  CHECK(sounds[0].factory);
  CHECK(registry.midiSoundMap.empty());
}

TEST_CASE("Factory Ready dirty ignores the postValid restore sentinel")
{
  // Live save always stamps postValid=true. Shipped Ready is VoLumAmpSettings{}
  // (postValid=false). That sentinel is not a knob: after a no-edit relaunch
  // the sounding scenes match, so PLAY + must assign Ready without Save As.
  const auto factory = volum::DefaultFactoryPresets();
  REQUIRE_FALSE(factory[0].settings.postValid);

  volum::VoLumAmpSettings live = factory[0].settings;
  live.postValid = true;
  REQUIRE(volum::AmpSettingsEqual(live, factory[0].settings));
  REQUIRE_FALSE(volum::LivePresetDirty(true, live, factory[0].settings));
  REQUIRE_FALSE(volum::AddHeardNeedsSaveAs(volum::PresetSaveAction::SaveUserCopy, false, false));
  REQUIRE_FALSE(volum::AddHeardNeedsSaveAs(
    volum::PresetSaveAction::SaveUserCopy, volum::LivePresetDirty(true, live, factory[0].settings), false));

  live.toneBass = 8.0;
  REQUIRE(volum::LivePresetDirty(true, live, factory[0].settings));
}

namespace
{
using volum::content::MidiSoundAssignment;

// A pedalboard with the Factory Sound of amp 0 on program 3 and a User Sound of
// amp 2 on program 12.
volum::content::Registry TwoSwitchBoard()
{
  volum::content::Registry reg;
  REQUIRE(volum::content::AssignMidiSound(reg, 3, "factory:0", "factory:0:v1"));
  REQUIRE(volum::content::AssignMidiSound(reg, 12, "factory:2", "preset_b"));
  return reg;
}

// _VolumPromptSaveAs's commit without the dialog: decide from the LIVE switch as
// it is at commit and the pair recorded at open, then point it at the new preset.
void CommitSave(volum::content::Registry& reg, volum::SaveOrigin origin, volum::UiMode modeAtStart, int liveSlot,
                const MidiSoundAssignment& editSource, const std::string& savedOwner, const std::string& savedId)
{
  if (volum::SaveRetargetsLiveSlot(origin, modeAtStart, volum::content::MidiSoundAtSlot(reg, liveSlot), editSource))
    volum::content::AssignMidiSound(reg, liveSlot, savedOwner, savedId);
}
} // namespace

TEST_CASE("A save started in BUILD leaves every PLAY switch as it was")
{
  // The LIVE marker stays set through BUILD, so it alone cannot license a write.
  auto reg = TwoSwitchBoard();
  const auto before = reg.midiSoundMap;
  const int live = 3;

  SUBCASE("Ctrl+S on another amp")
  {
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Build, live, {"factory:5", "preset_x"}, "factory:5",
               "preset_new");
  }
  SUBCASE("Save As after recalling the preset that sits on the LIVE switch")
  {
    // BUILD recall of an assigned preset moves the LIVE marker onto its switch
    // (_VolumSyncLivePlaySlotFromActivePair), so the pair matches exactly.
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Build, live, {"factory:0", "factory:0:v1"}, "factory:0",
               "preset_new");
  }
  CHECK(reg.midiSoundMap.size() == before.size());
  for (const auto& [slot, sound] : before)
  {
    const auto* now = volum::content::MidiSoundAtSlot(reg, slot);
    REQUIRE(now != nullptr);
    CHECK(now->ampId == sound.ampId);
    CHECK(now->presetId == sound.presetId);
  }
}

TEST_CASE("PLAY Ctrl+S moves the LIVE switch only while it holds the Sound the edit started from")
{
  auto reg = TwoSwitchBoard();
  const int live = 3;
  const MidiSoundAssignment factoryOnLive{"factory:0", "factory:0:v1"};

  SUBCASE("a tweaked Factory Sound on its LIVE switch moves that switch")
  {
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    REQUIRE(volum::content::MidiSoundAtSlot(reg, live) != nullptr);
    CHECK(volum::content::MidiSoundAtSlot(reg, live)->presetId == "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, 12)->presetId == "preset_b");
  }
  SUBCASE("the LIVE switch was reassigned to another Sound since the edit began")
  {
    REQUIRE(volum::content::AssignMidiSound(reg, live, "factory:7", "preset_other"));
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, live)->presetId == "preset_other");
  }
  SUBCASE("the LIVE switch was cleared")
  {
    REQUIRE(volum::content::ClearMidiSound(reg, live));
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, live) == nullptr);
  }
  SUBCASE("Add this sound adds a switch of its own")
  {
    CommitSave(reg, volum::SaveOrigin::AddSound, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, live)->presetId == "factory:0:v1");
  }
  SUBCASE("the Default sound has no preset id to match")
  {
    CHECK_FALSE(volum::SaveRetargetsLiveSlot(
      volum::SaveOrigin::Shortcut, volum::UiMode::Play, volum::content::MidiSoundAtSlot(reg, live), {"factory:0", ""}));
  }
}

TEST_CASE("Healed factory snapshot stamps postValid the way apply does")
{
  const auto factory = volum::DefaultFactoryPresets();
  const auto healed = volum::HealedFactoryPresetSettings(factory[2].settings);
  REQUIRE(healed.postValid);
  REQUIRE_FALSE(factory[2].settings.postValid);

  volum::VoLumAmpSettings live = factory[2].settings;
  live.postValid = true;
  REQUIRE(volum::AmpSettingsEqual(live, healed));
}
