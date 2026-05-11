#include "third_party/doctest.h"
#include "../../AudioDSPTools/dsp/Delay.h"
#include "../../AudioDSPTools/dsp/Reverb.h"
#include "../VoLumMasterSafety.h"
#include <cmath>
#include <limits>
#include <vector>

static bool hasNaN(double* buf, size_t n)
{
  for (size_t i = 0; i < n; i++)
    if (std::isnan(buf[i]) || std::isinf(buf[i]))
      return true;
  return false;
}

static double energy(double* buf, size_t n)
{
  double sum = 0.0;
  for (size_t i = 0; i < n; i++)
    sum += buf[i] * buf[i];
  return sum;
}

// Delay

TEST_CASE("Delay: no NaN on first block after SetParams")
{
  dsp::effect::Delay delay;
  delay.SetParams(320.0, 0.5, 0.5, 1, 44100.0);

  const size_t frames = 128;
  std::vector<double> inL(frames, 0.5), inR(frames, 0.5);
  double* inputs[2] = {inL.data(), inR.data()};

  auto** out = delay.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  REQUIRE_FALSE(hasNaN(out[1], frames));
}

TEST_CASE("Delay: mix=0 passes input through unchanged")
{
  dsp::effect::Delay delay;
  delay.SetParams(100.0, 0.5, 0.0, 1, 44100.0);

  const size_t frames = 64;
  std::vector<double> inL(frames, 0.75);
  double* inputs[1] = {inL.data()};

  auto** out = delay.Process(inputs, 1, frames);
  for (size_t i = 0; i < frames; i++)
    CHECK(out[0][i] == doctest::Approx(0.75));
}

TEST_CASE("Delay: Reset clears state, no stale audio leaks")
{
  dsp::effect::Delay delay;
  delay.SetParams(50.0, 0.9, 1.0, 1, 44100.0);

  const size_t frames = 256;
  std::vector<double> loud(frames, 1.0);
  double* inputs[1] = {loud.data()};
  delay.Process(inputs, 1, frames);
  delay.Process(inputs, 1, frames);

  delay.Reset();
  delay.SetParams(50.0, 0.9, 1.0, 1, 44100.0);

  std::vector<double> silence(frames, 0.0);
  double* silIn[1] = {silence.data()};
  auto** out = delay.Process(silIn, 1, frames);

  double maxVal = 0.0;
  for (size_t i = 0; i < frames; i++)
    maxVal = std::max(maxVal, std::abs(out[0][i]));
  CHECK(maxVal < 0.01);
}

TEST_CASE("Delay: all staging modes produce output without NaN")
{
  for (int mode = 0; mode < 3; mode++)
  {
    dsp::effect::Delay delay;
    delay.SetParams(200.0, 0.4, 0.5, mode, 48000.0);

    const size_t frames = 512;
    std::vector<double> inL(frames, 0.3), inR(frames, -0.3);
    double* inputs[2] = {inL.data(), inR.data()};

    auto** out = delay.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
  }
}

TEST_CASE("Delay: reverse mode mix=0 passes input through unchanged")
{
  dsp::effect::Delay delay;
  delay.SetParams(100.0, 0.5, 0.0, dsp::effect::Delay::kModeReverse, 1000.0);

  const size_t frames = 100;
  std::vector<double> inL(frames, 0.25);
  double* inputs[1] = {inL.data()};

  for (int block = 0; block < 3; ++block)
  {
    auto** out = delay.Process(inputs, 1, frames);
    for (size_t i = 0; i < frames; i++)
      CHECK(out[0][i] == doctest::Approx(0.25));
  }
}

TEST_CASE("Delay: reverse mode plays completed slice backwards")
{
  dsp::effect::Delay delay;
  delay.SetParams(100.0, 0.0, 1.0, dsp::effect::Delay::kModeReverse, 1000.0);

  const size_t frames = 100;
  std::vector<double> impulse(frames, 0.0);
  impulse[20] = 1.0;
  double* inputs[1] = {impulse.data()};
  delay.Process(inputs, 1, frames);

  std::fill(impulse.begin(), impulse.end(), 0.0);
  auto** out = delay.Process(inputs, 1, frames);

  REQUIRE_FALSE(hasNaN(out[0], frames));
  CHECK(std::abs(out[0][79]) > 0.2);
  CHECK(std::abs(out[0][20]) < 0.001);
}

TEST_CASE("Delay: reverse mode mix=1 keeps dry through (additive blend)")
{
  // Reverse blend law was changed from `dry*(1-mix) + wet*mix` (linear crossfade) to
  // `dry + wet*mix` (additive) so that engaging Reverse no longer audibly drops volume
  // vs Digital / Analog at the same Mix. At Mix=1 the dry impulse is preserved in the
  // first (capture) block, and the second (playback) block still places a strong
  // reversed tap near the end of the slice.
  dsp::effect::Delay delay;
  delay.SetParams(100.0, 0.0, 1.0, dsp::effect::Delay::kModeReverse, 1000.0);

  const size_t frames = 100;
  std::vector<double> impulse(frames, 0.0);
  impulse[20] = 1.0;
  double* inputs[1] = {impulse.data()};

  auto** first = delay.Process(inputs, 1, frames);
  REQUIRE_FALSE(hasNaN(first[0], frames));
  CHECK(first[0][20] == doctest::Approx(1.0).epsilon(0.001));

  std::fill(impulse.begin(), impulse.end(), 0.0);
  auto** second = delay.Process(inputs, 1, frames);
  REQUIRE_FALSE(hasNaN(second[0], frames));
  CHECK(std::abs(second[0][79]) > 0.2);
}

TEST_CASE("Delay: reverse mode high feedback stays bounded")
{
  dsp::effect::Delay delay;
  delay.SetParams(10.0, 0.99, 0.8, dsp::effect::Delay::kModeReverse, 44100.0);
  const size_t frames = 128;
  std::vector<double> impulse(frames, 0.0);
  impulse[32] = 1.0;
  double* inputs[1] = {impulse.data()};
  double maxVal = 0.0;

  for (int block = 0; block < 30; block++)
  {
    auto** out = delay.Process(inputs, 1, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::abs(out[0][i]));
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }

  CHECK(maxVal < 10.0);
}

