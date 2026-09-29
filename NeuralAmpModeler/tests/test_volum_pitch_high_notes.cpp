// PRE Pitch: high notes (the Octaver "wobble" on the upper frets).
//
// The tracker used to search 40-600 Hz with a raw, unnormalized autocorrelation. From the 10th fret
// of the high E up, the true period sat below the search floor, and on a decaying note the raw
// autocorrelation favours long lags anyway (older samples are louder), so the estimate settled on
// 10-24 periods. Every splice then jumped up to ~25 ms; on a stiff, vibrato'd high string the upper
// partials no longer line up across such a jump, so each splice (~24 ms apart on the octave-down)
// shifted the timbre. A high-gain amp after the pedal turns that into the reported modulation.
//
// Measured before the fix (48 kHz, plucks): 0 % of tracker updates within 3 % of the true period on
// every note from 659 Hz up; spectral flux excess after a tanh "amp" 1.4-2.0 dB. After: 100 % and
// 0.6-1.0 dB. The thresholds below sit between the two.

#include "third_party/doctest.h"

#include "../VoLumPitchShifter.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
using dsp::effect::GranularVoice;
using dsp::effect::VoLumPitch;

constexpr double kSR = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr size_t kBlock = 64;

// Upper-fret notes on the high E (and one on the B), in Hz.
const std::vector<double> kHighNotes = {659.26, 739.99, 880.00, 987.77, 1174.66, 1318.51};

// Additive plucked string with stiffness B (inharmonicity grows with fret height) and optional
// vibrato in cents at 5.5 Hz. Deterministic on every platform.
std::vector<double> MakeString(double f0, size_t n, double B, double vibCents, double sampleRate = kSR)
{
  std::vector<double> v(n, 0.0);
  for (int h = 1; h <= 16; ++h)
  {
    const double fh = f0 * h * std::sqrt(1.0 + B * h * h);
    if (fh > 0.45 * sampleRate)
      break;
    const double amp = 1.0 / h;
    const double hTau = 0.8 / (1.0 + 0.6 * (h - 1));
    double ph = 0.37 * h;
    for (size_t i = 0; i < n; ++i)
    {
      const double t = static_cast<double>(i) / sampleRate;
      const double vib = std::pow(2.0, vibCents / 1200.0 * std::sin(2.0 * kPi * 5.5 * t));
      ph += 2.0 * kPi * fh * vib / sampleRate;
      v[i] += amp * std::exp(-t / hTau) * std::sin(ph);
    }
  }
  double peak = 1e-9;
  for (double s : v)
    peak = std::max(peak, std::fabs(s));
  for (double& s : v)
    s *= 0.5 / peak;
  return v;
}

// Karplus-Strong lick: noise-burst plucks, one note every `dur` seconds, legato-free.
std::vector<double> MakeLick(const std::vector<double>& notes, double dur)
{
  uint32_t seed = 12345u;
  auto noise = [&seed]() {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<double>(seed >> 8) / 8388608.0 - 1.0;
  };
  const size_t per = static_cast<size_t>(dur * kSR);
  std::vector<double> out(per * notes.size(), 0.0);
  for (size_t k = 0; k < notes.size(); ++k)
  {
    const double N = kSR / notes[k] - 0.5;
    std::vector<double> line(static_cast<size_t>(std::ceil(N)) * 4 + 16, 0.0);
    const double sz = static_cast<double>(line.size());
    size_t w = 0;
    auto at = [&](double idx) {
      while (idx < 0.0)
        idx += sz;
      const size_t i0 = static_cast<size_t>(idx) % line.size();
      const size_t i1 = (i0 + 1) % line.size();
      const double f = idx - std::floor(idx);
      return line[i0] * (1.0 - f) + line[i1] * f;
    };
    for (size_t i = 0; i < per; ++i)
    {
      const double exc = i < static_cast<size_t>(N) ? noise() : 0.0;
      const double rp = static_cast<double>(w) - N;
      const double y = exc + 0.996 * 0.5 * (at(rp) + at(rp - 1.0));
      line[w] = y;
      w = (w + 1) % line.size();
      out[k * per + i] = y;
    }
  }
  double peak = 1e-9;
  for (double s : out)
    peak = std::max(peak, std::fabs(s));
  for (double& s : out)
    s *= 0.5 / peak;
  return out;
}

