#include "third_party/doctest.h"

#include "../VoLumPeakAvgSender.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
template <int NC>
using SenderBase = iplug::ISender<NC, 64, std::pair<float, float>>;
template <int NC>
using SenderData = iplug::ISenderData<NC, std::pair<float, float>>;
template <int NC>
using SenderQueuePtr = iplug::IPlugQueue<SenderData<NC>> SenderBase<NC>::*;

// ISender keeps its queue private and drains it only into an IEditorDelegate. Access
// checks do not apply to explicit instantiation arguments, which lets the test pop the
// queue directly without building a delegate.
template <int NC, SenderQueuePtr<NC> Member>
struct SenderQueueAccess
{
  friend SenderQueuePtr<NC> SenderQueue(SenderBase<NC>*) { return Member; }
};
SenderQueuePtr<1> SenderQueue(SenderBase<1>*);
SenderQueuePtr<2> SenderQueue(SenderBase<2>*);
template struct SenderQueueAccess<1, &SenderBase<1>::mQueue>;
template struct SenderQueueAccess<2, &SenderBase<2>::mQueue>;

template <int NC>
std::vector<SenderData<NC>> Drain(SenderBase<NC>& sender)
{
  std::vector<SenderData<NC>> out;
  SenderData<NC> d;
  while ((sender.*SenderQueue(static_cast<SenderBase<NC>*>(nullptr))).Pop(d))
    out.push_back(d);
  return out;
}

template <int NC>
bool SameData(const SenderData<NC>& a, const SenderData<NC>& b)
{
  if (a.ctrlTag != b.ctrlTag || a.nChans != b.nChans || a.chanOffset != b.chanOffset)
    return false;
  for (int c = 0; c < NC; ++c)
  {
    const float av[2] = {a.vals[c].first, a.vals[c].second};
    const float bv[2] = {b.vals[c].first, b.vals[c].second};
    if (std::memcmp(av, bv, sizeof(av)) != 0)
      return false;
  }
  return true;
}

// VoLum's NAMSender settings (NeuralAmpModeler.h).
template <int NC>
iplug::IPeakAvgSender<NC> MakeReference()
{
  return iplug::IPeakAvgSender<NC>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f);
}
template <int NC>
volum::PeakAvgSender<NC> MakeCandidate()
{
  return volum::PeakAvgSender<NC>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f);
}

// Guitar-like level changes: silence (below the -90 dB push threshold), quiet, loud
// and clipped stretches, so the threshold gate and the peak hold both switch.
template <int NC>
struct Signal
{
  std::mt19937 rng{0x5EEDu};
  std::vector<std::vector<iplug::sample>> channels = std::vector<std::vector<iplug::sample>>(NC);
  std::vector<iplug::sample*> pointers = std::vector<iplug::sample*>(NC);
  double level = 0.5;
  int levelLeft = 0;

  iplug::sample** Next(int nFrames)
  {
    std::uniform_real_distribution<double> noise(-1.0, 1.0);
    for (int c = 0; c < NC; ++c)
      channels[c].resize((size_t)nFrames);
    for (int s = 0; s < nFrames; ++s)
    {
      if (levelLeft-- <= 0)
      {
        static const double kLevels[] = {0.0, 1e-6, 0.01, 0.3, 0.9, 1.6};
        level = kLevels[rng() % 6];
        levelLeft = 1 + (int)(rng() % 20000);
      }
      for (int c = 0; c < NC; ++c)
        channels[c][(size_t)s] = level * noise(rng);
    }
    for (int c = 0; c < NC; ++c)
      pointers[c] = channels[c].data();
    return pointers.data();
  }
};

struct Step
{
  int frames = 0; // > 0: process a block of this size
  double resetRate = 0; // > 0: Reset(resetRate) before the block
};

