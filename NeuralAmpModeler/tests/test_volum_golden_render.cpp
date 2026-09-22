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
#include "../VoLumPitchShifter.h"
#include "../VoLumPreEffects.h"
#include "../VoLumTremolo.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"

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
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Sound fingerprints of every chain VoLum ships, rendered from one fixed
// synthetic guitar DI and compared against tests/golden/*.json.
//
// A fingerprint is octave-band RMS, peak, crest, spectral centroid and a 50 ms
// RMS envelope per channel, not a sample hash, so the same references hold on
// MSVC x64 and Apple clang arm64. The tolerances catch a 0.25 dB level move or a
// filter/voicing change, and they are fixed here in code: regenerating cannot
// loosen them.
//
// A plain run never writes a reference. regen-golden-renders.ps1 sets
// VOLUM_GOLDEN_REGEN=1 and VOLUM_GOLDEN_REASON; a group file is rewritten only
// when its fingerprints moved, and it records that reason. The changelog case
// below fails until the reason appears in installer/changelog.txt, so a sound
// change cannot land without a line naming it.

namespace
{
namespace fs = std::filesystem;
using nlohmann::json;

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 128;
constexpr double kPi = 3.14159265358979323846;
constexpr size_t kDiFrames = 96000; // 2.0 s
constexpr size_t kPostTailFrames = 48000; // 1.0 s of silence so POST tails are measured
constexpr size_t kFftSize = 4096;
constexpr size_t kFftHop = kFftSize / 2;
constexpr size_t kEnvFrame = 2400; // 50 ms
constexpr int kNumBands = 10;
constexpr double kFloorDb = -140.0;
constexpr int kSchema = 1;
constexpr size_t kMinReasonLength = 12;
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

bool RegenRequested()
{
  const char* v = std::getenv("VOLUM_GOLDEN_REGEN");
  return v != nullptr && std::string(v) == "1";
}

std::string RegenReason()
{
  const char* v = std::getenv("VOLUM_GOLDEN_REASON");
  return v ? std::string(v) : std::string();
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

void AddNote(std::vector<double>& out, const Note& n, std::uint32_t seed)
{
  const size_t begin = static_cast<size_t>(std::llround(n.start * kSampleRate));
  const size_t end = std::min(out.size(), static_cast<size_t>(std::llround((n.stop + 0.05) * kSampleRate)));
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
    const size_t hEnd = std::min(end, begin + static_cast<size_t>(tau * 18.0 * kSampleRate));
    for (size_t i = begin; i < hEnd; ++i)
    {
      const double t = static_cast<double>(i - begin) / kSampleRate;
      double env = std::exp(-t / tau);
      if (t > stopT)
        env *= std::exp(-(t - stopT) / 0.006);
      const double attack = t < 0.0004 ? t / 0.0004 : 1.0;
      out[i] += a * env * attack * std::sin(2.0 * kPi * fh * t + phase);
    }
  }

  std::uint32_t state = seed;
  double prev = 0.0;
  const size_t noiseEnd = std::min(end, begin + static_cast<size_t>(0.004 * kSampleRate));
  for (size_t i = begin; i < noiseEnd; ++i)
  {
    state = state * 1664525U + 1013904223U;
    const double white = static_cast<double>((state >> 8) & 0x00ffffffU) / static_cast<double>(0x00ffffffU) * 2.0 - 1.0;
    const double t = static_cast<double>(i - begin) / kSampleRate;
    const double burst = white * n.amp * 0.8 * std::exp(-t / 0.0008);
    out[i] += 0.5 * (burst - prev);
    prev = burst;
  }
}

std::vector<double> BuildDi()
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

  std::vector<double> di(kDiFrames, 0.0);
  for (size_t i = 0; i < notes.size(); ++i)
    AddNote(di, notes[i], 0x51f15eedU + static_cast<std::uint32_t>(i) * 7919U);
  for (auto& s : di)
    s = static_cast<double>(static_cast<float>(s));
  return di;
}

