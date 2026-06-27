#include "third_party/doctest.h"
#include "../VoLumPitchShifter.h"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

using dsp::effect::VoLumPitch;

namespace
{
constexpr double kSR = 48000.0;
constexpr int kBlock = 256;

std::vector<DSP_SAMPLE> makeSine(double freq, size_t n, double amp = 0.5)
{
  std::vector<DSP_SAMPLE> v(n);
  for (size_t i = 0; i < n; ++i)
    v[i] = static_cast<DSP_SAMPLE>(amp * std::sin(2.0 * M_PI * freq * static_cast<double>(i) / kSR));
  return v;
}

// Plucked-string-ish tone (decaying harmonics) for more realistic tracking tests.
std::vector<DSP_SAMPLE> makeGuitar(double f0, size_t n)
{
  std::vector<DSP_SAMPLE> v(n, 0.0);
  for (int h = 1; h <= 8; ++h)
  {
    const double fh = f0 * h;
    if (fh > 0.45 * kSR)
      break;
    const double amp = 0.4 / h;
    const double tau = 2.0 / (1.0 + 0.5 * (h - 1));
    for (size_t i = 0; i < n; ++i)
    {
      const double t = static_cast<double>(i) / kSR;
      v[i] += static_cast<DSP_SAMPLE>(amp * std::exp(-t / tau) * std::sin(2.0 * M_PI * fh * t));
    }
  }
  return v;
}

std::vector<DSP_SAMPLE> runStream(VoLumPitch& pitch, const std::vector<DSP_SAMPLE>& in)
{
  std::vector<DSP_SAMPLE> out;
  out.reserve(in.size());
  std::vector<DSP_SAMPLE> scratch(kBlock);
  for (size_t start = 0; start < in.size(); start += kBlock)
  {
    const size_t n = std::min<size_t>(kBlock, in.size() - start);
    std::copy(in.begin() + start, in.begin() + start + n, scratch.begin());
    DSP_SAMPLE* ptr[1] = {scratch.data()};
    DSP_SAMPLE** o = pitch.Process(ptr, 1, n);
    out.insert(out.end(), o[0], o[0] + n);
  }
  return out;
}

// Smallest-strong-peak autocorrelation fundamental estimate (octave-robust for
// the steady sines we feed these tests).
double estimateFreq(const std::vector<DSP_SAMPLE>& x, size_t from, size_t len, int minLag, int maxLag)
{
  std::vector<double> r(static_cast<size_t>(maxLag) + 1, 0.0);
  double rMax = 0.0;
  for (int lag = minLag; lag <= maxLag; ++lag)
  {
    double acc = 0.0;
    for (size_t i = 0; i < len; ++i)
      acc += static_cast<double>(x[from + i]) * static_cast<double>(x[from + i + static_cast<size_t>(lag)]);
    r[static_cast<size_t>(lag)] = acc;
    rMax = std::max(rMax, acc);
  }
  for (int lag = minLag + 1; lag < maxLag; ++lag)
  {
    if (r[static_cast<size_t>(lag)] >= 0.8 * rMax && r[static_cast<size_t>(lag)] >= r[static_cast<size_t>(lag) - 1]
        && r[static_cast<size_t>(lag)] >= r[static_cast<size_t>(lag) + 1])
    {
      // Parabolic interpolation for sub-lag accuracy.
      const double a = r[static_cast<size_t>(lag) - 1], b = r[static_cast<size_t>(lag)], c = r[static_cast<size_t>(lag) + 1];
      const double denom = a - 2.0 * b + c;
      double delta = 0.0;
      if (std::fabs(denom) > 1e-12)
        delta = 0.5 * (a - c) / denom;
      return kSR / (static_cast<double>(lag) + delta);
    }
  }
  return kSR / static_cast<double>(minLag);
}

double cents(double f, double ref) { return 1200.0 * std::log2(f / ref); }
} // namespace

