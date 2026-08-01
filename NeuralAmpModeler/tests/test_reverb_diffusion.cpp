// Reverb density and early-field metrics.
//
// A user reported hearing "discrete reflections ... as if a slapback delay were on",
// worst on muted notes. He was right, and neither of two audit passes over Reverb.cpp
// had caught it, because nothing here ever looked at the impulse response. Two
// structural faults produced it:
//
//   * The FDN modes (Hall, Oktaverb) had no input diffusion and read only full-length
//     taps, so the wet bus was silent until the shortest delay line - 52.7 ms on Hall -
//     and then delivered eight separate impulses.
//   * Plate implemented one allpass and one delay per tank half instead of Dattorro's
//     two, and took a single tap off the end of each half. The dropped stages were the
//     ones carrying the asymmetry, so both halves came to exactly 5125 samples and the
//     plate produced one centred repeat every 172 ms with nothing in between.
//
// The tests below measure the two things a listener actually complained about: a hole
// where reverb should have started, and a periodic repeat inside the tail. Every
// threshold was red-verified against the pre-fix topology while both were compilable
// side by side, and the numbers that run scored are quoted on each case, so the
// thresholds can still be judged now that the old code is gone.

#include "third_party/doctest.h"

#include "../../AudioDSPTools/dsp/Reverb.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr double kSR = 48000.0;
// 5 ms analysis windows: short enough to land inside the gaps we are hunting (the
// narrowest interesting one is Oktaverb's 31 ms onset), long enough that a single
// sample of interpolation noise cannot register as energy.
constexpr double kWindowMs = 5.0;

struct WetResponse
{
  std::vector<double> l;
  std::vector<double> r;
};

// Impulse response of the wet bus. Mix is 1.0, where the equal-power crossfade puts the
// dry coefficient at cos(pi/2) = 0, so nothing of the input impulse survives into the
// output and every sample below is reverb.
WetResponse RenderWet(int mode, int subMode, double decay, double tone, double preDelayMs, double seconds = 2.0)
{
  const size_t frames = static_cast<size_t>(kSR * seconds);
  dsp::effect::Reverb reverb;
  reverb.SetParams(1.0, decay, tone, preDelayMs, 0.0, mode, kSR, subMode);

  std::vector<double> in(frames, 0.0);
  in[0] = 1.0;
  std::vector<double> inR = in;
  double* inputs[2] = {in.data(), inR.data()};

  // One call: the smoothed Mix has already been snapped to target by the sample-rate
  // change inside SetParams, so the first block is at full wet.
  auto** out = reverb.Process(inputs, 2, frames);
  WetResponse resp;
  resp.l.assign(out[0], out[0] + frames);
  resp.r.assign(out[1], out[1] + frames);
  return resp;
}

// Per-window RMS of the stereo pair.
std::vector<double> Envelope(const WetResponse& resp)
{
  const size_t win = static_cast<size_t>(kSR * kWindowMs / 1000.0);
  std::vector<double> env;
  env.reserve(resp.l.size() / win);
  for (size_t start = 0; start + win <= resp.l.size(); start += win)
  {
    double sum = 0.0;
    for (size_t i = start; i < start + win; ++i)
      sum += resp.l[i] * resp.l[i] + resp.r[i] * resp.r[i];
    env.push_back(std::sqrt(sum / (2.0 * win)));
  }
  return env;
}

size_t MsToWindow(double ms)
{
  return static_cast<size_t>(ms / kWindowMs);
}

double PeakOf(const std::vector<double>& env)
{
  return env.empty() ? 0.0 : *std::max_element(env.begin(), env.end());
}

// First window carrying real energy. The threshold is well above the numerical floor
// and well below anything audible as reverb.
double OnsetMs(const std::vector<double>& env)
{
  const double peak = PeakOf(env);
  if (peak <= 0.0)
    return -1.0;
  for (size_t i = 0; i < env.size(); ++i)
    if (env[i] > peak * 0.01)
      return static_cast<double>(i) * kWindowMs;
  return -1.0;
}

