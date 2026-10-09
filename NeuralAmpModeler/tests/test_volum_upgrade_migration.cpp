#include "third_party/doctest.h"
#include "VoLumTestTempDir.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../../AudioDSPTools/dsp/wav.h"
#include "../VoLumChunkIdTail.h"
#include "../VoLumContentStore.h"
#include "../VoLumUpgradeMigration.h"
#include "../VoLumUserSettingsIO.h"

// Pins the 1.2.0 -> 1.2.1 in-place upgrade: real user data written by 1.2.0 must
// load under 1.2.1 without loss, healing, or a silent rewrite that cannot be
// undone. The format audit found no breaking change, so these are regression pins
// that keep it that way rather than tests for a known bug.

using namespace volum::content;

namespace
{
std::filesystem::path UpgradeTestBase(const char* name)
{
  auto root = volum_test::ProcessTempRoot() / "volum-upgrade-migration-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}

void WriteFile(const std::filesystem::path& p, const std::string& body)
{
  std::ofstream out(p, std::ios::binary);
  out << body;
  out.close();
}

std::string ReadFile(const std::filesystem::path& p)
{
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// A volum-content.json exactly as VoLum 1.2.0 wrote it: schema v2, and IR entries
// with no trimDb / lowCutHz / highCutHz keys at all.
const char* kSchemaV2Content = R"({
  "schemaVersion": 2,
  "nextPedalIndex": 65,
  "customAmps": [
    {
      "id": "amp_legacy",
      "name": "Legacy Custom",
      "cabNames": ["G12", "V30", "CB3"],
      "art": 2,
      "files": [
        {"file": "AMP-Legacy-1.nam", "slot": -1, "channel": 1, "storedPath": "amps/amp_legacy__1.nam"},
        {"file": "AMP-Legacy-5.nam", "slot": -1, "channel": 5, "storedPath": "amps/amp_legacy__5.nam"}
      ]
    }
  ],
  "irLibrary": [
    {"id": "ir_legacy", "name": "Legacy Cab", "path": "ir/ir_legacy__cab.wav"}
  ],
  "customPedals": [
    {"id": "pedal_legacy", "name": "Legacy Klon", "group": "klon",
     "path": "pedals/pedal_legacy.nam", "legacyIndex": 64}
  ]
})";
} // namespace

TEST_CASE("Schema v2 content from 1.2.0 loads cleanly under 1.2.1")
{
  const auto base = UpgradeTestBase("v2-load");
  ContentStore store;
  store.SetBaseDir(base);
  WriteFile(store.RegistryPath(), kSchemaV2Content);

  // A clean load: no heal, no move to .bak.
  REQUIRE(store.Load() == true);
  CHECK(std::filesystem::exists(store.RegistryPath()));
  CHECK_FALSE(std::filesystem::exists(store.BackupPath()));

  const auto& r = store.reg();
  REQUIRE(r.amps.size() == 1);
  CHECK(r.amps[0].id == "amp_legacy");
  CHECK(r.amps[0].name == "Legacy Custom");
  REQUIRE(r.amps[0].files.size() == 2);
  // The channel-5 capture that the reported bug lost must survive verbatim.
  CHECK(r.amps[0].files[1].channel == 5);
  CHECK(r.amps[0].files[1].storedPath == "amps/amp_legacy__5.nam");
  CHECK(r.amps[0].files[1].slot == volum::custom::kDirectSlot);

  REQUIRE(r.irs.size() == 1);
  CHECK(r.irs[0].id == "ir_legacy");
  CHECK(r.irs[0].file == "ir/ir_legacy__cab.wav");

  REQUIRE(r.pedals.size() == 1);
  CHECK(r.pedals[0].legacyIndex == 64);
  CHECK(r.nextPedalIndex == 65);
}

