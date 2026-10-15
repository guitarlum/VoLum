#include "third_party/doctest.h"
#include "../VoLumDualAmpPlan.h"
#include "../VoLumProcessIO.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"

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

// ApplyOutputGainBroadcast no longer clamps in either build: final-bus bounding is the
// master safety stage at the end of ProcessBlock (see VoLumMasterSafety.h). The two paths
// must produce identical output now that the standalone-only clamp is gone.
TEST_CASE("ApplyOutputGainBroadcast: APP_API and DAW paths produce identical unclamped output")
{
  std::vector<float> monoIn(1, 10.f);
  std::vector<float> appOut0(1, 0.f), appOut1(1, 0.f);
  std::vector<float> dawOut0(1, 0.f), dawOut1(1, 0.f);
  float* appOutputs[2] = {appOut0.data(), appOut1.data()};
  float* dawOutputs[2] = {dawOut0.data(), dawOut1.data()};

  volum::process_io::ApplyOutputGainBroadcast(monoIn.data(), appOutputs, 1, 2, 1.0, true);
  volum::process_io::ApplyOutputGainBroadcast(monoIn.data(), dawOutputs, 1, 2, 1.0, false);

  DOCTEST_CHECK(appOut0[0] == doctest::Approx(10.f));
  DOCTEST_CHECK(appOut1[0] == doctest::Approx(10.f));
  DOCTEST_CHECK(dawOut0[0] == doctest::Approx(10.f));
  DOCTEST_CHECK(dawOut1[0] == doctest::Approx(10.f));

  monoIn[0] = -10.f;
  volum::process_io::ApplyOutputGainBroadcast(monoIn.data(), appOutputs, 1, 2, 1.0, true);
  volum::process_io::ApplyOutputGainBroadcast(monoIn.data(), dawOutputs, 1, 2, 1.0, false);
  DOCTEST_CHECK(appOut0[0] == doctest::Approx(-10.f));
  DOCTEST_CHECK(dawOut0[0] == doctest::Approx(-10.f));
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

TEST_CASE("Dual amp inverted support polarity subtracts centered matching lanes")
{
  std::vector<float> main{1.f};
  std::vector<float> support{1.f};
  std::vector<float> left(1, 0.f), right(1, 0.f);
  float* outputs[2] = {left.data(), right.data()};

  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, 0.0, 0.0);
  volum::MergeDualAmpToStereo(main.data(), support.data(), outputs, 1, 2, 1.0, -1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(0.f));
  DOCTEST_CHECK(right[0] == doctest::Approx(0.f));
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

TEST_CASE("Dual amp latency compensation delays the lower-latency lane")
{
  auto comp = volum::MakeDualAmpLatencyCompensation(0, 3);
  DOCTEST_CHECK(comp.mainDelaySamples == 3);
  DOCTEST_CHECK(comp.supportDelaySamples == 0);

  comp = volum::MakeDualAmpLatencyCompensation(4, 1);
  DOCTEST_CHECK(comp.mainDelaySamples == 0);
  DOCTEST_CHECK(comp.supportDelaySamples == 3);

  comp = volum::MakeDualAmpLatencyCompensation(2, 2);
  DOCTEST_CHECK(comp.mainDelaySamples == 0);
  DOCTEST_CHECK(comp.supportDelaySamples == 0);
}

TEST_CASE("Dual amp delay line carries latency compensation across blocks")
{
  volum::DualAmpDelayLine<float> delay;
  std::vector<float> firstIn{1.f, 2.f};
  std::vector<float> secondIn{3.f, 4.f};
  std::vector<float> firstOut(2, -1.f);
  std::vector<float> secondOut(2, -1.f);

  const float* first = delay.Process(firstIn.data(), firstOut.data(), firstIn.size(), 3);
  const float* second = delay.Process(secondIn.data(), secondOut.data(), secondIn.size(), 3);

  DOCTEST_CHECK(first == firstOut.data());
  DOCTEST_CHECK(second == secondOut.data());
  DOCTEST_CHECK(firstOut[0] == doctest::Approx(0.f));
  DOCTEST_CHECK(firstOut[1] == doctest::Approx(0.f));
  DOCTEST_CHECK(secondOut[0] == doctest::Approx(0.f));
  DOCTEST_CHECK(secondOut[1] == doctest::Approx(1.f));
}

TEST_CASE("tier2a dual amp delay reserve stays inside the cap")
{
  volum::DualAmpDelayLine<float> delay;
  delay.Reserve(8);
  std::vector<float> in{1.f, 2.f};
  std::vector<float> out(2, -1.f);
  const float* delayed = delay.Process(in.data(), out.data(), in.size(), 3);
  CHECK(delayed == out.data());
  CHECK(out[0] == doctest::Approx(0.f));

  std::vector<float> bigIn(4, 0.5f);
  std::vector<float> bigOut(4, -1.f);
  const float* skipped = delay.Process(bigIn.data(), bigOut.data(), bigIn.size(), 100);
  CHECK(skipped == bigIn.data());
  CHECK(bigOut[0] == doctest::Approx(-1.f));
}

TEST_CASE("tier2a dual amp delay growth starts silent")
{
  volum::DualAmpDelayLine<float> delay;
  delay.Reserve(8);
  std::vector<float> in{1.f, 2.f, 3.f};
  std::vector<float> out(3, -1.f);
  delay.Process(in.data(), out.data(), in.size(), 2);
  std::vector<float> in2{4.f, 5.f, 6.f, 7.f};
  std::vector<float> out2(4, -1.f);
  delay.Process(in2.data(), out2.data(), in2.size(), 4);
  CHECK(out2[0] == doctest::Approx(0.f));
  CHECK(out2[1] == doctest::Approx(0.f));
  CHECK(out2[2] == doctest::Approx(0.f));
  CHECK(out2[3] == doctest::Approx(0.f));
}

TEST_CASE("tier2a dual amp delay over the reserve does not resume the old ring")
{
  volum::DualAmpDelayLine<float> delay;
  delay.Reserve(8);
  std::vector<float> in{1.f, 2.f};
  std::vector<float> out(2, -1.f);
  delay.Process(in.data(), out.data(), in.size(), 3);
  std::vector<float> big{9.f, 9.f};
  std::vector<float> bigOut(2, -1.f);
  const float* skipped = delay.Process(big.data(), bigOut.data(), big.size(), 100);
  CHECK(skipped == big.data());
  std::vector<float> in2{4.f, 5.f};
  std::vector<float> out2(2, -1.f);
  delay.Process(in2.data(), out2.data(), in2.size(), 3);
  CHECK(out2[0] == doctest::Approx(0.f));
  CHECK(out2[1] == doctest::Approx(0.f));
}

TEST_CASE("Dual amp center stack can align a delayed support impulse")
{
  std::vector<float> main{1.f, 0.f, 0.f};
  std::vector<float> support{0.f, 1.f, 0.f};
  std::vector<float> alignedMain(3, 0.f);
  std::vector<float> alignedSupport(3, 0.f);
  std::vector<float> left(3, -1.f), right(3, -1.f);
  float* outputs[2] = {left.data(), right.data()};

  volum::DualAmpDelayLine<float> mainDelay;
  volum::DualAmpDelayLine<float> supportDelay;
  const auto comp = volum::MakeDualAmpLatencyCompensation(0, 1);
  const float* mainLane = mainDelay.Process(main.data(), alignedMain.data(), main.size(), comp.mainDelaySamples);
  const float* supportLane =
    supportDelay.Process(support.data(), alignedSupport.data(), support.size(), comp.supportDelaySamples);
  const auto gains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom, 0.0, 0.0);

  volum::MergeDualAmpToStereo(mainLane, supportLane, outputs, main.size(), 2, 1.0, 1.0, gains, false);

  DOCTEST_CHECK(left[0] == doctest::Approx(0.f));
  DOCTEST_CHECK(right[0] == doctest::Approx(0.f));
  const float expected = static_cast<float>(0.70710678 * 2.0);
  DOCTEST_CHECK(left[1] == doctest::Approx(expected));
  DOCTEST_CHECK(right[1] == doctest::Approx(expected));
}

