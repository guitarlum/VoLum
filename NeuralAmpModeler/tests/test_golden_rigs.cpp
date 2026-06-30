#include "third_party/doctest.h"
#include "golden_helpers.h"

#define DR_WAV_IMPLEMENTATION
#include "third_party/dr_wav.h"

#include "activations.h"
#include "get_dsp.h"
#include "json.hpp"
#include "slimmable.h"
#include "wavenet/a2_fast.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if !defined(NAM_ENABLE_A2_FAST)
  #error "Golden rig tests must exercise the same NAM_ENABLE_A2_FAST path as product builds."
#endif

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

nlohmann::json ReadJson(const std::filesystem::path& path)
{
  std::ifstream input(path);
  REQUIRE(input.good());
  nlohmann::json json;
  input >> json;
  return json;
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

TEST_CASE("A2 rigs activate the specialized fast WaveNet path")
{
  const auto rigPath = RepoRoot() / "rigs/Ampete One/AMP-Ampt-1.nam";
  const auto json = ReadJson(rigPath);
  REQUIRE(json.at("architecture").get<std::string>() == "SlimmableContainer");

  const auto& submodels = json.at("config").at("submodels");
  REQUIRE(submodels.size() == 2);

  int channels = 0;
  CHECK(nam::wavenet::a2_fast::is_a2_shape(submodels.at(0).at("model").at("config"), &channels));
  CHECK(channels == 3);

  channels = 0;
  CHECK(nam::wavenet::a2_fast::is_a2_shape(submodels.at(1).at("model").at("config"), &channels));
  CHECK(channels == 8);
}

TEST_CASE("A2 detector rejects non-A2 WaveNet configs (strict, no false positives)")
{
  // Guard the fast path from hijacking ordinary models. The detector reads the
  // model "config" sub-object and must reject anything that is not the exact A2
  // signature, defensively (never throwing on arbitrary JSON). All bundled rigs
  // are A2 containers, so the negatives are synthesized to mirror the generic
  // WaveNet shapes the detector must turn down.
  using nam::wavenet::a2_fast::is_a2_shape;
  int channels = -1;

  // Empty / unrelated object: no "layers" key at all.
  CHECK_FALSE(is_a2_shape(nlohmann::json::object(), &channels));
  CHECK_FALSE(is_a2_shape(nlohmann::json{{"foo", 1}, {"bar", "baz"}}, &channels));

  // Generic WaveNet with the usual TWO layer arrays (A2 has exactly one).
  nlohmann::json twoArrays;
  twoArrays["layers"] = nlohmann::json::array();
  twoArrays["layers"].push_back({{"channels", 16}});
  twoArrays["layers"].push_back({{"channels", 8}});
  twoArrays["head_scale"] = 0.02;
  CHECK_FALSE(is_a2_shape(twoArrays, &channels));

  // One layer array but a post-stack head present (A2 has none).
  nlohmann::json withHead;
  withHead["layers"] = nlohmann::json::array();
  withHead["layers"].push_back({{"channels", 8}});
  withHead["head"] = {{"channels", 1}};
  withHead["head_scale"] = 0.01;
  CHECK_FALSE(is_a2_shape(withHead, &channels));

  // One layer array, no head, but head_scale missing (must be a trained scalar).
  nlohmann::json noScale;
  noScale["layers"] = nlohmann::json::array();
  noScale["layers"].push_back({{"channels", 8}});
  CHECK_FALSE(is_a2_shape(noScale, &channels));
}

TEST_CASE("A2 core load and prewarm timing is visible in test logs")
{
  const auto rigPath = RepoRoot() / "rigs/Ampete One/AMP-Ampt-1.nam";
  const auto start = std::chrono::steady_clock::now();
  auto model = nam::get_dsp(rigPath);
  REQUIRE(model != nullptr);
  model->Reset(static_cast<double>(kSampleRate), kBlockSize);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

  std::cout << "A2 load+prewarm " << rigPath.filename().string() << ": " << elapsed.count() << " ms" << std::endl;
#if defined(VOLUM_TEST_SANITIZERS)
  CHECK(elapsed < std::chrono::seconds(15));
#else
  CHECK(elapsed < std::chrono::seconds(5));
#endif
}

TEST_CASE("A2 container can lazily activate the Lite submodel after load")
{
  const auto rigPath = RepoRoot() / "rigs/Ampete One/AMP-Ampt-1.nam";
  auto model = nam::get_dsp(rigPath);
  REQUIRE(model != nullptr);
  model->Reset(static_cast<double>(kSampleRate), kBlockSize);

  auto* slimmable = dynamic_cast<nam::SlimmableModel*>(model.get());
  REQUIRE(slimmable != nullptr);
  slimmable->SetSlimmableSize(0.0);

  std::vector<NAM_SAMPLE> input(kBlockSize, static_cast<NAM_SAMPLE>(0.05));
  std::vector<NAM_SAMPLE> output(kBlockSize, static_cast<NAM_SAMPLE>(0.0));
  NAM_SAMPLE* inputPtr = input.data();
  NAM_SAMPLE* outputPtr = output.data();
  model->process(&inputPtr, &outputPtr, kBlockSize);

  for (const auto sample : output)
    CHECK(std::isfinite(static_cast<double>(sample)));
}

// Demonstrates that Lite mode does real work: the Lite slice (channels_3) and the
// Full slice (channels_8) of the SAME A2 container produce different audio and the
// Lite slice is measurably cheaper to run. The timings are printed so the CPU win
// is visible in the test log (same spirit as the load+prewarm timing case above).
TEST_CASE("A2 Lite slice differs from Full and is cheaper to run")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto rigPath = RepoRoot() / "rigs/Ampete One/AMP-Ampt-1.nam";
  auto model = nam::get_dsp(rigPath);
  REQUIRE(model != nullptr);
  auto* slimmable = dynamic_cast<nam::SlimmableModel*>(model.get());
  REQUIRE(slimmable != nullptr);

  // A repeatable, non-trivial guitar-like input (cheap deterministic waveform).
  std::vector<NAM_SAMPLE> input(kBlockSize);
  for (int i = 0; i < kBlockSize; ++i)
    input[i] = static_cast<NAM_SAMPLE>(0.2 * std::sin(0.05 * i));
  std::vector<NAM_SAMPLE> output(kBlockSize, static_cast<NAM_SAMPLE>(0.0));
  NAM_SAMPLE* inPtr = input.data();
  NAM_SAMPLE* outPtr = output.data();

  // Render one block on each slice and snapshot the output to prove the slice
  // actually changed which network is computing.
  auto renderOnce = [&](double size) {
    slimmable->SetSlimmableSize(size);
    model->Reset(static_cast<double>(kSampleRate), kBlockSize);
    std::fill(output.begin(), output.end(), static_cast<NAM_SAMPLE>(0.0));
    model->process(&inPtr, &outPtr, kBlockSize);
    return output; // copy
  };

  const auto liteOut = renderOnce(0.0); // Lite (channels_3)
  const auto fullOut = renderOnce(1.0); // Full (channels_8)

  bool anyDifferent = false;
  for (int i = 0; i < kBlockSize; ++i)
  {
    CHECK(std::isfinite(static_cast<double>(liteOut[i])));
    CHECK(std::isfinite(static_cast<double>(fullOut[i])));
    if (std::abs(static_cast<double>(liteOut[i]) - static_cast<double>(fullOut[i])) > 1e-6)
      anyDifferent = true;
  }
  CHECK(anyDifferent); // different slice => different audio

  // Throughput: process ~2 s of audio per slice and time it. channels_3 vs
  // channels_8 is a large gap, so Lite should be clearly faster.
  auto benchmark = [&](double size) {
    slimmable->SetSlimmableSize(size);
    model->Reset(static_cast<double>(kSampleRate), kBlockSize);
    const int blocks = static_cast<int>(kFrames / kBlockSize);
    const auto start = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
      model->process(&inPtr, &outPtr, kBlockSize);
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
  };

  benchmark(1.0); // warm caches
  const auto fullUs = benchmark(1.0);
  benchmark(0.0); // warm caches
  const auto liteUs = benchmark(0.0);

  const double fullMs = fullUs.count() / 1000.0;
  const double liteMs = liteUs.count() / 1000.0;
  std::cout << "A2 throughput (" << (kFrames) << " frames): Full(channels_8)=" << fullMs << " ms, Lite(channels_3)="
            << liteMs << " ms, speedup=" << (fullMs / std::max(liteMs, 1e-6)) << "x" << std::endl;

  CHECK(liteMs < fullMs); // Lite is cheaper
}
