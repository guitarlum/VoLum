#include "third_party/doctest.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumChorus.h"
#include "../VoLumUserSettingsIO.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

using volum::ChorusDSP;

namespace
{
constexpr double kSR = 48000.0;
constexpr int kBlock = 256;

struct Stereo
{
  std::vector<double> l;
  std::vector<double> r;
};

Stereo makeSine(double freq, size_t n, double amp = 0.5)
{
  Stereo s;
  s.l.resize(n);
  s.r.resize(n);
  for (size_t i = 0; i < n; ++i)
  {
    const double v = amp * std::sin(2.0 * M_PI * freq * static_cast<double>(i) / kSR);
    s.l[i] = v;
    s.r[i] = v;
  }
  return s;
}

// Broadband-ish content so every tap has something to smear.
Stereo makeChord(size_t n, double amp = 0.3)
{
  Stereo s;
  s.l.resize(n);
  s.r.resize(n);
  for (size_t i = 0; i < n; ++i)
  {
    const double t = static_cast<double>(i) / kSR;
    const double v =
      amp
      * (std::sin(2.0 * M_PI * 110.0 * t) + std::sin(2.0 * M_PI * 277.0 * t) + std::sin(2.0 * M_PI * 1320.0 * t) / 3.0);
    s.l[i] = v;
    s.r[i] = v;
  }
  return s;
}

Stereo runStream(ChorusDSP& chorus, const Stereo& in, int chans = 2, int block = kBlock)
{
  Stereo out = in;
  for (size_t start = 0; start < out.l.size(); start += static_cast<size_t>(block))
  {
    const size_t n = std::min<size_t>(static_cast<size_t>(block), out.l.size() - start);
    double* ptr[2] = {out.l.data() + start, out.r.data() + start};
    chorus.Process(ptr, chans, static_cast<int>(n));
  }
  return out;
}

Stereo runMode(int mode, const Stereo& in, double rate = 0.35, double depth = 0.6, double tone = 0.5,
               double width = 0.7, double mix = 1.0)
{
  ChorusDSP chorus;
  chorus.Prepare(kSR, kBlock, 2);
  chorus.SetParams(rate, depth, tone, width, mix, mode, kSR);
  chorus.Reset();
  return runStream(chorus, in);
}

double maxDiff(const std::vector<double>& a, const std::vector<double>& b, size_t skip = 8192)
{
  double d = 0.0;
  for (size_t i = skip; i < a.size(); ++i)
    d = std::max(d, std::abs(a[i] - b[i]));
  return d;
}

double peakOf(const std::vector<double>& v)
{
  double p = 0.0;
  for (double s : v)
    p = std::max(p, std::abs(s));
  return p;
}

// ---------------------------------------------------------------- strength probes

const size_t kProbeLen = static_cast<size_t>(8.0 * kSR);
const size_t kProbeSkip = static_cast<size_t>(1.0 * kSR);

// The shipped row for `mode`, optionally with MIX / RATE overridden.
Stereo runRow(int mode, const Stereo& in, int chans = 2, double mix = 1.0, double rate = -1.0)
{
  const auto row = volum::kVoLumChorusModeDefaults[mode];
  ChorusDSP chorus;
  chorus.Prepare(kSR, kBlock, 2);
  chorus.SetParams(rate >= 0.0 ? rate : row.rate, row.depth, row.tone, row.width, mix, mode, kSR);
  chorus.Reset();
  return runStream(chorus, in, chans);
}

// White noise through an 80 Hz high-pass and 5 kHz low-pass: a cab-shaped
// broadband signal, deterministic.
Stereo makeCabNoise(size_t n, double amp = 0.2)
{
  struct Biquad
  {
    double b0, b1, b2, a1, a2, z1 = 0.0, z2 = 0.0;
    double tick(double x)
    {
      const double y = b0 * x + z1;
      z1 = b1 * x - a1 * y + z2;
      z2 = b2 * x - a2 * y;
      return y;
    }
  };
  auto butter = [](double fc, bool high) {
    const double w0 = 2.0 * M_PI * fc / kSR, cw = std::cos(w0), al = std::sin(w0) / std::sqrt(2.0), a0 = 1.0 + al;
    const double b0 = (high ? 0.5 * (1.0 + cw) : 0.5 * (1.0 - cw)) / a0;
    const double b1 = (high ? -(1.0 + cw) : (1.0 - cw)) / a0;
    return Biquad{b0, b1, b0, -2.0 * cw / a0, (1.0 - al) / a0};
  };
  Biquad hp = butter(80.0, true), lp = butter(5000.0, false);
  Stereo s;
  s.l.resize(n);
  uint32_t state = 22222u;
  for (size_t i = 0; i < n; ++i)
  {
    state = state * 1664525u + 1013904223u;
    const double w = (static_cast<double>(state >> 8) / 16777216.0) * 2.0 - 1.0;
    s.l[i] = amp * lp.tick(hp.tick(w));
  }
  s.r = s.l;
  return s;
}

// 13 equal sines across 2..5 kHz, the band where chorus shimmer lives.
Stereo makeShimmerBand(size_t n)
{
  Stereo s;
  s.l.assign(n, 0.0);
  uint32_t state = 777u;
  for (double f = 2000.0; f <= 5000.0 + 1e-9; f += 250.0)
  {
    state = state * 1664525u + 1013904223u;
    const double ph = 2.0 * M_PI * static_cast<double>(state >> 8) / 16777216.0;
    for (size_t i = 0; i < n; ++i)
      s.l[i] += 0.05 * std::sin(2.0 * M_PI * f * static_cast<double>(i) / kSR + ph);
  }
  s.r = s.l;
  return s;
}

double meanPower(const std::vector<double>& v, size_t skip = kProbeSkip)
{
  double sum = 0.0;
  for (size_t i = skip; i < v.size(); ++i)
    sum += v[i] * v[i];
  return sum / static_cast<double>(v.size() - skip);
}

double dB(double powerRatio)
{
  return 10.0 * std::log10(powerRatio + 1e-30);
}

double correlation(const std::vector<double>& a, const std::vector<double>& b, size_t skip = kProbeSkip)
{
  double ab = 0.0, aa = 0.0, bb = 0.0;
  for (size_t i = skip; i < a.size(); ++i)
  {
    ab += a[i] * b[i];
    aa += a[i] * a[i];
    bb += b[i] * b[i];
  }
  return ab / std::sqrt(aa * bb + 1e-30);
}

// Peak detune of a processed sine, in cents. Per-cycle frequency from
// interpolated positive zero crossings, averaged over 3 cycles; cycles below
// half the median amplitude are dropped (a summed wet dipping through a null is
// not pitch). Returns the 95th percentile of |cents| against f0.
double peakCents(const std::vector<double>& x, double f0, size_t skip = kProbeSkip)
{
  std::vector<double> crossings, amps;
  double peak = 0.0;
  for (size_t i = skip + 1; i < x.size(); ++i)
  {
    peak = std::max(peak, std::abs(x[i]));
    if (x[i - 1] < 0.0 && x[i] >= 0.0)
    {
      crossings.push_back(static_cast<double>(i - 1) + x[i - 1] / (x[i - 1] - x[i]));
      amps.push_back(peak);
      peak = 0.0;
    }
  }
  REQUIRE(crossings.size() > 16);
  std::vector<double> sorted(amps.begin() + 1, amps.end());
  std::sort(sorted.begin(), sorted.end());
  const double gate = 0.5 * sorted[sorted.size() / 2];
  std::vector<double> cents;
  for (size_t k = 1; k + 3 < crossings.size(); ++k)
  {
    if (amps[k] < gate || amps[k + 1] < gate || amps[k + 2] < gate || amps[k + 3] < gate)
      continue;
    const double f = 3.0 * kSR / (crossings[k + 3] - crossings[k]);
    cents.push_back(std::abs(1200.0 * std::log2(f / f0)));
  }
  REQUIRE(!cents.empty());
  std::sort(cents.begin(), cents.end());
  return cents[static_cast<size_t>(0.95 * static_cast<double>(cents.size() - 1))];
}

struct StrengthTargets
{
  double minCents440; // peak detune of the left wet on a 440 Hz sine
  double minShimmerDb; // wet 2..5 kHz level against dry
  double maxCorrelation; // L/R correlation of the full default row (shipped MIX)
};

// Shared per-mode strength contract at the shipped default row: audible detune,
// a wet that keeps the 2..5 kHz band, a wide image at the shipped MIX (the dry
// sits in the centre, so this is what the listener hears), and a 1-1 track that
// hears the full wet level.
void checkStrength(int mode, const StrengthTargets& t)
{
  INFO("mode " << std::string(volum::VoLumChorusModeName(mode)));
  const auto sine = runRow(mode, makeSine(440.0, kProbeLen));
  CHECK(peakCents(sine.l, 440.0) >= t.minCents440);

  const auto band = makeShimmerBand(kProbeLen);
  const auto bandWet = runRow(mode, band);
  CHECK(dB(meanPower(bandWet.l) / meanPower(band.l)) >= t.minShimmerDb);

  const auto noise = makeCabNoise(kProbeLen);
  const auto shipped = runRow(mode, noise, 2, volum::kVoLumChorusModeDefaults[mode].mix);
  CHECK(correlation(shipped.l, shipped.r) <= t.maxCorrelation);

  const auto stereo = runRow(mode, noise);
  const auto mono = runRow(mode, noise, 1);
  CHECK(dB(meanPower(mono.l) / meanPower(stereo.l)) >= -1.0);
}

double maxStep(const std::vector<double>& v, size_t from, size_t to)
{
  double m = 0.0;
  for (size_t i = std::max<size_t>(from, 1); i < to && i < v.size(); ++i)
    m = std::max(m, std::abs(v[i] - v[i - 1]));
  return m;
}

// Largest coherent wet gain per output channel: the sum of the magnitudes of
// the voice weights each mode mixes into one side (mirrors the VoLumChorus
// matrices; 1 for the single-voice modes).
double wetGainSum(int mode, double width, int chans)
{
  if (mode == ChorusDSP::kClear)
  {
    const double k = 0.35, mid = 0.5 * (1.0 - k), side = 0.5 * (1.0 + k);
    if (chans == 1)
      return std::sqrt(2.0);
    const double gain = 1.0 / std::sqrt(2.0 * mid * mid + width * width * 2.0 * side * side);
    return gain * (std::abs(mid + width * side) + std::abs(mid - width * side));
  }
  if (mode == ChorusDSP::kEnsemble)
  {
    if (chans == 1)
      return (1.0 + 0.5 + 0.35) / std::sqrt(1.0 + 0.25 + 0.35 * 0.35);
    const double c = 0.5, norm = 1.0 / std::sqrt(1.0 + 0.5 * c * c);
    return norm * (std::sqrt(0.5 * (1.0 + width)) + std::sqrt(0.5 * (1.0 - width)) + c * std::sqrt(0.5));
  }
  return 1.0;
}

// Compile-time probe for ChorusDSP::MinReadDelayMs (absent on engines that
// predate the 1 ms floor test, which then read as a failure).
template <typename C, typename = void>
struct HasMinRead : std::false_type
{
};
template <typename C>
struct HasMinRead<C, std::void_t<decltype(std::declval<const C&>().MinReadDelayMs())>> : std::true_type
{
};
template <typename C>
double minReadMs(const C& chorus)
{
  if constexpr (HasMinRead<C>::value)
    return chorus.MinReadDelayMs();
  else
    return -1.0;
}
} // namespace

