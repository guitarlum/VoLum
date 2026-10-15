#include "third_party/doctest.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "activations.h"
#include "dsp.h"
#include "get_dsp.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumResamplingNam.h"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <psapi.h>
#endif

// ResamplingNAM allocates its Lanczos pair only while the host rate differs
// from the model's. These tests pin that and lock the output after rate
// changes to a wrapper built fresh at each rate, which is what the container
// Reset on every rate change produced.

namespace
{
constexpr double kPi = 3.14159265358979323846;

std::filesystem::path SoldanoAmp()
{
  const auto dir =
    std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "rigs" / "Soldano SLO100";
  std::vector<std::filesystem::path> hits;
  for (const auto& e : std::filesystem::directory_iterator(dir))
  {
    const std::string name = e.path().filename().string();
    if (e.path().extension() == ".nam" && name.rfind("AMP-", 0) == 0)
      hits.push_back(e.path());
  }
  std::sort(hits.begin(), hits.end());
  const std::string missing = dir.string() + " has no AMP-*.nam";
  REQUIRE_MESSAGE(!hits.empty(), missing);
  return hits.front();
}

std::unique_ptr<ResamplingNAM> Wrap(double hostRate)
{
  nam::activations::Activation::enable_fast_tanh();
  auto model = nam::get_dsp(SoldanoAmp());
  REQUIRE(model != nullptr);
  auto w = std::make_unique<ResamplingNAM>(std::move(model), hostRate);
  w->SetSlimmableSize(1.0);
  return w;
}

std::vector<NAM_SAMPLE> Guitar(double rate)
{
  const int frames = static_cast<int>(0.25 * rate);
  std::vector<NAM_SAMPLE> v(static_cast<size_t>(frames));
  for (int i = 0; i < frames; ++i)
  {
    const double t = static_cast<double>(i) / rate;
    const double s = std::sin(2.0 * kPi * 110.0 * t) + 0.5 * std::sin(2.0 * kPi * 220.0 * t + 0.3)
                     + 0.25 * std::sin(2.0 * kPi * 3300.0 * t + 1.1);
    v[static_cast<size_t>(i)] = static_cast<NAM_SAMPLE>(0.3 * std::exp(-3.0 * t) * s);
  }
  return v;
}

std::vector<NAM_SAMPLE> Render(ResamplingNAM& w, double rate, int block)
{
  std::vector<NAM_SAMPLE> in = Guitar(rate);
  std::vector<NAM_SAMPLE> out(in.size(), 0);
  const int total = static_cast<int>(in.size());
  for (int off = 0; off < total; off += block)
    w.process(in.data() + off, out.data() + off, std::min(block, total - off));
  return out;
}

bool SameBits(const std::vector<NAM_SAMPLE>& a, const std::vector<NAM_SAMPLE>& b)
{
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(NAM_SAMPLE)) == 0;
}

struct Step
{
  double rate;
  int block;
};
} // namespace

TEST_CASE("ResamplingNAM: no Lanczos resampler at the model's own rate")
{
  auto w = Wrap(48000.0);
  const double modelRate = w->GetEncapsulatedSampleRate();
  REQUIRE(modelRate == 48000.0);

  w->Reset(modelRate, 64);
  CHECK_FALSE(w->HasResampler());
  CHECK(w->GetLatency() == 0);

  w->Reset(44100.0, 64);
  CHECK(w->HasResampler());
  CHECK(w->GetLatency() > 0);

  w->Reset(modelRate, 64);
  CHECK_FALSE(w->HasResampler());
  CHECK(w->GetLatency() == 0);

#ifdef _WIN32
  // Private bytes a Reset at the model's rate adds for four wrappers (info only).
  std::vector<std::unique_ptr<ResamplingNAM>> four;
  for (int i = 0; i < 4; ++i)
    four.push_back(Wrap(48000.0));
  PROCESS_MEMORY_COUNTERS_EX before{};
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before), sizeof(before));
  for (auto& m : four)
    m->Reset(modelRate, 64);
  PROCESS_MEMORY_COUNTERS_EX after{};
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after), sizeof(after));
  const double mib = (static_cast<double>(after.PrivateUsage) - static_cast<double>(before.PrivateUsage)) / 1048576.0;
  MESSAGE("INFO: 4 wrappers Reset at 48 kHz add " << mib << " MiB private bytes");
#endif
}

TEST_CASE("ResamplingNAM: output after rate changes matches a wrapper built at that rate")
{
  // 44.1 -> 48 -> 44.1 and a block change must rebuild the resampler, not
  // resume the one from before the 48 kHz Reset.
  const Step steps[] = {{48000.0, 64}, {44100.0, 64},  {48000.0, 64}, {96000.0, 64},
                        {44100.0, 64}, {48000.0, 128}, {44100.0, 64}, {88200.0, 256}};
  auto w = Wrap(48000.0);
  for (const Step& s : steps)
  {
    CAPTURE(s.rate);
    CAPTURE(s.block);
    w->Reset(s.rate, s.block);
    const auto got = Render(*w, s.rate, s.block);

    auto fresh = Wrap(s.rate);
    fresh->Reset(s.rate, s.block);
    CHECK(w->GetLatency() == fresh->GetLatency());
    CHECK(SameBits(got, Render(*fresh, s.rate, s.block)));
  }

  // A second Reset at the same resampling rate and block keeps the container
  // and clears it, as before.
  w->Reset(44100.0, 64);
  const auto first = Render(*w, 44100.0, 64);
  w->Reset(44100.0, 64);
  const auto again = Render(*w, 44100.0, 64);

  auto ref = Wrap(44100.0);
  ref->Reset(44100.0, 64);
  CHECK(SameBits(first, Render(*ref, 44100.0, 64)));
  ref->Reset(44100.0, 64);
  CHECK(SameBits(again, Render(*ref, 44100.0, 64)));
}
