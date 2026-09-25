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
// Two guards. The production chain must cost well under the same chain Reset
// at the 8192 reserve; that ratio is machine-independent (the regression made
// them equal). The absolute share of the deadline is generous so a slow CI
// runner still passes, but PRE NAM + amp that cannot keep up does not. Medians
// of block-by-block alternation, so one scheduler spike cannot fail a run.

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
  // so a series sum is the same per-block cost). Returns microseconds.
  double ProcessTimed(const std::vector<NAM_SAMPLE>& src, int block)
  {
    a.assign(src.begin(), src.begin() + block);
    b.resize(static_cast<size_t>(block));
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
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
  }

  std::vector<NAM_SAMPLE> a, b;
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

double Median(std::vector<double> v)
{
  std::sort(v.begin(), v.end());
  return v.empty() ? 0.0 : v[v.size() / 2];
}

struct Measured
{
  double productionMedian = 0.0;
  double oversizedMedian = 0.0;
};

// Two chains loaded once and never Reset between samples, alternated block by
// block on the same input so a burst of machine noise lands on both.
Measured Measure(Chain& production, Chain& oversized, int block)
{
  production.Reset(volum::dsp_staging::NamResetBlockSize(block));
  oversized.Reset(volum::dsp_staging::ReservedAudioBlockSize(block));
  const int numBlocks = std::max(256, static_cast<int>(0.6 * kSampleRate / block));
  std::vector<NAM_SAMPLE> src;
  double t = 0.0;
  for (int n = 0; n < numBlocks / 4; ++n)
  {
    FillChordBlock(src, block, t);
    production.ProcessTimed(src, block);
    oversized.ProcessTimed(src, block);
  }
  std::vector<double> p, o;
  p.reserve(static_cast<size_t>(numBlocks));
  o.reserve(static_cast<size_t>(numBlocks));
  for (int n = 0; n < numBlocks; ++n)
  {
    FillChordBlock(src, block, t);
    if (n % 2 == 0)
    {
      p.push_back(production.ProcessTimed(src, block));
      o.push_back(oversized.ProcessTimed(src, block));
    }
    else
    {
      o.push_back(oversized.ProcessTimed(src, block));
      p.push_back(production.ProcessTimed(src, block));
    }
  }
  return {Median(p), Median(o)};
}

// maxMedianShare <= 0 skips the absolute check (cost the hardware owns, not VoLum).
void CheckChain(const std::vector<std::filesystem::path>& paths, double maxMedianShare, const std::string& label)
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
      const double share = m.productionMedian / deadlineUs;
      const double ratio = m.productionMedian / std::max(1e-3, m.oversizedMedian);
      INFO(label << (full ? " FULL" : " LITE") << " block " << block << ": median " << m.productionMedian << " us ("
                 << 100.0 * share << "% of deadline), vs the 8192-reserve reset " << ratio);
      // Healthy runs measure 0.14-0.36 here; the 1.3.0 regression is 1.0.
      CHECK(ratio <= 0.6);
      if (maxMedianShare > 0.0)
        CHECK(share < maxMedianShare);
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

TEST_CASE("The heaviest VoLum chain is not paying the realtime reserve" * doctest::skip(kSkipBudget))
{
  // Two PRE NAMs, main amp and Dual Amp SUPPORT all run on the audio thread.
  // Its absolute cost is the machine's (31-58% of the deadline here in FULL),
  // so only the sizing ratio is enforced.
  const auto rigs = RigsRoot();
  CheckChain({FirstNam(rigs / "PrePedals", "FX-PettyJohn-Myth"), FirstNam(rigs / "PrePedals", "FX-Minotaur-Klon"),
              FirstNam(rigs / "Soldano SLO100", "AMP-"), FirstNam(rigs / "Diezel Herbert Mk1", "AMP-")},
             0.0, "2 PRE + main + support");
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