TEST_CASE("Chorus MIX 0 settles to bit-identical without Reset after a wet buffer")
{
  // ProcessBlock calls SetParams every block and never Reset()s on MIX. MIX
  // glides (an instant snap clicks), but it must land exactly on 0 so the
  // stomp-on, MIX-0 state is a true bypass once the glide is over.
  const size_t settle = static_cast<size_t>(0.25 * kSR);
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    ChorusDSP chorus;
    chorus.Prepare(kSR, kBlock, 2);
    chorus.SetParams(0.5, 1.0, 0.5, 1.0, 1.0, mode, kSR);
    const auto wetIn = makeChord(kBlock * 4);
    runStream(chorus, wetIn);

    const auto dryIn = makeChord(1 << 15);
    chorus.SetParams(0.5, 1.0, 0.5, 1.0, 0.0, mode, kSR);
    const auto out = runStream(chorus, dryIn);
    INFO("mode " << mode);
    for (size_t i = settle; i < dryIn.l.size(); ++i)
    {
      REQUIRE(out.l[i] == dryIn.l[i]);
      REQUIRE(out.r[i] == dryIn.r[i]);
    }
  }
}

TEST_CASE("Chorus MIX 0 on a freshly prepared instance is bit-identical from the first sample")
{
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    ChorusDSP chorus;
    chorus.Prepare(kSR, kBlock, 2);
    chorus.SetParams(0.5, 1.0, 0.5, 1.0, 0.0, mode, kSR);
    const auto dryIn = makeChord(1 << 12);
    const auto out = runStream(chorus, dryIn);
    INFO("mode " << mode);
    CHECK(maxDiff(out.l, dryIn.l, 0) == 0.0);
    CHECK(maxDiff(out.r, dryIn.r, 0) == 0.0);
  }
}

