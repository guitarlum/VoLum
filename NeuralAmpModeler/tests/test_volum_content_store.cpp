#include "third_party/doctest.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include "../VoLumContentStore.h"
#include "../VoLumCustomNamImport.h"

using namespace volum::content;
using volum::VoLumAmpSettings;

namespace
{
std::filesystem::path TestBase(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-content-store-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}

std::filesystem::path WriteSrc(const std::filesystem::path& dir, const char* leaf, const char* body)
{
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const auto p = dir / leaf;
  std::ofstream out(p, std::ios::binary);
  out << body;
  out.close();
  return p;
}
} // namespace

TEST_CASE("MintId returns prefixed, unique opaque ids")
{
  Registry r;
  const std::string a = MintId(r, "amp");
  const std::string b = MintId(r, "ir");
  CHECK(a.rfind("amp_", 0) == 0);
  CHECK(b.rfind("ir_", 0) == 0);
  CHECK(a != b);

  IRItem item;
  item.id = a; // pretend a is taken
  r.irs.push_back(item);
  CHECK(IdInUse(r, a));
  CHECK(MintId(r, "amp") != a);
}

TEST_CASE("Cab names are clamped to the 3-char rule at the load boundary")
{
  // Direct model helper: whitespace dropped, uppercased, capped at 3; an empty
  // (or whitespace-only) slot falls back to its CBn default.
  volum::custom::CustomAmp a;
  a.cabNames = {"Bogner 4x12", "Marshall 1960", "   "};
  volum::custom::NormalizeAmpCabNames(a);
  CHECK(a.cabNames[0] == "BOG");
  CHECK(a.cabNames[1] == "MAR");
  CHECK(a.cabNames[2] == "CB3");

  // Load path: a hand-edited / migrated registry carrying over-long names must
  // not survive into the live model (otherwise the cabinet row overflows).
  nlohmann::json amp;
  amp["id"] = "amp_clamp";
  amp["name"] = "Monomyth Skeleton Key";
  amp["cabNames"] = nlohmann::json::array({"Bogner 4x12", "Marshall 1960", "Orange PPC"});
  nlohmann::json amps = nlohmann::json::array();
  amps.push_back(amp);
  nlohmann::json j;
  j["schemaVersion"] = kContentSchemaVersion;
  j["customAmps"] = amps;

  bool healed = false;
  Registry r = RegistryFromJson(j, &healed);
  REQUIRE(r.amps.size() == 1);
  CHECK(r.amps[0].cabNames[0] == "BOG");
  CHECK(r.amps[0].cabNames[1] == "MAR");
  CHECK(r.amps[0].cabNames[2] == "ORA");
}

TEST_CASE("Registry round-trips amps, IRs, pedals, and presets")
{
  Registry r;
  r.nextPedalIndex = 67;

  volum::custom::CustomAmp amp;
  amp.id = "amp_abc";
  amp.name = "My Plexi";
  amp.art = 2;
  amp.cabNames = {"G65", "V30", "CB3"};
  amp.files = {{"amps/amp_abc__AMP-1.nam", volum::custom::kDirectSlot, 1}, {"amps/amp_abc__G65-1.nam", 0, 1}};
  r.amps.push_back(amp);

  r.irs.push_back({"ir_1", "Mesa 4x12", "ir/ir_1__mesa.wav"});
  r.pedals.push_back({"pedal_1", "Klon", "klon", "pedals/pedal_1__klon.nam", 64});

  Preset preset;
  preset.id = "preset_1";
  preset.name = "Church clean";
  preset.settings.toneBass = 7.5;
  preset.settings.activeIrId = "ir_1";
  preset.settings.preNam1Capture = 64;
  r.presetBanks[FactoryOwnerKey(7)] = {preset};

  r.midiSoundMap[5] = MidiSoundAssignment{"amp_abc", "preset_1"};
  r.midiSoundMap[6] = MidiSoundAssignment{FactoryOwnerKey(7), FactoryPresetId(7)};

  const auto j = RegistryToJson(r);
  bool healed = true;
  const Registry loaded = RegistryFromJson(j, &healed);
  CHECK_FALSE(healed);

  REQUIRE(loaded.amps.size() == 1);
  CHECK(loaded.amps[0].id == "amp_abc");
  CHECK(loaded.amps[0].name == "My Plexi");
  CHECK(loaded.amps[0].art == 2);
  CHECK(loaded.amps[0].cabNames[0] == "G65");
  REQUIRE(loaded.amps[0].files.size() == 2);
  CHECK(loaded.amps[0].files[0].slot == volum::custom::kDirectSlot);

  REQUIRE(loaded.irs.size() == 1);
  CHECK(loaded.irs[0].name == "Mesa 4x12");
  CHECK(loaded.irs[0].file == "ir/ir_1__mesa.wav");

  REQUIRE(loaded.pedals.size() == 1);
  CHECK(loaded.pedals[0].group == "klon");
  CHECK(loaded.pedals[0].legacyIndex == 64);
  CHECK(loaded.nextPedalIndex >= 67);

  REQUIRE(loaded.presetBanks.count(FactoryOwnerKey(7)) == 1);
  const auto& bank = loaded.presetBanks.at(FactoryOwnerKey(7));
  REQUIRE(bank.size() == 1);
  CHECK(bank[0].name == "Church clean");
  CHECK(bank[0].settings.toneBass == doctest::Approx(7.5));
  CHECK(bank[0].settings.activeIrId == "ir_1");
  CHECK(bank[0].settings.preNam1Capture == 64);

  REQUIRE(loaded.midiSoundMap.size() == 2);
  CHECK(loaded.midiSoundMap.at(5).ampId == "amp_abc");
  CHECK(loaded.midiSoundMap.at(5).presetId == "preset_1");
  CHECK(loaded.midiSoundMap.at(6).presetId == "factory:7:v1");
}

TEST_CASE("Reader skips malformed entries and clamps without aborting")
{
  nlohmann::json j;
  j["schemaVersion"] = 2;
  j["customAmps"] = nlohmann::json::array();
  j["customAmps"].push_back({{"name", "no id here"}}); // skipped (no id)
  j["customAmps"].push_back({{"id", "amp_ok"}, {"name", "Good"}}); // kept
  j["irLibrary"] = nlohmann::json::array();
  j["irLibrary"].push_back({{"id", ""}, {"name", "empty id"}}); // skipped
  j["irLibrary"].push_back({{"id", "ir_ok"}, {"name", "Keep"}});

  bool healed = false;
  const Registry r = RegistryFromJson(j, &healed);
  CHECK(healed);
  REQUIRE(r.amps.size() == 1);
  CHECK(r.amps[0].id == "amp_ok");
  REQUIRE(r.irs.size() == 1);
  CHECK(r.irs[0].id == "ir_ok");
}

TEST_CASE("ContentStore Save/Load round-trips on disk")
{
  const auto base = TestBase("save-load");
  ContentStore store(base);
  store.reg().irs.push_back({"ir_x", "Test IR", "ir/ir_x__t.wav"});
  REQUIRE(store.Save());
  CHECK(std::filesystem::exists(store.RegistryPath()));

  ContentStore reloaded(base);
  CHECK(reloaded.Load());
  REQUIRE(reloaded.reg().irs.size() == 1);
  CHECK(reloaded.reg().irs[0].name == "Test IR");
}

TEST_CASE("ContentStore moves an unparseable registry to .bak and starts fresh")
{
  const auto base = TestBase("corrupt");
  std::error_code ec;
  std::filesystem::create_directories(base, ec);
  {
    std::ofstream out(base / "volum-content.json", std::ios::binary);
    out << "{ this is not valid json ]";
  }
  ContentStore store(base);
  CHECK_FALSE(store.Load());
  CHECK(store.reg().amps.empty());
  CHECK(std::filesystem::exists(store.BackupPath()));
  CHECK_FALSE(std::filesystem::exists(store.RegistryPath()));
}

TEST_CASE("ContentStore missing registry loads as empty (fresh install)")
{
  const auto base = TestBase("missing");
  ContentStore store(base);
  CHECK(store.Load());
  CHECK(store.reg().amps.empty());
  CHECK(store.reg().irs.empty());
}

