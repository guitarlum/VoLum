#include "third_party/doctest.h"
#include "golden_helpers.h"
#include "platform_null.h"

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

// linuxExpected / linuxSanitizedExpected are appended so the Windows and macOS
// literals at existing call sites stay byte-identical. The sanitizer run
// excludes "Golden *", so linuxSanitizedExpected stays unused for these cases.
const char* PlatformExpected(const char* windowsExpected, const char* macExpected, const char* macSanitizedExpected,
                             const char* linuxExpected = nullptr, const char* linuxSanitizedExpected = nullptr)
{
#if defined(__linux__)
  if (RunningWithAddressSanitizer() && linuxSanitizedExpected != nullptr)
    return linuxSanitizedExpected;
  return linuxExpected;
#elif defined(__APPLE__)
  if (RunningWithAddressSanitizer() && macSanitizedExpected != nullptr)
    return macSanitizedExpected;
  return macExpected;
#else
  (void)linuxExpected;
  (void)linuxSanitizedExpected;
  return windowsExpected;
#endif
}

void ExpectGoldenHash(const char* name, const std::string& actual, const char* windowsExpected, const char* macExpected,
                      const char* macSanitizedExpected = nullptr, const char* linuxExpected = nullptr,
                      const char* linuxSanitizedExpected = nullptr)
{
  // std::string, not the raw pointer: doctest stringifies a const char* as its address,
  // which made the failure log useless for reading a new hash off a CI run.
  INFO(std::string(name) << " actual=" << actual);
  const char* expected =
    PlatformExpected(windowsExpected, macExpected, macSanitizedExpected, linuxExpected, linuxSanitizedExpected);
  // FAIL_CHECK keeps the rest of the case running so one Linux pass prints every hash.
  if (expected == nullptr)
  {
    FAIL_CHECK("no platform hash for this build");
    return;
  }
  CHECK(actual == std::string(expected));
}

std::vector<double> RunDelayGolden(int mode, bool sameOnBothSides = false)
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
    right = sameOnBothSides
              ? left
              : volum::test::MakeReferenceInput(frames, sampleRate, 0x2000U + static_cast<unsigned>(block));
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
  const auto compressor = RunCompressorGolden();
  ExpectGoldenHash("compressor", volum::test::Sha256HexSamples(compressor),
                   "eca6a5932f0fac141bb84783123e93121d328e4419a5fe38f3848d7e0a45a3fc",
                   "d08747ac0a454b8ac11e8404709dc6527be74e683cbf7b44f6369a6de60675d8",
                   "f277e00d949ebab0a3314b9643effb7422c755d4af0c23c2a8e76779fca16844",
                   "e29547f103c809afa4ea1be2b80194db010f18aa62ac9ad2d94e40ad7c86489e");
  volum::test::CheckWindowsNull("compressor", compressor);
  const auto preEq = RunPreEqGolden();
  ExpectGoldenHash("pre-eq", volum::test::Sha256HexSamples(preEq),
                   "72697ba01950cdb27df11230f3f213c5d59db2dc96ef0e51b0b24bca96509f2b",
                   "986fec4c92c2af9645e6e7402ccc5321e12fd083c1d5d813253b93248e98aba5", nullptr,
                   "6c161e75690a524c91e27b92d4e8406b0c204f3e57d6c1885695f41ae5d87c5c");
  volum::test::CheckWindowsNull("pre-eq", preEq);
  const auto toneStack = RunToneStackGolden();
  ExpectGoldenHash("tone-stack", volum::test::Sha256HexSamples(toneStack),
                   "7781f751a998773bec158e1d46361551ba63b0df09f319e59697ddd6185c605e",
                   "81ef87af81dac5d6a5cf146cb233d0ac72248addf3bce8dd4721b0a00a865518", nullptr,
                   "a94f1473cbadca8a8d06d2280eb02573a1678debe9ff4dbd6e80912b31aa835f");
  volum::test::CheckWindowsNull("tone-stack", toneStack);
}

// Digital and Analog run ping-pong on independent noise per side, so their hashes moved
// in 1.3.0 when the ping-pong seed became the L/R mid (was L only). R is flipped only
// while L and R correlate negatively, so on this input the seed is still the mid.
TEST_CASE("Golden DSP: delay mode hashes stay stable")
{
  const auto digital = RunDelayGolden(dsp::effect::Delay::kModeDigital);
  ExpectGoldenHash("delay-digital", volum::test::Sha256Hex(digital),
                   "abfb022c399f822ee8aaac8d930170eed2cc37e795d4b8a785c991928f11a6a2",
                   "eee2751e4c25f5a2d29d30312129521c17cf07826d5437329533264b05289eed", nullptr,
                   "2b5e531d88d8ef454bb8bf45a585db07874c83416e7c5bac62f4fbb7e3b2b4f3");
  volum::test::CheckWindowsNull("delay-digital", digital);
  const auto analog = RunDelayGolden(dsp::effect::Delay::kModeAnalog);
  ExpectGoldenHash("delay-analog", volum::test::Sha256Hex(analog),
                   "ac3e9d18fadb531d8bf0aa66c498cf8d1d180dea650f4809c43a773afca99ac9",
                   "e2eb6d2b6326fb4232145e1fafae7aec4af1529217b07a407d25745943167bec", nullptr,
                   "038cb0c90cba84ca05c900919f4a4351dcf670ba8c83c1aea7f0bf2dfa3768a7");
  volum::test::CheckWindowsNull("delay-analog", analog);
  const auto reverse = RunDelayGolden(dsp::effect::Delay::kModeReverse);
  ExpectGoldenHash("delay-reverse", volum::test::Sha256Hex(reverse),
                   "5c402e0eb76693ebd5157a9f39669f26d3f97b9fd1fa6f0d5edca68ed97e6ae4",
                   "6e1a4b3544b681fb2dddd1a0e55b0ce1a44e7637e862cc10530ddc25eb9c0a02", nullptr,
                   "f38beabe2062622cdcd850fbfd2d70254e75eadddd14c45f095dd47037507e9c");
  volum::test::CheckWindowsNull("delay-reverse", reverse);
}

