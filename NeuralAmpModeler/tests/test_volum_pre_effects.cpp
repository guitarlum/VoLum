#include "third_party/doctest.h"
#include "../VoLumPreEffects.h"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

  CHECK(peak < 1.5);
  CHECK(out[0][samples.size() - 1] < 0.5);
}

// ---- 1176-style FET compressor (iteration 2) ----

TEST_CASE("VoLumCompressor 1176: fast attack rises faster than slow attack")
{
  // After a sudden burst of loud signal, faster attack should produce smaller wet output
  // (more gain reduction having taken effect) at the same time index.
  const size_t frames = 512;
  std::vector<DSP_SAMPLE> burst(frames, 0.9);
  DSP_SAMPLE* ptrs[] = {burst.data()};

  dsp::effect::VoLumCompressor fast;
  fast.SetParams(8.0, 4.0, /*attack*/ 0.05, /*release*/ 200.0, 1.0, 0.0, 48000.0);
  auto** outFast = fast.Process(ptrs, 1, frames);
  std::vector<DSP_SAMPLE> fastOut(outFast[0], outFast[0] + frames);

  // Reset input data because Process may have written into shared buffers.
  std::fill(burst.begin(), burst.end(), 0.9);

  dsp::effect::VoLumCompressor slow;
  slow.SetParams(8.0, 4.0, /*attack*/ 1.0, /*release*/ 200.0, 1.0, 0.0, 48000.0);
  auto** outSlow = slow.Process(ptrs, 1, frames);
  std::vector<DSP_SAMPLE> slowOut(outSlow[0], outSlow[0] + frames);

  // After a few ms (much longer than fast attack, comparable to slow attack), the fast
  // compressor should have pulled the wet signal lower than the slow one.
  const size_t checkIdx = static_cast<size_t>(48000.0 * 0.005); // ~5 ms in
  REQUIRE(checkIdx < frames);
  CHECK(std::abs(fastOut[checkIdx]) <= std::abs(slowOut[checkIdx]) + 1e-3);
}

TEST_CASE("VoLumCompressor 1176: ratio param is ignored (fixed 4:1)")
{
  // Two compressors with the same settings but different `ratio` arg should produce
  // identical output - the ratio parameter is locked internally.
  const size_t frames = 256;
  std::vector<DSP_SAMPLE> samples(frames, 0.6);
  DSP_SAMPLE* ptrsA[] = {samples.data()};

  dsp::effect::VoLumCompressor a;
  a.SetParams(6.0, 4.0, 0.5, 200.0, 1.0, 0.0, 48000.0);
  auto** outA = a.Process(ptrsA, 1, frames);
  std::vector<DSP_SAMPLE> aOut(outA[0], outA[0] + frames);

  std::fill(samples.begin(), samples.end(), 0.6);
  dsp::effect::VoLumCompressor b;
  b.SetParams(6.0, 20.0, 0.5, 200.0, 1.0, 0.0, 48000.0); // ratio differs - should be ignored
  auto** outB = b.Process(ptrsA, 1, frames);

  for (size_t i = 0; i < frames; ++i)
    CHECK(outB[0][i] == doctest::Approx(aOut[i]).epsilon(1e-6));
}

TEST_CASE("VoLumCompressor 1176: FET pre-detector saturation present at high drive")
{
  // Feed a clean sine at high amplitude and cranked Input. Output should contain
  // measurable harmonic content (the FET nonlinearity at work).
  const double sr = 48000.0;
  const double freq = 440.0;
  const size_t frames = 4096;

  std::vector<DSP_SAMPLE> samples(frames);
  for (size_t i = 0; i < frames; ++i)
    samples[i] = static_cast<DSP_SAMPLE>(0.85 * std::sin(2.0 * M_PI * freq * i / sr));
  DSP_SAMPLE* ptrs[] = {samples.data()};

  dsp::effect::VoLumCompressor comp;
  comp.SetParams(10.0, 4.0, 0.4, 250.0, 1.0, 0.0, sr);
  auto** out = comp.Process(ptrs, 1, frames);

  // Check that the output is not a pure sine: difference from a least-squares-fit sine
  // at the fundamental should be non-trivial (i.e. harmonic content exists).
  double sumSinSq = 0.0, sumCosSq = 0.0, sumSinX = 0.0, sumCosX = 0.0;
  for (size_t i = 0; i < frames; ++i)
  {
    const double phase = 2.0 * M_PI * freq * i / sr;
    const double s = std::sin(phase);
    const double c = std::cos(phase);
    const double x = static_cast<double>(out[0][i]);
    sumSinSq += s * s;
    sumCosSq += c * c;
    sumSinX += s * x;
    sumCosX += c * x;
  }
  const double aFit = sumSinX / sumSinSq;
  const double bFit = sumCosX / sumCosSq;

  double residualEnergy = 0.0;
  double signalEnergy = 0.0;
  for (size_t i = 0; i < frames; ++i)
  {
    const double phase = 2.0 * M_PI * freq * i / sr;
    const double fit = aFit * std::sin(phase) + bFit * std::cos(phase);
    const double x = static_cast<double>(out[0][i]);
    residualEnergy += (x - fit) * (x - fit);
    signalEnergy += x * x;
  }
  const double thd = std::sqrt(residualEnergy / std::max(signalEnergy, 1.0e-12));
  // Expect at least ~1% non-fundamental content - FET saturation is meaningful at
  // high input drive on a nearly-full-scale sine.
  CHECK(thd > 0.01);
}

TEST_CASE("VoLumCompressor 1176: attack range covers 0.02 ms target")
{
  // Even at the new fast attack target, no NaN and bounded output.
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(7.0, 4.0, 0.02, 50.0, 1.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples(128, 0.7);
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
