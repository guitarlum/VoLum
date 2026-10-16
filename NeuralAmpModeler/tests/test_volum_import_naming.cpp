#include "third_party/doctest.h"

#include "../VoLumImportNaming.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Manage > Add for custom IRs and pedals: long filenames that differ only at the
// end must land as distinct library entries, and only a byte-identical file is
// turned away.

using volum::custom::FilesIdentical;
using volum::custom::FindIdenticalFile;
using volum::custom::ImportOutcomeSummary;
using volum::custom::IrChipLabel;
using volum::custom::kMaxCustomNameLen;
using volum::custom::MiddleEllipsize;
using volum::custom::NameMatchesCI;
using volum::custom::UniqueImportName;

namespace
{
std::filesystem::path WriteTemp(const char* leaf, const std::string& bytes)
{
  const auto dir = std::filesystem::temp_directory_path() / "volum-import-naming-tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const auto path = dir / leaf;
  std::ofstream(path, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return path;
}

// Imports names one after another into a growing case-insensitive library, the
// way StartImport does.
std::vector<std::string> ImportAll(const std::vector<std::string>& bases)
{
  std::vector<std::string> library;
  for (const auto& base : bases)
  {
    const std::string name = UniqueImportName(base, kMaxCustomNameLen, [&](const std::string& n) {
      for (const auto& have : library)
        if (NameMatchesCI(have, n))
          return true;
      return false;
    });
    REQUIRE_FALSE(name.empty());
    library.push_back(name);
  }
  return library;
}

bool IsWellFormedUtf8(const std::string& s)
{
  return volum::custom::Utf8Prefix(s, s.size()) == s;
}
} // namespace

TEST_CASE("Long IR filenames that differ only in a trailing number import as distinct names")
{
  const std::vector<std::string> bases = {
    "IR_XY_asdasd_Studio_412_Brit_V30_Close_Cap_1",
    "IR_XY_asdasd_Studio_412_Brit_V30_Close_Cap_2",
    "IR_XY_asdasd_Studio_412_Brit_V30_Close_Cap_3",
    "IR_XY_asdasd_Studio_412_Brit_V30_Close_Cap_12",
  };
  const auto library = ImportAll(bases);
  REQUIRE(library.size() == bases.size());
  CHECK(std::set<std::string>(library.begin(), library.end()).size() == bases.size());
  for (std::size_t i = 0; i < library.size(); ++i)
  {
    INFO(library[i]);
    CHECK(library[i].size() <= kMaxCustomNameLen);
    CHECK(library[i].rfind("IR_XY_", 0) == 0); // the start survives
    CHECK(library[i].find(" 2") == std::string::npos); // told apart by their own ends, not a number
  }
  CHECK(library[0].substr(library[0].size() - 2) == "_1");
  CHECK(library[3].substr(library[3].size() - 3) == "_12");
}

TEST_CASE("MiddleEllipsize keeps both ends and the stored-name cap")
{
  CHECK(MiddleEllipsize("short", kMaxCustomNameLen) == "short");
  const std::string exact(kMaxCustomNameLen, 'a');
  CHECK(MiddleEllipsize(exact, kMaxCustomNameLen) == exact);

  const std::string cut = MiddleEllipsize("Studiocab_412_BritV30_Close_Cap_Edge_01", kMaxCustomNameLen);
  CHECK(cut == "Studiocab_412\xE2\x80\xA6_Edge_01");
  CHECK(cut.size() == kMaxCustomNameLen);

  // Both cut edges land on a word: no "…30_D57_1", no "asdasdd_…".
  CHECK(MiddleEllipsize("Cab_XY_asdasdd_Studio_412_Brit_V30_D57_1", kMaxCustomNameLen)
        == "Cab_XY_asdasdd\xE2\x80\xA6_D57_1");
}

TEST_CASE("MiddleEllipsize never splits a UTF-8 glyph at either end")
{
  // Euros and an emoji straddle every cut point in turn.
  std::string s;
  for (int i = 0; i < 12; ++i)
    s += i % 3 == 0 ? "\xF0\x9F\x8E\xB8" : "\xE2\x82\xAC";
  for (std::size_t cap = 4; cap <= 30; ++cap)
  {
    INFO(cap);
    const std::string out = MiddleEllipsize(s, cap);
    CHECK(out.size() <= cap);
    CHECK(IsWellFormedUtf8(out));
  }
  // A malformed byte ends the name the way ClampName does.
  CHECK(MiddleEllipsize(std::string("ok") + "\xE2" + "bc", kMaxCustomNameLen) == "ok");
}

TEST_CASE("A name that is still taken gets a number within the cap")
{
  const auto library = ImportAll({"Brit OS", "Brit OS", "brit os"});
  CHECK(library == std::vector<std::string>{"Brit OS", "Brit OS 2", "brit os 3"});

  const std::string longBase = "A_very_long_cabinet_capture_name_x";
  const auto longLib = ImportAll({longBase, longBase});
  REQUIRE(longLib.size() == 2);
  CHECK(longLib[0] != longLib[1]);
  CHECK(longLib[1].size() <= kMaxCustomNameLen);
  CHECK(longLib[1].substr(longLib[1].size() - 2) == " 2");
  CHECK(longLib[1].find("\xE2\x80\xA6") != std::string::npos); // still both ends of the original
}

TEST_CASE("UniqueImportName gives up only when there is nothing to name")
{
  CHECK(UniqueImportName("", kMaxCustomNameLen, [](const std::string&) { return false; }).empty());
  CHECK(UniqueImportName("x", kMaxCustomNameLen, [](const std::string&) { return true; }).empty());
}

TEST_CASE("The cab-row IR chip tells near-identical long IR names apart")
{
  CHECK(IrChipLabel("Brit OS") == "Brit OS");
  CHECK(IrChipLabel("exactly12chr") == "exactly12chr");
  const std::string a = IrChipLabel("IR_XY_asdasd_1");
  const std::string b = IrChipLabel("IR_XY_asdasd_2");
  CHECK(a != b);
  CHECK(a == "IR_XY_a\xE2\x80\xA6sd_1");
  CHECK(a.size() <= 11 + 3);
}

TEST_CASE("FilesIdentical matches only byte-identical files")
{
  const auto a = WriteTemp("a.wav", std::string(40000, 'x') + "end");
  const auto sameBytes = WriteTemp("renamed copy.wav", std::string(40000, 'x') + "end");
  const auto sameSize = WriteTemp("same-size.wav", std::string(40000, 'x') + "eNd");
  const auto shorter = WriteTemp("shorter.wav", std::string(40000, 'x'));

  CHECK(FilesIdentical(a, sameBytes));
  CHECK_FALSE(FilesIdentical(a, sameSize));
  CHECK_FALSE(FilesIdentical(a, shorter));
  CHECK_FALSE(FilesIdentical(a, a.parent_path() / "missing.wav"));
  CHECK_FALSE(FilesIdentical(a, {}));

  CHECK(FindIdenticalFile(sameBytes, {shorter, sameSize, a}) == 2);
  CHECK(FindIdenticalFile(sameSize, {shorter, a, {}}) == -1);
}

TEST_CASE("Import status line reports renames and exact duplicates")
{
  CHECK(ImportOutcomeSummary({}, {}).empty());
  CHECK(ImportOutcomeSummary({{"Brit OS", "Brit OS 2"}}, {}) == "\"Brit OS\" was taken - added as \"Brit OS 2\".");
  CHECK(ImportOutcomeSummary({}, {{"Cab 1", "My cab"}}) == "Already in your library as \"My cab\" - skipped.");
  CHECK(ImportOutcomeSummary({{"a", "a 2"}, {"b", "b 2"}}, {}) == "2 names were taken - added with a number.");
  CHECK(ImportOutcomeSummary({}, {{"c", "c"}, {"d", "d"}}) == "2 files are already in your library - skipped.");
  // Both kinds at once: counts, short enough for the one status line.
  const std::string both = ImportOutcomeSummary({{"a", "a 2"}}, {{"c", "c"}});
  CHECK(both == "1 added with a number (name taken), 1 skipped (already in your library).");
  CHECK(both.size() < 80);
  const std::string longest = ImportOutcomeSummary({{std::string(24, 'w'), std::string(24, 'g')}}, {});
  CHECK(longest.size() < 80);
}
