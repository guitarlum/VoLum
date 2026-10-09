#include "third_party/doctest.h"

#include "../VoLumIrFileGuard.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

namespace
{
void PutLe(std::string& out, std::uint32_t v, int bytes)
{
  for (int i = 0; i < bytes; ++i)
    out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

// A canonical RIFF/WAVE file whose data chunk DECLARES declaredData bytes but
// physically carries actualData bytes, optionally behind a LIST chunk.
std::string MakeWav(int format, int channels, std::uint32_t rate, int bits, std::uint32_t declaredData,
                    std::uint32_t actualData, bool listChunk = false)
{
  std::string out = "RIFF";
  PutLe(out, 0, 4); // RIFF size: the parser ignores it
  out += "WAVE";
  out += "fmt ";
  PutLe(out, 16, 4);
  PutLe(out, static_cast<std::uint32_t>(format), 2);
  PutLe(out, static_cast<std::uint32_t>(channels), 2);
  PutLe(out, rate, 4);
  PutLe(out, rate * static_cast<std::uint32_t>(channels * bits / 8), 4);
  PutLe(out, static_cast<std::uint32_t>(channels * bits / 8), 2);
  PutLe(out, static_cast<std::uint32_t>(bits), 2);
  if (listChunk)
  {
    out += "LIST";
    PutLe(out, 5, 4);
    out += std::string("INFOx", 5);
    out.push_back('\0'); // pad byte after the odd-sized chunk
  }
  out += "data";
  PutLe(out, declaredData, 4);
  out += std::string(actualData, '\x10');
  return out;
}

bool Valid(const char* leaf, const std::string& bytes, std::string& message)
{
  return volum::IrFileValidForImport(Utf8Path(BadIrPath(leaf, bytes)), message);
}
} // namespace

TEST_CASE("IR import accepts the odd-but-valid WAVs the loader reads")
{
  std::string message;
  CHECK(Valid("ok-stereo24-list.wav", MakeWav(1, 2, 48000, 24, 600, 600, true), message));
  CHECK(Valid("ok-mono-f32-96k.wav", MakeWav(3, 1, 96000, 32, 400, 400), message));
  CHECK(Valid("ok-6ch-16.wav", MakeWav(1, 6, 44100, 16, 1200, 1200), message));
}

TEST_CASE("IR import header guard: a declared size past the file never reaches the parser")
{
  std::string message;
  // 0x80000000 bytes declared in a 50-byte file: the parser used to throw
  // std::length_error out of the import path.
  CHECK_FALSE(Valid("huge-declared.wav", MakeWav(1, 1, 48000, 16, 0x80000000u, 8), message));
  CHECK(message == "WAV file is truncated.");
  // 60 bytes on disk declaring 256 MB: the parser used to allocate it all.
  const std::string small = MakeWav(1, 1, 48000, 16, 256u * 1024u * 1024u, 16);
  REQUIRE(small.size() == 60);
  CHECK_FALSE(Valid("declares-256mb.wav", small, message));
  CHECK(message == "WAV file is truncated.");
  // Truncated mid-chunk: declared 1000, 200 present.
  CHECK_FALSE(Valid("truncated.wav", MakeWav(1, 1, 48000, 16, 1000, 200), message));
  CHECK(message == "WAV file is truncated.");
}

TEST_CASE("IR import header guard: zero samples and a zero sample rate are rejected")
{
  std::string message;
  CHECK_FALSE(Valid("zero-samples.wav", MakeWav(1, 1, 48000, 16, 0, 0), message));
  CHECK(message == "WAV file contains no audio.");
  CHECK_FALSE(Valid("zero-rate.wav", MakeWav(1, 1, 0, 16, 400, 400), message));
  CHECK(message == "WAV file has an invalid sample rate.");
}

TEST_CASE("IR import never lets a throwing or empty-result loader escape")
{
  const std::string good = MakeWav(1, 1, 48000, 16, 400, 400);
  const auto path = Utf8Path(BadIrPath("loader-seam.wav", good));
  std::string message;

  // A parser that throws (length_error, bad_alloc, anything) is a rejected file.
  CHECK_FALSE(volum::IrFileValidForImport(
    path, message,
    [](const char*, std::vector<float>&, double&) -> dsp::wav::LoadReturnCode { throw std::length_error("vector"); }));
  CHECK(message == "File is not a readable WAV file.");
  CHECK_FALSE(volum::IrFileValidForImport(
    path, message, [](const char*, std::vector<float>&, double&) -> dsp::wav::LoadReturnCode { throw 7; }));

  // A parser that reports success with nothing decoded, or no sample rate, is
  // not a usable IR either.
  CHECK_FALSE(volum::IrFileValidForImport(path, message, [](const char*, std::vector<float>&, double& sr) {
    sr = 48000.0;
    return dsp::wav::LoadReturnCode::SUCCESS;
  }));
  CHECK(message == "WAV file contains no audio.");
  CHECK_FALSE(volum::IrFileValidForImport(path, message, [](const char*, std::vector<float>& audio, double& sr) {
    audio.assign(8, 0.1f);
    sr = 0.0;
    return dsp::wav::LoadReturnCode::SUCCESS;
  }));
  CHECK(message == "WAV file has an invalid sample rate.");
}

TEST_CASE("IR import banner names every rejected file and keeps the too-large report")
{
  using volum::IrImportRejectionSummary;
  const std::vector<std::pair<std::string, std::string>> one = {{"a", "File is not a WAV file."}};
  const std::vector<std::pair<std::string, std::string>> three = {{"a", "x"}, {"b", "x"}, {"c", "x"}};

  CHECK(IrImportRejectionSummary({}, one) == "\"a\": File is not a WAV file.");
  const std::string many = IrImportRejectionSummary({}, three);
  CHECK(many.find("3 files") != std::string::npos);
  CHECK(many.find("\"a\"") != std::string::npos);
  CHECK(many.find("\"b\"") != std::string::npos);
  CHECK(many.find("1 more") != std::string::npos);

  // A too-large pick is never replaced by an unrelated invalid-file banner.
  const std::string both = IrImportRejectionSummary({"big"}, one);
  CHECK(both.find("\"big\" is too large for an IR") != std::string::npos);
  CHECK(both.find("\"a\"") != std::string::npos);
  CHECK(IrImportRejectionSummary({"big"}, {}) == "\"big\" is too large for an IR - skipped.");
}