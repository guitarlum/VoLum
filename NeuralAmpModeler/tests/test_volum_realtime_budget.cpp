#include "third_party/doctest.h"
#include "VoLumBurstTiming.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "activations.h"
#include "dsp.h"
#include "get_dsp.h"
#include "slimmable.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"
#include "../architecture.hpp"
#if defined(ARCH_X86)
  #include <immintrin.h>
#endif

// Realtime budget of the NAM chain VoLum actually runs: bundled captures,
// Reset exactly as the plugin resets them (NamResetBlockSize), processed at
// small host blocks. 1.3.0 shipped a crackle because every NAM was Reset at the
// 8192 scratch reserve: nothing failed, the audio just missed its deadline.
//
// Three guards. Every model in the chain under test must be Reset at
// NamResetBlockSize (the host block, far below the 8192 reserve); the plugin's
// call sites are source-locked in test_volum_ui_regressions.cpp. The absolute
// share of the deadline is generous so a slow CI runner still passes, but PRE
// NAM + amp that cannot keep up does not. And the chain's cost in units of an
// Eigen GEMM yardstick timed right after every block, which is what catches a
// 2x regression: a loaded machine slows both alike (an efficiency core, a busy
// SMT sibling, an all-core clock), so the ratio does not move with load.
//
// Each block is timed on the test thread's CPU clock, on a thread set up like
// a host's audio thread, and the cost is a low percentile of block-by-block
// alternation, best of kRuns whole sequences: load only ever adds time. The
// cost against the same chain Reset at the reserve is printed only: the A2 ring
// used to refresh a reserve-sized mirror every block, and since it copies only
// written columns the two chains cost about the same.

#if defined(__SANITIZE_ADDRESS__)
  #define VOLUM_BUDGET_SANITIZED 1
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
    #define VOLUM_BUDGET_SANITIZED 1
  #endif
#endif

namespace
{
#if !defined(NDEBUG) || defined(VOLUM_BUDGET_SANITIZED)
constexpr bool kSkipBudget = true;
#else
constexpr bool kSkipBudget = false;
#endif

constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kRuns = 3;
constexpr double kCostPercentile = 0.25;

#if defined(ARCH_X86)
constexpr unsigned int kFtzDaz = 0x8040;
#endif

unsigned int ReadFpMode()
{
#if defined(ARCH_X86)
  return _mm_getcsr();
#else
  return 0;
#endif
}

// ProcessBlock runs the NAM chain with FTZ|DAZ set. Restoring the saved MXCSR
// keeps every later test case, the golden renders included, on IEEE denormals.
class ScopedDenormalsOff
{
public:
  ScopedDenormalsOff()
  {
#if defined(ARCH_X86)
    mSaved = _mm_getcsr();
    disable_denormals();
#endif
  }
  ~ScopedDenormalsOff()
  {
#if defined(ARCH_X86)
    _mm_setcsr(mSaved);
#endif
  }
  ScopedDenormalsOff(const ScopedDenormalsOff&) = delete;
  ScopedDenormalsOff& operator=(const ScopedDenormalsOff&) = delete;

#if defined(ARCH_X86)
private:
  unsigned int mSaved = 0;
#endif
};

std::filesystem::path RigsRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "rigs";
}

std::filesystem::path FirstNam(const std::filesystem::path& dir, const std::string& prefix)
{
  std::vector<std::filesystem::path> hits;
  for (const auto& e : std::filesystem::directory_iterator(dir))
  {
    const std::string name = e.path().filename().string();
    if (e.path().extension() == ".nam" && name.rfind(prefix, 0) == 0)
      hits.push_back(e.path());
  }
  std::sort(hits.begin(), hits.end());
  const std::string missing = dir.string() + " has no " + prefix + "*.nam";
  REQUIRE_MESSAGE(!hits.empty(), missing);
  return hits.front();
}

struct Chain
{
  std::vector<std::unique_ptr<nam::DSP>> models;

  void Load(const std::vector<std::filesystem::path>& paths, bool full)
  {
    nam::activations::Activation::enable_fast_tanh();
    for (const auto& p : paths)
    {
      auto m = nam::get_dsp(p);
      REQUIRE_MESSAGE(m != nullptr, p.string());
      if (auto* slim = dynamic_cast<nam::SlimmableModel*>(m.get()))
        slim->SetSlimmableSize(full ? 1.0 : 0.0);
      models.push_back(std::move(m));
    }
  }

  void Reset(int maxBlock)
  {
    resetBlock = maxBlock;
    for (auto& m : models)
      m->ResetAndPrewarm(kSampleRate, maxBlock);
  }

