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
      const double a = r[static_cast<size_t>(lag) - 1], b = r[static_cast<size_t>(lag)],
                   c = r[static_cast<size_t>(lag) + 1];
      const double denom = a - 2.0 * b + c;
      double delta = 0.0;
      if (std::fabs(denom) > 1e-12)
        delta = 0.5 * (a - c) / denom;
      return kSR / (static_cast<double>(lag) + delta);
    }
  }
  return kSR / static_cast<double>(minLag);
}

double cents(double f, double ref)
{
  return 1200.0 * std::log2(f / ref);
}

// Goertzel magnitude at a single frequency over [from, from+len): lets a
// polyphony test assert each chord voice's shifted fundamental is actually
// present in the output (a monophonic shifter collapses chords to one pitch).
double goertzel(const std::vector<DSP_SAMPLE>& x, size_t from, size_t len, double freq)
{
  const double w = 2.0 * M_PI * freq / kSR;
  const double coeff = 2.0 * std::cos(w);
  double s0 = 0.0, s1 = 0.0, s2 = 0.0;
  for (size_t i = 0; i < len; ++i)
  {
    s0 = static_cast<double>(x[from + i]) + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / static_cast<double>(len);
}

std::vector<DSP_SAMPLE> makeChord(const std::vector<double>& freqs, size_t n, double amp = 0.4)
{
  std::vector<DSP_SAMPLE> v(n, 0.0);
  for (double f : freqs)
    for (size_t i = 0; i < n; ++i)
      v[i] += static_cast<DSP_SAMPLE>(amp / static_cast<double>(freqs.size())
                                      * std::sin(2.0 * M_PI * f * static_cast<double>(i) / kSR));
  return v;
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

TEST_CASE("VoLumPitch pre-reserves host block growth and never reallocates in Process")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, 64);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 7.0, 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0,
                  VoLumPitch::Character::Poly);

  CHECK(pitch.PreparedBlockSize() >= VoLumPitch::kRealtimeBlockReserve);
  const int latency = pitch.Latency();

  for (size_t block : {size_t{64}, size_t{128}, size_t{256}, size_t{1024}})
  {
    auto in = makeSine(196.0, block);
    DSP_SAMPLE* ptr[1] = {in.data()};
    DSP_SAMPLE** out = pitch.Process(ptr, 1, block);
    CHECK(out != ptr); // Processed from the off-thread reserve, not dry fallback.
    CHECK(pitch.PreparedBlockSize() >= static_cast<int>(block));
    CHECK(pitch.Latency() == latency);
    for (size_t i = 0; i < block; ++i)
      CHECK(std::isfinite(static_cast<double>(out[0][i])));
  }

  // Beyond the fixed reserve, fail safe to dry for one block. Most importantly,
  // do not grow/reconfigure the shifter on the real-time thread.
  const size_t oversized = static_cast<size_t>(pitch.PreparedBlockSize()) + 1;
  auto in = makeSine(196.0, oversized);
  DSP_SAMPLE* ptr[1] = {in.data()};
  CHECK(pitch.Process(ptr, 1, oversized) == ptr);
  CHECK(pitch.PreparedBlockSize() == VoLumPitch::kRealtimeBlockReserve);
  CHECK(pitch.Latency() == latency);
}

