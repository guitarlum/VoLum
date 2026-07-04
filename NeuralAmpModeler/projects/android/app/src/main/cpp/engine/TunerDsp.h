#pragma once

// Guitar tuner pitch detector.
//
// Runs on the audio thread from the RAW DI (pre-gain) signal. Uses the McLeod
// NSDF (normalized square-difference function) with parabolic peak refinement -
// robust against the octave errors that plague plain autocorrelation, which
// matters for a low guitar E (82 Hz). The input is one-pole low-passed and
// decimated 4x (48k -> 12k) so the analysis window is cheap: NSDF over a
// ~2048-sample decimated window is a few hundred k mults, run ~6x/second.
//
// Threading: push() runs on the audio thread and stores the estimate in an
// atomic; hz() is read from the UI thread. No allocation after prepare().

#include <atomic>
#include <cmath>
#include <vector>

namespace volum::mobile
{

class TunerDsp
{
public:
  void prepare(double sampleRate)
  {
    mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    mDecim = 4;
    mDecRate = mSampleRate / mDecim;
    mWindow = 2048; // ~170 ms at 12 kHz
    mBuf.assign(static_cast<size_t>(mWindow), 0.0f);
    mWrite = 0;
    mDecCount = 0;
    mLpState = 0.0;
    // ~2 kHz anti-alias one-pole before decimation.
    const double c = 2.0 * M_PI * 2000.0 / mSampleRate;
    mLpCoef = c / (c + 1.0);
    mHz.store(0.0f, std::memory_order_relaxed);
    mClarity.store(0.0f, std::memory_order_relaxed);
  }

  void reset()
  {
    std::fill(mBuf.begin(), mBuf.end(), 0.0f);
    mWrite = 0;
    mDecCount = 0;
    mLpState = 0.0;
    mHz.store(0.0f, std::memory_order_relaxed);
    mClarity.store(0.0f, std::memory_order_relaxed);
  }

  // Feed the raw mono DI. Detection fires once the window fills.
  void push(const float* in, int n)
  {
    for (int i = 0; i < n; ++i)
    {
      mLpState += (static_cast<double>(in[i]) - mLpState) * mLpCoef;
      if (++mDecCount >= mDecim)
      {
        mDecCount = 0;
        mBuf[static_cast<size_t>(mWrite++)] = static_cast<float>(mLpState);
        if (mWrite >= mWindow)
        {
          detect();
          mWrite = 0;
        }
      }
    }
  }

  float hz() const { return mHz.load(std::memory_order_relaxed); }
  float clarity() const { return mClarity.load(std::memory_order_relaxed); }

private:
  void detect()
  {
    const int W = mWindow;
    // RMS gate: ignore silence / very quiet input.
    double energy = 0.0;
    for (int i = 0; i < W; ++i)
      energy += static_cast<double>(mBuf[static_cast<size_t>(i)]) * mBuf[static_cast<size_t>(i)];
    if (energy / W < 1e-7)
    {
      mHz.store(0.0f, std::memory_order_relaxed);
      mClarity.store(0.0f, std::memory_order_relaxed);
      return;
    }

    const int minLag = std::max(2, static_cast<int>(mDecRate / 1400.0)); // up to ~1.4 kHz
    const int maxLag = std::min(W - 1, static_cast<int>(mDecRate / 70.0)); // down to ~70 Hz

    // McLeod NSDF: n(tau) = 2*sum(x[i]x[i+tau]) / sum(x[i]^2 + x[i+tau]^2).
    double bestVal = 0.0;
    int bestLag = -1;
    double prev = 0.0, prevPrev = 0.0;
    int firstPeakLag = -1;
    double firstPeakVal = 0.0;
    for (int tau = minLag; tau <= maxLag; ++tau)
    {
      double acf = 0.0, norm = 0.0;
      const int count = W - tau;
      for (int i = 0; i < count; ++i)
      {
        const double a = mBuf[static_cast<size_t>(i)];
        const double b = mBuf[static_cast<size_t>(i + tau)];
        acf += a * b;
        norm += a * a + b * b;
      }
      const double nsdf = norm > 0.0 ? (2.0 * acf / norm) : 0.0;

      // Track the maximum overall (for the clarity threshold).
      if (nsdf > bestVal)
      {
        bestVal = nsdf;
        bestLag = tau;
      }
      // Detect the first local maximum that clears a clarity threshold: this is
      // the true period, avoiding the octave-down error of just taking the max.
      if (tau > minLag + 1)
      {
        if (prev > prevPrev && prev >= nsdf && prev > 0.6 && firstPeakLag < 0)
        {
          firstPeakLag = tau - 1;
          firstPeakVal = prev;
        }
      }
      prevPrev = prev;
      prev = nsdf;
    }

    int lag = firstPeakLag > 0 ? firstPeakLag : bestLag;
    double clarity = firstPeakLag > 0 ? firstPeakVal : bestVal;
    if (lag < minLag || clarity < 0.5)
    {
      mHz.store(0.0f, std::memory_order_relaxed);
      mClarity.store(0.0f, std::memory_order_relaxed);
      return;
    }

    // Parabolic interpolation around the chosen lag for sub-sample precision.
    double refined = lag;
    if (lag > minLag && lag < maxLag)
    {
      const double y0 = nsdfAt(lag - 1, W);
      const double y1 = nsdfAt(lag, W);
      const double y2 = nsdfAt(lag + 1, W);
      const double denom = (y0 - 2.0 * y1 + y2);
      if (std::abs(denom) > 1e-12)
        refined = lag + 0.5 * (y0 - y2) / denom;
    }

    const double freq = mDecRate / refined;
    mHz.store(static_cast<float>(freq), std::memory_order_relaxed);
    mClarity.store(static_cast<float>(clarity), std::memory_order_relaxed);
  }

  double nsdfAt(int tau, int W) const
  {
    double acf = 0.0, norm = 0.0;
    const int count = W - tau;
    for (int i = 0; i < count; ++i)
    {
      const double a = mBuf[static_cast<size_t>(i)];
      const double b = mBuf[static_cast<size_t>(i + tau)];
      acf += a * b;
      norm += a * a + b * b;
    }
    return norm > 0.0 ? (2.0 * acf / norm) : 0.0;
  }

  double mSampleRate = 48000.0;
  int mDecim = 4;
  double mDecRate = 12000.0;
  int mWindow = 2048;
  std::vector<float> mBuf;
  int mWrite = 0;
  int mDecCount = 0;
  double mLpState = 0.0;
  double mLpCoef = 0.2;
  std::atomic<float> mHz{0.0f};
  std::atomic<float> mClarity{0.0f};
};

} // namespace volum::mobile