// Share of tracker updates, after 0.1 s of warm-up, within 3 % of the true period.
double TrackedShare(const std::vector<double>& in, double f0, GranularVoice::Character character, double ratio,
                    double sampleRate = kSR)
{
  GranularVoice voice;
  voice.Configure(sampleRate, static_cast<int>(kBlock));
  voice.SetCharacter(character);
  voice.SetRatio(ratio);
  voice.Reset();
  std::vector<DSP_SAMPLE> out(kBlock);
  const double p0 = sampleRate / f0;
  int ok = 0, total = 0;
  for (size_t off = 0; off + kBlock <= in.size(); off += kBlock)
  {
    voice.Process(in.data() + off, out.data(), kBlock);
    if (off < static_cast<size_t>(0.1 * sampleRate))
      continue;
    ++total;
    if (std::fabs(voice.DebugPeriod() / p0 - 1.0) < 0.03)
      ++ok;
  }
  return total ? static_cast<double>(ok) / total : 0.0;
}

void Fft(std::vector<std::complex<double>>& a)
{
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i)
  {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1)
  {
    const double ang = -2.0 * kPi / static_cast<double>(len);
    const std::complex<double> wl(std::cos(ang), std::sin(ang));
    for (size_t i = 0; i < n; i += len)
    {
      std::complex<double> w(1.0);
      for (size_t j = 0; j < len / 2; ++j)
      {
        const std::complex<double> u = a[i + j], v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= wl;
      }
    }
  }
}

// Mean frame-to-frame spectral change in dB (1024-sample Hann frames, hop 256, 60 Hz - 8 kHz, bins
// within 60 dB of the frame peak). A splice that shifts the partials' phases shows up as flux.
double FluxDb(const std::vector<double>& x, size_t start)
{
  const size_t N = 1024, hop = 256;
  const size_t b0 = static_cast<size_t>(60.0 * N / kSR), b1 = static_cast<size_t>(8000.0 * N / kSR);
  std::vector<double> prev;
  double acc = 0.0;
  size_t frames = 0;
  for (size_t s = start; s + N <= x.size(); s += hop)
  {
    std::vector<std::complex<double>> a(N);
    for (size_t i = 0; i < N; ++i)
      a[i] = x[s + i] * (0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(N - 1)));
    Fft(a);
    std::vector<double> mag(N / 2);
    for (size_t i = 0; i < N / 2; ++i)
      mag[i] = 20.0 * std::log10(std::abs(a[i]) + 1e-9);
    if (!prev.empty())
    {
      double peak = -1e9;
      for (size_t i = b0; i < b1; ++i)
        peak = std::max(peak, mag[i]);
      double d = 0.0;
      size_t m = 0;
      for (size_t i = b0; i < b1; ++i)
        if (std::max(mag[i], prev[i]) > peak - 60.0)
        {
          d += std::fabs(mag[i] - prev[i]);
          ++m;
        }
      if (m)
      {
        acc += d / static_cast<double>(m);
        ++frames;
      }
    }
    prev = mag;
  }
  return frames ? acc / static_cast<double>(frames) : 0.0;
}

// Crude high-gain stage: pre-emphasis, tanh(25x), one-pole "cab".
std::vector<double> HighGainAmp(const std::vector<double>& x)
{
  std::vector<double> z(x.size());
  double prev = 0.0, lp = 0.0;
  for (size_t i = 0; i < x.size(); ++i)
  {
    const double hp = x[i] - 0.7 * prev;
    prev = x[i];
    lp += 0.25 * (std::tanh(25.0 * hp) - lp);
    z[i] = lp;
  }
  return z;
}

