#include "third_party/doctest.h"
#include <cstddef>
#include <vector>

// Mirrors NeuralAmpModeler::_ProcessInput contribution to mono bus (simplified: one frame).
static float MixInputMono(float inL, float inR, size_t nChansIn, double baseGain, bool appApi)
{
  double gain = baseGain;
  if (!appApi && nChansIn > 0)
    gain /= static_cast<double>(nChansIn);
  float acc = 0.f;
  if (nChansIn > 0)
    acc += static_cast<float>(gain * static_cast<double>(inL));
  if (nChansIn > 1)
    acc += static_cast<float>(gain * static_cast<double>(inR));
  return acc;
}

TEST_CASE("APP_API stereo sum uses full per-channel gain")
{
  const float y = MixInputMono(1.f, 1.f, 2, 1.0, true);
  DOCTEST_CHECK(y == doctest::Approx(2.f));
}

TEST_CASE("DAW path averages stereo before gain")
{
  const float y = MixInputMono(1.f, 1.f, 2, 1.0, false);
  DOCTEST_CHECK(y == doctest::Approx(1.f));
}

static float ApplyOutputGain(float x, double outGain, bool appApi)
{
  const double y = outGain * static_cast<double>(x);
  if (appApi)
    return static_cast<float>(y < -1.0 ? -1.0 : (y > 1.0 ? 1.0 : y));
  return static_cast<float>(y);
}

TEST_CASE("APP_API clamps output")
{
  DOCTEST_CHECK(ApplyOutputGain(10.f, 1.0, true) == doctest::Approx(1.f));
  DOCTEST_CHECK(ApplyOutputGain(-10.f, 1.0, true) == doctest::Approx(-1.f));
}

TEST_CASE("DAW path does not clamp output in plugin")
{
  DOCTEST_CHECK(ApplyOutputGain(10.f, 1.0, false) == doctest::Approx(10.f));
}
