#include "third_party/doctest.h"
#include "../VoLumTremolo.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

using volum::TremoloDSP;

namespace
{
constexpr double kSR = 48000.0;
constexpr int kBlock = 256;

// Stream one mono signal through the in-place tremolo in fixed blocks.
std::vector<double> runStream(TremoloDSP& trem, const std::vector<double>& in)
{
  std::vector<double> out = in;
  for (size_t start = 0; start < out.size(); start += kBlock)
  {
    const size_t n = std::min<size_t>(kBlock, out.size() - start);
    double* ptr[1] = {out.data() + start};
    trem.Process(ptr, 1, static_cast<int>(n));
  }
  return out;
}

std::vector<double> makeDC(size_t n, double v = 1.0)
{
  return std::vector<double>(n, v);
}

std::vector<double> makeSine(double freq, size_t n, double amp = 0.5)
{
  std::vector<double> v(n);
  for (size_t i = 0; i < n; ++i)
    v[i] = amp * std::sin(2.0 * M_PI * freq * static_cast<double>(i) / kSR);
  return v;
}
} // namespace

TEST_CASE("Tremolo passthrough at depth 0 (all modes)")
{
  for (int mode = 0; mode < TremoloDSP::kNumModes; ++mode)
  {
    TremoloDSP trem;
    trem.Prepare(kSR, kBlock, 1);
    trem.SetParams(5.0, 0.0 /*depth*/, 0.5, 1.0 /*mix*/, 800.0, mode, kSR);
    trem.Reset();
    auto in = makeSine(220.0, 4096);
    auto out = runStream(trem, in);
    double maxErr = 0.0;
    for (size_t i = 0; i < in.size(); ++i)
      maxErr = std::max(maxErr, std::abs(out[i] - in[i]));
    CHECK(maxErr < 1e-9);
  }
}

TEST_CASE("Tremolo passthrough at mix 0 (all modes)")
{
  for (int mode = 0; mode < TremoloDSP::kNumModes; ++mode)
  {
    TremoloDSP trem;
    trem.Prepare(kSR, kBlock, 1);
    trem.SetParams(5.0, 1.0 /*depth*/, 0.5, 0.0 /*mix*/, 800.0, mode, kSR);
    trem.Reset();
    auto in = makeSine(220.0, 4096);
    auto out = runStream(trem, in);
    double maxErr = 0.0;
    for (size_t i = 0; i < in.size(); ++i)
      maxErr = std::max(maxErr, std::abs(out[i] - in[i]));
    CHECK(maxErr < 1e-9);
  }
}

TEST_CASE("Tremolo finite and bounded at full depth (all modes/shapes)")
{
  for (int mode = 0; mode < TremoloDSP::kNumModes; ++mode)
  {
    for (double shape : {0.0, 0.5, 1.0})
    {
      TremoloDSP trem;
      trem.Prepare(kSR, kBlock, 1);
      trem.SetParams(6.0, 1.0, shape, 1.0, 800.0, mode, kSR);
      trem.Reset();
      auto in = makeSine(180.0, 1 << 15, 0.9);
      auto out = runStream(trem, in);
      double peak = 0.0;
      for (double v : out)
      {
        CHECK(std::isfinite(v));
        peak = std::max(peak, std::abs(v));
      }
      CHECK(peak <= 1.0 + 1e-6); // amplitude modulation never boosts above input
    }
  }
}