TEST_CASE("ImportFileCopy copies the source under a unique stored name")
{
  const auto base = TestBase("import");
  const auto src = WriteSrc(base / "incoming", "Mesa 4x12.wav", "RIFFfake");
  ContentStore store(base);
  const std::string rel = store.ImportFileCopy(src, "ir", "ir_zz");
  REQUIRE_FALSE(rel.empty());
  CHECK(rel == "ir/ir_zz__Mesa 4x12.wav");
  CHECK(std::filesystem::exists(store.ResolveStored(rel)));

  std::ifstream in(store.ResolveStored(rel), std::ios::binary);
  std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(body == "RIFFfake");
}

TEST_CASE("Custom NAM import validates every copied capture before commit")
{
  using volum::custom::CustomAmp;
  using volum::custom::CustomNamFile;

  const auto base = TestBase("custom-nam-transaction-success");
  const auto first = WriteSrc(base / "incoming", "MRSH.nam", "valid-one");
  const auto second = WriteSrc(base / "incoming", "Fender.nam", "valid-two");
  ContentStore store(base);

  CustomAmp draft;
  draft.id = "amp_tx";
  draft.name = "Two captures";
  draft.files = {
    CustomNamFile{"MRSH.nam", 0, 1, "", first.string()},
    CustomNamFile{"Fender.nam", 1, 2, "", second.string()},
  };

  int validations = 0;
  const auto prepared = PrepareCustomNamImport(store, draft, draft.id, [&](const std::filesystem::path& path) {
    ++validations;
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return body.rfind("valid-", 0) == 0 ? std::string() : std::string("invalid NAM");
  });

  REQUIRE(prepared);
  CHECK(validations == 2);
  CHECK(prepared.copiedPaths.size() == 2);
  REQUIRE(prepared.amp.files.size() == 2);
  for (const auto& file : prepared.amp.files)
  {
    CHECK(file.sourcePath.empty());
    REQUIRE_FALSE(file.storedPath.empty());
    CHECK(std::filesystem::exists(store.ResolveStored(file.storedPath)));
  }
  // Preparing is side-effect-free with respect to the registry; the UI commits
  // only after this result succeeds.
  CHECK(store.reg().amps.empty());
}

TEST_CASE("Custom NAM import and runtime resolution accept UTF-8 source and library paths")
{
  using volum::custom::CustomAmp;
  using volum::custom::CustomNamFile;

  const auto base = TestBase("custom-nam-unicode-source");
  const std::string unicodeDirUtf8 = "T\xC3\xB6ne_\xE6\x97\xA5\xE6\x9C\xAC_\xCE\x94";
  const auto src = WriteSrc(base / PathFromUtf8(unicodeDirUtf8), "MRSH SL68.nam", "valid-unicode-source");
  ContentStore store(base / PathFromUtf8("V\xC3\xB6Lum_\xE6\x97\xA5\xE6\x9C\xAC"));

  CustomAmp draft;
  draft.id = "amp_unicode";
  draft.name = "Unicode source";
  // PromptForFiles returns UTF-8 even on Windows. This exact boundary used to
  // feed filesystem::path(std::string), which treated it as ANSI and silently
  // saved an empty storedPath whenever a parent folder/username was non-ASCII.
  draft.files = {CustomNamFile{"MRSH SL68.nam", 0, 1, "", PathToUtf8(src)}};

  const auto prepared = PrepareCustomNamImport(store, draft, draft.id, [](const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return body == "valid-unicode-source" ? std::string() : std::string("unexpected bytes");
  });

  REQUIRE(prepared);
  REQUIRE(prepared.amp.files.size() == 1);
  CHECK_FALSE(prepared.amp.files[0].storedPath.empty());
  const auto resolved = store.ResolveStored(prepared.amp.files[0].storedPath);
  CHECK(std::filesystem::is_regular_file(resolved));
  // Product runtime queues absolute paths as UTF-8 strings and converts them
  // back only at filesystem boundaries. Pin that second half too.
  CHECK(std::filesystem::is_regular_file(PathFromUtf8(PathToUtf8(resolved))));
}

TEST_CASE("Custom NAM import failure rolls back the whole multi-file save")
{
  using volum::custom::CustomAmp;
  using volum::custom::CustomNamFile;

  const auto base = TestBase("custom-nam-transaction-rollback");
  const auto first = WriteSrc(base / "incoming", "Good.nam", "good");
  const auto second = WriteSrc(base / "incoming", "Broken.nam", "broken");
  ContentStore store(base);

  CustomAmp draft;
  draft.id = "amp_tx";
  draft.name = "Atomic amp";
  draft.files = {
    CustomNamFile{"Good.nam", 0, 1, "", first.string()},
    CustomNamFile{"Broken.nam", 1, 2, "", second.string()},
  };

  const auto prepared = PrepareCustomNamImport(store, draft, draft.id, [&](const std::filesystem::path& path) {
    return path.filename().string().find("Broken") == std::string::npos ? std::string()
                                                                        : std::string("unsupported NAM");
  });

  REQUIRE_FALSE(prepared);
  CHECK(prepared.copiedPaths.empty());
  CHECK(prepared.error.find("Broken.nam") != std::string::npos);
  CHECK(prepared.error.find("unsupported NAM") != std::string::npos);
  CHECK(store.reg().amps.empty());
  std::error_code ec;
  const auto ampsDir = store.AmpsDir();
  CHECK(std::filesystem::is_empty(ampsDir, ec));
  CHECK_FALSE(ec);
  // Caller-owned draft remains retryable; only the result copy was modified.
  CHECK(draft.files[0].sourcePath == first.string());
  CHECK(draft.files[1].sourcePath == second.string());
}

TEST_CASE("Custom NAM import rejects an empty persisted capture path")
{
  volum::custom::CustomAmp draft;
  draft.id = "amp_empty";
  draft.name = "Broken manifest";
  draft.files.push_back({"Missing.nam", 0, 1, "", ""});
  ContentStore store(TestBase("custom-nam-empty-path"));

  const auto prepared =
    PrepareCustomNamImport(store, draft, draft.id, [](const std::filesystem::path&) { return std::string(); });
  REQUIRE_FALSE(prepared);
  CHECK(prepared.error.find("saved capture path is empty") != std::string::npos);
}

TEST_CASE("Removal matrix: deleting a pedal clears referencing PRE slots")
{
  ContentStore store;
  store.reg().pedals.push_back({"pedal_1", "Klon", "klon", "", 64});
  Preset pr;
  pr.id = "preset_1";
  pr.settings.preNam1Capture = 64;
  pr.settings.preNam2Capture = 5; // factory, untouched
  store.reg().presetBanks["factory:0"] = {pr};

  store.RemovePedal("pedal_1");
  CHECK(store.reg().pedals.empty());
  CHECK(store.reg().presetBanks["factory:0"][0].settings.preNam1Capture == 0);
  CHECK(store.reg().presetBanks["factory:0"][0].settings.preNam2Capture == 5);
}

TEST_CASE("Removal matrix: deleting an IR clears activeIrId references")
{
  ContentStore store;
  store.reg().irs.push_back({"ir_1", "Mesa", ""});
  Preset pr;
  pr.id = "preset_1";
  pr.settings.activeIrId = "ir_1";
  pr.settings.supportActiveIrId = "ir_1";
  store.reg().presetBanks["factory:0"] = {pr};

  store.RemoveIR("ir_1");
  CHECK(store.reg().irs.empty());
  CHECK(store.reg().presetBanks["factory:0"][0].settings.activeIrId.empty());
  CHECK(store.reg().presetBanks["factory:0"][0].settings.supportActiveIrId.empty());
}

TEST_CASE("Removal matrix: deleting a custom amp cascades its bank and clears support refs")
{
  ContentStore store;
  volum::custom::CustomAmp amp;
  amp.id = "amp_main";
  store.reg().amps.push_back(amp);
  store.reg().presetBanks["amp_main"] = {Preset{"preset_1", "P", {}}};
  // A pre-1.3.0 library's shared scene for this amp is a migration leftover; the
  // delete has to drop it too or a re-imported amp reusing the id would inherit it.
  store.reg().legacyCustomScenes["amp_main"] = VoLumAmpSettings{};

  // A preset in an unrelated bank that referenced the removed amp as its support
  // partner must also be reset to "(none)".
  Preset withSupport;
  withSupport.id = "preset_sup";
  withSupport.name = "Dual";
  withSupport.settings.supportCustomId = "amp_main";
  store.reg().presetBanks["factory:2"] = {withSupport};

  store.RemoveCustomAmp("amp_main");
  CHECK(store.reg().amps.empty());
  CHECK(store.reg().presetBanks.count("amp_main") == 0);
  CHECK(store.reg().legacyCustomScenes.count("amp_main") == 0);
  CHECK(store.reg().presetBanks.at("factory:2")[0].settings.supportCustomId.empty());
}

