#include "third_party/doctest.h"
#include "golden_helpers.h"

#include "../../AudioDSPTools/dsp/Delay.h"
#include "../../AudioDSPTools/dsp/Reverb.h"
#include "../ToneStack.h"
#include "../VoLumPreEffects.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
void ExpectGoldenHash(const char* name, const std::string& actual, const char* expected)
{
  INFO(name << " actual=" << actual);
  REQUIRE(expected != nullptr);
  CHECK(actual == std::string(expected));
}

std::vector<double> RunDelayGolden(int mode)
{
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 512;
  dsp::effect::Delay delay;
  delay.SetParams(230.0, 0.42, 0.37, mode, sampleRate, 0.48, 0.32, mode != dsp::effect::Delay::kModeReverse);

  std::vector<double> rendered;
  rendered.reserve(frames * 2 * 24);
  std::vector<double> left(frames), right(frames);
  for (int block = 0; block < 24; ++block)
  {
    left = volum::test::MakeReferenceInput(frames, sampleRate, 0x1000U + static_cast<unsigned>(block));
    right = volum::test::MakeReferenceInput(frames, sampleRate, 0x2000U + static_cast<unsigned>(block));
    double* inputs[2] = {left.data(), right.data()};
    auto** out = delay.Process(inputs, 2, frames);
    volum::test::AppendChannel(rendered, out[0], frames);
    volum::test::AppendChannel(rendered, out[1], frames);
  }
  return rendered;
}

std::vector<double> RunReverbGolden(int mode, int subMode)
{
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 512;
  dsp::effect::Reverb reverb;
  reverb.SetParams(0.43, 5.8, 5.4, 23.0, 0.72, mode, sampleRate, subMode);

  std::vector<double> rendered;
  rendered.reserve(frames * 2 * 48);
  std::vector<double> left(frames), right(frames);
  for (int block = 0; block < 48; ++block)
  {
    left = volum::test::MakeReferenceInput(frames, sampleRate, 0x3000U + static_cast<unsigned>(block));
    right = volum::test::MakeReferenceInput(frames, sampleRate, 0x4000U + static_cast<unsigned>(block));
    double* inputs[2] = {left.data(), right.data()};
    auto** out = reverb.Process(inputs, 2, frames);
    volum::test::AppendChannel(rendered, out[0], frames);
    volum::test::AppendChannel(rendered, out[1], frames);
  }
  return rendered;
}

std::vector<DSP_SAMPLE> ToDsp(const std::vector<double>& values)
{
  std::vector<DSP_SAMPLE> out(values.size());
  for (std::size_t i = 0; i < values.size(); ++i)
    out[i] = static_cast<DSP_SAMPLE>(values[i]);
  return out;
}

std::vector<DSP_SAMPLE> RunCompressorGolden()
{
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 256;
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(7.25, 4.0, 0.38, 185.0, 0.82, 1.5, sampleRate);

  std::vector<DSP_SAMPLE> rendered;
  rendered.reserve(frames * 18);
  std::vector<double> ref;
  std::vector<DSP_SAMPLE> input;
  for (int block = 0; block < 18; ++block)
  {
    ref = volum::test::MakeReferenceInput(frames, sampleRate, 0x5000U + static_cast<unsigned>(block));
    input = ToDsp(ref);
    DSP_SAMPLE* inputs[1] = {input.data()};
    auto** out = comp.Process(inputs, 1, frames);
    volum::test::AppendSamples(rendered, out[0], frames);
  }
  return rendered;
}

std::vector<DSP_SAMPLE> RunPreEqGolden()
{
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 256;
  dsp::effect::VoLumPreEq eq;
  eq.Reset(sampleRate, frames);
  eq.SetParams(6.7, 3.8, 900.0, 5.9);

  std::vector<DSP_SAMPLE> rendered;
  rendered.reserve(frames * 12);
  std::vector<double> ref;
  std::vector<DSP_SAMPLE> input;
  for (int block = 0; block < 12; ++block)
  {
    ref = volum::test::MakeReferenceInput(frames, sampleRate, 0x6000U + static_cast<unsigned>(block));
    input = ToDsp(ref);
    DSP_SAMPLE* inputs[1] = {input.data()};
    auto** out = eq.Process(inputs, 1, frames);
    volum::test::AppendSamples(rendered, out[0], frames);
  }
  return rendered;
}