// Octaver at the factory defaults (down 0.8, up 0, dry 1, Modern), into the amp; returns the flux
// the pedal adds over the dry signal through the same amp.
double OctaverFluxExcessDb(const std::vector<double>& in)
{
  VoLumPitch pitch;
  pitch.Configure(kSR, static_cast<int>(kBlock));
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 1.0, 0.8, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0);
  pitch.Reset();
  std::vector<double> y(in.size());
  std::vector<DSP_SAMPLE> buf(kBlock);
  for (size_t off = 0; off < in.size(); off += kBlock)
  {
    const size_t m = std::min(kBlock, in.size() - off);
    std::copy(in.begin() + static_cast<long>(off), in.begin() + static_cast<long>(off + m), buf.begin());
    DSP_SAMPLE* ptr = buf.data();
    DSP_SAMPLE** out = pitch.Process(&ptr, 1, m);
    std::copy(out[0], out[0] + m, y.begin() + static_cast<long>(off));
  }
  const size_t start = static_cast<size_t>(0.1 * kSR);
  return FluxDb(HighGainAmp(y), start) - FluxDb(HighGainAmp(in), start);
}

} // namespace

TEST_CASE("PitchHighNotes: the tracker reads plucked upper-fret notes at their true period")
{
  const size_t n = static_cast<size_t>(1.2 * kSR);
  for (double f0 : kHighNotes)
  {
    for (double B : {0.0004, 0.003})
    {
      for (double vib : {0.0, 25.0})
      {
        const std::vector<double> in = MakeString(f0, n, B, vib);
        const double share = TrackedShare(in, f0, GranularVoice::Character::Drop, 0.5);
        INFO("f0=" << f0 << " B=" << B << " vibrato=" << vib << " cents: tracked share " << share);
        CHECK(share >= 0.9);
      }
    }
  }
}

TEST_CASE("PitchHighNotes: INSTANT and 44.1 / 96 kHz track the top of the neck too")
{
  // INSTANT splices by the same estimate with no search to rescue a wrong one.
  const size_t n = static_cast<size_t>(1.2 * kSR);
  for (double f0 : {880.0, 1318.51})
  {
    const std::vector<double> in = MakeString(f0, n, 0.0004, 0.0);
    const double share = TrackedShare(in, f0, GranularVoice::Character::Instant, std::pow(2.0, 7.0 / 12.0));
    INFO("INSTANT +7 f0=" << f0 << ": tracked share " << share);
    CHECK(share >= 0.9);
  }
  for (double sr : {44100.0, 96000.0})
  {
    const std::vector<double> in = MakeString(1318.51, static_cast<size_t>(1.2 * sr), 0.0004, 0.0, sr);
    const double share = TrackedShare(in, 1318.51, GranularVoice::Character::Drop, 0.5, sr);
    INFO("sr=" << sr << " f0=1318.51: tracked share " << share);
    CHECK(share >= 0.9);
  }
}

TEST_CASE("PitchHighNotes: a strong second harmonic does not read an octave high")
{
  // Extending the search to 1400 Hz puts the second harmonic of every note up to 700 Hz in range.
  // The first-peak pick must still land on the fundamental when that harmonic dominates.
  const size_t n = static_cast<size_t>(1.0 * kSR);
  for (double f0 : {110.0, 329.63, 440.0, 659.26})
  {
    std::vector<double> in(n);
    for (size_t i = 0; i < n; ++i)
    {
      const double t = static_cast<double>(i) / kSR;
      in[i] = 0.1 * std::sin(2.0 * kPi * f0 * t) + 0.25 * std::sin(2.0 * kPi * 2.0 * f0 * t + 0.4)
              + 0.03 * std::sin(2.0 * kPi * 3.0 * f0 * t + 1.1);
    }
    const double share = TrackedShare(in, f0, GranularVoice::Character::Drop, 0.5);
    INFO("f0=" << f0 << " with a dominant 2nd harmonic: tracked share " << share);
    CHECK(share >= 0.95);
  }
}