// Regression: RemoveCustomAmp must delete the copied .nam by its resolvable
// `storedPath`, not the display-leaf `file`. Deleting by leaf silently resolves
// to the wrong path and orphans the imported file on disk.
TEST_CASE("Removal matrix: deleting a custom amp removes its copied files by storedPath")
{
  const auto base = TestBase("amp-remove-files");
  const auto src = WriteSrc(base / "incoming", "G65-Plexi.nam", "NAMfake");
  ContentStore store(base);
  const std::string rel = store.ImportFileCopy(src, "amps", "amp_x_0");
  REQUIRE_FALSE(rel.empty());
  REQUIRE(std::filesystem::exists(store.ResolveStored(rel)));

  volum::custom::CustomAmp amp;
  amp.id = "amp_main";
  volum::custom::CustomNamFile f;
  f.file = "G65-Plexi.nam"; // display leaf only - not resolvable
  f.storedPath = rel; // registry-relative resolvable path
  f.slot = volum::custom::kDirectSlot;
  f.channel = 1;
  amp.files.push_back(f);
  store.reg().amps.push_back(amp);

  store.RemoveCustomAmp("amp_main");
  CHECK(store.reg().amps.empty());
  // The payload deliberately outlives the in-memory removal: it is destroyed only
  // once a registry that no longer names it is durable, so a failed registry write
  // cannot leave the on-disk library pointing at a file that is already gone.
  CHECK(std::filesystem::exists(store.ResolveStored(rel)));
  REQUIRE(store.Save());
  CHECK_FALSE(std::filesystem::exists(store.ResolveStored(rel)));
}

// Regression (ASan): the Remove* methods are commonly called with a reference to
// the id string OWNED by the element being erased (e.g. the UI bridge does
// RemoveIR(reg.irs[idx].id)). They must copy the id before erasing, or the
// subsequent scene/preset/support sweep dereferences freed memory. Passing the
// element-owned id here reproduces the exact dangling path under AddressSanitizer.
TEST_CASE("Removal matrix: deleting via the element-owned id does not dangle (ASan)")
{
  SUBCASE("IR")
  {
    ContentStore store;
    store.reg().irs.push_back({"ir_keep", "Keep", ""});
    store.reg().irs.push_back({"ir_drop", "Drop", ""});
    Preset pr;
    pr.id = "preset_1";
    pr.settings.activeIrId = "ir_drop";
    store.reg().presetBanks["factory:0"] = {pr};

    store.RemoveIR(store.reg().irs[1].id); // reference into the erased element
    CHECK(store.reg().irs.size() == 1);
    CHECK(store.reg().irs[0].id == "ir_keep");
    CHECK(store.reg().presetBanks["factory:0"][0].settings.activeIrId.empty());
  }

  SUBCASE("pedal")
  {
    ContentStore store;
    store.reg().pedals.push_back({"pedal_drop", "Klon", "klon", "", 64});
    Preset pr;
    pr.id = "preset_1";
    pr.settings.preNam1Capture = 64;
    store.reg().presetBanks["factory:0"] = {pr};

    store.RemovePedal(store.reg().pedals[0].id); // reference into the erased element
    CHECK(store.reg().pedals.empty());
    CHECK(store.reg().presetBanks["factory:0"][0].settings.preNam1Capture == 0);
  }

  SUBCASE("custom amp")
  {
    ContentStore store;
    volum::custom::CustomAmp amp;
    amp.id = "amp_drop";
    store.reg().amps.push_back(amp);
    store.reg().presetBanks["amp_drop"] = {Preset{"preset_1", "P", {}}};
    Preset other;
    other.id = "preset_other";
    other.settings.supportCustomId = "amp_drop";
    store.reg().presetBanks["factory:1"] = {other};

    store.RemoveCustomAmp(store.reg().amps[0].id); // reference into the erased element
    CHECK(store.reg().amps.empty());
    CHECK(store.reg().presetBanks.count("amp_drop") == 0);
    CHECK(store.reg().presetBanks["factory:1"][0].settings.supportCustomId.empty());
  }
}

TEST_CASE("AmpSettings JSON codec round-trips a full scene")
{
  VoLumAmpSettings s;
  s.speakerIdx = 2;
  s.channelIdx = 3;
  s.inputLevel = 4.5;
  s.toneBass = 7.0;
  s.preCompActive = true;
  s.preNam1Active = true;
  s.preNam1Capture = 65;
  s.dualAmpActive = true;
  s.supportAmpIdx = 9;
  s.supportPolarityInvert = false;
  s.postValid = true;
  s.postReverbActive = true;
  s.postReverbMix = 0.42;
  s.activeIrId = "ir_77";
  s.supportCustomId = "amp_55";
  s.supportActiveIrId = "ir_88";
  s.supportCustomSlot = 1;
  s.supportCustomChannel = 4;

  const auto j = volum::AmpSettingsToJson(s);
  VoLumAmpSettings out;
  const bool healed = volum::AmpSettingsFromJson(j, out);
  CHECK_FALSE(healed);
  CHECK(out.speakerIdx == 2);
  CHECK(out.channelIdx == 3);
  CHECK(out.inputLevel == doctest::Approx(4.5));
  CHECK(out.toneBass == doctest::Approx(7.0));
  CHECK(out.preCompActive);
  CHECK(out.preNam1Active);
  CHECK(out.preNam1Capture == 65);
  CHECK(out.dualAmpActive);
  CHECK(out.supportAmpIdx == 9);
  CHECK_FALSE(out.supportPolarityInvert);
  CHECK(out.postValid);
  CHECK(out.postReverbActive);
  CHECK(out.postReverbMix == doctest::Approx(0.42));
  CHECK(out.activeIrId == "ir_77");
  CHECK(out.supportCustomId == "amp_55");
  CHECK(out.supportActiveIrId == "ir_88");
  CHECK(out.supportCustomSlot == 1);
  CHECK(out.supportCustomChannel == 4);
}

TEST_CASE("CaptureFileFor resolves the storedPath for a (slot, channel) cell")
{
  using volum::custom::CustomAmp;
  using volum::custom::CustomNamFile;
  using volum::custom::kDirectSlot;
  CustomAmp amp;
  CustomNamFile direct;
  direct.file = "AMP-1.nam";
  direct.slot = kDirectSlot;
  direct.channel = 1;
  direct.storedPath = "amps/amp_x_0__AMP-1.nam";
  CustomNamFile cab;
  cab.file = "G65-2.nam";
  cab.slot = 0;
  cab.channel = 2;
  cab.storedPath = "amps/amp_x_1__G65-2.nam";
  amp.files = {direct, cab};

  CHECK(CaptureFileFor(amp, kDirectSlot, 1) == "amps/amp_x_0__AMP-1.nam");
  CHECK(CaptureFileFor(amp, 0, 2) == "amps/amp_x_1__G65-2.nam");
  CHECK(CaptureFileFor(amp, 0, 9).empty()); // no such channel
  CHECK(CaptureFileFor(amp, 1, 1).empty()); // empty cab slot
}

TEST_CASE("DefaultCaptureSelection picks DIRECT-first, lowest channel")
{
  using volum::custom::kDirectSlot;
  volum::custom::CustomAmp amp;
  // DIRECT has {1,2}, cab slot 0 has {1}: DIRECT should win, lowest channel = 1.
  amp.files = {{"a.nam", kDirectSlot, 2}, {"b.nam", kDirectSlot, 1}, {"c.nam", 0, 1}};
  int slot = 99, channel = 99;
  REQUIRE(DefaultCaptureSelection(amp, slot, channel));
  CHECK(slot == kDirectSlot); // DIRECT populated -> chosen first
  CHECK(channel == 1); // its lowest assigned channel

  volum::custom::CustomAmp empty;
  int s2 = 7, c2 = 7;
  CHECK_FALSE(DefaultCaptureSelection(empty, s2, c2)); // no files -> false, outputs untouched
  CHECK(s2 == 7);
  CHECK(c2 == 7);
}