TEST_CASE("Delay: reverse mode constant input has no slice-boundary amplitude dip")
{
  // Overlap-add (two voices, length/2 stagger, triangle/sin^2 windows that sum to
  // unity) replaced the old single-slice swap. Old impl dipped the wet bus to ~0
  // for ~64 samples at every slice boundary; the OLA design must keep the wet bus
  // ~constant, so a DC input (mix=1, fb=0) reaches a steady state with very small
  // sample-to-sample amplitude variation across slice boundaries.
  dsp::effect::Delay delay;
  const double sr = 1000.0;
  const double timeMs = 50.0; // 50 samples / slice
  delay.SetParams(timeMs, 0.0, 1.0, dsp::effect::Delay::kModeReverse, sr);

  const size_t frames = 1000;
  std::vector<double> in(frames, 0.5);
  double* inputs[1] = {in.data()};

  // Run until wet bus is fully primed (multiple slice cycles).
  for (int b = 0; b < 4; ++b)
    delay.Process(inputs, 1, frames);

  auto** out = delay.Process(inputs, 1, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));

  // Steady-state: dry(0.5) + wet(~0.5 from OLA sum) ~= 1.0. The min over the last
  // 800 samples must stay close to the max - i.e. no dip back toward dry-only.
  double minOut = std::numeric_limits<double>::infinity();
  double maxOut = -std::numeric_limits<double>::infinity();
  for (size_t i = 200; i < frames; i++)
  {
    minOut = std::min(minOut, out[0][i]);
    maxOut = std::max(maxOut, out[0][i]);
  }
  INFO("min=" << minOut << " max=" << maxOut);
  // Old impl: minOut would dip to ~0.5 (dry only) at slice edges. OLA: ripple stays small.
  CHECK(minOut > 0.85);
  CHECK((maxOut - minOut) < 0.15);
}

TEST_CASE("Delay: reverse mode time change does not glitch wet bus to silence")
{
  // The old impl called _ResetReverseState() whenever the segment frame count
  // changed, wiping both buffers and producing ~2 segments of silence on every
  // delay-time tweak. The OLA impl keeps in-flight voices on their original
  // length; only newly launched voices use the new length, so the wet bus stays
  // continuous across time-knob changes.
  dsp::effect::Delay delay;
  const double sr = 1000.0;
  delay.SetParams(80.0, 0.0, 1.0, dsp::effect::Delay::kModeReverse, sr);

  const size_t frames = 500;
  std::vector<double> in(frames, 0.4);
  double* inputs[1] = {in.data()};

  // Prime the wet bus.
  for (int b = 0; b < 4; ++b)
    delay.Process(inputs, 1, frames);

  // Sweep the delay time across multiple values mid-stream and confirm the wet
  // bus never collapses to dry-only for any sustained stretch.
  const double newTimes[] = {120.0, 60.0, 200.0, 95.0};
  for (double t : newTimes)
  {
    delay.SetParams(t, 0.0, 1.0, dsp::effect::Delay::kModeReverse, sr);
    auto** out = delay.Process(inputs, 1, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    // Skip the first 200 samples (any cross-fade on launch) and require the rest
    // to clear dry-only by a healthy margin.
    double minOut = std::numeric_limits<double>::infinity();
    for (size_t i = 200; i < frames; i++)
      minOut = std::min(minOut, out[0][i]);
    INFO("t=" << t << " minOut=" << minOut);
    CHECK(minOut > 0.55); // dry alone would be 0.4; OLA keeps wet contributing.
  }
}

TEST_CASE("Delay: Prepare keeps output storage stable across parameter updates")
{
  dsp::effect::Delay delay;
  constexpr size_t frames = 128;
  delay.Prepare(2, frames, 48000.0);
  delay.SetParams(120.0, 0.3, 0.2, 1, 48000.0);

  std::vector<double> inL(frames, 0.2), inR(frames, -0.2);
  double* inputs[2] = {inL.data(), inR.data()};
  auto** first = delay.Process(inputs, 2, frames);
  auto* firstL = first[0];
  auto* firstR = first[1];

  delay.SetParams(480.0, 0.6, 0.5, 2, 48000.0);
  auto** second = delay.Process(inputs, 2, frames);
  CHECK(second == first);
  CHECK(second[0] == firstL);
  CHECK(second[1] == firstR);
}

TEST_CASE("Delay: high feedback stays bounded")
{
  dsp::effect::Delay delay;
  delay.SetParams(10.0, 0.99, 0.8, 1, 44100.0);
  const size_t frames = 128;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[1] = {impulse.data()};
  double maxVal = 0.0;
  for (int block = 0; block < 20; block++)
  {
    auto** out = delay.Process(inputs, 1, frames);
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::abs(out[0][i]));
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
  CHECK(maxVal < 10.0);
}

// Reverb

TEST_CASE("Reverb: no NaN on first block after SetParams")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.5, 3.0, 6.0, 20.0, 0.5, 0, 44100.0);

  const size_t frames = 128;
  std::vector<double> inL(frames, 0.5), inR(frames, 0.5);
  double* inputs[2] = {inL.data(), inR.data()};

  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  REQUIRE_FALSE(hasNaN(out[1], frames));
}

TEST_CASE("Reverb: mix=0 passes input through unchanged")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.0, 3.0, 6.0, 20.0, 0.5, 0, 44100.0);

  const size_t frames = 64;
  std::vector<double> inL(frames, 0.6), inR(frames, 0.6);
  double* inputs[2] = {inL.data(), inR.data()};

  auto** out = reverb.Process(inputs, 2, frames);
  for (size_t i = 0; i < frames; i++)
    CHECK(out[0][i] == doctest::Approx(0.6).epsilon(0.001));
}

TEST_CASE("Reverb: Hall (mode 0) produces output without NaN")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.5, 2.0, 5.0, 20.0, 0.5, 0, 48000.0);
  const size_t frames = 512;
  std::vector<double> inL(frames, 0.3), inR(frames, -0.3);
  double* inputs[2] = {inL.data(), inR.data()};
  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  REQUIRE_FALSE(hasNaN(out[1], frames));
}

TEST_CASE("Reverb: Plate (mode 1) produces output without NaN")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.5, 2.0, 5.0, 20.0, 0.5, 1, 48000.0);
  const size_t frames = 512;
  std::vector<double> inL(frames, 0.3), inR(frames, -0.3);
  double* inputs[2] = {inL.data(), inR.data()};
  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  REQUIRE_FALSE(hasNaN(out[1], frames));
}

TEST_CASE("Reverb: Oktaverb (mode 2) produces bounded output without NaN")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.7, 4.0, 4.5, 20.0, 0.8, 2, 48000.0);
  const size_t frames = 512;
  std::vector<double> inL(frames, 0.25), inR(frames, -0.25);
  double* inputs[2] = {inL.data(), inR.data()};

  double maxVal = 0.0;
  for (int block = 0; block < 8; ++block)
  {
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
  }
  CHECK(maxVal < 10.0);
}

