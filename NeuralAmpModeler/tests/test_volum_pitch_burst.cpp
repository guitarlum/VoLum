#include "third_party/doctest.h"
#include "golden_helpers.h"
#include "VoLumBurstTiming.h"

#include "../VoLumPitchShifter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// The pitch tracker (DROP / INSTANT / Octaver) and the POLY splice search each ran as one burst
// inside a single audio callback: 1.5-9 ms, past a 64-frame buffer. They now compute several lags
// per pass and the Octaver's two voices share one tracker. The sound contract is exact: every
// render must carry the 1.3.0 engine's bits.

#if defined(__SANITIZE_ADDRESS__)
  #define VOLUM_PITCH_BURST_SANITIZED 1
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
    #define VOLUM_PITCH_BURST_SANITIZED 1
  #endif
#endif

namespace
{
using dsp::effect::GranularVoice;
using dsp::effect::VoLumPitch;
using Character = GranularVoice::Character;

#if !defined(NDEBUG) || defined(VOLUM_PITCH_BURST_SANITIZED) || (defined(__GNUC__) && !defined(__OPTIMIZE__))
constexpr bool kSkipTiming = true;
#else
constexpr bool kSkipTiming = false;
#endif

constexpr double kPi = 3.14159265358979323846;

bool SameBits(double a, double b)
{
  return std::memcmp(&a, &b, sizeof(double)) == 0;
}

size_t FirstDifference(const std::vector<DSP_SAMPLE>& a, const std::vector<DSP_SAMPLE>& b)
{
  if (a.size() != b.size())
    return 0;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::memcmp(&a[i], &b[i], sizeof(DSP_SAMPLE)) != 0)
      return i;
  return a.size();
}

double Median(std::vector<double> v)
{
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}
// Plucked notes back to back, then a silent slot and a noise slot, so the tracker sees voiced, silent
// and unvoiced input, and the splice search sees transients.
std::vector<double> MakeProgram(double sampleRate, double seconds, bool voicedOnly = false)
{
  const size_t n = static_cast<size_t>(seconds * sampleRate);
  const double notes[] = {82.41, 110.0, 196.0, 65.41, 146.83, 329.63};
  const size_t noteLen = static_cast<size_t>(0.4 * sampleRate);
  std::vector<double> v(n, 0.0);
  std::uint32_t state = 0x5eed1234U;
  for (size_t i = 0; i < n; ++i)
  {
    const size_t slot = (i / noteLen) % (voicedOnly ? 6 : 8);
    const double t = static_cast<double>(i % noteLen) / sampleRate;
    double s = 0.0;
    if (slot < 6)
    {
      const double f0 = notes[slot];
      for (int h = 1; h <= 8; ++h)
      {
        const double fh = f0 * h;
        if (fh > 0.45 * sampleRate)
          break;
        const double tau = 1.2 / (1.0 + 0.5 * (h - 1));
        s += (0.3 / h) * std::exp(-t / tau) * std::sin(2.0 * kPi * fh * t + 0.37 * h);
      }
    }
    else if (slot == 7)
    {
      state = state * 1664525U + 1013904223U;
      s = 0.05 * (static_cast<double>((state >> 8) & 0x00ffffffU) / static_cast<double>(0x00ffffffU) * 2.0 - 1.0);
    }
    v[i] = s;
  }
  return v;
}

void RenderInto(VoLumPitch& pitch, const std::vector<double>& in, size_t from, size_t count, size_t block,
                std::vector<DSP_SAMPLE>& out)
{
  std::vector<DSP_SAMPLE> buf(block);
  const size_t end = std::min(in.size(), from + count);
  for (size_t off = from; off < end; off += block)
  {
    const size_t m = std::min(block, end - off);
    for (size_t i = 0; i < m; ++i)
      buf[i] = static_cast<DSP_SAMPLE>(in[off + i]);
    DSP_SAMPLE* ptr = buf.data();
    DSP_SAMPLE** o = pitch.Process(&ptr, 1, m);
    out.insert(out.end(), o[0], o[0] + m);
  }
}

