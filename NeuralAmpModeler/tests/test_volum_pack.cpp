#include "third_party/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../VoLumPack.h"
#include "../VoLumPackArchive.h"

using namespace volum::pack;
using volum::content::ContentStore;
using volum::content::IRItem;
using volum::content::kCustomPedalIndexBase;
using volum::content::MidiSoundAssignment;
using volum::content::PedalItem;
using volum::content::Preset;
using volum::content::Registry;
using volum::custom::CustomAmp;
using volum::custom::CustomNamFile;
using volum::custom::kDirectSlot;

namespace
{
std::filesystem::path TestBase(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-pack-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}

std::filesystem::path WriteSrc(const std::filesystem::path& dir, const char* leaf, const std::string& body)
{
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const auto p = dir / leaf;
  std::ofstream out(p, std::ios::binary);
  out << body;
  out.close();
  return p;
}

// A library with one custom amp (one capture), one IR, one pedal, and a preset in
// the amp's bank that references all three. Enough for closure, replace-in-use and
// Reset to have something to be wrong about.
struct Library
{
  std::filesystem::path base;
  ContentStore store;
  std::string ampStoredPath;

  explicit Library(const char* name, const std::string& tag = "a")
  : base(TestBase(name))
  , store(base)
  {
    const auto ampSrc = WriteSrc(base / "incoming", "Plexi.nam", "NAM-amp-" + tag);
    const auto irSrc = WriteSrc(base / "incoming", "Mesa.wav", "RIFF-ir-" + tag);
    const auto pedalSrc = WriteSrc(base / "incoming", "Klon.nam", "NAM-pedal-" + tag);

    CustomAmp amp;
    amp.id = "amp_one";
    amp.name = "Plexi";
    CustomNamFile f;
    f.file = "Plexi.nam";
    f.slot = kDirectSlot;
    f.channel = 1;
    // The tag rides the stored file names too: two libraries built by this fixture
    // must not accidentally share a payload path, or an import would look like it
    // moved a file when it only overwrote one.
    f.storedPath = store.ImportFileCopy(ampSrc, "amps", "amp_one_0_" + tag);
    amp.files = {f};
    ampStoredPath = f.storedPath;
    store.reg().amps.push_back(amp);

    IRItem ir;
    ir.id = "ir_one";
    ir.name = "Mesa OS";
    ir.file = store.ImportFileCopy(irSrc, "ir", "ir_one_" + tag);
    store.reg().irs.push_back(ir);

    PedalItem pedal;
    pedal.id = "pedal_one";
    pedal.name = "Klon";
    pedal.group = "klon";
    pedal.file = store.ImportFileCopy(pedalSrc, "pedals", "pedal_one_" + tag);
    pedal.legacyIndex = kCustomPedalIndexBase;
    store.reg().pedals.push_back(pedal);
    store.reg().nextPedalIndex = kCustomPedalIndexBase + 1;

    Preset pr;
    pr.id = "preset_one";
    pr.name = "Lead";
    pr.settings.activeIrId = "ir_one";
    pr.settings.preNam1Capture = kCustomPedalIndexBase;
    store.reg().presetBanks["amp_one"] = {pr};

    REQUIRE(store.Save());
  }
};

ExportPlan EverythingPlan(const Registry& r)
{
  ExportSelection sel;
  sel.everything = true;
  return BuildExportPlan(r, sel);
}

bool Mentions(const std::vector<std::string>& list, const std::string& needle)
{
  return std::any_of(
    list.begin(), list.end(), [&needle](const std::string& s) { return s.find(needle) != std::string::npos; });
}
} // namespace

// ---------------------------------------------------------------------------
// Archive: the zip half
// ---------------------------------------------------------------------------

TEST_CASE("A Pack is a STORE-method zip that round-trips its entries byte for byte")
{
  std::vector<ArchiveEntry> entries;
  entries.push_back({"manifest.json", "{\"contractVersion\":1}"});
  entries.push_back({"payload/amps/a.nam", std::string("\0\1\2binary\xff", 10)});
  entries.push_back({"payload/ir/b.wav", std::string(70000, 'x')}); // spans buffer sizes

  const std::string blob = BuildArchive(entries);
  REQUIRE_FALSE(blob.empty());
  // Local file header, and method 0 at offset 8.
  CHECK(blob.compare(0, 4, "PK\x03\x04") == 0);
  CHECK(static_cast<unsigned char>(blob[8]) == 0);
  CHECK(static_cast<unsigned char>(blob[9]) == 0);

  const auto read = ParseArchive(blob);
  REQUIRE(read.ok);
  REQUIRE(read.entries.size() == 3);
  for (const auto& e : entries)
  {
    const std::string* got = read.Find(e.name);
    REQUIRE(got != nullptr);
    CHECK(*got == e.data);
  }
}

