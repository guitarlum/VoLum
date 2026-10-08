#include "third_party/doctest.h"

#include "../VoLumContentStore.h"
#include "../VoLumPlayModel.h"
#include "volum_factory_bank.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

using volum::content::ContentStore;
using volum::content::MidiSoundAssignment;

namespace
{
std::filesystem::path PrefillBase(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-play-prefill-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}

void WriteLibrary(const ContentStore& store, const std::string& body)
{
  std::ofstream out(store.RegistryPath(), std::ios::binary);
  out << body;
}

std::string ReadLibrary(const ContentStore& store)
{
  std::ifstream in(store.RegistryPath(), std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::map<int, MidiSoundAssignment> ShippedPrefill()
{
  return volum::PlayPrefillAssignments(volum_test::ShippedFactoryPresets());
}

// The owner's board, slot by slot: program -> (owner, preset id).
void CheckPrefilledBoard(const std::map<int, MidiSoundAssignment>& map)
{
  const std::pair<const char*, const char*> expected[] = {
    {"factory:12", "factory:12:v1"}, {"factory:13", "factory:13:v2"}, {"factory:6", "factory:6:v1"},
    {"factory:8", "factory:8:v1"},   {"factory:0", "factory:0:v2"},
  };
  REQUIRE(map.size() == std::size(expected));
  for (int program = 0; program < static_cast<int>(std::size(expected)); ++program)
  {
    CAPTURE(program);
    REQUIRE(map.count(program) == 1);
    CHECK(map.at(program).ampId == expected[program].first);
    CHECK(map.at(program).presetId == expected[program].second);
  }
}

std::map<int, MidiSoundAssignment> MapOnDisk(const std::filesystem::path& base)
{
  ContentStore reread(base);
  REQUIRE(reread.Load());
  CHECK(reread.reg().hasMidiSoundMap);
  return reread.reg().midiSoundMap;
}

// What VoLum 1.2.0 wrote: schema v2, no IR shaping, no midiSoundMap.
const char* kLibraryV2 = R"({
  "schemaVersion": 2,
  "nextPedalIndex": 65,
  "customAmps": [{"id": "amp_legacy", "name": "Legacy Custom", "cabNames": ["G12", "V30", "CB3"], "art": 2,
                  "files": [{"file": "AMP-1.nam", "slot": -1, "channel": 1, "storedPath": "amps/amp_legacy__1.nam"}]}],
  "irLibrary": [{"id": "ir_legacy", "name": "Legacy Cab", "path": "ir/ir_legacy__cab.wav"}],
  "customPedals": [{"id": "pedal_legacy", "name": "Legacy Klon", "group": "klon",
                    "path": "pedals/pedal_legacy.nam", "legacyIndex": 64}]
})";

// What VoLum 1.2.1 wrote: schema v3 with IR shaping and shared scenes, still no midiSoundMap.
const char* kLibraryV3 = R"({
  "schemaVersion": 3,
  "irLibrary": [{"id": "ir_shaped", "name": "Shaped", "path": "ir/x.wav", "trimDb": 6.5, "lowCutHz": 80.0,
                 "highCutHz": 8000.0}],
  "presetBanks": {"factory:3": [{"id": "preset_v3", "name": "Old Lead", "settings": {}}]},
  "customScenes": {}
})";
} // namespace

TEST_CASE("PLAY pre-fill names the shipped Factory presets it assigns")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  for (const auto& sound : volum::kPlayPrefillSounds)
  {
    CAPTURE(sound.presetId);
    const auto* preset = volum::FindFactoryPresetById(bank, sound.presetId);
    REQUIRE(preset != nullptr);
    CHECK(preset->name == sound.name);
  }
  CheckPrefilledBoard(ShippedPrefill());

  volum::content::Registry board;
  board.midiSoundMap = ShippedPrefill();
  const auto slots = volum::BuildPlaySlots(bank, board);
  const char* names[] = {"The bestest Clean", "SLO Crunch", "Modern Rhythm", "Crack the Skye", "Ampete Lead"};
  REQUIRE(slots.size() == std::size(names));
  for (size_t i = 0; i < slots.size(); ++i)
  {
    CAPTURE(i);
    CHECK(slots[i].valid);
    CHECK(slots[i].sound.presetName == names[i]);
  }
}

TEST_CASE("PLAY pre-fill skips a Sound the loaded Factory bank does not hold under its name")
{
  // No shipped file: the fallback bank has every v1 id, named "Factory 1".
  CHECK(volum::PlayPrefillAssignments(volum::FactoryPresetsOrFallback({})).empty());

  auto bank = volum_test::ShippedFactoryPresets();
  volum_test::FindMutableFactoryPreset(bank, "factory:8:v1")->name = "Renamed";
  const auto partial = volum::PlayPrefillAssignments(bank);
  CHECK(partial.size() == 4);
  CHECK(partial.count(3) == 0);
}