struct PedalCase
{
  std::string name;
  double sampleRate;
  size_t block;
  VoLumPitch::Mode mode;
  double semitones;
  VoLumPitch::Voicing voicing;
  Character character;
};

std::vector<PedalCase> PedalCases()
{
  using M = VoLumPitch::Mode;
  using V = VoLumPitch::Voicing;
  return {
    {"octaver modern 48k/64", 48000.0, 64, M::Octaver, 0.0, V::Modern, Character::Instant},
    {"octaver vintage 48k/333", 48000.0, 333, M::Octaver, 0.0, V::Vintage, Character::Instant},
    {"transpose instant -5 48k/64", 48000.0, 64, M::Transpose, -5.0, V::Modern, Character::Instant},
    {"transpose instant +7 48k/64", 48000.0, 64, M::Transpose, 7.0, V::Modern, Character::Instant},
    {"transpose drop -12 48k/128", 48000.0, 128, M::Transpose, -12.0, V::Modern, Character::Drop},
    {"transpose drop +7 48k/128", 48000.0, 128, M::Transpose, 7.0, V::Modern, Character::Drop},
    {"transpose poly -12 48k/64", 48000.0, 64, M::Transpose, -12.0, V::Modern, Character::Poly},
    {"transpose poly +7 48k/64", 48000.0, 64, M::Transpose, 7.0, V::Modern, Character::Poly},
    {"octaver modern 44.1k/96", 44100.0, 96, M::Octaver, 0.0, V::Modern, Character::Instant},
    {"transpose instant -7 96k/256", 96000.0, 256, M::Transpose, -7.0, V::Modern, Character::Instant},
  };
}

std::vector<DSP_SAMPLE> RenderPedalCase(const PedalCase& c, double seconds, bool reference = false)
{
  const std::vector<double> in = MakeProgram(c.sampleRate, seconds);
  VoLumPitch pitch;
  pitch.DebugSetReferenceKernels(reference);
  pitch.Configure(c.sampleRate, static_cast<int>(c.block));
  pitch.SetParams(c.mode, c.semitones, 1.0, 0.8, 0.8, 1.0, c.voicing, 0.0, c.character);
  pitch.Reset();
  std::vector<DSP_SAMPLE> out;
  RenderInto(pitch, in, 0, in.size(), c.block, out);
  return out;
}

// Live mode and character switches without a Reset in between, then a Reset and a sample-rate change.
// After Transpose the Octaver's up voice resumes on a stale ring, so the two voices are out of step.
std::vector<DSP_SAMPLE> RenderModeSwitches(bool reference = false)
{
  using M = VoLumPitch::Mode;
  using V = VoLumPitch::Voicing;
  const std::vector<double> in = MakeProgram(48000.0, 6.0);
  const size_t block = 64;
  VoLumPitch pitch;
  pitch.DebugSetReferenceKernels(reference);
  pitch.Configure(48000.0, static_cast<int>(block));
  pitch.Reset();
  std::vector<DSP_SAMPLE> out;
  size_t pos = 0;
  auto run = [&](M mode, double semi, V voicing, Character ch, double seconds) {
    pitch.SetParams(mode, semi, 1.0, 0.8, 0.8, 1.0, voicing, 0.0, ch);
    const size_t count = static_cast<size_t>(seconds * 48000.0);
    RenderInto(pitch, in, pos, count, block, out);
    pos += count;
  };
  run(M::Transpose, -5.0, V::Modern, Character::Instant, 0.6);
  run(M::Octaver, 0.0, V::Modern, Character::Instant, 0.6);
  run(M::Transpose, 7.0, V::Modern, Character::Poly, 0.4);
  run(M::Transpose, -12.0, V::Modern, Character::Drop, 0.4);
  run(M::Octaver, 0.0, V::Vintage, Character::Drop, 0.6);
  pitch.Reset();
  run(M::Octaver, 0.0, V::Modern, Character::Instant, 0.6);
  run(M::Transpose, 5.0, V::Modern, Character::Instant, 0.3);
  run(M::Octaver, 0.0, V::Modern, Character::Instant, 0.5);
  pitch.Configure(44100.0, static_cast<int>(block));
  run(M::Octaver, 0.0, V::Modern, Character::Instant, 0.6);
  pitch.Reset();
  run(M::Octaver, 0.0, V::Vintage, Character::Instant, 0.6);
  return out;
}

