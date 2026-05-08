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

TEST_CASE("Delay: all four modes produce output without NaN")
{
  for (int mode = 0; mode < 4; mode++)
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
  delay.SetParams(100.0, 0.5, 0.0, 3, 1000.0);

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
  delay.SetParams(100.0, 0.0, 1.0, 3, 1000.0);

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
  delay.SetParams(100.0, 0.0, 1.0, 3, 1000.0);

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
  delay.SetParams(10.0, 0.99, 0.8, 3, 44100.0);
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

TEST_CASE("Reverb: Oktaverb shimmer=0 matches Hall")
{
  dsp::effect::Reverb hall;
  dsp::effect::Reverb oktaverb;
  hall.SetParams(0.7, 3.0, 4.5, 20.0, 0.0, 0, 48000.0);
  oktaverb.SetParams(0.7, 3.0, 4.5, 20.0, 0.0, 2, 48000.0);

  const size_t frames = 256;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};

  for (int block = 0; block < 12; ++block)
  {
    auto** hallOut = hall.Process(inputs, 2, frames);
    auto** oktOut = oktaverb.Process(inputs, 2, frames);
    for (size_t i = 0; i < frames; i++)
    {
      CHECK(oktOut[0][i] == doctest::Approx(hallOut[0][i]).epsilon(0.000001));
      CHECK(oktOut[1][i] == doctest::Approx(hallOut[1][i]).epsilon(0.000001));
    }
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
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

  const size_t firstHallTap = 1500;
  CHECK(std::abs(noPreOut[0][firstHallTap]) > 0.000001);
  CHECK(std::abs(longPreOut[0][firstHallTap]) < 0.000001);
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

// =====================================================================================
// VoLum Effects Iteration 2 — new behaviors:
//   - Delay: tone/age/pingPong/tapeSubMode params, new mode order (Digital/Analog/Tape/Reverse)
//   - Reverb: subMode/tremRate params, mode 4 (TremVerb), reworked Oktaverb
// =====================================================================================

TEST_CASE("Delay: 9-arg SetParams accepts new params on every mode without NaN")
{
  for (int mode = 0; mode < 4; ++mode)
  {
    for (int sub = 0; sub < 3; ++sub)
    {
      for (int pp = 0; pp < 2; ++pp)
      {
        dsp::effect::Delay delay;
        // tone=0.5 (neutral), age=0.6 (heavily aged), pingPong=true/false, tapeSubMode=sub
        delay.SetParams(180.0, 0.45, 0.6, mode, 48000.0, 0.5, 0.6, pp != 0, sub);
        const size_t frames = 256;
        std::vector<double> inL(frames, 0.2), inR(frames, -0.2);
        double* inputs[2] = {inL.data(), inR.data()};
        for (int block = 0; block < 4; ++block)
        {
          auto** out = delay.Process(inputs, 2, frames);
          REQUIRE_FALSE(hasNaN(out[0], frames));
          REQUIRE_FALSE(hasNaN(out[1], frames));
        }
      }
    }
  }
}

TEST_CASE("Delay: Digital mix=0 + age=0 + pingPong=false passes input through cleanly")
{
  dsp::effect::Delay delay;
  delay.SetParams(120.0, 0.4, 0.0, 0 /*Digital*/, 48000.0, 0.5, 0.0, false, 0);

  const size_t frames = 128;
  std::vector<double> inL(frames, 0.4), inR(frames, 0.4);
  double* inputs[2] = {inL.data(), inR.data()};
  auto** out = delay.Process(inputs, 2, frames);
  for (size_t i = 0; i < frames; i++)
  {
    CHECK(out[0][i] == doctest::Approx(0.4).epsilon(0.001));
    CHECK(out[1][i] == doctest::Approx(0.4).epsilon(0.001));
  }
}

TEST_CASE("Delay: ping-pong distributes signal to the silent channel")
{
  // Drive ONLY the L channel with an impulse. A non-ping-pong delay leaves R essentially
  // silent (no cross-feed). A ping-pong delay routes feedback across channels, so the R
  // channel must accumulate measurable wet energy.
  dsp::effect::Delay monoDelay;
  dsp::effect::Delay ppDelay;
  monoDelay.SetParams(30.0, 0.5, 0.7, 0, 48000.0, 0.5, 0.0, false, 0);
  ppDelay.SetParams(30.0, 0.5, 0.7, 0, 48000.0, 0.5, 0.0, true, 0);

  const size_t frames = 8192;
  std::vector<double> imL(frames, 0.0), imR(frames, 0.0);
  imL[0] = 1.0;
  double* inputs[2] = {imL.data(), imR.data()};

  double monoEnergyR = 0.0;
  double ppEnergyR = 0.0;
  for (int block = 0; block < 4; ++block)
  {
    auto** monoOut = monoDelay.Process(inputs, 2, frames);
    auto** ppOut = ppDelay.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(monoOut[0], frames));
    REQUIRE_FALSE(hasNaN(ppOut[0], frames));
    for (size_t i = 0; i < frames; i++)
    {
      monoEnergyR += monoOut[1][i] * monoOut[1][i];
      ppEnergyR += ppOut[1][i] * ppOut[1][i];
    }
    std::fill(imL.begin(), imL.end(), 0.0);
  }
  // Ping-pong must put real energy on the right channel; non-ping-pong leaves it ~silent.
  CHECK(ppEnergyR > 0.01);
  CHECK(ppEnergyR > monoEnergyR * 5.0);
}

TEST_CASE("Delay: Tape sub-modes audibly differ at the same parameter set")
{
  // Drive a fixed input through the three Tape sub-modes and assert the outputs differ.
  // Studio (0) is brightest/cleanest, Vintage (1) middle, Broken (2) most degraded.
  // Use a short delay time so taps appear quickly and a steady tone so wow/flutter
  // and per-repeat damping have time to manifest.
  std::vector<std::vector<double>> outputs(3);
  const size_t frames = 2048;
  std::vector<double> tone(frames);
  for (size_t i = 0; i < frames; i++)
    tone[i] = 0.5 * std::sin(2.0 * 3.14159265358979 * 1000.0 * i / 48000.0);

  for (int sub = 0; sub < 3; ++sub)
  {
    dsp::effect::Delay delay;
    delay.SetParams(50.0, 0.5, 0.9, 2 /*Tape*/, 48000.0, 0.5, 0.4, false, sub);
    std::vector<double> accum;
    double* inputs[1] = {tone.data()};
    for (int block = 0; block < 6; ++block)
    {
      auto** out = delay.Process(inputs, 1, frames);
      REQUIRE_FALSE(hasNaN(out[0], frames));
      if (block == 5)
        accum.assign(out[0], out[0] + frames);
    }
    outputs[sub] = std::move(accum);
  }

  auto rms_diff = [](const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); i++)
    {
      const double d = a[i] - b[i];
      s += d * d;
    }
    return std::sqrt(s / a.size());
  };
  CHECK(rms_diff(outputs[0], outputs[1]) > 1e-5);
  CHECK(rms_diff(outputs[1], outputs[2]) > 1e-5);
  CHECK(rms_diff(outputs[0], outputs[2]) > 1e-5);
}

