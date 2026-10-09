#include "third_party/doctest.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"
#include "../VoLumChorus.h"
#include "../VoLumResetExclusion.h"
#include "../../AudioDSPTools/dsp/Delay.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
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
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end()); // pins below span lines
  return text;
}

std::string Between(const std::string& text, const std::string& from, const std::string& to)
{
  const auto start = text.find(from);
  REQUIRE(start != std::string::npos);
  const auto end = text.find(to, start + from.size());
  REQUIRE(end != std::string::npos);
  return text.substr(start, end - start);
}

size_t At(const std::string& text, const std::string& needle)
{
  INFO(needle);
  const auto pos = text.find(needle);
  REQUIRE(pos != std::string::npos);
  return pos;
}

std::string PluginCpp()
{
  return ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
}

std::string ProcessBlockBody()
{
  return Between(PluginCpp(), "void NeuralAmpModeler::ProcessBlock(", "void NeuralAmpModeler::OnReset()");
}

std::string OnResetBody()
{
  return Between(PluginCpp(), "void NeuralAmpModeler::OnReset()", "void NeuralAmpModeler::ProcessMidiMsg(");
}

// The objects OnReset re-prepares and ProcessBlock runs, with the plugin's locking.
// Delay::Prepare and the scratch re-assign reallocate on every other reset, so any
// block that overlaps one writes freed memory (an ASan heap-use-after-free).
struct ChainUnderReset
{
  static constexpr int kChannels = 2;
  static constexpr int kHostBlock = 64;

  volum::ResetExclusion exclusion;
  dsp::effect::Delay delay;
  volum::ChorusDSP chorus;
  std::vector<double> scratch;
  double sampleRate = 48000.0;

  void OnReset(int maxBlock, double rate)
  {
    const auto lock = exclusion.BeginReset();
    sampleRate = rate;
    delay.Prepare(kChannels, static_cast<size_t>(maxBlock), rate);
    delay.Reset();
    chorus.Prepare(rate, maxBlock, kChannels);
    chorus.Reset();
    scratch = std::vector<double>(static_cast<size_t>(maxBlock) * 4, 0.0);
  }

  // False when the block lost the race to a reset and was silenced.
  bool ProcessBlock(double** io)
  {
    const auto lock = exclusion.TryBeginBlock();
    if (!lock.owns_lock())
    {
      for (int c = 0; c < kChannels; ++c)
        std::fill(io[c], io[c] + kHostBlock, 0.0);
      return false;
    }
    if (!volum::dsp_staging::ResizeScratchNoAlloc(scratch, kHostBlock))
      return true;
    for (int i = 0; i < kHostBlock; ++i)
      scratch[static_cast<size_t>(i)] = io[0][i];
    chorus.SetParams(0.4, 0.6, 0.5, 0.8, 0.5, 0, sampleRate);
    chorus.Process(io, kChannels, kHostBlock);
    delay.SetParams(120.0, 0.6, 0.5, 0, sampleRate);
    double** out = delay.Process(io, kChannels, kHostBlock);
    for (int c = 0; c < kChannels; ++c)
      std::copy(out[c], out[c] + kHostBlock, io[c]);
    return true;
  }
};
} // namespace

TEST_CASE("A block that overlaps a reset is skipped, and the next one runs")
{
  volum::ResetExclusion exclusion;
  auto blockOwns = [&exclusion] {
    bool owns = false;
    std::thread audio([&] { owns = exclusion.TryBeginBlock().owns_lock(); });
    audio.join();
    return owns;
  };

  auto reset = exclusion.BeginReset();
  CHECK_FALSE(blockOwns());
  reset.unlock();
  CHECK(blockOwns());
}

TEST_CASE("OnReset on another thread never runs while a block processes (AUv2 render race)")
{
  // AUv2 calls OnReset on the main thread while render runs: a rate or buffer change,
  // bypass, or AudioUnitReset when the transport stops. OnReset re-prepared the POST
  // effects and dual-amp scratch under the audio thread's feet. Run under -Asan.
  ChainUnderReset chain;
  chain.OnReset(ChainUnderReset::kHostBlock, 48000.0);

  std::atomic<bool> done{false};
  std::atomic<int> processed{0};
  std::atomic<int> skipped{0};
  std::atomic<bool> finite{true};

  std::thread audio([&] {
    std::vector<double> L(ChainUnderReset::kHostBlock), R(ChainUnderReset::kHostBlock);
    double phase = 0.0;
    while (!done.load(std::memory_order_acquire))
    {
      for (int i = 0; i < ChainUnderReset::kHostBlock; ++i)
      {
        L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = 0.5 * std::sin(phase);
        phase += 0.05;
      }
      double* io[2] = {L.data(), R.data()};
      if (chain.ProcessBlock(io))
        processed.fetch_add(1, std::memory_order_acq_rel);
      else
        skipped.fetch_add(1, std::memory_order_relaxed);
      for (int i = 0; i < ChainUnderReset::kHostBlock; ++i)
        if (!std::isfinite(L[static_cast<size_t>(i)]) || !std::isfinite(R[static_cast<size_t>(i)]))
          finite.store(false);
    }
  });

  constexpr int kResets = 300;
  const int blocks[] = {64, 4096, 256, 2048};
  const double rates[] = {44100.0, 96000.0, 48000.0};
  for (int r = 0; r < kResets; ++r)
  {
    const int seen = processed.load(std::memory_order_acquire);
    chain.OnReset(blocks[r % 4], rates[r % 3]);
    // Let at least one block through between resets, so the two threads interleave.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (processed.load(std::memory_order_acquire) == seen && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
  }
  done.store(true, std::memory_order_release);
  audio.join();

  INFO("processed " << processed.load() << ", skipped " << skipped.load());
  CHECK(processed.load() >= kResets);
  CHECK(finite.load());
}