// Captured from the 1.3.0 engine (pre lag-blocking) on Windows x64 (MSVC, SSE2, /fp:precise).
// macOS contracts `r += a * b` into fused multiply-adds, so its bits differ; it prints its value
// instead until one is pinned from a CI run.
void ExpectEngineHash(const std::string& name, const std::string& actual, const char* windowsExpected)
{
#if defined(_WIN32)
  INFO(name << " actual=" << actual);
  CHECK(actual == std::string(windowsExpected));
#else
  (void)windowsExpected;
  MESSAGE(name << " hash (not pinned on this platform): " << actual);
#endif
}

const char* PinnedHash(const std::string& name)
{
  struct Pin
  {
    const char* name;
    const char* hash;
  };
  static const Pin pins[] = {
    {"octaver modern 48k/64", "84b6fcc53ca05bfdf10e178c9331373e078cb330a6aa325567bb23c475f73669"},
    {"octaver vintage 48k/333", "08fe4ea0e3209f7a92b5e2864b57fe9f828b0e16d39321145c5fb003bfc04336"},
    {"transpose instant -5 48k/64", "aaf406e8ebd0455b98ace0107d6ae627050908c3bb971894d5081d5695f14682"},
    {"transpose instant +7 48k/64", "652cf573273eecbdbee675575aea4e4384eb1d6690d706c7acc41955be3af8e3"},
    {"transpose drop -12 48k/128", "7ed6acd84dd2a5ee3ebe09d639cbabde639e33e5b544e560f4cee88c727cfd45"},
    {"transpose drop +7 48k/128", "5cbe16ca48655bd0b838b61b7daf4675b17e5e20412c2ccccaf07245a72f6096"},
    {"transpose poly -12 48k/64", "de0ba450f7e7be421f872a80700e03ee280c77033d9ea46d813c09d9c018eaee"},
    {"transpose poly +7 48k/64", "3f78aa8275600be1010da24405fd7b2ce3b273dc00df7dfa8212922034527e5b"},
    {"octaver modern 44.1k/96", "1fd907cda053aae5388531ca00305b5b8cb232b8969e4b17defc3a457413d327"},
    {"transpose instant -7 96k/256", "aba7743059ff49cb321e6e011e663f84fc541b8e2525f24f574000305a206393"},
    {"mode switches", "6b05ff4223164721d54ec33dc37106ee8041da07d2b170221d6b0e2b24a7f87f"},
  };
  for (const Pin& p : pins)
    if (name == p.name)
      return p.hash;
  return "";
}
} // namespace

TEST_CASE("Pitch burst: pedal renders keep the 1.3.0 engine's exact bits")
{
  // Reference kernels: the one-lag-at-a-time tracker and splice scan, each Octaver voice tracking
  // on its own. Equal bits there prove the blocked kernels and the shared tracker on every
  // platform; the pinned hashes prove the whole engine against the build before the change.
  for (const PedalCase& c : PedalCases())
  {
    const std::vector<DSP_SAMPLE> live = RenderPedalCase(c, 2.0);
    const std::vector<DSP_SAMPLE> reference = RenderPedalCase(c, 2.0, true);
    INFO(c.name << ": first differing sample " << FirstDifference(live, reference) << " of " << live.size());
    CHECK(FirstDifference(live, reference) == live.size());
    ExpectEngineHash(c.name, volum::test::Sha256HexSamples(live), PinnedHash(c.name));
  }
  const std::vector<DSP_SAMPLE> live = RenderModeSwitches();
  const std::vector<DSP_SAMPLE> reference = RenderModeSwitches(true);
  INFO("mode switches: first differing sample " << FirstDifference(live, reference) << " of " << live.size());
  CHECK(FirstDifference(live, reference) == live.size());
  ExpectEngineHash("mode switches", volum::test::Sha256HexSamples(live), PinnedHash("mode switches"));
}