TEST_CASE("Custom-amp storedPath survives a registry round-trip")
{
  Registry r;
  volum::custom::CustomAmp amp;
  amp.id = "amp_round";
  amp.name = "Round amp";
  volum::custom::CustomNamFile f;
  f.file = "AMP-1.nam";
  f.slot = volum::custom::kDirectSlot;
  f.channel = 1;
  f.storedPath = "amps/amp_round_0__AMP-1.nam";
  amp.files = {f};
  r.amps.push_back(amp);

  const auto j = RegistryToJson(r);
  const Registry back = RegistryFromJson(j);
  REQUIRE(back.amps.size() == 1);
  REQUIRE(back.amps[0].files.size() == 1);
  CHECK(back.amps[0].files[0].storedPath == "amps/amp_round_0__AMP-1.nam");
  CHECK(back.amps[0].files[0].sourcePath.empty()); // transient, never serialized
}

// ---- F5 preset equality (drives the header bar "(unsaved)" flag) ------------

TEST_CASE("AmpSettingsEqual is a full value-equality over the settings codec")
{
  VoLumAmpSettings a; // defaults
  VoLumAmpSettings b; // defaults
  CHECK(volum::AmpSettingsEqual(a, b));

  // A core-tone edit diverges them...
  b.toneTreble = a.toneTreble + 1.0;
  CHECK_FALSE(volum::AmpSettingsEqual(a, b));
  // ...and returning the value exactly restores equality (A/B clears dirty).
  b.toneTreble = a.toneTreble;
  CHECK(volum::AmpSettingsEqual(a, b));

  // The id-based custom refs participate too.
  b.activeIrId = "ir_abc";
  CHECK_FALSE(volum::AmpSettingsEqual(a, b));
  b.activeIrId.clear();
  b.supportCustomId = "amp_def";
  CHECK_FALSE(volum::AmpSettingsEqual(a, b));
  b.supportCustomId.clear();
  CHECK(volum::AmpSettingsEqual(a, b));

  // Custom SUPPORT cab/channel participate in equality (drives recall + dirty).
  b.supportCustomSlot = 1;
  CHECK_FALSE(volum::AmpSettingsEqual(a, b));
  b.supportCustomSlot = a.supportCustomSlot;
  b.supportCustomChannel = 4;
  CHECK_FALSE(volum::AmpSettingsEqual(a, b));
  b.supportCustomChannel = a.supportCustomChannel;
  CHECK(volum::AmpSettingsEqual(a, b));

  // A PRE-capture selection (custom pedal legacy index) diverges and restores.
  b.preNam1Capture = 64;
  CHECK_FALSE(volum::AmpSettingsEqual(a, b));
  b.preNam1Capture = a.preNam1Capture;
  CHECK(volum::AmpSettingsEqual(a, b));
}

TEST_CASE("A recalled preset snapshot round-trips through the registry equal")
{
  // A preset stores a full settings snapshot; after a JSON round-trip the
  // decoded snapshot must compare equal so recall lands clean (no false dirty).
  Registry r;
  Preset pr;
  pr.id = "preset_rt";
  pr.name = "Lead";
  pr.settings.toneBass = 7.5;
  pr.settings.activeIrId = "ir_123";
  pr.settings.preNam1Capture = 65;
  pr.settings.dualAmpActive = true;
  r.presetBanks["factory:3"] = {pr};

  const Registry back = RegistryFromJson(RegistryToJson(r));
  REQUIRE(back.presetBanks.count("factory:3") == 1);
  REQUIRE(back.presetBanks.at("factory:3").size() == 1);
  CHECK(volum::AmpSettingsEqual(back.presetBanks.at("factory:3")[0].settings, pr.settings));
}

// Release regression (1.2.0 "bring your own + presets"): a real project mixes a
// multi-.nam custom amp, a custom IR, a custom PRE pedal, a per-amp scene, and a
// preset that references all three at once. The codec/store units above each
// cover one facet; this pins the whole combination surviving a real on-disk
// Save() -> Load() with the copied capture files still resolvable. Missing this
// end-to-end pin is how a "presets/BYO lost my stuff on reload" bug would ship.
TEST_CASE("Combined BYO project (multi-nam amp + IR + pedal + scene + preset) round-trips on disk")
{
  using volum::custom::CustomAmp;
  using volum::custom::CustomNamFile;
  using volum::custom::kDirectSlot;

  const auto base = TestBase("byo-combined");
  const auto directSrc = WriteSrc(base / "incoming", "AMP-Plexi-1.nam", "NAMdirect");
  const auto cabSrc = WriteSrc(base / "incoming", "G65-Plexi-2.nam", "NAMcab");
  const auto irSrc = WriteSrc(base / "incoming", "Mesa 4x12.wav", "RIFFir");
  const auto pedalSrc = WriteSrc(base / "incoming", "Klon.nam", "NAMpedal");

  ContentStore store(base);

  // Custom amp with two captures on different (slot, channel) cells.
  CustomAmp amp;
  amp.id = "amp_byo";
  amp.name = "BYO Plexi";
  amp.cabNames = {"G65", "CB2", "CB3"};
  CustomNamFile direct;
  direct.file = "AMP-Plexi-1.nam";
  direct.slot = kDirectSlot;
  direct.channel = 1;
  direct.storedPath = store.ImportFileCopy(directSrc, "amps", "amp_byo_0");
  CustomNamFile cab;
  cab.file = "G65-Plexi-2.nam";
  cab.slot = 0;
  cab.channel = 2;
  cab.storedPath = store.ImportFileCopy(cabSrc, "amps", "amp_byo_1");
  amp.files = {direct, cab};
  store.reg().amps.push_back(amp);

  // Custom IR + custom PRE pedal (legacy index in the custom pool).
  IRItem ir;
  ir.id = "ir_byo";
  ir.name = "Mesa OS";
  ir.file = store.ImportFileCopy(irSrc, "ir", "ir_byo");
  store.reg().irs.push_back(ir);

  PedalItem pedal;
  pedal.id = "pedal_byo";
  pedal.name = "Klon";
  pedal.group = "klon";
  pedal.file = store.ImportFileCopy(pedalSrc, "pedals", "pedal_byo");
  pedal.legacyIndex = kCustomPedalIndexBase;
  store.reg().pedals.push_back(pedal);

  // MIDI slot pointing at the custom amp's own bank.
  store.reg().midiSoundMap[3] = MidiSoundAssignment{"amp_byo", "preset_byo"};

  // A preset in this amp's bank referencing all three custom refs together.
  Preset pr;
  pr.id = "preset_byo";
  pr.name = "Dual lead";
  pr.settings.activeIrId = "ir_byo";
  pr.settings.preNam1Active = true;
  pr.settings.preNam1Capture = kCustomPedalIndexBase;
  pr.settings.dualAmpActive = true;
  pr.settings.supportCustomId = "amp_byo";
  pr.settings.supportCustomSlot = 0;
  pr.settings.supportCustomChannel = 2;
  store.reg().presetBanks["amp_byo"] = {pr};

  REQUIRE(store.Save());

  ContentStore reloaded(base);
  REQUIRE(reloaded.Load());
  const Registry& r = reloaded.reg();

  // Custom amp + both captures survive, and their copied files resolve on disk.
  REQUIRE(r.amps.size() == 1);
  CHECK(r.amps[0].id == "amp_byo");
  REQUIRE(r.amps[0].files.size() == 2);
  CHECK(CaptureFileFor(r.amps[0], kDirectSlot, 1) == direct.storedPath);
  CHECK(CaptureFileFor(r.amps[0], 0, 2) == cab.storedPath);
  CHECK(std::filesystem::exists(reloaded.ResolveStored(direct.storedPath)));
  CHECK(std::filesystem::exists(reloaded.ResolveStored(cab.storedPath)));

  // Custom IR + pedal survive with resolvable files.
  REQUIRE(r.irs.size() == 1);
  CHECK(r.irs[0].id == "ir_byo");
  CHECK(std::filesystem::exists(reloaded.ResolveStored(r.irs[0].file)));
  REQUIRE(r.pedals.size() == 1);
  CHECK(r.pedals[0].legacyIndex == kCustomPedalIndexBase);
  CHECK(std::filesystem::exists(reloaded.ResolveStored(r.pedals[0].file)));

  // The MIDI slot survives the round-trip and still resolves.
  REQUIRE(r.midiSoundMap.count(3) == 1);
  CHECK(r.midiSoundMap.at(3).ampId == "amp_byo");
  CHECK(ResolveMidiSlot(r, 3, 12) == MidiSlotState::Valid);

  // Preset keeps every custom ref together (the recall payload is intact).
  REQUIRE(r.presetBanks.count("amp_byo") == 1);
  REQUIRE(r.presetBanks.at("amp_byo").size() == 1);
  const auto& got = r.presetBanks.at("amp_byo")[0].settings;
  CHECK(got.activeIrId == "ir_byo");
  CHECK(got.preNam1Capture == kCustomPedalIndexBase);
  CHECK(got.supportCustomId == "amp_byo");
  CHECK(got.supportCustomSlot == 0);
  CHECK(got.supportCustomChannel == 2);
  CHECK(volum::AmpSettingsEqual(r.presetBanks.at("amp_byo")[0].settings, pr.settings));
}