TEST_CASE("tier2a Chorus MIX 1 is exact wet on an empty line")
{
  // cos(pi/2) is a few ulps above zero, so a blend at MIX 1 still multiplies
  // a sliver of dry into the bus. An empty delay line's wet tap is silence;
  // the first sample must be that silence, not the input.
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    ChorusDSP chorus;
    chorus.Prepare(kSR, kBlock, 2);
    chorus.SetParams(0.5, 1.0, 0.5, 1.0, 1.0, mode, kSR);
    std::vector<double> l(static_cast<size_t>(kBlock), 0.0);
    std::vector<double> r(static_cast<size_t>(kBlock), 0.0);
    l[0] = 0.5;
    r[0] = -0.4;
    double* ptr[2] = {l.data(), r.data()};
    chorus.Process(ptr, 2, kBlock);
    INFO("mode " << mode);
    CHECK(l[0] == 0.0);
    CHECK(r[0] == 0.0);
  }
}

TEST_CASE("Chorus alters the signal at MIX 1 (all modes)")
{
  const auto in = makeChord(1 << 15);
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    const auto out = runMode(mode, in);
    INFO("mode " << mode);
    CHECK(maxDiff(out.l, in.l) > 0.02);
  }
}

TEST_CASE("Chorus modes produce distinct wet signatures at MIX 1")
{
  const auto in = makeChord(1 << 15);
  std::vector<Stereo> perMode;
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
    perMode.push_back(runMode(mode, in));

  for (int a = 0; a < ChorusDSP::kNumModes; ++a)
  {
    for (int b = a + 1; b < ChorusDSP::kNumModes; ++b)
    {
      INFO("modes " << a << " vs " << b);
      CHECK(maxDiff(perMode[a].l, perMode[b].l) > 0.02);
    }
  }
}