TEST_CASE("Delay: Analog mode produces output without NaN at high feedback")
{
  dsp::effect::Delay delay;
  delay.SetParams(380.0, 0.85, 0.6, 1 /*Analog*/, 48000.0, 0.5, 0.5, false, 0);
  const size_t frames = 512;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[1] = {impulse.data()};
  double maxVal = 0.0;
  for (int block = 0; block < 30; ++block)
  {
    auto** out = delay.Process(inputs, 1, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::abs(out[0][i]));
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
  CHECK(maxVal < 10.0);
}

TEST_CASE("Delay: tone knob shifts spectral balance on Digital mode")
{
  // Tone=0 (dark) vs tone=1 (bright) should produce different repeats. Use a short delay
  // so the wet tap appears within the buffer, then compare outputs after the wet path
  // has had time to develop.
  auto runTone = [](double tone, std::vector<double>& last) {
    dsp::effect::Delay delay;
    delay.SetParams(50.0, 0.6, 1.0, 0 /*Digital*/, 48000.0, tone, 0.0, false, 0);

    const size_t frames = 2048;
    std::vector<double> noise(frames);
    unsigned int seed = 12345u;
    for (size_t i = 0; i < frames; i++)
    {
      seed = seed * 1664525u + 1013904223u;
      noise[i] = (static_cast<int>(seed >> 8) & 0xFFFF) / 32768.0 - 1.0;
    }
    double* inputs[1] = {noise.data()};
    std::vector<double> out;
    for (int block = 0; block < 4; ++block)
    {
      auto** o = delay.Process(inputs, 1, frames);
      REQUIRE_FALSE(hasNaN(o[0], frames));
      if (block == 3)
        out.assign(o[0], o[0] + frames);
    }
    last = std::move(out);
  };
  std::vector<double> dark, bright;
  runTone(0.0, dark);
  runTone(1.0, bright);
  double diff = 0.0;
  for (size_t i = 0; i < dark.size(); i++)
    diff += std::abs(dark[i] - bright[i]);
  CHECK(diff > 0.01);
}

TEST_CASE("Delay: Reverse age=0 vs age=1 changes output character (fade-shape)")
{
  // Reverse mode captures mTimeMs windows and plays them backwards through a fade
  // envelope. Age picks the fade-shape softness (0 = triangle, 1 = sin^2 bloom). Drive
  // the line with a steady tone so the playback segments contain real signal in every
  // cycle, then accumulate over many blocks so the fade-shape difference shows up.
  const size_t frames = 1024;
  dsp::effect::Delay sharp, soft;
  sharp.SetParams(20.0, 0.0, 1.0, 3 /*Reverse*/, 48000.0, 0.5, 0.0 /*triangle*/, false, 0);
  soft.SetParams(20.0, 0.0, 1.0, 3 /*Reverse*/, 48000.0, 0.5, 1.0 /*sin^2 bloom*/, false, 0);

  std::vector<double> tone(frames);
  for (size_t i = 0; i < frames; i++)
    tone[i] = 0.5 * std::sin(2.0 * 3.14159265358979 * 800.0 * i / 48000.0);
  double* inputs[1] = {tone.data()};

  double diff = 0.0;
  for (int block = 0; block < 6; ++block)
  {
    auto** s = sharp.Process(inputs, 1, frames);
    auto** o = soft.Process(inputs, 1, frames);
    REQUIRE_FALSE(hasNaN(s[0], frames));
    REQUIRE_FALSE(hasNaN(o[0], frames));
    for (size_t i = 0; i < frames; i++)
      diff += std::abs(s[0][i] - o[0][i]);
  }
  CHECK(diff > 1e-3);
}

TEST_CASE("Reverb: 9-arg SetParams accepts subMode + tremRate on every mode without NaN")
{
  for (int mode = 0; mode < 4; ++mode)
  {
    for (int sub = 0; sub < 3; ++sub)
    {
      dsp::effect::Reverb reverb;
      reverb.SetParams(0.4, 2.5, 5.0, 20.0, 0.4, mode, 48000.0, sub, 4.0);
      const size_t frames = 256;
      std::vector<double> inL(frames, 0.2), inR(frames, -0.2);
      double* inputs[2] = {inL.data(), inR.data()};
      for (int block = 0; block < 4; ++block)
      {
        auto** out = reverb.Process(inputs, 2, frames);
        REQUIRE_FALSE(hasNaN(out[0], frames));
        REQUIRE_FALSE(hasNaN(out[1], frames));
      }
    }
  }
}

// Helper: drive a reverb instance with an impulse, run several blocks, and accumulate
// the wet tail energy so sub-mode comparisons see meaningful difference even after the
// dry-only prefix and pre-delay window.
static std::vector<double> RunReverbTail(int mode, int subMode, double tremRate)
{
  dsp::effect::Reverb reverb;
  // No pre-delay so the wet response starts immediately. Small mix/decay so the network
  // builds detectable energy quickly; sub-mode scaling will diverge the tail per mode.
  reverb.SetParams(0.7, 3.0, 5.5, 0.0, 0.0, mode, 48000.0, subMode, tremRate);
  const size_t frames = 1024;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};

  // Concatenate 6 blocks worth of output for a 6144-sample tail snapshot.
  std::vector<double> tail;
  tail.reserve(6 * frames);
  for (int block = 0; block < 6; ++block)
  {
    auto** out = reverb.Process(inputs, 2, frames);
    for (size_t i = 0; i < frames; i++)
      tail.push_back(out[0][i]);
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
  return tail;
}

TEST_CASE("Reverb: Hall sub-modes produce different responses")
{
  std::vector<std::vector<double>> outputs(3);
  for (int sub = 0; sub < 3; ++sub)
    outputs[sub] = RunReverbTail(0 /*Hall*/, sub, 4.0);
  auto diff = [](const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); i++)
      s += std::abs(a[i] - b[i]);
    return s;
  };
  CHECK(diff(outputs[0], outputs[1]) > 1e-3);
  CHECK(diff(outputs[1], outputs[2]) > 1e-3);
  CHECK(diff(outputs[0], outputs[2]) > 1e-3);
}