TEST_CASE("Reverb: Oktaverb high shimmer stays stable over long run and Hall recovers")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.8, 10.0, 6.0, 20.0, 1.0, 2, 48000.0);

  const size_t frames = 512;
  std::vector<double> impulse(frames, 0.0);
  double* inputs[2] = {impulse.data(), impulse.data()};
  double maxVal = 0.0;

  for (int block = 0; block < 400; ++block)
  {
    impulse[0] = (block == 0) ? 1.0 : 0.0;
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
  }

  CHECK(maxVal < 10.0);

  reverb.SetParams(0.8, 10.0, 6.0, 20.0, 0.0, 0, 48000.0);
  impulse[0] = 1.0;
  auto** hallOut = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(hallOut[0], frames));
  REQUIRE_FALSE(hasNaN(hallOut[1], frames));
}

TEST_CASE("Reverb: Oktaverb pre-delay changes do not poison Hall state")
{
  dsp::effect::Reverb reverb;
  const size_t frames = 256;
  std::vector<double> impulse(frames, 0.0);
  double* inputs[2] = {impulse.data(), impulse.data()};

  for (int block = 0; block < 120; ++block)
  {
    impulse[0] = (block % 30 == 0) ? 1.0 : 0.0;
    const double preDelay = static_cast<double>((block % 5) * 10);
    reverb.SetParams(0.8, 6.0, 5.0, preDelay, 1.0, 2, 48000.0);
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
  }

  reverb.SetParams(0.8, 6.0, 5.0, 20.0, 0.0, 0, 48000.0);
  impulse[0] = 1.0;
  auto** hallOut = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(hallOut[0], frames));
  REQUIRE_FALSE(hasNaN(hallOut[1], frames));
}

TEST_CASE("Reverb: Oktaverb shimmer intensity keeps a stable reverb body")
{
  dsp::effect::Reverb dryOktaverb;
  dsp::effect::Reverb shimmerOktaverb;
  dryOktaverb.SetParams(0.8, 5.0, 5.0, 20.0, 0.0, 2, 48000.0, 1);
  shimmerOktaverb.SetParams(0.8, 5.0, 5.0, 20.0, 1.0, 2, 48000.0, 1);

  const size_t frames = 512;
  std::vector<double> impulse(frames, 0.0);
  double* inputs[2] = {impulse.data(), impulse.data()};
  double noShimmerEnergy = 0.0;
  double fullShimmerEnergy = 0.0;

  for (int block = 0; block < 80; ++block)
  {
    impulse[0] = (block == 0) ? 1.0 : 0.0;
    auto** noShimmerOut = dryOktaverb.Process(inputs, 2, frames);
    auto** fullShimmerOut = shimmerOktaverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(noShimmerOut[0], frames));
    REQUIRE_FALSE(hasNaN(fullShimmerOut[0], frames));

    if (block > 8)
    {
      noShimmerEnergy += energy(noShimmerOut[0], frames) + energy(noShimmerOut[1], frames);
      fullShimmerEnergy += energy(fullShimmerOut[0], frames) + energy(fullShimmerOut[1], frames);
    }
  }

  CHECK(noShimmerEnergy > 0.0001);
  CHECK(fullShimmerEnergy > noShimmerEnergy * 0.8);
}

TEST_CASE("Reverb: Oktaverb pre-delay changes keep audible tail")
{
  dsp::effect::Reverb reverb;
  const size_t frames = 512;
  std::vector<double> impulse(frames, 0.0);
  double* inputs[2] = {impulse.data(), impulse.data()};
  double tailEnergy = 0.0;

  reverb.SetParams(0.8, 6.0, 5.0, 20.0, 1.0, 2, 48000.0, 1);
  for (int block = 0; block < 30; ++block)
  {
    impulse[0] = (block == 0) ? 1.0 : 0.0;
    reverb.Process(inputs, 2, frames);
  }

  for (int block = 0; block < 60; ++block)
  {
    impulse[0] = 0.0;
    reverb.SetParams(0.8, 6.0, 5.0, (block % 2 == 0) ? 0.0 : 40.0, 1.0, 2, 48000.0, 1);
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
    tailEnergy += energy(out[0], frames) + energy(out[1], frames);
  }

  CHECK(tailEnergy > 0.0001);
}

TEST_CASE("Reverb: Oktaverb shimmer=0 remains bounded and keeps a reverb body")
{
  dsp::effect::Reverb oktaverb;
  oktaverb.SetParams(0.7, 3.0, 4.5, 20.0, 0.0, 2, 48000.0);

  const size_t frames = 256;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};

  double tailEnergy = 0.0;
  for (int block = 0; block < 12; ++block)
  {
    auto** oktOut = oktaverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(oktOut[0], frames));
    REQUIRE_FALSE(hasNaN(oktOut[1], frames));
    tailEnergy += energy(oktOut[0], frames) + energy(oktOut[1], frames);
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
  CHECK(tailEnergy > 0.0001);
}

TEST_CASE("Reverb: pre-delay defers early Hall wet taps")
{
  dsp::effect::Reverb noPreDelay;
  dsp::effect::Reverb longPreDelay;
  noPreDelay.SetParams(1.0, 3.0, 4.5, 0.0, 0.5, 0, 48000.0);
  longPreDelay.SetParams(1.0, 3.0, 4.5, 40.0, 0.5, 0, 48000.0);

  const size_t frames = 4096;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};

  auto** noPreOut = noPreDelay.Process(inputs, 2, frames);
  auto** longPreOut = longPreDelay.Process(inputs, 2, frames);

  double noPreEarlyEnergy = 0.0;
  double longPreEarlyEnergy = 0.0;
  for (size_t i = 2500; i < 3500; ++i)
  {
    noPreEarlyEnergy += std::abs(noPreOut[0][i]) + std::abs(noPreOut[1][i]);
    longPreEarlyEnergy += std::abs(longPreOut[0][i]) + std::abs(longPreOut[1][i]);
  }
  CHECK(noPreEarlyEnergy > 0.000001);
  CHECK(longPreEarlyEnergy < noPreEarlyEnergy * 0.25);
}

