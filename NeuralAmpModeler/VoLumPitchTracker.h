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
  // kPmaxFreq covers the 24th fret of the high E (1319 Hz). The old 600 Hz ceiling left every note
  // from the 10th fret up with no true lag to find, so the estimate settled on a multiple of the
  // period and splices jumped up to ~25 ms: the Octaver "wobble" on high notes into a high-gain amp.
  static constexpr double kPmaxFreq = 1400.0;
  static constexpr double kPminFreq = 40.0;
  // Candidates are compared by NORMALIZED correlation, and the shortest lag whose peak reaches
  // kPeakTol of the best one wins. Raw autocorrelation favours long lags on a decaying note (the older
  // half of each product is louder), which is how a pluck read 2-24 periods long. The tolerance keeps
  // multiples of the period from out-voting the period itself, and is high enough that a dominant
  // second harmonic (fundamental 0.4x its level) still reads at the fundamental.
  static constexpr double kPeakTol = 0.9;
  // Below this normalized correlation the input is treated as unvoiced and the last estimate kept.
  static constexpr double kVoicedCorr = 0.35;
  // Across an attack or a muted note's release the newest window and the lagged one differ in level
  // by hundreds of times, and normalization then lands on lags that are no note's period (INSTANT,
  // which has no splice search, clicks on them). Past this energy ratio the update keeps the last
  // estimate. 4x was too tight: a 40 ms palm-muted chug decays by ~4x across its own long lag, so a
  // chug riff never got a reading at all.
  static constexpr double kStationaryRatio = 16.0;

  struct PeriodEstimate
  {
    double period = 0.0; // the tracker's next mPeriod
    int bestLag = 0; // picked lag before refinement; 0 when the input was too quiet or unvoiced
  };

  // Allocates - call OFF the audio thread.
  void Configure(double sampleRate)
  {
    mSampleRate = sampleRate;
    const int tmax = static_cast<int>(std::ceil(mSampleRate / kPminFreq));
    mPeriodScratch.assign(static_cast<size_t>(2 * tmax + 4), 0.0);
    mLagCorr.assign(static_cast<size_t>(tmax + 1), 0.0);
    mNormCorr.assign(static_cast<size_t>(tmax + 1), 0.0);
  }

  // Everything besides the ring that decides an estimate.
  bool SameShape(const PitchTracker& o) const
  {
    return mSampleRate == o.mSampleRate && mPeriodScratch.size() == o.mPeriodScratch.size()
           && mLagCorr.size() == o.mLagCorr.size() && mNormCorr.size() == o.mNormCorr.size();
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
        || static_cast<int>(mLagCorr.size()) < tmax || static_cast<int>(mNormCorr.size()) < tmax)
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
    return _Pick(s, L, tmin, tmax, e, previous);
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
    if (static_cast<long long>(writeCount) < span + 2 || static_cast<int>(mPeriodScratch.size()) < span
        || static_cast<int>(mLagCorr.size()) < tmax || static_cast<int>(mNormCorr.size()) < tmax)
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
    for (int lag = tmin; lag < tmax; ++lag)
    {
      double r = 0.0;
      for (int k = 0; k < L; ++k)
      {
        const double t = mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k + lag)];
        r += t;
      }
      mLagCorr[static_cast<size_t>(lag)] = r;
    }
    return _Pick(mPeriodScratch.data(), L, tmin, tmax, e, previous);
  }

private:
  // Normalize mLagCorr over [tmin, tmax) by the energy of both windows, then take the shortest lag
  // that is a local peak within kPeakTol of the best. Shared by both paths so the oracle stays exact.
  PeriodEstimate _Pick(const double* s, int L, int tmin, int tmax, double e, double previous)
  {
    const double* r = mLagCorr.data();
    double* nc = mNormCorr.data();
    double eLag = 0.0;
    for (int k = 0; k < L; ++k)
    {
      const double sq = s[k + tmin] * s[k + tmin];
      eLag += sq;
    }
    double best = 0.0;
    for (int lag = tmin; lag < tmax; ++lag)
    {
      if (lag > tmin)
      {
        // Slide the lagged window one sample older: mul and add kept apart (see LagCorrelations).
        const double sqIn = s[lag + L - 1] * s[lag + L - 1];
        const double sqOut = s[lag - 1] * s[lag - 1];
        eLag += sqIn;
        eLag -= sqOut;
      }
      const double denom = e * std::max(eLag, 1e-18);
      nc[lag] = r[lag] / std::sqrt(denom);
      if (nc[lag] > best)
        best = nc[lag];
    }
    if (best < kVoicedCorr)
      return {previous, 0};
    const double peakFloor = kPeakTol * best;
    int bestLag = 0;
    for (int lag = tmin + 1; lag < tmax - 1; ++lag)
    {
      if (nc[lag] >= peakFloor && nc[lag] >= nc[lag - 1] && nc[lag] >= nc[lag + 1])
      {
        bestLag = lag;
        break;
      }
    }
    if (bestLag <= 0)
      return {previous, 0};
    double eAt = 0.0;
    for (int k = 0; k < L; ++k)
    {
      const double sq = s[k + bestLag] * s[k + bestLag];
      eAt += sq;
    }
    if (eAt > kStationaryRatio * e || e > kStationaryRatio * eAt)
      return {previous, 0};
    double refined = static_cast<double>(bestLag);
    const double nm = nc[bestLag - 1];
    const double np = nc[bestLag + 1];
    const double curve = nm + np - 2.0 * nc[bestLag];
    if (std::abs(curve) > 1e-12)
      refined = bestLag + 0.5 * (nm - np) / curve;
    return {refined > 2.0 ? refined : previous, bestLag};
  }

  double mSampleRate = 0.0;
  std::vector<double> mPeriodScratch;
  std::vector<double> mLagCorr; // autocorrelation per lag, indexed by lag
  std::vector<double> mNormCorr; // mLagCorr normalized by both windows' energy, indexed by lag
};

} // namespace effect
} // namespace dsp