TEST_CASE("A corrupt Pack is refused with a reason, never read as a short one")
{
  std::vector<ArchiveEntry> entries;
  entries.push_back({"manifest.json", "{\"contractVersion\":1}"});
  entries.push_back({"payload/ir/b.wav", "RIFFbody"});
  const std::string good = BuildArchive(entries);
  REQUIRE(ParseArchive(good).ok);

  SUBCASE("a flipped payload byte fails the checksum")
  {
    std::string bad = good;
    const size_t at = bad.find("RIFFbody");
    REQUIRE(at != std::string::npos);
    bad[at + 2] = 'X';
    const auto read = ParseArchive(bad);
    CHECK_FALSE(read.ok);
    CHECK(read.entries.empty());
    CHECK(Mentions({read.error}, "corrupt"));
  }
  SUBCASE("a truncated download is not a two-entry Pack minus one")
  {
    const auto read = ParseArchive(good.substr(0, good.size() / 2));
    CHECK_FALSE(read.ok);
    CHECK(read.entries.empty());
  }
  SUBCASE("garbage is not a Pack at all")
  {
    const auto read = ParseArchive("this is a text file, not a zip");
    CHECK_FALSE(read.ok);
    CHECK(Mentions({read.error}, "not a Pack"));
  }
}

TEST_CASE("An entry name that escapes the archive root is refused on write and on read")
{
  CHECK(BuildArchive({{"../../evil.nam", "x"}}).empty());
  CHECK(BuildArchive({{"/etc/passwd", "x"}}).empty());
  CHECK(BuildArchive({{"payload/a/../../../b.nam", "x"}}).empty());
  CHECK_FALSE(BuildArchive({{"payload/amps/ok.nam", "x"}}).empty());

  // Hand-built archive whose central directory names a traversing path: a Pack
  // from a hostile source, which the reader has to refuse on its own.
  std::string blob = BuildArchive({{"payload/amps/ok.nam", "x"}});
  const size_t at = blob.find("payload/amps/ok.nam");
  REQUIRE(at != std::string::npos);
  blob.replace(at, 19, "payload/amps/../../x");
  const auto read = ParseArchive(blob);
  CHECK_FALSE(read.ok);
}

// ---------------------------------------------------------------------------
// Export: closure
// ---------------------------------------------------------------------------

TEST_CASE("Export closure auto-includes what a selection references")
{
  Library lib("export-closure");
  const Registry& r = lib.store.reg();

  SUBCASE("selecting an amp brings its bank, and the bank's IR and pedal")
  {
    ExportSelection sel;
    sel.everything = false;
    sel.ampIds = {"amp_one"};
    const auto plan = BuildExportPlan(r, sel);
    CHECK(plan.job == Job::Share);
    CHECK(plan.ampIds == std::vector<std::string>{"amp_one"});
    CHECK(plan.presetIds == std::vector<std::string>{"preset_one"});
    CHECK(plan.irIds == std::vector<std::string>{"ir_one"});
    CHECK(plan.pedalIds == std::vector<std::string>{"pedal_one"});
    // The preview names the companions; a requirement is not the user's choice.
    CHECK(Mentions(plan.alsoIncluding, "Mesa OS"));
    CHECK(Mentions(plan.alsoIncluding, "Klon"));
    CHECK(Mentions(plan.alsoIncluding, "Lead"));
  }

  SUBCASE("a preset on a factory amp packs its IR and pedal but no factory capture")
  {
    Preset pr;
    pr.id = "preset_factory";
    pr.name = "Factory lead";
    pr.settings.activeIrId = "ir_one";
    pr.settings.preNam2Capture = kCustomPedalIndexBase;
    lib.store.reg().presetBanks[volum::content::FactoryOwnerKey(7)] = {pr};

    ExportSelection sel;
    sel.everything = false;
    sel.presetIds = {"preset_factory"};
    const auto plan = BuildExportPlan(lib.store.reg(), sel);
    CHECK(plan.ampIds.empty()); // the factory capture ships with VoLum
    CHECK(plan.irIds == std::vector<std::string>{"ir_one"});
    CHECK(plan.pedalIds == std::vector<std::string>{"pedal_one"});
  }

  SUBCASE("a support partner is pulled in transitively, with its own requirements")
  {
    const auto partnerSrc = WriteSrc(lib.base / "incoming", "Partner.nam", "NAM-partner");
    const auto irSrc = WriteSrc(lib.base / "incoming", "Partner.wav", "RIFF-partner");
    CustomAmp partner;
    partner.id = "amp_partner";
    partner.name = "Partner";
    CustomNamFile f;
    f.file = "Partner.nam";
    f.slot = kDirectSlot;
    f.channel = 1;
    f.storedPath = lib.store.ImportFileCopy(partnerSrc, "amps", "amp_partner_0");
    partner.files = {f};
    lib.store.reg().amps.push_back(partner);

    IRItem partnerIr;
    partnerIr.id = "ir_partner";
    partnerIr.name = "Partner IR";
    partnerIr.file = lib.store.ImportFileCopy(irSrc, "ir", "ir_partner");
    lib.store.reg().irs.push_back(partnerIr);

    Preset partnerPreset;
    partnerPreset.id = "preset_partner";
    partnerPreset.name = "Partner tone";
    partnerPreset.settings.activeIrId = "ir_partner";
    lib.store.reg().presetBanks["amp_partner"] = {partnerPreset};

    lib.store.reg().presetBanks["amp_one"][0].settings.supportCustomId = "amp_partner";

    ExportSelection sel;
    sel.everything = false;
    sel.ampIds = {"amp_one"};
    const auto plan = BuildExportPlan(lib.store.reg(), sel);
    CHECK(Mentions(plan.ampIds, "amp_partner"));
    // ... and the partner's own bank came with it, IR and all.
    CHECK(Mentions(plan.presetIds, "preset_partner"));
    CHECK(Mentions(plan.irIds, "ir_partner"));
  }

  SUBCASE("an unreferenced IR travels only in Everything")
  {
    IRItem spare;
    spare.id = "ir_spare";
    spare.name = "Unused";
    spare.file = lib.store.ImportFileCopy(WriteSrc(lib.base / "incoming", "Spare.wav", "RIFF-spare"), "ir", "ir_spare");
    lib.store.reg().irs.push_back(spare);

    ExportSelection share;
    share.everything = false;
    share.ampIds = {"amp_one"};
    CHECK_FALSE(Mentions(BuildExportPlan(lib.store.reg(), share).irIds, "ir_spare"));
    CHECK(Mentions(EverythingPlan(lib.store.reg()).irIds, "ir_spare"));
  }
}