template <int NC>
void CheckSameSequence(const std::vector<Step>& steps, int& pushes)
{
  auto reference = MakeReference<NC>();
  auto candidate = MakeCandidate<NC>();
  Signal<NC> signal;
  int stepIndex = 0;
  for (const Step& step : steps)
  {
    if (step.resetRate > 0)
    {
      reference.Reset(step.resetRate);
      candidate.Reset(step.resetRate);
    }
    if (step.frames > 0)
    {
      iplug::sample** in = signal.Next(step.frames);
      reference.ProcessBlock(in, step.frames, 7, NC);
      candidate.ProcessBlock(in, step.frames, 7, NC);
    }
    const auto want = Drain<NC>(reference);
    const auto got = Drain<NC>(candidate);
    INFO("step " << stepIndex << " frames " << step.frames << " reset " << step.resetRate);
    REQUIRE(got.size() == want.size());
    for (size_t i = 0; i < want.size(); ++i)
      REQUIRE(SameData<NC>(got[i], want[i]));
    pushes += (int)want.size();
    ++stepIndex;
  }
}

std::vector<Step> RandomSteps(unsigned seed, int count)
{
  std::mt19937 rng(seed);
  static const double kRates[] = {22050.0, 44100.0, 48000.0, 88200.0, 96000.0};
  static const int kCommonBlocks[] = {1, 16, 32, 64, 128, 256, 480, 512, 1024, 2048};
  std::vector<Step> steps;
  steps.push_back({0, 48000.0});
  for (int i = 0; i < count; ++i)
  {
    Step step;
    step.frames = (rng() % 2) ? kCommonBlocks[rng() % 10] : 1 + (int)(rng() % 3000);
    if (rng() % 40 == 0)
      step.resetRate = kRates[rng() % 5];
    steps.push_back(step);
  }
  return steps;
}

std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

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

std::string Between(const std::string& text, const std::string& from, const std::string& to)
{
  const auto begin = text.find(from);
  REQUIRE(begin != std::string::npos);
  const auto end = text.find(to, begin + from.size());
  REQUIRE(end != std::string::npos);
  return text.substr(begin, end - begin);
}

size_t Count(const std::string& text, const std::string& needle)
{
  size_t n = 0;
  for (auto pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size()))
    ++n;
  return n;
}
} // namespace

TEST_CASE("VoLum meter sender pushes the IPeakAvgSender sequence over random blocks")
{
  int pushes = 0;
  CheckSameSequence<1>(RandomSteps(1u, 3000), pushes);
  CheckSameSequence<1>(RandomSteps(2u, 3000), pushes);
  CheckSameSequence<2>(RandomSteps(3u, 1500), pushes);
  CHECK(pushes > 10000);
}

TEST_CASE("VoLum meter sender matches IPeakAvgSender after a window shrink mid-count")
{
  int pushes = 0;
  SUBCASE("mCount lands inside twice the new window")
  {
    // 48 kHz window 240, count 230; 44.1 kHz window 220.
    CheckSameSequence<1>({{0, 48000.0}, {64}, {128}, {38}, {100, 44100.0}, {512}, {2048}, {64}}, pushes);
  }
  SUBCASE("mCount lands past twice the new window")
  {
    // 96 kHz window 480, count 470; 22.05 kHz window 110.
    CheckSameSequence<1>({{0, 96000.0}, {256}, {214}, {64, 22050.0}, {512}, {33}, {1024}}, pushes);
  }
  SUBCASE("mCount equals the old window end")
  {
    CheckSameSequence<1>({{0, 48000.0}, {239}, {64, 44100.0}, {300}}, pushes);
  }
  CHECK(pushes > 0);
}