// Quietest window in [fromMs, toMs), relative to the response's peak, in dB. A hole in
// the early field shows up here as a very negative number; a dense reverb keeps every
// window within a few tens of dB of the peak.
double QuietestWindowDb(const std::vector<double>& env, double fromMs, double toMs)
{
  const double peak = PeakOf(env);
  if (peak <= 0.0)
    return -200.0;
  double quietest = peak;
  const size_t to = std::min(MsToWindow(toMs), env.size());
  for (size_t i = MsToWindow(fromMs); i < to; ++i)
    quietest = std::min(quietest, env[i]);
  return 20.0 * std::log10(std::max(quietest, 1e-12) / peak);
}

// Strength of any periodic repeat in the tail.
//
// The envelope decays exponentially, which is a straight line in dB, so the trend is
// removed with a least-squares fit before correlating - otherwise the decay itself
// dominates the autocorrelation at every lag and swamps the thing we are looking for.
// What remains is the ripple, and a slapback shows up as a sharp peak at its period.
double PeriodicityPeak(const std::vector<double>& env, double minLagMs, double maxLagMs, double fromMs, double toMs)
{
  const size_t from = MsToWindow(fromMs);
  const size_t to = std::min(MsToWindow(toMs), env.size());
  if (to <= from + 4)
    return 0.0;

  std::vector<double> db;
  db.reserve(to - from);
  const double peak = PeakOf(env);
  for (size_t i = from; i < to; ++i)
    db.push_back(20.0 * std::log10(std::max(env[i], 1e-12) / std::max(peak, 1e-12)));

  const size_t n = db.size();
  double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
  for (size_t i = 0; i < n; ++i)
  {
    const double x = static_cast<double>(i);
    sx += x;
    sy += db[i];
    sxx += x * x;
    sxy += x * db[i];
  }
  const double denom = n * sxx - sx * sx;
  const double slope = (std::abs(denom) < 1e-9) ? 0.0 : (n * sxy - sx * sy) / denom;
  const double intercept = (sy - slope * sx) / n;
  for (size_t i = 0; i < n; ++i)
    db[i] -= slope * static_cast<double>(i) + intercept;

  double variance = 0.0;
  for (double v : db)
    variance += v * v;
  if (variance < 1e-9)
    return 0.0;

  double best = 0.0;
  const size_t minLag = std::max<size_t>(1, MsToWindow(minLagMs));
  const size_t maxLag = std::min(MsToWindow(maxLagMs), n / 2);
  for (size_t lag = minLag; lag <= maxLag; ++lag)
  {
    double acc = 0.0;
    for (size_t i = 0; i + lag < n; ++i)
      acc += db[i] * db[i + lag];
    best = std::max(best, acc / variance);
  }
  return best;
}

// How spiky the early field is: peak divided by mean over the window. A handful of
// discrete taps separated by silence scores high; a dense field scores near 1.
double CrestFactor(const std::vector<double>& env, double fromMs, double toMs)
{
  const size_t from = MsToWindow(fromMs);
  const size_t to = std::min(MsToWindow(toMs), env.size());
  if (to <= from)
    return 0.0;
  double peak = 0.0;
  double sum = 0.0;
  for (size_t i = from; i < to; ++i)
  {
    peak = std::max(peak, env[i]);
    sum += env[i];
  }
  const double mean = sum / static_cast<double>(to - from);
  return (mean <= 0.0) ? 1e9 : peak / mean;
}

// Correlation between the two output channels over the whole response.
double InterChannelCorrelation(const WetResponse& resp)
{
  double num = 0.0, dl = 0.0, dr = 0.0;
  for (size_t i = 0; i < resp.l.size(); ++i)
  {
    num += resp.l[i] * resp.r[i];
    dl += resp.l[i] * resp.l[i];
    dr += resp.r[i] * resp.r[i];
  }
  if (dl <= 0.0 || dr <= 0.0)
    return 1.0;
  return num / std::sqrt(dl * dr);
}
} // namespace

