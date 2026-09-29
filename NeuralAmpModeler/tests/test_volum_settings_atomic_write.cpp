#include "third_party/doctest.h"
#include "../VoLumSettingsFileIO.h"
#include "../VoLumUpdateState.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <thread>
#include <vector>

namespace
{

std::filesystem::path TestRoot(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-settings-atomic-write-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}

nlohmann::json ReadJsonFile(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  nlohmann::json j;
  in >> j;
  return j;
}

bool HasAtomicTempFile(const std::filesystem::path& dir)
{
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
  {
    if (entry.path().filename().string().find(".tmp.") != std::string::npos)
      return true;
  }
  return false;
}

} // namespace

TEST_CASE("ReplaceFileAtomically refuses POSIX rename over a write-bit-clear file")
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumSettingsFileIO.h";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  const std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(src.find("st.permissions() & std::filesystem::perms::owner_write") != std::string::npos);
}

TEST_CASE("WriteJsonAtomically writes complete JSON and removes temp file")
{
  const auto root = TestRoot("golden");
  const auto path = root / "volum-settings.json";
  const nlohmann::json payload = {
    {"version", 6},
    {"lastAmpIdx", 3},
    {"amps", {{"Ampete One", {{"speaker", 2}, {"channel", 1}}}}},
  };

  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, payload, ec));
  CHECK_FALSE(ec);
  CHECK(std::filesystem::exists(path));
  CHECK_FALSE(HasAtomicTempFile(root));
  CHECK(ReadJsonFile(path) == payload);
}

TEST_CASE("WriteJsonAtomically refuses a read-only target and leaves it intact")
{
  // macOS CI: POSIX rename replaces a chmod u-w file. The replace helper must
  // honor the write bit so a backup lock cannot wipe the library.
  const auto root = TestRoot("read-only-target");
  const auto path = root / "volum-settings.json";
  const nlohmann::json original = {{"writer", "original"}, {"value", 1}};
  const nlohmann::json replacement = {{"writer", "replacement"}, {"value", 2}};

  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, original, ec));
  std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace);

  CHECK_FALSE(volum::WriteJsonAtomically(path, replacement, ec));
  CHECK(ec);
  std::filesystem::permissions(path, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
  CHECK(ReadJsonFile(path) == original);
  CHECK_FALSE(HasAtomicTempFile(root));
}

TEST_CASE("WriteJsonAtomically leaves existing file untouched when target path is invalid")
{
  const auto root = TestRoot("invalid-target");
  const auto path = root / "volum-settings.json";
  const nlohmann::json original = {{"writer", "original"}, {"value", 1}};
  const nlohmann::json replacement = {{"writer", "replacement"}, {"value", 2}};

  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, original, ec));
  CHECK(ReadJsonFile(path) == original);

  const auto invalidTarget = root / "blocking-directory" / "nested.json";
  std::filesystem::create_directories(invalidTarget, ec);
  REQUIRE_FALSE(ec);
  CHECK_FALSE(volum::WriteJsonAtomically(invalidTarget, replacement, ec));
  CHECK(ec);
  CHECK(ReadJsonFile(path) == original);
}

TEST_CASE("WriteJsonAtomically concurrent writers leave a complete parseable file")
{
  const auto root = TestRoot("race");
  const auto path = root / "volum-settings.json";
  const nlohmann::json first = {{"writer", "first"}, {"value", 1}, {"payload", std::string(2048, 'a')}};
  const nlohmann::json second = {{"writer", "second"}, {"value", 2}, {"payload", std::string(2048, 'b')}};

  for (int iteration = 0; iteration < 25; ++iteration)
  {
    std::atomic<bool> start{false};
    std::error_code ec1;
    std::error_code ec2;
    bool ok1 = false;
    bool ok2 = false;

    std::thread t1([&]() {
      while (!start.load()) {}
      ok1 = volum::WriteJsonAtomically(path, first, ec1);
    });
    std::thread t2([&]() {
      while (!start.load()) {}
      ok2 = volum::WriteJsonAtomically(path, second, ec2);
    });

    start.store(true);
    t1.join();
    t2.join();

    CHECK(ok1);
    CHECK_FALSE(ec1);
    CHECK(ok2);
    CHECK_FALSE(ec2);

    const nlohmann::json loaded = ReadJsonFile(path);
    CHECK((loaded == first || loaded == second));
    CHECK_FALSE(HasAtomicTempFile(root));
  }
}

TEST_CASE("A document containing invalid UTF-8 fails the write instead of throwing")
{
  // Every user-entered name - amp, IR, pedal, preset, cabinet - is serialized
  // through here, and nlohmann::json::dump() throws type_error on invalid UTF-8
  // anywhere in the document. The individual name cuts are character-aware, but
  // this is the one place all of them pass through, so it has to be the backstop:
  // an exception here would escape into UI or host code during a save.
  const auto root = TestRoot("invalid-utf8");
  const auto path = root / "settings.json";

  nlohmann::json good;
  good["name"] = "fine";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, good, ec));
  REQUIRE_FALSE(ec);

  nlohmann::json bad;
  bad["name"] = std::string("half a glyph: ") + "\xF0\x9F\x98"; // truncated U+1F600

  bool ok = true;
  CHECK_NOTHROW(ok = volum::WriteJsonAtomically(path, bad, ec));
  CHECK_FALSE(ok);
  CHECK(ec == std::errc::invalid_argument);

  // The previous good file survives untouched, and no temp file is left behind.
  const nlohmann::json loaded = ReadJsonFile(path);
  CHECK(loaded == good);
  CHECK_FALSE(HasAtomicTempFile(root));
}

TEST_CASE("Update sidecar round-trips independently of user settings")
{
  const auto root = TestRoot("update-sidecar");
  const auto path = root / "volum-update-state.json";
  volum::update::UpdateState expected;
  expected.lastCheckUtc = 1'787'000'000;
  expected.lastSeenVersion = "1.3.0";
  expected.latestKnownVersion = "1.3.1";
  expected.latestKnownUrl = "https://github.com/guitarlum/VoLum/releases/tag/v1.3.1";
  expected.latestKnownNotes = "Maintenance release.";
  expected.autoCheck = false;

  REQUIRE(volum::update::SaveUpdateState(path, expected));
  const auto loaded = volum::update::LoadUpdateState(path);
  CHECK(loaded.lastCheckUtc == expected.lastCheckUtc);
  CHECK(loaded.lastSeenVersion == expected.lastSeenVersion);
  CHECK(loaded.latestKnownVersion == expected.latestKnownVersion);
  CHECK(loaded.latestKnownUrl == expected.latestKnownUrl);
  CHECK(loaded.latestKnownNotes == expected.latestKnownNotes);
  CHECK(loaded.autoCheck == expected.autoCheck);
  CHECK_FALSE(HasAtomicTempFile(root));
}
