#include "third_party/doctest.h"

#include "activations.h"
#include "get_dsp.h"
#include "json.hpp"
#include "slimmable.h"

#include "../../AudioDSPTools/dsp/Delay.h"
#include "../../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../../AudioDSPTools/dsp/RecursiveLinearFilter.h"
#include "../../AudioDSPTools/dsp/Reverb.h"
#include "../ToneStack.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumChorus.h"
#include "../VoLumIrShapingDsp.h"
#include "../VoLumLevelMute.h"
#include "../VoLumPitchShifter.h"
#include "../VoLumPreEffects.h"
#include "../VoLumTremolo.h"
#include "../VoLumUserSettingsIO.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"
#include "../VoLumResamplingNam.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Sound fingerprints of every chain VoLum ships, rendered from one fixed
// synthetic guitar DI and compared against tests/golden/*.json.
//
// A fingerprint is octave-band RMS, peak, crest, spectral centroid, a 50 ms RMS
// envelope and centroid track, the DC mean and the signed correlation with the
// chain's input (polarity) per channel, plus L/R correlation and the mid/side
// ratio for stereo renders. It is not a sample hash, so the same references hold
// on MSVC x64 and Apple clang arm64. Tolerances are fixed here in code:
// regenerating cannot loosen them. Every NAM renders through the production
// ResamplingNAM. The bundled .nam files themselves are pinned by SHA-256.
//
// A plain run never writes a reference. regen-golden-renders.ps1 sets
// VOLUM_GOLDEN_REGEN=1, VOLUM_GOLDEN_REASON and VOLUM_GOLDEN_GROUPS. A group file
// is rewritten only when its fingerprints moved AND the group is listed; a moved
// group that is not listed fails. The reason must not be in
// installer/changelog.txt yet, and the changelog case below fails until it is,
// so a sound change cannot land without a new line naming it.