TEST_CASE("VoLum meter sender Restart matches a freshly constructed sender")
{
  auto used = MakeCandidate<1>();
  used.Reset(48000.0);
  Signal<1> warm;
  for (int i = 0; i < 200; ++i)
    used.ProcessBlock(warm.Next(97), 97, 7, 1);
  Drain<1>(used);
  used.Restart();

  auto fresh = MakeCandidate<1>();
  fresh.Reset(48000.0);

  Signal<1> signal;
  int pushes = 0;
  for (int i = 0; i < 400; ++i)
  {
    const int frames = 1 + (int)(signal.rng() % 700);
    iplug::sample** in = signal.Next(frames);
    used.ProcessBlock(in, frames, 7, 1);
    fresh.ProcessBlock(in, frames, 7, 1);
    const auto want = Drain<1>(fresh);
    const auto got = Drain<1>(used);
    REQUIRE(got.size() == want.size());
    for (size_t k = 0; k < want.size(); ++k)
      REQUIRE(SameData<1>(got[k], want[k]));
    pushes += (int)want.size();
  }
  CHECK(pushes > 100);
}

TEST_CASE("Meter gate skips while the editor is closed and restarts on the first open block")
{
  using volum::MeterGateStep;
  bool ran = false;
  CHECK(volum::StepMeterGate(false, ran) == MeterGateStep::Skip);
  CHECK(volum::StepMeterGate(false, ran) == MeterGateStep::Skip);
  CHECK(volum::StepMeterGate(true, ran) == MeterGateStep::RestartThenRun);
  CHECK(volum::StepMeterGate(true, ran) == MeterGateStep::Run);
  CHECK(volum::StepMeterGate(true, ran) == MeterGateStep::Run);
  CHECK(volum::StepMeterGate(false, ran) == MeterGateStep::Skip);
  CHECK(volum::StepMeterGate(true, ran) == MeterGateStep::RestartThenRun);
  CHECK(volum::StepMeterGate(true, ran) == MeterGateStep::Run);
}

TEST_CASE("ProcessBlock runs meter work only through the editor gate")
{
  const std::string plugin = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");

  CHECK(header.find("class NAMSender : public volum::PeakAvgSender<>") != std::string::npos);
  CHECK(header.find("std::atomic<bool> mVolumEditorOpen{false};") != std::string::npos);

  const std::string process =
    Between(plugin, "void NeuralAmpModeler::ProcessBlock(", "void NeuralAmpModeler::OnReset()");
  const auto gate =
    process.find("volum::StepMeterGate(mVolumEditorOpen.load(std::memory_order_acquire), mVolumMetersRanLastBlock)");
  REQUIRE(gate != std::string::npos);

  const std::string restart = Between(process, "if (meterStep == volum::MeterGateStep::RestartThenRun)", "}");
  CHECK(restart.find("mInputSender.Restart();") != std::string::npos);
  CHECK(restart.find("mOutputSender.Restart();") != std::string::npos);
  CHECK(restart.find("mOutputSenderR.Restart();") != std::string::npos);

  const auto dualScan = process.find("if (runMeters && processingPlan.runDualAmp)");
  REQUIRE(dualScan != std::string::npos);
  CHECK(gate < dualScan);
  CHECK(Count(process, "processingPlan.runDualAmp)") == 1);
  CHECK(process.find("mVolumDualAmpOutputHot.store(false);", dualScan) != std::string::npos);

  CHECK(Count(process, "_UpdateMeters(") == 1);
  const auto meters = process.find("if (runMeters)\n    _UpdateMeters(");
  REQUIRE(meters != std::string::npos);
  CHECK(gate < meters);

  // The master-safety hold keeps counting with the editor closed.
  const auto safety = process.find("mMasterSafetyEngaged.store(mMasterSafetyHoldSamples > 0);");
  REQUIRE(safety != std::string::npos);
  CHECK(Between(process, "bool safetyEngagedThisBlock", "mMasterSafetyEngaged.store").find("runMeters")
        == std::string::npos);

  const std::string open = Between(plugin, "void NeuralAmpModeler::OnUIOpen()", "\n}\n");
  CHECK(open.find("mVolumEditorOpen.store(true, std::memory_order_release);") != std::string::npos);
  const std::string close = Between(plugin, "void NeuralAmpModeler::OnUIClose()", "\n}\n");
  CHECK(close.find("mVolumEditorOpen.store(false, std::memory_order_release);") != std::string::npos);
}