TEST_CASE("Pack export and import labels name the amp a preset belongs to")
{
  Library lib("preset-amp-label");
  const Registry& r = lib.store.reg();
  CHECK(OwnerDisplayName(r, "amp_one") == "Plexi");
  CHECK(PresetWithAmpLabel(r, "Lead", "amp_one") == "Preset \"Lead\"  ·  Plexi");
  CHECK(OwnerDisplayName(r, volum::content::FactoryOwnerKey(7)) == volum::kAmps[7].displayName);
  CHECK(OwnerDisplayName(r, "missing-amp") == "missing-amp");

  ExportSelection sel;
  sel.everything = false;
  sel.ampIds = {"amp_one"};
  CHECK(Mentions(BuildExportPlan(r, sel).alsoIncluding, "Preset \"Lead\"  ·  Plexi"));

  PackContents pack;
  pack.ok = true;
  pack.library = r;
  CHECK(Mentions(BuildImportPreview(r, pack, ImportVerb::Overwrite, false, true).replaces,
                 "Preset \"Lead\"  ·  Plexi"));
}

TEST_CASE("A Share Pack carries no settings and no MIDI sound map; Everything carries both")
{
  Library lib("export-payload");
  lib.store.reg().midiSoundMap[5] = MidiSoundAssignment{"amp_one", "preset_one"};

  const std::string settings = "{\"volumLastAmp\":3}";

  SUBCASE("Share")
  {
    ExportSelection sel;
    sel.everything = false;
    sel.ampIds = {"amp_one"};
    const auto plan = BuildExportPlan(lib.store.reg(), sel);
    CHECK_FALSE(plan.includeSettings);
    CHECK_FALSE(plan.includeMidiSoundMap);
    CHECK(PackRegistrySubset(lib.store.reg(), plan).midiSoundMap.empty());

    const auto out = lib.base / "share.volumpack";
    std::string err;
    REQUIRE(WritePack(lib.store, plan, settings, out, &err));
    const auto archive = ReadArchiveFromFile(out);
    REQUIRE(archive.ok);
    CHECK(archive.Find(kSettingsEntry) == nullptr);

    const auto contents = OpenPack(out);
    REQUIRE(contents.ok);
    CHECK(contents.job == Job::Share);
    CHECK(contents.settingsJson.empty());
    CHECK_FALSE(contents.includesMidiSoundMap);
    CHECK(contents.library.midiSoundMap.empty());
  }

  SUBCASE("Everything")
  {
    const auto plan = EverythingPlan(lib.store.reg());
    const auto out = lib.base / "all.volumpack";
    std::string err;
    REQUIRE(WritePack(lib.store, plan, settings, out, &err));

    const auto contents = OpenPack(out);
    REQUIRE(contents.ok);
    CHECK(contents.job == Job::Everything);
    CHECK(contents.settingsJson == settings);
    CHECK(contents.includesMidiSoundMap);
    REQUIRE(contents.library.midiSoundMap.count(5) == 1);
    CHECK(contents.library.midiSoundMap.at(5).ampId == "amp_one");
  }
}

