// VoLum: regression coverage for the multi-channel -> mono downmix added to
// AudioDSPTools/dsp/wav.cpp. Commercial IRs are frequently stereo; the loader
// must accept them (averaging channels) instead of rejecting with ERROR_NOT_MONO.

#include "third_party/doctest.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
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
