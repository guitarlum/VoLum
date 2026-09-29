#pragma once

// Autocorrelation pitch-period tracker behind the period-synchronous voices (DROP, INSTANT and the
// Octaver) in VoLumPitchVoice.h. It reads the voice's ring and keeps its own scratch.

#include "VoLumPitchKernels.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace dsp
{
namespace effect
{

class PitchTracker
{
public:
  // Period-search frequency bounds for the autocorrelation pitch estimate.
  // kPminFreq is the crux of low-string stability: the old 70 Hz floor put the
  // period of every drop-tuned / extended-range low string (drop C C2 65 Hz,
  // 7-string B1 62 Hz, 8-string F#1 46 Hz) OUT of the search range, so the
  // estimate locked to a wrong lag -> mid-cycle splices -> the audible "warps and
  // moves" detune on those strings. Lowering the floor to 40 Hz brings the whole
  // guitar range in and fixes it at ZERO latency cost (the tracker only widens its
  // history read; latency = xfade+..., independent of the search range). Measured:
  // drop C / 7-string / 8-string all go from tens/hundreds of cents of detune to
  // <=6 cents.
  static constexpr double kPmaxFreq = 600.0;
  static constexpr double kPminFreq = 40.0;

  struct PeriodEstimate
  {
    double period = 0.0; // the tracker's next mPeriod
    int bestLag = 0; // autocorrelation peak before refinement; 0 when the input was too quiet to search
  };

  // Allocates - call OFF the audio thread.
  void Configure(double sampleRate)
  {
    mSampleRate = sampleRate;
    const int tmax = static_cast<int>(std::ceil(mSampleRate / kPminFreq));
    mPeriodScratch.assign(static_cast<size_t>(2 * tmax + 4), 0.0);
    mLagCorr.assign(static_cast<size_t>(tmax + 1), 0.0);
  }

  // Everything besides the ring that decides an estimate.
  bool SameShape(const PitchTracker& o) const
  {
    return mSampleRate == o.mSampleRate && mPeriodScratch.size() == o.mPeriodScratch.size()
           && mLagCorr.size() == o.mLagCorr.size();
  }

  // Every lag's autocorrelation lands in mLagCorr from the lag-blocked kernel, so the refinement
  // reads its neighbours instead of summing them again.
  PeriodEstimate Estimate(const std::vector<double>& buf, size_t write, unsigned long long writeCount, double previous)
  {
    const int tmin = std::max(2, static_cast<int>(mSampleRate / kPmaxFreq));
    const int tmax = static_cast<int>(mSampleRate / kPminFreq);
    const int L = tmax;
    const int span = tmax + L;
    if (static_cast<long long>(writeCount) < span + 2 || static_cast<int>(mPeriodScratch.size()) < span
        || static_cast<int>(mLagCorr.size()) < tmax)
      return {previous, 0};
    // Integer delays: ReadRingAtDelay(k) is b[i0] * 1 + b[i1] * 0, spelled out so -0 and non-finite
    // neighbours come out the same.
    const size_t n = buf.size();
    size_t i0 = write;
    for (int k = 0; k < span; ++k)
    {
      const size_t i1 = (i0 + 1 >= n) ? 0 : i0 + 1;
      mPeriodScratch[static_cast<size_t>(k)] = buf[i0] * 1.0 + buf[i1] * 0.0;
      i0 = (i0 == 0) ? n - 1 : i0 - 1;
    }
    const double* s = mPeriodScratch.data();
    double e = 0.0;
    for (int k = 0; k < L; ++k)
    {
      const double sq = s[k] * s[k];
      e += sq;
    }
    if (e < 1e-7)
      return {previous, 0};
    double* r = mLagCorr.data();
    pitch_kernels::LagCorrelations(s, L, tmin, tmax, r);
    double best = 0.0;
    int bestLag = 0;
    for (int lag = tmin; lag < tmax; ++lag)
    {
      if (r[lag] > best)
      {
        best = r[lag];
        bestLag = lag;
      }
    }
    if (bestLag <= 0 || best < 0.35 * e)
      return {previous, bestLag};
    double refined = static_cast<double>(bestLag);
    if (bestLag > tmin && bestLag < tmax - 1)
    {
      const double rm = r[bestLag - 1];
      const double rp = r[bestLag + 1];
      const double denom = (rm + rp - 2.0 * best);
      if (std::abs(denom) > 1e-9)
        refined = bestLag + 0.5 * (rm - rp) / denom;
    }
    return {refined > 2.0 ? refined : previous, bestLag};
  }

  // The pre-blocking tracker: the oracle for Estimate and the reference for the burst test. Mul and
  // add are separate so Apple clang cannot contract them into FMAs that diverge from the blocked path
  // (see pitch_kernels::LagCorrelations).
  PeriodEstimate EstimateReference(const std::vector<double>& buf, size_t write, unsigned long long writeCount,
                                   double previous)
  {
    const int tmin = std::max(2, static_cast<int>(mSampleRate / kPmaxFreq));
    const int tmax = static_cast<int>(mSampleRate / kPminFreq);
    const int L = tmax;
    const int span = tmax + L;
    if (static_cast<long long>(writeCount) < span + 2 || static_cast<int>(mPeriodScratch.size()) < span)
      return {previous, 0};
    for (int k = 0; k < span; ++k)
      mPeriodScratch[static_cast<size_t>(k)] = pitch_kernels::ReadRingAtDelay(buf, write, static_cast<double>(k));
    double e = 0.0;
    for (int k = 0; k < L; ++k)
    {
      const double v = mPeriodScratch[static_cast<size_t>(k)];
      const double sq = v * v;
      e += sq;
    }
    if (e < 1e-7)
      return {previous, 0};
    double best = 0.0;
    int bestLag = 0;
    for (int lag = tmin; lag < tmax; ++lag)
    {
      double r = 0.0;
      for (int k = 0; k < L; ++k)
      {
        const double t = mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k + lag)];
        r += t;
      }
      if (r > best)
      {
        best = r;
        bestLag = lag;
      }
    }
    if (bestLag <= 0 || best < 0.35 * e)
      return {previous, bestLag};
    double refined = static_cast<double>(bestLag);
    if (bestLag > tmin && bestLag < tmax - 1)
    {
      double rm = 0.0, rp = 0.0;
      for (int k = 0; k < L; ++k)
      {
        const double tm = mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k + bestLag - 1)];
        const double tp = mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k + bestLag + 1)];
        rm += tm;
        rp += tp;
      }
      const double denom = (rm + rp - 2.0 * best);
      if (std::abs(denom) > 1e-9)
        refined = bestLag + 0.5 * (rm - rp) / denom;
    }
    return {refined > 2.0 ? refined : previous, bestLag};
  }

private:
  double mSampleRate = 0.0;
  std::vector<double> mPeriodScratch;
  std::vector<double> mLagCorr; // autocorrelation per lag, indexed by lag
};

} // namespace effect
} // namespace dsp
