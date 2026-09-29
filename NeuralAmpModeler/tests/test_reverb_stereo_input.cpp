// True-stereo reverb input.
//
// Dual Amp pans its two lanes hard left and right and, with polarity invert on,
// puts one of them in anti-phase. Every reverb mode used to sum its inputs to mono
// before the tank, so that pair cancelled on the way in: the tank heard only what
// differed between the two amps, and the owner heard a thin, dull reverb. Each side
// now excites its own half of the network (Hall/Oktaverb: its own diffuser into the
// lines its output taps read first; Plate: its own diffuser into one tank half).
//
// Two things are pinned here: anti-phase input carries as much reverb as in-phase
// input, and a centred source (L == R, i.e. every single-amp patch) comes out exactly
// as the mono-sum topology produced it.

#include "third_party/doctest.h"

#include "../../AudioDSPTools/dsp/Reverb.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <vector>

namespace
{
constexpr double kSR = 48000.0;
constexpr size_t kBlock = 256;
constexpr double kBurstSeconds = 0.25;
constexpr double kRenderSeconds = 2.5;
// The tail is measured once the burst has stopped, so nothing of the dry signal (which
// Oktaverb keeps even at full Mix) is in the number.
constexpr double kTailFromSeconds = 0.30;

struct ReverbCase
{
  const char* name;
  int mode;
  int subMode;
};

const ReverbCase kCases[] = {
  {"Hall", dsp::effect::Reverb::kModeHall, 0},
  {"Plate", dsp::effect::Reverb::kModePlate, 0},
  {"Oktaverb Halo", dsp::effect::Reverb::kModeOktaverb, 0},
  {"Oktaverb Shimmer", dsp::effect::Reverb::kModeOktaverb, 1},
  {"Oktaverb Bloom", dsp::effect::Reverb::kModeOktaverb, 2},
};

// Deterministic noise burst followed by silence: broadband, so no single delay-line
// resonance decides the result.
std::vector<double> Burst()
{
  const size_t frames = static_cast<size_t>(kSR * kRenderSeconds);
  const size_t burst = static_cast<size_t>(kSR * kBurstSeconds);
  std::vector<double> x(frames, 0.0);
  uint32_t seed = 0x2468ACEU;
  for (size_t i = 0; i < burst; ++i)
  {
    seed = seed * 1664525U + 1013904223U;
    x[i] = 0.3 * (static_cast<double>(seed >> 8) / static_cast<double>(1U << 24) * 2.0 - 1.0);
  }
  return x;
}

struct Rendered
{
  std::vector<double> l;
  std::vector<double> r;
};

Rendered Render(const ReverbCase& c, const std::vector<double>& inL, const std::vector<double>* inR)
{
  dsp::effect::Reverb reverb;
  const size_t channels = inR != nullptr ? 2 : 1;
  reverb.Prepare(channels, kBlock, kSR);
  reverb.SetParams(1.0, 3.0, 5.0, 10.0, 0.5, c.mode, kSR, c.subMode);
  Rendered out;
  out.l.reserve(inL.size());
  out.r.reserve(inL.size());
  std::vector<double> l(kBlock), r(kBlock);
  for (size_t start = 0; start < inL.size(); start += kBlock)
  {
    const size_t n = std::min(kBlock, inL.size() - start);
    for (size_t i = 0; i < n; ++i)
    {
      l[i] = inL[start + i];
      r[i] = inR != nullptr ? (*inR)[start + i] : 0.0;
    }
    double* io[2] = {l.data(), r.data()};
    auto** y = reverb.Process(io, channels, n);
    out.l.insert(out.l.end(), y[0], y[0] + n);
    if (channels > 1)
      out.r.insert(out.r.end(), y[1], y[1] + n);
  }
  return out;
}

double TailEnergy(const Rendered& out)
{
  const size_t from = static_cast<size_t>(kSR * kTailFromSeconds);
  double e = 0.0;
  for (size_t i = from; i < out.l.size(); ++i)
    e += out.l[i] * out.l[i] + out.r[i] * out.r[i];
  return e;
}

double RatioDb(double num, double den)
{
  return 10.0 * std::log10(std::max(num, 1e-30) / std::max(den, 1e-30));
}

std::vector<double> Scaled(const std::vector<double>& x, double g)
{
  std::vector<double> y(x.size());
  for (size_t i = 0; i < x.size(); ++i)
    y[i] = x[i] * g;
  return y;
}
} // namespace

