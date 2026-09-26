#include "third_party/doctest.h"
#include "golden_helpers.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
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

// Exact-output lock on the NAM models VoLum ships. Speed-ups inside
// NeuralAmpModelerCore must leave every output bit alone, so each case renders
// a bundled capture the way ProcessBlock does (48 kHz, FTZ/DAZ on, production
// Reset size, oversized blocks chunked) and compares a SHA-256 of the samples
// with the value captured before the change.

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

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

// Plucked notes with harmonics and a decay, both polarities and several
// levels, then exact silence for the model's tail.
std::vector<NAM_SAMPLE> GuitarLikeSignal(double seconds)
{
  const int frames = static_cast<int>(seconds * kSampleRate);
  const int silentFrom = frames - frames / 4;
  const double notes[] = {82.41, 110.0, 146.83, 196.0, 246.94, 329.63};
  const int noteLength = std::max(1, silentFrom / 6);
  std::vector<NAM_SAMPLE> out(static_cast<size_t>(frames), 0);
  for (int i = 0; i < silentFrom; ++i)
  {
    const int note = std::min(5, i / noteLength);
    const double t = static_cast<double>(i - note * noteLength) / kSampleRate;
    const double f = notes[note];
    const double level = 0.05 + 0.1 * note;
    const double env = std::exp(-6.0 * t);
    const double s =
      std::sin(2.0 * kPi * f * t) + 0.5 * std::sin(4.0 * kPi * f * t + 0.3) + 0.25 * std::sin(6.0 * kPi * f * t + 1.1);
    out[static_cast<size_t>(i)] = static_cast<NAM_SAMPLE>(level * env * s);
  }
  return out;
}

struct RenderStats
{
  bool fpModeRestored = true;
  bool denormalsOffDuringProcess = true;
};

// Renders `input` through one model in host blocks of `block` frames, Reset and
// chunked the way the plugin does it.
std::vector<NAM_SAMPLE> RenderNam(const std::filesystem::path& path, bool full, int block,
                                  const std::vector<NAM_SAMPLE>& input, RenderStats* stats = nullptr)
{
  nam::activations::Activation::enable_fast_tanh();
  auto model = nam::get_dsp(path);
  REQUIRE_MESSAGE(model != nullptr, path.string());
  if (auto* slim = dynamic_cast<nam::SlimmableModel*>(model.get()))
    slim->SetSlimmableSize(full ? 1.0 : 0.0);
  const int reserve = volum::dsp_staging::NamResetBlockSize(block);
  model->ResetAndPrewarm(kSampleRate, reserve);

  std::vector<NAM_SAMPLE> in(input);
  std::vector<NAM_SAMPLE> out(input.size(), 0);
  const int total = static_cast<int>(input.size());
  for (int off = 0; off < total; off += block)
  {
    const int n = std::min(block, total - off);
    const unsigned int before = ReadFpMode();
    {
      const ScopedDenormalsOff denormalsOff;
      volum::dsp_staging::ProcessNamInChunks(
        n, reserve, in.data() + off, out.data() + off, [&](NAM_SAMPLE** ip, NAM_SAMPLE** op, int frames) {
#if defined(ARCH_X86)
          if (stats && (ReadFpMode() & kFtzDaz) != kFtzDaz)
            stats->denormalsOffDuringProcess = false;
#endif
          model->process(ip, op, frames);
        });
    }
    if (stats && ReadFpMode() != before)
      stats->fpModeRestored = false;
  }
  return out;
}

struct LockCase
{
  const char* name;
  const char* folder;
  const char* prefix;
  bool full;
  int block;
};

std::vector<LockCase> LockCases()
{
  return {
    {"Soldano amp FULL 48k/64", "Soldano SLO100", "AMP-", true, 64},
    {"Soldano amp LITE 48k/64", "Soldano SLO100", "AMP-", false, 64},
    {"Myth PRE FULL 48k/64", "PrePedals", "FX-PettyJohn-Myth", true, 64},
    {"Myth PRE LITE 48k/64", "PrePedals", "FX-PettyJohn-Myth", false, 64},
  };
}

// Captured on Windows x64 (MSVC, SSE2, /fp:precise) from NeuralAmpModelerCore
// 27027cc5, before the perf-safe-wins changes. macOS contracts multiply-adds
// into FMAs, so it prints its value until one is pinned from a CI run.
const char* PinnedHash(const std::string& name)
{
  struct Pin
  {
    const char* name;
    const char* hash;
  };
  static const Pin pins[] = {
    {"Soldano amp FULL 48k/64", "1b535bd0820398f1c92986cd7ac7ee75382dbc5c5209a622cf2ca5980babad74"},
    {"Soldano amp LITE 48k/64", "1a732ea1bf225fb7d481007e935c59778cfddf4334fc4c9e218676c5f72fe980"},
    {"Myth PRE FULL 48k/64", "40f0fdf7041d62136c2302f1a18812288ae8d93600bdc3d673b8c413279114c0"},
    {"Myth PRE LITE 48k/64", "8e438ba76ecd28fa009078396f8393bd279623e6520cbb25ea043c99d8ecf5c0"},
  };
  for (const Pin& p : pins)
    if (name == p.name)
      return p.hash;
  return "";
}

void ExpectNamHash(const std::string& name, const std::string& actual)
{
#if defined(_WIN32)
  INFO(name << " actual=" << actual);
  CHECK(actual == std::string(PinnedHash(name)));
#else
  MESSAGE(name << " hash (not pinned on this platform): " << actual);
#endif
}
} // namespace

TEST_CASE("NAM exact: bundled captures render the pinned bits")
{
  const auto rigs = RigsRoot();
  const std::vector<NAM_SAMPLE> input = GuitarLikeSignal(1.0);
  for (const LockCase& c : LockCases())
  {
    RenderStats stats;
    const std::vector<NAM_SAMPLE> out = RenderNam(FirstNam(rigs / c.folder, c.prefix), c.full, c.block, input, &stats);
    INFO(std::string(c.name));
    CHECK(stats.fpModeRestored);
    CHECK(stats.denormalsOffDuringProcess);
    CHECK(std::all_of(out.begin(), out.end(), [](NAM_SAMPLE v) { return std::isfinite(v); }));
    CHECK(std::any_of(out.begin(), out.end(), [](NAM_SAMPLE v) { return v != 0; }));
    ExpectNamHash(c.name, volum::test::Sha256HexSamples(out));
  }
}