TEST_CASE("A v2 IR without trimDb is left uncalibrated so 1.2.1 normalizes it once")
{
  const auto base = UpgradeTestBase("v2-uncalibrated");
  ContentStore store;
  store.SetBaseDir(base);
  WriteFile(store.RegistryPath(), kSchemaV2Content);
  REQUIRE(store.Load() == true);

  REQUIRE(store.reg().irs.size() == 1);
  const auto& ir = store.reg().irs[0];
  // No trimDb key -> not yet calibrated, and inert until the migration runs.
  CHECK(ir.trimCalibrated == false);
  CHECK(ir.trimDb == doctest::Approx(0.0));
  CHECK(ir.lowCutHz == doctest::Approx(0.0));
  CHECK(ir.highCutHz == doctest::Approx(0.0));
}

TEST_CASE("A v3 IR with shaping is read back as already calibrated")
{
  const auto base = UpgradeTestBase("v3-calibrated");
  ContentStore store;
  store.SetBaseDir(base);
  WriteFile(store.RegistryPath(), R"({
    "schemaVersion": 3,
    "irLibrary": [
      {"id": "ir_shaped", "name": "Shaped", "path": "ir/x.wav",
       "trimDb": 6.5, "lowCutHz": 80.0, "highCutHz": 8000.0}
    ]
  })");
  REQUIRE(store.Load() == true);

  REQUIRE(store.reg().irs.size() == 1);
  const auto& ir = store.reg().irs[0];
  CHECK(ir.trimCalibrated == true); // presence of trimDb marks it done
  CHECK(ir.trimDb == doctest::Approx(6.5));
  CHECK(ir.lowCutHz == doctest::Approx(80.0));
  CHECK(ir.highCutHz == doctest::Approx(8000.0));
}

TEST_CASE("Upgrading a v2 library to v3 preserves every non-IR field verbatim")
{
  // The migration only adds IR shaping. Everything else must survive the rewrite,
  // because this is the step a user cannot undo by reinstalling 1.2.0.
  const auto base = UpgradeTestBase("v2-to-v3-roundtrip");
  ContentStore store;
  store.SetBaseDir(base);
  WriteFile(store.RegistryPath(), kSchemaV2Content);
  REQUIRE(store.Load() == true);

  REQUIRE(store.Save() == true);

  ContentStore reread;
  reread.SetBaseDir(base);
  REQUIRE(reread.Load() == true);
  const auto& r = reread.reg();

  REQUIRE(r.amps.size() == 1);
  CHECK(r.amps[0].name == "Legacy Custom");
  CHECK(r.amps[0].art == 2);
  CHECK(r.amps[0].cabNames[0] == "G12");
  CHECK(r.amps[0].cabNames[1] == "V30");
  REQUIRE(r.amps[0].files.size() == 2);
  CHECK(r.amps[0].files[1].channel == 5);
  REQUIRE(r.pedals.size() == 1);
  CHECK(r.pedals[0].name == "Legacy Klon");
  CHECK(r.pedals[0].legacyIndex == 64);
  CHECK(r.nextPedalIndex == 65);
}

// ---- one-time pre-migration backup -----------------------------------------

TEST_CASE("BackupBeforeMigration snapshots the pre-upgrade library")
{
  const auto base = UpgradeTestBase("migration-backup");
  ContentStore store;
  store.SetBaseDir(base);
  WriteFile(store.RegistryPath(), kSchemaV2Content);

  REQUIRE(store.BackupBeforeMigration("1.2.1") == true);
  const auto snap = store.MigrationBackupPath("1.2.1");
  REQUIRE(std::filesystem::exists(snap));
  CHECK(ReadFile(snap) == std::string(kSchemaV2Content));
  // The original is copied, not moved.
  CHECK(std::filesystem::exists(store.RegistryPath()));
}

TEST_CASE("BackupBeforeMigration never overwrites an existing snapshot")
{
  // Run twice: the snapshot must keep the ORIGINAL pre-upgrade bytes, otherwise a
  // second launch would overwrite the backup with already-migrated content and
  // the user's real pre-upgrade state would be gone.
  const auto base = UpgradeTestBase("migration-backup-idempotent");
  ContentStore store;
  store.SetBaseDir(base);
  WriteFile(store.RegistryPath(), kSchemaV2Content);
  REQUIRE(store.BackupBeforeMigration("1.2.1") == true);

  WriteFile(store.RegistryPath(), R"({"schemaVersion": 3, "irLibrary": []})");
  REQUIRE(store.BackupBeforeMigration("1.2.1") == true);

  CHECK(ReadFile(store.MigrationBackupPath("1.2.1")) == std::string(kSchemaV2Content));
}