TEST_CASE("A Share Pack that arrives with settings or a MIDI map still imports as Share")
{
  // A future build, or a hand-edited Pack, could put the extra entries in a Share
  // archive. The job in the manifest is the promise made to the user, so the
  // reader enforces it instead of trusting the payload.
  Library lib("share-enforced");
  lib.store.reg().midiSoundMap[9] = MidiSoundAssignment{"amp_one", ""};
  ExportSelection sel;
  sel.everything = false;
  sel.ampIds = {"amp_one"};
  auto plan = BuildExportPlan(lib.store.reg(), sel);

  std::string err;
  auto entries = BuildPackEntries(lib.store, plan, "", &err);
  REQUIRE_FALSE(entries.empty());
  // Splice in what a Share Pack must not carry.
  Registry withMap = lib.store.reg();
  for (auto& e : entries)
    if (e.name == kLibraryEntry)
      e.data = volum::content::RegistryToJson(withMap).dump();
  entries.push_back({kSettingsEntry, "{\"volumLastAmp\":11}"});

  const auto contents = ReadPackFromArchive(ParseArchive(BuildArchive(entries)));
  REQUIRE(contents.ok);
  CHECK(contents.job == Job::Share);
  CHECK(contents.settingsJson.empty());
  CHECK(contents.library.midiSoundMap.empty());
}

TEST_CASE("Export refuses when a capture file is missing rather than packing a hole")
{
  Library lib("export-missing-file");
  std::error_code ec;
  std::filesystem::remove(lib.store.ResolveStored(lib.ampStoredPath), ec);

  std::string err;
  const auto out = lib.base / "broken.volumpack";
  CHECK_FALSE(WritePack(lib.store, EverythingPlan(lib.store.reg()), "", out, &err));
  CHECK(Mentions({err}, "Could not read"));
  CHECK_FALSE(std::filesystem::exists(out));
}

// ---------------------------------------------------------------------------
// Contract version
// ---------------------------------------------------------------------------

TEST_CASE("A newer contract is refused by name; an older one always imports")
{
  Library lib("contract");
  std::string err;
  auto entries = BuildPackEntries(lib.store, EverythingPlan(lib.store.reg()), "", &err);
  REQUIRE_FALSE(entries.empty());

  auto withContract = [&entries](int version) {
    auto copy = entries;
    for (auto& e : copy)
      if (e.name == kManifestEntry)
      {
        auto j = nlohmann::json::parse(e.data);
        j["contractVersion"] = version;
        e.data = j.dump();
      }
    return ReadPackFromArchive(ParseArchive(BuildArchive(copy)));
  };

  const auto newer = withContract(kContractVersion + 1);
  CHECK_FALSE(newer.ok);
  CHECK(Mentions({newer.error}, kContractFirstApp));
  CHECK(Mentions({newer.error}, "needs VoLum"));

  CHECK(withContract(kContractVersion).ok);

  SUBCASE("same contract, unknown keys from a newer build are ignored")
  {
    auto copy = entries;
    for (auto& e : copy)
      if (e.name == kManifestEntry)
      {
        auto j = nlohmann::json::parse(e.data);
        j["chorusPresets"] = {{"id", "x"}};
        e.data = j.dump();
      }
      else if (e.name == kLibraryEntry)
      {
        auto j = nlohmann::json::parse(e.data);
        j["someFutureList"] = nlohmann::json::array({1, 2, 3});
        e.data = j.dump();
      }
    const auto contents = ReadPackFromArchive(ParseArchive(BuildArchive(copy)));
    REQUIRE(contents.ok);
    CHECK(contents.library.amps.size() == 1);
  }
}

TEST_CASE("A Pack whose manifest promises a file it does not carry is incomplete")
{
  Library lib("incomplete");
  std::string err;
  auto entries = BuildPackEntries(lib.store, EverythingPlan(lib.store.reg()), "", &err);
  REQUIRE_FALSE(entries.empty());
  entries.erase(
    std::remove_if(entries.begin(), entries.end(),
                   [](const ArchiveEntry& e) { return e.name.rfind(std::string(kPayloadPrefix) + "amps/", 0) == 0; }),
    entries.end());
  const auto contents = ReadPackFromArchive(ParseArchive(BuildArchive(entries)));
  CHECK_FALSE(contents.ok);
  CHECK(Mentions({contents.error}, "incomplete"));
}

// ---------------------------------------------------------------------------
// Share import
// ---------------------------------------------------------------------------

namespace
{
// Export from `from`, then read it back ready to import into another library.
PackContents PackFrom(Library& from, const ExportPlan& plan, const std::string& settings = "")
{
  std::string err;
  const auto out = from.base / "out.volumpack";
  REQUIRE_MESSAGE(WritePack(from.store, plan, settings, out, &err), err);
  return OpenPack(out);
}
} // namespace

