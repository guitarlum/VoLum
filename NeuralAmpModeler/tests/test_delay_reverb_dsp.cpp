#include "third_party/doctest.h"
#include "../../AudioDSPTools/dsp/Delay.h"
#include "../../AudioDSPTools/dsp/Reverb.h"
#include <cmath>
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
  delay.SetParams(380.0, 0.5, 0.5, 1, 44100.0);

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

TEST_CASE("Delay: reverse mode mix=1 suppresses straight dry signal")
{
  dsp::effect::Delay delay;
  delay.SetParams(100.0, 0.0, 1.0, dsp::effect::Delay::kModeReverse, 1000.0);

  const size_t frames = 100;
  std::vector<double> impulse(frames, 0.0);
  impulse[20] = 1.0;
  double* inputs[1] = {impulse.data()};

  auto** first = delay.Process(inputs, 1, frames);
  REQUIRE_FALSE(hasNaN(first[0], frames));
  CHECK(std::abs(first[0][20]) < 0.001);

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

TEST_CASE("Reverb: Oktaverb shimmer adds level without replacing Hall body")
{
  dsp::effect::Reverb dryOktaverb;
  dsp::effect::Reverb shimmerOktaverb;
  dryOktaverb.SetParams(0.8, 5.0, 5.0, 20.0, 0.0, 2, 48000.0);
  shimmerOktaverb.SetParams(0.8, 5.0, 5.0, 20.0, 1.0, 2, 48000.0);

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
  CHECK(fullShimmerEnergy > noShimmerEnergy * 1.05);
}

TEST_CASE("Reverb: Oktaverb pre-delay changes keep audible tail")
{
  dsp::effect::Reverb reverb;
  const size_t frames = 512;
  std::vector<double> impulse(frames, 0.0);
  double* inputs[2] = {impulse.data(), impulse.data()};
  double tailEnergy = 0.0;

  reverb.SetParams(0.8, 6.0, 5.0, 20.0, 1.0, 2, 48000.0);
  for (int block = 0; block < 30; ++block)
  {
    impulse[0] = (block == 0) ? 1.0 : 0.0;
    reverb.Process(inputs, 2, frames);
  }

  for (int block = 0; block < 60; ++block)
  {
    impulse[0] = 0.0;
    reverb.SetParams(0.8, 6.0, 5.0, (block % 2 == 0) ? 0.0 : 40.0, 1.0, 2, 48000.0);
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

TEST_CASE("Reverb: Oktaverb sub-modes produce distinct repaired voices")
{
  const auto oct = RunOktaverbSubMode(0);
  const auto fifth = RunOktaverbSubMode(1);
  const auto sub = RunOktaverbSubMode(2);
  REQUIRE(oct.size() == fifth.size());
  REQUIRE(oct.size() == sub.size());

  double diffFifth = 0.0;
  double diffSub = 0.0;
  double energyOct = 0.0;
  for (size_t i = 0; i < oct.size(); ++i)
  {
    diffFifth += std::abs(oct[i] - fifth[i]);
    diffSub += std::abs(oct[i] - sub[i]);
    energyOct += std::abs(oct[i]);
  }

  CHECK(energyOct > 0.01);
  CHECK(diffFifth > energyOct * 0.05);
  CHECK(diffSub > energyOct * 0.05);
}
