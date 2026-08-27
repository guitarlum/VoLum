// PRE Pitch: AUDIBLE-ARTIFACT regression net.
//
// Why this file exists separately from test_volum_pitch.cpp: that file pins pitch
// accuracy, latency, polyphony and splice CADENCE. Twice now a POLY upshift crackle
// was "fixed" by reasoning about cadence, shipped with a cadence-based regression
// test, and stayed audibly broken - 1.2.1's guard capped the upshift splice count
// and passed while every one of those splices joined the waveform at an arbitrary
// phase. Counting splices cannot see that. These tests measure the two things that
// actually correspond to what a player hears:
//
//   1. WHITE BOX - the normalized cross-correlation achieved at each splice, taken
//      at the delay actually spliced to. Misaligned joins are the crackle.
//   2. BLACK BOX - non-harmonic residual in the rendered audio, via a comb that
//      notches every expected partial, judged against the IDEAL pitch-shifted
//      signal rather than an arbitrary constant.
//
// Both layers were verified to FAIL against the pre-fix engine; see the recorded
// margins on each threshold below. Steady sine tones are deliberately NOT used as
// primary material: measured pre-fix, a 220 Hz sine scored clean at every shift
// while a plucked note exposed the defect immediately. That blind spot is precisely
// how this bug survived two releases.

#include "third_party/doctest.h"

#include "../VoLumPitchShifter.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
using dsp::effect::GranularVoice;
using dsp::effect::VoLumPitch;

constexpr double kSR = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr size_t kBlock = 256;

// Reachable Transpose range: kPrePitchSemitones is InitDouble(..., -12.0, 7.0, ...)
// in NeuralAmpModeler.cpp, so +7 is the largest upshift a user can dial in.
const std::vector<double> kReachableShifts = {-12.0, -7.0, -5.0, -2.0, 2.0, 5.0, 7.0};

struct Source
{
  // std::string, not const char*: doctest's INFO captures a bare pointer by value
  // and reports the address, which makes a failure report useless.
  std::string name;
  double f0;
};

// Normal guitar range, including drop C. These carry the strict bounds.
const std::vector<Source> kNormalRange = {{"E2 low E", 82.41}, {"A2", 110.00}, {"G3", 196.00}, {"drop C", 65.41}};

// Additive plucked string: harmonics with frequency-dependent decay and mild
// inharmonicity, i.e. rich transient material. An ideal pitch shifter maps this to
// the SAME function with f0 scaled by the ratio - each partial moves from f0*h to
// f0*ratio*h and keeps its envelope, which depends only on h. That identity is what
// makes an ideal reference available at all.
std::vector<double> MakePluck(double f0, size_t n)
{
  std::vector<double> v(n, 0.0);
  const double B = 0.0004; // steel-string inharmonicity
  for (int h = 1; h <= 12; ++h)
  {
    const double fh = f0 * h * std::sqrt(1.0 + B * h * h);
    if (fh > 0.45 * kSR)
      break;
    const double amp = 1.0 / h;
    const double hTau = 2.0 / (1.0 + 0.6 * (h - 1));
    const double phase = 0.37 * h;
    for (size_t i = 0; i < n; ++i)
      v[i] += amp * std::exp(-static_cast<double>(i) / kSR / hTau)
              * std::sin(2.0 * kPi * fh * static_cast<double>(i) / kSR + phase);
  }
  double peak = 1e-9;
  for (double s : v)
    peak = std::max(peak, std::fabs(s));
  for (double& s : v)
    s *= 0.5 / peak;
  return v;
}

// E major triad with a true third: E2, G#3, B3. Deliberately NOT a 2:3:4 voicing
// like E2/B2/E3 - those frequencies share a common period, so the sum behaves like a
// single complex tone and is an easy case dressed up as a hard one. An equal
// -tempered major third has no common period with the root, which is the situation
// fixed-grain POLY exists to survive.
std::vector<double> MakeTriad(size_t n)
{
  const double roots[3] = {82.41, 207.65, 246.94};
  std::vector<double> v(n, 0.0);
  for (double f0 : roots)
  {
    const std::vector<double> part = MakePluck(f0, n);
    for (size_t i = 0; i < n; ++i)
      v[i] += part[i];
  }
  double peak = 1e-9;
  for (double s : v)
    peak = std::max(peak, std::fabs(s));
  for (double& s : v)
    s *= 0.5 / peak;
  return v;
}

