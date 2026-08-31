#include "third_party/doctest.h"

#include "../VoLumContentStore.h"
#include "../VoLumChunkIdTail.h"
#include "../VoLumMidi.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}
} // namespace

TEST_CASE("MIDI latest-wins handoff drains only the newest slot")
{
  volum::MidiLatestWinsQueue queue;
  CHECK_FALSE(queue.Drain().has_value());
  queue.Enqueue(4);
  queue.Enqueue(91);
  REQUIRE(queue.Drain().has_value());

  queue.Enqueue(4);
  queue.Enqueue(91);
  const auto newest = queue.Drain();
  REQUIRE(newest.has_value());
  CHECK(*newest == 91);
  CHECK_FALSE(queue.Drain().has_value());
}

TEST_CASE("MIDI sound map assigns reassigns clears and round-trips")
{
  using namespace volum::content;
  Registry registry;
  REQUIRE(AssignMidiSound(registry, 5, "factory:7", "preset_lead"));
  REQUIRE(AssignMidiSound(registry, 6, "factory:7", "factory:7:v1"));
  REQUIRE(AssignMidiSound(registry, 5, "factory:2", "preset_clean"));

  REQUIRE(registry.midiSoundMap.size() == 2);
  REQUIRE(registry.midiSoundMap.count(5) == 1);
  CHECK(registry.midiSoundMap.at(5).ampId == "factory:2"); // reassigned in place
  CHECK(ClearMidiSound(registry, 6));
  CHECK_FALSE(ClearMidiSound(registry, 6));

  const Registry loaded = RegistryFromJson(RegistryToJson(registry));
  REQUIRE(loaded.midiSoundMap.size() == 1);
  REQUIRE(loaded.midiSoundMap.count(5) == 1);
  CHECK(loaded.midiSoundMap.at(5).presetId == "preset_clean");
}

TEST_CASE("Headless MIDI resolution applies assigned factory and custom User presets")
{
  using namespace volum::content;
  Registry registry;

  Preset factoryPreset;
  factoryPreset.id = "preset_factory_owner";
  factoryPreset.name = "Lead";
  factoryPreset.settings.toneBass = 8.25;
  registry.presetBanks["factory:7"] = {factoryPreset};
  REQUIRE(AssignMidiSound(registry, 12, "factory:7", factoryPreset.id));

  volum::custom::CustomAmp customAmp;
  customAmp.id = "amp_custom";
  customAmp.name = "Custom";
  registry.amps.push_back(customAmp);
  Preset customPreset;
  customPreset.id = "preset_custom";
  customPreset.name = "Clean";
  customPreset.settings.outputLevel = -4.5;
  registry.presetBanks[customAmp.id] = {customPreset};
  REQUIRE(AssignMidiSound(registry, 13, customAmp.id, customPreset.id));

  const auto factory = ResolveMidiSound(registry, 12);
  REQUIRE(factory.has_value());
  CHECK(factory->ampId == "factory:7");
  CHECK(factory->presetId == factoryPreset.id);
  CHECK(factory->settings.toneBass == doctest::Approx(8.25));

  const auto custom = ResolveMidiSound(registry, 13);
  REQUIRE(custom.has_value());
  CHECK(custom->ampId == customAmp.id);
  CHECK(custom->settings.outputLevel == doctest::Approx(-4.5));
}

TEST_CASE("Headless MIDI resolution ignores unassigned missing invalid and out-of-range Sounds")
{
  using namespace volum::content;
  Registry registry;
  REQUIRE(AssignMidiSound(registry, 2, "factory:99", "preset_missing_amp"));
  REQUIRE(AssignMidiSound(registry, 3, "factory:2", "preset_missing"));
  REQUIRE(AssignMidiSound(registry, 4, "amp_deleted", "preset_deleted"));

  CHECK_FALSE(ResolveMidiSound(registry, -1).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 128).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 1).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 2).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 3).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 4).has_value());
}

TEST_CASE("MIDI channel id tail defaults to Omni and clamps to 1 through 16")
{
  volum::ChunkIdTail tail;
  CHECK(tail.midiCh == 0);
  tail.midiCh = 12;
  CHECK(volum::IdTailFromJson(volum::IdTailToJson(tail)).midiCh == 12);
  CHECK(volum::IdTailFromJson(nlohmann::json::object()).midiCh == 0);
  CHECK(volum::IdTailFromJson({{"midiCh", 99}}).midiCh == 16);
  CHECK(volum::IdTailFromJson({{"midiCh", -2}}).midiCh == 0);
}

TEST_CASE("Settings MIDI chrome owns the channel and the assignment list, but stores neither")
{
  // The channel is per instance and rides the DAW id tail. The assignment list is
  // machine global and lives in the content registry. Both are edited on the MIDI
  // tab, and the tab must persist neither itself: a Settings-local copy of the
  // sound map is how PLAY and Settings would start disagreeing.
  const auto root = RepoRoot() / "NeuralAmpModeler";
  const std::string controls = ReadText(root / "NeuralAmpModelerControls.h");
  const std::string tabs = ReadText(root / "VoLumSettingsTabs.h");
  const std::string settings = ReadText(root / "VoLumSettingsScene.inc.cpp");

  CHECK(tabs.find("class VoLumMidiChannelControl") != std::string::npos);
  CHECK(tabs.find("mChannel == 0 ?") != std::string::npos);
  CHECK(tabs.find("Omni") != std::string::npos);
  CHECK(tabs.find("class VoLumMidiSoundMapControl") != std::string::npos);
  CHECK(controls.find("SetMidiCallbacks") != std::string::npos);
  CHECK(controls.find("SetMidiSoundMapCallbacks") != std::string::npos);
  CHECK(controls.find("void SetMidiChannel(int channel)") != std::string::npos);
  CHECK(controls.find("void SetMidiSoundMap(") != std::string::npos);
  // The pre-1.3.0 duplicate-list control stays gone.
  CHECK(controls.find("VoLumMidiSettingsControl") == std::string::npos);
  // The channel persists per instance; the sound map does not ride this document.
  CHECK(settings.find("j[\"midiCh\"] = mVolumMidiChannel.load()") != std::string::npos);
  CHECK(settings.find("midiSoundMap") == std::string::npos);
  // The tab reads the registry's map; it never assigns into it directly.
  CHECK(tabs.find("midiSoundMap =") == std::string::npos);
}

TEST_CASE("ProcessMidiMsg is an integer-only RT handoff")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const std::string beginNeedle = "void NeuralAmpModeler::ProcessMidiMsg";
  const auto begin = source.find(beginNeedle);
  REQUIRE(begin != std::string::npos);
  const auto end = source.find("void NeuralAmpModeler::OnIdle", begin);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(begin, end - begin);

  CHECK(body.find("DecodeMidiProgramChange") != std::string::npos);
  CHECK(body.find("mVolumMidiQueue.Enqueue") != std::string::npos);
  CHECK(body.find("GlobalContentStore") == std::string::npos);
  CHECK(body.find("filesystem") == std::string::npos);
  CHECK(body.find("GetUI") == std::string::npos);
}