// ---------------------------------------------------------------------------
// The hole before the reverb starts
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: wet energy starts right after the pre-delay, not at the first delay line")
{
  struct Case
  {
    const char* name;
    int mode;
    int subMode;
  };
  // Oktaverb is checked on Shimmer; Bloom deliberately swells from silence, so an
  // onset measurement means nothing there.
  const Case cases[] = {{"hall", dsp::effect::Reverb::kModeHall, 0},
                        {"plate", dsp::effect::Reverb::kModePlate, 0},
                        {"oktaverb-shimmer", dsp::effect::Reverb::kModeOktaverb, 1}};

  for (const auto& c : cases)
  {
    INFO("mode=" << c.name);
    const auto env = Envelope(RenderWet(c.mode, c.subMode, 2.5, 5.0, 0.0));
    const double onset = OnsetMs(env);
    INFO("onset=" << onset << " ms");
    // Within two analysis windows of the impulse. Before the fix this was 55 ms for
    // Hall, 175 ms for Plate and 30 ms for Oktaverb.
    CHECK(onset >= 0.0);
    CHECK(onset <= 10.0);
  }
}

TEST_CASE("Reverb: PRE-DLY sets the onset instead of adding to a hidden floor")
{
  // The complaint that led here included turning PRE-DLY up and finding it made the
  // artifact worse. It did: the control adds to the tank's own onset, so Plate's real
  // range was 172-372 ms rather than the 0-200 ms on the knob, and every millisecond
  // moved the discrete tap further from the dry note that had been masking it.
  const double preDelays[] = {0.0, 20.0, 60.0};
  const int modes[] = {dsp::effect::Reverb::kModeHall, dsp::effect::Reverb::kModePlate};

  for (int mode : modes)
    for (double pre : preDelays)
    {
      INFO("mode=" << mode << " preDelay=" << pre);
      const auto env = Envelope(RenderWet(mode, 0, 2.5, 5.0, pre));
      const double onset = OnsetMs(env);
      INFO("onset=" << onset << " ms");
      CHECK(onset >= 0.0);
      // The onset tracks the knob, with a couple of windows of slack for the diffuser.
      CHECK(onset >= pre - kWindowMs);
      CHECK(onset <= pre + 2.0 * kWindowMs);
    }
}

TEST_CASE("Reverb: no silent window inside the early field")
{
  // The gap itself, stated directly: once the reverb has started, it does not stop
  // again. Hall used to be 50 dB down across most of the first 50 ms and Plate had
  // nothing at all before 172 ms.
  struct Case
  {
    const char* name;
    int mode;
    int subMode;
  };
  const Case cases[] = {{"hall", dsp::effect::Reverb::kModeHall, 0},
                        {"plate", dsp::effect::Reverb::kModePlate, 0},
                        {"oktaverb-shimmer", dsp::effect::Reverb::kModeOktaverb, 1}};

  for (const auto& c : cases)
  {
    INFO("mode=" << c.name);
    const auto env = Envelope(RenderWet(c.mode, c.subMode, 2.5, 5.0, 0.0));
    const double quietest = QuietestWindowDb(env, 10.0, 250.0);
    INFO("quietest window in 10-250 ms = " << quietest << " dB below peak");
    // Measured around -7 dB for both, against -195 dB for Hall and -200 dB for Plate
    // before the fix - those are digital silence, which is exactly the complaint.
    CHECK(quietest > -40.0);
  }
}

TEST_CASE("Reverb: the early field is continuous rather than a handful of taps")
{
  // Measured: 1.21 for Hall and 1.34 for Plate against 3.37 and 4.12 before the fix,
  // so the threshold sits between them with room on both sides. A value near 1 means
  // every window in the first 200 ms carries about the same energy.
  const auto hall = Envelope(RenderWet(dsp::effect::Reverb::kModeHall, 0, 2.5, 5.0, 0.0));
  const double hallCrest = CrestFactor(hall, 0.0, 200.0);
  INFO("hall crest=" << hallCrest);
  CHECK(hallCrest < 2.0);

  const auto plate = Envelope(RenderWet(dsp::effect::Reverb::kModePlate, 0, 2.5, 5.0, 0.0));
  const double plateCrest = CrestFactor(plate, 0.0, 200.0);
  INFO("plate crest=" << plateCrest);
  CHECK(plateCrest < 2.0);
}