double LerpAt(const std::vector<double>& x, double idx)
{
  if (idx < 0.0 || idx >= static_cast<double>(x.size()) - 1.0)
    return 0.0;
  const size_t i0 = static_cast<size_t>(idx);
  const double f = idx - static_cast<double>(i0);
  return x[i0] * (1.0 - f) + x[i0 + 1] * f;
}

std::vector<double> MovingRms(const std::vector<double>& x, int win)
{
  std::vector<double> e(x.size(), 0.0);
  double acc = 0.0;
  for (size_t i = 0; i < x.size(); ++i)
  {
    acc += x[i] * x[i];
    if (i >= static_cast<size_t>(win))
      acc -= x[i - win] * x[i - win];
    e[i] = std::sqrt(std::max(acc, 0.0) / static_cast<double>(std::min(static_cast<size_t>(win), i + 1)));
  }
  return e;
}

double Percentile(std::vector<double> v, double p)
{
  if (v.empty())
    return 0.0;
  const size_t k = std::min(v.size() - 1, static_cast<size_t>(p * static_cast<double>(v.size())));
  std::nth_element(v.begin(), v.begin() + static_cast<long>(k), v.end());
  return v[k];
}

struct Artifacts
{
  double residDb = 0.0; // non-harmonic energy relative to total, in dB
  double crestDb = 0.0; // p99.5 / median of the residual envelope: impulsiveness
};

// Comb out every expected partial. res[n] = out[n] - out[n - P] with P the period of
// the EXPECTED output fundamental has a zero at every multiple of that fundamental,
// so whatever survives is not part of the harmonic series the shifter was supposed
// to produce: splice discontinuities, injected transients, noise.
Artifacts MeasureArtifacts(const std::vector<double>& out, double expectedF0)
{
  Artifacts a;
  const double period = kSR / expectedF0;
  const long start = std::max(static_cast<long>(0.15 * kSR), static_cast<long>(std::ceil(period)) + 1);
  if (start >= static_cast<long>(out.size()))
    return a;

  std::vector<double> res;
  res.reserve(out.size());
  double residEnergy = 0.0, totalEnergy = 0.0;
  for (long i = start; i < static_cast<long>(out.size()); ++i)
  {
    const double r = out[i] - LerpAt(out, static_cast<double>(i) - period);
    res.push_back(r);
    residEnergy += r * r;
    totalEnergy += out[i] * out[i];
  }
  a.residDb = 10.0 * std::log10((residEnergy + 1e-18) / (totalEnergy + 1e-18));

  const std::vector<double> env = MovingRms(res, static_cast<int>(0.001 * kSR));
  a.crestDb = 20.0 * std::log10((Percentile(env, 0.995) + 1e-15) / (Percentile(env, 0.5) + 1e-15));
  return a;
}

// The floor this case could achieve even with a mathematically perfect shifter.
// Subtracting it removes the part of the residual that is inherent to the signal
// (an envelope decaying across one comb period never cancels exactly), which
// otherwise scales with the comb period and makes deep downshifts look broken.
Artifacts IdealFloor(double f0, double ratio, size_t n)
{
  return MeasureArtifacts(MakePluck(f0 * ratio, n), f0 * ratio);
}

struct VoiceRun
{
  std::vector<double> out;
  double meanCorr = 1.0;
  unsigned long long splices = 0;
};

VoiceRun RunVoice(GranularVoice::Character character, double semitones, const std::vector<double>& in,
                  double sampleRate = kSR)
{
  GranularVoice voice;
  voice.Configure(sampleRate, static_cast<int>(kBlock));
  voice.SetCharacter(character);
  voice.SetRatio(std::pow(2.0, semitones / 12.0));
  voice.Reset();

  VoiceRun r;
  r.out.assign(in.size(), 0.0);
  for (size_t off = 0; off < in.size(); off += kBlock)
    voice.Process(in.data() + off, r.out.data() + off, std::min(kBlock, in.size() - off));
  r.meanCorr = voice.MeanSpliceCorr();
  r.splices = voice.SpliceStarts();
  return r;
}