TEST_CASE("VoLumPitch stays finite across supported sample rates and small host blocks")
{
  for (double sr : {44100.0, 48000.0, 96000.0})
    for (int block : {64, 128, 256})
      for (int workload = 0; workload < 3; ++workload)
      {
        const auto mode = workload == 2 ? VoLumPitch::Mode::Octaver : VoLumPitch::Mode::Transpose;
        const auto character =
          workload == 0 ? VoLumPitch::Character::Instant : VoLumPitch::Character::Poly;
        VoLumPitch pitch;
        pitch.Configure(sr, block);
        pitch.Reset();
        pitch.SetParams(mode, 7.0, 1.0, 0.8, 0.8, 0.5, VoLumPitch::Voicing::Modern, 0.0, character);

        std::vector<DSP_SAMPLE> in(static_cast<size_t>(block));
        for (int n = 0; n < block; ++n)
          in[static_cast<size_t>(n)] =
            static_cast<DSP_SAMPLE>(0.5 * std::sin(2.0 * M_PI * 196.0 * static_cast<double>(n) / sr));
        DSP_SAMPLE* ptr[1] = {in.data()};

        for (int pass = 0; pass < 32; ++pass)
        {
          DSP_SAMPLE** out = pitch.Process(ptr, 1, in.size());
          for (size_t i = 0; i < in.size(); ++i)
          {
            CHECK(std::isfinite(static_cast<double>(out[0][i])));
            CHECK(std::abs(static_cast<double>(out[0][i])) < 8.0);
          }
        }
      }
}

TEST_CASE("VoLumPitch latency is a per-character ladder (Instant < Poly < Drop) and bounded")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.SetParams(VoLumPitch::Mode::Transpose, 0.0, 1.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0,
                  VoLumPitch::Character::Drop);
  const int dropLat = pitch.Latency();
  CHECK(dropLat > 0);
  // Drop ~17 ms; well under the old 22.5 ms fixed grain and bounded above.
  CHECK(dropLat < static_cast<int>(kSR * 0.030));
  CHECK(dropLat > static_cast<int>(kSR * 0.008));

  pitch.SetParams(VoLumPitch::Mode::Transpose, 0.0, 1.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0,
                  VoLumPitch::Character::Instant);
  const int instantLat = pitch.Latency();
  CHECK(instantLat > 0);
  CHECK(instantLat < dropLat); // Instant shortens the crossfade + drops WSOLA for the lowest latency.
  // Instant ~8.6 ms; still above a hard floor (period-sync read-ahead).
  CHECK(instantLat > static_cast<int>(kSR * 0.004));

  // Poly (fixed-grain WSOLA, chord-capable) is now LOW latency: the dynamic-RE port
  // (RESEARCH-NOTES Phase 6) proved the read pointer is clamped to >= xfade by the
  // splice, so the WSOLA search/correlation are history reads that do not inflate the
  // delay floor (latency = xfade + 0.5*band). Poly therefore sits just above the
  // tightest mono character (Instant) and BELOW Drop - it buys polyphony at almost no
  // latency cost, ~14 ms vs the former ~49 ms.
  pitch.SetParams(VoLumPitch::Mode::Transpose, 0.0, 1.0, 0.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0,
                  VoLumPitch::Character::Poly);
  const int polyLat = pitch.Latency();
  CHECK(polyLat > instantLat); // poly adds polyphony just above the tightest mono character
  CHECK(polyLat < dropLat);    // ...but is now cheaper than Drop (was ~3x Drop before the RE port)
  // Poly ~14 ms; locked well under the old ~49 ms so a regression to the WSOLA-floor
  // tuning is caught, and bounded above so a runaway grain can't blow up PDC.
  CHECK(polyLat < static_cast<int>(kSR * 0.020));
  CHECK(polyLat > static_cast<int>(kSR * 0.008));

  // Octaver uses Drop-grade voices regardless of the transpose character.
  pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 1.0, 1.0, 0.0, 1.0, VoLumPitch::Voicing::Modern, 0.0,
                  VoLumPitch::Character::Instant);
  CHECK(pitch.Latency() == dropLat);

  // The static, param-based helper (used for host PDC + the settings readout on the
  // main thread) must agree with the live instance latency for every mode/character,
  // so the reported value never lags the audio thread by one change.
  CHECK(VoLumPitch::LatencyFor(VoLumPitch::Mode::Transpose, VoLumPitch::Character::Drop, kSR) == dropLat);
  CHECK(VoLumPitch::LatencyFor(VoLumPitch::Mode::Transpose, VoLumPitch::Character::Instant, kSR) == instantLat);
  CHECK(VoLumPitch::LatencyFor(VoLumPitch::Mode::Transpose, VoLumPitch::Character::Poly, kSR) == polyLat);
  // Octaver ignores the character pill and always reports Drop-grade latency.
  CHECK(VoLumPitch::LatencyFor(VoLumPitch::Mode::Octaver, VoLumPitch::Character::Instant, kSR) == dropLat);
  CHECK(VoLumPitch::LatencyFor(VoLumPitch::Mode::Octaver, VoLumPitch::Character::Drop, kSR) == dropLat);
  CHECK(VoLumPitch::LatencyFor(VoLumPitch::Mode::Octaver, VoLumPitch::Character::Poly, kSR) == dropLat);
}