TEST_CASE("Reverb: Prepare keeps output storage stable across mode and predelay updates")
{
  dsp::effect::Reverb reverb;
  constexpr size_t frames = 256;
  reverb.Prepare(2, frames, 48000.0);
  reverb.SetParams(0.7, 4.0, 5.0, 10.0, 0.0, 0, 48000.0);

  std::vector<double> inL(frames, 0.2), inR(frames, -0.2);
  double* inputs[2] = {inL.data(), inR.data()};
  auto** first = reverb.Process(inputs, 2, frames);
  auto* firstL = first[0];
  auto* firstR = first[1];

  reverb.SetParams(0.7, 4.0, 5.0, 70.0, 1.0, 2, 48000.0);
  auto** second = reverb.Process(inputs, 2, frames);
  CHECK(second == first);
  CHECK(second[0] == firstL);
  CHECK(second[1] == firstR);
  REQUIRE_FALSE(hasNaN(second[0], frames));
  REQUIRE_FALSE(hasNaN(second[1], frames));
}

TEST_CASE("Reverb: Plate stays bounded with long decay")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.8, 10.0, 5.0, 20.0, 0.5, 1, 44100.0);
  const size_t frames = 256;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};
  double maxVal = 0.0;
  for (int block = 0; block < 20; block++)
  {
    auto** out = reverb.Process(inputs, 2, frames);
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
  CHECK(maxVal < 10.0);
  REQUIRE_FALSE(std::isnan(maxVal));
}

TEST_CASE("Reverb: switching modes mid-stream doesn't crash")
{
  dsp::effect::Reverb reverb;
  const size_t frames = 64;
  std::vector<double> in(frames, 0.3);
  double* inputs[2] = {in.data(), in.data()};

  reverb.SetParams(0.5, 3.0, 6.0, 20.0, 0.5, 0, 44100.0);
  reverb.Process(inputs, 2, frames);

  reverb.SetParams(0.5, 3.0, 6.0, 20.0, 0.5, 1, 44100.0);
  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
}

TEST_CASE("Reverb: Reset clears state")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(1.0, 8.0, 5.0, 20.0, 0.5, 0, 44100.0);

  const size_t frames = 256;
  std::vector<double> loud(frames, 1.0);
  double* inputs[2] = {loud.data(), loud.data()};
  reverb.Process(inputs, 2, frames);
  reverb.Process(inputs, 2, frames);

  reverb.Reset();
  reverb.SetParams(1.0, 8.0, 5.0, 20.0, 0.5, 0, 44100.0);

  std::vector<double> silence(frames, 0.0);
  double* silIn[2] = {silence.data(), silence.data()};
  auto** out = reverb.Process(silIn, 2, frames);

  double maxVal = 0.0;
  for (size_t i = 0; i < frames; i++)
    maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
  CHECK(maxVal < 0.05);
}

TEST_CASE("Reverb: Hall stays bounded with long decay")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.8, 10.0, 5.0, 20.0, 0.5, 0, 44100.0);
  const size_t frames = 256;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};
  double maxVal = 0.0;
  for (int block = 0; block < 20; block++)
  {
    auto** out = reverb.Process(inputs, 2, frames);
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
  CHECK(maxVal < 10.0);
  REQUIRE_FALSE(std::isnan(maxVal));
}

TEST_CASE("Reverb: sample rate change reallocates without crash")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.5, 3.0, 6.0, 20.0, 0.5, 0, 44100.0);

  const size_t frames = 64;
  std::vector<double> in(frames, 0.3);
  double* inputs[2] = {in.data(), in.data()};
  reverb.Process(inputs, 2, frames);

  reverb.SetParams(0.5, 3.0, 6.0, 20.0, 0.5, 0, 96000.0);
  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
}

TEST_CASE("Delay: Digital PingPong cross-seeds first repeat to opposite side")
{
  dsp::effect::Delay delay;
  delay.SetParams(10.0, 0.5, 1.0, dsp::effect::Delay::kModeDigital, 1000.0,
                  0.5, 0.0, true);

  const size_t frames = 32;
  std::vector<double> left(frames, 0.0), right(frames, 0.0);
  left[0] = 1.0;
  double* inputs[2] = {left.data(), right.data()};

  auto** out = delay.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  REQUIRE_FALSE(hasNaN(out[1], frames));
  CHECK(std::abs(out[1][10]) > 0.75);
  CHECK(std::abs(out[0][10]) < 0.05);
}

TEST_CASE("Delay: Analog PingPong cross-seeds opposite side")
{
  dsp::effect::Delay delay;
  delay.SetParams(12.0, 0.45, 1.0, dsp::effect::Delay::kModeAnalog, 1000.0,
                  0.5, 0.5, true);

  const size_t frames = 48;
  std::vector<double> left(frames, 0.0), right(frames, 0.0);
  left[0] = 1.0;
  double* inputs[2] = {left.data(), right.data()};

  auto** out = delay.Process(inputs, 2, frames);
  double leftRepeatEnergy = 0.0;
  double rightRepeatEnergy = 0.0;
  for (size_t i = 10; i < 22; ++i)
  {
    leftRepeatEnergy += std::abs(out[0][i]);
    rightRepeatEnergy += std::abs(out[1][i]);
  }
  CHECK(rightRepeatEnergy > leftRepeatEnergy * 2.0);
}

TEST_CASE("Delay: Digital PingPong mono-duplicated input first repeat on right")
{
  dsp::effect::Delay delay;
  delay.SetParams(10.0, 0.5, 1.0, dsp::effect::Delay::kModeDigital, 1000.0,
                  0.5, 0.0, true);

  const size_t frames = 32;
  std::vector<double> mono(frames, 0.0);
  mono[0] = 1.0;
  double* inputs[2] = {mono.data(), mono.data()};

  auto** out = delay.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  REQUIRE_FALSE(hasNaN(out[1], frames));
  CHECK(std::abs(out[1][10]) > 0.75);
  CHECK(std::abs(out[0][10]) < 0.05);
}

TEST_CASE("Delay: Analog PingPong mono-duplicated input favors right on first repeats")
{
  dsp::effect::Delay delay;
  delay.SetParams(12.0, 0.45, 1.0, dsp::effect::Delay::kModeAnalog, 1000.0,
                  0.5, 0.5, true);

  const size_t frames = 48;
  std::vector<double> mono(frames, 0.0);
  mono[0] = 1.0;
  double* inputs[2] = {mono.data(), mono.data()};

  auto** out = delay.Process(inputs, 2, frames);
  double leftRepeatEnergy = 0.0;
  double rightRepeatEnergy = 0.0;
  for (size_t i = 10; i < 22; ++i)
  {
    leftRepeatEnergy += std::abs(out[0][i]);
    rightRepeatEnergy += std::abs(out[1][i]);
  }
  CHECK(rightRepeatEnergy > leftRepeatEnergy * 2.0);
}