TEST_CASE("Tremolo depth controls trough level (Bias, DC input)")
{
  auto troughPeakFor = [](double depth) {
    TremoloDSP trem;
    trem.Prepare(kSR, kBlock, 1);
    trem.SetParams(5.0, depth, 0.0 /*pure sine*/, 1.0, 800.0, TremoloDSP::kBias, kSR);
    trem.Reset();
    auto out = runStream(trem, makeDC(1 << 14, 1.0));
    // Skip the first cycle so smoothing settles.
    double lo = 1e9, hi = -1e9;
    for (size_t i = 4096; i < out.size(); ++i)
    {
      lo = std::min(lo, out[i]);
      hi = std::max(hi, out[i]);
    }
    return std::pair<double, double>(lo, hi);
  };

  auto shallow = troughPeakFor(0.5);
  auto deep = troughPeakFor(1.0);

  CHECK(shallow.second == doctest::Approx(1.0).epsilon(0.02)); // peak ~ unity
  CHECK(shallow.first == doctest::Approx(0.5).epsilon(0.05)); // trough ~ 1 - depth
  CHECK(deep.first == doctest::Approx(0.0).epsilon(0.02)); // full depth dips to silence
  CHECK(deep.first < shallow.first); // deeper => lower trough
}

TEST_CASE("Tremolo LFO rate matches requested Hz (Bias, DC input)")
{
  const double rateHz = 5.0;
  TremoloDSP trem;
  trem.Prepare(kSR, kBlock, 1);
  trem.SetParams(rateHz, 1.0, 0.0, 1.0, 800.0, TremoloDSP::kBias, kSR);
  trem.Reset();
  auto out = runStream(trem, makeDC(static_cast<size_t>(kSR) * 3, 1.0));

  // Collect upward mean-crossings, then derive the rate from the time spanned by
  // an integer number of cycles (robust to partial cycles at the buffer edges).
  const double mean = 0.5; // gain swings 0..1 about 0.5
  std::vector<size_t> crossings;
  for (size_t i = 1; i < out.size(); ++i)
    if (out[i - 1] < mean && out[i] >= mean)
      crossings.push_back(i);
  REQUIRE(crossings.size() >= 3);
  const double cycles = static_cast<double>(crossings.size() - 1);
  const double spanSec = static_cast<double>(crossings.back() - crossings.front()) / kSR;
  const double measuredHz = cycles / spanSec;
  CHECK(measuredHz == doctest::Approx(rateHz).epsilon(0.02));
}

TEST_CASE("Tremolo Harmonic differs from Bias (phasey, not plain AM)")
{
  auto runMode = [](int mode) {
    TremoloDSP trem;
    trem.Prepare(kSR, kBlock, 1);
    trem.SetParams(4.0, 0.9, 0.0, 1.0, 700.0, mode, kSR);
    trem.Reset();
    // Two-band content so the crossover has something to separate.
    std::vector<double> in(1 << 15);
    for (size_t i = 0; i < in.size(); ++i)
    {
      const double t = static_cast<double>(i) / kSR;
      in[i] = 0.4 * std::sin(2.0 * M_PI * 120.0 * t) + 0.4 * std::sin(2.0 * M_PI * 3000.0 * t);
    }
    return runStream(trem, in);
  };

  auto bias = runMode(TremoloDSP::kBias);
  auto harmonic = runMode(TremoloDSP::kHarmonic);

  double diff = 0.0;
  for (size_t i = 4096; i < bias.size(); ++i)
    diff = std::max(diff, std::abs(bias[i] - harmonic[i]));
  CHECK(diff > 0.01); // the two algorithms produce audibly different output

  for (double v : harmonic)
    CHECK(std::isfinite(v));
}

TEST_CASE("Tremolo Optical differs audibly from Bias at default sine settings")
{
  // The two share the same sine LFO target; Optical adds a photocell-style
  // asymmetric envelope. At the DEFAULT (sine shape, 5 Hz) that asymmetry must
  // still reshape the throb noticeably, otherwise switching Optical<->Bias is
  // inaudible (the reported "can't hear the mode" bug).
  auto runMode = [](int mode) {
    TremoloDSP trem;
    trem.Prepare(kSR, kBlock, 1);
    trem.SetParams(5.0, 0.85, 0.0 /*pure sine*/, 1.0, 800.0, mode, kSR);
    trem.Reset();
    return runStream(trem, makeDC(1 << 15, 1.0));
  };
  auto bias = runMode(TremoloDSP::kBias);
  auto opt = runMode(TremoloDSP::kOptical);
  double diff = 0.0;
  for (size_t i = 8192; i < bias.size(); ++i)
    diff = std::max(diff, std::abs(bias[i] - opt[i]));
  CHECK(diff > 0.08); // gain envelopes must visibly diverge
}

