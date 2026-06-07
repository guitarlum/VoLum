#include "third_party/doctest.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

#include "activations.h"
#include "get_dsp.h"
#include "slimmable.h"

namespace
{
std::filesystem::path RepoRoot()
{
  namespace fs = std::filesystem;
  return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

std::filesystem::path AmpeteDir()
{
  return RepoRoot() / "rigs" / "Ampete One";
}
} // namespace

TEST_CASE("Ampete NAM files exist")
{
  namespace fs = std::filesystem;
  const auto dir = AmpeteDir();
  INFO(dir.string());
  REQUIRE(fs::is_directory(dir));
  REQUIRE(fs::exists(dir / "AMP-Ampt-1.nam"));
}

TEST_CASE("Load Ampete NAM via nam::get_dsp(path)")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = AmpeteDir() / "AMP-Ampt-1.nam";
  REQUIRE(std::filesystem::exists(path));
  auto model = nam::get_dsp(path);
  REQUIRE(model != nullptr);
  model->Reset(48000.0, 512);
}

TEST_CASE("Load second Ampete NAM via path")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = AmpeteDir() / "G12-Ampt-2.nam";
  REQUIRE(std::filesystem::exists(path));
  auto model = nam::get_dsp(path);
  REQUIRE(model != nullptr);
  model->Reset(44100.0, 256);
}

TEST_CASE("Cached NAM dspData can construct multiple models when copied")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = AmpeteDir() / "AMP-Ampt-1.nam";

  nam::dspData cachedConfig;
  auto loadedModel = nam::get_dsp(path, cachedConfig);
  REQUIRE(loadedModel != nullptr);

  nam::dspData firstCopy = cachedConfig;
  auto firstCachedModel = nam::get_dsp(firstCopy);
  REQUIRE(firstCachedModel != nullptr);
  firstCachedModel->Reset(48000.0, 512);

  nam::dspData secondCopy = cachedConfig;
  auto secondCachedModel = nam::get_dsp(secondCopy);
  REQUIRE(secondCachedModel != nullptr);
  secondCachedModel->Reset(48000.0, 512);

  std::vector<NAM_SAMPLE> input(64, static_cast<NAM_SAMPLE>(0.05));
  std::vector<NAM_SAMPLE> output(64, static_cast<NAM_SAMPLE>(0.0));
  NAM_SAMPLE* inPtr = input.data();
  NAM_SAMPLE* outPtr = output.data();
  secondCachedModel->process(&inPtr, &outPtr, static_cast<int>(output.size()));
  for (const auto sample : output)
    CHECK(std::isfinite(static_cast<double>(sample)));
}

TEST_CASE("Core slimmable NAM example loads and processes")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = RepoRoot() / "NeuralAmpModelerCore" / "example_models" / "slimmable_wavenet.nam";
  REQUIRE(std::filesystem::exists(path));

  auto model = nam::get_dsp(path);
  REQUIRE(model != nullptr);

  auto* slimmable = dynamic_cast<nam::SlimmableModel*>(model.get());
  REQUIRE(slimmable != nullptr);

  constexpr int blockSize = 64;
  const double sampleRate = model->GetExpectedSampleRate() > 0.0 ? model->GetExpectedSampleRate() : 48000.0;
  std::vector<NAM_SAMPLE> input(blockSize, static_cast<NAM_SAMPLE>(0.05));
  std::vector<NAM_SAMPLE> output(blockSize, static_cast<NAM_SAMPLE>(0.0));
  NAM_SAMPLE* inPtr = input.data();
  NAM_SAMPLE* outPtr = output.data();

  for (double size : {0.0, 1.0})
  {
    slimmable->SetSlimmableSize(size);
    model->Reset(sampleRate, blockSize);
    std::fill(output.begin(), output.end(), static_cast<NAM_SAMPLE>(0.0));
    model->process(&inPtr, &outPtr, blockSize);

    CAPTURE(size);
    for (const auto sample : output)
      CHECK(std::isfinite(static_cast<double>(sample)));
  }
}

TEST_CASE("Load all bundled NAM files")
{
  namespace fs = std::filesystem;
  nam::activations::Activation::enable_fast_tanh();

  const auto rigsRoot = RepoRoot() / "rigs";
  REQUIRE(fs::is_directory(rigsRoot));

  std::vector<fs::path> namFiles;
  for (const auto& entry : fs::recursive_directory_iterator(rigsRoot))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".nam"
        && entry.path().parent_path().filename() != "PrePedals")
      namFiles.push_back(entry.path());
  }

  REQUIRE(namFiles.size() == 240);

  for (const auto& path : namFiles)
  {
    CAPTURE(path.string());
    auto model = nam::get_dsp(path);
    REQUIRE(model != nullptr);
    model->Reset(48000.0, 512);
  }
}

// 1.0 contract: every bundled .nam must not just load - it must produce a finite,
// bounded block of output when actually run. Catches NAMs that load metadata fine
// but emit NaN/Inf on first process() (corrupt weights, model/SR mismatch, etc).
// Without this, a bad rig would only fail at runtime in the user's session.
TEST_CASE("Process one block through every bundled main NAM file")
{
  namespace fs = std::filesystem;
  nam::activations::Activation::enable_fast_tanh();

  const auto rigsRoot = RepoRoot() / "rigs";
  REQUIRE(fs::is_directory(rigsRoot));

  std::vector<fs::path> namFiles;
  for (const auto& entry : fs::recursive_directory_iterator(rigsRoot))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".nam"
        && entry.path().parent_path().filename() != "PrePedals")
      namFiles.push_back(entry.path());
  }
  std::sort(namFiles.begin(), namFiles.end());

  constexpr int blockSize = 256;
  constexpr double sampleRate = 48000.0;
  std::vector<NAM_SAMPLE> input(blockSize, static_cast<NAM_SAMPLE>(0.1));
  std::vector<NAM_SAMPLE> output(blockSize, static_cast<NAM_SAMPLE>(0.0));

  for (const auto& path : namFiles)
  {
    CAPTURE(path.string());
    auto model = nam::get_dsp(path);
    REQUIRE(model != nullptr);
    model->Reset(sampleRate, blockSize);
    std::fill(output.begin(), output.end(), static_cast<NAM_SAMPLE>(0.0));
    NAM_SAMPLE* inPtr = input.data();
    NAM_SAMPLE* outPtr = output.data();
    model->process(&inPtr, &outPtr, blockSize);
    double maxAbs = 0.0;
    for (auto s : output)
    {
      const double d = static_cast<double>(s);
      CHECK(std::isfinite(d));
      maxAbs = std::max(maxAbs, std::abs(d));
    }
    // Any musical NAM at 0.1 input must stay well within a few units of full-scale.
    // A model emitting >100 here is broken and would not survive ProcessBlock's
    // downstream stages.
    CHECK(maxAbs < 100.0);
  }
}

TEST_CASE("Load all PRE pedal NAM captures")
{
  namespace fs = std::filesystem;
  nam::activations::Activation::enable_fast_tanh();

  const auto prePedalsRoot = RepoRoot() / "rigs" / "PrePedals";
  REQUIRE(fs::is_directory(prePedalsRoot));

  std::vector<fs::path> namFiles;
  for (const auto& entry : fs::directory_iterator(prePedalsRoot))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".nam")
      namFiles.push_back(entry.path());
  }

  std::sort(namFiles.begin(), namFiles.end());
  for (const auto& path : namFiles)
  {
    CAPTURE(path.string());
    auto model = nam::get_dsp(path);
    REQUIRE(model != nullptr);
    model->Reset(48000.0, 512);
  }
}
