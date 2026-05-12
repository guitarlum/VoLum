#include "third_party/doctest.h"
#include "../VoLumSettingsFileIO.h"

#include <atomic>
#include <filesystem>
#include <fstream>
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