// Release regression: recalling a preset / focusing a custom amp whose copied
// capture file was deleted from disk (moved library, partial sync) must not hand
// the loader a path that "resolves" to a missing file without any signal. This
// pins the store contract: CaptureFileFor still returns the recorded storedPath
// (so higher layers can decide), ResolveStored composes it under base, and the
// caller can detect the file is gone via std::filesystem::exists.
TEST_CASE("Missing copied capture is detectable via ResolveStored (no silent success)")
{
  using volum::custom::CustomAmp;
  using volum::custom::CustomNamFile;
  using volum::custom::kDirectSlot;

  const auto base = TestBase("byo-missing-file");
  const auto src = WriteSrc(base / "incoming", "AMP-1.nam", "NAMbody");
  ContentStore store(base);

  CustomNamFile f;
  f.file = "AMP-1.nam";
  f.slot = kDirectSlot;
  f.channel = 1;
  f.storedPath = store.ImportFileCopy(src, "amps", "amp_gone_0");
  REQUIRE_FALSE(f.storedPath.empty());
  CustomAmp amp;
  amp.id = "amp_gone";
  amp.files = {f};

  const auto resolved = store.ResolveStored(f.storedPath);
  REQUIRE(std::filesystem::exists(resolved));

  // Simulate the copied capture disappearing from the content library.
  std::error_code ec;
  std::filesystem::remove(resolved, ec);
  REQUIRE_FALSE(ec);

  // The reference is preserved (not silently blanked)...
  CHECK(CaptureFileFor(amp, kDirectSlot, 1) == f.storedPath);
  // ...but the resolved path now reports missing, so the loader can skip/report
  // instead of trying to load a phantom model.
  CHECK_FALSE(std::filesystem::exists(store.ResolveStored(f.storedPath)));
}

TEST_CASE("Reopen dirty baseline derives from preset content, not the live scene")
{
  // Funnel C: on session / DAW restore the recalled-snapshot baseline must be
  // the preset's STORED settings. If the project was closed with unsaved edits
  // (flushed into the live scene), reopen must read dirty; a clean reopen must
  // read clean. Here we model the equality check _VolumRecomputePresetDirty runs.
  Registry r;
  Preset pr;
  pr.id = "preset_dirty";
  pr.name = "Crunch";
  pr.settings.outputLevel = -6.0;
  pr.settings.toneMid = 6.0;
  r.presetBanks["factory:3"] = {pr};

  // Simulate reopen: baseline comes from the bank entry's stored settings.
  const Registry back = RegistryFromJson(RegistryToJson(r));
  const volum::VoLumAmpSettings baseline = back.presetBanks.at("factory:3")[0].settings;

  // Clean reopen: live scene == preset content -> not dirty.
  volum::VoLumAmpSettings cleanLive = baseline;
  CHECK(volum::AmpSettingsEqual(cleanLive, baseline));

  // Dirty reopen: an unsaved edit (e.g. dual amp toggled, output nudged) that
  // was flushed to settings on close must differ from the preset baseline.
  volum::VoLumAmpSettings editedLive = baseline;
  editedLive.dualAmpActive = !baseline.dualAmpActive;
  CHECK_FALSE(volum::AmpSettingsEqual(editedLive, baseline));

  volum::VoLumAmpSettings editedOutput = baseline;
  editedOutput.outputLevel = 0.0;
  CHECK_FALSE(volum::AmpSettingsEqual(editedOutput, baseline));
}

// ---------------------------------------------------------------------------
// Library durability
//
// volum-content.json is the only record of which custom amps, IRs, pedals and
// preset banks exist, and what their scenes are. The payload files beside it are
// worthless without it. Users sync this directory, restore it from backups and
// hand-edit it, so the store has to treat its own registry as untrusted input and
// has to order its writes so that no single failure destroys anything.
// ---------------------------------------------------------------------------

TEST_CASE("A stored path that escapes the content directory resolves to nothing")
{
  const auto base = TestBase("stored-path-escape");
  ContentStore store(base);

  // A relative walk out of the library, the form a hand-merged or sync-mangled
  // registry most plausibly ends up with.
  CHECK(store.ResolveStored("../../sentinel.wav").empty());
  CHECK(store.ResolveStored("ir/../../sentinel.wav").empty());
  CHECK(store.ResolveStored("..").empty());

  // An absolute path is worse than a walk: `mBase / absolute` discards mBase
  // entirely and resolves wherever the string points.
#ifdef _WIN32
  CHECK(store.ResolveStored("C:/Windows/System32/drivers/etc/hosts").empty());
  CHECK(store.ResolveStored("C:ir/relative-to-drive.wav").empty());
  CHECK(store.ResolveStored("\\\\server\\share\\file.wav").empty());
#endif
  CHECK(store.ResolveStored("/etc/passwd").empty());
  CHECK(store.ResolveStored("").empty());

  // Every payload lives in a subdirectory of the library, so an entry that names a
  // directory instead of a file is not a payload. It matters because remove()
  // succeeds on an empty directory: "." and "ir" resolved to the library root and
  // to its ir/ folder, and a delete would have taken the folder rather than a
  // capture.
  CHECK(store.ResolveStored(".").empty());
  CHECK(store.ResolveStored("./").empty());
  CHECK(store.ResolveStored("ir").empty());
  CHECK(store.ResolveStored("ir/").empty());
  CHECK(store.ResolveStored("./ir/ir_x__cab.wav").empty());

  // Genuine library-relative paths still resolve, under the base.
  const auto ok = store.ResolveStored("ir/ir_x__cab.wav");
  REQUIRE_FALSE(ok.empty());
  CHECK(ok == base / "ir" / "ir_x__cab.wav");
}

TEST_CASE("Deleting an entry whose path escapes the library leaves the outside file alone")
{
  const auto base = TestBase("stored-path-escape-delete");
  // Somewhere outside the content directory that a traversal can reach.
  const auto outside = WriteSrc(base.parent_path(), "sentinel-do-not-delete.wav", "precious");
  REQUIRE(std::filesystem::exists(outside));

  ContentStore store(base);
  IRItem ir;
  ir.id = "ir_evil";
  ir.name = "Escaping IR";
  ir.file = "../sentinel-do-not-delete.wav";
  store.reg().irs.push_back(ir);

  store.RemoveIR("ir_evil");
  REQUIRE(store.Save());

  CHECK(store.reg().irs.empty());
  // The assertion that fails against the pre-fix code, which composed
  // `mBase / relPath` and handed the result straight to filesystem::remove.
  CHECK(std::filesystem::exists(outside));

  std::error_code ec;
  std::filesystem::remove(outside, ec);
}

TEST_CASE("A library that exists but cannot be read is never overwritten")
{
  const auto base = TestBase("registry-unreadable");

  // A directory where the registry file belongs: it exists, and it is not a file we
  // can read. This stands in for the real causes - antivirus, a backup or cloud-sync
  // agent, another VoLum, a permissions change - without needing any of them.
  //
  // It has to be recognised before opening: Windows refuses to open a directory, but
  // libc++ opens it happily and fails on the first read, which looks exactly like an
  // empty file. That sent macOS down the "corrupt, back it up, rewrite" path and let
  // the save through - caught by CI on this very test, not by the Windows run.
  std::error_code ec;
  std::filesystem::create_directories(base / "volum-content.json", ec);
  REQUIRE_FALSE(ec);

  ContentStore store(base);
  CHECK_FALSE(store.Load());
  CHECK(store.RegistryUnreadable());

  // The in-memory registry is empty, because nothing could be read into it. The
  // guarantee is not that it is correct - it cannot be - but that this emptiness
  // never reaches disk. Before the fix Load() reported a clean empty library and
  // the next save serialized it over the real one.
  IRItem ir;
  ir.id = "ir_new";
  ir.name = "Imported during the outage";
  store.reg().irs.push_back(ir);
  CHECK_FALSE(store.Save());

  // And nothing was filed away as a corrupt copy, which would have implied the
  // contents were readable and rewritable.
  CHECK_FALSE(std::filesystem::exists(base / "volum-content.json.bak"));

  // Once the library can be read again, writing is allowed again.
  std::filesystem::remove_all(base / "volum-content.json", ec);
  REQUIRE_FALSE(ec);
  ContentStore recovered(base);
  CHECK(recovered.Load());
  CHECK_FALSE(recovered.RegistryUnreadable());
  CHECK(recovered.Save());
}