TEST_CASE("Audio scratch reserves the pitch-sized cap, not the last host block")
{
  // Revert of T1-3: OnReset assigned GetBlockSize() and ProcessBlock resized
  // (and ResamplingNAM::process threw) when a host grew nFrames without OnReset.
  CHECK(volum::dsp_staging::kRealtimeBlockReserve == 8192);
  CHECK(volum::dsp_staging::ReservedAudioBlockSize(64) == volum::dsp_staging::kRealtimeBlockReserve);
  CHECK(volum::dsp_staging::ReservedAudioBlockSize(128) == volum::dsp_staging::kRealtimeBlockReserve);
  CHECK(volum::dsp_staging::ReservedAudioBlockSize(16384) == 16384);
  CHECK(volum::dsp_staging::AudioBlockFitsReserve(64, 8192));
  CHECK_FALSE(volum::dsp_staging::AudioBlockFitsReserve(8193, 8192));
  CHECK_FALSE(volum::dsp_staging::AudioBlockFitsReserve(64, 0));
}

TEST_CASE("Scratch resize refuses to allocate past the off-thread reserve")
{
  std::vector<float> buf;
  buf.reserve(static_cast<size_t>(volum::dsp_staging::kRealtimeBlockReserve));
  const auto reserved = buf.capacity();

  CHECK(volum::dsp_staging::ResizeScratchNoAlloc(buf, 64));
  CHECK(buf.size() == 64);
  CHECK(buf.capacity() == reserved);

  CHECK(volum::dsp_staging::ResizeScratchNoAlloc(buf, static_cast<size_t>(volum::dsp_staging::kRealtimeBlockReserve)));
  CHECK(buf.capacity() == reserved);

  CHECK_FALSE(volum::dsp_staging::ResizeScratchNoAlloc(buf, reserved + 1));
  CHECK(buf.capacity() == reserved);
  CHECK(buf.size() == static_cast<size_t>(volum::dsp_staging::kRealtimeBlockReserve));
}

