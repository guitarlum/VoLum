// F-24: no DSP object allocates on the audio thread when the host block grows past the size it
// was reset for (inside the reserved block). A counting operator new makes that a hard assertion.
#include "third_party/doctest.h"

#include "../../AudioDSPTools/dsp/Delay.h"
#include "../../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../../AudioDSPTools/dsp/NoiseGate.h"
#include "../../AudioDSPTools/dsp/RecursiveLinearFilter.h"
#include "../../AudioDSPTools/dsp/Reverb.h"
#include "../ToneStack.h"
#include "../VoLumIrShapingDsp.h"
#include "../VoLumPitchShifter.h"
#include "../VoLumPreEffects.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::atomic<long> gAllocCount{0};
std::atomic<bool> gCounting{false};
} // namespace

// Replaced for the whole test executable; counts only while armed.
void* operator new(std::size_t size)
{
  if (gCounting.load(std::memory_order_relaxed))
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
  if (void* p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
  return operator new(size);
}
void operator delete(void* p) noexcept
{
  std::free(p);
}
void operator delete[](void* p) noexcept
{
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
  std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
  std::free(p);
}

namespace
{
constexpr size_t kReserve = 4096;
constexpr double kSR = 48000.0;
// Reset size (64) first, then the host grows to the reserve, then jumps around.
const size_t kBlocks[] = {64, kReserve, 1, 300, kReserve, 128};

// Allocations made by fn().
template <typename Fn>
long CountAllocs(Fn&& fn)
{
  gAllocCount.store(0);
  gCounting.store(true);
  fn();
  gCounting.store(false);
  return gAllocCount.load();
}

struct Block
{
  explicit Block(size_t channels)
  : data(channels, std::vector<double>(kReserve, 0.1))
  , ptrs(channels)
  {
    for (size_t c = 0; c < channels; ++c)
      ptrs[c] = data[c].data();
  }
  double** get() { return ptrs.data(); }
  std::vector<std::vector<double>> data;
  std::vector<double*> ptrs;
};

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string text = ss.str();
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  return text;
}
} // namespace

TEST_CASE("Delay: Process never allocates after Prepare, in every mode, for any block up to the reserve")
{
  dsp::effect::Delay delay;
  delay.Prepare(2, kReserve, kSR);
  Block in(2);
  for (int mode = 0; mode < dsp::effect::Delay::kNumModes; ++mode)
  {
    delay.SetParams(500.0, 0.4, 0.5, mode, kSR, 0.5, 0.5, true);
    for (size_t frames : kBlocks)
    {
      const long allocs = CountAllocs([&] { delay.Process(in.get(), 2, frames); });
      INFO("mode " << mode << " frames " << frames);
      CHECK(allocs == 0);
    }
  }
}

TEST_CASE("Reverb: Process never allocates after Prepare, in every mode, for any block up to the reserve")
{
  dsp::effect::Reverb reverb;
  reverb.Prepare(2, kReserve, kSR);
  Block in(2);
  for (int mode = 0; mode < dsp::effect::Reverb::kNumModes; ++mode)
  {
    reverb.SetParams(0.5, 2.0, 5.0, 20.0, 0.5, mode, kSR, 1);
    for (size_t frames : kBlocks)
    {
      const long allocs = CountAllocs([&] { reverb.Process(in.get(), 2, frames); });
      INFO("mode " << mode << " frames " << frames);
      CHECK(allocs == 0);
    }
  }
}

TEST_CASE("Filters, tone stack, pre EQ and IR shaping reserve their outputs, then never allocate")
{
  Block in(1);

  recursive_linear_filter::HighPass highPass;
  highPass.SetParams(recursive_linear_filter::HighPassParams(kSR, 80.0));
  highPass.ReserveOutputs(1, kReserve);
  recursive_linear_filter::Level level;
  level.ReserveOutputs(1, kReserve);

  dsp::tone_stack::BasicNamToneStack toneStack;
  toneStack.Reset(kSR, 64);
  toneStack.Reserve(1, static_cast<int>(kReserve));

  dsp::effect::VoLumPreEq preEq;
  preEq.Reset(kSR, 64);
  preEq.Reserve(1, kReserve);

  volum::IrShapingLane lane;
  lane.Reserve(1, kReserve);
  int irToken = 0;

  dsp::ImpulseResponse::IRData data;
  data.mRawAudio = {1.0f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f};
  data.mRawAudioSampleRate = 48000.0;
  dsp::ImpulseResponse ir(data, kSR);
  ir.ReserveOutputs(1, kReserve);

  for (size_t frames : kBlocks)
  {
    INFO("frames " << frames);
    CHECK(CountAllocs([&] { highPass.Process(in.get(), 1, frames); }) == 0);
    CHECK(CountAllocs([&] { level.Process(in.get(), 1, frames); }) == 0);
    CHECK(CountAllocs([&] { toneStack.Process(in.get(), 1, static_cast<int>(frames)); }) == 0);
    CHECK(CountAllocs([&] { preEq.Process(in.get(), 1, frames); }) == 0);
    CHECK(CountAllocs([&] { lane.Process(in.get(), 1, static_cast<int>(frames), kSR, 1.0, 80.0, 8000.0, &irToken); })
          == 0);
    CHECK(CountAllocs([&] { ir.Process(in.get(), 1, frames); }) == 0);
  }
}

