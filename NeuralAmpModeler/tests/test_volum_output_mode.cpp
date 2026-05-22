#include "third_party/doctest.h"
#include "../VoLumOutputMode.h"

#include <cstring>

TEST_CASE("Output mode labels stay aligned with param enum")
{
  CHECK(volum::kOutputModeCount == 3);
  CHECK(volum::kOutputModeDefault == 1);
  CHECK(std::strcmp(volum::kOutputModeLabels[0], "Raw") == 0);
  CHECK(std::strcmp(volum::kOutputModeLabels[1], "Normalized") == 0);
  CHECK(std::strcmp(volum::kOutputModeLabels[2], "Calibrated") == 0);
}

TEST_CASE("Output mode gain helper normalizes loudness to target")
{
  volum::OutputModeModelInfo modelInfo;
  modelInfo.hasLoudness = true;
  modelInfo.loudness = -24.0;

  const double gainDb = volum::ComputeOutputModeGainDb(0.0, 1, modelInfo, 0.0);
  CHECK(gainDb == doctest::Approx(6.0));
}

TEST_CASE("Output mode gain helper applies calibrated offset")
{
  volum::OutputModeModelInfo modelInfo;
  modelInfo.hasOutputLevel = true;
  modelInfo.outputLevel = -12.0;

  const double gainDb = volum::ComputeOutputModeGainDb(0.0, 2, modelInfo, -18.0);
  CHECK(gainDb == doctest::Approx(6.0));
}

TEST_CASE("Output mode gain helper leaves raw mode unchanged")
{
  volum::OutputModeModelInfo modelInfo;
  modelInfo.hasLoudness = true;
  modelInfo.loudness = -30.0;
  modelInfo.hasOutputLevel = true;
  modelInfo.outputLevel = -12.0;

  CHECK(volum::ComputeOutputModeGainDb(3.5, 0, modelInfo, -18.0) == doctest::Approx(3.5));
}