namespace
{
namespace fs = std::filesystem;
using nlohmann::json;

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 128;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDiSeconds = 2.0;
constexpr double kPostTailSeconds = 1.0; // silence so POST tails are measured
constexpr size_t kFftSize = 4096;
constexpr size_t kFftHop = kFftSize / 2;
constexpr double kEnvFrameSeconds = 0.05;
constexpr int kNumBands = 10;
constexpr double kFloorDb = -140.0;
constexpr double kMidSideCapDb = 120.0;
constexpr int kSchema = 2;
constexpr int kNamManifestSchema = 1;
constexpr size_t kMinReasonLength = 12;
// PreNam1Level / PreNam2Level InitDouble minimum (NeuralAmpModeler.cpp): the mute floor.
constexpr double kPreLevelMinDb = -20.0;
const char* const kBandNames[kNumBands] = {"31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};

fs::path ProjectDir()
{
  return fs::path(__FILE__).parent_path().parent_path();
}

fs::path RepoRoot()
{
  return ProjectDir().parent_path();
}

fs::path GoldenDir()
{
  return ProjectDir() / "tests" / "golden";
}

fs::path ChangelogPath()
{
  return ProjectDir() / "installer" / "changelog.txt";
}

std::string Env(const char* name)
{
  const char* v = std::getenv(name);
  return v ? std::string(v) : std::string();
}

bool RegenRequested()
{
  return Env("VOLUM_GOLDEN_REGEN") == "1";
}

std::string RegenReason()
{
  return Env("VOLUM_GOLDEN_REASON");
}

std::set<std::string> RegenGroups()
{
  std::set<std::string> groups;
  std::stringstream ss(Env("VOLUM_GOLDEN_GROUPS"));
  std::string item;
  while (std::getline(ss, item, ','))
  {
    item.erase(0, item.find_first_not_of(" \t"));
    item.erase(item.find_last_not_of(" \t") + 1);
    if (!item.empty())
      groups.insert(item);
  }
  return groups;
}

std::string ReadFileText(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// ---------------------------------------------------------------------------
// Synthetic guitar DI. Closed-form additive strings plus LCG pick noise, then
// rounded to float like an interface delivers it. Peaks near -8 dBFS.
// ---------------------------------------------------------------------------

struct Note
{
  double start;
  double f0;
  double amp;
  double tau;
  double stop;
  double brightness;
  double maxHz;
};

void AddNote(std::vector<double>& out, const Note& n, std::uint32_t seed, double sr)
{
  const size_t begin = static_cast<size_t>(std::llround(n.start * sr));
  const size_t end = std::min(out.size(), static_cast<size_t>(std::llround((n.stop + 0.05) * sr)));
  const double stopT = n.stop - n.start;
  for (int h = 1; h <= 40; ++h)
  {
    const double fh = n.f0 * h * std::sqrt(1.0 + 8.0e-5 * h * h);
    if (fh > n.maxHz)
      break;
    const double pick = 0.15 + 0.85 * std::abs(std::sin(kPi * h * 0.18));
    const double a = n.amp * pick / std::pow(static_cast<double>(h), n.brightness);
    const double tau = n.tau / (1.0 + 0.25 * (h - 1));
    const double phase = 0.37 * h * h;
    const size_t hEnd = std::min(end, begin + static_cast<size_t>(tau * 18.0 * sr));
    for (size_t i = begin; i < hEnd; ++i)
    {
      const double t = static_cast<double>(i - begin) / sr;
      double env = std::exp(-t / tau);
      if (t > stopT)
        env *= std::exp(-(t - stopT) / 0.006);
      const double attack = t < 0.0004 ? t / 0.0004 : 1.0;
      out[i] += a * env * attack * std::sin(2.0 * kPi * fh * t + phase);
    }
  }

  std::uint32_t state = seed;
  double prev = 0.0;
  const size_t noiseEnd = std::min(end, begin + static_cast<size_t>(0.004 * sr));
  for (size_t i = begin; i < noiseEnd; ++i)
  {
    state = state * 1664525U + 1013904223U;
    const double white = static_cast<double>((state >> 8) & 0x00ffffffU) / static_cast<double>(0x00ffffffU) * 2.0 - 1.0;
    const double t = static_cast<double>(i - begin) / sr;
    const double burst = white * n.amp * 0.8 * std::exp(-t / 0.0008);
    out[i] += 0.5 * (burst - prev);
    prev = burst;
  }
}

std::vector<double> BuildDi(double sr)
{
  std::vector<Note> notes;
  // Open E major strum, low string first, 12 ms apart.
  const double chord[6] = {82.41, 123.47, 164.81, 207.65, 246.94, 329.63};
  for (int s = 0; s < 6; ++s)
    notes.push_back({0.05 + 0.012 * s, chord[s], 0.085, 0.9, 0.70, 1.1, 7000.0});
  // Single notes up the neck, each stopped by the next.
  const double run[8] = {110.0, 146.83, 196.0, 261.63, 349.23, 440.0, 587.33, 783.99};
  for (int k = 0; k < 8; ++k)
    notes.push_back({0.72 + 0.06 * k, run[k], 0.2, 0.6, 0.72 + 0.06 * (k + 1), 1.0, 7000.0});
  // Palm-muted E5 chugs: short, dark, hard pick.
  for (int k = 0; k < 6; ++k)
  {
    const double t0 = 1.24 + 0.05 * k;
    notes.push_back({t0, 82.41, 0.26, 0.045, t0 + 0.04, 1.8, 1500.0});
    notes.push_back({t0 + 0.002, 123.47, 0.16, 0.045, t0 + 0.04, 1.8, 1500.0});
  }
  // Final E5 power chord, ringing into a decaying tail, then silence.
  const double power[4] = {82.41, 123.47, 164.81, 329.63};
  for (int s = 0; s < 4; ++s)
    notes.push_back({1.58 + 0.008 * s, power[s], 0.1, 0.3, 1.90, 1.2, 7000.0});

  std::vector<double> di(static_cast<size_t>(std::llround(kDiSeconds * sr)), 0.0);
  for (size_t i = 0; i < notes.size(); ++i)
    AddNote(di, notes[i], 0x51f15eedU + static_cast<std::uint32_t>(i) * 7919U, sr);
  for (auto& s : di)
    s = static_cast<double>(static_cast<float>(s));
  return di;
}

const std::vector<double>& Di()
{
  static const std::vector<double> di = BuildDi(kSampleRate);
  return di;
}

std::vector<double> DiWithTail()
{
  std::vector<double> x = Di();
  x.resize(x.size() + static_cast<size_t>(std::llround(kPostTailSeconds * kSampleRate)), 0.0);
  return x;
}

// ---------------------------------------------------------------------------
// Fingerprint
// ---------------------------------------------------------------------------

struct ChannelPrint
{
  double rmsDb = kFloorDb;
  double peakDb = kFloorDb;
  double crestDb = 0.0;
  double centroidHz = 0.0;
  std::array<double, kNumBands> bandsDb{};
  std::vector<double> envDb;
  // Schema 2.
  double meanDc = 0.0;
  double inputCorr = 0.0;
  std::vector<double> centroidTrackHz;
};

struct StereoPrint
{
  double lrCorr = 0.0;
  double midSideDb = 0.0;
};

enum class Tier
{
  // Deterministic arithmetic: filters, delay lines, NAM. Platform differences are
  // last-bit rounding only.
  Tight,
  // Engines that pick a splice point by argmax (WSOLA pitch). A near-tie can
  // resolve differently on another compiler, which moves one grain, not the level.
  Decisive,
};

struct Render
{
  std::string name;
  Tier tier = Tier::Tight;
  std::vector<ChannelPrint> channels;
  StereoPrint stereo;
};

double AmpToDb(double x)
{
  return x > 1.0e-7 ? 20.0 * std::log10(x) : kFloorDb;
}

double PowToDb(double p)
{
  return p > 1.0e-14 ? 10.0 * std::log10(p) : kFloorDb;
}

const std::vector<std::complex<double>>& Twiddles()
{
  static const std::vector<std::complex<double>> tw = [] {
    std::vector<std::complex<double>> t(kFftSize / 2);
    for (size_t k = 0; k < t.size(); ++k)
    {
      const double a = -2.0 * kPi * static_cast<double>(k) / static_cast<double>(kFftSize);
      t[k] = {std::cos(a), std::sin(a)};
    }
    return t;
  }();
  return tw;
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
  const auto& tw = Twiddles();
  for (size_t len = 2; len <= n; len <<= 1)
  {
    const size_t step = n / len;
    for (size_t i = 0; i < n; i += len)
      for (size_t k = 0; k < len / 2; ++k)
      {
        const std::complex<double> u = a[i + k];
        const std::complex<double> v = a[i + k + len / 2] * tw[k * step];
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
      }
  }
}

// Normalized zero-lag correlation; the shorter signal counts as zero past its end.
double Correlation(const std::vector<double>& a, const std::vector<double>& b)
{
  double ab = 0.0, aa = 0.0, bb = 0.0;
  for (size_t i = 0; i < a.size(); ++i)
  {
    aa += a[i] * a[i];
    if (i < b.size())
      ab += a[i] * b[i];
  }
  for (double s : b)
    bb += s * s;
  return (aa > 1.0e-20 && bb > 1.0e-20) ? ab / std::sqrt(aa * bb) : 0.0;
}

size_t EnvFrame(double sr)
{
  return static_cast<size_t>(std::llround(kEnvFrameSeconds * sr));
}

// Centroid of one Hann-windowed envelope frame, zero-padded to kFftSize. A frame
// longer than the FFT (50 ms past ~82 kHz) uses its first kFftSize samples.
double FrameCentroid(const std::vector<double>& x, size_t start, size_t end, double sr,
                     std::vector<std::complex<double>>& buf)
{
  const size_t len = std::min(end - start, kFftSize);
  std::fill(buf.begin(), buf.end(), std::complex<double>(0.0, 0.0));
  for (size_t i = 0; i < len; ++i)
  {
    const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(len));
    buf[i] = {x[start + i] * w, 0.0};
  }
  Fft(buf);
  const double binHz = sr / static_cast<double>(kFftSize);
  double num = 0.0, den = 0.0;
  for (size_t k = 1; k <= kFftSize / 2; ++k)
  {
    const double f = static_cast<double>(k) * binHz;
    if (f < 20.0 || f > 20000.0)
      continue;
    const double p = std::norm(buf[k]);
    num += f * p;
    den += p;
  }
  return den > 1.0e-20 ? num / den : 0.0;
}

ChannelPrint Fingerprint(const std::vector<double>& x, const std::vector<double>& input, double sr)
{
  ChannelPrint p;
  double sumSq = 0.0;
  double sum = 0.0;
  double peak = 0.0;
  for (double s : x)
  {
    sumSq += s * s;
    sum += s;
    peak = std::max(peak, std::abs(s));
  }
  const double count = static_cast<double>(std::max<size_t>(1, x.size()));
  p.rmsDb = AmpToDb(std::sqrt(sumSq / count));
  p.peakDb = AmpToDb(peak);
  p.crestDb = (p.rmsDb > kFloorDb && p.peakDb > kFloorDb) ? p.peakDb - p.rmsDb : 0.0;
  p.meanDc = sum / count;
  p.inputCorr = Correlation(x, input);

  // Welch power spectrum, Hann window, 50% overlap, zero-padded tail frame.
  std::vector<double> window(kFftSize);
  double sumW2 = 0.0;
  for (size_t i = 0; i < kFftSize; ++i)
  {
    window[i] = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(kFftSize));
    sumW2 += window[i] * window[i];
  }
  std::vector<double> power(kFftSize / 2 + 1, 0.0);
  std::vector<std::complex<double>> buf(kFftSize);
  size_t frames = 0;
  for (size_t start = 0; start < x.size(); start += kFftHop)
  {
    for (size_t i = 0; i < kFftSize; ++i)
    {
      const size_t idx = start + i;
      buf[i] = {idx < x.size() ? x[idx] * window[i] : 0.0, 0.0};
    }
    Fft(buf);
    for (size_t k = 0; k <= kFftSize / 2; ++k)
      power[k] += std::norm(buf[k]);
    ++frames;
    if (start + kFftSize >= x.size())
      break;
  }
  const double norm = 2.0 / (static_cast<double>(kFftSize) * sumW2 * static_cast<double>(std::max<size_t>(1, frames)));
  const double binHz = sr / static_cast<double>(kFftSize);
  double centroidNum = 0.0;
  double centroidDen = 0.0;
  for (int b = 0; b < kNumBands; ++b)
  {
    const double fc = 31.25 * std::pow(2.0, b);
    const double lo = fc / std::sqrt(2.0);
    const double hi = fc * std::sqrt(2.0);
    double bandPow = 0.0;
    for (size_t k = 1; k <= kFftSize / 2; ++k)
    {
      const double f = static_cast<double>(k) * binHz;
      if (f >= lo && f < hi)
        bandPow += power[k] * norm;
    }
    p.bandsDb[static_cast<size_t>(b)] = PowToDb(bandPow);
  }
  for (size_t k = 1; k <= kFftSize / 2; ++k)
  {
    const double f = static_cast<double>(k) * binHz;
    if (f < 20.0 || f > 20000.0)
      continue;
    centroidNum += f * power[k];
    centroidDen += power[k];
  }
  p.centroidHz = centroidDen > 0.0 ? centroidNum / centroidDen : 0.0;

  const size_t envFrame = EnvFrame(sr);
  for (size_t start = 0; start < x.size(); start += envFrame)
  {
    const size_t end = std::min(x.size(), start + envFrame);
    double e = 0.0;
    for (size_t i = start; i < end; ++i)
      e += x[i] * x[i];
    p.envDb.push_back(AmpToDb(std::sqrt(e / static_cast<double>(end - start))));
    p.centroidTrackHz.push_back(FrameCentroid(x, start, end, sr, buf));
  }
  return p;
}

StereoPrint StereoFingerprint(const std::vector<double>& l, const std::vector<double>& r)
{
  StereoPrint s;
  s.lrCorr = Correlation(l, r);
  double mid = 0.0, side = 0.0;
  for (size_t i = 0; i < l.size(); ++i)
  {
    const double m = 0.5 * (l[i] + r[i]);
    const double d = 0.5 * (l[i] - r[i]);
    mid += m * m;
    side += d * d;
  }
  const double cap = std::pow(10.0, kMidSideCapDb / 10.0);
  if (side * cap <= mid)
    s.midSideDb = kMidSideCapDb;
  else if (mid * cap <= side)
    s.midSideDb = -kMidSideCapDb;
  else
    s.midSideDb = 10.0 * std::log10(mid / side);
  return s;
}

// inputs: what fed the chain, per channel (one entry = every channel); empty = the DI.
Render MakeRender(std::string name, Tier tier, const std::vector<std::vector<double>>& channels,
                  const std::vector<std::vector<double>>& inputs = {}, double sr = kSampleRate)
{
  Render r;
  r.name = std::move(name);
  r.tier = tier;
  for (size_t c = 0; c < channels.size(); ++c)
  {
    const std::vector<double>& in = inputs.empty() ? Di() : inputs[std::min(c, inputs.size() - 1)];
    r.channels.push_back(Fingerprint(channels[c], in, sr));
  }
  if (channels.size() == 2)
    r.stereo = StereoFingerprint(channels[0], channels[1]);
  return r;
}

// ---------------------------------------------------------------------------
// Tolerances
// ---------------------------------------------------------------------------

struct Tolerance
{
  double rmsDb;
  double peakDb;
  double crestDb;
  double centroidRel;
  // By how far a band / envelope frame sits below the loudest one in the
  // reference: a quiet band carries the same absolute rounding noise as a loud
  // one, so its dB value moves more for the same error.
  double bandDb[3];
  double envDb[3];
  // Schema 2. Absolute on the DC mean (full scale 1.0) and on correlations (-1..1).
  double meanAbs;
  double corrAbs;
  // By |mid/side| in dB: under 30, 60, 90. Past 90 dB the render is mono and
  // only has to stay past 90.
  double midSideDb[3];
  // Relative, for envelope frames within 40 / 70 dB of the loudest; quieter ones
  // are not compared.
  double centroidTrackRel[2];
};

// Tight: NAM runs float Eigen GEMV whose SIMD summation order differs between
// AVX and NEON, so relative error is ~1e-6 (the existing sample-level rig golden
// holds 1e-4 RMSE across both CI platforms). That is ~1e-5 dB; 0.02-0.1 dB is
// three orders of magnitude of margin and still catches a 0.25 dB level move
// (rms) or a 1 dB voicing change in any one octave (bands).
// The schema-2 fields carry the same ~1e-6 relative error: the DC mean moves by
// ~1e-7 against a 1e-4 bound (fails a 1e-3 offset tenfold), a correlation by
// ~1e-6 against 0.01 (a polarity flip moves it by 2x its value), mid/side like
// the bands, and a 50 ms centroid by far under 0.5% where the frame is loud.
constexpr Tolerance kTight{
  0.02, 0.05, 0.07, 0.005, {0.05, 0.25, 1.0}, {0.1, 0.5, 2.0}, 1.0e-4, 0.01, {0.05, 0.25, 1.0}, {0.005, 0.02}};
// Decisive: a flipped WSOLA splice moves energy inside one grain. The rms bound
// still fails a 0.25 dB level change; bands, envelope, correlation and the
// centroid track allow the grain move.
constexpr Tolerance kDecisive{
  0.1, 0.5, 0.6, 0.02, {0.3, 1.0, 3.0}, {1.0, 3.0, 6.0}, 3.0e-4, 0.05, {0.3, 1.0, 3.0}, {0.05, 0.15}};

const Tolerance& TolFor(Tier t)
{
  return t == Tier::Decisive ? kDecisive : kTight;
}

// Divide last so the stored double is the one nearest the decimal and dumps short.
double Round(double v, double scale)
{
  return std::round(v * scale) / scale;
}

json ChannelToJson(const ChannelPrint& p)
{
  json j;
  j["rmsDb"] = Round(p.rmsDb, 1000.0);
  j["peakDb"] = Round(p.peakDb, 1000.0);
  j["crestDb"] = Round(p.crestDb, 1000.0);
  j["centroidHz"] = Round(p.centroidHz, 10.0);
  json bands = json::array();
  for (double b : p.bandsDb)
    bands.push_back(Round(b, 1000.0));
  j["bandsDb"] = bands;
  json env = json::array();
  for (double e : p.envDb)
    env.push_back(Round(e, 1000.0));
  j["envDb"] = env;
  j["meanDc"] = Round(p.meanDc, 1.0e7);
  j["inputCorr"] = Round(p.inputCorr, 1.0e5);
  json track = json::array();
  for (double c : p.centroidTrackHz)
    track.push_back(Round(c, 10.0));
  j["centroidTrackHz"] = track;
  return j;
}

json StereoToJson(const StereoPrint& s)
{
  json j;
  j["lrCorr"] = Round(s.lrCorr, 1.0e5);
  j["midSideDb"] = Round(s.midSideDb, 1000.0);
  return j;
}

struct Diff
{
  double ratio;
  std::string text;
};

std::string Fmt(double v, int precision = 3)
{
  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(precision);
  ss << v;
  return ss.str();
}

void CompareValue(std::vector<Diff>& out, const std::string& where, double ref, double now, double tol,
                  int precision = 3)
{
  if (ref <= kFloorDb + 1.0 && now <= kFloorDb + 1.0)
    return;
  const double delta = now - ref;
  const double ratio = std::abs(delta) / tol;
  out.push_back({ratio, where + ": ref " + Fmt(ref, precision) + ", now " + Fmt(now, precision) + " (delta "
                          + (delta >= 0 ? "+" : "") + Fmt(delta, precision) + ", tol " + Fmt(tol, precision) + ")"});
}

double TierTol(const double tiers[3], double belowDb, double tier0, double tier1, double tier2)
{
  if (belowDb < tier0)
    return tiers[0];
  if (belowDb < tier1)
    return tiers[1];
  if (belowDb < tier2)
    return tiers[2];
  return -1.0;
}

// A schema-2 field the reference must carry. Missing = a hand edit or a stale file.
const json* Field(std::vector<Diff>& out, const std::string& where, const json& obj, const char* key)
{
  if (obj.contains(key))
    return &obj.at(key);
  out.push_back({1e9, where + ": reference has no " + key + " (regenerate the group)"});
  return nullptr;
}

void CompareSchema2Channel(std::vector<Diff>& diffs, const std::string& at, const ChannelPrint& p, const json& r,
                           const Tolerance& tol)
{
  if (const json* v = Field(diffs, at, r, "meanDc"))
    CompareValue(diffs, at + " DC mean", v->get<double>(), p.meanDc, tol.meanAbs, 7);
  if (const json* v = Field(diffs, at, r, "inputCorr"))
    CompareValue(diffs, at + " input correlation", v->get<double>(), p.inputCorr, tol.corrAbs, 5);

  const json* track = Field(diffs, at, r, "centroidTrackHz");
  if (track == nullptr)
    return;
  const auto& env = r.at("envDb");
  if (track->size() != p.centroidTrackHz.size() || env.size() != p.centroidTrackHz.size())
  {
    diffs.push_back({1e9, at + ": centroid track length changed"});
    return;
  }
  double loudestFrame = kFloorDb;
  for (const auto& e : env)
    loudestFrame = std::max(loudestFrame, e.get<double>());
  for (size_t f = 0; f < env.size(); ++f)
  {
    const double below = loudestFrame - env[f].get<double>();
    const double rel = below < 40.0 ? tol.centroidTrackRel[0] : below < 70.0 ? tol.centroidTrackRel[1] : -1.0;
    if (rel < 0.0)
      continue;
    const double refHz = (*track)[f].get<double>();
    CompareValue(diffs, at + " centroid " + std::to_string(f * 50) + " ms Hz", refHz, p.centroidTrackHz[f],
                 std::max(2.0, refHz * rel), 1);
  }
}

void CompareStereo(std::vector<Diff>& diffs, const Render& now, const json& ref, const Tolerance& tol)
{
  const json* st = Field(diffs, now.name, ref, "stereo");
  if (st == nullptr)
    return;
  if (const json* v = Field(diffs, now.name, *st, "lrCorr"))
    CompareValue(diffs, now.name + " L/R correlation", v->get<double>(), now.stereo.lrCorr, tol.corrAbs, 5);
  const json* ms = Field(diffs, now.name, *st, "midSideDb");
  if (ms == nullptr)
    return;
  const double refMs = ms->get<double>();
  const double nowMs = now.stereo.midSideDb;
  const std::string where = now.name + " mid/side dB";
  if (std::abs(refMs) >= 90.0)
  {
    // Mono (or pure side) reference: fail only if the render stopped being that.
    if (std::abs(nowMs) < 90.0 || (refMs > 0.0) != (nowMs > 0.0))
      diffs.push_back({1.0 + std::abs(nowMs - refMs), where + ": ref " + Fmt(refMs) + ", now " + Fmt(nowMs)
                                                        + " (the render changed its stereo image)"});
    return;
  }
  CompareValue(diffs, where, refMs, nowMs, TierTol(tol.midSideDb, std::abs(refMs), 30.0, 60.0, 90.0));
}

// Every comparison, pass or fail, as ratio = |delta| / tolerance. legacyOnly
// compares only the schema-1 fields (used to prove a schema bump moved nothing).
std::vector<Diff> CompareRender(const Render& now, const json& ref, bool legacyOnly)
{
  std::vector<Diff> diffs;
  const Tolerance& tol = TolFor(now.tier);
  static const json kNoChannels;
  const json& channels = ref.is_array() ? ref : (ref.contains("channels") ? ref.at("channels") : kNoChannels);
  if (!channels.is_array() || channels.size() != now.channels.size())
  {
    diffs.push_back({1e9, now.name + ": channel count changed (ref "
                            + std::to_string(channels.is_array() ? channels.size() : 0) + ", now "
                            + std::to_string(now.channels.size()) + ")"});
    return diffs;
  }
  for (size_t c = 0; c < now.channels.size(); ++c)
  {
    const ChannelPrint& p = now.channels[c];
    const json& r = channels[c];
    const std::string at = now.name + " ch" + std::to_string(c);
    CompareValue(diffs, at + " rms dB", r.at("rmsDb").get<double>(), p.rmsDb, tol.rmsDb);
    CompareValue(diffs, at + " peak dB", r.at("peakDb").get<double>(), p.peakDb, tol.peakDb);
    const double refRms = r.at("rmsDb").get<double>();
    if (refRms > -100.0)
    {
      CompareValue(diffs, at + " crest dB", r.at("crestDb").get<double>(), p.crestDb, tol.crestDb);
      const double refCentroid = r.at("centroidHz").get<double>();
      const double relTol = std::max(1.0, refCentroid * tol.centroidRel);
      CompareValue(diffs, at + " centroid Hz", refCentroid, p.centroidHz, relTol);
    }

    const auto& bands = r.at("bandsDb");
    if (bands.size() != static_cast<size_t>(kNumBands))
    {
      diffs.push_back({1e9, at + ": band count changed"});
      continue;
    }
    double loudestBand = kFloorDb;
    for (const auto& b : bands)
      loudestBand = std::max(loudestBand, b.get<double>());
    for (int b = 0; b < kNumBands; ++b)
    {
      const double refDb = bands[static_cast<size_t>(b)].get<double>();
      const double t = TierTol(tol.bandDb, loudestBand - refDb, 30.0, 60.0, 90.0);
      if (t > 0.0)
        CompareValue(diffs, at + " band " + kBandNames[b] + " dB", refDb, p.bandsDb[static_cast<size_t>(b)], t);
    }

    const auto& env = r.at("envDb");
    if (env.size() != p.envDb.size())
    {
      diffs.push_back({1e9, at + ": envelope length changed (ref " + std::to_string(env.size()) + " frames, now "
                              + std::to_string(p.envDb.size()) + ")"});
      continue;
    }
    double loudestFrame = kFloorDb;
    for (const auto& e : env)
      loudestFrame = std::max(loudestFrame, e.get<double>());
    for (size_t f = 0; f < env.size(); ++f)
    {
      const double refDb = env[f].get<double>();
      const double t = TierTol(tol.envDb, loudestFrame - refDb, 40.0, 70.0, 100.0);
      if (t > 0.0)
        CompareValue(diffs, at + " env " + std::to_string(f * 50) + " ms dB", refDb, p.envDb[f], t);
    }

    if (!legacyOnly)
      CompareSchema2Channel(diffs, at, p, r, tol);
  }
  if (!legacyOnly && now.channels.size() == 2)
    CompareStereo(diffs, now, ref, tol);
  return diffs;
}

struct GroupResult
{
  std::vector<std::string> failures;
  Diff worst{0.0, ""};
  size_t unreferenced = 0;
};

// legacyOnly: renders with no reference are counted, not failed (a schema bump may add renders).
GroupResult CompareGroup(const std::vector<Render>& renders, const json& refRenders, bool legacyOnly)
{
  GroupResult result;
  for (const auto& r : renders)
  {
    if (!refRenders.contains(r.name))
    {
      ++result.unreferenced;
      if (!legacyOnly)
        result.failures.push_back(r.name + ": no reference (a new chain or rig)");
      continue;
    }
    for (auto& d : CompareRender(r, refRenders.at(r.name), legacyOnly))
    {
      if (d.ratio > result.worst.ratio)
        result.worst = d;
      if (d.ratio > 1.0)
        result.failures.push_back(d.text);
    }
  }
  for (auto it = refRenders.begin(); it != refRenders.end(); ++it)
  {
    const bool produced =
      std::any_of(renders.begin(), renders.end(), [&](const Render& r) { return r.name == it.key(); });
    if (!produced)
      result.failures.push_back(it.key() + ": reference exists but nothing renders it any more");
  }
  return result;
}

const char* const kHowToChange =
  "Never edit by hand. Run NeuralAmpModeler/scripts/regen-golden-renders.ps1 -Reason \"<sound change>\" "
  "-Groups <this group> and add a changelog.txt line containing that reason.";

std::string GroupFileText(const std::string& group, const std::string& reason, const std::vector<Render>& renders)
{
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"schema\": " << kSchema << ",\n";
  ss << "  \"group\": " << json(group).dump() << ",\n";
  ss << "  \"reason\": " << json(reason).dump() << ",\n";
  ss << "  \"howToChange\": " << json(kHowToChange).dump() << ",\n";
  ss << "  \"sampleRate\": " << static_cast<int>(kSampleRate) << ",\n";
  ss << "  \"blockSize\": " << kBlock << ",\n";
  ss << "  \"renders\": {\n";
  for (size_t i = 0; i < renders.size(); ++i)
  {
    const Render& r = renders[i];
    ss << "    " << json(r.name).dump() << ": {\n";
    ss << "      \"channels\": [\n";
    for (size_t c = 0; c < r.channels.size(); ++c)
      ss << "        " << ChannelToJson(r.channels[c]).dump() << (c + 1 < r.channels.size() ? ",\n" : "\n");
    ss << "      ]";
    if (r.channels.size() == 2)
      ss << ",\n      \"stereo\": " << StereoToJson(r.stereo).dump();
    ss << "\n    }" << (i + 1 < renders.size() ? ",\n" : "\n");
  }
  ss << "  }\n}\n";
  return ss.str();
}

const char* const kRegenHint =
  "run NeuralAmpModeler/scripts/regen-golden-renders.ps1 -Reason \"<what changed in the sound>\" -Groups <group> "
  "and add a changelog.txt line containing that reason";

// Regen mode only: the reason must be new, and never on CI.
void RequireRegenAllowed()
{
  const std::string ci = "Golden references are never regenerated on CI (CI / GITHUB_ACTIONS is set).";
  REQUIRE_MESSAGE((Env("CI").empty() && Env("GITHUB_ACTIONS").empty()), ci);
  const std::string reason = RegenReason();
  REQUIRE_MESSAGE(reason.size() >= kMinReasonLength, "VOLUM_GOLDEN_REASON must name the sound change");
  const std::string reused = "installer/changelog.txt already contains \"" + reason
                             + "\". A new sound needs a reason that is not in the changelog yet.";
  REQUIRE_MESSAGE(ReadFileText(ChangelogPath()).find(reason) == std::string::npos, reused);
  REQUIRE_MESSAGE(!RegenGroups().empty(), "VOLUM_GOLDEN_GROUPS must list the groups allowed to change");
}

// Regen mode only. True when the group file should be (re)written.
bool RegenShouldWrite(const std::string& group, bool moved, const std::string& why)
{
  if (!moved)
  {
    std::cout << "golden " << group << ": unchanged, kept" << std::endl;
    return false;
  }
  if (RegenGroups().count(group) == 0)
  {
    const std::string msg = "golden " + group + " moved (" + why
                            + ") but is not in -Groups, so it was NOT rewritten. If that change is intended, add it "
                              "to -Groups; otherwise something changed a sound you did not mean to touch.";
    FAIL_CHECK(msg);
    return false;
  }
  std::cout << "golden " << group << ": " << why << std::endl;
  return true;
}

void WriteGroupFile(const fs::path& path, const std::string& text)
{
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
  REQUIRE(out.good());
  std::cout << "golden: REWROTE " << path.string() << std::endl;
}

bool ReadGroupFile(const fs::path& path, json& existing)
{
  if (!fs::exists(path))
    return false;
  try
  {
    existing = json::parse(ReadFileText(path));
    return existing.is_object();
  }
  catch (const std::exception&)
  {
    return false;
  }
}

void CheckGroup(const std::string& group, const std::vector<Render>& renders)
{
  REQUIRE(!renders.empty());
  const fs::path path = GoldenDir() / (group + ".json");
  json existing;
  const bool haveExisting =
    ReadGroupFile(path, existing) && existing.contains("renders") && existing.at("renders").is_object();

  if (RegenRequested())
  {
    RequireRegenAllowed();
    bool moved = true;
    std::string why = "no reference yet";
    if (haveExisting)
    {
      const int schema = existing.value("schema", 0);
      if (schema == kSchema)
      {
        const GroupResult same = CompareGroup(renders, existing.at("renders"), false);
        moved = !same.failures.empty();
        if (moved)
          why = std::to_string(same.failures.size()) + " value(s) moved, e.g. " + same.failures.front();
      }
      else
      {
        why = "schema " + std::to_string(schema) + " -> " + std::to_string(kSchema);
        if (schema == 1)
        {
          const GroupResult legacy = CompareGroup(renders, existing.at("renders"), true);
          std::cout << "golden " << group << ": schema-1 fields vs the old file: worst " << Fmt(legacy.worst.ratio)
                    << " of tolerance, " << legacy.failures.size() << " out of tolerance, " << legacy.unreferenced
                    << " new render(s)" << (legacy.worst.text.empty() ? "" : " (worst at " + legacy.worst.text + ")")
                    << std::endl;
          for (size_t i = 0; i < std::min<size_t>(legacy.failures.size(), 10); ++i)
            std::cout << "  MOVED " << legacy.failures[i] << std::endl;
        }
      }
    }
    if (RegenShouldWrite(group, moved, why))
      WriteGroupFile(path, GroupFileText(group, RegenReason(), renders));
    return;
  }

  const std::string missing = "Missing golden reference " + path.string() + ": " + kRegenHint + ".";
  REQUIRE_MESSAGE(haveExisting, missing);
  const std::string schemaMoved = "Golden schema changed: " + std::string(kRegenHint) + ".";
  REQUIRE_MESSAGE(existing.value("schema", 0) == kSchema, schemaMoved);
  REQUIRE(existing.value("sampleRate", 0) == static_cast<int>(kSampleRate));
  REQUIRE(existing.value("blockSize", 0) == kBlock);

  const GroupResult result = CompareGroup(renders, existing.at("renders"), false);
  std::cout << "golden " << group << ": " << renders.size() << " renders, worst " << Fmt(result.worst.ratio)
            << " of tolerance" << (result.worst.text.empty() ? "" : " at " + result.worst.text) << std::endl;
  if (!result.failures.empty())
  {
    std::ostringstream msg;
    msg << "The sound of " << group << " changed (" << result.failures.size() << " value(s) out of tolerance vs "
        << path.string() << "):\n";
    const size_t shown = std::min<size_t>(result.failures.size(), 25);
    for (size_t i = 0; i < shown; ++i)
      msg << "  " << result.failures[i] << "\n";
    if (shown < result.failures.size())
      msg << "  ... and " << (result.failures.size() - shown) << " more\n";
    msg << "If this change is intended, " << kRegenHint << ".";
    FAIL_CHECK(msg.str());
  }
}

// ---------------------------------------------------------------------------
// SHA-256 (FIPS 180-4), for the .nam manifest.
// ---------------------------------------------------------------------------

class Sha256
{
public:
  void Update(const unsigned char* data, size_t len)
  {
    for (size_t i = 0; i < len; ++i)
    {
      mBuf[mBufLen++] = data[i];
      if (mBufLen == 64)
      {
        Block(mBuf);
        mBufLen = 0;
      }
    }
    mBits += static_cast<std::uint64_t>(len) * 8U;
  }