TEST_CASE("Pitch burst: Octaver replays several period updates per host block exactly")
{
  // A block longer than the 10 ms update interval holds several estimates, which the up voice replays
  // in order after the down voice has run the whole block.
  struct Run
  {
    std::string name;
    double sampleRate;
    std::vector<size_t> blocks; // cycled
    VoLumPitch::Voicing voicing;
  };
  const std::vector<Run> runs = {
    {"48k/2048", 48000.0, {2048}, VoLumPitch::Voicing::Modern},
    {"44.1k/1024", 44100.0, {1024}, VoLumPitch::Voicing::Vintage},
    {"48k mixed 37/480/481/2048/64", 48000.0, {37, 480, 481, 2048, 64}, VoLumPitch::Voicing::Modern},
  };
  for (const Run& run : runs)
  {
    const std::vector<double> in = MakeProgram(run.sampleRate, 3.0);
    std::vector<DSP_SAMPLE> outs[2];
    for (int reference = 0; reference < 2; ++reference)
    {
      VoLumPitch pitch;
      pitch.DebugSetReferenceKernels(reference == 1);
      pitch.Configure(run.sampleRate, 2048);
      pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 1.0, 0.8, 0.8, 1.0, run.voicing, 0.0);
      pitch.Reset();
      size_t pos = 0;
      for (size_t i = 0; pos < in.size(); ++i)
      {
        const size_t m = std::min(run.blocks[i % run.blocks.size()], in.size() - pos);
        RenderInto(pitch, in, pos, m, m, outs[reference]);
        pos += m;
      }
    }
    INFO(run.name << ": first differing sample " << FirstDifference(outs[0], outs[1]) << " of " << outs[0].size());
    CHECK(FirstDifference(outs[0], outs[1]) == outs[0].size());
  }
}

TEST_CASE("Pitch burst: blocked tracker matches the one-lag-at-a-time oracle")
{
  // Covers voiced notes, the silent slot (energy gate) and noise (weak peak), at three rates.
  struct Rate
  {
    double sampleRate;
    Character character;
  };
  for (const Rate& rate :
       {Rate{48000.0, Character::Drop}, Rate{44100.0, Character::Instant}, Rate{96000.0, Character::Instant}})
  {
    const std::vector<double> in = MakeProgram(rate.sampleRate, 3.3);
    const size_t block = 97;
    const size_t blocks = (in.size() + block - 1) / block;
    const size_t stride = std::max<size_t>(1, blocks / 120);
    GranularVoice voice;
    voice.Configure(rate.sampleRate, static_cast<int>(block));
    voice.SetCharacter(rate.character);
    voice.SetRatio(std::pow(2.0, -5.0 / 12.0));
    voice.Reset();
    std::vector<DSP_SAMPLE> out(block);
    int compared = 0, searched = 0, accepted = 0, mismatches = 0;
    for (size_t b = 0; b < blocks; ++b)
    {
      const size_t off = b * block;
      const size_t m = std::min(block, in.size() - off);
      voice.Process(in.data() + off, out.data(), m);
      if (b % stride != 0)
        continue;
      const auto fast = voice.DebugEstimatePeriod();
      const auto ref = voice.DebugEstimatePeriodReference();
      ++compared;
      if (ref.bestLag > 0)
        ++searched;
      if (!SameBits(ref.period, voice.DebugPeriod()))
        ++accepted;
      if (!SameBits(fast.period, ref.period) || fast.bestLag != ref.bestLag)
      {
        ++mismatches;
        INFO("sr=" << rate.sampleRate << " block " << b << ": fast " << fast.period << " @ " << fast.bestLag
                   << ", oracle " << ref.period << " @ " << ref.bestLag);
        CHECK(false);
      }
    }
    INFO("sr=" << rate.sampleRate << " compared " << compared << ", searched " << searched << ", accepted "
               << accepted);
    CHECK(mismatches == 0);
    CHECK(searched > compared / 2);
    CHECK(accepted > compared / 3);
    CHECK(searched < compared); // the silent slot keeps the last estimate
  }
}