TEST_CASE("BackupBeforeMigration is a no-op when there is nothing to snapshot")
{
  const auto base = UpgradeTestBase("migration-backup-missing");
  ContentStore store;
  store.SetBaseDir(base);
  CHECK(store.BackupBeforeMigration("1.2.1") == false);
  CHECK_FALSE(std::filesystem::exists(store.MigrationBackupPath("1.2.1")));

  ContentStore inMemory; // no base dir configured (unit-test / unconfigured store)
  CHECK(inMemory.BackupBeforeMigration("1.2.1") == false);
}

TEST_CASE("The migration snapshot is kept apart from the corrupt-file backup")
{
  // A later parse failure moves the registry to .bak. That must not clobber the
  // pre-upgrade snapshot, which is the only copy of the user's 1.2.0 library.
  const auto base = UpgradeTestBase("migration-backup-distinct");
  ContentStore store;
  store.SetBaseDir(base);
  CHECK(store.MigrationBackupPath("1.2.1") != store.BackupPath());

  WriteFile(store.RegistryPath(), kSchemaV2Content);
  REQUIRE(store.BackupBeforeMigration("1.2.1") == true);

  WriteFile(store.RegistryPath(), "{ this is not json");
  CHECK(store.Load() == false); // corrupt -> moved to .bak, defaults restored
  CHECK(std::filesystem::exists(store.BackupPath()));
  CHECK(ReadFile(store.MigrationBackupPath("1.2.1")) == std::string(kSchemaV2Content));
}

// ---- volum-settings.json written by 1.2.0 ----------------------------------

TEST_CASE("A 1.2.0-shaped volum-settings.json loads under 1.2.1 without healing")
{
  // Version 6 with the keys 1.2.0 wrote. Nothing here may trigger the destructive
  // legacy-v1 migration path or report a heal.
  const auto j = nlohmann::json::parse(R"({
    "version": 6,
    "lastAmpIdx": 3,
    "preLocked": false,
    "postLocked": true,
    "amps": {
      "Ampete": {
        "speakerIdx": 0,
        "channelIdx": 1,
        "gain": 6.5,
        "bass": 5.5,
        "middle": 4.5,
        "treble": 7.5,
        "output": -3.0,
        "noiseGate": true,
        "eq": true
      }
    }
  })");

  volum::VoLumAmpSettings amps[volum::kAmpCount];
  int lastAmpIdx = 0;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, &lastAmpIdx, nullptr, &healed);

  CHECK(healed == false);
  CHECK(lastAmpIdx == 3);
}

TEST_CASE("A settings file from a newer build loads without destroying its data")
{
  // Forward tolerance guard: an A/B downgrade must not read a future version as a
  // corrupt v1 file and wipe the user's per-amp tweaks. This is the exact bug the
  // 1.0.1 RC hit by bumping the version for two additive booleans.
  auto j = nlohmann::json::parse(R"({
    "version": 99,
    "lastAmpIdx": 2,
    "someUnknownFutureKey": {"nested": true},
    "amps": {
      "Ampete": {"speakerIdx": 2, "channelIdx": 1, "gain": 8.25, "futureKnob": 1.0}
    }
  })");

  volum::VoLumAmpSettings amps[volum::kAmpCount];
  int lastAmpIdx = 0;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, &lastAmpIdx, nullptr, &healed);

  CHECK(healed == false);
  CHECK(lastAmpIdx == 2);
}

// ---- 1.2.x custom scenes -> 1.3.0 per-instance scenes ----------------------