TEST_CASE("A missing library is a clean empty one, not an unreadable one")
{
  // The distinction the fix turns on: first run on a new machine must stay a
  // normal, writable empty library.
  const auto base = TestBase("registry-absent");
  ContentStore store(base);
  CHECK(store.Load());
  CHECK_FALSE(store.RegistryUnreadable());
  CHECK(store.Save());
  CHECK(std::filesystem::exists(base / "volum-content.json"));
}

TEST_CASE("A failed registry write keeps the payload it was about to orphan")
{
  const auto base = TestBase("delete-save-failure");
  const auto src = WriteSrc(base / "incoming", "cab.wav", "RIFFfake");
  ContentStore store(base);
  const std::string rel = store.ImportFileCopy(src, "ir", "ir_x");
  REQUIRE_FALSE(rel.empty());

  IRItem ir;
  ir.id = "ir_x";
  ir.name = "Doomed";
  ir.file = rel;
  store.reg().irs.push_back(ir);
  REQUIRE(store.Save());

  // Make the registry file unwritable by putting a directory in its place, which
  // is what a sharing violation or a read-only library looks like to the atomic
  // write. Note the store was constructed after this point in no test - the
  // unreadable flag is not involved here; this is purely a write failure.
  std::error_code ec;
  std::filesystem::remove(base / "volum-content.json", ec);
  std::filesystem::create_directories(base / "volum-content.json", ec);
  REQUIRE_FALSE(ec);

  store.RemoveIR("ir_x");
  CHECK_FALSE(store.Save());

  // Entry and file are still consistent with each other: the on-disk registry
  // never lost the entry, and the payload it names is still there. Before the fix
  // the file was deleted first, so this combination left an item that reappeared
  // on restart and could never load again.
  CHECK(std::filesystem::exists(base / "ir" / std::filesystem::path(rel).filename()));

  // And the deletion is not forgotten - it lands as soon as a write succeeds.
  std::filesystem::remove_all(base / "volum-content.json", ec);
  REQUIRE(store.Save());
  CHECK_FALSE(std::filesystem::exists(base / "ir" / std::filesystem::path(rel).filename()));
}

TEST_CASE("A second spelling of the same payload still counts as a reference")
{
  // Two entries can name one file in spellings that differ as text but resolve to the
  // same file: a library merged from two machines, or restored from a backup made on
  // a case-insensitive volume. A byte comparison saw no surviving reference and
  // deleted the payload the just-written registry still names - the exact outcome the
  // deferred delete exists to prevent, arrived at from the other side.
  const auto base = TestBase("delete-alias-guard");
  const auto src = WriteSrc(base / "incoming", "cab.wav", "RIFFfake");
  ContentStore store(base);
  const std::string rel = store.ImportFileCopy(src, "ir", "ir_shared");
  REQUIRE_FALSE(rel.empty());

  const std::string leaf = std::filesystem::path(rel).filename().string();

  IRItem doomed;
  doomed.id = "ir_doomed";
  doomed.name = "Doomed";
  doomed.file = rel;

  IRItem survivor;
  survivor.id = "ir_survivor";
  survivor.name = "Survivor";
  // Same file, different spelling: a backslash separator and a capitalised folder.
  survivor.file = "IR\\" + leaf;

  store.reg().irs.push_back(doomed);
  store.reg().irs.push_back(survivor);
  REQUIRE(store.Save());

  store.RemoveIR("ir_doomed");
  REQUIRE(store.Save());

  CHECK(store.reg().irs.size() == 1);
  CHECK(std::filesystem::exists(base / "ir" / leaf));
}

TEST_CASE("Rolling back an import deletes its copies immediately")
{
  // The other half of the ordering rule: files copied in by a transaction that
  // never committed are referenced by nothing, so they must not wait for a Save
  // that a failed import is never going to perform.
  const auto base = TestBase("import-rollback-immediate");
  const auto src = WriteSrc(base / "incoming", "cab.wav", "RIFFfake");
  ContentStore store(base);
  const std::string rel = store.ImportFileCopy(src, "ir", "ir_tmp");
  REQUIRE_FALSE(rel.empty());
  REQUIRE(std::filesystem::exists(store.ResolveStored(rel)));

  store.RemoveStoredFile(rel);
  CHECK_FALSE(std::filesystem::exists(store.ResolveStored(rel)));
}

// ===========================================================================
// Two-writer library (1.3.0)
//
// Before 1.3.0 every Save() serialized this writer's whole in-memory catalog over
// the file. Two VoLums that both had the library open therefore overwrote each
// other wholesale: an IR imported in standalone vanished the moment the DAW
// instance saved a preset, because the DAW's copy of the catalog predated the
// import. The fix is a locked read-modify-write - re-read under a cross-process
// lock, replay only this writer's id-level changes, write the merge.
// ===========================================================================

TEST_CASE("Two writers on one library: both new items survive both saves")
{
  const auto base = TestBase("two-writer-both-items");

  ContentStore standalone(base);
  ContentStore daw(base);
  REQUIRE(standalone.Load());
  REQUIRE(daw.Load()); // both read the same (empty) starting catalog

  // Standalone imports an IR.
  standalone.reg().irs.push_back({"ir_standalone", "Mesa OS", "ir/mesa.wav"});
  REQUIRE(standalone.Save());

  // The DAW instance, whose catalog predates that import, saves a named preset.
  Preset pr;
  pr.id = "preset_daw";
  pr.name = "Church clean";
  daw.reg().presetBanks["factory:3"] = {pr};
  REQUIRE(daw.Save());

  ContentStore next(base);
  REQUIRE(next.Load());
  REQUIRE(next.reg().irs.size() == 1);
  CHECK(next.reg().irs[0].id == "ir_standalone");
  REQUIRE(next.reg().presetBanks.count("factory:3") == 1);
  REQUIRE(next.reg().presetBanks.at("factory:3").size() == 1);
  CHECK(next.reg().presetBanks.at("factory:3")[0].name == "Church clean");

  // The writer that merged also adopts the other's item, so its own next save
  // cannot drop it again.
  REQUIRE(daw.reg().irs.size() == 1);
  CHECK(daw.reg().irs[0].id == "ir_standalone");
}

TEST_CASE("Two writers: merges are by id, not by whole-list replacement")
{
  const auto base = TestBase("two-writer-id-level");

  ContentStore a(base);
  a.reg().pedals.push_back({"pedal_shared", "Klon", "klon", "pedals/klon.nam", 64});
  a.reg().irs.push_back({"ir_shared", "Shared", "ir/shared.wav"});
  REQUIRE(a.Save());

  ContentStore b(base);
  REQUIRE(b.Load());

  SUBCASE("an add by each writer lands in one list")
  {
    a.reg().irs.push_back({"ir_a", "From A", "ir/a.wav"});
    b.reg().irs.push_back({"ir_b", "From B", "ir/b.wav"});
    REQUIRE(a.Save());
    REQUIRE(b.Save());

    ContentStore next(base);
    REQUIRE(next.Load());
    REQUIRE(next.reg().irs.size() == 3);
    std::vector<std::string> ids;
    for (const auto& ir : next.reg().irs)
      ids.push_back(ir.id);
    CHECK(std::find(ids.begin(), ids.end(), "ir_shared") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "ir_a") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "ir_b") != ids.end());
  }

  SUBCASE("a delete by one writer is not resurrected by the other's save")
  {
    // B still has ir_shared in memory. If Save() replayed B's whole list, the
    // delete A already committed would come back from the dead.
    a.RemoveIR("ir_shared");
    REQUIRE(a.Save());

    b.reg().irs.push_back({"ir_b", "From B", "ir/b.wav"});
    REQUIRE(b.Save());

    ContentStore next(base);
    REQUIRE(next.Load());
    REQUIRE(next.reg().irs.size() == 1);
    CHECK(next.reg().irs[0].id == "ir_b");
  }

  SUBCASE("a rename by one writer is not reverted by the other's save")
  {
    a.reg().irs[0].name = "Renamed by A";
    REQUIRE(a.Save());

    b.reg().pedals[0].name = "Renamed by B";
    REQUIRE(b.Save());

    ContentStore next(base);
    REQUIRE(next.Load());
    REQUIRE(next.reg().irs.size() == 1);
    CHECK(next.reg().irs[0].name == "Renamed by A");
    REQUIRE(next.reg().pedals.size() == 1);
    CHECK(next.reg().pedals[0].name == "Renamed by B");
  }
}

