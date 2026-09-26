#include "third_party/doctest.h"
#include "../VoLumTunerDSP.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

// The tuner analysis (YIN) is due every 4096 samples. As one scalar loop in one
// audio callback it took 4-13 ms, overrunning every small host buffer each time
// the tuner measured. The SIMD difference function must give the scalar loop's
// exact bits and cost a fraction of it, and the analysis is spread over the
// following blocks so no single block overruns, even on a slow core.

#if defined(__SANITIZE_ADDRESS__)
  #define VOLUM_TUNER_BURST_SANITIZED 1
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
    #define VOLUM_TUNER_BURST_SANITIZED 1
  #endif
#endif

namespace
{
#if !defined(NDEBUG) || defined(VOLUM_TUNER_BURST_SANITIZED) || (defined(__GNUC__) && !defined(__OPTIMIZE__))
constexpr bool kSkipTiming = true;
#else
constexpr bool kSkipTiming = false;
#endif

constexpr int kN = volum::TunerDSP::kBufferSize;
constexpr int kHalf = kN / 2;
constexpr float kSampleRate = 48000.f;
constexpr double kPi = 3.14159265358979323846;

// The 1.3.0 difference loop, kept verbatim as the reference.
void ReferenceDifference(const float* x, float* d)
{
  d[0] = 0.f;
  for (int tau = 1; tau < kHalf; ++tau)
  {
    float sum = 0.f;
    for (int j = 0; j < kHalf; ++j)
    {
      const float diff = x[j] - x[j + tau];
      sum += diff * diff;
    }
    d[tau] = sum;
  }
}

float ChordSample(long long n)
{
  const double t = static_cast<double>(n) / kSampleRate;
  return static_cast<float>(0.08
                            * (std::sin(2.0 * kPi * 82.41 * t) + std::sin(2.0 * kPi * 123.47 * t)
                               + std::sin(2.0 * kPi * 164.81 * t) + std::sin(2.0 * kPi * 207.65 * t)));
}

std::vector<float> Sine(float freq, float amplitude)
{
  std::vector<float> v(kN);
  for (int i = 0; i < kN; ++i)
    v[i] = amplitude * std::sin(2.f * static_cast<float>(kPi) * freq * i / kSampleRate);
  return v;
}

std::vector<float> Chord(int length = kN)
{
  std::vector<float> v(static_cast<size_t>(length));
  for (int i = 0; i < length; ++i)
    v[static_cast<size_t>(i)] = ChordSample(i);
  return v;
}

std::vector<float> Noise(unsigned seed, float amplitude, int length = kN)
{
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(-amplitude, amplitude);
  std::vector<float> v(static_cast<size_t>(length));
  for (float& s : v)
    s = dist(rng);
  return v;
}

void CheckMatchesReference(const float* x, const float* d, const std::string& label)
{
  std::vector<float> ref(kHalf, -2.f);
  ReferenceDifference(x, ref.data());
  int mismatches = 0;
  int firstTau = -1;
  for (int tau = 0; tau < kHalf; ++tau)
  {
    if (std::memcmp(&d[tau], &ref[tau], sizeof(float)) != 0)
    {
      if (firstTau < 0)
        firstTau = tau;
      ++mismatches;
    }
  }
  INFO(label << ": " << mismatches << " taus differ, first at tau " << firstTau);
  CHECK(mismatches == 0);
}

void RequireBitIdentical(const std::vector<float>& x, const std::string& label)
{
  std::vector<float> fast(kHalf, -1.f);
  volum::TunerDSP::DifferenceFunction(x.data(), fast.data());
  CheckMatchesReference(x.data(), fast.data(), label);
}

// Feeds `signal` in blocks of `block` frames until the first analysis has published, then checks the
// tuner's sliced d[] against the reference over the snapshot it took (the last kN samples of the block
// the analysis became due in).
void CheckSlicedAnalysis(const std::vector<float>& signal, int block, int testTausPerBlock, const std::string& label)
{
  volum::TunerDSP tuner;
  tuner.Reset(kSampleRate);
  tuner.SetSliceTausPerBlockForTest(testTausPerBlock);
  tuner.SetActive(true);

  const int dueBlocks = (kN + block - 1) / block;
  const size_t snapshotEnd = static_cast<size_t>(dueBlocks) * static_cast<size_t>(block);
  size_t fed = 0;
  int blocks = 0;
  auto feed = [&] {
    tuner.Process(signal.data() + fed, block);
    fed += static_cast<size_t>(block);
    ++blocks;
  };
  while (blocks < dueBlocks)
    feed();
  while (tuner.AnalysisInProgress() && fed + static_cast<size_t>(block) <= signal.size())
    feed();
  INFO(label << ", block " << block << ", taus per block " << testTausPerBlock);
  REQUIRE_FALSE(tuner.AnalysisInProgress());
  CheckMatchesReference(signal.data() + (snapshotEnd - kN), tuner.Difference(), label);
}

std::vector<float> TwoTones(float first, float second, int firstSamples, int total)
{
  std::vector<float> v(static_cast<size_t>(total));
  for (int i = 0; i < total; ++i)
    v[static_cast<size_t>(i)] =
      0.5f * std::sin(2.f * static_cast<float>(kPi) * (i < firstSamples ? first : second) * i / kSampleRate);
  return v;
}

double Median(std::vector<double> v)
{
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

bool OnCi()
{
  return std::getenv("CI") != nullptr || std::getenv("GITHUB_ACTIONS") != nullptr;
}
} // namespace

TEST_CASE("Tuner difference function is bit-identical to the scalar loop")
{
  RequireBitIdentical(Sine(440.f, 0.5f), "A4 sine");
  RequireBitIdentical(Sine(82.41f, 0.9f), "E2 sine");
  RequireBitIdentical(Chord(), "E chord");
  RequireBitIdentical(Noise(1, 1.f), "full-scale noise");
  RequireBitIdentical(Noise(7, 0.001f), "noise near the silence floor");

  std::vector<float> mixed = Noise(3, 0.05f);
  for (int i = 0; i < kN; i += 97)
    mixed[i] = (i % 2 == 0) ? 1.5f : -1e-30f;
  RequireBitIdentical(mixed, "noise with spikes and tiny values");
}

TEST_CASE("Sliced tuner analysis gives the scalar loop's d[] at every block size")
{
  const int length = 40000;
  const std::vector<float> chord = Chord(length);
  const std::vector<float> noise = Noise(11, 0.5f, length);
  for (const int block : {1, 16, 64, 100, 128, 1000, 4096})
  {
    CheckSlicedAnalysis(chord, block, 0, "chord");
    CheckSlicedAnalysis(noise, block, 0, "noise");
  }
  // Ragged slice edges: not multiples of the 4- or 32-tau SIMD blocks.
  for (const int taus : {5, 33, 127})
  {
    CheckSlicedAnalysis(chord, 64, taus, "chord");
    CheckSlicedAnalysis(noise, 64, taus, "noise");
  }
}

TEST_CASE("Tuner readout at 64 frames lands 15 blocks after the analysis is due, every 4096 samples")
{
  constexpr int kBlock = 64;
  const std::vector<float> signal = TwoTones(440.f, 220.f, kN, kBlock * 160);
  volum::TunerDSP tuner;
  tuner.Reset(kSampleRate);
  tuner.SetActive(true);
  for (int b = 0; b < 160; ++b)
  {
    tuner.Process(signal.data() + static_cast<size_t>(b) * kBlock, kBlock);
    const auto r = tuner.GetResult();
    INFO("block " << b);
    // Due in block 63 (4096 samples) and 127; each takes 16 blocks at 128 taus per block.
    CHECK(tuner.AnalysisInProgress() == ((b >= 63 && b < 78) || (b >= 127 && b < 142)));
    if (b < 78)
      CHECK_FALSE(r.valid);
    else if (b < 142)
    {
      REQUIRE(r.valid);
      CHECK(r.frequency == doctest::Approx(440.f).epsilon(0.02));
    }
    else
    {
      REQUIRE(r.valid);
      CHECK(r.frequency == doctest::Approx(220.f).epsilon(0.02));
    }
  }
}

TEST_CASE("A tuner analysis due while one is running starts in the block after that one publishes")
{
  constexpr int kBlock = 64;
  constexpr int kBlocks = 1100;
  const std::vector<float> signal = TwoTones(440.f, 220.f, kN, kBlock * kBlocks);
  volum::TunerDSP tuner;
  tuner.Reset(kSampleRate);
  tuner.SetSliceTausPerBlockForTest(4); // 512 blocks per analysis; dues every 64 blocks pile up
  tuner.SetActive(true);
  for (int b = 0; b < kBlocks; ++b)
  {
    tuner.Process(signal.data() + static_cast<size_t>(b) * kBlock, kBlock);
    const auto r = tuner.GetResult();
    INFO("block " << b);
    // The dues merged while the first ran start the second at 575; the dues during that one start the third.
    CHECK(tuner.AnalysisInProgress() == ((b >= 63 && b < 574) || (b >= 575 && b < 1086) || b >= 1087));
    if (b < 574)
      CHECK_FALSE(r.valid);
    else if (b < 1086)
    {
      REQUIRE(r.valid);
      CHECK(r.frequency == doctest::Approx(440.f).epsilon(0.02));
    }
    else
    {
      REQUIRE(r.valid);
      CHECK(r.frequency == doctest::Approx(220.f).epsilon(0.02));
    }
    if (b == 1086)
    {
      const size_t snapshotEnd = static_cast<size_t>(576) * kBlock;
      CheckMatchesReference(signal.data() + (snapshotEnd - kN), tuner.Difference(), "second analysis");
    }
  }
}

TEST_CASE("Closing the tuner mid-analysis never publishes that snapshot after it reopens")
{
  constexpr int kBlock = 64;
  const std::vector<float> signal = Sine(220.f, 0.5f);
  volum::TunerDSP tuner;
  tuner.Reset(kSampleRate);
  tuner.SetActive(true);
  auto block = [&](int b) { tuner.Process(signal.data() + (static_cast<size_t>(b) * kBlock) % kN, kBlock); };
  for (int b = 0; b < 70; ++b)
    block(b);
  REQUIRE(tuner.AnalysisInProgress());
  tuner.SetActive(false);
  for (int b = 70; b < 80; ++b)
    block(b);
  tuner.SetActive(true);
  // 384 samples were collected before closing, so the next analysis is due 58 blocks after reopening.
  for (int r = 0; r < 73; ++r)
  {
    block(80 + r);
    INFO("block " << r << " after reopening");
    CHECK(tuner.GetResult().valid == (r >= 72));
  }
}

TEST_CASE("Tuner analysis blocks cost a fraction of the scalar loop" * doctest::skip(kSkipTiming))
{
  constexpr int kBlock = 64;
  constexpr int kBlocksPerAnalysis = kN / kBlock;
  constexpr int kWarmup = 3;
  constexpr int kRuns = 25;

  volum::TunerDSP tuner;
  tuner.Reset(kSampleRate);
  tuner.SetActive(true);

  std::vector<float> stream(kN);
  std::vector<float> refD(kHalf);
  long long n = 0;
  double refSink = 0.0;
  double worst = 0.0;
  std::vector<double> perAnalysis, reference;

  for (int run = 0; run < kWarmup + kRuns; ++run)
  {
    for (int i = 0; i < kN; ++i)
      stream[i] = ChordSample(n++);

    // Every block of one 4096-sample period: one whole analysis runs across them in steady state.
    auto timeAnalysis = [&] {
      double total = 0.0;
      for (int b = 0; b < kBlocksPerAnalysis; ++b)
      {
        const auto t0 = std::chrono::steady_clock::now();
        tuner.Process(stream.data() + static_cast<size_t>(b) * kBlock, kBlock);
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        total += us;
        if (run >= kWarmup)
          worst = std::max(worst, us);
      }
      return total;
    };
    auto timeReference = [&] {
      const auto t0 = std::chrono::steady_clock::now();
      ReferenceDifference(stream.data(), refD.data());
      const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
      refSink += refD[kHalf / 3];
      return us;
    };

    double a = 0.0, r = 0.0;
    if (run % 2 == 0)
    {
      a = timeAnalysis();
      r = timeReference();
    }
    else
    {
      r = timeReference();
      a = timeAnalysis();
    }
    if (run >= kWarmup)
    {
      perAnalysis.push_back(a);
      reference.push_back(r);
    }
  }

  REQUIRE(tuner.GetResult().valid);
  CHECK(refSink > 0.0);

  const double analysisMedian = Median(perAnalysis);
  const double referenceMedian = Median(reference);
  const double ratio = analysisMedian / std::max(1e-3, referenceMedian);
  INFO("tuner cost per analysis (all blocks of 4096 samples) median "
       << analysisMedian << " us, worst block " << worst << " us; scalar difference loop median " << referenceMedian
       << " us; ratio " << ratio);
  CHECK(ratio <= 0.35);
  // Every block must fit a 64-frame buffer at 48 kHz. Hosted runners share cores.
  if (!OnCi())
    CHECK(worst < 1e6 * kBlock / kSampleRate);
}
