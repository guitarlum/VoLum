#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "../VoLumContentStore.h"
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
  auto root = std::filesystem::temp_directory_path() / "volum-upgrade-migration-tests" / name;
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
