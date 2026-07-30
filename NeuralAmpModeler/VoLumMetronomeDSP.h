#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace volum
{

enum class MetronomeTimeSig : int
{
  Quarter_1_4 = 0, // 1/4 - single click per beat
  Quarter_2_4, // 2/4
  Quarter_3_4, // 3/4
  Quarter_4_4, // 4/4
  Eighth_6_8, // 6/8
  kCount
};

inline const char* MetronomeTimeSigName(MetronomeTimeSig sig)
{
  switch (sig)
  {
    case MetronomeTimeSig::Quarter_1_4: return "1/4";
    case MetronomeTimeSig::Quarter_2_4: return "2/4";
    case MetronomeTimeSig::Quarter_3_4: return "3/4";
    case MetronomeTimeSig::Quarter_4_4: return "4/4";
    case MetronomeTimeSig::Eighth_6_8: return "6/8";
    default: return "?";
  }
}

inline int MetronomeBeatsPerMeasure(MetronomeTimeSig sig)
{
  switch (sig)
  {
    case MetronomeTimeSig::Quarter_1_4: return 1;
    case MetronomeTimeSig::Quarter_2_4: return 2;
    case MetronomeTimeSig::Quarter_3_4: return 3;
    case MetronomeTimeSig::Quarter_4_4: return 4;
    case MetronomeTimeSig::Eighth_6_8: return 6;
    default: return 4;
  }
}

inline bool MetronomeIsCompound(MetronomeTimeSig sig)
{
  return sig == MetronomeTimeSig::Eighth_6_8;
}

class MetronomeDSP
{
public:
  static constexpr float kMinBPM = 30.f;
  static constexpr float kMaxBPM = 300.f;
  static constexpr float kDefaultBPM = 120.f;
  static constexpr float kClickDurationSec = 0.012f;
  static constexpr float kDownbeatFreq = 1250.f;
  static constexpr float kMidbeatFreq = 880.f;
  static constexpr float kUpbeatFreq = 880.f;
  static constexpr float kClickPeak = 0.72f; // below full-scale; shaped transient keeps perceived level high
  static constexpr float kDownbeatGain = 1.f;
  static constexpr float kMidbeatGain = 0.82f;
  static constexpr float kUpbeatGain = 0.62f;

  void Reset(double sampleRate)
  {
    mSampleRate = static_cast<float>(sampleRate);
    mClickLenSamples = static_cast<int>(kClickDurationSec * mSampleRate);
    mCachedBPM = mBPM.load(std::memory_order_relaxed);
    mCachedTimeSig = static_cast<MetronomeTimeSig>(mTimeSig.load(std::memory_order_relaxed));
    _RecalcSamplesPerBeat();
    mResetRequested.store(true, std::memory_order_relaxed);
  }

  // Sum click into stereo output buffer
  void Process(double** outputs, int nFrames, int nChans)
  {
    if (!mActive.load(std::memory_order_relaxed))
      return;

    float vol = mVolume.load(std::memory_order_relaxed);
    float bpm = mBPM.load(std::memory_order_relaxed);
    auto sig = static_cast<MetronomeTimeSig>(mTimeSig.load(std::memory_order_relaxed));
    bool resetRequested = mResetRequested.exchange(false, std::memory_order_relaxed);

    if (bpm != mCachedBPM || sig != mCachedTimeSig)
    {
      mCachedBPM = bpm;
      mCachedTimeSig = sig;
      _RecalcSamplesPerBeat();
    }

    int beatsPerMeasure = MetronomeBeatsPerMeasure(sig);
    if (resetRequested)
      _ResetCounters(true);

    for (int s = 0; s < nFrames; ++s)
    {
      if (mSampleCounter >= mSamplesPerBeat)
      {
        mSampleCounter = 0;
        mBeatIndex = (mBeatIndex + 1) % beatsPerMeasure;
        mClickSamplePos = 0;
      }

      float clickSample = 0.f;
      if (mClickSamplePos < mClickLenSamples)
      {
        const int accent = _AccentForBeat(sig, mBeatIndex);

        float freq = (accent == 2) ? kDownbeatFreq : ((accent == 1) ? kMidbeatFreq : kUpbeatFreq);
        float gain = (accent == 2) ? kDownbeatGain : ((accent == 1) ? kMidbeatGain : kUpbeatGain);
        float t = static_cast<float>(mClickSamplePos) / mSampleRate;
        float envelope = 1.f - (static_cast<float>(mClickSamplePos) / static_cast<float>(mClickLenSamples));
        envelope *= envelope; // quadratic decay
        float raw = std::sin(2.f * static_cast<float>(M_PI) * freq * t)
                    + 0.35f * std::sin(2.f * static_cast<float>(M_PI) * freq * 2.37f * t);
        clickSample = std::tanh(raw * 1.7f) * kClickPeak * envelope * gain * vol;
        ++mClickSamplePos;
      }

      for (int ch = 0; ch < nChans; ++ch)
        outputs[ch][s] += static_cast<double>(clickSample);

      ++mSampleCounter;
    }

    // Store beat phase for UI animation (0..1 within current beat)
    float phase = static_cast<float>(mSampleCounter) / static_cast<float>(mSamplesPerBeat);
    mBeatPhase.store(phase, std::memory_order_relaxed);
  }

  void SetActive(bool active)
  {
    const bool wasActive = mActive.exchange(active, std::memory_order_relaxed);
    if (active && !wasActive)
      mResetRequested.store(true, std::memory_order_relaxed);
  }

  bool IsActive() const { return mActive.load(std::memory_order_relaxed); }

  // The isfinite guard is not redundant with the clamp: std::clamp(NaN, lo, hi)
  // returns NaN, because both of its comparisons are false. A NaN tempo divides
  // into samplesPerBeat below and its conversion to int is undefined.
  // A non-finite tempo is discarded, not substituted: replacing it with the factory
  // default would silently throw away a tempo the user set and leave the overlay's
  // readout disagreeing with what is clicking.
  void SetBPM(float bpm)
  {
    if (!std::isfinite(bpm))
      return;
    mBPM.store(std::clamp(bpm, kMinBPM, kMaxBPM), std::memory_order_relaxed);
  }
  float GetBPM() const { return mBPM.load(std::memory_order_relaxed); }

  void SetVolume(float vol) { mVolume.store(std::clamp(vol, 0.f, 1.f), std::memory_order_relaxed); }
  float GetVolume() const { return mVolume.load(std::memory_order_relaxed); }

  void SetTimeSig(MetronomeTimeSig sig) { mTimeSig.store(static_cast<int>(sig), std::memory_order_relaxed); }
  MetronomeTimeSig GetTimeSig() const
  {
    return static_cast<MetronomeTimeSig>(mTimeSig.load(std::memory_order_relaxed));
  }

  float GetBeatPhase() const { return mBeatPhase.load(std::memory_order_relaxed); }

private:
  void _ResetCounters(bool startWithClick)
  {
    mSampleCounter = 0;
    mBeatIndex = 0;
    mClickSamplePos = startWithClick ? 0 : mClickLenSamples;
    mBeatPhase.store(0.f, std::memory_order_relaxed);
  }

  void _RecalcSamplesPerBeat()
  {
    float bpm = mCachedBPM;
    // Negated comparison so a NaN that ever reaches here is also floored, rather
    // than sailing through to the division below.
    if (!(bpm >= kMinBPM))
      bpm = kMinBPM;
    bool compound = MetronomeIsCompound(mCachedTimeSig);
    // For 6/8, each "beat" is an eighth note; BPM refers to dotted-quarter = 3 eighth notes
    float effectiveBPM = compound ? (bpm * 3.f) : bpm;
    mSamplesPerBeat = static_cast<int>(mSampleRate * 60.f / effectiveBPM);
    if (mSamplesPerBeat < 1)
      mSamplesPerBeat = 1;
  }

  int _AccentForBeat(MetronomeTimeSig sig, int beatIndex) const
  {
    switch (sig)
    {
      case MetronomeTimeSig::Quarter_1_4: return 0; // plain click, no bar accent
      case MetronomeTimeSig::Quarter_2_4:
      case MetronomeTimeSig::Quarter_3_4: return beatIndex == 0 ? 2 : 0;
      case MetronomeTimeSig::Quarter_4_4: return beatIndex == 0 ? 2 : (beatIndex == 2 ? 1 : 0);
      case MetronomeTimeSig::Eighth_6_8: return beatIndex == 0 ? 2 : (beatIndex == 3 ? 1 : 0);
      default: return beatIndex == 0 ? 2 : 0;
    }
  }

  float mSampleRate = 48000.f;
  int mSamplesPerBeat = 24000;
  int mClickLenSamples = 576;
  int mSampleCounter = 0;
  int mBeatIndex = 0;
  int mClickSamplePos = 0;

  float mCachedBPM = kDefaultBPM;
  MetronomeTimeSig mCachedTimeSig = MetronomeTimeSig::Quarter_4_4;

  std::atomic<bool> mActive{false};
  std::atomic<float> mBPM{kDefaultBPM};
  std::atomic<float> mVolume{0.5f};
  std::atomic<int> mTimeSig{static_cast<int>(MetronomeTimeSig::Quarter_4_4)};
  std::atomic<float> mBeatPhase{0.f};
  std::atomic<bool> mResetRequested{false};
};

} // namespace volum
