#include "third_party/doctest.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"
#include "../VoLumChorus.h"
#include "../VoLumLatencyRequests.h"
#include "../VoLumLatencySnapshot.h"
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

// Joins clang-format's wrapped lines so a pin survives re-wrapping.
std::string Unwrapped(const std::string& text)
{
  std::string out;
  for (size_t i = 0; i < text.size(); ++i)
  {
    if (text[i] != '\n')
    {
      out += text[i];
      continue;
    }
    while (i + 1 < text.size() && text[i + 1] == ' ')
      ++i;
  }
  return out;
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

std::string LatencyBody()
{
  return Unwrapped(Between(PluginCpp(), "int NeuralAmpModeler::_ReportedLatencySamples() const",
                           "void NeuralAmpModeler::_VolumSetSupportSelected("));
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
  const std::string body = LatencyBody();
  At(body, "volum::AmpLatencySamples(lanes.main != kNoModel,");
  At(body, "haveSupportModel, lanes.support);");
  CHECK(body.find("GetParam(kDualAmpActive)->Bool() && mSupportModel") == std::string::npos);
}

TEST_CASE("Host latency reads the published lane snapshot, never the live model pointers")
{
  // The report is computed outside the reset exclusion (after OnReset unlocks, from
  // OnUIOpen and OnIdle) while ProcessBlock may publish or retire those models.
  const std::string body = LatencyBody();
  At(body, "const volum::LaneLatencies lanes = mLiveLatency.Read();");
  for (const char* live : {"mModel", "mSupportModel", "mPreModel"})
  {
    INFO(live);
    CHECK(body.find(live) == std::string::npos);
  }

  const std::string reset = OnResetBody();
  const auto lock = At(reset, "mResetExclusion.BeginReset();");
  const auto publish = At(reset, "_VolumPublishLiveLatency();");
  CHECK(lock < publish);
  CHECK(At(reset, "_ResetModelAndIR(") < publish);
  CHECK(publish < At(reset, "mMetronomeDSP.Reset(sampleRate);\n  }\n  _UpdateLatency();"));

  const std::string staging =
    Between(PluginCpp(), "void NeuralAmpModeler::_ApplyDSPStaging()", "void NeuralAmpModeler::_ResetModelAndIR(");
  At(staging, "_VolumPublishLiveLatency();");
}

TEST_CASE("Every latency input asks the main thread to recompute, and only the main thread reports")
{
  const std::string cpp = PluginCpp();
  // Each publish asks, so the audio thread never latches a value of its own.
  const std::string publish = Between(
    cpp, "void NeuralAmpModeler::_VolumPublishLiveLatency()", "int NeuralAmpModeler::_ReportedLatencySamples()");
  CHECK(At(publish, "mLiveLatency.Publish(lanes);") < At(publish, "mLatencyRequests.Request();"));

  // Dual Amp, PRE NAM and PRE Pitch params: one request ahead of the switch, in the
  // body every param source (host, UI, MIDI) reaches.
  const std::string onParam =
    Between(cpp, "void NeuralAmpModeler::OnParamChange(int paramIdx, EParamSource source, int sampleOffset)",
            "    // Changes to the input gain");
  CHECK(At(onParam, "if (volum::ParamAffectsReportedLatency(paramIdx))\n    mLatencyRequests.Request();")
        < At(onParam, "switch (paramIdx)"));

  // SUPPORT selection: one writer, which asks whenever the selection changes.
  const std::string setter =
    Between(cpp, "void NeuralAmpModeler::_VolumSetSupportSelected(", "void NeuralAmpModeler::_UpdateLatency()");
  At(setter, "if (mVolumSupportSelected.exchange(selected) != selected)\n    mLatencyRequests.Request();");
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");
  CHECK(loader.find("mVolumSupportSelected.store(") == std::string::npos);
  At(loader, "_VolumSetSupportSelected(false);");
  At(loader, "_VolumSetSupportSelected(true);");

  const std::string idle = Between(cpp, "void NeuralAmpModeler::OnIdle()", "bool NeuralAmpModeler::SerializeState(");
  At(Unwrapped(idle),
     "mLatencyRequests.Service([this] { return _ReportedLatencySamples(); }, [this](int latency) { "
     "_ApplyReportedLatency(latency); });");
  // The old latch let a main-thread recompute overwrite a newer audio-thread value.
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");
  for (const std::string latch : {std::string("mPending") + "Latency", std::string("mLatency") + "Dirty"})
  {
    INFO(latch);
    CHECK(cpp.find(latch) == std::string::npos);
    CHECK(header.find(latch) == std::string::npos);
  }
}

TEST_CASE("The latency-input param list covers every param the latency report reads")
{
  const std::string body = LatencyBody();
  const std::string requests = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLatencyRequests.h");
  const std::string list = Between(requests, "inline bool ParamAffectsReportedLatency(", "default: return false;");
  int reads = 0;
  for (size_t pos = body.find("GetParam(k"); pos != std::string::npos; pos = body.find("GetParam(k", pos + 1))
  {
    const size_t start = pos + std::string("GetParam(").size();
    const std::string param = body.substr(start, body.find(')', start) - start);
    INFO(param);
    CHECK(list.find("case " + param + ":") != std::string::npos);
    ++reads;
  }
  CHECK(reads >= 8);

  CHECK(volum::ParamAffectsReportedLatency(kDualAmpActive));
  CHECK(volum::ParamAffectsReportedLatency(kPreNam2Capture));
  CHECK(volum::ParamAffectsReportedLatency(kPrePitchTransChar));
  CHECK_FALSE(volum::ParamAffectsReportedLatency(kMainAmpPan));
  CHECK_FALSE(volum::ParamAffectsReportedLatency(kSupportAmpIdx));
}

namespace
{
struct HostLatency
{
  int reported = -1;
  int reports = 0;
};

int ComputeLatency(const volum::LiveLatencySnapshot& snapshot)
{
  return std::max(0, snapshot.Read().main);
}
} // namespace

TEST_CASE("A model published during a main-thread recompute is still reported (lost update)")
{
  volum::LiveLatencySnapshot snapshot;
  volum::LatencyRecomputeRequests requests;
  HostLatency host;
  auto apply = [&](int latency) {
    host.reported = latency;
    ++host.reports;
  };
  auto publish = [&](int mainLatency) {
    volum::LaneLatencies lanes;
    lanes.main = mainLatency;
    snapshot.Publish(lanes);
    requests.Request();
  };

  CHECK_FALSE(requests.Service([&] { return ComputeLatency(snapshot); }, apply));

  publish(64);
  // The main thread reads the old snapshot, then the audio thread publishes a new
  // model before the recompute is reported.
  CHECK(requests.Service(
    [&] {
      const int seen = ComputeLatency(snapshot);
      publish(4096);
      return seen;
    },
    apply));
  CHECK(host.reported == 64);
  CHECK(requests.Pending());

  CHECK(requests.Service([&] { return ComputeLatency(snapshot); }, apply));
  CHECK(host.reported == 4096);
  CHECK_FALSE(requests.Pending());
  CHECK_FALSE(requests.Service([&] { return ComputeLatency(snapshot); }, apply));
  CHECK(host.reports == 2);
}

TEST_CASE("A selection or param change with no model swap still reaches the host")
{
  // Dual Amp off / SUPPORT deselected: ProcessBlock drops SUPPORT at once and no
  // model is retired, so the request is the only thing that re-reports latency.
  volum::LatencyRecomputeRequests requests;
  bool supportPlays = true;
  int reported = -1;
  auto compute = [&] { return supportPlays ? 4096 : 32; };
  auto apply = [&](int latency) { reported = latency; };
  requests.Request();
  CHECK(requests.Service(compute, apply));
  CHECK(reported == 4096);

  supportPlays = false;
  requests.Request();
  CHECK(requests.Service(compute, apply));
  CHECK(reported == 32);
}

TEST_CASE("Concurrent publishes always end with the newest latency reported")
{
  volum::LiveLatencySnapshot snapshot;
  volum::LatencyRecomputeRequests requests;
  std::atomic<bool> done{false};
  constexpr int kPublishes = 100000;
  std::thread audio([&] {
    for (int i = 1; i <= kPublishes; ++i)
    {
      volum::LaneLatencies lanes;
      lanes.main = i % volum::LiveLatencySnapshot::kMaxLatency;
      snapshot.Publish(lanes);
      requests.Request();
    }
    done.store(true);
  });
  int reported = -1;
  auto compute = [&] { return ComputeLatency(snapshot); };
  auto apply = [&](int latency) { reported = latency; };
  while (!done.load())
    requests.Service(compute, apply);
  audio.join();
  requests.Service(compute, apply);
  CHECK_FALSE(requests.Pending());
  CHECK(reported == kPublishes % volum::LiveLatencySnapshot::kMaxLatency);
}

TEST_CASE("Host latency and ProcessBlock share one SUPPORT selected-and-loaded rule")
{
  // A deselected SUPPORT stays loaded until its removal is staged; latency still
  // counted it while ProcessBlock already played MAIN alone.
  const std::string latency = LatencyBody();
  At(latency,
     "volum::HaveSelectedSupportModel(mVolumSupportSelected.load(std::memory_order_relaxed), lanes.support "
     "!= kNoModel)");
  At(ProcessBlockBody(), "volum::HaveSelectedSupportModel(supportAmpSelected, mSupportModel != nullptr)");
}

TEST_CASE("Latency snapshot starts empty and round-trips each lane")
{
  volum::LiveLatencySnapshot snapshot;
  const auto empty = snapshot.Read();
  CHECK(empty.main == volum::LaneLatencies::kNoModel);
  CHECK(empty.support == volum::LaneLatencies::kNoModel);
  CHECK(empty.pre[0] == volum::LaneLatencies::kNoModel);
  CHECK(empty.pre[1] == volum::LaneLatencies::kNoModel);

  volum::LaneLatencies lanes;
  lanes.main = 0; // loaded at the native rate: present, zero latency
  lanes.support = 37;
  lanes.pre[1] = 1234;
  snapshot.Publish(lanes);
  const auto read = snapshot.Read();
  CHECK(read.main == 0);
  CHECK(read.support == 37);
  CHECK(read.pre[0] == volum::LaneLatencies::kNoModel);
  CHECK(read.pre[1] == 1234);

  lanes.main = 1 << 20;
  snapshot.Publish(lanes);
  CHECK(snapshot.Read().main == volum::LiveLatencySnapshot::kMaxLatency);
}

TEST_CASE("A latency snapshot read never mixes lanes from two publishes")
{
  volum::LaneLatencies a;
  a.main = 11;
  a.support = 12;
  a.pre[0] = 13;
  a.pre[1] = 14;
  volum::LaneLatencies b;
  b.main = 21;
  b.support = volum::LaneLatencies::kNoModel;
  b.pre[0] = 23;
  b.pre[1] = volum::LaneLatencies::kNoModel;

  volum::LiveLatencySnapshot snapshot;
  snapshot.Publish(a);
  std::atomic<bool> done{false};
  std::thread publisher([&] {
    for (int i = 0; i < 200000; ++i)
      snapshot.Publish(i % 2 ? a : b);
    done.store(true);
  });
  int torn = 0;
  long reads = 0;
  while (!done.load())
  {
    const auto r = snapshot.Read();
    const bool isA = r.main == a.main && r.support == a.support && r.pre[0] == a.pre[0] && r.pre[1] == a.pre[1];
    const bool isB = r.main == b.main && r.support == b.support && r.pre[0] == b.pre[0] && r.pre[1] == b.pre[1];
    torn += (isA || isB) ? 0 : 1;
    ++reads;
  }
  publisher.join();
  CHECK(reads > 0);
  CHECK(torn == 0);
}
