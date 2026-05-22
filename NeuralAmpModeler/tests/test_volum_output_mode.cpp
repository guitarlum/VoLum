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