  // Series chain like PRE NAM -> amp (support lanes run on the same thread too,
  // so a series sum is the same per-block cost). Returns thread CPU microseconds.
  double ProcessTimed(const std::vector<NAM_SAMPLE>& src, int block)
  {
    a.assign(src.begin(), src.begin() + block);
    b.resize(static_cast<size_t>(block));
    const ScopedDenormalsOff denormalsOff;
    const double t0 = volum_test::ThreadCpuUs();
    NAM_SAMPLE* in = a.data();
    NAM_SAMPLE* out = b.data();
    for (auto& m : models)
    {
      NAM_SAMPLE* ip[1] = {in};
      NAM_SAMPLE* op[1] = {out};
      m->process(ip, op, block);
      std::swap(in, out);
    }
    const double t1 = volum_test::ThreadCpuUs();
    fpModeDuringProcess = ReadFpMode();
    return t1 - t0;
  }

  std::vector<NAM_SAMPLE> a, b;
  int resetBlock = 0;
  unsigned int fpModeDuringProcess = 0;
};

void FillChordBlock(std::vector<NAM_SAMPLE>& dst, int block, double& t)
{
  dst.resize(static_cast<size_t>(block));
  for (int i = 0; i < block; ++i, t += 1.0 / kSampleRate)
    dst[static_cast<size_t>(i)] =
      static_cast<NAM_SAMPLE>(0.08
                              * (std::sin(2.0 * kPi * 82.41 * t) + std::sin(2.0 * kPi * 123.47 * t)
                                 + std::sin(2.0 * kPi * 164.81 * t) + std::sin(2.0 * kPi * 207.65 * t)));
}

double Percentile(std::vector<double> v, double q)
{
  if (v.empty())
    return 0.0;
  const auto k = static_cast<std::ptrdiff_t>(q * static_cast<double>(v.size() - 1));
  std::nth_element(v.begin(), v.begin() + k, v.end());
  return v[static_cast<size_t>(k)];
}

// The Eigen GEMM the A2 fast path spends its time in, at a fixed size and with
// no NAM or VoLum code in it, so a regression there cannot move the yardstick.
struct GemmYardstick
{
  Eigen::MatrixXf w = Eigen::MatrixXf::Constant(16, 16, 0.05f);
  Eigen::MatrixXf x, y;
  float sink = 0.0f;

  double Timed(int block)
  {
    x = Eigen::MatrixXf::Constant(16, block, 0.3f);
    const double t0 = volum_test::ThreadCpuUs();
    for (int pass = 0; pass < 32; ++pass)
    {
      y.noalias() = w * x;
      x = y.cwiseMax(-1.0f).cwiseMin(1.0f);
    }
    const double t1 = volum_test::ThreadCpuUs();
    sink += x(0, 0);
    return t1 - t0;
  }
};

struct Measured
{
  double production = 0.0;
  double oversized = 0.0;
  double yardsticks = 0.0;
};

// Two chains loaded once and never Reset between samples, alternated block by
// block on the same input so a burst of machine noise lands on both. The
// yardstick runs right after every production block, on the same core.
Measured MeasureOnce(Chain& production, Chain& oversized, int block, double& t)
{
  GemmYardstick yardstick;
  const int numBlocks = std::max(256, static_cast<int>(0.6 * kSampleRate / block));
  std::vector<NAM_SAMPLE> src;
  for (int n = 0; n < numBlocks / 4; ++n)
  {
    FillChordBlock(src, block, t);
    production.ProcessTimed(src, block);
    oversized.ProcessTimed(src, block);
  }
  std::vector<double> p, o, y;
  p.reserve(static_cast<size_t>(numBlocks));
  o.reserve(static_cast<size_t>(numBlocks));
  y.reserve(static_cast<size_t>(numBlocks));
  for (int n = 0; n < numBlocks; ++n)
  {
    FillChordBlock(src, block, t);
    if (n % 2 == 0)
    {
      p.push_back(production.ProcessTimed(src, block));
      y.push_back(p.back() / std::max(1e-3, yardstick.Timed(block)));
      o.push_back(oversized.ProcessTimed(src, block));
    }
    else
    {
      o.push_back(oversized.ProcessTimed(src, block));
      p.push_back(production.ProcessTimed(src, block));
      y.push_back(p.back() / std::max(1e-3, yardstick.Timed(block)));
    }
  }
  CHECK(std::isfinite(yardstick.sink));
  return {Percentile(p, kCostPercentile), Percentile(o, kCostPercentile), Percentile(y, kCostPercentile)};
}

Measured Measure(Chain& production, Chain& oversized, int block)
{
  production.Reset(volum::dsp_staging::NamResetBlockSize(block));
  oversized.Reset(volum::dsp_staging::ReservedAudioBlockSize(block));
  const volum_test::ScopedRealtimeThread realtime;
  double t = 0.0;
  Measured best = MeasureOnce(production, oversized, block, t);
  for (int run = 1; run < kRuns; ++run)
  {
    const Measured m = MeasureOnce(production, oversized, block, t);
    best.production = std::min(best.production, m.production);
    best.oversized = std::min(best.oversized, m.oversized);
    best.yardsticks = std::min(best.yardsticks, m.yardsticks);
  }
  return best;
}

struct Limits
{
  double share = 0.0; // of the deadline; <= 0 skips it (cost the hardware owns, not VoLum)
  double fullYardsticks = 0.0; // <= 0 skips it
  double liteYardsticks = 0.0;
};

