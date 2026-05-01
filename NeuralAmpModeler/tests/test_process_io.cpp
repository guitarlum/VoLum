#include "third_party/doctest.h"
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