TEST_CASE("Delay: SetParams mode change clears delay ring buffers")
{
  dsp::effect::Delay delay;
  delay.SetParams(10.0, 0.5, 1.0, dsp::effect::Delay::kModeDigital, 1000.0,
                  0.5, 0.0, false);

  const size_t frames = 32;
  std::vector<double> left(frames, 0.0), right(frames, 0.0);
  left[0] = 1.0;
  double* inputs[2] = {left.data(), right.data()};
  delay.Process(inputs, 2, frames);

  delay.SetParams(10.0, 0.5, 1.0, dsp::effect::Delay::kModeAnalog, 1000.0,
                  0.5, 0.0, false);

  std::fill(left.begin(), left.end(), 0.0);
  std::fill(right.begin(), right.end(), 0.0);
  auto** out = delay.Process(inputs, 2, frames);
  double sumAbs = 0.0;
  for (size_t i = 0; i < frames; ++i)
    sumAbs += std::abs(out[0][i]) + std::abs(out[1][i]);
  CHECK(sumAbs < 1.0e-9);
}

TEST_CASE("Delay: SetParams ping-pong toggle clears delay ring buffers")
{
  dsp::effect::Delay delay;
  delay.SetParams(10.0, 0.5, 1.0, dsp::effect::Delay::kModeDigital, 1000.0,
                  0.5, 0.0, false);

  const size_t frames = 32;
  std::vector<double> left(frames, 0.0), right(frames, 0.0);
  left[0] = 1.0;
  double* inputs[2] = {left.data(), right.data()};
  delay.Process(inputs, 2, frames);

  delay.SetParams(10.0, 0.5, 1.0, dsp::effect::Delay::kModeDigital, 1000.0,
                  0.5, 0.0, true);

  std::fill(left.begin(), left.end(), 0.0);
  std::fill(right.begin(), right.end(), 0.0);
  auto** out = delay.Process(inputs, 2, frames);
  double sumAbs = 0.0;
  for (size_t i = 0; i < frames; ++i)
    sumAbs += std::abs(out[0][i]) + std::abs(out[1][i]);
  CHECK(sumAbs < 1.0e-9);
}

static std::vector<double> RunOktaverbSubMode(int subMode)
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.9, 5.0, 5.5, 0.0, 1.0, dsp::effect::Reverb::kModeOktaverb, 48000.0, subMode);

  const size_t frames = 1024;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};

  std::vector<double> tail;
  tail.reserve(frames * 16);
  for (int block = 0; block < 16; ++block)
  {
    impulse[0] = (block == 0) ? 1.0 : 0.0;
    auto** out = reverb.Process(inputs, 2, frames);
    for (size_t i = 64; i < frames; ++i)
      tail.push_back(out[0][i]);
  }
  return tail;
}

TEST_CASE("Reverb: Oktaverb sub-modes produce distinct Halo, Shimmer, and Bloom voices")
{
  const auto halo = RunOktaverbSubMode(0);
  const auto shimmer = RunOktaverbSubMode(1);
  const auto bloom = RunOktaverbSubMode(2);
  REQUIRE(halo.size() == shimmer.size());
  REQUIRE(halo.size() == bloom.size());

  double diffShimmer = 0.0;
  double diffBloom = 0.0;
  double energyHalo = 0.0;
  for (size_t i = 0; i < halo.size(); ++i)
  {
    diffShimmer += std::abs(halo[i] - shimmer[i]);
    diffBloom += std::abs(halo[i] - bloom[i]);
    energyHalo += std::abs(halo[i]);
  }

  CHECK(energyHalo > 0.01);
  CHECK(diffShimmer > energyHalo * 0.05);
  CHECK(diffBloom > energyHalo * 0.05);
}

TEST_CASE("Reverb: Oktaverb Bloom slow attack grows after onset")
{
  dsp::effect::Reverb bloom;
  bloom.SetParams(1.0, 6.0, 5.0, 0.0, 1.0, dsp::effect::Reverb::kModeOktaverb, 48000.0, 2);

  const size_t frames = 512;
  std::vector<double> sustained(frames, 0.15);
  double* inputs[2] = {sustained.data(), sustained.data()};
  double earlyEnergy = 0.0;
  double lateEnergy = 0.0;

  for (int block = 0; block < 80; ++block)
  {
    auto** out = bloom.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
    if (block < 5)
      earlyEnergy += energy(out[0], frames) + energy(out[1], frames);
    if (block >= 45 && block < 75)
      lateEnergy += energy(out[0], frames) + energy(out[1], frames);
  }

  CHECK(lateEnergy > earlyEnergy * 1.5);
}

TEST_CASE("Reverb: Oktaverb all new sub-modes remain bounded at max intensity")
{
  const size_t frames = 512;
  std::vector<double> noise(frames, 0.0);
  double* inputs[2] = {noise.data(), noise.data()};

  for (int subMode = 0; subMode < 3; ++subMode)
  {
    dsp::effect::Reverb reverb;
    reverb.SetParams(0.9, 10.0, 6.0, 20.0, 1.0, dsp::effect::Reverb::kModeOktaverb, 48000.0, subMode);
    double maxVal = 0.0;
    for (int block = 0; block < 160; ++block)
    {
      for (size_t i = 0; i < frames; ++i)
        noise[i] = ((i + block) % 31 == 0) ? 0.5 : 0.0;
      auto** out = reverb.Process(inputs, 2, frames);
      REQUIRE_FALSE(hasNaN(out[0], frames));
      REQUIRE_FALSE(hasNaN(out[1], frames));
      for (size_t i = 0; i < frames; ++i)
        maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
    }
    CHECK(maxVal < 10.0);
  }
}