TEST_CASE("Reverb: Plate sub-modes produce different responses")
{
  std::vector<std::vector<double>> outputs(3);
  for (int sub = 0; sub < 3; ++sub)
    outputs[sub] = RunReverbTail(1 /*Plate*/, sub, 4.0);
  auto diff = [](const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); i++)
      s += std::abs(a[i] - b[i]);
    return s;
  };
  CHECK(diff(outputs[0], outputs[1]) > 1e-3);
  CHECK(diff(outputs[1], outputs[2]) > 1e-3);
  CHECK(diff(outputs[0], outputs[2]) > 1e-3);
}

TEST_CASE("Reverb: TremVerb (mode 3) produces bounded output without NaN")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.5, 1.6, 5.5, 8.0, 0.7 /*Trem Depth*/, 3 /*TremVerb*/, 48000.0, 0, 5.0 /*Hz*/);

  const size_t frames = 512;
  std::vector<double> impulse(frames, 0.0);
  impulse[0] = 1.0;
  double* inputs[2] = {impulse.data(), impulse.data()};
  double maxVal = 0.0;

  for (int block = 0; block < 60; ++block)
  {
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    REQUIRE_FALSE(hasNaN(out[1], frames));
    for (size_t i = 0; i < frames; i++)
      maxVal = std::max(maxVal, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
    std::fill(impulse.begin(), impulse.end(), 0.0);
  }
  CHECK(maxVal < 10.0);
  CHECK(maxVal > 0.0001);
}

