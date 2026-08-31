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
  volum::AssignMidiSound(registry.midiSoundMap, {5, "factory:7", "preset_lead"});
  volum::AssignMidiSound(registry.midiSoundMap, {6, "factory:7", "factory:7:v1"});
  volum::AssignMidiSound(registry.midiSoundMap, {5, "factory:2", "preset_clean"});

  REQUIRE(registry.midiSoundMap.size() == 2);
  CHECK(registry.midiSoundMap[0].slot == 5);
  CHECK(registry.midiSoundMap[0].ampId == "factory:2");
  CHECK(volum::ClearMidiSound(registry.midiSoundMap, 6));
  CHECK_FALSE(volum::ClearMidiSound(registry.midiSoundMap, 6));

  const Registry loaded = RegistryFromJson(RegistryToJson(registry));
  REQUIRE(loaded.midiSoundMap.size() == 1);
  CHECK(loaded.midiSoundMap[0].slot == 5);
  CHECK(loaded.midiSoundMap[0].presetId == "preset_clean");
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
  volum::AssignMidiSound(registry.midiSoundMap, {12, "factory:7", factoryPreset.id});

  volum::custom::CustomAmp customAmp;
  customAmp.id = "amp_custom";
  customAmp.name = "Custom";
  registry.amps.push_back(customAmp);
  Preset customPreset;
  customPreset.id = "preset_custom";
  customPreset.name = "Clean";
  customPreset.settings.outputLevel = -4.5;
  registry.presetBanks[customAmp.id] = {customPreset};
  volum::AssignMidiSound(registry.midiSoundMap, {13, customAmp.id, customPreset.id});

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
  volum::AssignMidiSound(registry.midiSoundMap, {2, "factory:99", "preset_missing_amp"});
  volum::AssignMidiSound(registry.midiSoundMap, {3, "factory:2", "preset_missing"});
  volum::AssignMidiSound(registry.midiSoundMap, {4, "amp_deleted", "preset_deleted"});

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

TEST_CASE("Settings MIDI chrome exposes the channel and marks invalid assignments red")
{
  const auto root = RepoRoot() / "NeuralAmpModeler";
  const std::string controls = ReadText(root / "NeuralAmpModelerControls.h");
  const std::string overlay = ReadText(root / "VoLumSettingsOverlay.h");
  const std::string settings = ReadText(root / "VoLumSettingsScene.inc.cpp");

  CHECK(controls.find("VoLumMidiSettingsControl") != std::string::npos);
  CHECK(controls.find("SetMidiCallbacks") != std::string::npos);
  CHECK(overlay.find("mChannel == 0 ?") != std::string::npos);
  CHECK(overlay.find("Omni") != std::string::npos);
  CHECK(overlay.find("row.valid ? VoLumColors::TEXT_MED : IColor(255, 224, 88, 88)") != std::string::npos);
  CHECK(settings.find("j[\"midiCh\"] = mVolumMidiChannel.load()") != std::string::npos);
  CHECK(settings.find("midiSoundMap") == std::string::npos);
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
