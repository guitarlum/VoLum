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
bool RunningWithAddressSanitizer()
{
#if defined(__has_feature)
  #if __has_feature(address_sanitizer)
  return true;
  #endif
#endif
#if defined(__SANITIZE_ADDRESS__)
  return true;
#else
  return false;
#endif
}

const char* PlatformExpected(const char* windowsExpected, const char* macExpected, const char* macSanitizedExpected)
{
#if defined(__APPLE__)
  if (RunningWithAddressSanitizer() && macSanitizedExpected != nullptr)
    return macSanitizedExpected;
  return macExpected;
#else
  return windowsExpected;
#endif
}

void ExpectGoldenHash(const char* name, const std::string& actual, const char* windowsExpected, const char* macExpected,
                      const char* macSanitizedExpected = nullptr)
{
  // std::string, not the raw pointer: doctest stringifies a const char* as its address,
  // which made the failure log useless for reading a new hash off a CI run.
  INFO(std::string(name) << " actual=" << actual);
  const char* expected = PlatformExpected(windowsExpected, macExpected, macSanitizedExpected);
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
                   "eca6a5932f0fac141bb84783123e93121d328e4419a5fe38f3848d7e0a45a3fc",
                   "d08747ac0a454b8ac11e8404709dc6527be74e683cbf7b44f6369a6de60675d8",
                   "f277e00d949ebab0a3314b9643effb7422c755d4af0c23c2a8e76779fca16844");
  ExpectGoldenHash("pre-eq", volum::test::Sha256HexSamples(RunPreEqGolden()),
                   "72697ba01950cdb27df11230f3f213c5d59db2dc96ef0e51b0b24bca96509f2b",
                   "986fec4c92c2af9645e6e7402ccc5321e12fd083c1d5d813253b93248e98aba5");
  ExpectGoldenHash("tone-stack", volum::test::Sha256HexSamples(RunToneStackGolden()),
                   "7781f751a998773bec158e1d46361551ba63b0df09f319e59697ddd6185c605e",
                   "81ef87af81dac5d6a5cf146cb233d0ac72248addf3bce8dd4721b0a00a865518");
}

TEST_CASE("Golden DSP: delay mode hashes stay stable")
{
  ExpectGoldenHash("delay-digital", volum::test::Sha256Hex(RunDelayGolden(dsp::effect::Delay::kModeDigital)),
                   "32c0b64268eb5d72978c332ed545e751c8c5f20ba2d8e31442f8d77997be9f1c",
                   "2e86b97a8c05dea10e7adb1f49fd2406746dd48a6b8019e4b7fa53ea97dadaf8");
  ExpectGoldenHash("delay-analog", volum::test::Sha256Hex(RunDelayGolden(dsp::effect::Delay::kModeAnalog)),
                   "6fa73c812fac2f053beea8f3ae363a04c45a626ec52874dc41f3eb9b2f212573",
                   "2b22e1f4e29f863495a9bed90c24271c0543ff30aec1fe043b577134788eb195");
  ExpectGoldenHash("delay-reverse", volum::test::Sha256Hex(RunDelayGolden(dsp::effect::Delay::kModeReverse)),
                   "5c402e0eb76693ebd5157a9f39669f26d3f97b9fd1fa6f0d5edca68ed97e6ae4",
                   "6e1a4b3544b681fb2dddd1a0e55b0ce1a44e7637e862cc10530ddc25eb9c0a02");
}

// Every reverb hash below changed in 1.2.1, deliberately: the topology was fixed. The
// FDN modes gained input diffusion and an early field, and Plate gained the half of
// Dattorro's tank that had been missing along with his output tap network. Tail
// lengths, loop gains and tone curves did not move, but every output sample did. What
// the new response has to satisfy lives in test_reverb_diffusion.cpp; this case only
// pins it against unintended drift from here on.
TEST_CASE("Golden DSP: reverb and Oktaverb mode hashes stay stable")
{
  ExpectGoldenHash("reverb-hall", volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeHall, 0)),
                   "b9c9c3d862c7b2574af9b4f3e8d49c1cc335359da23085381e5b46095463811c",
                   "mac-hash-pending-read-it-off-the-failing-ci-run");
  ExpectGoldenHash("reverb-plate", volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModePlate, 0)),
                   "d5bfb6e4c8719746bfd4766338cf13ef8521f146457eb95a6468a481db58ea4b",
                   "mac-hash-pending-read-it-off-the-failing-ci-run");
  ExpectGoldenHash("oktaverb-halo", volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 0)),
                   "a41f1390cffb91bfb95370ebfe33d9bf05df45981cbaf8042cce22a4548326ef",
                   "mac-hash-pending-read-it-off-the-failing-ci-run");
  ExpectGoldenHash("oktaverb-shimmer", volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 1)),
                   "6bc913cb27b0981b7c51754d255eb59960f251bfffb92e99ba013527d3144e3f",
                   "mac-hash-pending-read-it-off-the-failing-ci-run");
  ExpectGoldenHash("oktaverb-bloom", volum::test::Sha256Hex(RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 2)),
                   "4882fc22573ea37ddcde462baf0b01bf4033f6f2d9297dc0df278b094f444573",
                   "mac-hash-pending-read-it-off-the-failing-ci-run");
}
