// File-CRUD edge cases for the custom-content store: UTF-8/long paths,
// missing/absent directories, import name collisions, removing files that are
// already gone, and registry round-trips that preserve non-ASCII names
// byte-for-byte. These run on Windows and under the macOS CI ASan/UBSan job.
//
// Non-ASCII strings are written as narrow UTF-8 byte escapes (not u8"" literals,
// which are char8_t[] under C++20 and will not assign to std::string) so the
// source file stays pure-ASCII and encoding-agnostic across compilers. String
// literal concatenation ("\xC3\xA9" "set") terminates each hex escape so it does
// not swallow the following readable character.

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "../VoLumContentStore.h"

using namespace volum::content;
using volum::VoLumAmpSettings;

namespace
{
std::filesystem::path CrudBase(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-content-crud-edge" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}

std::filesystem::path WriteSrc(const std::filesystem::path& dir, const std::string& leaf, const char* body)
{
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const auto p = dir / PathFromUtf8(leaf);
  std::ofstream out(p, std::ios::binary);
  out << body;
  out.close();
  return p;
}

std::string ReadAll(const std::filesystem::path& p)
{
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("ImportFileCopy preserves a unicode leaf name and copies the bytes")
{
  const auto base = CrudBase("unicode-leaf");
  // "Mesa Rec'tifier <astral>.wav": e-acute, right-quote, i-diaeresis, and a
  // 4-byte astral glyph (U+1D11E musical G-clef) in the leaf.
  const std::string leaf =
    "M\xC3\xA9"
    "sa Rec\xE2\x80\x99"
    "t\xC3\xAF"
    "fier \xF0\x9D\x84\x9E.wav";
  const auto src = WriteSrc(base / "incoming", leaf, "RIFFdata");
  ContentStore store(base);

  const std::string rel = store.ImportFileCopy(src, "ir", "ir_uni");
  REQUIRE_FALSE(rel.empty());
  const auto dst = store.ResolveStored(rel);
  CHECK(std::filesystem::exists(dst));
  CHECK(ReadAll(dst) == "RIFFdata");
  CHECK(rel.rfind("ir/ir_uni__", 0) == 0);
  CHECK(rel.find(leaf) != std::string::npos);
}

TEST_CASE("ImportFileCopy handles a very long leaf name without truncating mid-copy")
{
  const auto base = CrudBase("long-leaf");
  // Long enough to exercise the copy path, but the full absolute path must stay
  // under Windows' default MAX_PATH (260) given the temp-dir prefix, so cap the
  // leaf well below the 255-byte single-component limit.
  const std::string leaf = std::string(80, 'a') + ".wav";
  const auto src = WriteSrc(base / "incoming", leaf, "LONG");
  ContentStore store(base);

  const std::string rel = store.ImportFileCopy(src, "amps", "amp_long");
  REQUIRE_FALSE(rel.empty());
  CHECK(std::filesystem::exists(store.ResolveStored(rel)));
  CHECK(ReadAll(store.ResolveStored(rel)) == "LONG");
}

TEST_CASE("ImportFileCopy creates the target subdir on demand")
{
  const auto base = CrudBase("makes-subdir");
  const auto src = WriteSrc(base / "incoming", "cap.nam", "{}");
  ContentStore store(base);
  CHECK_FALSE(std::filesystem::exists(store.PedalsDir()));

  const std::string rel = store.ImportFileCopy(src, "pedals", "pedal_x");
  REQUIRE_FALSE(rel.empty());
  CHECK(std::filesystem::exists(store.PedalsDir()));
  CHECK(std::filesystem::exists(store.ResolveStored(rel)));
}

TEST_CASE("ImportFileCopy returns empty for a missing source and an unset base dir")
{
  const auto base = CrudBase("missing-src");
  ContentStore store(base);
  CHECK(store.ImportFileCopy(base / "nope.wav", "ir", "ir_a").empty());

  ContentStore noBase; // empty base dir => Save() is a no-op, import must refuse
  const auto src = WriteSrc(base / "incoming", "real.wav", "x");
  CHECK(noBase.ImportFileCopy(src, "ir", "ir_b").empty());
}

TEST_CASE("ImportFileCopy: distinct id prefixes keep same-named sources side by side")
{
  const auto base = CrudBase("collide-leaf");
  // Two different sources that share the exact same leaf name.
  const auto a = WriteSrc(base / "in_a", "4x12.wav", "AAA");
  const auto b = WriteSrc(base / "in_b", "4x12.wav", "BBB");
  ContentStore store(base);

  const std::string relA = store.ImportFileCopy(a, "ir", "ir_aaaa");
  const std::string relB = store.ImportFileCopy(b, "ir", "ir_bbbb");
  REQUIRE_FALSE(relA.empty());
  REQUIRE_FALSE(relB.empty());
  CHECK(relA != relB);
  // Both survive: the id prefix disambiguates a shared leaf, no clobber.
  CHECK(ReadAll(store.ResolveStored(relA)) == "AAA");
  CHECK(ReadAll(store.ResolveStored(relB)) == "BBB");
}

TEST_CASE("RemoveStoredFile is a no-op for an empty or already-missing path")
{
  const auto base = CrudBase("remove-missing");
  ContentStore store(base);
  // Neither should throw nor create anything.
  store.RemoveStoredFile("");
  store.RemoveStoredFile("ir/does-not-exist.wav");
  CHECK_FALSE(std::filesystem::exists(store.ResolveStored("ir/does-not-exist.wav")));
}

TEST_CASE("Save creates a missing base dir and Load round-trips from it")
{
  const auto parent = CrudBase("save-makes-base");
  const auto base = parent / "nested" / "content"; // does not exist yet
  CHECK_FALSE(std::filesystem::exists(base));

  ContentStore store(base);
  volum::custom::CustomAmp amp;
  amp.id = "amp_rt";
  amp.name =
    "\xC3\x9C"
    "bercaster"; // leading U-umlaut
  store.reg().amps.push_back(amp);
  REQUIRE(store.Save());
  CHECK(std::filesystem::exists(base));
  CHECK(std::filesystem::exists(store.RegistryPath()));

  ContentStore reload(base);
  CHECK(reload.Load());
  REQUIRE(reload.reg().amps.size() == 1);
  CHECK(reload.reg().amps[0].id == "amp_rt");
  CHECK(reload.reg().amps[0].name == amp.name); // unicode survives the round-trip
}

TEST_CASE("Corrupt registry is backed up even when a stale .bak already exists")
{
  const auto base = CrudBase("bak-overwrite");
  ContentStore store(base);
  std::error_code ec;
  std::filesystem::create_directories(base, ec);

  // Stale leftover backup from a prior recovery.
  {
    std::ofstream(store.BackupPath(), std::ios::binary) << "OLD-BAK";
  }
  // Current registry is unparseable JSON.
  {
    std::ofstream(store.RegistryPath(), std::ios::binary) << "{ this is not json ";
  }

  CHECK_FALSE(store.Load()); // recovered, not clean
  CHECK(store.reg().amps.empty());
  // The corrupt file was moved aside and the backup now reflects the corrupt
  // content, replacing the stale one.
  CHECK(std::filesystem::exists(store.BackupPath()));
  CHECK(ReadAll(store.BackupPath()) != "OLD-BAK");
}

TEST_CASE("Registry round-trips unicode preset and IR names byte-for-byte")
{
  Registry r;
  IRItem ir;
  ir.id = "ir_u";
  ir.name =
    "Gr\xC3\xB6\xC3\x9F"
    "e \xE4\xB8\xAD\xE6\x96\x87"; // umlaut + CJK
  ir.file = "ir/ir_u__x.wav";
  r.irs.push_back(ir);

  Preset pr;
  pr.id = "preset_u";
  pr.name =
    "Pr\xC3\xA9"
    "set \xF0\x9F\x8E\xB8"; // accent + emoji guitar
  r.presetBanks["amp_x"] = {pr};

  bool healed = false;
  const Registry back = RegistryFromJson(RegistryToJson(r), &healed);
  CHECK_FALSE(healed);
  REQUIRE(back.irs.size() == 1);
  CHECK(back.irs[0].name == ir.name);
  REQUIRE(back.presetBanks.count("amp_x") == 1);
  REQUIRE(back.presetBanks.at("amp_x").size() == 1);
  CHECK(back.presetBanks.at("amp_x")[0].name == pr.name);
}