TEST_CASE("Reverb: TremVerb mix=0 passes input through")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.0, 1.5, 5.0, 0.0, 0.7, 3 /*TremVerb*/, 48000.0, 0, 5.0);
  const size_t frames = 64;
  std::vector<double> in(frames, 0.4);
  double* inputs[2] = {in.data(), in.data()};
  auto** out = reverb.Process(inputs, 2, frames);
  for (size_t i = 0; i < frames; i++)
  {
    CHECK(out[0][i] == doctest::Approx(0.4).epsilon(0.005));
    CHECK(out[1][i] == doctest::Approx(0.4).epsilon(0.005));
  }
}

TEST_CASE("Reverb: TremVerb depth=0 yields no audible volume modulation")
{
  // Drive a steady tone through TremVerb with depth=0 and verify the wet RMS doesn't pulse.
  dsp::effect::Reverb reverb;
  reverb.SetParams(1.0 /*100% wet*/, 1.5, 5.0, 0.0, 0.0 /*depth=0*/, 3 /*TremVerb*/, 48000.0, 0, 5.0);

  const size_t frames = 256;
  // Run for ~2 seconds; collect block RMS and verify low std-dev (steady).
  std::vector<double> blockRms;
  std::vector<double> tone(frames);
  // Pre-fill reverb tail with a few impulses so the wet path has signal to modulate.
  for (size_t i = 0; i < frames; i++)
    tone[i] = std::sin(2.0 * 3.14159265358979 * 440.0 * i / 48000.0);
  double* inputs[2] = {tone.data(), tone.data()};
  for (int block = 0; block < 200; ++block)
  {
    auto** out = reverb.Process(inputs, 2, frames);
    if (block > 80)
    {
      double sum = 0.0;
      for (size_t i = 0; i < frames; i++)
        sum += out[0][i] * out[0][i];
      blockRms.push_back(std::sqrt(sum / frames));
    }
  }
  REQUIRE(blockRms.size() > 10);
  double mean = 0.0;
  for (double r : blockRms) mean += r;
  mean /= blockRms.size();
  double variance = 0.0;
  for (double r : blockRms) variance += (r - mean) * (r - mean);
  variance /= blockRms.size();
  const double std_dev = std::sqrt(variance);
  // depth=0 should not produce visible amplitude swing per block.
  if (mean > 1e-6)
    CHECK(std_dev / mean < 0.1);
}