TEST_CASE("Share import merges by id: new ids add, extras stay")
{
  Library sender("share-add-sender", "sender");
  // Rename the sender's items so nothing collides by id with the receiver's.
  sender.store.reg().amps[0].id = "amp_two";
  sender.store.reg().amps[0].name = "Bassman";
  sender.store.reg().presetBanks["amp_two"] = sender.store.reg().presetBanks["amp_one"];
  sender.store.reg().presetBanks.erase("amp_one");
  sender.store.reg().presetBanks["amp_two"][0].id = "preset_two";
  sender.store.reg().irs[0].id = "ir_two";
  sender.store.reg().irs[0].name = "Bassman IR";
  sender.store.reg().presetBanks["amp_two"][0].settings.activeIrId = "ir_two";
  sender.store.reg().pedals[0].id = "pedal_two";
  sender.store.reg().pedals[0].name = "Tube Screamer";
  REQUIRE(sender.store.Save());

  ExportSelection sel;
  sel.everything = false;
  sel.ampIds = {"amp_two"};
  const auto pack = PackFrom(sender, BuildExportPlan(sender.store.reg(), sel));
  REQUIRE(pack.ok);

  Library receiver("share-add-receiver", "recv");
  const auto preview = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, false, true);
  CHECK(Mentions(preview.adds, "Bassman"));
  CHECK(preview.replaces.empty());
  CHECK(preview.removals.empty());

  const auto result = ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true);
  REQUIRE_MESSAGE(result.ok, result.error);

  ContentStore reloaded(receiver.base);
  REQUIRE(reloaded.Load());
  CHECK(reloaded.reg().amps.size() == 2); // mine stayed
  CHECK(reloaded.reg().irs.size() == 2);
  CHECK(reloaded.reg().pedals.size() == 2);
  CHECK(reloaded.reg().presetBanks.count("amp_one") == 1);
  CHECK(reloaded.reg().presetBanks.count("amp_two") == 1);
  // The imported capture landed in the library and is the sender's bytes.
  std::string body;
  REQUIRE(ReadWholeFile(reloaded.ResolveStored(sender.ampStoredPath), body));
  CHECK(body == "NAM-amp-sender");
  // An imported pedal's index is claimed, so the next local import cannot reuse it.
  CHECK(reloaded.reg().nextPedalIndex > kCustomPedalIndexBase);
}

TEST_CASE("Same-id import replaces that item; the same name with a different id keeps both")
{
  Library sender("share-collide-sender", "sender");
  Library receiver("share-collide-receiver", "recv");

  SUBCASE("same id: one item, the Pack's payload")
  {
    const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
    REQUIRE(pack.ok);
    const auto preview = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, false, true);
    CHECK(Mentions(preview.replaces, "Plexi"));
    CHECK(preview.nameCollisions.empty());
    CHECK(preview.adds.empty());

    const auto result = ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true);
    REQUIRE_MESSAGE(result.ok, result.error);
    CHECK(Mentions(result.replacedIds, "amp_one"));

    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    CHECK(reloaded.reg().amps.size() == 1);
    std::string body;
    REQUIRE(ReadWholeFile(reloaded.ResolveStored(sender.ampStoredPath), body));
    CHECK(body == "NAM-amp-sender"); // the Pack won
  }

  SUBCASE("same name, different id: both survive and the preview warns")
  {
    sender.store.reg().irs[0].id = "ir_other";
    sender.store.reg().irs[0].name = "Mesa OS"; // the receiver's IR name
    REQUIRE(sender.store.Save());
    const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
    REQUIRE(pack.ok);

    const auto preview = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, false, true);
    CHECK(Mentions(preview.nameCollisions, "Mesa OS"));
    CHECK(Mentions(preview.adds, "Mesa OS")); // an add, never a silent replace

    REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true).ok);
    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    CHECK(reloaded.reg().irs.size() == 2);
  }
}

TEST_CASE("Import preview names the in-use ids this instance would have to reload")
{
  Library sender("in-use-sender", "sender");
  Library receiver("in-use-receiver", "recv");
  const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
  REQUIRE(pack.ok);

  // Nothing playing: a replace is silent as far as the rig is concerned.
  CHECK(BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, false, true).inUseReloads.empty());

  // MAIN is playing the id the Pack replaces.
  const auto playing = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, false, true, {"amp_one"});
  CHECK(Mentions(playing.inUseReloads, "Plexi"));

  // Add keeps mine, so there is nothing to reload even while it is playing.
  const auto adding = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Add, false, true, {"amp_one"});
  CHECK(adding.inUseReloads.empty());
  CHECK(adding.replaces.empty());
}

// ---------------------------------------------------------------------------
// FULL import verbs
// ---------------------------------------------------------------------------