TEST_CASE("VoLumPitch transpose is pitch-accurate downshifting in both characters")
{
  // Core claim: exact-ratio read pointer + period-sync splices => no octave error
  // and tight pitch on low-string downtuning (the primary use case). DROP (WSOLA)
  // is tighter; INSTANT is period-sync only (looser on big shifts) but still well
  // within an eighth-tone.
  const double base = 110.0; // A2, low string
  for (int chI = 0; chI < 2; ++chI) // Drop, Instant
  {
    const auto ch = static_cast<VoLumPitch::Character>(chI);
    for (int semi : {-12, -7, -5, -3, -2})
    {
      VoLumPitch pitch;
      pitch.Configure(kSR, kBlock);
      pitch.Reset();
      pitch.SetParams(VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0,
                      VoLumPitch::Voicing::Modern, 0.0, ch);
      auto in = makeSine(base, 1 << 16);
      auto out = runStream(pitch, in);
      const double target = base * std::pow(2.0, semi / 12.0);
      const int minLag = std::max(8, static_cast<int>(kSR / (target * 2.0)));
      const int maxLag = static_cast<int>(kSR / (target * 0.5));
      const double f = estimateFreq(out, static_cast<size_t>(pitch.Latency()) + 24000, 8192, minLag, maxLag);
      INFO("char=", chI, " semi=", semi, " target=", target, " measured=", f);
      const double tolCents = (ch == VoLumPitch::Character::Drop) ? 25.0 : 55.0;
      CHECK(std::abs(cents(f, target)) < tolCents);
    }
  }
}

TEST_CASE("VoLumPitch tracks extended-range LOW strings (drop C / 7- / 8-string) without detune")
{
  // REGRESSION GUARD for the "low C string warps and moves" bug: the autocorr
  // pitch tracker floor used to be 70 Hz, ABOVE drop-C C2 (65 Hz), 7-string B1
  // (62 Hz) and 8-string F#1 (46 Hz). Those strings fell outside the search range,
  // locked to a wrong period, and detuned by tens/hundreds of cents with audible
  // warble. The floor is now 40 Hz (kPminFreq), which fixes the period-synchronous
  // characters (INSTANT here; DROP internally) at ZERO latency cost. POLY is NOT
  // covered here: it is fixed-grain and never uses the pitch estimate, so this
  // tracker fix does not apply to it (its accuracy is pinned by the POLY single-note
  // and sustain tests). INSTANT must now hold pitch within a quarter-tone on every
  // low string on a downshift.
  struct LowString
  {
    const char* name;
    double f0;
  };
  for (const LowString s : {LowString{"C2", 65.41}, LowString{"B1", 61.74}, LowString{"F#1", 46.25}})
  {
    for (int semi : {-2, -5})
    {
      VoLumPitch pitch;
      pitch.Configure(kSR, kBlock);
      pitch.Reset();
      pitch.SetParams(VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0,
                      VoLumPitch::Voicing::Modern, 0.0, VoLumPitch::Character::Instant);
      auto in = makeSine(s.f0, 1 << 16);
      auto out = runStream(pitch, in);

      // No NaN/Inf on deep input.
      for (double v : out)
        CHECK(std::isfinite(v));

      const double target = s.f0 * std::pow(2.0, semi / 12.0);
      const int minLag = std::max(8, static_cast<int>(kSR / (target * 2.0)));
      const int maxLag = static_cast<int>(kSR / (target * 0.5));
      const double f = estimateFreq(out, static_cast<size_t>(pitch.Latency()) + 24000, 8192, minLag, maxLag);
      INFO("string=", s.name, " semi=", semi, " target=", target, " measured=", f);
      // Pre-fix these detuned by 78..584 cents; a quarter-tone (50 c) tolerance
      // catches any regression to the old out-of-range tracker while staying
      // robust to the output-side autocorr estimator at these low frequencies.
      CHECK(std::abs(cents(f, target)) < 50.0);
    }
  }
}

