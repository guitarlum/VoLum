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

// Stream a mono signal through the engine in fixed audio blocks.
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

// Estimate fundamental frequency of a steady region via autocorrelation.
// A pure sine has autocorrelation peaks at the period AND every multiple of it,
// so picking the global max can latch onto a sub-octave. Instead, return the
// smallest lag whose peak is within 80% of the strongest peak: that is the true
// fundamental, and it cleanly distinguishes a real shift from "no shift".
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
      return kSR / static_cast<double>(lag);
  }
  return kSR / static_cast<double>(minLag);
}
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

TEST_CASE("VoLumPitch latency positive and grows with quality")
{
  VoLumPitch lo, hi;
  lo.Configure(kSR, 0.0, kBlock);
  hi.Configure(kSR, 1.0, kBlock);
  CHECK(lo.Latency() > 0);
  CHECK(hi.Latency() > lo.Latency());
}

TEST_CASE("VoLumPitch transpose Mix=0 equals latency-delayed dry")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, 0.5, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 7.0, 0.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0);
  const int lat = pitch.Latency();

  auto in = makeSine(180.0, 8192);
  auto out = runStream(pitch, in);

  // With Mix=0 the wet voice is muted, so output == input delayed by Latency().
  double maxErr = 0.0;
  for (size_t t = static_cast<size_t>(lat); t < in.size(); ++t)
    maxErr = std::max(maxErr, std::abs(static_cast<double>(out[t]) - static_cast<double>(in[t - lat])));
  CHECK(maxErr < 1e-4);
}

TEST_CASE("VoLumPitch transpose 0 semitones Mix=1 stays aligned to dry")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, 0.5, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 0.0, 1.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0);
  const int lat = pitch.Latency();

  auto in = makeSine(220.0, 32768);
  auto out = runStream(pitch, in);

  // Cross-correlate steady region of out against dry delayed by Latency():
  // the peak should sit at ~zero residual lag and be strongly positive.
  const size_t from = static_cast<size_t>(lat) + 8192;
  const size_t len = 8192;
  double num = 0.0, den = 0.0;
  for (size_t i = 0; i < len; ++i)
  {
    const double dry = static_cast<double>(in[from + i - static_cast<size_t>(lat)]);
    const double wet = static_cast<double>(out[from + i]);
    num += dry * wet;
    den += dry * dry;
  }
  const double corr = num / den; // ~1 if wet tracks delayed dry in amplitude
  CHECK(corr > 0.6);
}

TEST_CASE("VoLumPitch transpose up an octave doubles frequency")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, 1.0, kBlock); // best quality for clean tracking
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 12.0, 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0);

  auto in = makeSine(220.0, 1 << 16);
  auto out = runStream(pitch, in);

  const size_t from = static_cast<size_t>(pitch.Latency()) + 16384;
  const double f = estimateFreq(out, from, 8192, 40, 600);
  CHECK(f == doctest::Approx(440.0).epsilon(0.12));
}

TEST_CASE("VoLumPitch octaver down an octave halves frequency")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, 1.0, kBlock);
  pitch.Reset();
  // Only the down voice, no dry/up.
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 0.0, 1.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0);

  auto in = makeSine(220.0, 1 << 16);
  auto out = runStream(pitch, in);

  const size_t from = static_cast<size_t>(pitch.Latency()) + 16384;
  const double f = estimateFreq(out, from, 8192, 60, 1200);
  CHECK(f == doctest::Approx(110.0).epsilon(0.12));
}

TEST_CASE("VoLumPitch octaver dry-only equals delayed dry")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, 0.5, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 0.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0);
  const int lat = pitch.Latency();

  auto in = makeSine(150.0, 8192);
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
      pitch.Configure(kSR, 0.5, kBlock);
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
  pitch.Configure(kSR, 0.5, kBlock);
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