TEST_CASE("PitchHighNotes: a palm-muted note's release keeps the last reading")
{
  // Palm-muted E5 chugs (E2 + B2, dark), each stopped with a 6 ms release. Across the release the newest
  // window is far quieter than the lagged one; reading a period there landed on lags that were no note's
  // period (537 / 637 samples against the dyad's 1165), and INSTANT clicked on them.
  const size_t n = static_cast<size_t>(1.0 * kSR);
  std::vector<double> in(n, 0.0);
  const double roots[2] = {82.41, 123.47};
  const double amps[2] = {0.26, 0.16};
  for (int k = 0; k < 6; ++k)
  {
    const double t0 = 0.2 + 0.05 * k;
    for (int r = 0; r < 2; ++r)
      for (int h = 1; h * roots[r] < 1500.0; ++h)
      {
        const double fh = roots[r] * h;
        const double tau = 0.045 / (1.0 + 0.25 * (h - 1));
        for (size_t i = static_cast<size_t>(t0 * kSR); i < n; ++i)
        {
          const double t = static_cast<double>(i) / kSR - t0;
          double env = std::exp(-t / tau);
          if (t > 0.04)
            env *= std::exp(-(t - 0.04) / 0.006);
          if (env < 1e-6)
            break;
          in[i] += amps[r] / std::pow(static_cast<double>(h), 1.8) * env * std::sin(2.0 * kPi * fh * t + 0.37 * h * h);
        }
      }
  }
  GranularVoice voice;
  voice.Configure(kSR, static_cast<int>(kBlock));
  voice.SetCharacter(GranularVoice::Character::Instant);
  voice.SetRatio(0.5);
  voice.Reset();
  std::vector<DSP_SAMPLE> out(kBlock);
  const double common = kSR / (82.41 / 2.0); // E2 and B2 (3:2) repeat together every 1165 samples
  int readings = 0, offPeriod = 0;
  for (size_t off = 0; off + kBlock <= n; off += kBlock)
  {
    voice.Process(in.data() + off, out.data(), kBlock);
    if (off < static_cast<size_t>(0.3 * kSR))
      continue;
    const double p = voice.DebugPeriod();
    ++readings;
    // Anything the dyad can honestly read: its common period or a divisor that fits both roots.
    bool honest = false;
    for (double d : {1.0, 2.0, 3.0, 6.0})
      honest = honest || std::fabs(p / (common / d) - 1.0) < 0.03;
    if (!honest)
    {
      ++offPeriod;
      INFO("t=" << static_cast<double>(off) / kSR << " period " << p);
      CHECK(honest);
    }
  }
  CHECK(readings > 50);
  CHECK(offPeriod == 0);
}

TEST_CASE("PitchHighNotes: an octave jump in the estimate needs a second update to confirm it")
{
  GranularVoice voice;
  voice.Configure(kSR, static_cast<int>(kBlock));
  voice.Reset();
  voice.DebugAcceptEstimate(100.0, 100);
  CHECK(voice.DebugPeriod() == 100.0);
  // One stray octave reading is held back...
  voice.DebugAcceptEstimate(200.3, 200);
  CHECK(voice.DebugPeriod() == 100.0);
  // ...and taken when the next update agrees.
  voice.DebugAcceptEstimate(199.8, 200);
  CHECK(voice.DebugPeriod() == 199.8);
  // A twelfth down (x3) is held the same way; an unvoiced update in between drops the candidate.
  voice.DebugAcceptEstimate(600.0, 600);
  voice.DebugAcceptEstimate(0.0, 0);
  voice.DebugAcceptEstimate(600.0, 600);
  CHECK(voice.DebugPeriod() == 199.8);
  voice.DebugAcceptEstimate(600.0, 600);
  CHECK(voice.DebugPeriod() == 600.0);
  // Glides, bends and ordinary interval changes follow at once.
  voice.DebugAcceptEstimate(560.0, 560);
  CHECK(voice.DebugPeriod() == 560.0);
  voice.DebugAcceptEstimate(420.0, 420);
  CHECK(voice.DebugPeriod() == 420.0);
}

TEST_CASE("PitchHighNotes: the octaver adds no splice wobble on high notes into a high-gain amp")
{
  const size_t n = static_cast<size_t>(1.2 * kSR);
  struct Case
  {
    std::string name;
    std::vector<double> in;
  };
  std::vector<Case> cases;
  for (double f0 : {880.0, 987.77, 1174.66})
    cases.push_back({"pluck " + std::to_string(f0), MakeString(f0, n, 0.0004, 0.0)});
  for (double base : {659.26, 880.0})
    cases.push_back({"lick from " + std::to_string(base),
                     MakeLick({base, base * 1.1225, base * 1.2599, base * 1.1225, base, base * 1.3348}, 0.25)});
  for (const Case& c : cases)
  {
    const double excess = OctaverFluxExcessDb(c.in);
    INFO(c.name << ": flux excess " << excess << " dB");
    CHECK(excess < 1.2);
  }
}
