#pragma once

#include <atomic>
#include <cmath>
#include <cstring>
#include <string>

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

  void Reset(double sampleRate)
  {
    mSampleRate = static_cast<float>(sampleRate);
    mWritePos = 0;
    mSamplesCollected = 0;
    mSmoothedFreq = 0.f;
    mSmoothedValid = false;
    mInvalidDetections = 0;
    std::memset(mBuffer, 0, sizeof(mBuffer));
    mResult.store({});
  }

  void Process(const float* input, int nFrames)
  {
    if (!mActive.load(std::memory_order_relaxed))
      return;

    for (int i = 0; i < nFrames; ++i)
    {
      mBuffer[mWritePos] = input[i];
      mWritePos = (mWritePos + 1) % kBufferSize;
      ++mSamplesCollected;
    }

    if (mSamplesCollected >= kBufferSize)
    {
      mSamplesCollected = 0;
      _RunYIN();
    }
  }

  void Process(const double* input, int nFrames)
  {
    if (!mActive.load(std::memory_order_relaxed))
      return;

    for (int i = 0; i < nFrames; ++i)
    {
      mBuffer[mWritePos] = static_cast<float>(input[i]);
      mWritePos = (mWritePos + 1) % kBufferSize;
      ++mSamplesCollected;
    }

    if (mSamplesCollected >= kBufferSize)
    {
      mSamplesCollected = 0;
      _RunYIN();
    }
  }

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

private:
  void _RunYIN()
  {
    const int halfBuf = kBufferSize / 2;
    float d[kBufferSize / 2];
    float cumNorm[kBufferSize / 2];

    const int tail = kBufferSize - mWritePos;
    std::memcpy(mAnalysisBuffer, mBuffer + mWritePos, static_cast<size_t>(tail) * sizeof(float));
    if (mWritePos > 0)
      std::memcpy(mAnalysisBuffer + tail, mBuffer, static_cast<size_t>(mWritePos) * sizeof(float));

    d[0] = 0.f;
    cumNorm[0] = 1.f;

    float runningSum = 0.f;
    float energy = 0.f;
    for (int j = 0; j < kBufferSize; ++j)
      energy += mAnalysisBuffer[j] * mAnalysisBuffer[j];
    const float rms = std::sqrt(energy / static_cast<float>(kBufferSize));
    if (rms < 0.0005f)
    {
      _PublishInvalid();
      return;
    }

    for (int tau = 1; tau < halfBuf; ++tau)
    {
      float sum = 0.f;
      for (int j = 0; j < halfBuf; ++j)
      {
        const float diff = mAnalysisBuffer[j] - mAnalysisBuffer[j + tau];
        sum += diff * diff;
      }
      d[tau] = sum;
      runningSum += sum;

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
      mSmoothedFreq = semitoneDelta < 0.75f
                        ? (mSmoothedFreq * (1.f - kSmoothingAlpha) + freq * kSmoothingAlpha)
                        : freq;
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
  int mWritePos = 0;
  int mSamplesCollected = 0;
  float mSampleRate = 48000.f;
  float mSmoothedFreq = 0.f;
  bool mSmoothedValid = false;
  int mInvalidDetections = 0;

  std::atomic<bool> mActive{false};
  std::atomic<TunerResult> mResult{};
};

} // namespace volum