// A single amp reaches the delay as L == R. These were pinned from the left-only
// ping-pong seed before it became the L/R mid, and must never move.
TEST_CASE("Golden DSP: ping-pong with the same input on both sides stays stable")
{
  const auto digital = RunDelayGolden(dsp::effect::Delay::kModeDigital, true);
  ExpectGoldenHash("delay-digital-same-lr", volum::test::Sha256Hex(digital),
                   "e3734fcefe5528ee7e6c3906b1168a960dc24272a87620cee752e3e3d2a54745",
                   "f1317e7e4e65a70ea3ff667baaf71295d2c46ee475e914e75b44ee9df339cf22", nullptr,
                   "2875de0950ee933143747e226c649de118e9b0f5510ec5bd11204d410ab5d9d1");
  volum::test::CheckWindowsNull("delay-digital-same-lr", digital);
  const auto analog = RunDelayGolden(dsp::effect::Delay::kModeAnalog, true);
  ExpectGoldenHash("delay-analog-same-lr", volum::test::Sha256Hex(analog),
                   "fe11f80414720180f957516e95cb22ed578aca9ae8b726194638ab9bcd408def",
                   "494af847639a264c8d65ef38d9071bc8ba8e66e2817fbef49ad2b4bd80360548", nullptr,
                   "8a187987c4c1b6a0653375142899c391fa8c88df1f772c1d106bfc4e64d1ad80");
  volum::test::CheckWindowsNull("delay-analog-same-lr", analog);
}

// Every reverb hash below changed in 1.2.1, deliberately: the topology was fixed. The
// FDN modes gained input diffusion and an early field, and Plate gained the half of
// Dattorro's tank that had been missing along with his output tap network. Tail
// lengths, loop gains and tone curves did not move, but every output sample did. What
// the new response has to satisfy lives in test_reverb_diffusion.cpp; this case only
// pins it against unintended drift from here on. They moved again in 1.3.0 when the
// reverb started taking L and R separately (this case feeds independent noise per
// side); an L == R input is bit-identical to before (test_reverb_stereo_input.cpp).
TEST_CASE("Golden DSP: reverb and Oktaverb mode hashes stay stable")
{
  const auto hall = RunReverbGolden(dsp::effect::Reverb::kModeHall, 0);
  ExpectGoldenHash("reverb-hall", volum::test::Sha256Hex(hall),
                   "0e0dea2148d1d48201560bbd9ced13f5d8d5abd55d457f8189fc7fb84cefe275",
                   "651537db17d746e23b90f30fc04a36b2714bd93e452e307eb1bff93756acb8a5", nullptr,
                   "6c0cee9cceee012f126f0923b737a840439abe2f9fb01a31a2d5928c4d642acf");
  volum::test::CheckWindowsNull("reverb-hall", hall);
  const auto plate = RunReverbGolden(dsp::effect::Reverb::kModePlate, 0);
  ExpectGoldenHash("reverb-plate", volum::test::Sha256Hex(plate),
                   "36bb5435950bd077ef61f2fa4a563485490caf0384e90ef328b96f9ecb8b0c8c",
                   "a4805485fccd8a767fbc8f6ddc87484d008f572a0d1e4311bc446722bb41425b", nullptr,
                   "0e6f453961758a3526fbdc12efaabbb34ce693a1068c55cc5fd1c41aca2c725d");
  volum::test::CheckWindowsNull("reverb-plate", plate);
  const auto halo = RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 0);
  ExpectGoldenHash("oktaverb-halo", volum::test::Sha256Hex(halo),
                   "8c7271c2c52d50c11cfb359ffa77e2c539cf5431c0239fe78451763a4041f434",
                   "6165bcba62f985a34a94f1be51f8dbd135a9a68c479b48fe40ca79f5d7112a02", nullptr,
                   "1daf2177d39c99feebef9eb7229c8b340c2fe55647874a2fe647d4d6bcfdd444");
  volum::test::CheckWindowsNull("oktaverb-halo", halo);
  const auto shimmer = RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 1);
  ExpectGoldenHash("oktaverb-shimmer", volum::test::Sha256Hex(shimmer),
                   "3403ec0e51ca3dc37eb94cd14a1847b8135a1925cc83ab695dfa78498d775c86",
                   "0b77bfc150d7d07009b79f96e8d8999e50645320eb02b23af0c2ed773de89339", nullptr,
                   "ad15f8ba5903d4f24850cf67d25c91dc39e18d4fed24a4f7a1c2c1d5e1ec5c0a");
  volum::test::CheckWindowsNull("oktaverb-shimmer", shimmer);
  const auto bloom = RunReverbGolden(dsp::effect::Reverb::kModeOktaverb, 2);
  ExpectGoldenHash("oktaverb-bloom", volum::test::Sha256Hex(bloom),
                   "bb97b2c1331309464e2d18f4d75ee81bf374ffbe99a6346bb6b6bf9e561d678e",
                   "91e46693fc1341469e3c429d149a26c491e746f4d371dfa1e9dbafe152807fc2", nullptr,
                   "d91f61f60ebf77fa633df8dc8d8b97a98bcff44db4a6d08929c3a4765de23cf3");
  volum::test::CheckWindowsNull("oktaverb-bloom", bloom);
}