TEST_CASE("The reader tells a library without midiSoundMap from one with an empty map")
{
  CHECK_FALSE(volum::content::RegistryFromJson(nlohmann::json::object()).hasMidiSoundMap);
  CHECK_FALSE(volum::content::RegistryFromJson(nlohmann::json::parse(kLibraryV2)).hasMidiSoundMap);
  auto empty = nlohmann::json::parse(kLibraryV3);
  empty["midiSoundMap"] = nlohmann::json::array();
  CHECK(volum::content::RegistryFromJson(empty).hasMidiSoundMap);
  // Every write stores the key, so a saved library never reads as never-filled.
  CHECK(volum::content::RegistryToJson(volum::content::Registry{}).contains("midiSoundMap"));
}

TEST_CASE("A fresh install is pre-filled once and the board is saved")
{
  const auto base = PrefillBase("fresh");
  ContentStore store(base);
  REQUIRE(store.EnsureLoaded());
  REQUIRE_FALSE(std::filesystem::exists(store.RegistryPath()));

  CHECK(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
  CheckPrefilledBoard(store.reg().midiSoundMap);
  CHECK_FALSE(store.HasUnflushedChanges());
  CheckPrefilledBoard(MapOnDisk(base));

  // A second plugin instance in the same process.
  CHECK_FALSE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
  CheckPrefilledBoard(MapOnDisk(base));
}

TEST_CASE("A 1.2.x library is pre-filled and keeps everything it had")
{
  for (const char* fixture : {kLibraryV2, kLibraryV3})
  {
    CAPTURE(fixture);
    const auto base = PrefillBase("upgrade");
    ContentStore store(base);
    WriteLibrary(store, fixture);
    REQUIRE(store.Load());
    REQUIRE_FALSE(store.reg().hasMidiSoundMap);
    const auto before = nlohmann::json::parse(fixture);

    CHECK(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
    CheckPrefilledBoard(MapOnDisk(base));

    ContentStore reread(base);
    REQUIRE(reread.Load());
    CHECK(reread.reg().amps.size() == before.value("customAmps", nlohmann::json::array()).size());
    CHECK(reread.reg().irs.size() == before.value("irLibrary", nlohmann::json::array()).size());
    CHECK(reread.reg().pedals.size() == before.value("customPedals", nlohmann::json::array()).size());
    CHECK(reread.reg().presetBanks.size() == before.value("presetBanks", nlohmann::json::object()).size());

    ContentStore relaunch(base);
    REQUIRE(relaunch.Load());
    CHECK_FALSE(relaunch.PrefillMidiSoundMapOnce(ShippedPrefill()));
  }
}

// The constructor pre-fills before its migrations, and they read the old file: an
// IR without trimDb is the trim migration's retry marker, and a v3 file's
// customScenes are what a 1.2.x project's custom amp restores its knobs from. A DAW
// scan pre-fills too, and never runs those migrations' own saves.
TEST_CASE("Pre-filling a 1.2.x library adds the midiSoundMap key and changes nothing else")
{
  auto v3 = nlohmann::json::parse(kLibraryV3);
  v3["customScenes"] = {{"amp_legacy", nlohmann::json::object()}};
  for (const std::string& fixture : {std::string(kLibraryV2), v3.dump(2)})
  {
    CAPTURE(fixture);
    const auto base = PrefillBase("key-only");
    ContentStore store(base);
    WriteLibrary(store, fixture);
    store.Load();
    REQUIRE_FALSE(store.reg().hasMidiSoundMap);
    REQUIRE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));

    auto after = nlohmann::json::parse(ReadLibrary(store));
    REQUIRE(after.contains("midiSoundMap"));
    after.erase("midiSoundMap");
    CHECK(after == nlohmann::json::parse(fixture));
    CheckPrefilledBoard(MapOnDisk(base));
  }
}

TEST_CASE("Pre-filling a 1.2.0 library leaves the trim migration's snapshot the untouched file")
{
  const auto base = PrefillBase("v2-snapshot");
  ContentStore store(base);
  WriteLibrary(store, kLibraryV2);
  REQUIRE(store.Load());
  REQUIRE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));

  // What _VolumMigrateIrTrims does next, before its own save.
  REQUIRE(store.BackupBeforeMigration("1.2.1"));
  std::ifstream in(store.MigrationBackupPath("1.2.1"), std::ios::binary);
  const std::string snapshot((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(snapshot == kLibraryV2);
}

TEST_CASE("A library that stores midiSoundMap is never pre-filled")
{
  SUBCASE("an empty map")
  {
    const auto base = PrefillBase("existing-empty");
    ContentStore store(base);
    WriteLibrary(store, R"({"schemaVersion": 4, "midiSoundMap": []})");
    const std::string bytes = ReadLibrary(store);
    REQUIRE(store.Load());
    CHECK_FALSE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
    CHECK(store.reg().midiSoundMap.empty());
    CHECK(ReadLibrary(store) == bytes);
  }
  SUBCASE("a player's own board")
  {
    const auto base = PrefillBase("existing-board");
    ContentStore store(base);
    WriteLibrary(store, R"({"schemaVersion": 4,
                            "midiSoundMap": [{"slot": 7, "ampId": "factory:2", "presetId": "factory:2:v1"}]})");
    REQUIRE(store.Load());
    CHECK_FALSE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
    const auto map = MapOnDisk(base);
    REQUIRE(map.size() == 1);
    CHECK(map.at(7).presetId == "factory:2:v1");
  }
}