TEST_CASE("Chorus output stays finite and inside its analytic peak bound (all modes, knob extremes)")
{
  // Power-normalized voices can line up on a sustained low note, so the wet of
  // a multi-voice mode peaks above the input. The bound per channel is the
  // equal-power blend of the dry and the coherent sum of that side's voice
  // weights, plus 5 % for the TONE / HPF filters' time-domain overshoot.
  std::vector<Stereo> signals = {
    makeChord(1 << 15, 0.5), makeSine(110.0, 1 << 15), makeSine(165.0, 1 << 15), makeSine(330.0, 1 << 15)};
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    for (const auto& in : signals)
    {
      const double inPeak = peakOf(in.l);
      for (double width : {0.0, 1.0})
      {
        for (double rate : {0.0, 0.5, 1.0})
        {
          for (double depth : {0.0, 1.0})
          {
            for (double mix : {0.5, 1.0})
            {
              const auto out = runMode(mode, in, rate, depth, 0.5, width, mix);
              for (size_t i = 0; i < out.l.size(); ++i)
              {
                REQUIRE(std::isfinite(out.l[i]));
                REQUIRE(std::isfinite(out.r[i]));
              }
              const double blend = std::cos(mix * M_PI * 0.5) + std::sin(mix * M_PI * 0.5) * wetGainSum(mode, width, 2);
              const double bound = inPeak * blend * 1.05 + 1e-6;
              INFO("mode " << mode << " width " << width << " rate " << rate << " depth " << depth << " mix " << mix);
              CHECK(peakOf(out.l) <= bound);
              CHECK(peakOf(out.r) <= bound);
            }
          }
        }
      }
    }
  }
}

TEST_CASE("Chorus recovers from one block of NaN knobs")
{
  // std::clamp passes NaN through; a NaN target used to latch in the knob
  // smoothing and silence (or poison) the pedal for good.
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    const auto row = volum::kVoLumChorusModeDefaults[mode];
    ChorusDSP chorus;
    chorus.Prepare(kSR, kBlock, 2);
    chorus.SetParams(row.rate, row.depth, row.tone, row.width, row.mix, mode, kSR);
    auto io = makeChord(static_cast<size_t>(kSR));
    const auto dry = io;
    for (size_t start = 0; start < io.l.size(); start += kBlock)
    {
      if (start == 8 * static_cast<size_t>(kBlock))
        chorus.SetParams(nan, nan, nan, nan, nan, mode, nan);
      else
        chorus.SetParams(row.rate, row.depth, row.tone, row.width, row.mix, mode, kSR);
      const size_t n = std::min<size_t>(kBlock, io.l.size() - start);
      double* ptr[2] = {io.l.data() + start, io.r.data() + start};
      chorus.Process(ptr, 2, static_cast<int>(n));
    }
    INFO("mode " << mode);
    for (size_t i = 0; i < io.l.size(); ++i)
    {
      REQUIRE(std::isfinite(io.l[i]));
      REQUIRE(std::isfinite(io.r[i]));
    }
    // Still chorusing afterwards, not stuck on a clamped or silent state.
    CHECK(maxDiff(io.l, dry.l, static_cast<size_t>(0.5 * kSR)) > 0.02);
    CHECK(peakOf(io.l) <= peakOf(dry.l) * 2.0);
  }
}

TEST_CASE("Chorus WARPED never reads closer than 1 ms, even after minutes of drift")
{
  // The right side mixes two random drifts (cos*L + sin*R, up to sqrt(2) x the
  // drift), so the centre budget must cover that or a long session eventually
  // pulls the read toward the write head.
  struct Knobs
  {
    double rate, depth, width;
  };
  for (const Knobs k : {Knobs{0.0, 1.0, 0.5}, Knobs{0.25, 0.75, 0.5}, Knobs{0.44, 0.8, 0.5}})
  {
    ChorusDSP chorus;
    chorus.Prepare(kSR, 1024, 2);
    chorus.SetParams(k.rate, k.depth, 1.0, k.width, 1.0, ChorusDSP::kWarped, kSR);
    chorus.Reset();
    std::vector<double> l(1024, 0.0), r(1024, 0.0);
    const size_t blocks = static_cast<size_t>(300.0 * kSR) / 1024;
    for (size_t b = 0; b < blocks; ++b)
    {
      double* ptr[2] = {l.data(), r.data()};
      chorus.Process(ptr, 2, 1024);
    }
    INFO("rate " << k.rate << " depth " << k.depth << " width " << k.width);
    CHECK(minReadMs(chorus) >= 0.999);
  }
}