TEST_CASE("Overwrite, Add and Reset differ exactly where the ticket says they do")
{
  auto senderPack = [](Library& sender) {
    sender.store.reg().amps[0].name = "Plexi (theirs)";
    REQUIRE(sender.store.Save());
    return PackFrom(sender, EverythingPlan(sender.store.reg()));
  };

  // A local-only item, present in the receiver and absent from every Pack.
  auto addLocalOnly = [](Library& lib) {
    IRItem mine;
    mine.id = "ir_local";
    mine.name = "My own IR";
    mine.file = lib.store.ImportFileCopy(WriteSrc(lib.base / "incoming", "Mine.wav", "RIFF-mine"), "ir", "ir_local");
    lib.store.reg().irs.push_back(mine);
    REQUIRE(lib.store.Save());
    return mine.file;
  };

  SUBCASE("Overwrite: Pack wins, local-only stays")
  {
    Library sender("verb-ow-sender", "sender");
    Library receiver("verb-ow-receiver", "recv");
    const auto mineFile = addLocalOnly(receiver);
    const auto pack = senderPack(sender);
    REQUIRE(pack.ok);

    CHECK(BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, false, true).removals.empty());
    REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true).ok);

    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    REQUIRE(reloaded.reg().amps.size() == 1);
    CHECK(reloaded.reg().amps[0].name == "Plexi (theirs)");
    CHECK(reloaded.reg().irs.size() == 2);
    CHECK(std::filesystem::exists(reloaded.ResolveStored(mineFile)));
  }

  SUBCASE("Add: mine wins, nothing is deleted")
  {
    Library sender("verb-add-sender", "sender");
    Library receiver("verb-add-receiver", "recv");
    addLocalOnly(receiver);
    const auto pack = senderPack(sender);
    REQUIRE(pack.ok);

    const auto preview = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Add, false, true);
    CHECK(preview.replaces.empty());
    CHECK(preview.removals.empty());
    const auto result = ApplyPack(receiver.store, pack, ImportVerb::Add, false, true);
    REQUIRE(result.ok);
    CHECK(result.replacedIds.empty());

    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    REQUIRE(reloaded.reg().amps.size() == 1);
    CHECK(reloaded.reg().amps[0].name == "Plexi"); // mine, untouched
    CHECK(reloaded.reg().irs.size() == 2);
  }

  SUBCASE("Reset: Pack wins and local-only items are deleted, named first")
  {
    Library sender("verb-reset-sender", "sender");
    Library receiver("verb-reset-receiver", "recv");
    const auto mineFile = addLocalOnly(receiver);
    const auto pack = senderPack(sender);
    REQUIRE(pack.ok);

    const auto preview = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Reset, false, true);
    CHECK(Mentions(preview.removals, "My own IR")); // named, not counted
    REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Reset, false, true).ok);

    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    REQUIRE(reloaded.reg().amps.size() == 1);
    CHECK(reloaded.reg().amps[0].name == "Plexi (theirs)");
    REQUIRE(reloaded.reg().irs.size() == 1);
    CHECK(reloaded.reg().irs[0].id == "ir_one");
    CHECK_FALSE(std::filesystem::exists(reloaded.ResolveStored(mineFile)));
  }

  SUBCASE("Reset also drops a local-only preset")
  {
    Library sender("verb-reset2-sender", "sender");
    Library receiver("verb-reset2-receiver", "recv");
    Preset mine;
    mine.id = "preset_local";
    mine.name = "My own tone";
    receiver.store.reg().presetBanks["amp_one"].push_back(mine);
    REQUIRE(receiver.store.Save());
    const auto pack = senderPack(sender);
    REQUIRE(pack.ok);

    CHECK(
      Mentions(BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Reset, false, true).removals, "My own tone"));
    REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Reset, false, true).ok);

    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    REQUIRE(reloaded.reg().presetBanks.count("amp_one") == 1);
    REQUIRE(reloaded.reg().presetBanks.at("amp_one").size() == 1);
    CHECK(reloaded.reg().presetBanks.at("amp_one")[0].id == "preset_one");
  }
}

TEST_CASE("An imported pedal whose PRE index is taken is renumbered, and the Pack's presets follow")
{
  // Two libraries both minted capture index 64 for their first pedal. Merging by id
  // alone would leave two pedals claiming one PRE slot, and the imported preset
  // would play the local pedal - the wrong sound under the right name.
  Library sender("pedal-index-sender", "sender");
  sender.store.reg().pedals[0].id = "pedal_theirs";
  sender.store.reg().pedals[0].name = "Tube Screamer";
  sender.store.reg().amps[0].id = "amp_theirs";
  sender.store.reg().presetBanks["amp_theirs"] = sender.store.reg().presetBanks["amp_one"];
  sender.store.reg().presetBanks.erase("amp_one");
  sender.store.reg().presetBanks["amp_theirs"][0].id = "preset_theirs";
  sender.store.reg().presetBanks["amp_theirs"][0].settings.activeIrId.clear();
  REQUIRE(sender.store.reg().pedals[0].legacyIndex == kCustomPedalIndexBase);
  REQUIRE(sender.store.reg().presetBanks["amp_theirs"][0].settings.preNam1Capture == kCustomPedalIndexBase);
  REQUIRE(sender.store.Save());

  Library receiver("pedal-index-receiver", "recv");
  REQUIRE(receiver.store.reg().pedals[0].legacyIndex == kCustomPedalIndexBase);

  const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
  REQUIRE(pack.ok);
  REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true).ok);

  ContentStore reloaded(receiver.base);
  REQUIRE(reloaded.Load());
  REQUIRE(reloaded.reg().pedals.size() == 2);
  int mineIdx = -1, theirsIdx = -1;
  for (const auto& p : reloaded.reg().pedals)
  {
    if (p.id == "pedal_one")
      mineIdx = p.legacyIndex;
    if (p.id == "pedal_theirs")
      theirsIdx = p.legacyIndex;
  }
  CHECK(mineIdx == kCustomPedalIndexBase); // mine kept its number
  CHECK(theirsIdx > kCustomPedalIndexBase); // theirs was renumbered
  CHECK(reloaded.reg().nextPedalIndex > theirsIdx);
  // And the imported preset points at the imported pedal, not at mine.
  REQUIRE(reloaded.reg().presetBanks.count("amp_theirs") == 1);
  CHECK(reloaded.reg().presetBanks.at("amp_theirs")[0].settings.preNam1Capture == theirsIdx);
  // A local preset still points at the local pedal.
  CHECK(reloaded.reg().presetBanks.at("amp_one")[0].settings.preNam1Capture == kCustomPedalIndexBase);
}