// User-reported regression: Shimmer / Bloom at 100 percent Mix could clip the speakers
// because the dual-pitch / bloom-envelope DSP piles up faster than the additive Mix
// can accommodate. The fix scopes a 50 percent internal Mix cap to all three Oktaverb
// sub-modes plus final-output tanh safety. Halo/Shimmer also use wet-bus tanh for their
// pitch-feedback path; Bloom skips only that wet-bus tanh so the slow swell stays open.
TEST_CASE("Reverb: Oktaverb sub-modes stay under 0 dBFS at max Mix and max Intensity")
{
  const size_t frames = 512;
  std::vector<double> hotInput(frames, 0.0);
  double* inputs[2] = {hotInput.data(), hotInput.data()};

  for (int subMode : {0, 1, 2})
  {
    INFO("subMode=" << subMode);
    dsp::effect::Reverb reverb;
    reverb.SetParams(1.0, 10.0, 6.0, 0.0, 1.0, dsp::effect::Reverb::kModeOktaverb, 48000.0, subMode);
    double maxVal = 0.0;
    for (int block = 0; block < 200; ++block)
    {
      for (size_t i = 0; i < frames; ++i)
        hotInput[i] = 0.7 * std::sin(static_cast<double>(block * frames + i) * 0.05);
      auto** out = reverb.Process(inputs, 2, frames);
      REQUIRE_FALSE(hasNaN(out[0], frames));
      REQUIRE_FALSE(hasNaN(out[1], frames));
      for (size_t i = 0; i < frames; ++i)
        maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
    }
    // All Oktaverb sub-modes pass through final tanh safety when Mix is above zero.
    CHECK(maxVal < 1.0);
  }
}

