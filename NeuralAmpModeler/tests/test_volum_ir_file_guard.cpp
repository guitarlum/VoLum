#include "third_party/doctest.h"

#include "../VoLumIrFileGuard.h"

#include <filesystem>
#include <fstream>

// VoLum custom-IR file-size guard: small/normal captures are accepted; absurdly
// large WAVs (e.g. a whole song picked by mistake) are rejected before decoding.

namespace
{
std::filesystem::path BadIrPath(const char* leaf, const std::string& bytes)
{
  const auto dir = std::filesystem::temp_directory_path() / "volum-ir-import-guard-tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const auto path = dir / leaf;
  std::ofstream(path, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return path;
}

std::string Utf8Path(const std::filesystem::path& path)
{
  const auto u8 = path.u8string();
  return std::string(u8.begin(), u8.end());
}
} // namespace

TEST_CASE("IR byte guard accepts normal-sized cabinet captures")
{
  CHECK(volum::IrFileBytesAcceptable(0));
  CHECK(volum::IrFileBytesAcceptable(144 * 1024)); // ~1s mono 24-bit 48k
  CHECK(volum::IrFileBytesAcceptable(6ull * 1024 * 1024)); // ~10s stereo 24-bit 96k
}

TEST_CASE("IR byte guard accepts exactly the cap and rejects beyond it")
{
  CHECK(volum::IrFileBytesAcceptable(volum::kMaxIrFileBytes));
  CHECK_FALSE(volum::IrFileBytesAcceptable(volum::kMaxIrFileBytes + 1));
  CHECK_FALSE(volum::IrFileBytesAcceptable(500ull * 1024 * 1024)); // a 500 MB "IR"
}

TEST_CASE("IR too-large message names the size and the cap")
{
  const std::string msg = volum::IrTooLargeMessage(volum::kMaxIrFileBytes * 2);
  CHECK(msg.find("too large") != std::string::npos);
  CHECK(msg.find("128") != std::string::npos); // 2x the 64 MB cap
  CHECK(msg.find("64") != std::string::npos); // the cap itself
}

TEST_CASE("IR import guard rejects malformed files through the production WAV parser")
{
  const std::filesystem::path cases[] = {
    BadIrPath("junk-renamed.wav", "this is not audio"),
    BadIrPath("empty.wav", ""),
    BadIrPath("header-only.wav", std::string("RIFF\0\0\0\0WAVE", 12)),
    BadIrPath("not-a-wave.txt", "plain text"),
  };

  for (const auto& path : cases)
  {
    CAPTURE(path.string());
    std::string message;
    CHECK_FALSE(volum::IrFileValidForImport(Utf8Path(path), message));
    CHECK(message == "File is not a WAV file.");
  }
}