TEST_CASE("Re-importing the same pedal keeps the PRE index local presets already point at")
{
  // Same id, so it is the same pedal - just a newer capture. Taking the Pack's
  // index would leave every local preset pointing at an index nothing holds.
  Library sender("pedal-same-sender", "sender");
  sender.store.reg().pedals[0].legacyIndex = kCustomPedalIndexBase + 9; // their numbering
  sender.store.reg().pedals[0].name = "Klon (theirs)";
  REQUIRE(sender.store.Save());

  Library receiver("pedal-same-receiver", "recv");
  const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
  REQUIRE(pack.ok);
  REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true).ok);

  ContentStore reloaded(receiver.base);
  REQUIRE(reloaded.Load());
  REQUIRE(reloaded.reg().pedals.size() == 1);
  CHECK(reloaded.reg().pedals[0].name == "Klon (theirs)"); // the Pack won the payload
  CHECK(reloaded.reg().pedals[0].legacyIndex == kCustomPedalIndexBase); // but not the number
  CHECK(reloaded.reg().presetBanks.at("amp_one")[0].settings.preNam1Capture == kCustomPedalIndexBase);
}

// ---------------------------------------------------------------------------
// Machine settings checkbox
// ---------------------------------------------------------------------------

TEST_CASE("Machine settings and the MIDI map ride the standalone checkbox, not the verbs")
{
  Library sender("settings-sender", "sender");
  sender.store.reg().midiSoundMap[5] = MidiSoundAssignment{"amp_one", "preset_one"};
  REQUIRE(sender.store.Save());
  const std::string settings = "{\"volumLastAmp\":9}";
  const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()), settings);
  REQUIRE(pack.ok);

  SUBCASE("standalone with the box ticked writes the file and replaces the map")
  {
    Library receiver("settings-standalone", "recv");
    receiver.store.reg().midiSoundMap[1] = MidiSoundAssignment{"factory:2", ""};
    REQUIRE(receiver.store.Save());
    const auto settingsPath = receiver.base / "volum-settings.json";

    const auto preview = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, true, true);
    CHECK(preview.writesSettings);
    CHECK(preview.replacesMidiSoundMap);

    REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, true, true, settingsPath).ok);
    std::string got;
    REQUIRE(ReadWholeFile(settingsPath, got));
    CHECK(got == settings);

    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    CHECK(reloaded.reg().midiSoundMap.count(5) == 1);
    CHECK(reloaded.reg().midiSoundMap.count(1) == 0); // replaced, not merged
  }

  SUBCASE("standalone with the box clear leaves settings and the map alone")
  {
    Library receiver("settings-unticked", "recv");
    receiver.store.reg().midiSoundMap[1] = MidiSoundAssignment{"factory:2", ""};
    REQUIRE(receiver.store.Save());
    const auto settingsPath = receiver.base / "volum-settings.json";

    const auto preview = BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, false, true);
    CHECK_FALSE(preview.writesSettings);
    CHECK_FALSE(preview.replacesMidiSoundMap);

    REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true, settingsPath).ok);
    CHECK_FALSE(std::filesystem::exists(settingsPath));

    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    CHECK(reloaded.reg().midiSoundMap.count(1) == 1); // mine
    CHECK(reloaded.reg().midiSoundMap.count(5) == 0);
  }

  SUBCASE("a plugin never writes volum-settings.json, box or no box")
  {
    Library receiver("settings-plugin", "recv");
    const auto settingsPath = receiver.base / "volum-settings.json";

    const auto preview =
      BuildImportPreview(receiver.store.reg(), pack, ImportVerb::Overwrite, true, /*standalone=*/false);
    CHECK_FALSE(preview.writesSettings);
    CHECK_FALSE(preview.replacesMidiSoundMap);

    REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, true, /*standalone=*/false, settingsPath).ok);
    CHECK_FALSE(std::filesystem::exists(settingsPath));
    ContentStore reloaded(receiver.base);
    REQUIRE(reloaded.Load());
    CHECK(reloaded.reg().midiSoundMap.empty());
  }
}