// Thresholds. Each records the measured margin between the fixed engine and the
// pre-fix engine on the same material, so a future retune can tell whether it is
// eating into real headroom.
//
//   kMinSpliceCorr    fixed 0.911..0.974   pre-fix -0.490..0.775
//   kMaxCrestExcessDb fixed <= 1.13 dB     pre-fix 5.31..16.94 dB
//   kMaxResidExcessDb fixed <= 2.80 dB     pre-fix 3.12..18.64 dB
constexpr double kMinSpliceCorr = 0.85;
constexpr double kMaxCrestExcessDb = 4.0;
constexpr double kMaxResidExcessDb = 5.0;

} // namespace

TEST_CASE("PitchArtifacts: POLY splices stay waveform-aligned at every reachable shift")
{
  // THE guard for the 1.2.1 crackle. Alignment, not cadence, is what breaks: the old
  // fix searched [-search,+search] and then clamped the ANSWER up to the intended
  // jump, which discarded the alignment on nearly every upshift splice because the
  // best-correlating candidate on a decaying note is reliably the nearest one.
  const size_t n = static_cast<size_t>(1.0 * kSR);
  for (const Source& src : kNormalRange)
  {
    const std::vector<double> in = MakePluck(src.f0, n);
    for (double semi : kReachableShifts)
    {
      const VoiceRun r = RunVoice(GranularVoice::Character::Poly, semi, in);
      INFO("source=" << src.name << " semitones=" << semi << " meanCorr=" << r.meanCorr << " splices=" << r.splices);
      REQUIRE(r.splices > 3); // the effect must actually be engaged
      CHECK(r.meanCorr > kMinSpliceCorr);
    }
  }
}

TEST_CASE("PitchArtifacts: POLY injects no impulsive non-harmonic energy (crackle)")
{
  // Black-box counterpart: judge the rendered audio, not the engine's internals.
  // crest is the crackle detector (misaligned joins are impulsive); resid is the
  // supporting level of non-harmonic content.
  const size_t n = static_cast<size_t>(1.0 * kSR);
  for (const Source& src : kNormalRange)
  {
    const std::vector<double> in = MakePluck(src.f0, n);
    for (double semi : kReachableShifts)
    {
      const double ratio = std::pow(2.0, semi / 12.0);
      const VoiceRun r = RunVoice(GranularVoice::Character::Poly, semi, in);
      const Artifacts got = MeasureArtifacts(r.out, src.f0 * ratio);
      const Artifacts floor = IdealFloor(src.f0, ratio, n);
      const double crestExcess = got.crestDb - floor.crestDb;
      const double residExcess = got.residDb - floor.residDb;
      INFO("source=" << src.name << " semitones=" << semi << " crestExcess=" << crestExcess
                     << " dB residExcess=" << residExcess << " dB");
      CHECK(crestExcess < kMaxCrestExcessDb);
      CHECK(residExcess < kMaxResidExcessDb);
    }
  }
}

TEST_CASE("PitchArtifacts: POLY holds a chord without crackling")
{
  // Forward-looking coverage, NOT a pin for the upshift-clamp bug: measured against
  // the pre-fix engine, chords score the same before and after the fix (mean splice
  // correlation 0.94/0.97 pre-fix versus 0.93/0.95 post-fix at +5/+7). The defect
  // needed a single sustained note, where the best-correlating candidate is reliably
  // the nearest one in history and the clamp therefore fired on every splice - which
  // is also why players heard it on clean amps and ringing notes. The single-note
  // cases above carry the teeth for that; this one guards POLY's actual purpose.
  const size_t n = static_cast<size_t>(1.0 * kSR);
  const std::vector<double> chord = MakeTriad(n);
  for (double semi : kReachableShifts)
  {
    const VoiceRun r = RunVoice(GranularVoice::Character::Poly, semi, chord);
    REQUIRE(r.splices > 3);

    // Alignment is only bounded on upshift. With three unrelated periods no splice
    // point can align all voices at once, so downshift settles near 0.46 by nature
    // (identical pre- and post-fix) and a correlation floor there would be fiction.
    // The artifact ceiling below still applies at every shift.
    if (semi > 0.0)
    {
      INFO("chord semitones=" << semi << " meanCorr=" << r.meanCorr);
      CHECK(r.meanCorr > kMinSpliceCorr);
    }

    // Judge the chord against its root voice; the comb notches that harmonic series,
    // and the crest statistic is dominated by injected transients either way.
    const double ratio = std::pow(2.0, semi / 12.0);
    const Artifacts got = MeasureArtifacts(r.out, 82.41 * ratio);
    const Artifacts floor = IdealFloor(82.41, ratio, n);
    INFO("chord semitones=" << semi << " crestExcess=" << (got.crestDb - floor.crestDb) << " dB");
    CHECK(got.crestDb - floor.crestDb < kMaxCrestExcessDb);
  }
}

