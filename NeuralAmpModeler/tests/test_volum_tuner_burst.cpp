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

// The tuner analysis (YIN) runs inside one audio callback every 4096 samples.
// As a scalar loop it took 4-13 ms, overrunning every small host buffer each
// time the tuner measured. The SIMD difference function must give the scalar
// loop's exact bits, and cost a fraction of it.

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

std::vector<float> Chord()
{
  std::vector<float> v(kN);
  for (int i = 0; i < kN; ++i)
    v[i] = ChordSample(i);
  return v;
}

std::vector<float> Noise(unsigned seed, float amplitude)
{
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(-amplitude, amplitude);
  std::vector<float> v(kN);
  for (float& s : v)
    s = dist(rng);
  return v;
}

void RequireBitIdentical(const std::vector<float>& x, const std::string& label)
{
  std::vector<float> fast(kHalf, -1.f), ref(kHalf, -2.f);
  volum::TunerDSP::DifferenceFunction(x.data(), fast.data());
  ReferenceDifference(x.data(), ref.data());
  int mismatches = 0;
  int firstTau = -1;
  for (int tau = 0; tau < kHalf; ++tau)
  {
    if (std::memcmp(&fast[tau], &ref[tau], sizeof(float)) != 0)
    {
      if (firstTau < 0)
        firstTau = tau;
      ++mismatches;
    }
  }
  INFO(label << ": " << mismatches << " taus differ, first at tau " << firstTau);
  CHECK(mismatches == 0);
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

TEST_CASE("Tuner analysis block costs a fraction of the scalar loop" * doctest::skip(kSkipTiming))
{
  constexpr int kBlock = 64;
  constexpr int kWarmup = 3;
  constexpr int kRuns = 25;

  volum::TunerDSP tuner;
  tuner.Reset(kSampleRate);
  tuner.SetActive(true);

  std::vector<float> stream(kN);
  std::vector<float> snapshot(kN);
  std::vector<float> refD(kHalf);
  long long n = 0;
  double refSink = 0.0;
  std::vector<double> analysis, reference;

  for (int run = 0; run < kWarmup + kRuns; ++run)
  {
    for (int i = 0; i < kN; ++i)
      stream[i] = ChordSample(n++);
    std::copy(stream.begin(), stream.end(), snapshot.begin());
    // The first kN - kBlock samples only collect; the last block runs the analysis.
    tuner.Process(stream.data(), kN - kBlock);

    auto timeAnalysis = [&] {
      const auto t0 = std::chrono::steady_clock::now();
      tuner.Process(stream.data() + (kN - kBlock), kBlock);
      return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    };
    auto timeReference = [&] {
      const auto t0 = std::chrono::steady_clock::now();
      ReferenceDifference(snapshot.data(), refD.data());
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
      analysis.push_back(a);
      reference.push_back(r);
    }
  }

  REQUIRE(tuner.GetResult().valid);
  CHECK(refSink > 0.0);

  const double analysisMedian = Median(analysis);
  const double referenceMedian = Median(reference);
  const double worst = *std::max_element(analysis.begin(), analysis.end());
  const double ratio = analysisMedian / std::max(1e-3, referenceMedian);
  INFO("analysis block median " << analysisMedian << " us, worst " << worst << " us; scalar difference loop median "
                                << referenceMedian << " us; ratio " << ratio);
  CHECK(ratio <= 0.35);
  // One analysis must fit a 64-frame buffer at 48 kHz. Hosted runners share cores.
  if (!OnCi())
    CHECK(worst < 1e6 * kBlock / kSampleRate);
}