const std::vector<double>& Di()
{
  static const std::vector<double> di = BuildDi();
  return di;
}

std::vector<double> DiWithTail()
{
  std::vector<double> x = Di();
  x.resize(kDiFrames + kPostTailFrames, 0.0);
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

ChannelPrint Fingerprint(const std::vector<double>& x)
{
  ChannelPrint p;
  double sumSq = 0.0;
  double peak = 0.0;
  for (double s : x)
  {
    sumSq += s * s;
    peak = std::max(peak, std::abs(s));
  }
  p.rmsDb = AmpToDb(std::sqrt(sumSq / static_cast<double>(std::max<size_t>(1, x.size()))));
  p.peakDb = AmpToDb(peak);
  p.crestDb = (p.rmsDb > kFloorDb && p.peakDb > kFloorDb) ? p.peakDb - p.rmsDb : 0.0;

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
  const double binHz = kSampleRate / static_cast<double>(kFftSize);
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

  for (size_t start = 0; start < x.size(); start += kEnvFrame)
  {
    const size_t end = std::min(x.size(), start + kEnvFrame);
    double e = 0.0;
    for (size_t i = start; i < end; ++i)
      e += x[i] * x[i];
    p.envDb.push_back(AmpToDb(std::sqrt(e / static_cast<double>(end - start))));
  }
  return p;
}

Render MakeRender(std::string name, Tier tier, const std::vector<std::vector<double>>& channels)
{
  Render r;
  r.name = std::move(name);
  r.tier = tier;
  for (const auto& c : channels)
    r.channels.push_back(Fingerprint(c));
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
};

// Tight: NAM runs float Eigen GEMV whose SIMD summation order differs between
// AVX and NEON, so relative error is ~1e-6 (the existing sample-level rig golden
// holds 1e-4 RMSE across both CI platforms). That is ~1e-5 dB; 0.02-0.1 dB is
// three orders of magnitude of margin and still catches a 0.25 dB level move
// (rms) or a 1 dB voicing change in any one octave (bands).
constexpr Tolerance kTight{0.02, 0.05, 0.07, 0.005, {0.05, 0.25, 1.0}, {0.1, 0.5, 2.0}};
// Decisive: a flipped WSOLA splice moves energy inside one grain. The rms bound
// still fails a 0.25 dB level change; bands and envelope allow the grain move.
constexpr Tolerance kDecisive{0.1, 0.5, 0.6, 0.02, {0.3, 1.0, 3.0}, {1.0, 3.0, 6.0}};

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
  return j;
}

struct Diff
{
  double ratio;
  std::string text;
};

std::string Fmt(double v)
{
  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(3);
  ss << v;
  return ss.str();
}