void CheckChain(const std::vector<std::filesystem::path>& paths, const Limits& limits, const std::string& label)
{
  for (const bool full : {true, false})
  {
    Chain production;
    Chain oversized;
    production.Load(paths, full);
    oversized.Load(paths, full);
    for (const int block : {64, 128})
    {
      const auto m = Measure(production, oversized, block);
      const double deadlineUs = 1e6 * block / kSampleRate;
      const double share = m.production / deadlineUs;
      const double ratio = m.production / std::max(1e-3, m.oversized);
      MESSAGE(label << (full ? " FULL" : " LITE") << " block " << block << ": " << m.production << " us CPU ("
                    << 100.0 * share << "% of deadline), " << m.yardsticks << " yardsticks, vs the 8192-reserve reset "
                    << ratio);
      const int resetBlock = volum::dsp_staging::NamResetBlockSize(block);
      CHECK(resetBlock == block);
      CHECK(resetBlock * 16 <= volum::dsp_staging::ReservedAudioBlockSize(block));
      CHECK(production.resetBlock == resetBlock);
      if (limits.share > 0.0)
        CHECK(share < limits.share);
#if defined(ARCH_X86)
      // Calibrated on x86-64 only; Apple silicon prints its ratio until a CI run pins it.
      const double maxYardsticks = full ? limits.fullYardsticks : limits.liteYardsticks;
      if (maxYardsticks > 0.0)
        CHECK(m.yardsticks < maxYardsticks);
#endif
    }
  }
}
} // namespace

TEST_CASE("PRE NAM + amp keeps well inside the realtime deadline" * doctest::skip(kSkipBudget))
{
  // Yardsticks on a Core Ultra 7 155H, idle or with every core busy: FULL 3.8-4.2,
  // LITE 1.1-1.25; with each block's cost doubled, FULL 7.4-8.4, LITE 2.2-2.5.
  const auto rigs = RigsRoot();
  CheckChain({FirstNam(rigs / "PrePedals", "FX-PettyJohn-Myth"), FirstNam(rigs / "Soldano SLO100", "AMP-")},
             {0.50, 6.0, 1.7}, "Myth -> Soldano");
}

TEST_CASE("The heaviest VoLum chain is not paying the realtime reserve" * doctest::skip(kSkipBudget))
{
  // Two PRE NAMs, main amp and Dual Amp SUPPORT all run on the audio thread.
  // Its absolute cost is the machine's (31-58% of the deadline here in FULL),
  // so only the Reset size is enforced.
  const auto rigs = RigsRoot();
  CheckChain({FirstNam(rigs / "PrePedals", "FX-PettyJohn-Myth"), FirstNam(rigs / "PrePedals", "FX-Minotaur-Klon"),
              FirstNam(rigs / "Soldano SLO100", "AMP-"), FirstNam(rigs / "Diezel Herbert Mk1", "AMP-")},
             Limits{}, "2 PRE + main + support");
}

TEST_CASE("The budget chain runs with denormals flushed and hands back the caller's FP mode")
{
  Chain chain;
  chain.Load({FirstNam(RigsRoot() / "Soldano SLO100", "AMP-")}, false);
  chain.Reset(64);
  std::vector<NAM_SAMPLE> src;
  double t = 0.0;
  FillChordBlock(src, 64, t);

  const unsigned int before = ReadFpMode();
  chain.ProcessTimed(src, 64);
  CHECK(ReadFpMode() == before);
#if defined(ARCH_X86)
  CHECK_MESSAGE((before & kFtzDaz) == 0u, "an earlier test case left FTZ/DAZ set");
  CHECK((chain.fpModeDuringProcess & kFtzDaz) == kFtzDaz);
#endif
}

TEST_CASE("Chunking an oversized host block is sample-identical to smaller host blocks")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = FirstNam(RigsRoot() / "Soldano SLO100", "AMP-");
  auto chunked = nam::get_dsp(path);
  auto reference = nam::get_dsp(path);
  REQUIRE(chunked != nullptr);
  REQUIRE(reference != nullptr);
  chunked->ResetAndPrewarm(kSampleRate, 64);
  reference->ResetAndPrewarm(kSampleRate, 64);

  std::vector<NAM_SAMPLE> in(512), outChunked(512, 0), outRef(512, 0);
  for (size_t i = 0; i < in.size(); ++i)
    in[i] = 0.2 * std::sin(2.0 * kPi * 110.0 * static_cast<double>(i) / kSampleRate);

  volum::dsp_staging::ProcessNamInChunks(512, 64, in.data(), outChunked.data(),
                                         [&](NAM_SAMPLE** ip, NAM_SAMPLE** op, int n) { chunked->process(ip, op, n); });
  for (int off = 0; off < 512; off += 64)
  {
    NAM_SAMPLE* ip[1] = {in.data() + off};
    NAM_SAMPLE* op[1] = {outRef.data() + off};
    reference->process(ip, op, 64);
  }
  for (size_t i = 0; i < in.size(); ++i)
    REQUIRE(outChunked[i] == outRef[i]);
}
