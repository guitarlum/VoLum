// VoLum: regression coverage for the multi-channel -> mono downmix added to
// AudioDSPTools/dsp/wav.cpp. Commercial IRs are frequently stereo; the loader
// must accept them (averaging channels) instead of rejecting with ERROR_NOT_MONO.

#include "third_party/doctest.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../../AudioDSPTools/dsp/wav.h"

namespace
{
std::filesystem::path TestDir()
{
  auto root = std::filesystem::temp_directory_path() / "volum-wav-downmix-tests";
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  return root;
}

void WriteLE16(std::ofstream& f, std::uint16_t v)
{
  std::uint8_t b[2] = {(std::uint8_t)(v & 0xFF), (std::uint8_t)((v >> 8) & 0xFF)};
  f.write(reinterpret_cast<char*>(b), 2);
}

void WriteLE32(std::ofstream& f, std::uint32_t v)
{
  std::uint8_t b[4] = {(std::uint8_t)(v & 0xFF), (std::uint8_t)((v >> 8) & 0xFF), (std::uint8_t)((v >> 16) & 0xFF),
                       (std::uint8_t)((v >> 24) & 0xFF)};
  f.write(reinterpret_cast<char*>(b), 4);
}

// Write a 16-bit PCM WAV with the given interleaved samples and channel count.
std::filesystem::path WritePcm16Wav(const char* leaf, int numChannels, int sampleRate,
                                    const std::vector<std::int16_t>& interleaved)
{
  const auto path = TestDir() / leaf;
  std::ofstream f(path, std::ios::binary);
  const std::uint32_t dataBytes = (std::uint32_t)(interleaved.size() * 2);
  const std::uint16_t blockAlign = (std::uint16_t)(numChannels * 2);
  const std::uint32_t byteRate = (std::uint32_t)sampleRate * blockAlign;

  f.write("RIFF", 4);
  WriteLE32(f, 36 + dataBytes);
  f.write("WAVE", 4);
  f.write("fmt ", 4);
  WriteLE32(f, 16);
  WriteLE16(f, 1); // PCM
  WriteLE16(f, (std::uint16_t)numChannels);
  WriteLE32(f, (std::uint32_t)sampleRate);
  WriteLE32(f, byteRate);
  WriteLE16(f, blockAlign);
  WriteLE16(f, 16); // bits per sample
  f.write("data", 4);
  WriteLE32(f, dataBytes);
  for (std::int16_t s : interleaved)
    WriteLE16(f, (std::uint16_t)s);
  f.close();
  return path;
}
} // namespace

TEST_CASE("Stereo WAV loads as averaged mono")
{
  // Two frames. Frame 0: L=16384 (~0.5), R=0 -> 0.25. Frame 1: L=32767 (~1.0),
  // R=-32768 (~-1.0) -> ~0.0.
  const std::vector<std::int16_t> interleaved = {16384, 0, 32767, -32768};
  const auto path = WritePcm16Wav("stereo.wav", 2, 48000, interleaved);

  std::vector<float> audio;
  double sampleRate = 0.0;
  const auto rc = dsp::wav::Load(path.string().c_str(), audio, sampleRate);

  REQUIRE(rc == dsp::wav::LoadReturnCode::SUCCESS);
  REQUIRE(audio.size() == 2); // downmixed to one channel
  CHECK(sampleRate == doctest::Approx(48000.0));
  CHECK(audio[0] == doctest::Approx(0.25).epsilon(0.01));
  CHECK(audio[1] == doctest::Approx(0.0).epsilon(0.01));
}

TEST_CASE("Mono WAV is unchanged by the downmix path")
{
  const std::vector<std::int16_t> interleaved = {16384, -16384, 32767};
  const auto path = WritePcm16Wav("mono.wav", 1, 44100, interleaved);

  std::vector<float> audio;
  double sampleRate = 0.0;
  const auto rc = dsp::wav::Load(path.string().c_str(), audio, sampleRate);

  REQUIRE(rc == dsp::wav::LoadReturnCode::SUCCESS);
  REQUIRE(audio.size() == 3);
  CHECK(sampleRate == doctest::Approx(44100.0));
  CHECK(audio[0] == doctest::Approx(0.5).epsilon(0.01));
  CHECK(audio[1] == doctest::Approx(-0.5).epsilon(0.01));
}

TEST_CASE("WAV loader opens a UTF-8 path with non-ASCII directory names")
{
  // AudioDSPTools #25 / v0.1.2: ifstream(const char*) uses the ANSI code page on
  // Windows, so a UTF-8 IR path fails to open. Load() must take a UTF-8 char*
  // and open via filesystem::path. This is also the VoLum IR-load gate:
  // ImpulseResponse and _StageIR pass that same UTF-8 char* through.
#ifdef _WIN32
  const auto emojiDir = std::filesystem::path(L"volum-ir-\U0001F3B8");
#else
  const auto emojiDir = std::filesystem::path("volum-ir-\xF0\x9F\x8E\xB8");
#endif
  const auto root = TestDir() / emojiDir;
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);

  const std::vector<std::int16_t> interleaved = {16384};
  const auto asciiPath = WritePcm16Wav("unicode-src.wav", 1, 48000, interleaved);
  const auto unicodePath = root / "ir.wav";
  std::filesystem::copy_file(asciiPath, unicodePath, std::filesystem::copy_options::overwrite_existing, ec);
  REQUIRE_FALSE(ec);

  const auto utf8 = [](const std::filesystem::path& p) {
    const auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
  }(unicodePath);

  std::vector<float> audio;
  double sampleRate = 0.0;
  const auto rc = dsp::wav::Load(utf8.c_str(), audio, sampleRate);

  std::filesystem::remove_all(root, ec);

  REQUIRE(rc == dsp::wav::LoadReturnCode::SUCCESS);
  REQUIRE(audio.size() == 1);
  CHECK(sampleRate == doctest::Approx(48000.0));
  CHECK(audio[0] == doctest::Approx(0.5).epsilon(0.01));
}