// With the mono sum every mode read the 1e-30 floor here, -313 to -323 dB: L + R was
// exactly 0, so the tank was never excited. True stereo reads -0.03 to -0.12 dB across
// the five modes; 1 dB is headroom for another platform's rounding, nowhere near what
// any cancellation would leave.
TEST_CASE("Reverb stereo input: anti-phase L/R carries as much tail as in-phase, every mode")
{
  const auto x = Burst();
  const auto minusX = Scaled(x, -1.0);
  for (const auto& c : kCases)
  {
    const double inPhase = TailEnergy(Render(c, x, &x));
    const double antiPhase = TailEnergy(Render(c, x, &minusX));
    const double db = RatioDb(antiPhase, inPhase);
    INFO(std::string(c.name) << ": anti-phase vs in-phase tail " << db << " dB");
    REQUIRE(inPhase > 1e-3);
    CHECK(std::abs(db) < 1.0);
  }
}

// A hard-panned source is half the input power of the same source centred, so its
// tail should sit 3 dB under the centred one. Mono-summing halved its amplitude on the
// way in and put it at -5.9 to -6.0 dB, so a panned Dual Amp lane got less reverb than
// the same amp centred. True stereo reads -2.8 to -3.3 dB.
TEST_CASE("Reverb stereo input: a hard-panned source keeps its full level into the tank")
{
  const auto x = Burst();
  const std::vector<double> silence(x.size(), 0.0);
  for (const auto& c : kCases)
  {
    const double centred = TailEnergy(Render(c, x, &x));
    const double leftOnly = TailEnergy(Render(c, x, &silence));
    const double rightOnly = TailEnergy(Render(c, silence, &x));
    const double dbL = RatioDb(leftOnly, centred);
    const double dbR = RatioDb(rightOnly, centred);
    INFO(std::string(c.name) << ": L-only " << dbL << " dB, R-only " << dbR << " dB vs centred");
    CHECK(std::abs(dbL + 3.0) < 1.0);
    CHECK(std::abs(dbR + 3.0) < 1.0);
  }
}

namespace
{
struct Digest
{
  double energyL;
  double energyR;
  // Projection onto a fixed +/-1 sequence: any sample that moves by d moves this by d,
  // where an energy alone could stay put under a sign flip or a time shift.
  double projL;
  double projR;
};

Digest DigestOf(const Rendered& out)
{
  Digest d{0.0, 0.0, 0.0, 0.0};
  uint32_t seed = 0x13579BDU;
  for (size_t i = 0; i < out.l.size(); ++i)
  {
    seed = seed * 1664525U + 1013904223U;
    const double w = (seed & 0x80000000U) ? 1.0 : -1.0;
    d.energyL += out.l[i] * out.l[i];
    d.energyR += out.r[i] * out.r[i];
    d.projL += w * out.l[i];
    d.projR += w * out.r[i];
  }
  return d;
}

bool Near(double actual, double expected, double scale)
{
  return std::abs(actual - expected) <= 1e-9 * scale;
}
} // namespace

// L == R is every single-amp patch, so the stereo input must reproduce the mono-sum
// topology it replaced. The figures below were rendered by that topology (AudioDSPTools
// b78f811) before the change and are matched here to 1e-9 of the channel's energy
// scale: bit-exact on the platform that recorded them, while leaving room for another
// libm's last-bit sin/pow/tanh. It holds exactly because (x + x) * 0.5 == x in binary
// floating point, so each side's stage runs on precisely the sample the mono sum fed.
TEST_CASE("Reverb stereo input: L == R reproduces the pre-stereo mono-sum render, every mode")
{
  struct Pinned
  {
    const char* name;
    Digest d;
  };
  const Pinned pinned[] = {
    {"Hall", {71.421573720704373, 71.563078645746245, 4.2423848119324115, 17.007902199986582}},
    {"Plate", {171.28043762240617, 166.72493623338249, -1.1948077170238049, 21.937922401938572}},
    {"Oktaverb Halo", {285.08532048344443, 279.96782606412825, 10.833913809610989, 17.026594700743818}},
    {"Oktaverb Shimmer", {293.31095092102606, 287.25215202515125, 26.091001745017657, 19.217493076810069}},
    {"Oktaverb Bloom", {187.82141264344386, 187.7634380090785, 8.6058100939647755, 13.552830216246777}},
  };
  const auto x = Burst();
  for (size_t k = 0; k < std::size(kCases); ++k)
  {
    const Digest d = DigestOf(Render(kCases[k], x, &x));
    const Digest& e = pinned[k].d;
    char line[256];
    std::snprintf(line, sizeof(line), "{\"%s\", {%.17g, %.17g, %.17g, %.17g}},", kCases[k].name, d.energyL, d.energyR,
                  d.projL, d.projR);
    INFO(line);
    const double scaleL = std::sqrt(e.energyL * static_cast<double>(x.size()));
    const double scaleR = std::sqrt(e.energyR * static_cast<double>(x.size()));
    CHECK(Near(d.energyL, e.energyL, e.energyL));
    CHECK(Near(d.energyR, e.energyR, e.energyR));
    CHECK(Near(d.projL, e.projL, scaleL));
    CHECK(Near(d.projR, e.projR, scaleR));
  }
}
