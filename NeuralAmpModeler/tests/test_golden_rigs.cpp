#include "third_party/doctest.h"
#include "golden_helpers.h"

#define DR_WAV_IMPLEMENTATION
#include "third_party/dr_wav.h"

#include "activations.h"
#include "get_dsp.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
constexpr unsigned int kSampleRate = 48000;
constexpr int kBlockSize = 512;
constexpr std::size_t kFrames = kSampleRate * 2;

struct GoldenRigCase
{
  const char* name;
  const char* rigPath;
  const char* goldenFile;
};

std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::filesystem::path GoldenDir()
{
  return std::filesystem::path(__FILE__).parent_path() / "golden_rigs";
}

bool UpdateGoldens()
{
  return std::getenv("VOLUM_UPDATE_GOLDEN_RIGS") != nullptr;
}

std::vector<float> MakeGoldenRigInput()
{
  const auto ref = volum::test::MakeReferenceInput(kFrames, static_cast<double>(kSampleRate), 0x9a17U);
  std::vector<float> input(ref.size());
  for (std::size_t i = 0; i < ref.size(); ++i)
    input[i] = static_cast<float>(std::max(-0.8, std::min(0.8, ref[i])));
  return input;
}

void WriteMonoFloatWav(const std::filesystem::path& path, const std::vector<float>& samples)
{
  std::filesystem::create_directories(path.parent_path());

  drwav_data_format format{};
  format.container = drwav_container_riff;
  format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  format.channels = 1;
  format.sampleRate = kSampleRate;
  format.bitsPerSample = 32;

  drwav wav{};
  REQUIRE(drwav_init_file_write(&wav, path.string().c_str(), &format, nullptr));
  const drwav_uint64 written = drwav_write_pcm_frames(&wav, samples.size(), samples.data());
  drwav_uninit(&wav);
  REQUIRE(written == samples.size());
}

std::vector<float> ReadMonoFloatWav(const std::filesystem::path& path)
{
  unsigned int channels = 0;
  unsigned int sampleRate = 0;
  drwav_uint64 frames = 0;
  float* data = drwav_open_file_and_read_pcm_frames_f32(path.string().c_str(), &channels, &sampleRate, &frames, nullptr);
  REQUIRE(data != nullptr);
  REQUIRE(channels == 1);
  REQUIRE(sampleRate == kSampleRate);

  std::vector<float> samples(data, data + frames);
  drwav_free(data, nullptr);
  return samples;
}

std::vector<float> RenderRig(const std::filesystem::path& rigPath, const std::vector<float>& input)
{
  nam::activations::Activation::enable_fast_tanh();
  auto model = nam::get_dsp(rigPath);
  REQUIRE(model != nullptr);
  model->Reset(static_cast<double>(kSampleRate), kBlockSize);

  std::vector<float> output(input.size(), 0.0f);
  std::vector<NAM_SAMPLE> inBlock(kBlockSize, 0.0f);
  std::vector<NAM_SAMPLE> outBlock(kBlockSize, 0.0f);

  for (std::size_t offset = 0; offset < input.size(); offset += kBlockSize)
  {
    const std::size_t frames = std::min<std::size_t>(kBlockSize, input.size() - offset);
    std::fill(inBlock.begin(), inBlock.end(), static_cast<NAM_SAMPLE>(0.0));
    std::fill(outBlock.begin(), outBlock.end(), static_cast<NAM_SAMPLE>(0.0));
    for (std::size_t i = 0; i < frames; ++i)
      inBlock[i] = static_cast<NAM_SAMPLE>(input[offset + i]);

    NAM_SAMPLE* inPtr = inBlock.data();
    NAM_SAMPLE* outPtr = outBlock.data();
    model->process(&inPtr, &outPtr, static_cast<int>(frames));

    for (std::size_t i = 0; i < frames; ++i)
    {
      const float sample = static_cast<float>(outBlock[i]);
      REQUIRE(std::isfinite(sample));
      output[offset + i] = sample;
    }
  }
  return output;
}

void CompareToGolden(const GoldenRigCase& testCase, const std::vector<float>& actual)
{
  const auto goldenPath = GoldenDir() / testCase.goldenFile;
  if (UpdateGoldens())
  {
    WriteMonoFloatWav(goldenPath, actual);
    return;
  }

  INFO("golden=" << goldenPath.string());
  REQUIRE(std::filesystem::exists(goldenPath));
  const auto expected = ReadMonoFloatWav(goldenPath);
  REQUIRE(expected.size() == actual.size());

  double errorEnergy = 0.0;
  double expectedPeak = 0.0;
  double actualPeak = 0.0;
  for (std::size_t i = 0; i < actual.size(); ++i)
  {
    const double e = static_cast<double>(expected[i]);
    const double a = static_cast<double>(actual[i]);
    const double diff = a - e;
    errorEnergy += diff * diff;
    expectedPeak = std::max(expectedPeak, std::abs(e));
    actualPeak = std::max(actualPeak, std::abs(a));
  }

  const double rmse = std::sqrt(errorEnergy / static_cast<double>(actual.size()));
  const double peakDelta = std::abs(actualPeak - expectedPeak);
  INFO(testCase.name << " rmse=" << rmse << " peakDelta=" << peakDelta);
  CHECK(rmse < 1.0e-4);
  CHECK(peakDelta < 1.0e-3);
}
} // namespace

TEST_CASE("Golden rigs: reference NAM renders stay within tolerance")
{
  const GoldenRigCase cases[] = {
    {"ampete-amp", "rigs/Ampete One/AMP-Ampt-1.nam", "ampete-amp.wav"},
    {"diezel-herbert", "rigs/Diezel Herbert Mk1/AMP-Herb-4.nam", "diezel-herbert.wav"},
    {"pre-minotaur", "rigs/PrePedals/FX-Minotaur-Klon-1.nam", "pre-minotaur.wav"},
  };

  const auto input = MakeGoldenRigInput();
  if (UpdateGoldens())
    WriteMonoFloatWav(GoldenDir() / "_input.wav", input);

  for (const auto& testCase : cases)
  {
    CAPTURE(testCase.name);
    const auto rigPath = RepoRoot() / testCase.rigPath;
    REQUIRE(std::filesystem::exists(rigPath));
    CompareToGolden(testCase, RenderRig(rigPath, input));
  }
}
