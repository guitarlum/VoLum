#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>

#include "../VoLumContentStore.h"

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

TEST_CASE("Registry round-trips amps, IRs, pedals, presets, and scenes")
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

  VoLumAmpSettings scene;
  scene.outputLevel = -3.0;
  scene.supportCustomId = "amp_def";
  r.customScenes["amp_abc"] = scene;

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

  REQUIRE(loaded.customScenes.count("amp_abc") == 1);
  CHECK(loaded.customScenes.at("amp_abc").outputLevel == doctest::Approx(-3.0));
  CHECK(loaded.customScenes.at("amp_abc").supportCustomId == "amp_def");
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

TEST_CASE("Removal matrix: deleting a pedal clears referencing PRE slots")
{
  ContentStore store;
  store.reg().pedals.push_back({"pedal_1", "Klon", "klon", "", 64});
  VoLumAmpSettings scene;
  scene.preNam1Capture = 64;
  scene.preNam2Capture = 5; // factory, untouched
  store.reg().customScenes["amp_a"] = scene;
  Preset pr;
  pr.id = "preset_1";
  pr.settings.preNam2Capture = 64;
  store.reg().presetBanks["factory:0"] = {pr};

  store.RemovePedal("pedal_1");
  CHECK(store.reg().pedals.empty());
  CHECK(store.reg().customScenes["amp_a"].preNam1Capture == 0);
  CHECK(store.reg().customScenes["amp_a"].preNam2Capture == 5);
  CHECK(store.reg().presetBanks["factory:0"][0].settings.preNam2Capture == 0);
}

TEST_CASE("Removal matrix: deleting an IR clears activeIrId references")
{
  ContentStore store;
  store.reg().irs.push_back({"ir_1", "Mesa", ""});
  VoLumAmpSettings scene;
  scene.activeIrId = "ir_1";
  store.reg().customScenes["amp_a"] = scene;

  store.RemoveIR("ir_1");
  CHECK(store.reg().irs.empty());
  CHECK(store.reg().customScenes["amp_a"].activeIrId.empty());
}

TEST_CASE("Removal matrix: deleting a custom amp cascades bank/scene and clears support refs")
{
  ContentStore store;
  volum::custom::CustomAmp amp;
  amp.id = "amp_main";
  store.reg().amps.push_back(amp);
  store.reg().presetBanks["amp_main"] = {Preset{"preset_1", "P", {}}};
  store.reg().customScenes["amp_main"] = VoLumAmpSettings{};

  VoLumAmpSettings other;
  other.supportCustomId = "amp_main";
  store.reg().customScenes["amp_other"] = other;

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
  CHECK(store.reg().customScenes.count("amp_main") == 0);
  CHECK(store.reg().customScenes["amp_other"].supportCustomId.empty());
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
    VoLumAmpSettings scene;
    scene.activeIrId = "ir_drop";
    store.reg().customScenes["amp_a"] = scene;
    Preset pr;
    pr.id = "preset_1";
    pr.settings.activeIrId = "ir_drop";
    store.reg().presetBanks["factory:0"] = {pr};

    store.RemoveIR(store.reg().irs[1].id); // reference into the erased element
    CHECK(store.reg().irs.size() == 1);
    CHECK(store.reg().irs[0].id == "ir_keep");
    CHECK(store.reg().customScenes["amp_a"].activeIrId.empty());
    CHECK(store.reg().presetBanks["factory:0"][0].settings.activeIrId.empty());
  }

  SUBCASE("pedal")
  {
    ContentStore store;
    store.reg().pedals.push_back({"pedal_drop", "Klon", "klon", "", 64});
    VoLumAmpSettings scene;
    scene.preNam1Capture = 64;
    store.reg().customScenes["amp_a"] = scene;

    store.RemovePedal(store.reg().pedals[0].id); // reference into the erased element
    CHECK(store.reg().pedals.empty());
    CHECK(store.reg().customScenes["amp_a"].preNam1Capture == 0);
  }

  SUBCASE("custom amp")
  {
    ContentStore store;
    volum::custom::CustomAmp amp;
    amp.id = "amp_drop";
    store.reg().amps.push_back(amp);
    store.reg().presetBanks["amp_drop"] = {Preset{"preset_1", "P", {}}};
    store.reg().customScenes["amp_drop"] = VoLumAmpSettings{};
    VoLumAmpSettings other;
    other.supportCustomId = "amp_drop";
    store.reg().customScenes["amp_other"] = other;

    store.RemoveCustomAmp(store.reg().amps[0].id); // reference into the erased element
    CHECK(store.reg().amps.empty());
    CHECK(store.reg().presetBanks.count("amp_drop") == 0);
    CHECK(store.reg().customScenes.count("amp_drop") == 0);
    CHECK(store.reg().customScenes["amp_other"].supportCustomId.empty());
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

  // Per-amp scene wiring the custom IR + pedal into the live rig.
  VoLumAmpSettings scene;
  scene.channelIdx = 2;
  scene.activeIrId = "ir_byo";
  scene.preNam1Active = true;
  scene.preNam1Capture = kCustomPedalIndexBase;
  store.reg().customScenes["amp_byo"] = scene;

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

  // Per-amp scene keeps its custom IR + pedal wiring.
  REQUIRE(r.customScenes.count("amp_byo") == 1);
  CHECK(r.customScenes.at("amp_byo").activeIrId == "ir_byo");
  CHECK(r.customScenes.at("amp_byo").preNam1Capture == kCustomPedalIndexBase);

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
