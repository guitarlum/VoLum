#include "third_party/doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../VoLumDiagLog.h"

// The rolling diagnostic log. Two things must hold forever: it is bounded on disk,
// and it never becomes the reason something else fails.

using namespace volum::diag;

namespace
{
std::filesystem::path TempBase(const char* leaf)
{
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  auto p = std::filesystem::temp_directory_path() / ("volum-log-" + std::string(leaf) + "-" + std::to_string(stamp));
  std::filesystem::remove_all(p);
  std::filesystem::create_directories(p);
  return p;
}

std::string ReadAll(const std::filesystem::path& p)
{
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("Rotation triggers only when the pending line would exceed the cap")
{
  CHECK_FALSE(ShouldRotateLog(0, 10, 100)); // empty file: always write
  CHECK_FALSE(ShouldRotateLog(50, 10, 100)); // 60 <= 100
  CHECK_FALSE(ShouldRotateLog(90, 10, 100)); // exactly at the cap still fits
  CHECK(ShouldRotateLog(91, 10, 100)); // 101 > 100

  // An entry larger than the whole cap still gets written once into an empty file
  // rather than rotating forever and losing it.
  CHECK_FALSE(ShouldRotateLog(0, 5000, 100));
  CHECK(ShouldRotateLog(1, 5000, 100));
}

TEST_CASE("Writing appends timestamped, categorized lines")
{
  const auto base = TempBase("write");
  const auto path = base / "volum.log";
  Log::Instance().Open(path);
  REQUIRE(Log::Instance().enabled());

  VOLUM_LOG("startup", "VoLum 1.2.1 (standalone) instance created");
  VOLUM_LOG("audio", "reset: 48000 Hz, block 128");

  const std::string body = ReadAll(path);
  CHECK(body.find("[startup] VoLum 1.2.1 (standalone) instance created") != std::string::npos);
  CHECK(body.find("[audio] reset: 48000 Hz, block 128") != std::string::npos);

  // Two lines, each opening with a "YYYY-MM-DD HH:MM:SS.mmm" stamp.
  std::istringstream lines(body);
  std::string line;
  int count = 0;
  while (std::getline(lines, line))
  {
    if (line.empty())
      continue;
    ++count;
    CHECK(line.size() > 23);
    CHECK(line[4] == '-');
    CHECK(line[13] == ':');
  }
  CHECK(count == 2);

  Log::Instance().Close();
  std::filesystem::remove_all(base);
}

TEST_CASE("The log stays bounded: one rolled generation, nothing older")
{
  const auto base = TempBase("rotate");
  const auto path = base / "volum.log";
  Log::Instance().Open(path);

  // Well past the cap: kMaxLogBytes / ~200 bytes per line, times three.
  const std::string filler(200, 'x');
  const int lines = static_cast<int>(3 * kMaxLogBytes / 200);
  for (int i = 0; i < lines; ++i)
    VOLUM_LOG("bulk", filler);

  const auto rolled = Log::Instance().rolledPath();
  CHECK(std::filesystem::exists(path));
  CHECK(std::filesystem::exists(rolled));
  CHECK(std::filesystem::file_size(path) <= kMaxLogBytes);
  CHECK(std::filesystem::file_size(rolled) <= kMaxLogBytes);

  // Exactly two files: no .2, .3, ... accumulating over a year of daily use.
  int fileCount = 0;
  for (const auto& e : std::filesystem::directory_iterator(base))
  {
    (void)e;
    ++fileCount;
  }
  CHECK(fileCount == 2);

  Log::Instance().Close();
  std::filesystem::remove_all(base);
}

TEST_CASE("A log that cannot be opened is silent, not fatal")
{
  Log::Instance().Close();
  CHECK_FALSE(Log::Instance().enabled());
  VOLUM_LOG("startup", "dropped on the floor"); // must not throw

  // An empty path is how the plugin reports "no writable location on this OS".
  Log::Instance().Open({});
  CHECK_FALSE(Log::Instance().enabled());
  VOLUM_LOG("startup", "also dropped");

  // A path under a file (not a directory) cannot be created; writing must still
  // be harmless rather than throwing out of a plugin constructor.
  const auto base = TempBase("badpath");
  const auto blocker = base / "not-a-dir";
  {
    std::ofstream out(blocker);
    out << "x";
  }
  Log::Instance().Open(blocker / "sub" / "volum.log");
  VOLUM_LOG("startup", "still harmless");

  Log::Instance().Close();
  std::filesystem::remove_all(base);
}