namespace
{
// A volum-content.json as VoLum 1.2.3 wrote it: schema v3, three custom amps, and
// each amp's live knobs in the shared "customScenes" map.
const char* kLibrary123 = R"({
  "schemaVersion": 3,
  "nextPedalIndex": 64,
  "customAmps": [
    {"id": "amp_a", "name": "Amp A", "cabNames": ["", "", ""], "art": 0,
     "files": [{"file": "A-1.nam", "slot": -1, "channel": 1, "storedPath": "amps/amp_a__1.nam"}]},
    {"id": "amp_b", "name": "Amp B", "cabNames": ["", "", ""], "art": 1,
     "files": [{"file": "B-1.nam", "slot": -1, "channel": 1, "storedPath": "amps/amp_b__1.nam"}]},
    {"id": "amp_c", "name": "Amp C", "cabNames": ["", "", ""], "art": 2,
     "files": [{"file": "C-1.nam", "slot": -1, "channel": 1, "storedPath": "amps/amp_c__1.nam"}]}
  ],
  "irLibrary": [{"id": "ir_cab", "name": "Cab", "path": "ir/ir_cab.wav", "trimDb": 4.5, "lowCutHz": 0.0,
                 "highCutHz": 0.0}],
  "customPedals": [],
  "presetBanks": {},
  "customScenes": {
    "amp_a": {"bass": 6.5, "treble": 3.5, "output": -3.0},
    "amp_b": {"bass": 2.0, "treble": 8.0, "output": -7.5},
    "amp_c": {"bass": 9.0, "treble": 1.5, "output": 1.5}
  }
})";

// The id tail of a project VoLum 1.2.3 saved with amp_a focused: the custom amp is
// referenced by id, and its knobs are in the library, not here.
nlohmann::json ProjectTail123()
{
  return nlohmann::json::parse(R"({"v": 5, "customMainId": "amp_a", "customSupportId": "", "activePresetId": ""})");
}

// One VoLum instance's life against the library on disk, through the seams the
// plugin uses: EnsureLoaded in the constructor, the project's id tail installed the
// way UnserializeState does it, InstanceCustomScene on focus (_VolumCustomScene),
// and ContentStore::Save from the flush paths (settings idle flush, SerializeState,
// the plugin destructor).
struct Instance
{
  volum::content::ContentStore store;
  std::map<std::string, volum::VoLumAmpSettings> scenes;

  Instance(const std::filesystem::path& base, const nlohmann::json& projectTail)
  : store(base)
  {
    store.EnsureLoaded();
    const auto tail = volum::IdTailFromJson(projectTail);
    if (!tail.customScenes.empty())
      scenes = tail.customScenes;
  }

  volum::VoLumAmpSettings& Focus(const std::string& ampId)
  {
    return volum::content::InstanceCustomScene(scenes, store.reg().legacyCustomScenes, ampId);
  }

  // What SerializeState writes when the host saves the project.
  nlohmann::json SaveProject(const std::string& focusedId) const
  {
    volum::ChunkIdTail tail;
    tail.customMainId = focusedId;
    tail.customScenes = scenes;
    return volum::IdTailToJson(tail);
  }
};

void CheckScene(const volum::VoLumAmpSettings& s, double bass, double treble, double output)
{
  CHECK(s.toneBass == doctest::Approx(bass));
  CHECK(s.toneTreble == doctest::Approx(treble));
  CHECK(s.outputLevel == doctest::Approx(output));
}

nlohmann::json LibraryOnDisk(const volum::content::ContentStore& store)
{
  return nlohmann::json::parse(ReadFile(store.RegistryPath()));
}