TEST_CASE("Pitch burst: blocked splice scan matches the one-lag-at-a-time oracle")
{
  const double sr = 48000.0;
  const std::vector<double> in = MakeProgram(sr, 3.3);
  const size_t block = 61;
  const size_t blocks = (in.size() + block - 1) / block;
  const size_t stride = std::max<size_t>(1, blocks / 60);
  for (auto character : {Character::Poly, Character::Drop})
  {
    for (double semi : {-12.0, -5.0, 7.0})
    {
      GranularVoice voice;
      voice.Configure(sr, static_cast<int>(block));
      voice.SetCharacter(character);
      voice.SetRatio(std::pow(2.0, semi / 12.0));
      voice.Reset();
      const auto g = GranularVoice::SpliceGeometryFor(character, sr);
      std::vector<DSP_SAMPLE> out(block);
      int compared = 0;
      for (size_t b = 0; b < blocks; ++b)
      {
        const size_t off = b * block;
        const size_t m = std::min(block, in.size() - off);
        voice.Process(in.data() + off, out.data(), m);
        if (b % stride != 0 || b < 60)
          continue;
        const double cand = voice.DebugDelay();
        struct Range
        {
          int lagMin, lagMax;
          bool preferNearest;
        };
        for (const Range& r : {Range{-g.search, g.search, false}, Range{0, g.search, true}})
        {
          const size_t count = static_cast<size_t>(r.lagMax - r.lagMin + 1);
          const double refPick = voice.DebugWsolaRefineRangeReference(cand, r.lagMin, r.lagMax, r.preferNearest);
          const std::vector<double> refCorr(voice.DebugCorrScratch().begin(), voice.DebugCorrScratch().begin() + count);
          const double fastPick = voice.DebugWsolaRefineRange(cand, r.lagMin, r.lagMax, r.preferNearest);
          const std::vector<double>& fastCorr = voice.DebugCorrScratch();
          size_t firstDiff = count;
          for (size_t i = 0; i < count && firstDiff == count; ++i)
            if (!SameBits(refCorr[i], fastCorr[i]))
              firstDiff = i;
          INFO("character=" << static_cast<int>(character) << " semitones=" << semi << " block " << b << " range ["
                            << r.lagMin << ", " << r.lagMax << "] pick fast " << fastPick << " oracle " << refPick
                            << ", first differing lag slot " << firstDiff);
          CHECK(SameBits(fastPick, refPick));
          CHECK(firstDiff == count);
          ++compared;
        }
      }
      CHECK(compared > 50);
    }
  }
}