TEST_CASE("PitchArtifacts: octaver up voice stays aligned")
{
  // The Octaver runs DROP-character voices at 0.5x and 2.0x. Its up voice goes
  // through the same one-sided upshift path POLY does, so it is covered here.
  // The down voice is deliberately not bounded: at -12 its output fundamental falls
  // below the engine's kPminFreq design floor (low E -> 41 Hz, 8-string -> 23 Hz),
  // a pre-existing limitation this fix does not change and must not pretend to.
  const size_t n = static_cast<size_t>(1.0 * kSR);
  for (const Source& src : kNormalRange)
  {
    const std::vector<double> in = MakePluck(src.f0, n);
    const VoiceRun r = RunVoice(GranularVoice::Character::Drop, 12.0, in);
    INFO("octave-up source=" << src.name << " meanCorr=" << r.meanCorr);
    REQUIRE(r.splices > 3);
    CHECK(r.meanCorr > kMinSpliceCorr);
  }
}

TEST_CASE("PitchArtifacts: extended-range low string upshifts without crackle")
{
  // 8-string F#1. Its period (1038 samples at 48 kHz) exceeds POLY's 20 ms grain, so
  // the fixed-grain assumption is strained and the strict alignment bound does not
  // apply. Upshift must still be free of impulsive artifacts, which is the user
  // -visible property. Measured post-fix: crest excess -0.38..1.25 dB.
  const size_t n = static_cast<size_t>(1.0 * kSR);
  const double f0 = 46.25;
  const std::vector<double> in = MakePluck(f0, n);
  for (double semi : {2.0, 5.0, 7.0})
  {
    const double ratio = std::pow(2.0, semi / 12.0);
    const VoiceRun r = RunVoice(GranularVoice::Character::Poly, semi, in);
    const Artifacts got = MeasureArtifacts(r.out, f0 * ratio);
    const Artifacts floor = IdealFloor(f0, ratio, n);
    INFO("8-string semitones=" << semi << " crestExcess=" << (got.crestDb - floor.crestDb) << " dB");
    CHECK(got.crestDb - floor.crestDb < kMaxCrestExcessDb);
  }
}

TEST_CASE("PitchArtifacts: splice geometry invariants hold at every sample rate")
{
  // Cheap, no rendering. The grain geometry is derived from the sample rate, so an
  // invariant that only holds at 48 kHz is not an invariant.
  for (double sr : {44100.0, 48000.0, 96000.0})
  {
    for (auto character :
         {GranularVoice::Character::Drop, GranularVoice::Character::Instant, GranularVoice::Character::Poly})
    {
      const auto g = GranularVoice::SpliceGeometryFor(character, sr);
      INFO("sr=" << sr << " character=" << static_cast<int>(character) << " search=" << g.search << " band=" << g.band
                 << " xfade=" << g.xfade);
      // A splice must never be able to jump by more than a whole grain, or the very
      // next sample can retrigger one (runaway re-splicing).
      if (g.fixedGrain)
        CHECK(g.search < g.band);
      // The crossfade has to fit inside the grain, otherwise splices overlap and the
      // engine is permanently mid-fade.
      CHECK(g.xfade < g.band);
      CHECK(g.xfade > 0);
    }
  }
}