TEST_CASE("Tremolo stays finite across a live mode switch")
{
  TremoloDSP trem;
  trem.Prepare(kSR, kBlock, 1);
  trem.Reset();
  auto in = makeSine(200.0, 1 << 14, 0.8);
  std::vector<double> out = in;
  int mode = 0;
  for (size_t start = 0; start < out.size(); start += kBlock)
  {
    const size_t n = std::min<size_t>(kBlock, out.size() - start);
    trem.SetParams(6.0, 0.9, 0.3, 1.0, 800.0, mode % TremoloDSP::kNumModes, kSR);
    mode++; // switch mode every block
    double* ptr[1] = {out.data() + start};
    trem.Process(ptr, 1, static_cast<int>(n));
  }
  for (double v : out)
  {
    CHECK(std::isfinite(v));
    CHECK(std::abs(v) <= 1.0 + 1e-6);
  }
}

TEST_CASE("Tremolo shape morph is gentle (near sine through the first quarter)")
{
  // Drive the modulation gain via a DC carrier so out[] traces the LFO gain
  // directly. The shape knob must stay close to a pure sine early in its travel
  // and only depart strongly near 1.0 (the "squares too early" fix).
  auto gainFor = [](double shape) {
    TremoloDSP trem;
    trem.Prepare(kSR, kBlock, 1);
    trem.SetParams(5.0, 1.0, shape, 1.0, 800.0, TremoloDSP::kBias, kSR);
    trem.Reset();
    return runStream(trem, makeDC(1 << 14, 1.0));
  };
  auto maxDiff = [](const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0.0;
    for (size_t i = 4096; i < a.size(); ++i)
      d = std::max(d, std::abs(a[i] - b[i]));
    return d;
  };
  auto sine = gainFor(0.0);
  const double dQuarter = maxDiff(sine, gainFor(0.25));
  const double dFull = maxDiff(sine, gainFor(1.0));
  CHECK(dQuarter < 0.10); // first quarter stays close to a sine
  CHECK(dFull > 0.20); // full shape clearly departs toward square
  CHECK(dQuarter < dFull); // monotonic: more knob => more square
}

TEST_CASE("Tremolo depth knob maps onto an audible floor")
{
  using volum::VoLumTremoloDepthKnobToInternal;
  CHECK(VoLumTremoloDepthKnobToInternal(0.0) == doctest::Approx(0.40)); // min knob still throbs
  CHECK(VoLumTremoloDepthKnobToInternal(1.0) == doctest::Approx(1.0)); // max = full chop
  CHECK(VoLumTremoloDepthKnobToInternal(0.5) == doctest::Approx(0.70));
  CHECK(VoLumTremoloDepthKnobToInternal(-1.0) == doctest::Approx(0.40)); // clamps
  CHECK(VoLumTremoloDepthKnobToInternal(2.0) == doctest::Approx(1.0)); // clamps
}

TEST_CASE("Tremolo sync division to ms mapping (delay reuse)")
{
  // ms = period of the division = 1000 / Hz. 120 BPM: 1/4 = 500 ms, 1/8 = 250 ms.
  CHECK(volum::VoLumTremoloSyncMs(120.0, 1) == doctest::Approx(500.0)); // 1/4
  CHECK(volum::VoLumTremoloSyncMs(120.0, 4) == doctest::Approx(250.0)); // 1/8
  CHECK(volum::VoLumTremoloSyncMs(120.0, 0) == doctest::Approx(1000.0)); // 1/2
  CHECK(volum::VoLumTremoloSyncMs(120.0, 7) == doctest::Approx(125.0)); // 1/16
  CHECK(volum::VoLumTremoloSyncMs(60.0, 1) == doctest::Approx(1000.0)); // tempo scales
}

