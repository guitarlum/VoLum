#include "third_party/doctest.h"
#include "../VoLumDualAmpPlan.h"
#include "../VoLumProcessIO.h"
#include <vector>

TEST_CASE("APP_API stereo sum uses full per-channel gain")
{
  std::vector<float> L(1, 1.f), R(1, 1.f);
  float* inputs[2] = {L.data(), R.data()};
  std::vector<float> mono(1, 0.f);
  volum::process_io::MixExternalInputsToMono(inputs, 1, 2, 1.0, true, mono.data());
  DOCTEST_CHECK(mono[0] == doctest::Approx(2.f));
}

TEST_CASE("DAW path averages stereo before gain")
{
  std::vector<float> L(1, 1.f), R(1, 1.f);
  float* inputs[2] = {L.data(), R.data()};
  std::vector<float> mono(1, 0.f);
  volum::process_io::MixExternalInputsToMono(inputs, 1, 2, 1.0, false, mono.data());
  DOCTEST_CHECK(mono[0] == doctest::Approx(1.f));
}

TEST_CASE("APP_API clamps output")
{
  std::vector<float> monoIn(1, 10.f);
  std::vector<float> out0(1, 0.f), out1(1, 0.f);
  float* outputs[2] = {out0.data(), out1.data()};
  volum::process_io::ApplyOutputGainBroadcast(monoIn.data(), outputs, 1, 2, 1.0, true);
  DOCTEST_CHECK(out0[0] == doctest::Approx(1.f));
  DOCTEST_CHECK(out1[0] == doctest::Approx(1.f));

  monoIn[0] = -10.f;
  volum::process_io::ApplyOutputGainBroadcast(monoIn.data(), outputs, 1, 2, 1.0, true);
  DOCTEST_CHECK(out0[0] == doctest::Approx(-1.f));
}

TEST_CASE("DAW path does not clamp output in plugin")
{
  std::vector<float> monoIn(1, 10.f);
  std::vector<float> out0(1, 0.f);
  float* outputs[1] = {out0.data()};
  volum::process_io::ApplyOutputGainBroadcast(monoIn.data(), outputs, 1, 1, 1.0, false);
  DOCTEST_CHECK(out0[0] == doctest::Approx(10.f));
}

TEST_CASE("ClearBuffers silences every output channel")
{
  std::vector<float> out0{1.f, -2.f, 3.f};
  std::vector<float> out1{4.f, 5.f, -6.f};
  float* outputs[2] = {out0.data(), out1.data()};

  volum::process_io::ClearBuffers(outputs, out0.size(), 2);

  for (float sample : out0)
    DOCTEST_CHECK(sample == doctest::Approx(0.f));
  for (float sample : out1)
    DOCTEST_CHECK(sample == doctest::Approx(0.f));
}

TEST_CASE("Dual amp L/R route hard-pans main and support lanes")
{
  std::vector<float> main{1.f};
  std::vector<float> support{0.5f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::LeftRight, 0.0, 0.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, 1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(1.f));
  DOCTEST_CHECK(right[0] == doctest::Approx(0.5f));
}

TEST_CASE("Dual amp stack route sends matching mono mix to stereo outputs")
{
  std::vector<float> main{1.f};
  std::vector<float> support{1.f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Stack, -1.0, 1.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, 1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(right[0]));
}

TEST_CASE("Dual amp custom hard-pan: both lanes left silences right output")
{
  std::vector<float> main{1.f};
  std::vector<float> support{0.5f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, -1.0, -1.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, 1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(1.5f));
  DOCTEST_CHECK(right[0] == doctest::Approx(0.f));
}

TEST_CASE("Dual amp custom hard-pan: both lanes right silences left output")
{
  std::vector<float> main{1.f};
  std::vector<float> support{0.5f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, 1.0, 1.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, 1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(0.f));
  DOCTEST_CHECK(right[0] == doctest::Approx(1.5f));
}

TEST_CASE("Dual amp custom hard-pan: main left, support right keeps lanes isolated")
{
  std::vector<float> main{1.f};
  std::vector<float> support{0.25f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, -1.0, 1.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, 1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(1.f));
  DOCTEST_CHECK(right[0] == doctest::Approx(0.25f));
}

TEST_CASE("Dual amp custom center pan applies constant-power -3 dB to both lanes")
{
  std::vector<float> main{1.f};
  std::vector<float> support{1.f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, 0.0, 0.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, 1.0, gains, false);

  // cos(pi/4) = sin(pi/4) = sqrt(2)/2 ≈ 0.7071. Both lanes contribute equally to L and R.
  const float expected = static_cast<float>(0.70710678 * (1.0 + 1.0));
  DOCTEST_CHECK(left[0] == doctest::Approx(expected));
  DOCTEST_CHECK(right[0] == doctest::Approx(expected));
}

TEST_CASE("Dual amp mainLevel scales main lane only")
{
  std::vector<float> main{1.f};
  std::vector<float> support{1.f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  // Hard-split so we can read each lane independently from L/R.
  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, -1.0, 1.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 0.5, 1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(0.5f));
  DOCTEST_CHECK(right[0] == doctest::Approx(1.f));
}

TEST_CASE("Dual amp supportLevel scales support lane only")
{
  std::vector<float> main{1.f};
  std::vector<float> support{1.f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, -1.0, 1.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, 0.25, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(1.f));
  DOCTEST_CHECK(right[0] == doctest::Approx(0.25f));
}

TEST_CASE("MakeDualAmpPanGains custom routing honors both pan params")
{
  // Hard-left main, mid-right support: main shows full L, zero R; support has unequal L/R weights.
  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, -1.0, 0.5);
  DOCTEST_CHECK(gains.mainLeft == doctest::Approx(1.0));
  DOCTEST_CHECK(gains.mainRight == doctest::Approx(0.0));
  // pan=0.5 → t=0.75 → cos(0.75 pi/2) ≈ 0.3827, sin(0.75 pi/2) ≈ 0.9239.
  DOCTEST_CHECK(gains.supportLeft == doctest::Approx(0.38268343));
  DOCTEST_CHECK(gains.supportRight == doctest::Approx(0.92387953));
}

TEST_CASE("MakeDualAmpPanGains stack ignores pan params and centers both lanes")
{
  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Stack, -1.0, 1.0);
  DOCTEST_CHECK(gains.mainLeft == doctest::Approx(gains.mainRight));
  DOCTEST_CHECK(gains.supportLeft == doctest::Approx(gains.supportRight));
  DOCTEST_CHECK(gains.mainLeft == doctest::Approx(0.70710678));
}