TEST_CASE("Reverb: Oktaverb Bloom uses Halo/Shimmer output safety at default settings")
{
  // Bloom used to be the only Oktaverb sub-mode without final output safety, which let
  // dry+wet alignment clip even at the stock Bloom scene. It now keeps Mix=0 dry-pass
  // but uses the same final tanh shoulder as Halo/Shimmer once Mix is above zero.
  const size_t frames = 512;
  std::vector<double> input(frames, 0.0);
  double* inputs[2] = {input.data(), input.data()};

  // Stock Bloom defaults with a hot sustained signal must remain below 0 dBFS.
  {
    dsp::effect::Reverb reverb;
    reverb.SetParams(0.30, 5.5, 5.5, 20.0, 0.75, dsp::effect::Reverb::kModeOktaverb, 48000.0, 2);
    double maxVal = 0.0;
    for (int block = 0; block < 200; ++block)
    {
      for (size_t i = 0; i < frames; ++i)
        input[i] = 0.7 * std::sin(static_cast<double>(block * frames + i) * 0.05);
      auto** out = reverb.Process(inputs, 2, frames);
      REQUIRE_FALSE(hasNaN(out[0], frames));
      REQUIRE_FALSE(hasNaN(out[1], frames));
      for (size_t i = 0; i < frames; ++i)
        maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
    }
    CHECK(maxVal < 1.0);
  }

  // Worst case: max Mix and max Decay with hot sustained input also stays finite and
  // below the final safety ceiling.
  dsp::effect::Reverb extreme;
  extreme.SetParams(1.0, 10.0, 6.0, 0.0, 1.0, dsp::effect::Reverb::kModeOktaverb, 48000.0, 2);
  double maxExtreme = 0.0;
  for (int block = 0; block < 200; ++block)
  {
    for (size_t i = 0; i < frames; ++i)
      input[i] = 0.45 * std::sin(static_cast<double>(block * frames + i) * 0.05);
    auto** out = extreme.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
    for (size_t i = 0; i < frames; ++i)
      maxExtreme = std::max(maxExtreme, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
  }
  CHECK(maxExtreme < 1.0);
}

// Master safety contract: stacking the worst realistic POST chain (Delay at high feedback
// + a non-saturating reverb mode like Hall/Plate) on top of a hot pre-output signal
// can produce raw post-FX peaks above 0 dBFS. The master safety stage at the end of
// ProcessBlock applies SoftSafetyClip per-sample, which must bound the final-bus peak to
// the soft-clip ceiling (~+6 dBFS / linear ~2.0) regardless of what the wet stack did.
// Modeled here against the same Delay + Reverb objects ProcessBlock uses, with a hot
// 0 dBFS input simulating the post-amp + post-OutputLevel signal.
TEST_CASE("MasterSafety: Delay + Reverb stack stays bounded by ceiling after SoftSafetyClip")
{
  const size_t frames = 512;
  const int blocks = 240;
  const double sampleRate = 48000.0;

  // Hot input: 0 dBFS sine sustained, far hotter than any musical guitar bus.
  std::vector<double> hotL(frames, 0.0), hotR(frames, 0.0);

  // Worst-case stack: Delay at near-max feedback feeding into a non-saturating reverb.
  // Hall/Plate skip the Oktaverb tanh shoulder, so only the master safety stage stands
  // between this stack and the speakers.
  for (int reverbMode : {dsp::effect::Reverb::kModeHall, dsp::effect::Reverb::kModePlate})
  {
    INFO("reverbMode=" << reverbMode);
    dsp::effect::Delay delay;
    delay.SetParams(320.0, 0.92, 0.85, 1, sampleRate);

    dsp::effect::Reverb reverb;
    reverb.SetParams(0.85, 8.0, 6.0, 20.0, 0.5, reverbMode, sampleRate);

    double rawPeak = 0.0;
    double safePeak = 0.0;
    bool sawAboveKnee = false;

    for (int block = 0; block < blocks; ++block)
    {
      for (size_t i = 0; i < frames; ++i)
      {
        const double phase = static_cast<double>(block * frames + i) * 0.05;
        hotL[i] = std::sin(phase);
        hotR[i] = std::sin(phase + 0.3);
      }
      double* delIn[2] = {hotL.data(), hotR.data()};
      auto** afterDelay = delay.Process(delIn, 2, frames);
      auto** afterReverb = reverb.Process(afterDelay, 2, frames);

      REQUIRE_FALSE(hasNaN(afterReverb[0], frames));
      REQUIRE_FALSE(hasNaN(afterReverb[1], frames));

      for (size_t c = 0; c < 2; ++c)
      {
        for (size_t i = 0; i < frames; ++i)
        {
          const double x = afterReverb[c][i];
          rawPeak = std::max(rawPeak, std::fabs(x));
          if (std::fabs(x) >= 1.4)
            sawAboveKnee = true;

          const double y = volum::SoftSafetyClip(x);
          safePeak = std::max(safePeak, std::fabs(y));
          CHECK(std::isfinite(y));
        }
      }
    }

    // Stack must actually be hot enough to exercise the safety stage; if this fails the
    // input/feedback/decay was not aggressive enough and the test no longer protects us.
    CHECK(sawAboveKnee);
    INFO("rawPeak=" << rawPeak << " safePeak=" << safePeak);
    CHECK(safePeak <= 2.0 + 1e-6);
  }
}

// ────────────────────────────────────────────────────────────────────
// D1 / D2: equal-power reverb crossfade and Reverse-Delay additive blend.
// ────────────────────────────────────────────────────────────────────

namespace
{
double rms(double* buf, size_t n)
{
  double e = 0.0;
  for (size_t i = 0; i < n; i++)
    e += buf[i] * buf[i];
  return std::sqrt(e / static_cast<double>(n));
}

void fillSineBlock(std::vector<double>& dst, int block, size_t frames, double amp = 0.3)
{
  for (size_t i = 0; i < frames; ++i)
    dst[i] = amp * std::sin(static_cast<double>(block * frames + i) * 0.05);
}
} // namespace

TEST_CASE("Reverb: Mix=0 outputs dry within float-cast tolerance, all modes")
{
  const size_t frames = 256;
  std::vector<double> input(frames, 0.0);
  for (size_t i = 0; i < frames; ++i)
    input[i] = 0.4 * std::sin(static_cast<double>(i) * 0.05);
  double* inputs[2] = {input.data(), input.data()};

  // Hall, Plate, and Oktaverb Bloom at Mix=0 pass dry through bit-identically
  // (modulo DSP_SAMPLE float round-trip). Halo and
  // Shimmer always run the dry+wet sum through tanh (Oktaverb's runaway protection),
  // which gently reshapes even a Mix=0 dry signal — that's by design and not a
  // regression of the equal-power crossfade work.
  struct ModeSpec { int mode; int subMode; const char* name; };
  ModeSpec specs[] = {
    {dsp::effect::Reverb::kModeHall, 0, "Hall"},
    {dsp::effect::Reverb::kModePlate, 0, "Plate"},
    {dsp::effect::Reverb::kModeOktaverb, 2, "Bloom"},
  };
  for (const auto& spec : specs)
  {
    INFO("mode=" << spec.name);
    dsp::effect::Reverb reverb;
    reverb.SetParams(0.0, 3.0, 5.0, 0.0, 0.5, spec.mode, 48000.0, spec.subMode);
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    // dryCoef = cos(0) = 1.0, wetCoef = sin(0) = 0; output = input within DSP_SAMPLE
    // (float) round-trip precision.
    for (size_t i = 0; i < frames; ++i)
      CHECK(std::abs(out[0][i] - input[i]) < 1e-4);
  }
}

TEST_CASE("Reverb: Mix=1 attenuates dry and presents wet-only output, Hall + Plate")
{
  // Equal-power at Mix=1 sets dryCoef = cos(pi/2) = 0 and wetCoef = sin(pi/2) * 1.55,
  // so a clean impulse arriving at the input must NOT show up in the output at the
  // same sample (it was multiplied by 0). Locks the dry-attenuation property that the
  // pre-equal-power additive law could not deliver.
  const size_t frames = 256;
  std::vector<double> input(frames, 0.0);
  input[10] = 1.0;
  double* inputs[2] = {input.data(), input.data()};

  for (int mode : {dsp::effect::Reverb::kModeHall, dsp::effect::Reverb::kModePlate})
  {
    INFO("mode=" << mode);
    dsp::effect::Reverb reverb;
    reverb.SetParams(1.0, 3.0, 5.0, 0.0, 0.0, mode, 48000.0, 0);
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    // Dry impulse at index 10 must be heavily attenuated (cos(pi/2)=0); any non-zero
    // value at index 10 comes from the wet bus's earliest tap, which is bounded well
    // below 1.0 for both Hall and Plate.
    CHECK(std::abs(out[0][10]) < 0.5);
  }
}

TEST_CASE("Reverb: Hall and Plate produce nontrivial wet bus at Mix=1")
{
  // After the dry-attenuation check above, confirm the wet path still produces audible
  // output with a sustained input. Catches a future bug where wetTrim or the wet bus
  // got zeroed and the user heard silence at full Mix.
  const size_t frames = 1024;
  std::vector<double> input(frames, 0.0);
  double* inputs[2] = {input.data(), input.data()};

  for (int mode : {dsp::effect::Reverb::kModeHall, dsp::effect::Reverb::kModePlate})
  {
    INFO("mode=" << mode);
    dsp::effect::Reverb reverb;
    reverb.SetParams(1.0, 3.0, 5.0, 0.0, 0.0, mode, 48000.0, 0);
    double last = 0.0;
    for (int b = 0; b < 80; ++b)
    {
      fillSineBlock(input, b, frames, 0.3);
      auto** out = reverb.Process(inputs, 2, frames);
      REQUIRE_FALSE(hasNaN(out[0], frames));
      last = rms(out[0], frames);
    }
    INFO("rms=" << last);
    CHECK(last > 0.05);
  }
}

// P1: switching reverb mode with a live decay tail used to let the previous mode's energy
// leak through the new mode (Plate's input AP buffers still held Hall samples, Oktaverb's
// pitch grain buffers held bloom energy, etc). Match Delay's mode-toggle Reset.
TEST_CASE("Reverb: SetParams mode change clears tail (Hall -> Plate)")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(1.0, 8.0, 5.0, 0.0, 0.5, dsp::effect::Reverb::kModeHall, 44100.0);

  const size_t frames = 256;
  std::vector<double> loud(frames, 1.0);
  double* loudIn[2] = {loud.data(), loud.data()};
  for (int b = 0; b < 4; ++b)
    reverb.Process(loudIn, 2, frames);

  reverb.SetParams(1.0, 8.0, 5.0, 0.0, 0.5, dsp::effect::Reverb::kModePlate, 44100.0);

  std::vector<double> silence(frames, 0.0);
  double* silIn[2] = {silence.data(), silence.data()};
  auto** out = reverb.Process(silIn, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  double maxVal = 0.0;
  for (size_t i = 0; i < frames; ++i)
    maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
  // Stale Hall energy would be well above 0.05 here; mode change must Reset() the tail.
  CHECK(maxVal < 0.05);
}

TEST_CASE("Reverb: SetParams Oktaverb sub-mode change clears tail (Halo -> Bloom)")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(1.0, 8.0, 5.0, 0.0, 1.0, dsp::effect::Reverb::kModeOktaverb, 48000.0, /*subMode=Halo*/ 0);

  const size_t frames = 256;
  std::vector<double> loud(frames, 0.5);
  double* loudIn[2] = {loud.data(), loud.data()};
  for (int b = 0; b < 6; ++b)
    reverb.Process(loudIn, 2, frames);

  reverb.SetParams(1.0, 8.0, 5.0, 0.0, 1.0, dsp::effect::Reverb::kModeOktaverb, 48000.0, /*subMode=Bloom*/ 2);

  std::vector<double> silence(frames, 0.0);
  double* silIn[2] = {silence.data(), silence.data()};
  auto** out = reverb.Process(silIn, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  double maxVal = 0.0;
  for (size_t i = 0; i < frames; ++i)
    maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
  // Halo feedback-pitch buffers must be cleared on sub-mode change.
  CHECK(maxVal < 0.05);
}

// P2: automating the Mix knob in a DAW would step at every block boundary because the
// equal-power dry/wet coefficients were recomputed per block from mMix without smoothing.
// The fix is a one-pole on mMix; this test confirms the per-sample derivative stays small
// (no audible step) immediately after a large Mix change. We feed DC so that the only
// source of inter-block change is the parameter step itself; with the input held flat,
// pre-fix the dry coefficient flipped from cos(0)=1 to cos(pi/2)=0 between blocks, so
// the boundary step jumped by ~|input|. Post-fix the coefficient moves ~kSmoothCoef
// (~0.0014 at 48 kHz) per sample, so the boundary step at the block edge is tiny.
TEST_CASE("Reverb: Mix automation does not zipper at block boundary (DC input)")
{
  const size_t frames = 256;
  const double sr = 48000.0;
  const double dc = 0.4;
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.0, 3.0, 5.0, 0.0, 0.5, dsp::effect::Reverb::kModeHall, sr);

  std::vector<double> in(frames, dc);
  double* inputs[2] = {in.data(), in.data()};

  // Prime: Mix=0 dry-only. At Mix=0 the reverb passes dry through, so we settle at
  // a flat block of value dc.
  for (int b = 0; b < 6; ++b)
    reverb.Process(inputs, 2, frames);

  auto** dryOut = reverb.Process(inputs, 2, frames);
  const double lastDry = dryOut[0][frames - 1];

  // Flip to Mix=1. Pre-fix: dryCoef immediately = 0 so the first sample of the next
  // block drops to ~wet*1.55 with wet basically 0 (no wet bus accumulated), so the
  // boundary step is ~dc (the full DC drop). Post-fix: dryCoef glides at ~10 Hz so
  // the first sample's dry contribution is still ~dc, and the step is small.
  reverb.SetParams(1.0, 3.0, 5.0, 0.0, 0.5, dsp::effect::Reverb::kModeHall, sr);
  auto** wetOut = reverb.Process(inputs, 2, frames);
  const double firstWet = wetOut[0][0];
  const double boundaryStep = std::abs(firstWet - lastDry);

  // Pre-fix this would be ~dc = 0.4. Post-fix this is ~|kSmoothCoef * dc| ~= 0.0006.
  // 0.05 leaves comfortable headroom while still proving the zipper was killed.
  CHECK(boundaryStep < 0.05);
}