  std::string HexDigest()
  {
    const std::uint64_t bits = mBits;
    const unsigned char pad = 0x80;
    Update(&pad, 1);
    const unsigned char zero = 0;
    while (mBufLen != 56)
      Update(&zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i)
      len[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
    Update(len, 8);
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (std::uint32_t h : mH)
      for (int s = 28; s >= 0; s -= 4)
        out += hex[(h >> s) & 0xfU];
    return out;
  }

private:
  static std::uint32_t Rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

  void Block(const unsigned char* p)
  {
    static const std::uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (static_cast<std::uint32_t>(p[4 * i]) << 24) | (static_cast<std::uint32_t>(p[4 * i + 1]) << 16)
             | (static_cast<std::uint32_t>(p[4 * i + 2]) << 8) | static_cast<std::uint32_t>(p[4 * i + 3]);
    for (int i = 16; i < 64; ++i)
    {
      const std::uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = mH[0], b = mH[1], c = mH[2], d = mH[3], e = mH[4], f = mH[5], g = mH[6], h = mH[7];
    for (int i = 0; i < 64; ++i)
    {
      const std::uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
      const std::uint32_t ch = (e & f) ^ (~e & g);
      const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
      const std::uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = s0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    mH[0] += a;
    mH[1] += b;
    mH[2] += c;
    mH[3] += d;
    mH[4] += e;
    mH[5] += f;
    mH[6] += g;
    mH[7] += h;
  }

  std::uint32_t mH[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  unsigned char mBuf[64] = {};
  size_t mBufLen = 0;
  std::uint64_t mBits = 0;
};

std::string Sha256Hex(const std::string& bytes)
{
  Sha256 sha;
  sha.Update(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
  return sha.HexDigest();
}

// ---------------------------------------------------------------------------
// Chains
// ---------------------------------------------------------------------------

// Processes a mono buffer in host blocks. fn(block, n) returns the block's output.
std::vector<double> RunMono(const std::vector<double>& input, const std::function<double*(double*, int)>& fn)
{
  std::vector<double> out(input.size(), 0.0);
  std::vector<double> block(kBlock);
  for (size_t off = 0; off < input.size(); off += kBlock)
  {
    const int n = static_cast<int>(std::min<size_t>(kBlock, input.size() - off));
    std::copy(input.begin() + off, input.begin() + off + n, block.begin());
    const double* y = fn(block.data(), n);
    std::copy(y, y + n, out.begin() + off);
  }
  return out;
}

// The POST bus. Mono amp = the same signal on both channels; dual amp can pan
// the lanes apart, hence the L != R form.
std::vector<std::vector<double>> RunStereo(const std::vector<double>& inL, const std::vector<double>& inR,
                                           const std::function<double**(double**, int)>& fn)
{
  REQUIRE(inL.size() == inR.size());
  std::vector<std::vector<double>> out(2, std::vector<double>(inL.size(), 0.0));
  std::vector<double> l(kBlock), r(kBlock);
  for (size_t off = 0; off < inL.size(); off += kBlock)
  {
    const int n = static_cast<int>(std::min<size_t>(kBlock, inL.size() - off));
    std::copy(inL.begin() + off, inL.begin() + off + n, l.begin());
    std::copy(inR.begin() + off, inR.begin() + off + n, r.begin());
    double* io[2] = {l.data(), r.data()};
    double** y = fn(io, n);
    for (int c = 0; c < 2; ++c)
      std::copy(y[c], y[c] + n, out[static_cast<size_t>(c)].begin() + off);
  }
  return out;
}

std::vector<std::vector<double>> RunStereo(const std::vector<double>& input,
                                           const std::function<double**(double**, int)>& fn)
{
  return RunStereo(input, input, fn);
}

// What the loader does (VoLumLoader.inc.cpp): wrap in ResamplingNAM at the host rate.
std::unique_ptr<ResamplingNAM> LoadNam(const fs::path& path, double hostRate = kSampleRate)
{
  nam::activations::Activation::enable_fast_tanh();
  auto model = nam::get_dsp(path);
  REQUIRE_MESSAGE(model != nullptr, path.string());
  return std::make_unique<ResamplingNAM>(std::move(model), hostRate);
}

// Then pick the slice, then Reset at the production size.
void PrepareNam(ResamplingNAM& model, bool full, double hostRate = kSampleRate)
{
  model.SetSlimmableSize(full ? 1.0 : 0.0);
  model.Reset(hostRate, volum::dsp_staging::NamResetBlockSize(kBlock));
}

std::vector<double> RunNam(nam::DSP& model, const std::vector<double>& input)
{
  std::vector<NAM_SAMPLE> y(kBlock, 0.0);
  return RunMono(input, [&](double* x, int n) {
    NAM_SAMPLE* ip[1] = {x};
    NAM_SAMPLE* op[1] = {y.data()};
    model.process(ip, op, n);
    return y.data();
  });
}

struct PreSlotSettings
{
  double gainDb, bass, mid, midFreq, treble, levelDb;
};

PreSlotSettings DefaultPreSlot()
{
  const volum::VoLumAmpSettings d;
  return {d.preNam1Gain, d.preNam1Bass, d.preNam1Mid, d.preNam1MidFreq, d.preNam1Treble, d.preNam1Level};
}

// One PRE NAM slot as processPreSlot runs it (VoLumProcessBlock.inc.cpp):
// input gain -> capture -> pedal EQ -> output level with its mute floor.
std::vector<double> RunPreSlot(ResamplingNAM& pedal, const std::vector<double>& input, const PreSlotSettings& s)
{
  recursive_linear_filter::Level inputGain;
  recursive_linear_filter::Level outputGain;
  dsp::effect::VoLumPreEq eq;
  eq.Reset(kSampleRate, kBlock);
  std::vector<NAM_SAMPLE> y(kBlock, 0.0);
  return RunMono(input, [&](double* x, int n) {
    double* io[1] = {x};
    inputGain.SetParams(recursive_linear_filter::LevelParams(std::pow(10.0, s.gainDb / 20.0)));
    double** p = inputGain.Process(io, 1, static_cast<size_t>(n));
    pedal.process(p[0], y.data(), n);
    double* yp[1] = {y.data()};
    eq.SetParams(s.bass, s.mid, s.midFreq, s.treble);
    p = eq.Process(yp, 1, static_cast<size_t>(n));
    outputGain.SetParams(recursive_linear_filter::LevelParams(volum::DbToAmpWithMuteFloor(s.levelDb, kPreLevelMinDb)));
    return outputGain.Process(p, 1, static_cast<size_t>(n))[0];
  });
}

std::vector<fs::path> NamFiles(const fs::path& dir, const std::string& prefix)
{
  std::vector<fs::path> hits;
  for (const auto& e : fs::directory_iterator(dir))
  {
    const std::string name = e.path().filename().string();
    if (e.path().extension() == ".nam" && name.rfind(prefix, 0) == 0)
      hits.push_back(e.path());
  }
  std::sort(hits.begin(), hits.end());
  return hits;
}

std::string RigLabel(const fs::path& path)
{
  return path.parent_path().filename().string() + "/" + path.stem().string();
}

const fs::path& ReferenceAmpPath()
{
  static const fs::path p = RepoRoot() / "rigs" / "Ampete One" / "AMP-Ampt-1.nam";
  return p;
}

// Cab-like synthetic IR: damped resonances from 95 Hz to 4.2 kHz, 2048 taps.
std::vector<float> SyntheticCabIr()
{
  const double f[6] = {95.0, 180.0, 700.0, 1600.0, 2800.0, 4200.0};
  const double a[6] = {0.5, 0.6, 0.8, 0.7, 0.5, 0.25};
  const double tau[6] = {0.010, 0.008, 0.004, 0.0025, 0.0015, 0.0008};
  std::vector<double> h(2048, 0.0);
  double peak = 0.0;
  for (size_t n = 0; n < h.size(); ++n)
  {
    const double t = static_cast<double>(n) / kSampleRate;
    for (int k = 0; k < 6; ++k)
      h[n] += a[k] * std::exp(-t / tau[k]) * std::sin(2.0 * kPi * f[k] * t + 0.3 * k);
    peak = std::max(peak, std::abs(h[n]));
  }
  std::vector<float> ir(h.size());
  for (size_t n = 0; n < h.size(); ++n)
    ir[n] = static_cast<float>(h[n] / peak);
  return ir;
}

double Seconds(std::chrono::steady_clock::time_point t0)
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}
} // namespace

TEST_CASE("Golden renders: the synthetic DI itself")
{
  const auto& di = Di();
  const Render r = MakeRender("DI", Tier::Tight, {di});
  // A realistic single-coil/humbucker DI level, not a hot test tone.
  CHECK(r.channels[0].peakDb > -12.0);
  CHECK(r.channels[0].peakDb < -4.0);
  CheckGroup("input", {r});
}

TEST_CASE("Golden renders: every bundled amp in FULL and LITE")
{
  const auto t0 = std::chrono::steady_clock::now();
  std::vector<Render> renders;
  const auto& di = Di();
  for (const auto& amp : volum::kAmps)
  {
    const auto files = NamFiles(RepoRoot() / "rigs" / amp.folderName, "AMP-");
    const std::string none = std::string(amp.folderName) + " has no AMP-*.nam";
    REQUIRE_MESSAGE(!files.empty(), none);
    for (const auto& file : files)
    {
      auto model = LoadNam(file);
      for (const bool full : {true, false})
      {
        PrepareNam(*model, full);
        renders.push_back(MakeRender(RigLabel(file) + (full ? " FULL" : " LITE"), Tier::Tight, {RunNam(*model, di)}));
      }
    }
  }
  std::cout << "golden amps rendered in " << Seconds(t0) << " s" << std::endl;
  CheckGroup("amps", renders);
}

TEST_CASE("Golden renders: every amp's default speaker capture in FULL")
{
  // A fresh amp opens on this speaker (VoLumAmpSettings::speakerIdx), so these
  // captures are the sound most people hear first.
  const auto t0 = std::chrono::steady_clock::now();
  const std::string prefix = std::string(volum::kSpeakerPrefixes[volum::VoLumAmpSettings{}.speakerIdx]) + "-";
  REQUIRE(prefix == "V30-");
  std::vector<Render> renders;
  const auto& di = Di();
  for (const auto& amp : volum::kAmps)
  {
    const auto files = NamFiles(RepoRoot() / "rigs" / amp.folderName, prefix);
    const std::string none = std::string(amp.folderName) + " has no " + prefix + "*.nam";
    REQUIRE_MESSAGE(!files.empty(), none);
    for (const auto& file : files)
    {
      auto model = LoadNam(file);
      PrepareNam(*model, true);
      renders.push_back(MakeRender(RigLabel(file) + " FULL", Tier::Tight, {RunNam(*model, di)}));
    }
  }
  std::cout << "golden default speaker captures rendered in " << Seconds(t0) << " s" << std::endl;
  CheckGroup("default-speaker", renders);
}

TEST_CASE("Golden renders: ResamplingNAM at non-48 kHz host rates")
{
  // Every other render runs at the models' own 48 kHz, where ResamplingNAM passes
  // straight through. A 44.1 kHz or 96 kHz session goes through its Lanczos
  // resampler, which nothing else pins.
  std::vector<Render> renders;
  for (const double hostRate : {44100.0, 96000.0})
  {
    auto model = LoadNam(ReferenceAmpPath(), hostRate);
    PrepareNam(*model, true, hostRate);
    const auto di = BuildDi(hostRate);
    const std::string label =
      RigLabel(ReferenceAmpPath()) + " FULL, host " + (hostRate == 44100.0 ? "44.1" : "96") + " kHz";
    renders.push_back(MakeRender(label, Tier::Tight, {RunNam(*model, di)}, {di}, hostRate));
  }
  CheckGroup("host-rate", renders);
}

TEST_CASE("Golden renders: every PRE pedal alone and into the reference amp")
{
  const auto& di = Di();
  auto amp = LoadNam(ReferenceAmpPath());
  std::vector<Render> renders;
  const auto pedals = NamFiles(RepoRoot() / "rigs" / "PrePedals", "");
  REQUIRE(!pedals.empty());
  const std::string ampLabel = RigLabel(ReferenceAmpPath()) + " FULL";
  for (const auto& file : pedals)
  {
    auto pedal = LoadNam(file);
    PrepareNam(*pedal, true);
    renders.push_back(MakeRender(RigLabel(file) + " FULL", Tier::Tight, {RunNam(*pedal, di)}));

    // The production PRE slot at its defaults, into the amp.
    PrepareNam(*pedal, true);
    const auto slotOut = RunPreSlot(*pedal, di, DefaultPreSlot());
    PrepareNam(*amp, true);
    renders.push_back(MakeRender(RigLabel(file) + " FULL -> " + ampLabel, Tier::Tight, {RunNam(*amp, slotOut)}));

    PrepareNam(*pedal, false);
    renders.push_back(MakeRender(RigLabel(file) + " LITE", Tier::Tight, {RunNam(*pedal, di)}));
  }

  // Every slot stage off its default at once: drive, a non-flat EQ, and level.
  {
    auto pedal = LoadNam(pedals.front());
    PrepareNam(*pedal, true);
    const PreSlotSettings shaped{6.0, 7.0, 3.0, 1200.0, 6.5, -4.0};
    const auto slotOut = RunPreSlot(*pedal, di, shaped);
    PrepareNam(*amp, true);
    renders.push_back(
      MakeRender(RigLabel(pedals.front()) + " FULL, gain +6 dB, EQ 7/3 at 1200 Hz/6.5, level -4 dB -> " + ampLabel,
                 Tier::Tight, {RunNam(*amp, slotOut)}));
  }
  CheckGroup("pre-pedals", renders);
}

TEST_CASE("Golden renders: tone stack, cab IR and compressor")
{
  const auto& di = Di();
  std::vector<Render> renders;

  struct ToneSetting
  {
    const char* name;
    double bass, mid, treble;
  };
  const ToneSetting tones[] = {
    {"tone stack 5/5/5", 5.0, 5.0, 5.0},
    {"tone stack 10/0/10", 10.0, 0.0, 10.0},
    {"tone stack 7/2/8", 7.0, 2.0, 8.0},
    {"tone stack 2/9/3", 2.0, 9.0, 3.0},
  };
  for (const auto& t : tones)
  {
    dsp::tone_stack::BasicNamToneStack stack;
    stack.Reset(kSampleRate, kBlock);
    stack.SetParam("bass", t.bass);
    stack.SetParam("middle", t.mid);
    stack.SetParam("treble", t.treble);
    renders.push_back(MakeRender(t.name, Tier::Tight, {RunMono(di, [&](double* x, int n) {
                                   double* io[1] = {x};
                                   return stack.Process(io, 1, n)[0];
                                 })}));
  }

  dsp::ImpulseResponse::IRData irData;
  irData.mRawAudio = SyntheticCabIr();
  irData.mRawAudioSampleRate = kSampleRate;
  {
    dsp::ImpulseResponse ir(irData, kSampleRate);
    renders.push_back(MakeRender("cab IR (synthetic)", Tier::Tight, {RunMono(di, [&](double* x, int n) {
                                   double* io[1] = {x};
                                   return ir.Process(io, 1, static_cast<size_t>(n))[0];
                                 })}));
  }
  {
    dsp::ImpulseResponse ir(irData, kSampleRate);
    recursive_linear_filter::HighPass lowCut;
    recursive_linear_filter::LowPass highCut;
    const double trim = std::pow(10.0, 6.0 / 20.0);
    renders.push_back(
      MakeRender("cab IR (synthetic) +6 dB, 80 Hz - 6 kHz", Tier::Tight, {RunMono(di, [&](double* x, int n) {
                   double* io[1] = {x};
                   double** y = ir.Process(io, 1, static_cast<size_t>(n));
                   return volum::ApplyIrShapingLane(y, 1, n, kSampleRate, trim, 80.0, 6000.0, lowCut, highCut)[0];
                 })}));
  }

  {
    const volum::VoLumAmpSettings d;
    dsp::effect::VoLumCompressor comp;
    comp.Reset();
    renders.push_back(MakeRender("compressor defaults", Tier::Tight, {RunMono(di, [&](double* x, int n) {
                                   comp.SetParams(d.preCompAmount, d.preCompRatio, d.preCompAttack, d.preCompRelease,
                                                  1.0, d.preCompLevel, kSampleRate);
                                   double* io[1] = {x};
                                   return comp.Process(io, 1, static_cast<size_t>(n))[0];
                                 })}));
  }
  CheckGroup("tone-ir", renders);
}

TEST_CASE("Golden renders: pitch modes and characters")
{
  using dsp::effect::VoLumPitch;
  const auto& di = Di();
  const volum::VoLumAmpSettings d;
  struct PitchCase
  {
    const char* name;
    VoLumPitch::Mode mode;
    double semitones;
    VoLumPitch::Voicing voicing;
    VoLumPitch::Character character;
  };
  const PitchCase cases[] = {
    {"transpose INSTANT -12", VoLumPitch::Mode::Transpose, -12.0, VoLumPitch::Voicing::Modern,
     VoLumPitch::Character::Instant},
    {"transpose INSTANT +7", VoLumPitch::Mode::Transpose, 7.0, VoLumPitch::Voicing::Modern,
     VoLumPitch::Character::Instant},
    {"transpose POLY -12", VoLumPitch::Mode::Transpose, -12.0, VoLumPitch::Voicing::Modern,
     VoLumPitch::Character::Poly},
    {"transpose POLY +7", VoLumPitch::Mode::Transpose, 7.0, VoLumPitch::Voicing::Modern, VoLumPitch::Character::Poly},
    {"octaver MODERN", VoLumPitch::Mode::Octaver, 0.0, VoLumPitch::Voicing::Modern, VoLumPitch::Character::Instant},
    {"octaver VINTAGE", VoLumPitch::Mode::Octaver, 0.0, VoLumPitch::Voicing::Vintage, VoLumPitch::Character::Instant},
  };
  std::vector<Render> renders;
  for (const auto& c : cases)
  {
    VoLumPitch pitch;
    pitch.Configure(kSampleRate, kBlock);
    renders.push_back(MakeRender(c.name, Tier::Decisive, {RunMono(di, [&](double* x, int n) {
                                   pitch.SetParams(c.mode, c.semitones, d.prePitchMix, d.prePitchOctDown,
                                                   d.prePitchOctUp, d.prePitchDry, c.voicing, d.prePitchLevel,
                                                   c.character);
                                   double* io[1] = {x};
                                   return pitch.Process(io, 1, static_cast<size_t>(n))[0];
                                 })}));
  }
  CheckGroup("pitch", renders);
}

TEST_CASE("Golden renders: chorus modes at shipped defaults")
{
  const auto input = DiWithTail();
  std::vector<Render> renders;
  for (int mode = 0; mode < volum::kVoLumChorusModeCount; ++mode)
  {
    const auto& row = volum::kVoLumChorusModeDefaults[mode];
    volum::ChorusDSP chorus;
    chorus.Prepare(kSampleRate, kBlock, 2);
    chorus.Reset();
    renders.push_back(MakeRender(
      std::string("chorus ") + volum::VoLumChorusModeName(mode), Tier::Tight, RunStereo(input, [&](double** io, int n) {
        chorus.SetParams(row.rate, row.depth, row.tone, row.width, row.mix, mode, kSampleRate);
        chorus.Process(io, 2, n);
        return io;
      })));
  }
  CheckGroup("chorus", renders);
}

TEST_CASE("Golden renders: delay modes at shipped defaults")
{
  const auto input = DiWithTail();
  const volum::VoLumAmpSettings d;
  struct DelayCase
  {
    const char* name;
    int mode;
    bool pingPong;
  };
  const DelayCase cases[] = {
    {"delay DIGITAL", volum::kVoLumDelayModeDigital, false},
    {"delay ANALOG", volum::kVoLumDelayModeAnalog, false},
    {"delay REVERSE", volum::kVoLumDelayModeReverse, false},
    {"delay DIGITAL ping-pong", volum::kVoLumDelayModeDigital, true},
  };
  std::vector<Render> renders;
  for (const auto& c : cases)
  {
    const auto& row = d.postDelayModes[c.mode];
    dsp::effect::Delay delay;
    delay.Prepare(2, kBlock, kSampleRate);
    delay.Reset();
    renders.push_back(MakeRender(c.name, Tier::Tight, RunStereo(input, [&](double** io, int n) {
                                   delay.SetParams(row.time, row.feedback, row.mix, c.mode, kSampleRate, row.tone,
                                                   row.age, c.pingPong);
                                   return delay.Process(io, 2, static_cast<size_t>(n));
                                 })));
  }
  CheckGroup("delay", renders);
}

TEST_CASE("Golden renders: reverb modes at shipped defaults")
{
  const auto input = DiWithTail();
  const volum::VoLumAmpSettings d;
  struct ReverbCase
  {
    std::string name;
    int mode;
    int subMode;
    double mix, decay, tone, preDelay, shimmer;
  };
  std::vector<ReverbCase> cases;
  for (int m : {volum::kVoLumReverbModeHall, volum::kVoLumReverbModePlate})
  {
    const auto& row = d.postReverbModes[m];
    cases.push_back({m == volum::kVoLumReverbModeHall ? "reverb HALL" : "reverb PLATE", m, row.subMode, row.mix,
                     row.decay, row.tone, row.preDelay, row.shimmer});
  }
  const char* subNames[3] = {"HALO", "SHIMMER", "BLOOM"};
  for (int s = 0; s < 3; ++s)
  {
    const auto& row = d.postOktaverbSubModes[s];
    cases.push_back({std::string("reverb OKTAVERB ") + subNames[s], volum::kVoLumReverbModeOktaverb, s, row.mix,
                     row.decay, row.tone, row.preDelay, row.shimmer});
  }
  std::vector<Render> renders;
  auto render = [&](const ReverbCase& c, const std::string& name, const std::vector<double>& inL,
                    const std::vector<double>& inR) {
    dsp::effect::Reverb reverb;
    reverb.Prepare(2, kBlock, kSampleRate);
    reverb.Reset();
    renders.push_back(MakeRender(name, Tier::Tight,
                                 RunStereo(inL, inR,
                                           [&](double** io, int n) {
                                             reverb.SetParams(c.mix, c.decay, c.tone, c.preDelay, c.shimmer, c.mode,
                                                              kSampleRate, c.subMode);
                                             return reverb.Process(io, 2, static_cast<size_t>(n));
                                           }),
                                 {inL, inR}));
  };
  for (const auto& c : cases)
    render(c, c.name, input, input);

  // A dual-amp pan puts different signals on L and R: here R is the DI 7.3 ms
  // late at -6 dB, so a swapped or cross-fed channel shows up.
  std::vector<double> right(input.size(), 0.0);
  for (size_t i = 350; i < input.size(); ++i)
    right[i] = 0.5 * input[i - 350];
  render(cases.front(), cases.front().name + ", L != R input", input, right);
  CheckGroup("reverb", renders);
}

TEST_CASE("Golden renders: tremolo modes at shipped defaults")
{
  const auto input = DiWithTail();
  const volum::VoLumAmpSettings d;
  const char* names[volum::kVoLumTremoloModeCount] = {"tremolo OPTICAL", "tremolo BIAS", "tremolo HARMONIC"};
  std::vector<Render> renders;
  for (int mode = 0; mode < volum::kVoLumTremoloModeCount; ++mode)
  {
    const auto& row = d.postTremoloModes[mode];
    volum::TremoloDSP tremolo;
    tremolo.Prepare(kSampleRate, kBlock, 2);
    tremolo.Reset();
    renders.push_back(MakeRender(names[mode], Tier::Tight, RunStereo(input, [&](double** io, int n) {
                                   tremolo.SetParams(row.rate, volum::VoLumTremoloDepthKnobToInternal(row.depth),
                                                     row.shape, row.mix, row.crossover, mode, kSampleRate);
                                   tremolo.Process(io, 2, n);
                                   return io;
                                 })));
  }
  CheckGroup("tremolo", renders);
}

TEST_CASE("Golden renders: SHA-256 of every bundled .nam file")
{
  // The renders cover the AMP- and default-speaker captures; this covers every
  // other .nam that ships (G12/G65 speakers, anything new). CRs are dropped
  // before hashing: .nam is JSON, and a Windows checkout may convert newlines.
  REQUIRE(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  REQUIRE(Sha256Hex(std::string(1000, 'a')) == "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");

  const fs::path rigs = RepoRoot() / "rigs";
  std::map<std::string, std::string> now;
  for (const auto& e : fs::recursive_directory_iterator(rigs))
  {
    if (!e.is_regular_file() || e.path().extension() != ".nam")
      continue;
    std::string bytes = ReadFileText(e.path());
    bytes.erase(std::remove(bytes.begin(), bytes.end(), '\r'), bytes.end());
    now[fs::relative(e.path(), rigs).generic_string()] = Sha256Hex(bytes);
  }
  REQUIRE(now.size() > 200);

  const std::string group = "nam-files";
  const fs::path path = GoldenDir() / (group + ".json");
  json existing;
  const bool haveExisting = ReadGroupFile(path, existing) && existing.value("schema", 0) == kNamManifestSchema
                            && existing.contains("files") && existing.at("files").is_object();
  std::vector<std::string> changes;
  if (haveExisting)
  {
    const json& ref = existing.at("files");
    for (const auto& [file, sha] : now)
    {
      if (!ref.contains(file))
        changes.push_back(file + ": new .nam with no reference");
      else if (ref.at(file).get<std::string>() != sha)
        changes.push_back(file + ": contents changed");
    }
    for (auto it = ref.begin(); it != ref.end(); ++it)
      if (now.count(it.key()) == 0)
        changes.push_back(it.key() + ": reference exists but the file is gone");
  }

  if (RegenRequested())
  {
    RequireRegenAllowed();
    const bool moved = !haveExisting || !changes.empty();
    const std::string why =
      !haveExisting
        ? "no reference yet"
        : (changes.empty() ? "" : std::to_string(changes.size()) + " file(s) changed, e.g. " + changes.front());
    if (!RegenShouldWrite(group, moved, why))
      return;
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schema\": " << kNamManifestSchema << ",\n";
    ss << "  \"group\": " << json(group).dump() << ",\n";
    ss << "  \"reason\": " << json(RegenReason()).dump() << ",\n";
    ss << "  \"howToChange\": " << json(kHowToChange).dump() << ",\n";
    ss << "  \"files\": {\n";
    size_t i = 0;
    for (const auto& [file, sha] : now)
      ss << "    " << json(file).dump() << ": " << json(sha).dump() << (++i < now.size() ? ",\n" : "\n");
    ss << "  }\n}\n";
    WriteGroupFile(path, ss.str());
    return;
  }

  const std::string missing = "Missing or stale golden reference " + path.string() + ": " + kRegenHint + ".";
  REQUIRE_MESSAGE(haveExisting, missing);
  std::cout << "golden " << group << ": " << now.size() << " files, " << changes.size() << " changed" << std::endl;
  if (!changes.empty())
  {
    std::ostringstream msg;
    msg << "Bundled .nam files changed (" << changes.size() << "):\n";
    for (size_t i = 0; i < std::min<size_t>(changes.size(), 25); ++i)
      msg << "  " << changes[i] << "\n";
    msg << "A different capture is a different sound. If intended, " << kRegenHint << ".";
    FAIL_CHECK(msg.str());
  }
}

TEST_CASE("Golden render references: every regeneration reason is in the changelog")
{
  // Script-enforced on Windows too (check-golden-changelog.ps1); here so macOS CI
  // enforces it without PowerShell.
  REQUIRE(fs::is_directory(GoldenDir()));
  const std::string changelog = ReadFileText(ChangelogPath());
  REQUIRE(!changelog.empty());
  int files = 0;
  for (const auto& e : fs::directory_iterator(GoldenDir()))
  {
    if (e.path().extension() != ".json")
      continue;
    ++files;
    CAPTURE(e.path().string());
    const json j = json::parse(ReadFileText(e.path()));
    const std::string reason = j.value("reason", "");
    const std::string tooShort = e.path().filename().string() + " has no regeneration reason (or one under "
                                 + std::to_string(kMinReasonLength) + " characters)";
    CHECK_MESSAGE(reason.size() >= kMinReasonLength, tooShort);
    const std::string notLogged = e.path().filename().string() + " was regenerated for \"" + reason
                                  + "\", but no line in installer/changelog.txt contains that text. Add a changelog "
                                    "line naming the sound change.";
    CHECK_MESSAGE(changelog.find(reason) != std::string::npos, notLogged);
  }
  CHECK(files > 0);
}

TEST_CASE("Golden render references: the rendered POST rows are the live effect-settings defaults")
{
  // The POST renders read VoLumAmpSettings rows; a fresh session seeds the live
  // pedals from VoLumEffectSettings. If the two drift, the goldens pin a sound
  // nobody hears.
  const volum::VoLumAmpSettings amp;
  const volum::VoLumEffectSettings fx;
  for (int m = 0; m < volum::kVoLumDelayModeCount; ++m)
  {
    CAPTURE(m);
    const auto &a = amp.postDelayModes[m], &e = fx.delayModes[m];
    CHECK(a.time == e.time);
    CHECK(a.feedback == e.feedback);
    CHECK(a.mix == e.mix);
    CHECK(a.tone == e.tone);
    CHECK(a.age == e.age);
    CHECK(a.pingPong == e.pingPong);
  }
  for (int m = 0; m < volum::kVoLumReverbModeCount; ++m)
  {
    CAPTURE(m);
    const auto &a = amp.postReverbModes[m], &e = fx.reverbModes[m];
    CHECK(a.mix == e.mix);
    CHECK(a.decay == e.decay);
    CHECK(a.tone == e.tone);
    CHECK(a.preDelay == e.preDelay);
    CHECK(a.shimmer == e.shimmer);
    CHECK(a.subMode == e.subMode);
  }
  for (int s = 0; s < 3; ++s)
  {
    CAPTURE(s);
    const auto &a = amp.postOktaverbSubModes[s], &e = fx.oktaverbSubModes[s];
    CHECK(a.mix == e.mix);
    CHECK(a.decay == e.decay);
    CHECK(a.tone == e.tone);
    CHECK(a.preDelay == e.preDelay);
    CHECK(a.shimmer == e.shimmer);
  }
  for (int m = 0; m < volum::kVoLumTremoloModeCount; ++m)
  {
    CAPTURE(m);
    const auto &a = amp.postTremoloModes[m], &e = fx.tremoloModes[m];
    CHECK(a.rate == e.rate);
    CHECK(a.depth == e.depth);
    CHECK(a.shape == e.shape);
    CHECK(a.mix == e.mix);
    CHECK(a.crossover == e.crossover);
  }
  for (int m = 0; m < volum::kVoLumChorusModeCount; ++m)
  {
    CAPTURE(m);
    const auto& k = volum::kVoLumChorusModeDefaults[m];
    for (const auto* row : {&amp.postChorusModes[m], &fx.chorusModes[m]})
    {
      CHECK(row->rate == k.rate);
      CHECK(row->depth == k.depth);
      CHECK(row->tone == k.tone);
      CHECK(row->width == k.width);
      CHECK(row->mix == k.mix);
    }
  }
}