TEST_CASE("Reset without the settings box can leave a MIDI slot invalid, never renumbered")
{
  // The locked answer: numbers stay, a missing Sound goes red. Wiping the row
  // would silently move every footswitch below it.
  Library receiver("reset-midi", "recv");
  CustomAmp local;
  local.id = "amp_local";
  local.name = "Gone soon";
  receiver.store.reg().amps.push_back(local);
  receiver.store.reg().midiSoundMap[7] = MidiSoundAssignment{"amp_local", ""};
  REQUIRE(receiver.store.Save());
  REQUIRE(volum::content::ResolveMidiSlot(receiver.store.reg(), 7, 12) == volum::content::MidiSlotState::Valid);

  Library sender("reset-midi-sender", "sender");
  const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
  REQUIRE(pack.ok);
  REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Reset, false, true).ok);

  ContentStore reloaded(receiver.base);
  REQUIRE(reloaded.Load());
  REQUIRE(reloaded.reg().midiSoundMap.count(7) == 1);
  CHECK(reloaded.reg().midiSoundMap.at(7).ampId == "amp_local");
  CHECK(volum::content::ResolveMidiSlot(reloaded.reg(), 7, 12) == volum::content::MidiSlotState::Invalid);
}

// ---------------------------------------------------------------------------
// Transactional failure
// ---------------------------------------------------------------------------

TEST_CASE("A corrupt Pack changes nothing at all")
{
  Library sender("txn-sender", "sender");
  Library receiver("txn-receiver", "recv");
  std::string err;
  const auto out = sender.base / "out.volumpack";
  REQUIRE(WritePack(sender.store, EverythingPlan(sender.store.reg()), "", out, &err));

  // Flip a byte in the payload: the archive still has a directory, so the damage
  // is only found at the checksum.
  std::string blob;
  REQUIRE(ReadWholeFile(out, blob));
  const size_t at = blob.find("NAM-amp-sender");
  REQUIRE(at != std::string::npos);
  blob[at + 4] = 'X';
  REQUIRE(WriteWholeFile(out, blob));

  const std::string before = volum::content::RegistryToJson(receiver.store.reg()).dump();
  const auto pack = OpenPack(out);
  CHECK_FALSE(pack.ok);
  const auto result = ApplyPack(receiver.store, pack, ImportVerb::Reset, true, true);
  CHECK_FALSE(result.ok);
  CHECK_FALSE(result.error.empty());

  CHECK(volum::content::RegistryToJson(receiver.store.reg()).dump() == before);
  ContentStore reloaded(receiver.base);
  REQUIRE(reloaded.Load());
  CHECK(reloaded.reg().amps.size() == 1);
  CHECK(reloaded.reg().amps[0].name == "Plexi");
  std::string body;
  REQUIRE(ReadWholeFile(reloaded.ResolveStored(receiver.ampStoredPath), body));
  CHECK(body == "NAM-amp-recv"); // never touched
}

TEST_CASE("A successful import leaves the prior library in a backup")
{
  Library sender("backup-sender", "sender");
  Library receiver("backup-receiver", "recv");
  const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
  REQUIRE(pack.ok);
  const std::string before = volum::content::RegistryToJson(receiver.store.reg()).dump();

  const auto result = ApplyPack(receiver.store, pack, ImportVerb::Reset, false, true);
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE_FALSE(result.backupPath.empty());
  REQUIRE(std::filesystem::exists(result.backupPath));

  std::string backup;
  REQUIRE(ReadWholeFile(result.backupPath, backup));
  CHECK(nlohmann::json::parse(backup).dump() == before);
  // And no staging directory is left behind.
  CHECK_FALSE(std::filesystem::exists(receiver.base / ".volumpack-stage"));
}

TEST_CASE("An import is a catalog writer: a sibling's unflushed item survives it")
{
  // Import goes through ContentStore::Save, so the same locked read-merge-write
  // that protects two plugin instances protects an import against one.
  Library sender("import-merge-sender", "sender");
  Library receiver("import-merge-receiver", "recv");
  const auto pack = PackFrom(sender, EverythingPlan(sender.store.reg()));
  REQUIRE(pack.ok);

  // A sibling store on the same library adds an IR and flushes it.
  ContentStore sibling(receiver.base);
  REQUIRE(sibling.Load());
  IRItem theirs;
  theirs.id = "ir_sibling";
  theirs.name = "Sibling IR";
  sibling.reg().irs.push_back(theirs);
  REQUIRE(sibling.Save());

  // The importing store never saw it, and must not drop it.
  REQUIRE(ApplyPack(receiver.store, pack, ImportVerb::Overwrite, false, true).ok);
  ContentStore reloaded(receiver.base);
  REQUIRE(reloaded.Load());
  bool found = false;
  for (const auto& ir : reloaded.reg().irs)
    if (ir.id == "ir_sibling")
      found = true;
  CHECK(found);
}