TEST_CASE("A NAM is reset at the host block, not the scratch reserve")
{
  // 1.3.0 crackle: every NAM was Reset at the 8192 scratch reserve. Its ring
  // buffers then span the whole reserve and a PRE NAM + amp at 64 frames blew
  // the realtime deadline. test_volum_realtime_budget.cpp measures the cost.
  CHECK(volum::dsp_staging::NamResetBlockSize(64) == 64);
  CHECK(volum::dsp_staging::NamResetBlockSize(128) == 128);
  CHECK(volum::dsp_staging::NamResetBlockSize(1024) == 1024);
  CHECK(volum::dsp_staging::NamResetBlockSize(16) == 64);
  CHECK(volum::dsp_staging::NamResetBlockSize(0) == 64);
  CHECK(volum::dsp_staging::NamResetBlockSize(128) < volum::dsp_staging::ReservedAudioBlockSize(128));
}

TEST_CASE("An oversized NAM block runs in chunks, never dry")
{
  std::vector<float> in(10), out(10, 0.f);
  for (int i = 0; i < 10; ++i)
    in[static_cast<size_t>(i)] = static_cast<float>(i + 1);
  std::vector<int> chunkSizes;

  volum::dsp_staging::ProcessNamInChunks(10, 4, in.data(), out.data(), [&](float** ip, float** op, int n) {
    chunkSizes.push_back(n);
    for (int i = 0; i < n; ++i)
      op[0][i] = -ip[0][i];
  });

  CHECK(chunkSizes == std::vector<int>{4, 4, 2});
  for (int i = 0; i < 10; ++i)
    CHECK(out[static_cast<size_t>(i)] == doctest::Approx(-(i + 1)));
}

TEST_CASE("A NAM block within its reset size is one call")
{
  float in[3] = {1.f, 2.f, 3.f};
  float out[3] = {0.f, 0.f, 0.f};
  int calls = 0;
  volum::dsp_staging::ProcessNamInChunks(3, 3, in, out, [&](float**, float**, int n) {
    ++calls;
    CHECK(n == 3);
  });
  CHECK(calls == 1);
}

TEST_CASE("A NAM that was never reset copies dry instead of processing")
{
  float in[2] = {0.5f, -0.25f};
  float out[2] = {9.f, 9.f};
  int calls = 0;
  volum::dsp_staging::ProcessNamInChunks(2, 0, in, out, [&](float**, float**, int) { ++calls; });
  CHECK(calls == 0);
  CHECK(out[0] == doctest::Approx(0.5f));
  CHECK(out[1] == doctest::Approx(-0.25f));
}

TEST_CASE("An oversized ProcessBlock copies or silences the external bus")
{
  float inL[2] = {0.5f, -0.25f};
  float outL[2] = {9.f, 9.f};
  float outR[2] = {8.f, 8.f};
  float* inputs[1] = {inL};
  float* outputs[2] = {outL, outR};

  volum::dsp_staging::CopyOrSilenceExternalBlock(inputs, outputs, 2, 1, 2);
  CHECK(outL[0] == doctest::Approx(0.5f));
  CHECK(outL[1] == doctest::Approx(-0.25f));
  CHECK(outR[0] == doctest::Approx(0.f));
  CHECK(outR[1] == doctest::Approx(0.f));
}
