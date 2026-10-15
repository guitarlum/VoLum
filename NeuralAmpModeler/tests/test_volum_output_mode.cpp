#include "third_party/doctest.h"
#include "../VoLumOutputMode.h"

#include <cstring>
#include <vector>

namespace
{
// Stand-in for WDL_TypedBuf<bool>: Resize leaves new slots holding whatever was
// in memory, which is what the real buffer does.
struct GarbageOnResizeBuf
{
  std::vector<bool> v;
  int GetSize() const { return static_cast<int>(v.size()); }
  void Resize(int n) { v.resize(static_cast<size_t>(n), true); }
  struct Ref
  {
    std::vector<bool>* v;
    std::vector<bool>::reference operator[](int i) { return (*v)[static_cast<size_t>(i)]; }
  };
  Ref Get() { return {&v}; }
};
} // namespace

TEST_CASE("Growing the output-mode disabled flags leaves Raw and Normalized selectable")
{
  GarbageOnResizeBuf disabled;
  volum::EnsureRadioDisabledStates(disabled, volum::kOutputModeCount);
  REQUIRE(disabled.GetSize() == volum::kOutputModeCount);
  CHECK_FALSE(disabled.v[volum::kOutputModeRaw]);
  CHECK_FALSE(disabled.v[volum::kOutputModeNormalized]);
  CHECK_FALSE(disabled.v[volum::kOutputModeCalibrated]);

  // Calibrated then locks alone, and a second call keeps that lock.
  disabled.v[volum::kOutputModeCalibrated] = true;
  volum::EnsureRadioDisabledStates(disabled, volum::kOutputModeCount);
  CHECK_FALSE(disabled.v[volum::kOutputModeRaw]);
  CHECK_FALSE(disabled.v[volum::kOutputModeNormalized]);
  CHECK(disabled.v[volum::kOutputModeCalibrated]);
}

TEST_CASE("Output mode labels stay aligned with param enum")
{
  CHECK(volum::kOutputModeCount == 3);
  CHECK(volum::kOutputModeDefault == volum::kOutputModeNormalized);
  CHECK(std::strcmp(volum::kOutputModeLabels[0], "Raw") == 0);
  CHECK(std::strcmp(volum::kOutputModeLabels[1], "Normalized") == 0);
  CHECK(std::strcmp(volum::kOutputModeLabels[2], "Calibrated") == 0);
}

TEST_CASE("Output mode gain helper normalizes loudness to target")
{
  volum::OutputModeModelInfo modelInfo;
  modelInfo.hasLoudness = true;
  modelInfo.loudness = -24.0;

  const double gainDb = volum::ComputeOutputModeGainDb(0.0, volum::kOutputModeNormalized, modelInfo, 0.0);
  CHECK(gainDb == doctest::Approx(6.0));
}

TEST_CASE("Output mode gain helper applies calibrated offset")
{
  volum::OutputModeModelInfo modelInfo;
  modelInfo.hasOutputLevel = true;
  modelInfo.outputLevel = -12.0;

  const double gainDb = volum::ComputeOutputModeGainDb(0.0, volum::kOutputModeCalibrated, modelInfo, -18.0);
  CHECK(gainDb == doctest::Approx(6.0));
}

TEST_CASE("Output mode gain helper leaves raw mode unchanged")
{
  volum::OutputModeModelInfo modelInfo;
  modelInfo.hasLoudness = true;
  modelInfo.loudness = -30.0;
  modelInfo.hasOutputLevel = true;
  modelInfo.outputLevel = -12.0;

  CHECK(volum::ComputeOutputModeGainDb(3.5, volum::kOutputModeRaw, modelInfo, -18.0) == doctest::Approx(3.5));
}
