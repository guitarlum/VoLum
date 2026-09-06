#include "third_party/doctest.h"
#include "../VoLumChorus.h"

#include <algorithm>
#include <cmath>
#include <string>
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
    const double v = amp * (std::sin(2.0 * M_PI * 110.0 * t) + std::sin(2.0 * M_PI * 277.0 * t)
                            + std::sin(2.0 * M_PI * 1320.0 * t) / 3.0);
    s.l[i] = v;
    s.r[i] = v;
  }
  return s;
}

Stereo runStream(ChorusDSP& chorus, const Stereo& in)
{
  Stereo out = in;
  for (size_t start = 0; start < out.l.size(); start += kBlock)
  {
    const size_t n = std::min<size_t>(kBlock, out.l.size() - start);
    double* ptr[2] = {out.l.data() + start, out.r.data() + start};
    chorus.Process(ptr, 2, static_cast<int>(n));
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
} // namespace

TEST_CASE("Chorus MIX 0 is bit-identical without Reset after a wet buffer")
{
  // ProcessBlock calls SetParams every block and never Reset()s on MIX. The
  // one-pole must snap to 0 or the wet blend lingers.
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    ChorusDSP chorus;
    chorus.Prepare(kSR, kBlock, 2);
    chorus.SetParams(0.5, 1.0, 0.5, 1.0, 1.0, mode, kSR);
    const auto wetIn = makeChord(kBlock * 4);
    runStream(chorus, wetIn);

    const auto dryIn = makeChord(1 << 14);
    chorus.SetParams(0.5, 1.0, 0.5, 1.0, 0.0, mode, kSR);
    const auto out = runStream(chorus, dryIn);
    INFO("mode " << mode);
    for (size_t i = 0; i < dryIn.l.size(); ++i)
    {
      REQUIRE(out.l[i] == dryIn.l[i]);
      REQUIRE(out.r[i] == dryIn.r[i]);
    }
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

TEST_CASE("Chorus output stays finite and bounded (all modes, knob extremes)")
{
  const auto in = makeChord(1 << 15, 0.5);
  const double inPeak = peakOf(in.l);
  for (int mode = 0; mode < ChorusDSP::kNumModes; ++mode)
  {
    for (double rate : {0.0, 0.5, 1.0})
    {
      for (double depth : {0.0, 1.0})
      {
        for (double mix : {0.5, 1.0})
        {
          const auto out = runMode(mode, in, rate, depth, 0.5, 1.0, mix);
          for (size_t i = 0; i < out.l.size(); ++i)
          {
            REQUIRE(std::isfinite(out.l[i]));
            REQUIRE(std::isfinite(out.r[i]));
          }
          // Equal-power blend peaks at sqrt(2) when dry and wet line up.
          const double bound = inPeak * 1.45 + 1e-6;
          CHECK(peakOf(out.l) <= bound);
          CHECK(peakOf(out.r) <= bound);
        }
      }
    }
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

TEST_CASE("Chorus mode change resets the line (Delay/Tremolo contract)")
{
  ChorusDSP chorus;
  chorus.Prepare(kSR, kBlock, 2);
  chorus.SetParams(0.35, 0.6, 0.5, 0.7, 1.0, ChorusDSP::kWarped, kSR);
  chorus.Reset();
  auto burst = makeSine(220.0, 4096, 0.9);
  runStream(chorus, burst);

  // Switching mode through SetParams must clear the line just like Delay does,
  // so the new voice never starts by replaying the old one's buffer.
  chorus.SetParams(0.35, 0.6, 0.5, 0.7, 1.0, ChorusDSP::kClear, kSR);
  Stereo silence;
  silence.l.assign(2048, 0.0);
  silence.r.assign(2048, 0.0);
  const auto out = runStream(chorus, silence);
  CHECK(peakOf(out.l) < 1e-12);
}

TEST_CASE("Chorus WIDTH decorrelates the two channels (all modes)")
{
  // Long enough for WARPED's few-percent L/R rate offset to open up.
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
  // A one-pole low-pass on the wet path: turning TONE down must shed high-
  // frequency energy from the wet signal, so a bright test tone loses level.
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