void WriteLE16(std::ofstream& f, std::uint16_t v)
{
  const char b[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
  f.write(b, 2);
}

void WriteLE32(std::ofstream& f, std::uint32_t v)
{
  const char b[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                     static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
  f.write(b, 4);
}

// A mono 16-bit PCM impulse response: one full-scale sample, then silence.
void WriteImpulseWav(const std::filesystem::path& path)
{
  std::filesystem::create_directories(path.parent_path());
  const std::vector<std::int16_t> samples = {32767, 0, 0, 0, 0, 0, 0, 0};
  const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * 2);
  std::ofstream f(path, std::ios::binary);
  f.write("RIFF", 4);
  WriteLE32(f, 36 + dataBytes);
  f.write("WAVEfmt ", 8);
  WriteLE32(f, 16);
  WriteLE16(f, 1); // PCM
  WriteLE16(f, 1); // mono
  WriteLE32(f, 48000);
  WriteLE32(f, 48000 * 2);
  WriteLE16(f, 2);
  WriteLE16(f, 16);
  f.write("data", 4);
  WriteLE32(f, dataBytes);
  for (std::int16_t s : samples)
    WriteLE16(f, static_cast<std::uint16_t>(s));
}

// The reader _VolumMigrateIrTrims hands the migration: the real .wav loader.
bool MeasureWav(const std::filesystem::path& file, double& l2)
{
  std::vector<float> audio;
  double sampleRate = 0.0;
  if (dsp::wav::Load(file.string().c_str(), audio, sampleRate) != dsp::wav::LoadReturnCode::SUCCESS || audio.empty())
    return false;
  double sumSq = 0.0;
  for (float v : audio)
    sumSq += static_cast<double>(v) * static_cast<double>(v);
  l2 = std::sqrt(sumSq);
  return true;
}

const volum::content::IRItem& IrById(const volum::content::ContentStore& store, const std::string& id)
{
  for (const auto& ir : store.reg().irs)
    if (ir.id == id)
      return ir;
  FAIL("IR " << id << " missing");
  return store.reg().irs.front();
}
} // namespace

TEST_CASE("1.2.3 upgrade: a custom scene survives opening an old project and quitting without saving")
{
  const auto base = UpgradeTestBase("scene-open-no-save");
  {
    ContentStore seed;
    seed.SetBaseDir(base);
    WriteFile(seed.RegistryPath(), kLibrary123);
  }

  bool catalogEdit = false;
  SUBCASE("nothing else changes in the session") {}
  SUBCASE("a preset is saved during the session")
  {
    catalogEdit = true;
  }
  CAPTURE(catalogEdit);

  {
    Instance daw(base, ProjectTail123());
    auto& scene = daw.Focus("amp_a");
    CheckScene(scene, 6.5, 3.5, -3.0);
    scene.toneBass = 1.0; // the player tweaks, then closes the DAW without saving
    if (catalogEdit)
      daw.store.reg().presetBanks["factory:0"].push_back(Preset{"preset_new", "New", {}});
    REQUIRE(daw.store.Save()); // the destructor's flush
  }

  Instance reopened(base, ProjectTail123());
  CheckScene(reopened.Focus("amp_a"), 6.5, 3.5, -3.0);
  CHECK(LibraryOnDisk(reopened.store)["customScenes"].size() == 3);
}

TEST_CASE("1.2.3 upgrade: the first save keeps the scenes of every custom amp the session never focused")
{
  const auto base = UpgradeTestBase("scene-first-save");
  {
    ContentStore seed;
    seed.SetBaseDir(base);
    WriteFile(seed.RegistryPath(), kLibrary123);
  }

  nlohmann::json savedProject;
  {
    Instance daw(base, ProjectTail123());
    auto& a = daw.Focus("amp_a");
    a.toneBass = 9.5; // a real 1.3.0 edit, carried by the project from now on
    daw.store.reg().presetBanks["amp_a"].push_back(Preset{"preset_a", "A Lead", a});
    REQUIRE(daw.store.Save()); // the first library write after the upgrade
    savedProject = daw.SaveProject("amp_a");
  }

  // Only amp_a ever reached the project, so amp_b and amp_c exist nowhere but the
  // library's migration source.
  CHECK(volum::IdTailFromJson(savedProject).customScenes.size() == 1);

  Instance reopened(base, savedProject);
  CheckScene(reopened.Focus("amp_a"), 9.5, 3.5, -3.0); // the project's copy wins
  CheckScene(reopened.Focus("amp_b"), 2.0, 8.0, -7.5);
  CheckScene(reopened.Focus("amp_c"), 9.0, 1.5, 1.5);

  // A brand-new insert (no project scenes at all) still starts amp_b from 1.2.3.
  Instance fresh(base, nlohmann::json::object());
  CheckScene(fresh.Focus("amp_b"), 2.0, 8.0, -7.5);
}