// ---------------------------------------------------------------------------
// The slapback
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: the tail carries no periodic repeat")
{
  // Plate is the one that rang: 672 + 4453 and 908 + 4217 are both 5125 samples, so
  // both halves fired together every 172 ms. Hall's eight equally-weighted taps ripple
  // for the same kind of reason. The lag window covers slapback territory either way.
  struct Case
  {
    const char* name;
    int mode;
    int subMode;
  };
  const Case cases[] = {{"hall", dsp::effect::Reverb::kModeHall, 0},
                        {"plate", dsp::effect::Reverb::kModePlate, 0},
                        {"oktaverb-shimmer", dsp::effect::Reverb::kModeOktaverb, 1}};

  for (const auto& c : cases)
  {
    INFO("mode=" << c.name);
    const auto env = Envelope(RenderWet(c.mode, c.subMode, 2.5, 5.0, 0.0));
    const double periodicity = PeriodicityPeak(env, 30.0, 400.0, 0.0, 1200.0);
    INFO("periodicity=" << periodicity);
    // Measured: Plate 0.02 against 0.76 before the fix, Hall 0.36, Oktaverb 0.19.
    // Hall keeps some ripple - eight equally weighted lines in a Hadamard network
    // always will - and the threshold is set to allow it while still rejecting a
    // train of repeats.
    CHECK(periodicity < 0.45);
  }
}

TEST_CASE("Reverb: Plate produces a stereo image rather than two copies of one signal")
{
  // 1.2.0 panned tank half 0 to the left and half 1 to the right at 0.6/0.4. With both
  // halves the same length that cannot decorrelate anything - the two channels carried
  // the same events at the same instants. Dattorro's output network reads seven taps
  // per channel from inside both halves instead.
  const auto plate = RenderWet(dsp::effect::Reverb::kModePlate, 0, 2.5, 5.0, 0.0);
  const double correlation = InterChannelCorrelation(plate);
  INFO("L/R correlation=" << correlation);
  // Measured at 0.002, against 0.93 before the fix.
  CHECK(std::abs(correlation) < 0.5);
}

// ---------------------------------------------------------------------------
// Guards on the fix itself
// ---------------------------------------------------------------------------

TEST_CASE("Reverb: Plate tank halves are different lengths")
{
  // Stated as a property of the response rather than of the constants, so it holds
  // whoever edits the table: if the two halves were the same length again, they would
  // fire together and the correlation and periodicity checks above would both move.
  // This one pins the sample rate scaling too - the offsets are scaled from Dattorro's
  // 29761 Hz reference, so a rounding change that collapsed them would show here.
  for (double sr : {44100.0, 48000.0, 96000.0})
  {
    INFO("sr=" << sr);
    const size_t frames = static_cast<size_t>(sr * 1.5);
    dsp::effect::Reverb reverb;
    reverb.SetParams(1.0, 2.5, 5.0, 0.0, 0.0, dsp::effect::Reverb::kModePlate, sr, 0);
    std::vector<double> in(frames, 0.0);
    in[0] = 1.0;
    std::vector<double> inR = in;
    double* inputs[2] = {in.data(), inR.data()};
    auto** out = reverb.Process(inputs, 2, frames);

    double num = 0.0, dl = 0.0, dr = 0.0;
    for (size_t i = 0; i < frames; ++i)
    {
      num += out[0][i] * out[1][i];
      dl += out[0][i] * out[0][i];
      dr += out[1][i] * out[1][i];
    }
    REQUIRE(dl > 0.0);
    REQUIRE(dr > 0.0);
    const double correlation = num / std::sqrt(dl * dr);
    INFO("correlation=" << correlation);
    CHECK(std::abs(correlation) < 0.6);
  }
}