TEST_CASE("OnReset sizes every post-effect and filter output for the reserved block, not the host reset size")
{
  const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
  const std::string source = ReadText(root / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const auto begin = source.find("void NeuralAmpModeler::OnReset()");
  const auto end = source.find("void NeuralAmpModeler::ProcessMidiMsg(");
  REQUIRE(begin != std::string::npos);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(begin, end - begin);

  CHECK(body.find("mDelay.Prepare(postEffectChannels, postEffectFrames, sampleRate);") != std::string::npos);
  CHECK(body.find("mReverb.Prepare(postEffectChannels, postEffectFrames, sampleRate);") != std::string::npos);
  CHECK(body.find("const size_t postEffectFrames = static_cast<size_t>(reservedBlock);") != std::string::npos);
  for (const char* reserve :
       {"mToneStack->Reserve(", "mSupportToneStack->Reserve(", "mPreEq[i].Reserve(", "mPreInputGain[i].ReserveOutputs(",
        "mPreOutputGain[i].ReserveOutputs(", "mNoiseGateTrigger.ReserveOutputs(", "mNoiseGateGain.ReserveOutputs(",
        "mHighPass.ReserveOutputs(", "mSupportHighPass.ReserveOutputs(", "mIrShaping.Reserve(",
        "mNoiseGateGain.ReserveGainReduction(", "mSupportNoiseGateTrigger.ReserveOutputs(",
        "mSupportNoiseGateGain.ReserveOutputs(", "mSupportNoiseGateGain.ReserveGainReduction(",
        "mPreCompressor.ReserveOutputs(", "mSupportIrShaping.Reserve(", "mIR->ReserveOutputs(",
        "mSupportIR->ReserveOutputs("})
  {
    INFO(reserve);
    CHECK(body.find(reserve) != std::string::npos);
  }
  // A freshly staged IR is sized off the audio thread too.
  CHECK(source.find("stagedIR->ReserveOutputs(") != std::string::npos);
}

TEST_CASE("Compressor: Process never allocates after ReserveOutputs, for any block up to the reserve")
{
  dsp::effect::VoLumCompressor compressor;
  compressor.SetParams(5.0, 4.0, 0.4, 250.0, 1.0, 0.0, kSR);
  compressor.ReserveOutputs(1, kReserve);
  Block in(1);
  for (size_t frames : kBlocks)
  {
    INFO("frames " << frames);
    CHECK(CountAllocs([&] { compressor.Process(in.get(), 1, frames); }) == 0);
  }
}

// MAIN and SUPPORT each own a trigger/gain pair; the trigger hands its gain reduction to the gain
// every block, which used to deep-copy a vector (an allocation on the first block and on growth).
TEST_CASE("Noise gate: trigger to gain handoff never allocates, MAIN and SUPPORT")
{
  for (int lane = 0; lane < 2; ++lane)
  {
    dsp::noise_gate::Trigger trigger;
    dsp::noise_gate::Gain gain;
    trigger.AddListener(&gain);
    trigger.SetSampleRate(kSR);
    trigger.ReserveOutputs(1, kReserve);
    gain.ReserveOutputs(1, kReserve);
    gain.ReserveGainReduction(1, kReserve);
    Block in(1), out(1);
    for (size_t frames : kBlocks)
    {
      INFO((lane ? "SUPPORT" : "MAIN") << " frames " << frames);
      CHECK(CountAllocs([&] {
              auto** gated = trigger.Process(in.get(), 1, frames);
              gain.Process(gated, 1, frames);
            })
            == 0);
    }
  }
}

namespace
{
// One lane of ProcessBlock's DSP objects in signal order, prepared the way OnReset prepares them.
struct LaneChain
{
  LaneChain()
  {
    compressor.SetParams(5.0, 4.0, 0.4, 250.0, 1.0, 0.0, kSR);
    inputGain.SetParams(recursive_linear_filter::LevelParams(2.0));
    outputGain.SetParams(recursive_linear_filter::LevelParams(0.5));
    preEq.Reset(kSR, 64);
    toneStack.Reset(kSR, 64);
    highPass.SetParams(recursive_linear_filter::HighPassParams(kSR, 80.0));
    trigger.AddListener(&gate);
    trigger.SetSampleRate(kSR);
    dsp::ImpulseResponse::IRData data;
    data.mRawAudio = {1.0f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f};
    data.mRawAudioSampleRate = 48000.0;
    ir = std::make_unique<dsp::ImpulseResponse>(data, kSR);
    pitch.Configure(kSR, 64);
    pitch.SetParams(dsp::effect::VoLumPitch::Mode::Transpose, 3.0, 1.0, 0.8, 0.8, 1.0,
                    dsp::effect::VoLumPitch::Voicing::Modern, 0.0, dsp::effect::VoLumPitch::Character::Poly);
    pitch.Reset();
  }

  void Reserve()
  {
    toneStack.Reserve(1, static_cast<int>(kReserve));
    preEq.Reserve(1, kReserve);
    inputGain.ReserveOutputs(1, kReserve);
    outputGain.ReserveOutputs(1, kReserve);
    compressor.ReserveOutputs(1, kReserve);
    trigger.ReserveOutputs(1, kReserve);
    gate.ReserveOutputs(1, kReserve);
    gate.ReserveGainReduction(1, kReserve);
    highPass.ReserveOutputs(1, kReserve);
    shaping.Reserve(1, kReserve);
    ir->ReserveOutputs(1, kReserve);
  }

  void Process(double** in, size_t frames)
  {
    double** p = pitch.Process(in, 1, frames);
    p = compressor.Process(p, 1, frames);
    p = inputGain.Process(p, 1, frames);
    p = preEq.Process(p, 1, frames);
    p = outputGain.Process(p, 1, frames);
    p = trigger.Process(p, 1, frames);
    p = gate.Process(p, 1, frames);
    p = toneStack.Process(p, 1, static_cast<int>(frames));
    p = ir->Process(p, 1, frames);
    p = shaping.Process(p, 1, static_cast<int>(frames), kSR, 1.0, 80.0, 8000.0, this);
    highPass.Process(p, 1, frames);
  }

  dsp::effect::VoLumPitch pitch;
  dsp::effect::VoLumCompressor compressor;
  recursive_linear_filter::Level inputGain, outputGain;
  dsp::effect::VoLumPreEq preEq;
  dsp::noise_gate::Trigger trigger;
  dsp::noise_gate::Gain gate;
  dsp::tone_stack::BasicNamToneStack toneStack;
  std::unique_ptr<dsp::ImpulseResponse> ir;
  volum::IrShapingLane shaping;
  recursive_linear_filter::HighPass highPass;
};
} // namespace

TEST_CASE("Both amp lanes and the POST chain run block growth 64 -> reserve with no allocation")
{
  LaneChain main, support;
  main.Reserve();
  support.Reserve();
  dsp::effect::Delay delay;
  dsp::effect::Reverb reverb;
  delay.Prepare(2, kReserve, kSR);
  reverb.Prepare(2, kReserve, kSR);
  delay.SetParams(500.0, 0.4, 0.5, dsp::effect::Delay::kModeReverse, kSR, 0.5, 0.5, false);
  reverb.SetParams(0.5, 2.0, 5.0, 20.0, 0.5, 0, kSR, 0);
  Block lane(1), post(2);
  for (size_t frames : kBlocks)
  {
    INFO("frames " << frames);
    CHECK(CountAllocs([&] { main.Process(lane.get(), frames); }) == 0);
    CHECK(CountAllocs([&] { support.Process(lane.get(), frames); }) == 0);
    CHECK(CountAllocs([&] {
            auto** wet = delay.Process(post.get(), 2, frames);
            reverb.Process(wet, 2, frames);
          })
          == 0);
  }
}

// Audit: every DSP object ProcessBlock runs through Process() must be reserved (or deliberately
// exempt) in OnReset. A new object added to the chain without a reserve fails here, by name.
TEST_CASE("OnReset reserves every DSP object ProcessBlock runs")
{
  const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
  const std::string block = ReadText(root / "VoLumProcessBlock.inc.cpp");
  const std::string plugin = ReadText(root / "NeuralAmpModeler.cpp");
  const auto begin = plugin.find("void NeuralAmpModeler::OnReset()");
  const auto end = plugin.find("void NeuralAmpModeler::ProcessMidiMsg(");
  REQUIRE(begin != std::string::npos);
  REQUIRE(end != std::string::npos);
  const std::string reset = plugin.substr(begin, end - begin);

  std::set<std::string> processed;
  const std::regex call(R"((m[A-Za-z]+)(\[[a-z]+\])?(\.|->)Process\()");
  for (std::sregex_iterator it(block.begin(), block.end(), call), last; it != last; ++it)
    processed.insert((*it)[1].str());
  CHECK(processed.size() >= 15);
  for (const std::string& name : processed)
  {
    INFO(name);
    if (name == "mPitch" || name == "mDelay" || name == "mReverb" || name == "mChorus" || name == "mTremolo")
      continue;
    const bool reserved = reset.find(name + ".Reserve") != std::string::npos
                          || reset.find(name + "[i].Reserve") != std::string::npos
                          || reset.find(name + "->Reserve") != std::string::npos
                          || reset.find(name + "->ReserveOutputs(") != std::string::npos
                          || reset.find(name + ".ReserveOutputs(") != std::string::npos
                          || reset.find(name + "[i].ReserveOutputs(") != std::string::npos;
    CHECK(reserved);
  }
}