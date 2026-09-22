#include "third_party/doctest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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

// Realtime budget of the NAM chain VoLum actually runs: bundled captures,
// Reset exactly as the plugin resets them (NamResetBlockSize), processed at
// small host blocks. 1.3.0 shipped a crackle because every NAM was Reset at the
// 8192 scratch reserve: nothing failed, the audio just missed its deadline.
//
// Two guards. The ratio against a model Reset at exactly the block size is
// machine-independent and catches any sizing regression. The absolute share of
// the deadline is generous so a slow CI runner still passes, but a chain that
// cannot keep up on real hardware does not.

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
    for (auto& m : models)
      m->ResetAndPrewarm(kSampleRate, maxBlock);
  }

  // Series chain like PRE NAM -> amp (support lanes run on the same thread too,
  // so a series sum is the same per-block cost).
  std::vector<double> TimeBlocks(int block, int numBlocks)
  {
    std::vector<NAM_SAMPLE> a(static_cast<size_t>(block)), b(static_cast<size_t>(block));
    std::vector<double> us;
    us.reserve(static_cast<size_t>(numBlocks));
    double t = 0.0;
    for (int n = 0; n < numBlocks; ++n)
    {
      for (int i = 0; i < block; ++i, t += 1.0 / kSampleRate)
        a[static_cast<size_t>(i)] = 0.08
                                    * (std::sin(2.0 * kPi * 82.41 * t) + std::sin(2.0 * kPi * 123.47 * t)
                                       + std::sin(2.0 * kPi * 164.81 * t) + std::sin(2.0 * kPi * 207.65 * t));
      const auto t0 = std::chrono::steady_clock::now();
      NAM_SAMPLE* in = a.data();
      NAM_SAMPLE* out = b.data();
      for (auto& m : models)
      {
        NAM_SAMPLE* ip[1] = {in};
        NAM_SAMPLE* op[1] = {out};
        m->process(ip, op, block);
        std::swap(in, out);
      }
      const auto t1 = std::chrono::steady_clock::now();
      us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    return us;
  }
};

double Mean(const std::vector<double>& v)
{
  double s = 0.0;
  for (double x : v)
    s += x;
  return v.empty() ? 0.0 : s / static_cast<double>(v.size());
}

double Median(std::vector<double> v)
{
  std::sort(v.begin(), v.end());
  return v.empty() ? 0.0 : v[v.size() / 2];
}

struct Measured
{
  double productionMean = 0.0;
  double productionMedian = 0.0;
  double referenceMedian = 0.0;
};

// Alternating rounds so a burst of machine noise hits both sides.
Measured Measure(Chain& chain, int block)
{
  const int blocksPerRound = std::max(64, static_cast<int>(0.3 * kSampleRate / block));
  std::vector<double> production, reference;
  for (int round = 0; round < 3; ++round)
  {
    chain.Reset(volum::dsp_staging::NamResetBlockSize(block));
    chain.TimeBlocks(block, blocksPerRound / 4);
    const auto p = chain.TimeBlocks(block, blocksPerRound);
    production.insert(production.end(), p.begin(), p.end());

    chain.Reset(block);
    chain.TimeBlocks(block, blocksPerRound / 4);
    const auto r = chain.TimeBlocks(block, blocksPerRound);
    reference.insert(reference.end(), r.begin(), r.end());
  }
  return {Mean(production), Median(production), Median(reference)};
}

void CheckChain(const std::vector<std::filesystem::path>& paths, double maxMeanShare, const std::string& label)
{
  for (const bool full : {true, false})
  {
    Chain chain;
    chain.Load(paths, full);
    for (const int block : {64, 128})
    {
      const auto m = Measure(chain, block);
      const double deadlineUs = 1e6 * block / kSampleRate;
      const double share = m.productionMean / deadlineUs;
      const double ratio = m.productionMedian / std::max(1e-3, m.referenceMedian);
      INFO(label << (full ? " FULL" : " LITE") << " block " << block << ": mean " << m.productionMean << " us ("
                 << 100.0 * share << "% of deadline), median ratio vs block-sized reset " << ratio);
      CHECK(ratio <= 1.5);
      CHECK(share < maxMeanShare);
    }
  }
}
} // namespace

TEST_CASE("PRE NAM + amp keeps well inside the realtime deadline" * doctest::skip(kSkipBudget))
{
  const auto rigs = RigsRoot();
  CheckChain({FirstNam(rigs / "PrePedals", "FX-PettyJohn-Myth"), FirstNam(rigs / "Soldano SLO100", "AMP-")}, 0.50,
             "Myth -> Soldano");
}

TEST_CASE("The heaviest VoLum chain still fits the realtime deadline" * doctest::skip(kSkipBudget))
{
  // Two PRE NAMs, main amp and Dual Amp SUPPORT all run on the audio thread.
  const auto rigs = RigsRoot();
  CheckChain({FirstNam(rigs / "PrePedals", "FX-PettyJohn-Myth"), FirstNam(rigs / "PrePedals", "FX-Minotaur-Klon"),
              FirstNam(rigs / "Soldano SLO100", "AMP-"), FirstNam(rigs / "Diezel Herbert Mk1", "AMP-")},
             1.00, "2 PRE + main + support");
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