std::vector<DSP_SAMPLE> RunToneStackGolden()
{
  constexpr double sampleRate = 48000.0;
  constexpr int frames = 256;
  dsp::tone_stack::BasicNamToneStack toneStack;
  toneStack.Reset(sampleRate, frames);
  toneStack.SetParam("bass", 6.2);
  toneStack.SetParam("middle", 4.4);
  toneStack.SetParam("treble", 7.1);

  std::vector<DSP_SAMPLE> rendered;
  rendered.reserve(frames * 2 * 12);
  std::vector<double> refL;
  std::vector<double> refR;
  std::vector<DSP_SAMPLE> left;
  std::vector<DSP_SAMPLE> right;
  for (int block = 0; block < 12; ++block)
  {
    refL = volum::test::MakeReferenceInput(frames, sampleRate, 0x7000U + static_cast<unsigned>(block));
    refR = volum::test::MakeReferenceInput(frames, sampleRate, 0x8000U + static_cast<unsigned>(block));
    left = ToDsp(refL);
    right = ToDsp(refR);
    DSP_SAMPLE* inputs[2] = {left.data(), right.data()};
    auto** out = toneStack.Process(inputs, 2, frames);
    volum::test::AppendSamples(rendered, out[0], frames);
    volum::test::AppendSamples(rendered, out[1], frames);
  }
  return rendered;
}
} // namespace

TEST_CASE("Golden DSP: PRE effects and tone stack hashes stay stable")
{
  ExpectGoldenHash("compressor", volum::test::Sha256HexSamples(RunCompressorGolden()),
                   "eca6a5932f0fac141bb84783123e93121d328e4419a5fe38f3848d7e0a45a3fc");
  ExpectGoldenHash("pre-eq", volum::test::Sha256HexSamples(RunPreEqGolden()),
                   "72697ba01950cdb27df11230f3f213c5d59db2dc96ef0e51b0b24bca96509f2b");
  ExpectGoldenHash("tone-stack", volum::test::Sha256HexSamples(RunToneStackGolden()),
                   "7781f751a998773bec158e1d46361551ba63b0df09f319e59697ddd6185c605e");
}

TEST_CASE("Golden DSP: delay mode hashes stay stable")
{
  ExpectGoldenHash("delay-digital", volum::test::Sha256Hex(RunDelayGolden(dsp::effect::Delay::kModeDigital)),
                   "32c0b64268eb5d72978c332ed545e751c8c5f20ba2d8e31442f8d77997be9f1c");
  ExpectGoldenHash("delay-analog", volum::test::Sha256Hex(RunDelayGolden(dsp::effect::Delay::kModeAnalog)),
                   "6fa73c812fac2f053beea8f3ae363a04c45a626ec52874dc41f3eb9b2f212573");
  ExpectGoldenHash("delay-reverse", volum::test::Sha256Hex(RunDelayGolden(dsp::effect::Delay::kModeReverse)),
                   "5c402e0eb76693ebd5157a9f39669f26d3f97b9fd1fa6f0d5edca68ed97e6ae4");
}

TEST_CASE("Golden DSP: reverb and Oktaverb mode hashes stay stable")
{
  ExpectGoldenHash("reverb-hall", volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeHall, 0)),
                   "6543e40308c95bad7b3f92e2547ad7356487fd3b17feeab2d67fe59d32c49c27");
  ExpectGoldenHash("reverb-plate", volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModePlate, 0)),
                   "9dfd2ba555997d6ad0538af6b5611633670dcdf0c788a69fad3bf32cc3704537");
  ExpectGoldenHash("oktaverb-halo",
                   volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 0)),
                   "82f6cf8958e8eadba4b001366180294205bd0ff673e91dfd70d1e6e961e1b593");
  ExpectGoldenHash("oktaverb-shimmer",
                   volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 1)),
                   "0bc3193b9a26cb6d9565298620094b3b5a8c3de207dd58e40e9281ed37b05795");
  ExpectGoldenHash("oktaverb-bloom",
                   volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 2)),
                   "82c916ceddd513d49093f0834a10d5d622db7d8111ff54d1cdd2eaeb77b114c9");
}