TEST_CASE("Reverb: TremVerb depth=1 yields audible volume modulation")
{
  // Same setup, but depth=1: wet RMS should swing significantly between blocks.
  dsp::effect::Reverb reverb;
  reverb.SetParams(1.0, 1.5, 5.0, 0.0, 1.0, 3, 48000.0, 0, 4.0);

  const size_t frames = 512;
  std::vector<double> blockRms;
  std::vector<double> tone(frames);
  for (size_t i = 0; i < frames; i++)
    tone[i] = std::sin(2.0 * 3.14159265358979 * 440.0 * i / 48000.0);
  double* inputs[2] = {tone.data(), tone.data()};
  for (int block = 0; block < 200; ++block)
  {
    auto** out = reverb.Process(inputs, 2, frames);
    REQUIRE_FALSE(hasNaN(out[0], frames));
    if (block > 80)
    {
      double sum = 0.0;
      for (size_t i = 0; i < frames; i++)
        sum += out[0][i] * out[0][i];
      blockRms.push_back(std::sqrt(sum / frames));
    }
  }
  REQUIRE(blockRms.size() > 10);
  double minR = blockRms[0], maxR = blockRms[0];
  for (double r : blockRms)
  {
    minR = std::min(minR, r);
    maxR = std::max(maxR, r);
  }
  // Photocell trem swings the wet level meaningfully. Block RMS averages partly over the
  // tremolo cycle (period ~250ms at 4 Hz, block ~10.6ms at 48k/512), so we expect a
  // noticeable but not extreme ratio between the loudest and quietest blocks.
  if (maxR > 1e-6)
    CHECK(maxR / std::max(minR, 1e-9) > 1.2);
}

TEST_CASE("Reverb: Oktaverb sub-modes produce different responses")
{
  // Oktaverb's pitched feedback line modulates over multiple blocks, so we accumulate a
  // multi-block tail (RunReverbTail uses pre-delay=0 + mix=0.7 so the wet path is loud
  // and visible quickly).
  std::vector<std::vector<double>> outputs(3);
  for (int sub = 0; sub < 3; ++sub)
  {
    dsp::effect::Reverb reverb;
    reverb.SetParams(0.7, 4.0, 5.0, 0.0, 0.7, 2 /*Oktaverb*/, 48000.0, sub, 4.0);
    const size_t frames = 1024;
    std::vector<double> impulse(frames, 0.0);
    impulse[0] = 1.0;
    double* inputs[2] = {impulse.data(), impulse.data()};
    std::vector<double> tail;
    tail.reserve(8 * frames);
    for (int block = 0; block < 8; ++block)
    {
      auto** out = reverb.Process(inputs, 2, frames);
      for (size_t i = 0; i < frames; i++)
        tail.push_back(out[0][i]);
      std::fill(impulse.begin(), impulse.end(), 0.0);
    }
    outputs[sub] = std::move(tail);
  }
  auto diff = [](const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); i++)
      s += std::abs(a[i] - b[i]);
    return s;
  };
  CHECK(diff(outputs[0], outputs[1]) > 1e-3);
  CHECK(diff(outputs[1], outputs[2]) > 1e-3);
  CHECK(diff(outputs[0], outputs[2]) > 1e-3);
}
