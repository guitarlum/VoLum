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

// â”€â”€â”€ Delay â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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

TEST_CASE("Delay: all three modes produce output without NaN")
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

// â”€â”€â”€ Reverb â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

TEST_CASE("Reverb: no NaN on first block after SetParams")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.5, 3.0, 6.0, 0, 44100.0);

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
  reverb.SetParams(0.0, 3.0, 6.0, 0, 44100.0);

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
  reverb.SetParams(0.5, 2.0, 5.0, 0, 48000.0);
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
  reverb.SetParams(0.5, 2.0, 5.0, 1, 48000.0);
  const size_t frames = 512;
  std::vector<double> inL(frames, 0.3), inR(frames, -0.3);
  double* inputs[2] = {inL.data(), inR.data()};
  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
  REQUIRE_FALSE(hasNaN(out[1], frames));
}

TEST_CASE("Reverb: Plate stays bounded with long decay")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.8, 10.0, 5.0, 1, 44100.0);
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

  reverb.SetParams(0.5, 3.0, 6.0, 0, 44100.0);
  reverb.Process(inputs, 2, frames);

  reverb.SetParams(0.5, 3.0, 6.0, 1, 44100.0);
  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
}

TEST_CASE("Reverb: Reset clears state")
{
  dsp::effect::Reverb reverb;
  reverb.SetParams(1.0, 8.0, 5.0, 0, 44100.0);

  const size_t frames = 256;
  std::vector<double> loud(frames, 1.0);
  double* inputs[2] = {loud.data(), loud.data()};
  reverb.Process(inputs, 2, frames);
  reverb.Process(inputs, 2, frames);

  reverb.Reset();
  reverb.SetParams(1.0, 8.0, 5.0, 0, 44100.0);

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
  reverb.SetParams(0.8, 10.0, 5.0, 0, 44100.0);
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
  reverb.SetParams(0.5, 3.0, 6.0, 0, 44100.0);

  const size_t frames = 64;
  std::vector<double> in(frames, 0.3);
  double* inputs[2] = {in.data(), in.data()};
  reverb.Process(inputs, 2, frames);

  reverb.SetParams(0.5, 3.0, 6.0, 0, 96000.0);
  auto** out = reverb.Process(inputs, 2, frames);
  REQUIRE_FALSE(hasNaN(out[0], frames));
}