TEST_CASE("VoLumPitch POLY WSOLA search stays below the grain band (no runaway re-splicing)")
{
  // REGRESSION GUARD for the POLY "stutter/crackle at any non-zero shift" bug. POLY
  // is fixed-grain: it splices by a whole `band` (grain spacing) and _WsolaRefine
  // nudges the splice target within [-search, +search] for waveform alignment. If
  // search >= band the correlation can move the target MORE than a whole grain (even
  // back above dHi), so the next sample retriggers a splice and POLY re-splices every
  // crossfade -> continuous crackle on real transient playing. A 24 ms search (>= the
  // 20 ms band) briefly shipped and did exactly that; it slipped through because
  // steady sine/triad tests can't hear it (every jump lands on similar-looking
  // periodic waveform). Pin the invariant directly across the supported sample rates
  // so no future band/search retune can silently reintroduce it.
  using GV = dsp::effect::GranularVoice;
  for (double sr : {44100.0, 48000.0, 88200.0, 96000.0})
  {
    const GV::SpliceGeometry g = GV::SpliceGeometryFor(GV::Character::Poly, sr);
    INFO("sr=", sr, " search=", g.search, " band=", g.band);
    CHECK(g.fixedGrain);
    CHECK(g.search > 0);
    CHECK(g.band > 0);
    CHECK(g.search < g.band); // THE invariant: search >= band => runaway splicing.
  }
}

namespace
{
// Run one POLY GranularVoice over rich transient material at a given ratio and
// return how many crossfaded splices it started. Splice cadence is the direct
// signal for the positive-semitone crackle: a shortened grain re-splices far more
// often than the ~20 ms band.
unsigned long long polySpliceCount(double ratio, size_t n)
{
  using GV = dsp::effect::GranularVoice;
  GV voice;
  voice.Configure(kSR, kBlock);
  voice.SetCharacter(GV::Character::Poly);
  voice.Reset();
  voice.SetRatio(ratio);
  auto in = makeGuitar(196.0, n); // G3, decaying harmonics: transient + polyphonic-ish
  std::vector<DSP_SAMPLE> out(n);
  for (size_t start = 0; start < n; start += static_cast<size_t>(kBlock))
  {
    const size_t m = std::min<size_t>(static_cast<size_t>(kBlock), n - start);
    voice.Process(in.data() + start, out.data() + start, m);
  }
  return voice.SpliceStarts();
}
} // namespace