TEST_CASE("A board cleared after the pre-fill stays cleared")
{
  const auto base = PrefillBase("cleared");
  {
    ContentStore store(base);
    REQUIRE(store.EnsureLoaded());
    REQUIRE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
    for (int program = 0; program < 5; ++program)
      store.ClearMidiSlot(program);
    REQUIRE(store.Save());
    CHECK_FALSE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
  }
  ContentStore relaunch(base);
  REQUIRE(relaunch.Load());
  CHECK(relaunch.reg().hasMidiSoundMap);
  CHECK_FALSE(relaunch.PrefillMidiSoundMapOnce(ShippedPrefill()));
  CHECK(MapOnDisk(base).empty());
}

TEST_CASE("Two VoLums opening one never-filled library fill it once")
{
  // Standalone and a DAW's VST3 both loaded the library before either filled it.
  const auto base = PrefillBase("two-writers");
  ContentStore standalone(base);
  ContentStore daw(base);
  WriteLibrary(standalone, kLibraryV2);
  REQUIRE(standalone.Load());
  REQUIRE(daw.Load());

  REQUIRE(standalone.PrefillMidiSoundMapOnce(ShippedPrefill()));
  // The standalone's player clears a switch before the DAW instance gets to its fill.
  standalone.ClearMidiSlot(2);
  REQUIRE(standalone.Save());

  CHECK_FALSE(daw.PrefillMidiSoundMapOnce(ShippedPrefill()));
  CHECK(daw.reg().midiSoundMap.size() == 4); // shows the board on disk
  CHECK(daw.reg().midiSoundMap.count(2) == 0);
  REQUIRE(daw.Save());

  const auto map = MapOnDisk(base);
  CHECK(map.size() == 4);
  CHECK(map.count(2) == 0);
}

TEST_CASE("Two threads pre-filling one fresh library write one board")
{
  const auto base = PrefillBase("two-threads");
  ContentStore a(base);
  ContentStore b(base);
  REQUIRE(a.EnsureLoaded());
  REQUIRE(b.EnsureLoaded());
  const auto sounds = ShippedPrefill();
  bool filledA = false, filledB = false;
  std::thread ta([&] { filledA = a.PrefillMidiSoundMapOnce(sounds); });
  std::thread tb([&] { filledB = b.PrefillMidiSoundMapOnce(sounds); });
  ta.join();
  tb.join();
  CHECK(filledA != filledB);
  CheckPrefilledBoard(a.reg().midiSoundMap);
  CheckPrefilledBoard(b.reg().midiSoundMap);
  CheckPrefilledBoard(MapOnDisk(base));
}

TEST_CASE("An unreadable library is not pre-filled")
{
  const auto base = PrefillBase("unreadable");
  ContentStore store(base);
  std::filesystem::create_directories(store.RegistryPath()); // a directory where the file belongs
  CHECK_FALSE(store.Load());
  REQUIRE(store.RegistryUnreadable());
  CHECK_FALSE(store.PrefillMidiSoundMapOnce(ShippedPrefill()));
  CHECK(store.reg().midiSoundMap.empty());
  CHECK(std::filesystem::is_directory(store.RegistryPath()));
}

TEST_CASE("The constructor pre-fills before any other library write")
{
  std::ifstream in(
    volum_test::ShippedFactoryPresetsPath().parent_path().parent_path() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp",
    std::ios::binary);
  REQUIRE(in.good());
  const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const auto loaded = source.find("volum::content::GlobalContentStore().EnsureLoaded();");
  const auto prefill = source.find("PrefillMidiSoundMapOnce(", loaded);
  const auto trims = source.find("_VolumMigrateIrTrims();", loaded);
  REQUIRE(loaded != std::string::npos);
  REQUIRE(prefill != std::string::npos);
  REQUIRE(trims != std::string::npos);
  // The trim migration saves, and a save stores midiSoundMap even as [].
  CHECK(prefill < trims);
  CHECK(source.find("volum::PlayPrefillAssignments(mVolumFactoryPresets)", loaded) != std::string::npos);
}