void CompareValue(std::vector<Diff>& out, const std::string& where, double ref, double now, double tol)
{
  if (ref <= kFloorDb + 1.0 && now <= kFloorDb + 1.0)
    return;
  const double delta = now - ref;
  const double ratio = std::abs(delta) / tol;
  out.push_back({ratio, where + ": ref " + Fmt(ref) + ", now " + Fmt(now) + " (delta " + (delta >= 0 ? "+" : "")
                          + Fmt(delta) + ", tol " + Fmt(tol) + ")"});
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

// Every comparison, pass or fail, as ratio = |delta| / tolerance.
std::vector<Diff> CompareRender(const Render& now, const json& ref)
{
  std::vector<Diff> diffs;
  const Tolerance& tol = TolFor(now.tier);
  if (!ref.is_array() || ref.size() != now.channels.size())
  {
    diffs.push_back({1e9, now.name + ": channel count changed (ref " + std::to_string(ref.is_array() ? ref.size() : 0)
                            + ", now " + std::to_string(now.channels.size()) + ")"});
    return diffs;
  }
  for (size_t c = 0; c < now.channels.size(); ++c)
  {
    const ChannelPrint& p = now.channels[c];
    const json& r = ref[c];
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
  }
  return diffs;
}

struct GroupResult
{
  std::vector<std::string> failures;
  Diff worst{0.0, ""};
};

GroupResult CompareGroup(const std::vector<Render>& renders, const json& refRenders)
{
  GroupResult result;
  for (const auto& r : renders)
  {
    if (!refRenders.contains(r.name))
    {
      result.failures.push_back(r.name + ": no reference (a new chain or rig)");
      continue;
    }
    for (auto& d : CompareRender(r, refRenders.at(r.name)))
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

std::string GroupFileText(const std::string& group, const std::string& reason, const std::vector<Render>& renders)
{
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"schema\": " << kSchema << ",\n";
  ss << "  \"group\": " << json(group).dump() << ",\n";
  ss << "  \"reason\": " << json(reason).dump() << ",\n";
  ss << "  \"howToChange\": "
     << json(
          "Never edit by hand. Run NeuralAmpModeler/scripts/regen-golden-renders.ps1 -Reason \"<sound change>\" "
          "and add a changelog.txt line containing that reason.")
          .dump()
     << ",\n";
  ss << "  \"sampleRate\": " << static_cast<int>(kSampleRate) << ",\n";
  ss << "  \"blockSize\": " << kBlock << ",\n";
  ss << "  \"renders\": {\n";
  for (size_t i = 0; i < renders.size(); ++i)
  {
    ss << "    " << json(renders[i].name).dump() << ": [\n";
    for (size_t c = 0; c < renders[i].channels.size(); ++c)
      ss << "      " << ChannelToJson(renders[i].channels[c]).dump()
         << (c + 1 < renders[i].channels.size() ? ",\n" : "\n");
    ss << "    ]" << (i + 1 < renders.size() ? ",\n" : "\n");
  }
  ss << "  }\n}\n";
  return ss.str();
}

void CheckGroup(const std::string& group, const std::vector<Render>& renders)
{
  REQUIRE(!renders.empty());
  const fs::path path = GoldenDir() / (group + ".json");
  const std::string regenHint =
    "run NeuralAmpModeler/scripts/regen-golden-renders.ps1 -Reason \"<what changed in the sound>\" and add a "
    "changelog.txt line containing that reason";

  json existing;
  bool haveExisting = false;
  if (fs::exists(path))
  {
    try
    {
      existing = json::parse(ReadFileText(path));
      haveExisting = existing.contains("renders") && existing.at("renders").is_object();
    }
    catch (const std::exception&)
    {
      haveExisting = false;
    }
  }

  if (RegenRequested())
  {
    const std::string reason = RegenReason();
    REQUIRE_MESSAGE(reason.size() >= kMinReasonLength, "VOLUM_GOLDEN_REASON must name the sound change");
    if (haveExisting && existing.value("schema", 0) == kSchema)
    {
      const GroupResult same = CompareGroup(renders, existing.at("renders"));
      if (same.failures.empty())
      {
        std::cout << "golden " << group << ": unchanged, kept (reason stays \"" << existing.value("reason", "") << "\")"
                  << std::endl;
        return;
      }
      std::cout << "golden " << group << ": " << same.failures.size() << " value(s) moved, e.g. "
                << same.failures.front() << std::endl;
    }
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << GroupFileText(group, reason, renders);
    REQUIRE(out.good());
    std::cout << "golden " << group << ": REWROTE " << path.string() << " (" << renders.size() << " renders)"
              << std::endl;
    return;
  }

  const std::string missing = "Missing golden reference " + path.string() + ": " + regenHint + ".";
  REQUIRE_MESSAGE(haveExisting, missing);
  const std::string schemaMoved = "Golden schema changed: " + regenHint + ".";
  REQUIRE_MESSAGE(existing.value("schema", 0) == kSchema, schemaMoved);
  REQUIRE(existing.value("sampleRate", 0) == static_cast<int>(kSampleRate));
  REQUIRE(existing.value("blockSize", 0) == kBlock);

  const GroupResult result = CompareGroup(renders, existing.at("renders"));
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
    msg << "If this change is intended, " << regenHint << ".";
    FAIL_CHECK(msg.str());
  }
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

// The POST bus: the mono amp signal on both channels.
std::vector<std::vector<double>> RunStereo(const std::vector<double>& input,
                                           const std::function<double**(double**, int)>& fn)
{
  std::vector<std::vector<double>> out(2, std::vector<double>(input.size(), 0.0));
  std::vector<double> l(kBlock), r(kBlock);
  for (size_t off = 0; off < input.size(); off += kBlock)
  {
    const int n = static_cast<int>(std::min<size_t>(kBlock, input.size() - off));
    std::copy(input.begin() + off, input.begin() + off + n, l.begin());
    std::copy(input.begin() + off, input.begin() + off + n, r.begin());
    double* io[2] = {l.data(), r.data()};
    double** y = fn(io, n);
    for (int c = 0; c < 2; ++c)
      std::copy(y[c], y[c] + n, out[static_cast<size_t>(c)].begin() + off);
  }
  return out;
}

std::unique_ptr<nam::DSP> LoadNam(const fs::path& path)
{
  nam::activations::Activation::enable_fast_tanh();
  auto model = nam::get_dsp(path);
  REQUIRE_MESSAGE(model != nullptr, path.string());
  return model;
}

// What the loader does: pick the slice, then Reset at the production size.
void PrepareNam(nam::DSP& model, bool full)
{
  if (auto* slim = dynamic_cast<nam::SlimmableModel*>(&model))
    slim->SetSlimmableSize(full ? 1.0 : 0.0);
  model.ResetAndPrewarm(kSampleRate, volum::dsp_staging::NamResetBlockSize(kBlock));
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
  std::cout << "golden amps rendered in "
            << std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() << " s" << std::endl;
  CheckGroup("amps", renders);
}

TEST_CASE("Golden renders: every PRE pedal alone and into the reference amp")
{
  const auto& di = Di();
  auto amp = LoadNam(ReferenceAmpPath());
  const volum::VoLumAmpSettings defaults;
  std::vector<Render> renders;
  const auto pedals = NamFiles(RepoRoot() / "rigs" / "PrePedals", "");
  REQUIRE(!pedals.empty());
  for (const auto& file : pedals)
  {
    auto pedal = LoadNam(file);
    PrepareNam(*pedal, true);
    const auto pedalOut = RunNam(*pedal, di);
    renders.push_back(MakeRender(RigLabel(file) + " FULL", Tier::Tight, {pedalOut}));

    // Production PRE slot at its defaults: capture -> pedal EQ -> amp.
    dsp::effect::VoLumPreEq eq;
    eq.Reset(kSampleRate, kBlock);
    const auto eqOut = RunMono(pedalOut, [&](double* x, int n) {
      eq.SetParams(defaults.preNam1Bass, defaults.preNam1Mid, defaults.preNam1MidFreq, defaults.preNam1Treble);
      double* io[1] = {x};
      return eq.Process(io, 1, static_cast<size_t>(n))[0];
    });
    PrepareNam(*amp, true);
    renders.push_back(MakeRender(
      RigLabel(file) + " FULL -> " + RigLabel(ReferenceAmpPath()) + " FULL", Tier::Tight, {RunNam(*amp, eqOut)}));

    PrepareNam(*pedal, false);
    renders.push_back(MakeRender(RigLabel(file) + " LITE", Tier::Tight, {RunNam(*pedal, di)}));
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
  for (const auto& c : cases)
  {
    dsp::effect::Reverb reverb;
    reverb.Prepare(2, kBlock, kSampleRate);
    reverb.Reset();
    renders.push_back(MakeRender(c.name, Tier::Tight, RunStereo(input, [&](double** io, int n) {
                                   reverb.SetParams(
                                     c.mix, c.decay, c.tone, c.preDelay, c.shimmer, c.mode, kSampleRate, c.subMode);
                                   return reverb.Process(io, 2, static_cast<size_t>(n));
                                 })));
  }
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