TEST_CASE("POLY upshift keeps its grain cadence (no positive-semitone crackle)")
{
  // REGRESSION GUARD for the positive-semitone crackle. POLY splices by a whole
  // `band` (~20 ms) and _WsolaRefine nudges the target within [-search, +search].
  // DOWNtuning always sounded great; UPshift crackled because WSOLA's negative lags
  // shortened the grain, so POLY re-spliced far more often than one grain per band.
  // The fix floors the upshift splice target at the intended one-band jump, so WSOLA
  // can only push the splice DEEPER into history (longer grain), never shorten it.
  // That imposes a hard cadence ceiling: with each grain travelling at least `band`,
  // the splice count over N samples at ratio f cannot exceed ~ N*(f-1)/band. Steady
  // sines hide the bug (any offset lands on similar waveform), so drive rich
  // transient material. Measured on this signal: pre-fix upshift ~93 splices/s,
  // post-fix ~25 (== the ceiling); the known-good downshift path is ~61 and untouched.
  const double ratioUp = std::pow(2.0, 7.0 / 12.0);  // +7 semitones (worst allowed upshift)
  const double ratioDn = std::pow(2.0, -7.0 / 12.0); // -7 semitones (known-good downtuning)
  const size_t n = static_cast<size_t>(kSR);         // 1 second
  const double band = 0.020 * kSR;                   // POLY grain spacing in samples

  const auto up = polySpliceCount(ratioUp, n);
  const auto down = polySpliceCount(ratioDn, n);
  const double ceiling = static_cast<double>(n) * (ratioUp - 1.0) / band; // ~24.9

  INFO("upshift splices=", up, " downshift splices=", down, " ceiling~", ceiling);
  // The effect is engaged in both directions.
  CHECK(up > 3);
  CHECK(down > 3);
  // THE guard: post-fix the upshift grain never shortens below `band`, so the splice
  // count is capped near the one-band-per-grain ceiling. Pre-fix it ran several-fold
  // higher (WSOLA-shortened grains); the +5 slack covers block/warmup boundary jitter.
  CHECK(static_cast<double>(up) <= ceiling + 5.0);
  // The upshift-only fix must not perturb the downshift path into a runaway either.
  CHECK(static_cast<double>(down) < 4.0 * ceiling);
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
    pitch.SetParams(
      VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0);
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
    pitch.SetParams(
      VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0);
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

TEST_CASE("VoLumPitch octaver Vintage voicing differs audibly from Modern")
{
  // VINTAGE shapes the wet octave voices (tanh drive + low-pass); MODERN leaves
  // them clean. Isolating the down-octave voice (DRY=0), the two voicings must
  // reshape it enough to actually hear when switching the pill.
  auto runVoicing = [](VoLumPitch::Voicing v) {
    VoLumPitch pitch;
    pitch.Configure(kSR, kBlock);
    pitch.Reset();
    pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 0.0, 1.0 /*octDown*/, 0.0 /*octUp*/, 0.0 /*dry*/, v, 0.0);
    return runStream(pitch, makeGuitar(196.0, 1 << 16));
  };
  auto vintage = runVoicing(VoLumPitch::Voicing::Vintage);
  auto modern = runVoicing(VoLumPitch::Voicing::Modern);
  double diff = 0.0, energy = 0.0;
  for (size_t i = 24000; i < vintage.size(); ++i)
  {
    const double d = static_cast<double>(vintage[i]) - static_cast<double>(modern[i]);
    diff += d * d;
    energy += static_cast<double>(modern[i]) * static_cast<double>(modern[i]);
  }
  const double rel = std::sqrt(diff / std::max(energy, 1e-9));
  CHECK(rel > 0.15); // vintage saturation + LPF must reshape the sub voice
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

// --- POLY character (clean-room replication of NDSP Rabea X transpose family) ---

TEST_CASE("VoLumPitch POLY transposes a CHORD keeping every voice (polyphonic)")
{
  // THE differentiator vs Drop/Instant: feed a triad and check all three shifted
  // fundamentals survive. A monophonic period-sync engine collapses/garbles this.
  const std::vector<double> chord = {110.0, 138.59, 164.81}; // A2 major (A, C#, E)
  const int semi = 5;
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0,
                  VoLumPitch::Voicing::Modern, 0.0, VoLumPitch::Character::Poly);
  auto in = makeChord(chord, 1 << 16);
  auto out = runStream(pitch, in);
  const size_t from = static_cast<size_t>(pitch.Latency()) + 20000;
  const size_t len = 1 << 14;

  // Reference: broadband energy floor between the voices (anti-presence probe).
  const double ratio = std::pow(2.0, semi / 12.0);
  double minVoice = 1e9;
  for (double f : chord)
    minVoice = std::min(minVoice, goertzel(out, from, len, f * ratio));
  // Energy at a clearly non-chord frequency (e.g. a semitone below the lowest voice).
  const double offTone = goertzel(out, from, len, chord[0] * ratio * std::pow(2.0, -1.0 / 12.0));
  INFO("minVoice=", minVoice, " offTone=", offTone);
  // Each shifted voice must be present and clearly stronger than a non-chord bin.
  CHECK(minVoice > 4.0 * (offTone + 1e-9));
}