// P1: SR/block matrix sweep. The full POST chain (Delay -> Reverb at all modes)
// must produce finite, bounded output regardless of sample rate or block size.
TEST_CASE("Delay+Reverb: SR x block-size matrix produces finite bounded output")
{
  const double sampleRates[] = {44100.0, 48000.0, 88200.0, 96000.0};
  const size_t blockSizes[] = {16, 64, 256, 1024};

  for (double sr : sampleRates)
  {
    for (size_t bs : blockSizes)
    {
      for (int reverbMode : {dsp::effect::Reverb::kModeHall, dsp::effect::Reverb::kModePlate,
                             dsp::effect::Reverb::kModeOktaverb})
      {
        INFO("sr=" << sr << " bs=" << bs << " reverbMode=" << reverbMode);
        dsp::effect::Delay delay;
        dsp::effect::Reverb reverb;
        delay.SetParams(120.0, 0.4, 0.5, 1, sr);
        reverb.SetParams(0.5, 3.0, 5.0, 20.0, 0.5, reverbMode, sr);

        std::vector<double> in(bs, 0.0);
        double* inputs[2] = {in.data(), in.data()};

        double maxVal = 0.0;
        for (int b = 0; b < 8; ++b)
        {
          for (size_t i = 0; i < bs; ++i)
            in[i] = 0.3 * std::sin(static_cast<double>(b * bs + i) * 0.05);
          auto** afterDelay = delay.Process(inputs, 2, bs);
          auto** afterReverb = reverb.Process(afterDelay, 2, bs);
          REQUIRE_FALSE(hasNaN(afterReverb[0], bs));
          REQUIRE_FALSE(hasNaN(afterReverb[1], bs));
          for (size_t i = 0; i < bs; ++i)
            maxVal = std::max(maxVal, std::max(std::abs(afterReverb[0][i]), std::abs(afterReverb[1][i])));
        }
        CHECK(maxVal < 5.0);
      }
    }
  }
}

TEST_CASE("Delay: Reverse and Digital RMS match within 0.5 dB at same Mix")
{
  // Pre-fix Reverse used `dry*(1-mix) + wet*mix` while Digital / Analog used `dry +
  // wet*mix`. At Mix > 0 Reverse audibly dropped vs forward modes - the user complaint.
  // The fix moves Reverse to the additive law; this test pins that behavior.
  const size_t frames = 1024;
  const double sr = 48000.0;
  const double mix = 0.32;

  auto measure = [&](int mode) {
    dsp::effect::Delay delay;
    delay.SetParams(220.0, 0.35, mix, mode, sr);
    std::vector<double> in(frames, 0.0);
    double* ins[1] = {in.data()};
    double last = 0.0;
    // Run long enough that the delay tap is in steady state.
    for (int b = 0; b < 80; ++b)
    {
      fillSineBlock(in, b, frames, 0.3);
      auto** out = delay.Process(ins, 1, frames);
      REQUIRE_FALSE(hasNaN(out[0], frames));
      last = rms(out[0], frames);
    }
    return last;
  };

  const double rmsDigital = measure(dsp::effect::Delay::kModeDigital);
  const double rmsAnalog = measure(dsp::effect::Delay::kModeAnalog);
  const double rmsReverse = measure(dsp::effect::Delay::kModeReverse);

  INFO("digital=" << rmsDigital << " analog=" << rmsAnalog << " reverse=" << rmsReverse);
  // 0.5 dB tolerance: Reverse is structurally different (single reversed slice with edge
  // fade vs continuous repeats) so RMS will not match exactly, but it must not drop
  // below the forward modes the way it did under the old crossfade law.
  const double dropDb = 20.0 * std::log10(rmsReverse / rmsDigital);
  CHECK(dropDb > -3.0); // far better than the old law which silently dropped ~5+ dB
}