TEST_CASE("Pitch burst: Octaver and INSTANT tracker bursts cost a fraction of the reference"
          * doctest::skip(kSkipTiming))
{
  constexpr double kSampleRate = 48000.0;
  constexpr size_t kBlock = 64;
  constexpr int kWarmupBursts = 3;
  constexpr int kUpdateEvery = 480; // 10 ms
  constexpr int kHistory = 2 * 1200 + 2; // the tracker waits for two maximum periods of input
  struct Setup
  {
    std::string name;
    VoLumPitch::Mode mode;
    double semitones;
  };
  for (const Setup& s : {Setup{"octaver", VoLumPitch::Mode::Octaver, 0.0},
                         Setup{"transpose instant -5", VoLumPitch::Mode::Transpose, -5.0}})
  {
    const std::vector<double> in = MakeProgram(kSampleRate, 0.8, true);
    std::vector<double> liveBursts, refBursts;
    double otherWorst = 0.0;
    std::string worsts;
    const double worst = volum_test::MinWorstBlockUs(volum_test::DeadlineRuns(), [&](int run) {
      VoLumPitch live, reference;
      reference.DebugSetReferenceKernels(true);
      for (VoLumPitch* p : {&live, &reference})
      {
        p->Configure(kSampleRate, static_cast<int>(kBlock));
        p->SetParams(s.mode, s.semitones, 1.0, 0.8, 0.8, 1.0, VoLumPitch::Voicing::Modern, 0.0, Character::Instant);
        p->Reset();
      }
      std::vector<DSP_SAMPLE> a(kBlock), b(kBlock);
      std::vector<double> runLive, runRef, runLiveCpu;
      double runOtherWorst = 0.0;
      int bursts = 0;
      for (size_t off = 0; off + kBlock <= in.size(); off += kBlock)
      {
        for (size_t i = 0; i < kBlock; ++i)
          a[i] = b[i] = static_cast<DSP_SAMPLE>(in[off + i]);
        auto timeBlock = [&](VoLumPitch& p, std::vector<DSP_SAMPLE>& buf, double& cpuUs) {
          DSP_SAMPLE* ptr = buf.data();
          const double c0 = volum_test::ThreadCpuUs();
          const auto t0 = std::chrono::steady_clock::now();
          p.Process(&ptr, 1, kBlock);
          const double wall = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
          cpuUs = volum_test::ThreadCpuUs() - c0;
          return wall;
        };
        double tl = 0.0, tr = 0.0, cl = 0.0, cr = 0.0;
        if ((off / kBlock) % 2 == 0)
        {
          tl = timeBlock(live, a, cl);
          tr = timeBlock(reference, b, cr);
        }
        else
        {
          tr = timeBlock(reference, b, cr);
          tl = timeBlock(live, a, cl);
        }
        // Sample n (0-based) runs the tracker when (n + 1) is a multiple of the update interval.
        const size_t firstUpdate = ((off / kUpdateEvery) + 1) * kUpdateEvery - 1;
        const bool burst = firstUpdate < off + kBlock && firstUpdate + 1 >= static_cast<size_t>(kHistory);
        if (!burst)
        {
          runOtherWorst = std::max(runOtherWorst, tl);
          continue;
        }
        if (++bursts <= kWarmupBursts)
          continue;
        runLive.push_back(tl);
        runRef.push_back(tr);
        runLiveCpu.push_back(cl);
      }
      REQUIRE(runLive.size() >= 20);
      if (run == 0)
      {
        liveBursts = runLive;
        refBursts = runRef;
        otherWorst = runOtherWorst;
      }
      const double runWorst = *std::max_element(runLiveCpu.begin(), runLiveCpu.end());
      worsts += " " + std::to_string(runWorst);
      return runWorst;
    });
    const double liveMedian = Median(liveBursts);
    const double refMedian = Median(refBursts);
    const double ratio = liveMedian / std::max(1e-3, refMedian);
    INFO(s.name << ": burst block median " << liveMedian << " us, worst thread-CPU per run" << worsts
                << " us; reference median " << refMedian << " us; ratio " << ratio << "; " << liveBursts.size()
                << " bursts; other blocks worst " << otherWorst << " us");
    MESSAGE(s.name << ": burst block median " << liveMedian << " us, worst thread-CPU per run" << worsts
                   << " us; reference median " << refMedian << " us; ratio " << ratio);
    CHECK(ratio <= 0.35);
    // A burst must fit a 64-frame buffer at 48 kHz in at least one of the runs. Hosted runners share cores.
    if (!volum_test::OnCi())
      CHECK(worst < volum_test::DeadlineUs(static_cast<double>(kBlock), kSampleRate));
  }
}