TEST_CASE("VoLumPitch POLY beats DROP at preserving chord voices")
{
  // Direct A/B: the weakest surviving voice of a triad should be (much) stronger
  // under POLY than under the monophonic DROP engine.
  const std::vector<double> chord = {110.0, 138.59, 164.81};
  const int semi = 5;
  const double ratio = std::pow(2.0, semi / 12.0);
  auto weakestVoice = [&](VoLumPitch::Character ch) {
    VoLumPitch pitch;
    pitch.Configure(kSR, kBlock);
    pitch.Reset();
    pitch.SetParams(
      VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0, ch);
    auto out = runStream(pitch, makeChord(chord, 1 << 16));
    const size_t from = static_cast<size_t>(pitch.Latency()) + 20000;
    double mn = 1e9;
    for (double f : chord)
      mn = std::min(mn, goertzel(out, from, 1 << 14, f * ratio));
    return mn;
  };
  const double poly = weakestVoice(VoLumPitch::Character::Poly);
  const double drop = weakestVoice(VoLumPitch::Character::Drop);
  INFO("poly weakest=", poly, " drop weakest=", drop);
  CHECK(poly > 1.5 * drop);
}

TEST_CASE("VoLumPitch POLY single-note pitch is accurate across the range")
{
  // Fixed-grain WSOLA must still nail single notes (exact-ratio read pointer).
  for (double base : {82.41, 110.0, 196.0}) // E2, A2, G3
  {
    for (int semi : {-12, -5, 5, 7, 12})
    {
      VoLumPitch pitch;
      pitch.Configure(kSR, kBlock);
      pitch.Reset();
      pitch.SetParams(VoLumPitch::Mode::Transpose, static_cast<double>(semi), 1.0, 0.0, 0.0, 0.0,
                      VoLumPitch::Voicing::Modern, 0.0, VoLumPitch::Character::Poly);
      auto in = makeSine(base, 1 << 16);
      auto out = runStream(pitch, in);
      const double target = base * std::pow(2.0, semi / 12.0);
      const int minLag = std::max(8, static_cast<int>(kSR / (target * 2.0)));
      const int maxLag = static_cast<int>(kSR / (target * 0.5));
      const double f = estimateFreq(out, static_cast<size_t>(pitch.Latency()) + 24000, 8192, minLag, maxLag);
      INFO("base=", base, " semi=", semi, " target=", target, " measured=", f);
      CHECK(std::abs(cents(f, target)) < 20.0);
    }
  }
}

TEST_CASE("VoLumPitch POLY holds pitch across a long sustain (no drift)")
{
  VoLumPitch pitch;
  pitch.Configure(kSR, kBlock);
  pitch.Reset();
  pitch.SetParams(VoLumPitch::Mode::Transpose, 7.0, 1.0, 0.0, 0.0, 0.0, VoLumPitch::Voicing::Modern, 0.0,
                  VoLumPitch::Character::Poly);
  auto in = makeSine(110.0, 1 << 17); // ~2.7 s
  auto out = runStream(pitch, in);
  const double target = 110.0 * std::pow(2.0, 7.0 / 12.0);
  const int minLag = std::max(8, static_cast<int>(kSR / (target * 2.0)));
  const int maxLag = static_cast<int>(kSR / (target * 0.5));
  const size_t lat = static_cast<size_t>(pitch.Latency());
  const double fEarly = estimateFreq(out, lat + 12000, 8192, minLag, maxLag);
  const double fLate = estimateFreq(out, lat + 90000, 8192, minLag, maxLag);
  INFO("early=", fEarly, " late=", fLate);
  CHECK(std::abs(cents(fLate, fEarly)) < 10.0);
}