TEST_CASE("Chorus Reset clears the line so re-engaging does not replay a tail")
{
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    ChorusDSP chorus;
    chorus.Prepare(kSR, kBlock, 2);
    chorus.SetParams(0.35, 0.6, 0.5, 0.7, 1.0, mode, kSR);
    chorus.Reset();

    // Ring the delay line with a loud burst.
    auto burst = makeSine(220.0, 4096, 0.9);
    runStream(chorus, burst);

    // Bypass edge: the host calls Reset(), then the effect is re-engaged onto
    // silence. Any surviving buffer content would be audible here.
    chorus.Reset();
    Stereo silence;
    silence.l.assign(4096, 0.0);
    silence.r.assign(4096, 0.0);
    const auto out = runStream(chorus, silence);
    INFO("mode " << mode);
    CHECK(peakOf(out.l) < 1e-12);
    CHECK(peakOf(out.r) < 1e-12);
  }
}

TEST_CASE("Chorus without Reset does replay the buffered tail (guards the Reset test)")
{
  // The counter-case for the test above: if Reset() were a no-op the silence
  // pass would still carry the burst, so this proves the Reset assertion has
  // something real to catch.
  ChorusDSP chorus;
  chorus.Prepare(kSR, kBlock, 2);
  chorus.SetParams(0.35, 0.6, 0.5, 0.7, 1.0, ChorusDSP::kWarped, kSR);
  chorus.Reset();
  auto burst = makeSine(220.0, 4096, 0.9);
  runStream(chorus, burst);

  Stereo silence;
  silence.l.assign(2048, 0.0);
  silence.r.assign(2048, 0.0);
  const auto out = runStream(chorus, silence);
  CHECK(peakOf(out.l) > 1e-4);
}

TEST_CASE("Chorus mode switch and MIX end-stop drags do not click")
{
  // Clearing the lines on a mode switch cut the wet to silence mid-waveform
  // (16x the steady sample step); snapping MIX to an end stop jumped the blend
  // (8-11x). A duck-switch-fade and a gliding MIX keep the step near steady.
  const size_t n = static_cast<size_t>(3.0 * kSR);
  const size_t at = static_cast<size_t>(1.5 * kSR) / kBlock * kBlock;
  const auto in = makeSine(220.0, n, 0.5);
  for (int from = 0; from < ChorusDSP::kNumModes; ++from)
  {
    for (int to = 0; to < ChorusDSP::kNumModes; ++to)
    {
      if (from == to)
        continue;
      ChorusDSP chorus;
      chorus.Prepare(kSR, kBlock, 2);
      auto io = in;
      for (size_t start = 0; start < n; start += kBlock)
      {
        const int mode = (start < at) ? from : to;
        const auto row = volum::kVoLumChorusModeDefaults[mode];
        chorus.SetParams(row.rate, row.depth, row.tone, row.width, 0.5, mode, kSR);
        double* ptr[2] = {io.l.data() + start, io.r.data() + start};
        chorus.Process(ptr, 2, kBlock);
      }
      const double steady = std::max(maxStep(io.l, at - 24000, at), maxStep(io.r, at - 24000, at));
      const double around = std::max(maxStep(io.l, at, at + 4800), maxStep(io.r, at, at + 4800));
      INFO("mode " << from << " -> " << to << " steady " << steady << " around " << around);
      CHECK(around <= 1.5 * steady);
    }
  }
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    for (double endMix : {0.0, 1.0})
    {
      const auto row = volum::kVoLumChorusModeDefaults[mode];
      ChorusDSP chorus;
      chorus.Prepare(kSR, kBlock, 2);
      auto io = in;
      const size_t t0 = static_cast<size_t>(1.0 * kSR) / kBlock * kBlock;
      for (size_t start = 0; start < n; start += kBlock)
      {
        double mix = row.mix;
        if (start >= t0) // the user drags MIX to the end stop over ~43 ms
          mix = row.mix + (endMix - row.mix) * std::min(1.0, static_cast<double>(start - t0) / (8.0 * kBlock));
        chorus.SetParams(row.rate, row.depth, row.tone, row.width, mix, mode, kSR);
        double* ptr[2] = {io.l.data() + start, io.r.data() + start};
        chorus.Process(ptr, 2, kBlock);
      }
      const double steady = maxStep(io.l, t0 - 24000, t0);
      const double later = maxStep(io.l, t0 + 24000, n);
      const double drag = maxStep(io.l, t0, t0 + 24000);
      INFO("mode " << mode << " MIX -> " << endMix << " steady " << steady << " drag " << drag);
      CHECK(drag <= 1.5 * std::max(steady, later));
    }
  }
}