TEST_CASE("Two writers: preset banks merge per preset, not per bank")
{
  // Two instances focused on the same amp both save a preset into that amp's
  // bank. Replacing the bank wholesale loses one of them.
  const auto base = TestBase("two-writer-banks");

  ContentStore a(base);
  volum::custom::CustomAmp amp;
  amp.id = "amp_shared";
  amp.name = "Plexi";
  a.reg().amps.push_back(amp);
  REQUIRE(a.Save());

  ContentStore b(base);
  REQUIRE(b.Load());

  Preset fromA;
  fromA.id = "preset_a";
  fromA.name = "Crunch";
  a.reg().presetBanks["amp_shared"].push_back(fromA);
  REQUIRE(a.Save());

  Preset fromB;
  fromB.id = "preset_b";
  fromB.name = "Lead";
  b.reg().presetBanks["amp_shared"].push_back(fromB);
  REQUIRE(b.Save());

  ContentStore next(base);
  REQUIRE(next.Load());
  REQUIRE(next.reg().presetBanks.count("amp_shared") == 1);
  const auto& bank = next.reg().presetBanks.at("amp_shared");
  REQUIRE(bank.size() == 2);
  std::vector<std::string> names;
  for (const auto& pr : bank)
    names.push_back(pr.name);
  CHECK(std::find(names.begin(), names.end(), "Crunch") != names.end());
  CHECK(std::find(names.begin(), names.end(), "Lead") != names.end());
}

TEST_CASE("Two writers: MIDI slot assignments merge per slot")
{
  const auto base = TestBase("two-writer-midi");

  ContentStore a(base);
  a.reg().amps.push_back(volum::custom::CustomAmp{});
  a.reg().amps[0].id = "amp_1";
  REQUIRE(a.Save());

  ContentStore b(base);
  REQUIRE(b.Load());

  a.SetMidiSlot(1, "amp_1", "");
  REQUIRE(a.Save());
  b.SetMidiSlot(2, "amp_1", "");
  REQUIRE(b.Save());

  ContentStore next(base);
  REQUIRE(next.Load());
  REQUIRE(next.reg().midiSoundMap.size() == 2);
  CHECK(next.reg().midiSoundMap.at(1).ampId == "amp_1");
  CHECK(next.reg().midiSoundMap.at(2).ampId == "amp_1");

  // Clearing a slot is a change too, and it must not be undone by the other
  // writer's stale copy of the map.
  a.ClearMidiSlot(1);
  REQUIRE(a.Save());
  b.SetMidiSlot(3, "amp_1", "");
  REQUIRE(b.Save());

  ContentStore after(base);
  REQUIRE(after.Load());
  CHECK(after.reg().midiSoundMap.count(1) == 0);
  CHECK(after.reg().midiSoundMap.count(2) == 1);
  CHECK(after.reg().midiSoundMap.count(3) == 1);
}

TEST_CASE("Concurrent saves from two threads keep the library parseable and complete")
{
  // The cross-process lock is per handle and does not serialize threads inside
  // one process, so two plugin instances on two host threads need the in-process
  // mutex as well. Without it this test corrupts the file or loses an item.
  const auto base = TestBase("two-writer-threads");

  ContentStore a(base);
  ContentStore b(base);
  REQUIRE(a.Load());
  REQUIRE(b.Load());

  std::atomic<int> failures{0};
  auto writer = [&failures](ContentStore& store, const char* prefix) {
    for (int i = 0; i < 25; ++i)
    {
      IRItem ir;
      ir.id = std::string(prefix) + std::to_string(i);
      ir.name = ir.id;
      ir.file = "ir/" + ir.id + ".wav";
      {
        std::lock_guard<std::recursive_mutex> guard(ContentStoreMutex());
        store.reg().irs.push_back(ir);
      }
      if (!store.Save())
        ++failures;
    }
  };

  std::thread ta([&] { writer(a, "ir_a_"); });
  std::thread tb([&] { writer(b, "ir_b_"); });
  ta.join();
  tb.join();
  CHECK(failures.load() == 0);

  ContentStore next(base);
  REQUIRE(next.Load()); // a clean (non-healed) load: the file is well-formed
  CHECK(next.reg().irs.size() == 50);
}

TEST_CASE("A second EnsureLoaded does not read over an unflushed catalog")
{
  // Every plugin instance's constructor reaches the process-global store. When
  // that call was an unconditional Load(), opening a second VoLum threw away an
  // import or a preset the first one had not flushed yet, and the next Save()
  // persisted the version without it.
  const auto base = TestBase("ensure-loaded");

  ContentStore store(base);
  store.reg().irs.push_back({"ir_saved", "Saved", "ir/saved.wav"});
  REQUIRE(store.Save());

  store.reg().irs.push_back({"ir_unflushed", "Not saved yet", "ir/pending.wav"});
  CHECK(store.HasUnflushedChanges());

  REQUIRE(store.EnsureLoaded()); // second instance's constructor
  REQUIRE(store.reg().irs.size() == 2);
  CHECK(store.reg().irs[1].id == "ir_unflushed");
  CHECK(store.HasUnflushedChanges());

  // The unflushed item then reaches disk on the next save.
  REQUIRE(store.Save());
  CHECK_FALSE(store.HasUnflushedChanges());
  ContentStore next(base);
  REQUIRE(next.Load());
  CHECK(next.reg().irs.size() == 2);
}

TEST_CASE("EnsureLoaded still performs the first read")
{
  const auto base = TestBase("ensure-loaded-first");
  {
    ContentStore seed(base);
    seed.reg().irs.push_back({"ir_seed", "Seed", "ir/seed.wav"});
    REQUIRE(seed.Save());
  }

  ContentStore store(base);
  CHECK_FALSE(store.IsLoaded());
  REQUIRE(store.EnsureLoaded());
  CHECK(store.IsLoaded());
  REQUIRE(store.reg().irs.size() == 1);
  CHECK(store.reg().irs[0].id == "ir_seed");
}

TEST_CASE("A failed save stays pending and is carried by the next successful one")
{
  // The merge replays the difference between this writer's catalog and the last
  // state it knows is on disk. A save that did not reach the disk must therefore
  // leave that baseline alone: advancing it anyway makes the failed write look
  // like a write that happened, and the item is silently dropped from the next
  // change set - the user's import is gone with no error anywhere.
  const auto base = TestBase("two-writer-save-failure");

  ContentStore store(base);
  store.reg().irs.push_back({"ir_first", "First", "ir/first.wav"});
  REQUIRE(store.Save());
  CHECK_FALSE(store.HasUnflushedChanges());

  // Make the library file unwritable the way a permissions change or a backup
  // agent does: readable, so the store does not latch RegistryUnreadable, but
  // impossible to replace by rename.
  std::filesystem::permissions(store.RegistryPath(), std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::replace);

  store.reg().irs.push_back({"ir_pending", "Pending", "ir/pending.wav"});
  CHECK_FALSE(store.Save());
  CHECK(store.TakeWriteFailure());
  CHECK_FALSE(store.TakeWriteFailure()); // a banner is shown once

  // The item is still in memory - the user's edit is not silently dropped - and
  // the store still knows it is not on disk.
  CHECK(store.reg().irs.size() == 2);
  CHECK(store.HasUnflushedChanges());

  std::filesystem::permissions(store.RegistryPath(), std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace);
  REQUIRE(store.Save());
  CHECK_FALSE(store.HasUnflushedChanges());

  ContentStore next(base);
  REQUIRE(next.Load());
  REQUIRE(next.reg().irs.size() == 2);
  CHECK(next.reg().irs[1].id == "ir_pending");
}