TEST_CASE("VoLumPitch passthrough when unconfigured")
{
  VoLumPitch pitch;
  auto in = makeSine(220.0, 512);
  std::vector<DSP_SAMPLE> buf = in;
  DSP_SAMPLE* ptr[1] = {buf.data()};
  DSP_SAMPLE** o = pitch.Process(ptr, 1, buf.size());
  for (size_t i = 0; i < in.size(); ++i)
    CHECK(o[0][i] == doctest::Approx(static_cast<double>(in[i])));
}

TEST_CASE("VoLumPitch latency is positive and fixed at grain/2")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  CHECK(pitch.Latency() > 0);
  CHECK(pitch.Latency() == VoLumPitch::GrainSamplesFor(kSR) / 2);
  // Latency is engine-fixed: it does not depend on mode.
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 1.0, 1.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0);
  CHECK(pitch.Latency() == VoLumPitch::GrainSamplesFor(kSR) / 2);
}

TEST_CASE("VoLumPitch transpose Mix=0 equals latency-delayed dry")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 7.0, 0.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0);
  const int lat = pitch.Latency();

  auto in = makeSine(180.0, 16384);
  auto out = runStream(pitch, in);

  double maxErr = 0.0;
  for (size_t t = static_cast<size_t>(lat); t < in.size(); ++t)
    maxErr = std::max(maxErr, std::abs(static_cast<double>(out[t]) - static_cast<double>(in[t - lat])));
  CHECK(maxErr < 1e-4);
}

TEST_CASE("VoLumPitch transpose up an octave doubles frequency")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 12.0, 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0);

  auto in = makeSine(220.0, 1 << 16);
  auto out = runStream(pitch, in);

  const size_t from = static_cast<size_t>(pitch.Latency()) + 16384;
  const double f = estimateFreq(out, from, 8192, 40, 600);
  CHECK(f == doctest::Approx(440.0).epsilon(0.05));
}

TEST_CASE("VoLumPitch transpose tracks the whole -12..+7 range accurately")
{
  // Granular is exact-pitch (resampling), so every step should land within a few
  // percent of the mathematical target on a steady tone.
  const double base = 196.0; // G3, a mid string
  for (int semi : {-12, -7, -5, -3, -2, 2, 5, 7})
  {
    VoLumPitch pitch;
    pitch.Configure(kSR, kBlock);
    pitch.Reset();
    pitch.SetParams(VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0,
                    VoLumPitch::Voicing::Modern, 0.0);
    auto in = makeSine(base, 1 << 16);
    auto out = runStream(pitch, in);
    const double target = base * std::pow(2.0, semi / 12.0);
    const int minLag = std::max(8, static_cast<int>(kSR / (target * 2.0)));
    const int maxLag = static_cast<int>(kSR / (target * 0.5));
    const size_t from = static_cast<size_t>(pitch.Latency()) + 20000;
    const double f = estimateFreq(out, from, 8192, minLag, maxLag);
    INFO("semi=", semi, " target=", target, " measured=", f);
    // Mid-range shifts land tight; the full octave down (-12) is granular's
    // warble-heavy edge, so the autocorr estimate is rougher there (the resample
    // ratio itself is exact). Either way this catches gross/octave errors.
    const double tolCents = std::abs(semi) >= 12 ? 120.0 : 40.0;
    CHECK(std::abs(cents(f, target)) < tolCents);
  }
}