TEST_CASE("Reverb: the fix did not change how loud the reverb is")
{
  // Broadband, because a single sine only probes wherever that frequency happens to
  // sit among the tank's modes: measured with a 220 Hz tone the new Hall read 2.2x the
  // old one and with 382 Hz it read 0.46x, both of them modal accidents rather than a
  // level change. Noise averages over the modes and gives the number a listener hears.
  //
  // Measured at 48 kHz, decay 2.5 s, tone 5, full wet. Plate and Oktaverb come out
  // within 0.1 dB of the topology they replace. Hall is about 2 dB louder, which is
  // the early field it did not previously have; the tank's own gain is untouched.
  auto wetRms = [](int mode, int sub) {
    const size_t frames = 512;
    dsp::effect::Reverb reverb;
    reverb.SetParams(1.0, 2.5, 5.0, 0.0, 0.0, mode, kSR, sub);
    std::vector<double> l(frames), r(frames);
    double* inputs[2] = {l.data(), r.data()};
    double acc = 0.0;
    int counted = 0;
    unsigned state = 22222u;
    for (int block = 0; block < 200; ++block)
    {
      for (size_t i = 0; i < frames; ++i)
      {
        state = state * 1103515245u + 12345u;
        l[i] = 0.3 * (static_cast<double>((state >> 8) & 0xFFFFu) / 32768.0 - 1.0);
        r[i] = l[i];
      }
      auto** out = reverb.Process(inputs, 2, frames);
      // Discard the first half: the tank has to fill before the level means anything.
      if (block >= 100)
      {
        double s = 0.0;
        for (size_t i = 0; i < frames; ++i)
          s += out[0][i] * out[0][i];
        acc += std::sqrt(s / frames);
        ++counted;
      }
    }
    return acc / counted;
  };

  // Generous bands - this is a guard against a future edit moving the wet bus by a
  // stage, not a pin on the exact value.
  const double hall = wetRms(dsp::effect::Reverb::kModeHall, 0);
  INFO("hall=" << hall);
  CHECK(hall > 0.045);
  CHECK(hall < 0.095);

  const double plate = wetRms(dsp::effect::Reverb::kModePlate, 0);
  INFO("plate=" << plate);
  CHECK(plate > 0.085);
  CHECK(plate < 0.165);

  const double okta = wetRms(dsp::effect::Reverb::kModeOktaverb, 1);
  INFO("okta=" << okta);
  CHECK(okta > 0.105);
  CHECK(okta < 0.205);
}

TEST_CASE("Reverb: the fixed topology stays finite and bounded")
{
  // The early taps and the seven-tap output network both add summing paths, and the
  // plate's loop is twice as long as it was. Long decay, extreme tone, sustained loud
  // input - the combination most likely to expose an accumulation bug.
  for (int mode : {dsp::effect::Reverb::kModeHall, dsp::effect::Reverb::kModePlate, dsp::effect::Reverb::kModeOktaverb})
  {
    INFO("mode=" << mode);
    dsp::effect::Reverb reverb;
    reverb.SetParams(1.0, 10.0, 10.0, 0.0, 1.0, mode, kSR, 1);

    const size_t frames = 512;
    std::vector<double> l(frames), r(frames);
    double* inputs[2] = {l.data(), r.data()};
    double peak = 0.0;
    for (int block = 0; block < 400; ++block)
    {
      for (size_t i = 0; i < frames; ++i)
      {
        const double t = static_cast<double>(block * frames + i) / kSR;
        l[i] = 0.9 * std::sin(2.0 * 3.14159265358979 * 220.0 * t);
        r[i] = l[i];
      }
      auto** out = reverb.Process(inputs, 2, frames);
      for (size_t i = 0; i < frames; ++i)
      {
        REQUIRE(std::isfinite(out[0][i]));
        REQUIRE(std::isfinite(out[1][i]));
        peak = std::max(peak, std::max(std::abs(out[0][i]), std::abs(out[1][i])));
      }
    }
    INFO("peak=" << peak);
    CHECK(peak < 4.0);
  }
}
