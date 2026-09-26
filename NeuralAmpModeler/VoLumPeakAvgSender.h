#pragma once

// VoLum meter feed. PeakAvgSender pushes exactly the ISenderData sequence of
// iplug::IPeakAvgSender (pinned by test_volum_meter_sender.cpp) without the two
// integer divisions per sample, and StepMeterGate lets ProcessBlock skip the
// meters while no editor is open.

#include "../iPlug2/IPlug/IPlugEditorDelegate.h"
#include "../iPlug2/IPlug/ISender.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace volum
{

template <int MAXNC = 1, int QUEUE_SIZE = 64>
class PeakAvgSender : public iplug::ISender<MAXNC, QUEUE_SIZE, std::pair<float, float>>
{
public:
  using Base = iplug::ISender<MAXNC, QUEUE_SIZE, std::pair<float, float>>;
  using Data = iplug::ISenderData<MAXNC, std::pair<float, float>>;
  using EnvelopeFollower = typename iplug::IPeakAvgSender<MAXNC, QUEUE_SIZE>::EnvelopeFollower;

  PeakAvgSender(double minThresholdDb = -90.0, bool rmsMode = true, float windowSizeMs = 5.0f,
                float attackTimeMs = 1.0f, float decayTimeMs = 100.0f, float peakHoldTimeMs = 500.0f)
  : mThreshold(static_cast<float>(iplug::DBToAmp(minThresholdDb)))
  , mRMSMode(rmsMode)
  , mWindowSizeMs(windowSizeMs)
  , mAttackTimeMs(attackTimeMs)
  , mDecayTimeMs(decayTimeMs)
  , mPeakHoldTimeMs(peakHoldTimeMs)
  {
    Reset(iplug::DEFAULT_SAMPLE_RATE);
  }

  // Like upstream, keeps mCount: a lower rate can leave it past the new window end.
  void Reset(double sampleRate)
  {
    SetWindowSizeMs(mWindowSizeMs, sampleRate);
    SetAttackTimeMs(mAttackTimeMs, sampleRate);
    SetDecayTimeMs(mDecayTimeMs, sampleRate);
    SetPeakHoldTimeMs(mPeakHoldTimeMs, sampleRate);
    std::fill(mHeldPeaks.begin(), mHeldPeaks.end(), 0.0f);
  }

  // Back to the freshly constructed state at the current window size. Never resizes,
  // so the audio thread may call it.
  void Restart()
  {
    for (auto& buffer : mBuffers)
      std::fill(buffer.begin(), buffer.end(), 0.0f);
    std::fill(mHeldPeaks.begin(), mHeldPeaks.end(), 0.0f);
    std::fill(mPeakHoldCounters.begin(), mPeakHoldCounters.end(), mPeakHoldTime);
    for (auto& follower : mEnvFollowers)
      follower = EnvelopeFollower{};
    mPreviousSum = 1.0f;
    mCount = 0;
  }

  void SetAttackTimeMs(double timeMs, double sampleRate)
  {
    mAttackTimeMs = static_cast<float>(timeMs);
    mAttackTimeSamples = static_cast<float>(timeMs * 0.001 * (sampleRate / double(mWindowSize)));
  }

  void SetDecayTimeMs(double timeMs, double sampleRate)
  {
    mDecayTimeMs = static_cast<float>(timeMs);
    mDecayTimeSamples = static_cast<float>(timeMs * 0.001 * (sampleRate / mWindowSize));
  }

  void SetWindowSizeMs(double timeMs, double sampleRate)
  {
    mWindowSizeMs = static_cast<float>(timeMs);
    mWindowSize = static_cast<int>(timeMs * 0.001 * sampleRate);

    for (auto i = 0; i < MAXNC; i++)
    {
      mBuffers[i].resize(mWindowSize);
      std::fill(mBuffers[i].begin(), mBuffers[i].end(), 0.0f);
    }
  }

  void SetPeakHoldTimeMs(double timeMs, double sampleRate)
  {
    mPeakHoldTimeMs = static_cast<float>(timeMs);
    mPeakHoldTime = static_cast<int>(timeMs * 0.001 * sampleRate);
    std::fill(mPeakHoldCounters.begin(), mPeakHoldCounters.end(), mPeakHoldTime);
  }

  void ProcessBlock(iplug::sample** inputs, int nFrames, int ctrlTag = iplug::kNoTag, int nChans = MAXNC,
                    int chanOffset = 0)
  {
    // Upstream indexes the window with the block-relative s % mWindowSize, not mCount.
    int windowPos = 0;
    for (auto s = 0; s < nFrames; s++)
    {
      for (auto c = chanOffset; c < (chanOffset + nChans); c++)
      {
        mBuffers[c][windowPos] = static_cast<float>(inputs[c][s]);
      }

      if (mCount == 0)
      {
        Data d{ctrlTag, nChans, chanOffset};

        auto avgSum = 0.0f;

        for (auto c = chanOffset; c < (chanOffset + nChans); c++)
        {
          auto peakVal = 0.0f;
          auto avgVal = 0.0f;
#if defined OS_IOS || defined OS_MAC
          vDSP_vabs(mBuffers[c].data(), 1, mBuffers[c].data(), 1, mWindowSize);
          vDSP_maxv(mBuffers[c].data(), 1, &peakVal, mWindowSize);

          if (mRMSMode)
          {
            vDSP_rmsqv(mBuffers[c].data(), 1, &avgVal, mWindowSize);
          }
          else
          {
            vDSP_meanv(mBuffers[c].data(), 1, &avgVal, mWindowSize);
          }
#else
          for (auto i = 0; i < mWindowSize; i++)
          {
            auto absVal = std::fabs(mBuffers[c][i]);

            if (absVal > peakVal)
            {
              peakVal = absVal;
            }

            if (mRMSMode)
            {
              absVal = absVal * absVal;
            }

            avgVal += absVal;
          }

          avgVal /= static_cast<float>(mWindowSize);

          if (mRMSMode)
          {
            avgVal = std::sqrt(avgVal);
          }
#endif

          if (mPeakHoldCounters[c] <= 0)
          {
            mHeldPeaks[c] = 0.0f;
          }

          if (mHeldPeaks[c] < peakVal)
          {
            mHeldPeaks[c] = peakVal;
            mPeakHoldCounters[c] = mPeakHoldTime;
          }
          else
          {
            if (mPeakHoldCounters[c] > 0)
            {
              mPeakHoldCounters[c] -= mWindowSize;
            }
          }

          std::get<0>(d.vals[c]) = mHeldPeaks[c];

          auto smoothedAvg = mEnvFollowers[c].Process(avgVal, mAttackTimeSamples, mDecayTimeSamples);
          std::get<1>(d.vals[c]) = smoothedAvg;

          avgSum += smoothedAvg;
        }

        // Upstream's peak-hold fallback below the threshold never pushes: its flag starts
        // false and is only ANDed.
        if (mPreviousSum > mThreshold)
        {
          Base::PushData(d);
        }

        mPreviousSum = avgSum;
      }

      if (++windowPos >= mWindowSize)
        windowPos = 0;

      // >= because Reset to a lower rate can leave mCount past the new window end;
      // the rare second step keeps the upstream (mCount + 1) % mWindowSize result.
      if (++mCount >= mWindowSize)
      {
        mCount -= mWindowSize;
        if (mCount >= mWindowSize)
          mCount %= mWindowSize;
      }
    }
  }

private:
  float mThreshold = 0.01f;
  bool mRMSMode = false;
  float mPreviousSum = 1.0f;
  int mWindowSize = 32;
  int mPeakHoldTime = 1 << 16;
  int mCount = 0;
  float mWindowSizeMs = 5.f;
  float mAttackTimeMs = 1.f;
  float mDecayTimeMs = 100.f;
  float mPeakHoldTimeMs = 100.f;
  float mAttackTimeSamples = 1.0f;
  float mDecayTimeSamples = iplug::DEFAULT_SAMPLE_RATE / 10.0f;
  std::array<float, MAXNC> mHeldPeaks = {0};
  std::array<std::vector<float>, MAXNC> mBuffers;
  std::array<int, MAXNC> mPeakHoldCounters;
  std::array<EnvelopeFollower, MAXNC> mEnvFollowers;
};

enum class MeterGateStep
{
  Skip,
  RestartThenRun,
  Run
};

// Audio thread, once per block. The meters feed only the editor: skip them while it is
// closed, and restart them on the first block after it opens so a peak held before the
// close never shows on the new editor. ranLastBlock is audio-thread state.
inline MeterGateStep StepMeterGate(const bool editorOpen, bool& ranLastBlock)
{
  const bool ranBefore = ranLastBlock;
  ranLastBlock = editorOpen;
  if (!editorOpen)
    return MeterGateStep::Skip;
  return ranBefore ? MeterGateStep::Run : MeterGateStep::RestartThenRun;
}

} // namespace volum