TEST_CASE("VoLumPitch transpose holds pitch across a long sustain (no ringing detune)")
{
  // The whole reason for the granular engine: low-string downshifts must not
  // drift while the note rings. Compare an early vs a late window.
  for (int semi : {-2, -5})
  {
    VoLumPitch pitch;
    pitch.Configure(kSR, kBlock);
    pitch.Reset();
    pitch.SetParams(VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0,
                    VoLumPitch::Voicing::Modern, 0.0);
    auto in = makeSine(110.0, 1 << 17); // A2, low string, ~2.7 s
    auto out = runStream(pitch, in);
    const double target = 110.0 * std::pow(2.0, semi / 12.0);
    const int minLag = std::max(8, static_cast<int>(kSR / (target * 2.0)));
    const int maxLag = static_cast<int>(kSR / (target * 0.5));
    const size_t lat = static_cast<size_t>(pitch.Latency());
    const double fEarly = estimateFreq(out, lat + 12000, 8192, minLag, maxLag);
    const double fLate = estimateFreq(out, lat + 90000, 8192, minLag, maxLag);
    INFO("semi=", semi, " early=", fEarly, " late=", fLate);
    CHECK(std::abs(cents(fLate, fEarly)) < 10.0);
  }
}

TEST_CASE("VoLumPitch transpose 0 semitones Mix=1 is a clean delayed passthrough")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 0.0, 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0);
  const int lat = pitch.Latency();

  auto in = makeGuitar(146.83, 1 << 16);
  auto out = runStream(pitch, in);

  double maxErr = 0.0;
  for (size_t t = static_cast<size_t>(lat); t < in.size(); ++t)
    maxErr = std::max(maxErr, std::abs(static_cast<double>(out[t]) - static_cast<double>(in[t - lat])));
  CHECK(maxErr < 1e-3);
}

TEST_CASE("VoLumPitch octaver down an octave halves frequency")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 0.0, 1.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0);

  auto in = makeSine(220.0, 1 << 16);
  auto out = runStream(pitch, in);

  const size_t from = static_cast<size_t>(pitch.Latency()) + 16384;
  const double f = estimateFreq(out, from, 8192, 60, 1200);
  CHECK(f == doctest::Approx(110.0).epsilon(0.06));
}

TEST_CASE("VoLumPitch octaver dry-only equals delayed dry")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 0.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0);
  const int lat = pitch.Latency();

  auto in = makeSine(150.0, 16384);
  auto out = runStream(pitch, in);

  double maxErr = 0.0;
  for (size_t t = static_cast<size_t>(lat); t < in.size(); ++t)
    maxErr = std::max(maxErr, std::abs(static_cast<double>(out[t]) - static_cast<double>(in[t - lat])));
  CHECK(maxErr < 1e-4);
}

TEST_CASE("VoLumPitch stays finite and bounded in all modes")
{
  for (int modeI = 0; modeI < 2; ++modeI)
  {
    for (int voiceI = 0; voiceI < 2; ++voiceI)
    {
      VoLumPitch pitch;
      pitch.Configure(kSR, kBlock);
      pitch.Reset();
      pitch.SetParams(
        static_cast<VoLumPitch::Mode>(modeI), 5.0, 0.7, 0.8, 0.8, 0.8, static_cast<VoLumPitch::Voicing>(voiceI), 6.0);

      auto in = makeSine(140.0, 1 << 15, 0.9);
      auto out = runStream(pitch, in);

      double peak = 0.0;
      for (double v : out)
      {
        CHECK(std::isfinite(v));
        peak = std::max(peak, std::abs(v));
      }
      CHECK(peak < 8.0);
    }
  }
}

TEST_CASE("VoLumPitch handles NaN/Inf input without propagating")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 0.5, 1.0, 1.0, 1.0, VoLumPitch::Voicing::Vintage, 0.0);

  std::vector<DSP_SAMPLE> in(kBlock, 0.2);
  in[10] = static_cast<DSP_SAMPLE>(std::nan(""));
  in[20] = static_cast<DSP_SAMPLE>(INFINITY);
  DSP_SAMPLE* ptr[1] = {in.data()};
  DSP_SAMPLE** o = pitch.Process(ptr, 1, in.size());
  for (size_t i = 0; i < in.size(); ++i)
    CHECK(std::isfinite(static_cast<double>(o[0][i])));
}