TEST_CASE("PitchArtifacts: alignment holds at 44.1 and 96 kHz too")
{
  // The bug lived in a lag range derived from the sample rate, so pin the behaviour
  // off the 48 kHz grid as well. Shorter render: this is a smoke check, not the
  // full matrix.
  for (double sr : {44100.0, 96000.0})
  {
    const size_t n = static_cast<size_t>(1.0 * sr);
    // MakePluck synthesises on the 48 kHz constant; at other rates the pitch differs
    // from the label, which does not matter - only alignment is asserted here.
    const std::vector<double> in = MakePluck(82.41, n);
    for (double semi : {-7.0, 7.0})
    {
      const VoiceRun r = RunVoice(GranularVoice::Character::Poly, semi, in, sr);
      INFO("sr=" << sr << " semitones=" << semi << " meanCorr=" << r.meanCorr);
      REQUIRE(r.splices > 3);
      CHECK(r.meanCorr > kMinSpliceCorr);
    }
  }
}

TEST_CASE("PitchArtifacts: extract-once WSOLA picks the same lag as nested reads")
{
  // CPU-only change: overlapping candidate windows are interpolated once, then
  // the lag loop uses that linear buffer. The j-loop sum order is unchanged, so
  // the chosen lag must match the nested-read oracle on the same warmed-up state.
  // POLY and DROP both search; INSTANT does not. Exact == is the sound contract.
  const size_t n = static_cast<size_t>(0.5 * kSR);
  const std::vector<double> in = MakePluck(82.41, n);
  for (auto character : {GranularVoice::Character::Poly, GranularVoice::Character::Drop})
  {
    for (double semi : kReachableShifts)
    {
      GranularVoice voice;
      voice.Configure(kSR, static_cast<int>(kBlock));
      voice.SetCharacter(character);
      voice.SetRatio(std::pow(2.0, semi / 12.0));
      voice.Reset();
      std::vector<double> out(n);
      for (size_t off = 0; off < n; off += kBlock)
        voice.Process(in.data() + off, out.data() + off, std::min(kBlock, n - off));

      const auto g = GranularVoice::SpliceGeometryFor(character, kSR);
      const double cand = voice.DebugDelay();
      const double nestedTwo = voice.DebugWsolaRefineRangeNested(cand, -g.search, g.search, false);
      const double extractTwo = voice.DebugWsolaRefineRange(cand, -g.search, g.search, false);
      const double nestedUp = voice.DebugWsolaRefineRangeNested(cand, 0, g.search, true);
      const double extractUp = voice.DebugWsolaRefineRange(cand, 0, g.search, true);
      INFO("character=" << static_cast<int>(character) << " semitones=" << semi << " cand=" << cand
                        << " nestedTwo=" << nestedTwo << " extractTwo=" << extractTwo << " nestedUp=" << nestedUp
                        << " extractUp=" << extractUp);
      CHECK(extractTwo == nestedTwo);
      CHECK(extractUp == nestedUp);
    }
  }
}

TEST_CASE("PitchArtifacts: full pedal stays finite and bounded across the matrix")
{
  // Guards the whole VoLumPitch wrapper (dry ring, mix, level, octaver summing)
  // rather than the bare voice, at the settings the artifact tests exercise.
  const size_t n = static_cast<size_t>(0.5 * kSR);
  const std::vector<double> in = MakePluck(82.41, n);
  for (auto mode : {VoLumPitch::Mode::Transpose, VoLumPitch::Mode::Octaver})
  {
    for (double semi : kReachableShifts)
    {
      VoLumPitch pitch;
      pitch.Configure(kSR, static_cast<int>(kBlock));
      pitch.SetParams(mode, semi, 1.0, 0.8, 0.8, 1.0, VoLumPitch::Voicing::Modern, 0.0, VoLumPitch::Character::Poly);
      pitch.Reset();
      double peak = 0.0;
      std::vector<DSP_SAMPLE> buf(kBlock);
      for (size_t off = 0; off < n; off += kBlock)
      {
        const size_t m = std::min(kBlock, n - off);
        std::copy(in.begin() + static_cast<long>(off), in.begin() + static_cast<long>(off + m), buf.begin());
        DSP_SAMPLE* ptr = buf.data();
        DSP_SAMPLE** out = pitch.Process(&ptr, 1, m);
        for (size_t i = 0; i < m; ++i)
        {
          REQUIRE(std::isfinite(static_cast<double>(out[0][i])));
          peak = std::max(peak, std::fabs(static_cast<double>(out[0][i])));
        }
      }
      INFO("mode=" << static_cast<int>(mode) << " semitones=" << semi << " peak=" << peak);
      CHECK(peak < 8.0);
    }
  }
}