TEST_CASE("Chorus WIDTH decorrelates the two channels (all modes)")
{
  const auto in = makeChord(1 << 16);
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    const auto narrow = runMode(mode, in, 0.35, 0.6, 0.5, 0.0 /*width*/, 1.0);
    const auto wide = runMode(mode, in, 0.35, 0.6, 0.5, 1.0 /*width*/, 1.0);
    INFO("mode " << mode);
    // At WIDTH 0 both channels see the same treatment; at WIDTH 1 they diverge.
    CHECK(maxDiff(narrow.l, narrow.r) < 1e-9);
    CHECK(maxDiff(wide.l, wide.r) > 0.01);
  }
}

TEST_CASE("Chorus TONE darkens the wet bus counter-clockwise")
{
  // A 2-pole low-pass on the wet path (3..12 kHz): turning TONE down must shed
  // high-frequency energy from the wet signal, so a bright test tone loses level.
  auto wetEnergyAbove = [](double tone) {
    Stereo in;
    const size_t n = 1 << 15;
    in.l.resize(n);
    in.r.resize(n);
    for (size_t i = 0; i < n; ++i)
    {
      const double v = 0.5 * std::sin(2.0 * M_PI * 6000.0 * static_cast<double>(i) / kSR);
      in.l[i] = v;
      in.r[i] = v;
    }
    const auto out = runMode(ChorusDSP::kClear, in, 0.2, 0.4, tone, 0.0, 1.0);
    double sum = 0.0;
    for (size_t i = 8192; i < n; ++i)
      sum += out.l[i] * out.l[i];
    return sum;
  };

  const double dark = wetEnergyAbove(0.0);
  const double bright = wetEnergyAbove(1.0);
  CHECK(dark < bright * 0.5);
}

TEST_CASE("Chorus mode names cover every mode and default safely")
{
  std::vector<std::string> names;
  for (int mode = 0; mode < volum::kVoLumChorusModeCount; ++mode)
  {
    const char* name = volum::VoLumChorusModeName(mode);
    REQUIRE(name != nullptr);
    CHECK(std::string(name).size() > 0);
    names.push_back(name);
  }
  // Every voice needs its own label: two modes sharing one name is how a card
  // ends up reading "Warped" while it is running something else.
  std::sort(names.begin(), names.end());
  CHECK(std::unique(names.begin(), names.end()) == names.end());
  CHECK(std::string(volum::VoLumChorusModeName(-1)) == "Warped");
  CHECK(std::string(volum::VoLumChorusModeName(999)) == "Warped");
  CHECK(volum::kVoLumChorusModeDefault == volum::kVoLumChorusModeWarped);
  const auto classic = volum::kVoLumChorusModeDefaults[volum::kVoLumChorusModeClassic];
  CHECK(classic.rate == doctest::Approx(0.40));
  CHECK(classic.depth == doctest::Approx(0.62));
  CHECK(classic.width == doctest::Approx(1.00));
  const auto warped = volum::kVoLumChorusModeDefaults[volum::kVoLumChorusModeWarped];
  CHECK(warped.depth == doctest::Approx(0.36));
  CHECK(warped.tone == doctest::Approx(0.21));
  // Every voice ships at the 1:1 hardware blend.
  for (const auto& row : volum::kVoLumChorusModeDefaults)
    CHECK(row.mix == doctest::Approx(0.50));
}

TEST_CASE("Chorus survives an out-of-range mode and degenerate sample rate")
{
  const auto in = makeChord(1 << 14);

  ChorusDSP chorus;
  chorus.Prepare(0.0, kBlock, 2); // degenerate rate heals to 48 kHz
  chorus.SetParams(2.0, -1.0, 5.0, -3.0, 2.0, 99 /*mode*/, 0.0);
  chorus.Reset();
  const auto out = runStream(chorus, in);
  for (size_t i = 0; i < out.l.size(); ++i)
  {
    REQUIRE(std::isfinite(out.l[i]));
    REQUIRE(std::isfinite(out.r[i]));
  }
  // Healed, not merely survived: a zero rate must leave a usable line rather
  // than a zero-length one that silently passes audio through.
  CHECK(maxDiff(out.l, in.l) > 0.02);

  // And an unknown mode must land on the default voice, not on whatever the
  // tuning table happens to clamp to.
  ChorusDSP reference;
  reference.Prepare(48000.0, kBlock, 2);
  reference.SetParams(2.0, -1.0, 5.0, -3.0, 2.0, volum::kVoLumChorusModeDefault, 0.0);
  reference.Reset();
  const auto ref = runStream(reference, in);
  CHECK(maxDiff(out.l, ref.l) < 1e-12);
}