TEST_CASE("Tremolo sync clamps out-of-range division and degenerate BPM (no OOB)")
{
  using volum::kVoLumTremoloDivisionDefault;
  using volum::VoLumTremoloSyncMs;
  using volum::VoLumTremoloSyncRateHz;
  // A corrupt/old project or hostile chunk can hand a division outside [0,8) or a
  // zero/negative BPM. Both helpers must clamp (default division, BPM->120) and
  // never index kQuarterMultiplier[] out of bounds.
  const double defMs120 = VoLumTremoloSyncMs(120.0, kVoLumTremoloDivisionDefault);
  const double defHz120 = VoLumTremoloSyncRateHz(120.0, kVoLumTremoloDivisionDefault);
  for (int bad : {-1, -1000, 8, 99, 100000})
  {
    CHECK(VoLumTremoloSyncMs(120.0, bad) == doctest::Approx(defMs120));
    CHECK(VoLumTremoloSyncRateHz(120.0, bad) == doctest::Approx(defHz120));
  }
  // Degenerate BPM heals to 120 BPM.
  CHECK(VoLumTremoloSyncRateHz(0.0, 1) == doctest::Approx(VoLumTremoloSyncRateHz(120.0, 1)));
  CHECK(VoLumTremoloSyncRateHz(-50.0, 4) == doctest::Approx(VoLumTremoloSyncRateHz(120.0, 4)));
  CHECK(VoLumTremoloSyncMs(0.0, 4) == doctest::Approx(VoLumTremoloSyncMs(120.0, 4)));
  // The synced result stays inside the delay's process-time clamp window.
  for (int div = 0; div < volum::kVoLumTremoloDivisionCount; ++div)
  {
    const double ms = VoLumTremoloSyncMs(40.0, div); // slowest sane tempo
    CHECK(std::isfinite(ms));
    CHECK(ms > 0.0);
  }
}

TEST_CASE("Tremolo division name is defined for every division and defaults safely")
{
  for (int div = 0; div < volum::kVoLumTremoloDivisionCount; ++div)
  {
    const char* name = volum::VoLumTremoloDivisionName(div);
    REQUIRE(name != nullptr);
    CHECK(std::string(name).size() > 0);
  }
  CHECK(std::string(volum::VoLumTremoloDivisionName(-1)) == "1/8");
  CHECK(std::string(volum::VoLumTremoloDivisionName(999)) == "1/8");
}

TEST_CASE("Tremolo sync division to Hz mapping")
{
  // 120 BPM -> quarter note = 2 Hz.
  CHECK(volum::VoLumTremoloSyncRateHz(120.0, 1) == doctest::Approx(2.0)); // 1/4
  CHECK(volum::VoLumTremoloSyncRateHz(120.0, 0) == doctest::Approx(1.0)); // 1/2
  CHECK(volum::VoLumTremoloSyncRateHz(120.0, 4) == doctest::Approx(4.0)); // 1/8
  CHECK(volum::VoLumTremoloSyncRateHz(120.0, 7) == doctest::Approx(8.0)); // 1/16
  CHECK(volum::VoLumTremoloSyncRateHz(120.0, 6) == doctest::Approx(6.0)); // 1/8T
  CHECK(volum::VoLumTremoloSyncRateHz(120.0, 2) == doctest::Approx(4.0 / 3.0)); // 1/4.
  // Scales linearly with tempo.
  CHECK(volum::VoLumTremoloSyncRateHz(60.0, 1) == doctest::Approx(1.0));
  CHECK(volum::VoLumTremoloSyncRateHz(240.0, 1) == doctest::Approx(4.0));
  // Out-of-range division falls back to default (1/8 -> 4 Hz at 120).
  CHECK(volum::VoLumTremoloSyncRateHz(120.0, 99) == doctest::Approx(4.0));
}
