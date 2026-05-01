#include "third_party/doctest.h"
#include "../VoLumPreEffects.h"

#include <algorithm>
#include <cmath>
#include <vector>

TEST_CASE("VoLumPreEq noon settings preserve finite audio")
{
  dsp::effect::VoLumPreEq eq;
  eq.Reset(48000.0, 64);
  eq.SetParams(5.0, 5.0, 650.0, 5.0);

  std::vector<DSP_SAMPLE> samples(64, 0.1);
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = eq.Process(ptrs, 1, samples.size());

  for (size_t i = 0; i < samples.size(); ++i)
    CHECK(std::isfinite(out[0][i]));
}

TEST_CASE("VoLumCompressor outputs finite bounded signal")
{
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(8.0, 8.0, 2.0, 120.0, 1.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples(128, 1.0);
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = comp.Process(ptrs, 1, samples.size());

  double peak = 0.0;
  for (size_t i = 0; i < samples.size(); ++i)
  {
    CHECK(std::isfinite(out[0][i]));
    peak = std::max(peak, std::abs(static_cast<double>(out[0][i])));
  }
  CHECK(peak < 2.5);
}

TEST_CASE("VoLumCompressor dry mix passes input unchanged")
{
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(10.0, 20.0, 0.1, 80.0, 0.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples{0.0, 0.25, -0.5, 0.75, -1.0};
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = comp.Process(ptrs, 1, samples.size());

  for (size_t i = 0; i < samples.size(); ++i)
    CHECK(out[0][i] == doctest::Approx(samples[i]));
}

TEST_CASE("VoLumCompressor amount reduces sustained loud signal")
{
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(10.0, 20.0, 0.1, 80.0, 1.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples(256, 1.0);
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = comp.Process(ptrs, 1, samples.size());

  double peak = 0.0;
  for (size_t i = 0; i < samples.size(); ++i)
    peak = std::max(peak, std::abs(static_cast<double>(out[0][i])));

  CHECK(peak < 0.8);
  CHECK(out[0][samples.size() - 1] < 0.2);
}