TEST_CASE("Chorus is stable across a live mode switch every block")
{
  ChorusDSP chorus;
  chorus.Prepare(kSR, kBlock, 2);
  chorus.Reset();
  const auto dry = makeChord(1 << 15);
  auto io = dry;
  int mode = 0;
  for (size_t start = 0; start < io.l.size(); start += kBlock)
  {
    const size_t n = std::min<size_t>(kBlock, io.l.size() - start);
    chorus.SetParams(0.4, 0.7, 0.5, 0.8, 0.7, mode % ChorusDSP::kNumModes, kSR);
    mode++;
    double* ptr[2] = {io.l.data() + start, io.r.data() + start};
    chorus.Process(ptr, 2, static_cast<int>(n));
  }
  for (size_t i = 0; i < io.l.size(); ++i)
    REQUIRE(std::isfinite(io.l[i]));
  // Still processing (not silently bailing out) and still inside the same peak
  // bound as steady state: switching voices every block must not stack dry and
  // wet into a click.
  CHECK(maxDiff(io.l, dry.l) > 0.02);
  CHECK(peakOf(io.l) <= peakOf(dry.l) * 1.45 + 1e-6);
}

// ---------------------------------------------------------------- strength at the shipped rows
// The 1.3.0 chorus measured 3-5 cents with a 1-8.6 dB darker wet. Each case
// below fails on that engine; the numbers come from the Juno-60, Dimension D
// and tri-stereo rack references in .scratch/chorus-research/report.md.

TEST_CASE("Chorus strength: CLASSIC is a Juno-60 I sweep at its default row")
{
  // 1.6 -> 5.3 ms triangle at 0.63 Hz = 8 cents (Juno I 6.6, Juno II 11.3).
  checkStrength(ChorusDSP::kClassic, {7.0, -1.5, 0.55});
}

TEST_CASE("Chorus strength: WARPED warbles by about 18 cents at its default row")
{
  checkStrength(ChorusDSP::kWarped, {15.0, -3.0, 0.60});
}

TEST_CASE("Chorus strength: CLEAR is wide in stereo and pitch-clean in mono")
{
  checkStrength(ChorusDSP::kClear, {7.0, -1.5, 0.40});

  // The two voices move in anti-phase, so their pitch wobble cancels in L+R
  // (and on a 1-1 track) while each side keeps it.
  const auto sine = runRow(ChorusDSP::kClear, makeSine(440.0, kProbeLen));
  std::vector<double> sum(sine.l.size());
  for (size_t i = 0; i < sum.size(); ++i)
    sum[i] = sine.l[i] + sine.r[i];
  const double side = peakCents(sine.l, 440.0);
  CHECK(peakCents(sum, 440.0) <= 0.25 * side);
  const auto mono = runRow(ChorusDSP::kClear, makeSine(440.0, kProbeLen), 1);
  CHECK(peakCents(mono.l, 440.0) <= 0.25 * side);
}

TEST_CASE("Chorus strength: ENSEMBLE detunes low notes too (no constant-mean-delay voice set)")
{
  checkStrength(ChorusDSP::kEnsemble, {7.0, -1.5, 0.60});

  // Three sine taps 120 deg apart have a constant mean delay; summed, a low
  // note (where the taps are nearly in phase) hears no pitch motion at all.
  const auto low = runRow(ChorusDSP::kEnsemble, makeSine(100.0, kProbeLen));
  CHECK(peakCents(low.l, 100.0) >= 4.0);
  CHECK(peakCents(low.r, 100.0) >= 4.0);
  const auto lowMono = runRow(ChorusDSP::kEnsemble, makeSine(100.0, kProbeLen), 1);
  CHECK(peakCents(lowMono.l, 100.0) >= 4.0);
}

TEST_CASE("Chorus DEPTH means cents at any RATE (WARPED, CLEAR)")
{
  // A hardware ms depth goes nearly silent at slow rates. The shipped DEPTH is
  // detune, so RATE 0.2 and 0.8 keep the same strength. CLASSIC keeps the Juno
  // ms sweep on purpose. ENSEMBLE follows the same law per voice, but its
  // outputs sum two voices whose beating swamps a zero-crossing readout.
  for (int mode : {static_cast<int>(ChorusDSP::kWarped), static_cast<int>(ChorusDSP::kClear)})
  {
    INFO("mode " << std::string(volum::VoLumChorusModeName(mode)));
    const auto sine = makeSine(440.0, static_cast<size_t>(14.0 * kSR));
    const double slow = peakCents(runRow(mode, sine, 2, 1.0, 0.2).l, 440.0);
    const double fast = peakCents(runRow(mode, sine, 2, 1.0, 0.8).l, 440.0);
    CHECK(slow == doctest::Approx(fast).epsilon(0.25));
  }
}

