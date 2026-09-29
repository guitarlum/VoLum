#pragma once

#include <atomic>
#include <cmath>
#include <cstring>
#include <string>

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  #include <emmintrin.h>
  #define VOLUM_TUNER_SSE2 1
#elif defined(__aarch64__) && defined(__ARM_NEON)
  #include <arm_neon.h>
  #define VOLUM_TUNER_NEON 1
#endif

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace volum
{

struct TunerResult
{
  float frequency = 0.f;
  float cents = 0.f;
  int noteIndex = -1; // 0=C, 1=C#, ..., 11=B
  int octave = 0;
  bool valid = false;
};

class TunerDSP
{
public:
  static constexpr int kBufferSize = 4096;
  static constexpr float kReferenceA4 = 440.f;
  static constexpr float kYINThreshold = 0.12f;
  static constexpr float kMinFreq = 20.f;
  static constexpr float kMaxFreq = 2000.f;
  static constexpr float kSmoothingAlpha = 0.22f;
  // An analysis is due every kBufferSize samples. It snapshots the last kBufferSize samples in the block
  // it becomes due, then computes kSliceTausPerFrame difference taus per processed frame, so no block
  // carries more than its share of the work. Blocks of 1024 frames or more finish in the block the
  // analysis became due; smaller blocks publish about 1024 samples later. An analysis that becomes due
  // while one is running starts in the block after that one publishes; further dues until then merge.
  static constexpr int kSliceTausPerFrame = 2;

  void Reset(double sampleRate)
  {
    mSampleRate = static_cast<float>(sampleRate);
    mWritePos = 0;
    mSamplesCollected = 0;
    mNextTau = 0;
    mAnalysisDue = false;
    mWasActive = false;
    mSmoothedFreq = 0.f;
    mSmoothedValid = false;
    mInvalidDetections = 0;
    std::memset(mBuffer, 0, sizeof(mBuffer));
    mResult.store({});
  }

  void Process(const float* input, int nFrames)
  {
    if (!mActive.load(std::memory_order_relaxed))
    {
      mWasActive = false;
      return;
    }

    for (int i = 0; i < nFrames; ++i)
    {
      mBuffer[mWritePos] = input[i];
      mWritePos = (mWritePos + 1) % kBufferSize;
      ++mSamplesCollected;
    }
    _Analyze(nFrames);
  }

  void Process(const double* input, int nFrames)
  {
    if (!mActive.load(std::memory_order_relaxed))
    {
      mWasActive = false;
      return;
    }

    for (int i = 0; i < nFrames; ++i)
    {
      mBuffer[mWritePos] = static_cast<float>(input[i]);
      mWritePos = (mWritePos + 1) % kBufferSize;
      ++mSamplesCollected;
    }
    _Analyze(nFrames);
  }

  // Test hook: a fixed number of taus per block instead of kSliceTausPerFrame per frame (0 = off).
  void SetSliceTausPerBlockForTest(int taus) { mTestTausPerBlock = taus; }
  // d[] of the analysis in progress or last finished; d[0] is 0.
  const float* Difference() const { return mDiff; }
  bool AnalysisInProgress() const { return mNextTau > 0; }

  void SetActive(bool active)
  {
    mActive.store(active, std::memory_order_relaxed);
    if (active)
    {
      mSmoothedFreq = 0.f;
      mSmoothedValid = false;
      mInvalidDetections = 0;
      mResult.store({});
    }
  }
  bool IsActive() const { return mActive.load(std::memory_order_relaxed); }
  TunerResult GetResult() const { return mResult.load(std::memory_order_relaxed); }

  static const char* NoteName(int noteIndex)
  {
    static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (noteIndex < 0 || noteIndex > 11)
      return "?";
    return names[noteIndex];
  }

  // YIN difference: d[tau] = sum over j < kBufferSize / 2 of (x[j] - x[j + tau])^2, d[0] = 0. SIMD runs
  // one lane per tau, each summing in ascending j. SSE2 and NEON both multiply and add separately.
  // A fused arm64 add (`vfmaq`) does not match the scalar loop on the CI clang, so d[] would drift.
  static void DifferenceFunction(const float* x, float* d)
  {
    d[0] = 0.f;
    DifferenceRange(x, d, 1, kBufferSize / 2);
  }

  // d[tau] for tauBegin <= tau < tauEnd, with 1 <= tauBegin and tauEnd <= kBufferSize / 2.
  static void DifferenceRange(const float* x, float* d, int tauBegin, int tauEnd)
  {
    constexpr int halfBuf = kBufferSize / 2;
    int tau = tauBegin;
#if defined(VOLUM_TUNER_SSE2)
    for (; tau + 32 <= tauEnd; tau += 32)
    {
      __m128 acc0 = _mm_setzero_ps(), acc1 = _mm_setzero_ps(), acc2 = _mm_setzero_ps(), acc3 = _mm_setzero_ps();
      __m128 acc4 = _mm_setzero_ps(), acc5 = _mm_setzero_ps(), acc6 = _mm_setzero_ps(), acc7 = _mm_setzero_ps();
      for (int j = 0; j < halfBuf; ++j)
      {
        const __m128 xj = _mm_set1_ps(x[j]);
        const float* lag = x + j + tau;
        acc0 = _SquareDiffAdd(acc0, xj, lag);
        acc1 = _SquareDiffAdd(acc1, xj, lag + 4);
        acc2 = _SquareDiffAdd(acc2, xj, lag + 8);
        acc3 = _SquareDiffAdd(acc3, xj, lag + 12);
        acc4 = _SquareDiffAdd(acc4, xj, lag + 16);
        acc5 = _SquareDiffAdd(acc5, xj, lag + 20);
        acc6 = _SquareDiffAdd(acc6, xj, lag + 24);
        acc7 = _SquareDiffAdd(acc7, xj, lag + 28);
      }
      _mm_storeu_ps(d + tau, acc0);
      _mm_storeu_ps(d + tau + 4, acc1);
      _mm_storeu_ps(d + tau + 8, acc2);
      _mm_storeu_ps(d + tau + 12, acc3);
      _mm_storeu_ps(d + tau + 16, acc4);
      _mm_storeu_ps(d + tau + 20, acc5);
      _mm_storeu_ps(d + tau + 24, acc6);
      _mm_storeu_ps(d + tau + 28, acc7);
    }
    for (; tau + 4 <= tauEnd; tau += 4)
    {
      __m128 acc = _mm_setzero_ps();
      for (int j = 0; j < halfBuf; ++j)
        acc = _SquareDiffAdd(acc, _mm_set1_ps(x[j]), x + j + tau);
      _mm_storeu_ps(d + tau, acc);
    }
#elif defined(VOLUM_TUNER_NEON)
    for (; tau + 32 <= tauEnd; tau += 32)
    {
      float32x4_t acc0 = vdupq_n_f32(0.f), acc1 = vdupq_n_f32(0.f), acc2 = vdupq_n_f32(0.f), acc3 = vdupq_n_f32(0.f);
      float32x4_t acc4 = vdupq_n_f32(0.f), acc5 = vdupq_n_f32(0.f), acc6 = vdupq_n_f32(0.f), acc7 = vdupq_n_f32(0.f);
      for (int j = 0; j < halfBuf; ++j)
      {
        const float32x4_t xj = vdupq_n_f32(x[j]);
        const float* lag = x + j + tau;
        acc0 = _SquareDiffAdd(acc0, xj, lag);
        acc1 = _SquareDiffAdd(acc1, xj, lag + 4);
        acc2 = _SquareDiffAdd(acc2, xj, lag + 8);
        acc3 = _SquareDiffAdd(acc3, xj, lag + 12);
        acc4 = _SquareDiffAdd(acc4, xj, lag + 16);
        acc5 = _SquareDiffAdd(acc5, xj, lag + 20);
        acc6 = _SquareDiffAdd(acc6, xj, lag + 24);
        acc7 = _SquareDiffAdd(acc7, xj, lag + 28);
      }
      vst1q_f32(d + tau, acc0);
      vst1q_f32(d + tau + 4, acc1);
      vst1q_f32(d + tau + 8, acc2);
      vst1q_f32(d + tau + 12, acc3);
      vst1q_f32(d + tau + 16, acc4);
      vst1q_f32(d + tau + 20, acc5);
      vst1q_f32(d + tau + 24, acc6);
      vst1q_f32(d + tau + 28, acc7);
    }
    for (; tau + 4 <= tauEnd; tau += 4)
    {
      float32x4_t acc = vdupq_n_f32(0.f);
      for (int j = 0; j < halfBuf; ++j)
        acc = _SquareDiffAdd(acc, vdupq_n_f32(x[j]), x + j + tau);
      vst1q_f32(d + tau, acc);
    }
#endif
    for (; tau < tauEnd; ++tau)
    {
#if defined(__clang__)
  #pragma clang fp contract(off)
#endif
      float sum = 0.f;
#if defined(__clang__)
  #pragma clang loop unroll(disable)
#endif
      for (int j = 0; j < halfBuf; ++j)
      {
        const float diff = x[j] - x[j + tau];
        const float sq = diff * diff;
        sum += sq;
      }
      d[tau] = sum;
    }
  }

private:
#if defined(VOLUM_TUNER_SSE2)
  static inline __m128 _SquareDiffAdd(__m128 acc, __m128 xj, const float* lag)
  {
    const __m128 diff = _mm_sub_ps(xj, _mm_loadu_ps(lag));
    return _mm_add_ps(acc, _mm_mul_ps(diff, diff));
  }
#elif defined(VOLUM_TUNER_NEON)
  static inline float32x4_t _SquareDiffAdd(float32x4_t acc, float32x4_t xj, const float* lag)
  {
    const float32x4_t diff = vsubq_f32(xj, vld1q_f32(lag));
    return vaddq_f32(acc, vmulq_f32(diff, diff));
  }
#endif

  void _Analyze(int nFrames)
  {
    if (!mWasActive)
    {
      // A snapshot taken before the tuner was closed never publishes after it reopens.
      mWasActive = true;
      mNextTau = 0;
      mAnalysisDue = false;
    }
    if (mSamplesCollected >= kBufferSize)
    {
      mSamplesCollected = 0;
      mAnalysisDue = true;
    }
    if (mNextTau == 0 && mAnalysisDue)
    {
      mAnalysisDue = false;
      _BeginAnalysis();
    }
    if (mNextTau > 0)
    {
      const int budget = mTestTausPerBlock > 0 ? mTestTausPerBlock : kSliceTausPerFrame * nFrames;
      const int end = (budget >= kBufferSize / 2 - mNextTau) ? kBufferSize / 2 : mNextTau + budget;
      DifferenceRange(mAnalysisBuffer, mDiff, mNextTau, end);
      mNextTau = end < kBufferSize / 2 ? end : 0;
      if (mNextTau == 0)
        _FinishAnalysis();
    }
  }

  void _BeginAnalysis()
  {
    const int tail = kBufferSize - mWritePos;
    std::memcpy(mAnalysisBuffer, mBuffer + mWritePos, static_cast<size_t>(tail) * sizeof(float));
    if (mWritePos > 0)
      std::memcpy(mAnalysisBuffer + tail, mBuffer, static_cast<size_t>(mWritePos) * sizeof(float));

    float energy = 0.f;
    for (int j = 0; j < kBufferSize; ++j)
      energy += mAnalysisBuffer[j] * mAnalysisBuffer[j];
    const float rms = std::sqrt(energy / static_cast<float>(kBufferSize));
    if (rms < 0.0005f)
    {
      _PublishInvalid();
      return;
    }
    mNextTau = 1;
  }

  void _FinishAnalysis()
  {
    const int halfBuf = kBufferSize / 2;
    const float* d = mDiff;
    float cumNorm[kBufferSize / 2];
    cumNorm[0] = 1.f;

    float runningSum = 0.f;
    for (int tau = 1; tau < halfBuf; ++tau)
    {
      runningSum += d[tau];
      cumNorm[tau] = (runningSum > 0.f) ? (d[tau] * tau / runningSum) : 1.f;
    }

    int tauEstimate = -1;
    int minTau = static_cast<int>(mSampleRate / kMaxFreq);
    int maxTau = static_cast<int>(mSampleRate / kMinFreq);
    if (minTau < 2)
      minTau = 2;
    if (maxTau >= halfBuf)
      maxTau = halfBuf - 1;

    for (int tau = minTau; tau < maxTau; ++tau)
    {
      if (cumNorm[tau] < kYINThreshold)
      {
        while (tau + 1 < maxTau && cumNorm[tau + 1] < cumNorm[tau])
          ++tau;
        tauEstimate = tau;
        break;
      }
    }

    if (tauEstimate < 0)
    {
      // No clear pitch found - find global minimum as fallback
      float minVal = cumNorm[minTau];
      tauEstimate = minTau;
      for (int tau = minTau + 1; tau < maxTau; ++tau)
      {
        if (cumNorm[tau] < minVal)
        {
          minVal = cumNorm[tau];
          tauEstimate = tau;
        }
      }
      if (minVal > 0.5f)
      {
        _PublishInvalid();
        return;
      }
    }

    // Parabolic interpolation for sub-sample accuracy
    float betterTau = static_cast<float>(tauEstimate);
    if (tauEstimate > 0 && tauEstimate < halfBuf - 1)
    {
      float s0 = cumNorm[tauEstimate - 1];
      float s1 = cumNorm[tauEstimate];
      float s2 = cumNorm[tauEstimate + 1];
      float denom = 2.f * (s0 - 2.f * s1 + s2);
      if (std::fabs(denom) > 1e-12f)
        betterTau = tauEstimate + (s0 - s2) / denom;
    }

    float freq = mSampleRate / betterTau;

    if (freq < kMinFreq || freq > kMaxFreq)
    {
      _PublishInvalid();
      return;
    }

    _PublishFrequency(freq);
  }

  TunerResult _BuildResult(float freq) const
  {
    float semitones = 12.f * std::log2(freq / kReferenceA4);
    int midiNote = static_cast<int>(std::round(semitones)) + 69;
    float cents = (semitones - (midiNote - 69)) * 100.f;

    TunerResult r;
    r.frequency = freq;
    r.noteIndex = ((midiNote % 12) + 12) % 12;
    r.octave = (midiNote / 12) - 1;
    r.cents = cents;
    r.valid = true;
    return r;
  }

  void _PublishFrequency(float freq)
  {
    if (mSmoothedValid)
    {
      const float semitoneDelta = std::fabs(12.f * std::log2(freq / mSmoothedFreq));
      mSmoothedFreq = semitoneDelta < 0.75f ? (mSmoothedFreq * (1.f - kSmoothingAlpha) + freq * kSmoothingAlpha) : freq;
    }
    else
    {
      mSmoothedFreq = freq;
      mSmoothedValid = true;
    }

    mInvalidDetections = 0;
    mResult.store(_BuildResult(mSmoothedFreq));
  }

  void _PublishInvalid()
  {
    if (mSmoothedValid && mInvalidDetections++ < 3)
    {
      mResult.store(_BuildResult(mSmoothedFreq));
      return;
    }

    mSmoothedValid = false;
    mSmoothedFreq = 0.f;
    mResult.store({});
  }

  float mBuffer[kBufferSize] = {};
  float mAnalysisBuffer[kBufferSize] = {};
  float mDiff[kBufferSize / 2] = {};
  int mWritePos = 0;
  int mSamplesCollected = 0;
  int mNextTau = 0; // next difference tau of the analysis in progress; 0 = none running
  bool mAnalysisDue = false;
  bool mWasActive = false;
  int mTestTausPerBlock = 0;
  float mSampleRate = 48000.f;
  float mSmoothedFreq = 0.f;
  bool mSmoothedValid = false;
  int mInvalidDetections = 0;

  std::atomic<bool> mActive{false};
  std::atomic<TunerResult> mResult{};
};

} // namespace volum