TEST_CASE("ProcessBlock try-locks the reset exclusion before touching any DSP")
{
  const std::string body = ProcessBlockBody();
  const auto lock = At(body, "const auto resetLock = mResetExclusion.TryBeginBlock();");
  const auto bail = At(body, "if (!resetLock.owns_lock())");
  const auto silence = At(body, "volum::process_io::ClearBuffers(outputs, numFrames, numChannelsExternalOut);");
  CHECK(lock < bail);
  CHECK(bail < silence);
  CHECK(silence < At(body, "AudioBlockFitsReserve("));
  CHECK(silence < At(body, "_PrepareBuffers("));
  CHECK(silence < At(body, "_ApplyDSPStaging();"));
  // The audio thread must never wait for a reset.
  CHECK(body.find("BeginReset()") == std::string::npos);
}

TEST_CASE("OnReset holds the reset exclusion over every re-prepare and makes no host call inside it")
{
  const std::string body = OnResetBody();
  const auto lock = At(body, "const auto resetLock = mResetExclusion.BeginReset();");
  for (const char* prepare : {"_ResetModelAndIR(", "mToneStack->Reset(", "mDelay.Prepare(", "mReverb.Prepare(",
                              "mTremolo.Prepare(", "mChorus.Prepare(", "mDualMainLaneBuffer.assign(",
                              "mDualSupportAlignedBuffer.assign(", "_PrepareBuffers(", "mReservedAudioBlockSize ="})
    CHECK(lock < At(body, prepare));
  // SetLatency can re-enter OnReset (VST3 restartComponent); the mutex is not recursive.
  At(body, "mMetronomeDSP.Reset(sampleRate);\n  }\n  _UpdateLatency();");
}

TEST_CASE("OnReset re-stages the SUPPORT IR at the new rate, like MAIN")
{
  // Only MAIN's IR was resampled, so after a rate change the SUPPORT lane convolved
  // its cab at the old rate: pitched and filtered wrong until the IR was reloaded.
  const std::string body =
    Between(PluginCpp(), "void NeuralAmpModeler::_ResetModelAndIR(", "void NeuralAmpModeler::_SetInputGain()");
  const auto lock = At(body, "std::lock_guard<std::mutex> lock(mStagingMutex);");
  At(body, "replacedIr = volum::dsp_staging::RestageIrForSampleRate(mStagedIR, mIR, sampleRate);");
  At(body, "replacedSupportIr = volum::dsp_staging::RestageIrForSampleRate(mStagedSupportIR, mSupportIR, sampleRate);");
  // The IRs a re-stage replaces are destroyed after the staging lock is released.
  CHECK(At(body, "std::unique_ptr<dsp::ImpulseResponse> replacedIr;") < lock);
  CHECK(At(body, "std::unique_ptr<dsp::ImpulseResponse> replacedSupportIr;") < lock);
  CHECK(body.find("std::make_unique<dsp::ImpulseResponse>") == std::string::npos);
}

TEST_CASE("Both IR lanes' cuts start clean on OnReset, on an idle block and on a new convolver")
{
  const std::string reset = OnResetBody();
  CHECK(At(reset, "mResetExclusion.BeginReset();") < At(reset, "mIrShaping.Reset();"));
  At(reset, "mSupportIrShaping.Reset();");

  const std::string process = ProcessBlockBody();
  At(process, "if (!processingPlan.runIR)\n    mIrShaping.Reset();");
  At(process, "if (!processingPlan.runSupportIR)\n    mSupportIrShaping.Reset();");

  const std::string scene = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSceneRig.inc.cpp");
  const std::string apply = Between(scene, "iplug::sample** NeuralAmpModeler::_VolumApplyIrShaping(", "\n}\n");
  At(apply, "static_cast<const void*>(mSupportIR.get())");
  At(apply, "static_cast<const void*>(mIR.get())");
  At(apply, "lane.Process(");
}

TEST_CASE("Host latency asks the plan's SUPPORT rule instead of the Dual Amp toggle alone")
{
  // Dual Amp with SUPPORT loaded but MAIN missing reported SUPPORT's latency for a
  // block that plays silence. AmpLatencySamples is pinned in test_volum_processing_plan.cpp.
  const std::string body = Between(PluginCpp(), "int NeuralAmpModeler::_ReportedLatencySamples() const",
                                   "void NeuralAmpModeler::_ApplyLatchedLatency()");
  At(body, "volum::AmpLatencySamples(mModel != nullptr,");
  CHECK(body.find("GetParam(kDualAmpActive)->Bool() && mSupportModel") == std::string::npos);
}