TEST_CASE("An unreadable library is never overwritten, and the failure is visible")
{
  const auto base = TestBase("two-writer-unreadable");

  // A directory where the library file belongs: present, but unreadable as a
  // registry. The store must refuse to write rather than replace it.
  std::error_code ec;
  ContentStore store(base);
  std::filesystem::create_directories(store.RegistryPath(), ec);
  REQUIRE_FALSE(ec);

  CHECK_FALSE(store.Load());
  CHECK(store.RegistryUnreadable());

  store.reg().irs.push_back({"ir_doomed", "Doomed", "ir/doomed.wav"});
  CHECK_FALSE(store.Save());
  CHECK(store.TakeWriteFailure());
  CHECK(std::filesystem::is_directory(store.RegistryPath())); // untouched
}

TEST_CASE("The library lock is exclusive while held")
{
  const auto base = TestBase("lock-exclusive");
  std::error_code ec;
  std::filesystem::create_directories(base, ec);
  const auto lockFile = base / "volum-content.lock";

  RegistryFileLock first;
  REQUIRE(first.Acquire(lockFile));
  CHECK(first.Held());

  RegistryFileLock second;
  CHECK_FALSE(second.Acquire(lockFile, 50)); // contended: gives up, does not hang
  CHECK_FALSE(second.Held());

  first.Release();
  CHECK(second.Acquire(lockFile, 50));
}

TEST_CASE("A killed lock holder does not stuck-lock the library")
{
  // The lock has to die with the process. A pid file or a cookie does not: a
  // VoLum that crashes or is force-quit while holding it would leave the library
  // unwritable for everyone until someone deleted a stale file by hand.
  const auto base = TestBase("lock-killed-holder");
  ContentStore victim(base);
  victim.reg().irs.push_back({"ir_before", "Before", "ir/before.wav"});
  REQUIRE(victim.Save());

  RegistryFileLock holder;
  REQUIRE(holder.Acquire(victim.LockPath()));
  holder.SimulateProcessDeath(); // handle goes away with no orderly unlock

  // The lock file itself is still on disk; that must not matter.
  CHECK(std::filesystem::exists(victim.LockPath()));

  ContentStore next(base);
  REQUIRE(next.Load());
  next.reg().irs.push_back({"ir_after", "After", "ir/after.wav"});
  REQUIRE(next.Save());
  CHECK_FALSE(next.TakeWriteFailure());

  ContentStore verify(base);
  REQUIRE(verify.Load());
  CHECK(verify.reg().irs.size() == 2);
}

TEST_CASE("The lock lives beside the registry, never on it")
{
  // The registry is replaced by rename on every write, so a lock taken on it
  // would be a lock on a file that no longer exists.
  ContentStore store(std::filesystem::path("/tmp/volum-lock-path"));
  CHECK(store.LockPath() != store.RegistryPath());
  CHECK(store.LockPath().parent_path() == store.RegistryPath().parent_path());
}

TEST_CASE("MIDI sound map reader ignores unknown keys and malformed slots")
{
  nlohmann::json j;
  j["schemaVersion"] = kContentSchemaVersion;
  j["someFutureTopLevelKey"] = 42; // forward-compatible: ignored, not fatal
  j["midiSoundMap"] = nlohmann::json::array();
  j["midiSoundMap"].push_back(
    {{"slot", 4}, {"ampId", "amp_x"}, {"presetId", "preset_x"}, {"unknownPerSlotKey", "ignored"}});
  j["midiSoundMap"].push_back({{"ampId", "amp_y"}}); // no slot: skipped
  j["midiSoundMap"].push_back({{"slot", 5}}); // no ampId: skipped
  j["midiSoundMap"].push_back({{"slot", "not a number"}, {"ampId", "amp_z"}}); // skipped
  j["midiSoundMap"].push_back({{"slot", 6}, {"ampId", "amp_q"}}); // amp-only Sound

  bool healed = false;
  const Registry r = RegistryFromJson(j, &healed);
  REQUIRE(r.midiSoundMap.size() == 2);
  CHECK(r.midiSoundMap.at(4).ampId == "amp_x");
  CHECK(r.midiSoundMap.at(4).presetId == "preset_x");
  CHECK(r.midiSoundMap.at(6).ampId == "amp_q");
  CHECK(r.midiSoundMap.at(6).presetId.empty());
  CHECK(r.midiSoundMap.count(5) == 0);
}

TEST_CASE("MIDI slot resolution reports gone content as invalid, never as empty")
{
  // Deleting content can only ever turn a row red. Renumbering the rows below it
  // would hand the player's footswitch a different sound than the one it had.
  Registry r;
  volum::custom::CustomAmp amp;
  amp.id = "amp_live";
  r.amps.push_back(amp);
  Preset pr;
  pr.id = "preset_live";
  pr.name = "Live";
  r.presetBanks["amp_live"] = {pr};

  r.midiSoundMap[1] = MidiSoundAssignment{"amp_live", "preset_live"};
  r.midiSoundMap[2] = MidiSoundAssignment{"amp_live", ""}; // amp-only
  r.midiSoundMap[3] = MidiSoundAssignment{"amp_gone", "preset_live"};
  r.midiSoundMap[4] = MidiSoundAssignment{"amp_live", "preset_gone"};
  r.midiSoundMap[5] = MidiSoundAssignment{FactoryOwnerKey(2), FactoryPresetId(2)};
  r.midiSoundMap[6] = MidiSoundAssignment{FactoryOwnerKey(99), ""}; // beyond the bank

  CHECK(ResolveMidiSlot(r, 1, 12) == MidiSlotState::Valid);
  CHECK(ResolveMidiSlot(r, 2, 12) == MidiSlotState::Valid);
  CHECK(ResolveMidiSlot(r, 3, 12) == MidiSlotState::Invalid);
  CHECK(ResolveMidiSlot(r, 4, 12) == MidiSlotState::Invalid);
  CHECK(ResolveMidiSlot(r, 5, 12) == MidiSlotState::Valid);
  CHECK(ResolveMidiSlot(r, 6, 12) == MidiSlotState::Invalid);
  CHECK(ResolveMidiSlot(r, 7, 12) == MidiSlotState::Unassigned);
}

TEST_CASE("Deleting content leaves its MIDI slot assigned but invalid")
{
  ContentStore store;
  volum::custom::CustomAmp amp;
  amp.id = "amp_doomed";
  store.reg().amps.push_back(amp);
  store.reg().irs.push_back({"ir_1", "Mesa", ""});
  store.SetMidiSlot(9, "amp_doomed", "");
  REQUIRE(ResolveMidiSlot(store.reg(), 9, 12) == MidiSlotState::Valid);

  store.RemoveCustomAmp("amp_doomed");
  REQUIRE(store.reg().midiSoundMap.count(9) == 1); // the number stays with the pedalboard
  CHECK(ResolveMidiSlot(store.reg(), 9, 12) == MidiSlotState::Invalid);
}

TEST_CASE("The registry no longer writes shared custom scenes")
{
  // 1.2.0 kept per-amp scenes in the library, so two instances editing the same
  // custom amp moved each other's knobs. Scenes are per-instance state in 1.3.0;
  // the field is read once for migration and never written again.
  Registry r;
  volum::custom::CustomAmp amp;
  amp.id = "amp_a";
  r.amps.push_back(amp);
  VoLumAmpSettings scene;
  scene.outputLevel = -3.0;
  r.legacyCustomScenes["amp_a"] = scene;

  const auto j = RegistryToJson(r);
  CHECK_FALSE(j.contains("customScenes"));
  CHECK(j["schemaVersion"].get<int>() == kContentSchemaVersion);

  // A pre-1.3.0 file's scenes are still readable, so the instance that first
  // focuses the amp can adopt them.
  nlohmann::json old;
  old["schemaVersion"] = 3;
  old["customScenes"] = nlohmann::json::object();
  old["customScenes"]["amp_a"] = volum::AmpSettingsToJson(scene);
  bool healed = false;
  const Registry migrated = RegistryFromJson(old, &healed);
  REQUIRE(migrated.legacyCustomScenes.count("amp_a") == 1);
  CHECK(migrated.legacyCustomScenes.at("amp_a").outputLevel == doctest::Approx(-3.0));
}