TEST_CASE("Pitch burst: POLY splice bursts cost a fraction of the reference" * doctest::skip(kSkipTiming))
{
  constexpr double kSampleRate = 48000.0;
  constexpr size_t kBlock = 64;
  constexpr int kWarmupBursts = 3;
  for (double semi : {7.0, -12.0})
  {
    const std::vector<double> in = MakeProgram(kSampleRate, 2.0, true);
    GranularVoice live, reference;
    reference.DebugSetReferenceKernels(true);
    for (GranularVoice* v : {&live, &reference})
    {
      v->Configure(kSampleRate, static_cast<int>(kBlock));
      v->SetCharacter(Character::Poly);
      v->SetRatio(std::pow(2.0, semi / 12.0));
      v->Reset();
    }
    std::vector<DSP_SAMPLE> outA(kBlock), outB(kBlock);
    std::vector<double> liveBursts, refBursts;
    int bursts = 0;
    for (size_t off = 0; off + kBlock <= in.size(); off += kBlock)
    {
      const unsigned long long before = live.SpliceStarts();
      auto timeBlock = [&](GranularVoice& v, std::vector<DSP_SAMPLE>& out) {
        const auto t0 = std::chrono::steady_clock::now();
        v.Process(in.data() + off, out.data(), kBlock);
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
      };
      double tl = 0.0, tr = 0.0;
      if ((off / kBlock) % 2 == 0)
      {
        tl = timeBlock(live, outA);
        tr = timeBlock(reference, outB);
      }
      else
      {
        tr = timeBlock(reference, outB);
        tl = timeBlock(live, outA);
      }
      REQUIRE(live.SpliceStarts() == reference.SpliceStarts());
      if (live.SpliceStarts() == before || ++bursts <= kWarmupBursts)
        continue;
      liveBursts.push_back(tl);
      refBursts.push_back(tr);
    }
    REQUIRE(liveBursts.size() >= 20);
    const double liveMedian = Median(liveBursts);
    const double refMedian = Median(refBursts);
    const double worst = *std::max_element(liveBursts.begin(), liveBursts.end());
    const double ratio = liveMedian / std::max(1e-3, refMedian);
    INFO("POLY " << semi << ": splice block median " << liveMedian << " us, worst " << worst << " us; reference median "
                 << refMedian << " us; ratio " << ratio << "; " << liveBursts.size() << " splices");
    MESSAGE("POLY " << semi << ": splice block median " << liveMedian << " us, worst " << worst
                    << " us; reference median " << refMedian << " us; ratio " << ratio);
    CHECK(ratio <= 0.5);
  }
}

TEST_CASE("Pitch burst: reported latency is unchanged")
{
  struct Row
  {
    double sampleRate;
    int drop, instant, poly;
  };
  for (const Row& r : {Row{44100.0, 744, 380, 619}, Row{48000.0, 809, 413, 674}, Row{96000.0, 1616, 824, 1346}})
  {
    INFO("sr=" << r.sampleRate);
    CHECK(GranularVoice::LatencyFor(Character::Drop, r.sampleRate) == r.drop);
    CHECK(GranularVoice::LatencyFor(Character::Instant, r.sampleRate) == r.instant);
    CHECK(GranularVoice::LatencyFor(Character::Poly, r.sampleRate) == r.poly);
    VoLumPitch pitch;
    pitch.Configure(r.sampleRate, 64);
    pitch.SetParams(VoLumPitch::Mode::Octaver, 0.0, 1.0, 0.8, 0.8, 1.0, VoLumPitch::Voicing::Modern, 0.0);
    CHECK(pitch.Latency() == r.drop);
    pitch.SetParams(
      VoLumPitch::Mode::Transpose, -5.0, 1.0, 0.8, 0.8, 1.0, VoLumPitch::Voicing::Modern, 0.0, Character::Instant);
    CHECK(pitch.Latency() == r.instant);
  }
}