TEST_CASE("Chorus knob moves do not step the delay (RATE, DEPTH, WIDTH smoothing)")
{
  // A 440 Hz sine's largest sample step is 2*pi*440/48000 * amp. A stepped
  // delay (an unsmoothed WIDTH phase jump, a DEPTH jump) shows up as one step
  // several times larger than that.
  const double amp = 0.5;
  const double sineStep = 2.0 * M_PI * 440.0 / kSR * amp;
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    ChorusDSP chorus;
    chorus.Prepare(kSR, kBlock, 2);
    chorus.SetParams(0.3, 0.3, 1.0, 0.0, 1.0, mode, kSR);
    chorus.Reset();
    auto in = makeSine(440.0, static_cast<size_t>(1.5 * kSR), amp);
    double worst = 0.0;
    double prev[2] = {0.0, 0.0};
    for (size_t start = 0; start < in.l.size(); start += kBlock)
    {
      if (start == static_cast<size_t>(0.75 * kSR) / kBlock * kBlock)
        chorus.SetParams(0.7, 0.9, 1.0, 1.0, 1.0, mode, kSR);
      const size_t n = std::min<size_t>(kBlock, in.l.size() - start);
      double* ptr[2] = {in.l.data() + start, in.r.data() + start};
      chorus.Process(ptr, 2, static_cast<int>(n));
      for (size_t i = start; i < start + n; ++i)
      {
        if (i > 4800)
        {
          worst = std::max(worst, std::abs(in.l[i] - prev[0]));
          worst = std::max(worst, std::abs(in.r[i] - prev[1]));
        }
        prev[0] = in.l[i];
        prev[1] = in.r[i];
      }
    }
    INFO("mode " << mode);
    CHECK(worst < 1.6 * sineStep);
  }
}

TEST_CASE("Chorus output does not depend on host block size (1 and 2 channels, up to 8192)")
{
  const auto in = makeChord(1 << 15);
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    for (int chans : {1, 2})
    {
      ChorusDSP a;
      a.Prepare(kSR, 64, chans);
      a.SetParams(0.5, 0.6, 0.6, 0.8, 0.5, mode, kSR);
      a.Reset();
      ChorusDSP b;
      b.Prepare(kSR, 8192, chans);
      b.SetParams(0.5, 0.6, 0.6, 0.8, 0.5, mode, kSR);
      b.Reset();
      const auto small = runStream(a, in, chans, 64);
      const auto big = runStream(b, in, chans, 8192);
      INFO("mode " << mode << " chans " << chans);
      CHECK(maxDiff(small.l, big.l, 0) < 1e-12);
      if (chans == 2)
        CHECK(maxDiff(small.r, big.r, 0) < 1e-12);
      else
        CHECK(maxDiff(big.r, in.r, 0) == 0.0); // a 1-1 track never touches a second buffer
    }
  }
}

TEST_CASE("Chorus on/off restore reads VoLumEffectSettings.chorusActive (pack-import mapping)")
{
  // Pack-settings import's last apply is _VolumRestoreEffectSettings, which
  // must set kChorusActive from fx.chorusActive the same way delay/reverb read
  // fx.delayActive / fx.reverbActive. The plugin mapping is
  // VoLumEffectChorusActiveParam (Scene applies it after restore). A DSP-only
  // chorus test never sees this field.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].postChorusActive = false; // per-amp scene disagrees on purpose
  volum::VoLumEffectSettings fx;
  fx.chorusActive = true;
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);

  REQUIRE(j["effects"].contains("chorusActive"));
  REQUIRE(j["effects"]["chorusActive"] == true);

  volum::VoLumEffectSettings loaded;
  loaded.chorusActive = false; // stale live switch, the H15 pre-import state
  int last = 0;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, &last, &loaded);
  CHECK(loaded.chorusActive == true);
  CHECK(volum::VoLumEffectChorusActiveParam(loaded) == 1.0);

  fx.chorusActive = false;
  const nlohmann::json jOff = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);
  loaded.chorusActive = true;
  volum::VolumUserSettingsFromJson(jOff, amps, volum::kAmpCount, &last, &loaded);
  CHECK(loaded.chorusActive == false);
  CHECK(volum::VoLumEffectChorusActiveParam(loaded) == 0.0);

  const auto scenePath = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumSettingsScene.inc.cpp";
  std::ifstream sceneIn(scenePath, std::ios::binary);
  REQUIRE(sceneIn);
  const std::string scene((std::istreambuf_iterator<char>(sceneIn)), std::istreambuf_iterator<char>());
  CHECK(scene.find("VoLumEffectChorusActiveParam(mVolumEffectSettings)") != std::string::npos);
  CHECK(scene.find("_VolumRestoreEffectSettings()") != std::string::npos);
}

TEST_CASE("Legacy effects JSON without chorusActive seeds from amp postChorusActive")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].postChorusActive = true;
  volum::VoLumEffectSettings fx;
  fx.chorusActive = false;
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);
  REQUIRE(j["effects"].contains("chorusActive"));
  j["effects"].erase("chorusActive");

  volum::VoLumEffectSettings loaded;
  loaded.chorusActive = false; // stale live off
  int last = 0;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, &last, &loaded);
  CHECK(loaded.chorusActive == true);
  CHECK(volum::VoLumEffectChorusActiveParam(loaded) == 1.0);
}