TEST_CASE("1.2.3 upgrade: a sibling's save keeps the migration source and deleting an amp drops its entry")
{
  const auto base = UpgradeTestBase("scene-two-writers");
  {
    ContentStore seed;
    seed.SetBaseDir(base);
    WriteFile(seed.RegistryPath(), kLibrary123);
  }

  Instance standalone(base, nlohmann::json::object());
  Instance daw(base, ProjectTail123());
  standalone.Focus("amp_b");
  daw.Focus("amp_a");

  standalone.store.RemoveCustomAmp("amp_c");
  REQUIRE(standalone.store.Save());
  daw.store.reg().presetBanks["amp_a"].push_back(Preset{"preset_a", "A Lead", daw.Focus("amp_a")});
  REQUIRE(daw.store.Save());

  const auto scenes = LibraryOnDisk(daw.store)["customScenes"];
  CHECK(scenes.contains("amp_a"));
  CHECK(scenes.contains("amp_b"));
  CHECK_FALSE(scenes.contains("amp_c")); // the user deleted that amp
  ContentStore relaunch(base);
  REQUIRE(relaunch.Load());
  REQUIRE(relaunch.reg().legacyCustomScenes.count("amp_b") == 1);
  CheckScene(relaunch.reg().legacyCustomScenes.at("amp_b"), 2.0, 8.0, -7.5);
}

TEST_CASE("1.2.x upgrade: an IR unreadable during the trim migration is measured once it can be read")
{
  const auto base = UpgradeTestBase("trim-unreadable");
  ContentStore store;
  store.SetBaseDir(base);
  WriteFile(store.RegistryPath(), R"({
    "schemaVersion": 2,
    "irLibrary": [
      {"id": "ir_here", "name": "Here", "path": "ir/ir_here.wav"},
      {"id": "ir_late", "name": "Late", "path": "ir/ir_late.wav"}
    ]
  })");
  WriteImpulseWav(base / "ir" / "ir_here.wav");
  const double impulseTrim = AutoNormalizeIrTrimDb(32767.0 / 32768.0);
  REQUIRE(impulseTrim > 6.0); // a real correction, nowhere near the 0 dB fallback
  REQUIRE(store.Load());

  const auto first = MigrateIrTrims(store, MeasureWav);
  CHECK(first.calibrated.size() == 1);
  CHECK(first.pendingNames == std::vector<std::string>{"Late"});
  CHECK(IrById(store, "ir_here").trimDb == doctest::Approx(impulseTrim).epsilon(1e-3));
  CHECK_FALSE(IrById(store, "ir_late").trimCalibrated);

  // A later write in the same session must not turn the retry marker into 0 dB.
  store.reg().presetBanks["factory:0"].push_back(Preset{"preset_x", "X", {}});
  REQUIRE(store.Save());
  CHECK_FALSE(LibraryOnDisk(store)["irLibrary"][1].contains("trimDb"));

  // Next launch, the file is back (sync finished, drive mounted).
  WriteImpulseWav(base / "ir" / "ir_late.wav");
  ContentStore relaunch;
  relaunch.SetBaseDir(base);
  REQUIRE(relaunch.Load());
  CHECK_FALSE(IrById(relaunch, "ir_late").trimCalibrated);
  const auto second = MigrateIrTrims(relaunch, MeasureWav);
  CHECK(second.calibrated.size() == 1);
  CHECK(second.pendingNames.empty());

  ContentStore third;
  third.SetBaseDir(base);
  REQUIRE(third.Load());
  CHECK(IrById(third, "ir_late").trimCalibrated);
  CHECK(IrById(third, "ir_late").trimDb == doctest::Approx(impulseTrim).epsilon(1e-3));
  CHECK(IrById(third, "ir_here").trimDb == doctest::Approx(impulseTrim).epsilon(1e-3));
}
