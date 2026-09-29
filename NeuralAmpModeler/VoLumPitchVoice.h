#pragma once

// One voice of the VoLum Pitch pedal: a single variable-speed read pointer over a ring buffer with
// period-synchronous, WSOLA-aligned splices. The engine overview is at the top of VoLumPitchShifter.h.

#include "../AudioDSPTools/dsp/dsp.h"
#include "VoLumPitchKernels.h"
#include "VoLumPitchTracker.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dsp
{
namespace effect
{

// Single variable-speed read pointer over a ring buffer. Pitch is set purely by
// the read speed (exact, no drift); period-synchronous crossfaded jumps keep the
// read pointer within a small delay band (low latency) without warble.
class GranularVoice
{
public:
  enum class Character
  {
    Drop = 0, // WSOLA splice search, period-sync: exact mono pitch on big shifts, ~17 ms.
    Instant = 1, // period-sync, 2.5 ms crossfade: ~8.6 ms, tightest feel, grainier splices.
    // (The former FAST character was period-sync with a 6 ms crossfade at ~12 ms; it
    //  was sonically identical to INSTANT with more latency, so it was removed.)
    Poly = 2 // FIXED-grain WSOLA (no period estimate): polyphonic/chord-capable, ~14 ms.
  };

  // Lowest note we design the delay band around (low E standard, 82.41 Hz). Notes
  // below this (drop tunings) still track; they just get marginally more warble.
  static constexpr double kDesignFmin = 82.41;
  // How much normalized cross-correlation a splice candidate may give up to be
  // chosen for being nearer. Correlation peaks recur every signal period and are
  // near-equal in height, so a small tolerance picks the nearest peak instead of an
  // arbitrarily distant one; too large a value would accept genuinely misaligned
  // joins, which is the failure this whole path exists to avoid.
  static constexpr double kAlignTol = 0.02;

  // Allocates - call OFF the audio thread (Configure). Ring is sized for the
  // worst case (DROP) so changing character later never reallocates.
  void Configure(double sampleRate, int maxBlockSize)
  {
    mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    mMaxBlock = std::max(maxBlockSize, 64);

    // Size for the worst case across ALL characters so a live character switch
    // never reallocates. POLY has the widest band + correlation window.
    const Timing drop = _ComputeTiming(Character::Drop);
    const Timing poly = _ComputeTiming(Character::Poly);
    const Timing worst = (poly.dHi + poly.search + poly.corrWin > drop.dHi + drop.search + drop.corrWin) ? poly : drop;
    const int tmax = static_cast<int>(std::ceil(mSampleRate / PitchTracker::kPminFreq));
    // Max delay any read can touch: candidate jump up to dHi, plus the WSOLA
    // search and correlation window read further back, plus the period estimate
    // needs 2*tmax of history.
    const int maxReadDelay = static_cast<int>(std::ceil(worst.dHi)) + worst.search + worst.corrWin + 2;
    const int historyNeed = std::max(maxReadDelay, 2 * tmax + 2);
    const size_t need = static_cast<size_t>(historyNeed + mMaxBlock + 32);
    if (mBuf.size() < need)
      mBuf.assign(need, 0.0);

    mTracker.Configure(mSampleRate);
    mRefWin.assign(static_cast<size_t>(std::max(worst.corrWin, 1)), 0.0);
    // One correlation slot per candidate lag, for the widest search any character
    // uses. Sized here so _WsolaRefineRange never allocates on the audio thread.
    const int maxSearch = std::max(drop.search, poly.search);
    mCorrScratch.assign(static_cast<size_t>(2 * maxSearch + 1), 0.0);
    // Candidate span for extract-once WSOLA: two-sided search plus the correlation
    // window. Filled once per splice so the lag loop never re-interpolates the ring.
    const int maxCandSpan = 2 * maxSearch + std::max(worst.corrWin, 1);
    mCandWin.assign(static_cast<size_t>(std::max(maxCandSpan, 1)), 0.0);
    mCandSq.assign(mCandWin.size(), 0.0);

    mPeriodUpdate = std::max(1, static_cast<int>(std::lround(mSampleRate * 0.01)));
    SetCharacter(Character::Drop);
    Reset();
  }

  // Cheap (no allocation): recompute the timing band + reported latency. Safe to
  // call from the audio thread; idempotent when the character is unchanged.
  void SetCharacter(Character c)
  {
    if (mHasChar && c == mChar)
      return;
    mChar = c;
    mHasChar = true;
    const Timing t = _ComputeTiming(c);
    mXfade = t.xfade;
    mSearch = t.search;
    mCorrWin = t.corrWin;
    mWsola = t.wsola;
    mDLo = t.dLo;
    mDHi = t.dHi;
    mBand = t.band;
    mFixedGrain = t.fixedGrain;
    mLatency = t.latency;
    // The delay band just moved. Re-centre the read pointer on the new latency so a
    // LIVE character switch keeps wet aligned with the (latency-delayed) dry. This
    // matters most at ratio 1.0, where the delay never drifts back into the new band
    // on its own. Abort any in-flight crossfade. (Harmless during Configure: Reset()
    // re-seeds these immediately afterwards.)
    mDelay = static_cast<double>(mLatency);
    mDelayNew = mDelay;
    mFading = false;
    mFadePos = 0;
  }

  int Latency() const { return mLatency; }

  // Reported latency for a character at a sample rate, without allocating. Used to
  // size the dry-delay ring for the worst case (DROP).
  static int LatencyFor(Character c, double sampleRate)
  {
    return _ComputeTimingFor(c, sampleRate > 0.0 ? sampleRate : 48000.0).latency;
  }

  // Introspection for tests: the effective WSOLA splice geometry (post-clamp) for a
  // character. Exposed so a unit test can pin the fixed-grain (POLY) invariant
  // search < band, whose violation causes runaway re-splicing (stutter/crackle).
  struct SpliceGeometry
  {
    int search = 0;
    int band = 0;
    int xfade = 0;
    bool fixedGrain = false;
  };
  static SpliceGeometry SpliceGeometryFor(Character c, double sampleRate)
  {
    const Timing t = _ComputeTimingFor(c, sampleRate > 0.0 ? sampleRate : 48000.0);
    return {t.search, static_cast<int>(t.band), t.xfade, t.fixedGrain};
  }

  void Reset()
  {
    std::fill(mBuf.begin(), mBuf.end(), 0.0);
    mWrite = 0;
    mWriteCount = 0;
    mPeriod = mSampleRate / 110.0; // default A2 until estimated
    mPeriodCountdown = mPeriodUpdate;
    mDelay = static_cast<double>(mLatency);
    mDelayNew = mDelay;
    mFading = false;
    mFadePos = 0;
    mSpliceStarts = 0;
    mSpliceCorrSum = 0.0;
    mSpliceCorrCount = 0;
  }

  // ratio = output_freq / input_freq (2^(semitones/12)).
  void SetRatio(double ratio) { mRatio = std::clamp(ratio, 0.25, 4.0); }

  // Period estimates produced inside one Process call, in update order. Two voices fed the same input
  // from the same Reset reach every update at the same sample with the same ring, so the second can
  // replay the first one's estimates instead of recomputing them.
  struct PeriodLog
  {
    double* values = nullptr;
    size_t capacity = 0;
    size_t count = 0;
  };

  void Process(const DSP_SAMPLE* in, DSP_SAMPLE* out, size_t numFrames)
  {
    _Process(in, out, numFrames, nullptr, nullptr);
  }
  void ProcessRecordingPeriods(const DSP_SAMPLE* in, DSP_SAMPLE* out, size_t numFrames, PeriodLog& log)
  {
    log.count = 0;
    _Process(in, out, numFrames, &log, nullptr);
  }
  void ProcessReplayingPeriods(const DSP_SAMPLE* in, DSP_SAMPLE* out, size_t numFrames, const PeriodLog& log)
  {
    _Process(in, out, numFrames, nullptr, &log);
  }

  // Everything the period tracker reads besides the ring contents, which the caller must vouch for.
  bool TrackerMatches(const GranularVoice& o) const
  {
    return !mFixedGrain && !o.mFixedGrain && mSampleRate == o.mSampleRate && mBuf.size() == o.mBuf.size()
           && mTracker.SameShape(o.mTracker) && mWrite == o.mWrite && mWriteCount == o.mWriteCount
           && mPeriodUpdate == o.mPeriodUpdate && mPeriodCountdown == o.mPeriodCountdown && mPeriod == o.mPeriod;
  }

private:
  void _Process(const DSP_SAMPLE* in, DSP_SAMPLE* out, size_t numFrames, PeriodLog* record, const PeriodLog* replay)
  {
    size_t replayed = 0;
    if (mBuf.empty())
    {
      std::copy(in, in + numFrames, out);
      return;
    }
    const size_t sz = mBuf.size();
    const double f = mRatio;
    const double grow = 1.0 - f; // delay change per sample
    for (size_t i = 0; i < numFrames; ++i)
    {
      mBuf[mWrite] = static_cast<double>(in[i]);
      ++mWriteCount;

      // POLY splices by the fixed grain, not the period estimate. The tracker is a
      // ~1.3M-op burst every 10 ms that INSTANT already proves is not the crackle;
      // skip it here so POLY does not pay for a value it never reads.
      if (!mFixedGrain && --mPeriodCountdown <= 0)
      {
        mPeriodCountdown = mPeriodUpdate;
        if (replay != nullptr && replayed < replay->count)
          mPeriod = replay->values[replayed++];
        else
        {
          _UpdatePeriod();
          if (record != nullptr && record->count < record->capacity)
            record->values[record->count++] = mPeriod;
        }
      }

      double s = _ReadAtDelay(mDelay);
      if (mFading)
      {
        const double s2 = _ReadAtDelay(mDelayNew);
        const double w = static_cast<double>(mFadePos) / static_cast<double>(mXfade);
        s = s * (1.0 - w) + s2 * w;
      }
      out[i] = static_cast<DSP_SAMPLE>(s);

      mDelay += grow;
      if (mFading)
      {
        mDelayNew += grow;
        if (++mFadePos >= mXfade)
        {
          mDelay = mDelayNew;
          mFading = false;
        }
      }

      if (!mFading)
      {
        // POLY splices by a FIXED grain (mBand) so it never needs a pitch estimate
        // and therefore tracks polyphonic material; DROP/INSTANT splice by the
        // estimated period (monophonic, but tighter and lower latency).
        const double P = mFixedGrain ? mBand : mPeriod;
        if (f < 1.0 && mDelay > mDHi)
        {
          const int k = std::max(1, static_cast<int>(std::lround((mDelay - mDLo) / P)));
          double cand = mDelay - k * P; // smaller delay (skip forward)
          // Downshift keeps the full two-sided search. Here the target sits at the LOW
          // delay boundary and grain length is `dHi - cand`, so it is a POSITIVE lag
          // that shortens the grain while the `dc >= xfade+1` guard blocks most of the
          // negative half. Downshift can therefore only shorten - the opposite of what
          // the 1.2.1 comment claimed - yet it measures clean, because a shortened but
          // waveform-ALIGNED grain is benign; only misaligned joins crackle. Alignment,
          // not cadence, is the thing worth protecting.
          if (mWsola)
            cand = _WsolaRefine(cand);
          if (cand >= static_cast<double>(mXfade) + 1.0)
          {
            mDelayNew = cand;
            mFading = true;
            mFadePos = 0;
            _NoteSplice();
          }
        }
        else if (f > 1.0 && mDelay < mDLo)
        {
          const int k = std::max(1, static_cast<int>(std::lround((mDHi - mDelay) / P)));
          const double base = mDelay + k * P; // larger delay (jump back / duplicate)
          double cand = base;
          // UPSHIFT grain floor, applied to the SEARCH RANGE. The target sits at the
          // HIGH delay boundary, so grain length is `cand - dLo`: a negative lag
          // SHORTENS the grain and makes the next splice fire early. Restricting the
          // search to non-negative lags means every candidate WSOLA may return is
          // already a legal (non-shortening) one, so we get a waveform-aligned join
          // AND a bounded cadence. 1.2.1 instead searched both halves and clamped the
          // answer up to `base` afterwards, which threw the alignment away on nearly
          // every splice and is what actually crackled - see _WsolaRefineRange.
          if (mWsola)
            cand = _WsolaRefineRange(cand, 0, mSearch, true);
          mDelayNew = cand;
          mFading = true;
          mFadePos = 0;
          _NoteSplice();
        }
      }

      if (++mWrite >= sz)
        mWrite = 0;
    }
  }

  struct Timing
  {
    int xfade = 0;
    int search = 0;
    int corrWin = 0;
    bool wsola = false;
    double dLo = 0.0;
    double dHi = 0.0;
    double band = 0.0; // delay-band width = splice-jump unit for fixed-grain (POLY)
    bool fixedGrain = false; // POLY: splice by `band` instead of the pitch period
    int latency = 0;
  };

  Timing _ComputeTiming(Character c) const { return _ComputeTimingFor(c, mSampleRate > 0.0 ? mSampleRate : 48000.0); }

  static Timing _ComputeTimingFor(Character c, double sr)
  {
    const double pDesign = sr / kDesignFmin;
    Timing t;
    if (c == Character::Poly)
    {
      // POLY: fixed-grain WSOLA, no pitch estimate -> works on chords. Pitch is set
      // purely by the read-pointer rate (exact, no drift); the WSOLA search/corr only
      // pick a waveform-aligned splice point and the delay BAND sets how often we
      // splice. A short delay band is what buys the low impulse latency here, and it
      // stays constant across host block sizes rather than relying on in-block
      // lookahead. The latency floor is xfade + 0.5*band, NOT
      // xfade+search+corr+0.5*band - the splice already clamps the read pointer to
      // >= xfade, so search/corr are history reads that do NOT raise latency (see the
      // fixed-grain dLo below). That lets us use a GENEROUS search/corr (robust splice
      // alignment, incl. low-E octave-down) at zero latency cost and a much shorter
      // band. Band 20 ms is the floor at which WSOLA splices stay rare enough to keep
      // every chord voice (shorter bands splice so often that WSOLA's per-splice
      // dominant-period locking cancels inner voices). Net: ~49 ms -> ~14 ms, still
      // exact pitch (<=3 cents) and polyphonic.
      t.wsola = true;
      t.fixedGrain = true;
      t.xfade = std::max(8, static_cast<int>(std::lround(sr * 0.004)));
      // Search + correlation window = 16 ms. CRITICAL INVARIANT: the WSOLA search
      // range MUST stay < band (the fixed-grain splice spacing). _WsolaRefine picks
      // bestLag in [-search, +search]; if search >= band the correlation can move the
      // splice target by MORE than a whole grain - even back above dHi - so the very
      // next sample retriggers a splice, and POLY re-splices every crossfade. On real
      // (transient) playing that is continuous stutter/crackle at any non-zero shift;
      // on steady sines it is inaudible because every jump lands on similar-looking
      // periodic waveform (which is why a 24 ms search passed the sine/triad tests but
      // broke live guitar). The clamp below enforces the invariant defensively.
      t.search = std::max(1, static_cast<int>(std::lround(sr * 0.016)));
      t.corrWin = std::max(8, static_cast<int>(std::lround(sr * 0.016)));
      t.band = std::lround(sr * 0.020);
    }
    else
    {
      // Crossfade length per character: DROP 5 ms (WSOLA aligns the splice anyway),
      // INSTANT 2.5 ms (shortest join -> lowest latency; splices are grainier but
      // period-sync keeps pitch exact). Both splice by the estimated pitch period.
      const double xfadeMs = (c == Character::Drop) ? 5.0 : 2.5;
      t.xfade = std::max(8, static_cast<int>(std::lround(sr * xfadeMs / 1000.0)));
      if (c == Character::Drop)
      {
        t.wsola = true;
        t.search = std::max(1, static_cast<int>(std::lround(sr * 0.0015)));
        t.corrWin = std::max(8, static_cast<int>(std::lround(0.35 * pDesign)));
      }
      else
      {
        t.wsola = false;
        t.search = 0;
        t.corrWin = 0;
      }
      t.band = pDesign;
    }
    // Enforce the fixed-grain invariant (see POLY comment above): the WSOLA search
    // range must stay strictly below the grain spacing, or splices run away and
    // POLY stutters/crackles. Clamp defensively so no future band/search retune can
    // silently re-break it.
    if (t.fixedGrain && t.band > 1.0)
      t.search = std::min(t.search, static_cast<int>(t.band) - 1);
    // Latency floor = xfade + 0.5*band. The splice clamps the read pointer to
    // >= xfade+1, so for fixed-grain (POLY) the search/corr windows only read
    // further into the PAST (history) and must NOT inflate the minimum delay.
    // DROP/INSTANT keep the conservative floor (their splice cadence depends on the
    // live period estimate, so reserve the full search+corr headroom there).
    const double dLoExtra = t.fixedGrain ? 0.0 : static_cast<double>(t.search + t.corrWin);
    t.dLo = static_cast<double>(t.xfade) + dLoExtra + 2.0;
    t.dHi = t.dLo + t.band;
    t.latency = static_cast<int>(std::lround(t.dLo + 0.5 * t.band));
    return t;
  }

  double _ReadAtDelay(double delay) const { return pitch_kernels::ReadRingAtDelay(mBuf, mWrite, delay); }

  // WSOLA: pick the splice offset in [lagMin, lagMax] around `cand` whose window
  // best matches (normalized cross-correlation) the window we are leaving, so the
  // crossfade joins waveform-aligned segments -> minimal warble and zero pitch
  // bias (self-correcting; does not depend on an exact period estimate).
  //
  // The caller passes the lag range, and the range is the ONLY place grain-length
  // constraints may be applied. Clamping the RESULT instead silently discards the
  // alignment WSOLA just found: on a decaying note the best-correlating candidate
  // is reliably the nearest one in history (higher harmonics decay, so closer
  // segments look more alike), so a "floor at the intended jump" clamp fired on
  // essentially every splice and pinned the target to an arbitrary phase. That was
  // the 1.2.1 upshift crackle - measurably, upshift splice gaps became rigidly
  // constant at exactly the nominal grain, the signature of a defeated search.
  //
  // With preferNearest, candidates correlating within kAlignTol of the best are
  // treated as equally aligned (correlation peaks recur every signal period and are
  // near-equal in height) and the one with the SMALLEST |lag| wins. That keeps the
  // read pointer's group delay - and therefore dry/wet comb filtering below 100%
  // MIX - as tight as the alignment allows. It is enabled only on the upshift path,
  // whose range is one-sided and would otherwise drift far into history. The
  // downshift path keeps plain argmax: it measures clean today, and the nearest-lag
  // preference costs it several dB of non-harmonic energy there.
  double _WsolaRefine(double cand) { return _WsolaRefineRange(cand, -mSearch, mSearch, false); }

  // Shared early-outs and the preferNearest pick. Both extract-once and the nested
  // oracle call this after they have filled mCorrScratch / bestC / bestLag.
  double _WsolaPick(double cand, int lagMin, int lagMax, bool preferNearest, double bestC, int bestLag, bool any) const
  {
    if (!any)
      return cand;
    int chosen = bestLag;
    if (preferNearest)
    {
      for (int lag = lagMin; lag <= lagMax; ++lag)
      {
        if (mCorrScratch[static_cast<size_t>(lag - lagMin)] < bestC - kAlignTol)
          continue;
        if (std::abs(lag) < std::abs(chosen))
          chosen = lag;
      }
    }
    return cand + chosen;
  }

  bool _WsolaPrepare(int lagMin, int lagMax, int win) const
  {
    return win > 0 && lagMax >= lagMin
           && static_cast<long long>(mWriteCount) >= static_cast<long long>(mDHi) + win + mSearch + 4
           && static_cast<size_t>(lagMax - lagMin + 1) <= mCorrScratch.size();
  }

  // Extract-once WSOLA: overlapping candidate windows share interpolating ring
  // reads, so the nested `_ReadAtDelay(dc + j)` loop was ~search*corrWin ring
  // walks per splice (POLY: ~590k up, ~1.2M down). Fill the span once with the
  // same reader, then run the identical j-loop sum order from that linear buffer.
  // Same winner, same alignment; the cost is the interpolations, not the FMAs.
  double _WsolaRefineRange(double cand, int lagMin, int lagMax, bool preferNearest)
  {
    const int win = mCorrWin;
    if (!_WsolaPrepare(lagMin, lagMax, win))
      return cand;
    const int span = (lagMax - lagMin) + win;
    if (span <= 0 || static_cast<size_t>(span) > mCandWin.size())
      return cand; // never reachable with Configure()'s sizing; fail safe, no alloc
    double rn = 0.0;
    for (int j = 0; j < win; ++j)
    {
      const double v = _ReadAtDelay(mDelay + j);
      mRefWin[static_cast<size_t>(j)] = v;
      rn += v * v;
    }
    rn = std::sqrt(rn) + 1e-9;
    for (int i = 0; i < span; ++i)
      mCandWin[static_cast<size_t>(i)] = _ReadAtDelay(cand + static_cast<double>(lagMin + i));
    double bestC = -2.0;
    int bestLag = 0;
    bool any = false;
    if (mReferenceKernels)
      _WsolaScanReference(cand, lagMin, lagMax, win, rn, bestC, bestLag, any);
    else
      _WsolaScan(cand, lagMin, lagMax, win, rn, bestC, bestLag, any);
    return _WsolaPick(cand, lagMin, lagMax, preferNearest, bestC, bestLag, any);
  }

  static constexpr int kWsolaBlock = pitch_kernels::kWsolaBlock;

  // Lag-blocked scan: kWsolaBlock lags share each pass over j, every lag keeping its own dot / sn
  // accumulators in ascending j, so each correlation carries the one-lag-at-a-time loop's bits.
  // The argmax still walks lags in ascending order.
  void _WsolaScan(double cand, int lagMin, int lagMax, int win, double rn, double& bestC, int& bestLag, bool& any)
  {
    const double dcMin = static_cast<double>(mXfade) + 1.0;
    int lag = lagMin;
    // cand + lag grows with lag, so the lags too close to the write head are a prefix.
    for (; lag <= lagMax && cand + lag < dcMin; ++lag)
      mCorrScratch[static_cast<size_t>(lag - lagMin)] = -2.0;
#if defined(VOLUM_PITCH_SSE2)
    const int span = (lagMax - lagMin) + win;
    for (int i = lag - lagMin; i < span; ++i)
    {
      const double v = mCandWin[static_cast<size_t>(i)];
      mCandSq[static_cast<size_t>(i)] = v * v;
    }
#endif
    double dot[kWsolaBlock];
    double sn[kWsolaBlock];
    while (lag <= lagMax)
    {
      const int count = std::min(kWsolaBlock, lagMax - lag + 1);
      const size_t offset = static_cast<size_t>(lag - lagMin);
      pitch_kernels::WindowCorrelations(
        mRefWin.data(), mCandWin.data() + offset, mCandSq.data() + offset, win, count, dot, sn);
      for (int b = 0; b < count; ++b, ++lag)
      {
        const size_t slot = static_cast<size_t>(lag - lagMin);
        const double dc = cand + lag;
        if (dc < dcMin)
        {
          mCorrScratch[slot] = -2.0;
          continue;
        }
        const double cc = dot[b] / (rn * (std::sqrt(sn[b]) + 1e-9));
        mCorrScratch[slot] = cc;
        if (cc > bestC)
        {
          bestC = cc;
          bestLag = lag;
          any = true;
        }
      }
    }
  }


  // The pre-blocking scan: the oracle for _WsolaScan and the reference for the burst test.
  // Mul and add are separate statements (like SSE2) so Apple clang cannot contract them into
  // FMAs that diverge from the blocked path on the same machine.
  void _WsolaScanReference(double cand, int lagMin, int lagMax, int win, double rn, double& bestC, int& bestLag,
                           bool& any)
  {
    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
      const size_t slot = static_cast<size_t>(lag - lagMin);
      const double dc = cand + lag;
      if (dc < static_cast<double>(mXfade) + 1.0)
      {
        mCorrScratch[slot] = -2.0;
        continue;
      }
      double dot = 0.0;
      double sn = 0.0;
      const int offset = lag - lagMin;
      for (int j = 0; j < win; ++j)
      {
        const double v = mCandWin[static_cast<size_t>(offset + j)];
        const double rd = mRefWin[static_cast<size_t>(j)] * v;
        const double sq = v * v;
        dot += rd;
        sn += sq;
      }
      const double cc = dot / (rn * (std::sqrt(sn) + 1e-9));
      mCorrScratch[slot] = cc;
      if (cc > bestC)
      {
        bestC = cc;
        bestLag = lag;
        any = true;
      }
    }
  }

  // Nested-read oracle for the extract-once pin. Not used by Process. Same
  // interpolating reader and the same j-loop order as the pre-extract engine.
  double _WsolaRefineRangeNested(double cand, int lagMin, int lagMax, bool preferNearest)
  {
    const int win = mCorrWin;
    if (!_WsolaPrepare(lagMin, lagMax, win))
      return cand;
    double rn = 0.0;
    for (int j = 0; j < win; ++j)
    {
      const double v = _ReadAtDelay(mDelay + j);
      mRefWin[static_cast<size_t>(j)] = v;
      rn += v * v;
    }
    rn = std::sqrt(rn) + 1e-9;
    double bestC = -2.0;
    int bestLag = 0;
    bool any = false;
    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
      const size_t slot = static_cast<size_t>(lag - lagMin);
      const double dc = cand + lag;
      if (dc < static_cast<double>(mXfade) + 1.0)
      {
        mCorrScratch[slot] = -2.0;
        continue;
      }
      double dot = 0.0;
      double sn = 0.0;
      for (int j = 0; j < win; ++j)
      {
        const double v = _ReadAtDelay(dc + j);
        dot += mRefWin[static_cast<size_t>(j)] * v;
        sn += v * v;
      }
      const double cc = dot / (rn * (std::sqrt(sn) + 1e-9));
      mCorrScratch[slot] = cc;
      if (cc > bestC)
      {
        bestC = cc;
        bestLag = lag;
        any = true;
      }
    }
    return _WsolaPick(cand, lagMin, lagMax, preferNearest, bestC, bestLag, any);
  }

  // Normalized cross-correlation between the segment we are leaving (at mDelay) and
  // the one we are joining (at dc). Measured at the delay ACTUALLY spliced to, not
  // the one the search returned, so it stays honest if a caller ever post-processes
  // the search result - which is exactly the mistake that produced the 1.2.1 crackle
  // while a cadence-based regression test stayed green. 1.0 is a perfect join.
  double _SpliceCorr(double dc) const
  {
    const int win = mCorrWin;
    if (win <= 0)
      return 1.0;
    double dot = 0.0, rn = 0.0, sn = 0.0;
    for (int j = 0; j < win; ++j)
    {
      const double a = _ReadAtDelay(mDelay + static_cast<double>(j));
      const double b = _ReadAtDelay(dc + static_cast<double>(j));
      dot += a * b;
      rn += a * a;
      sn += b * b;
    }
    return dot / ((std::sqrt(rn) + 1e-9) * (std::sqrt(sn) + 1e-9));
  }

  using PeriodEstimate = PitchTracker::PeriodEstimate;

  // Autocorrelation period estimate over recent history. Keeps the last estimate
  // on unvoiced/weak input. Runs ~every 10 ms, not per sample.
  void _UpdatePeriod()
  {
    mPeriod = mReferenceKernels ? _EstimatePeriodReference(mPeriod).period : _EstimatePeriod(mPeriod).period;
  }

  PeriodEstimate _EstimatePeriod(double previous) { return mTracker.Estimate(mBuf, mWrite, mWriteCount, previous); }


  // The pre-blocking tracker: the oracle for _EstimatePeriod and the reference for the burst test.
  PeriodEstimate _EstimatePeriodReference(double previous)
  {
    return mTracker.EstimateReference(mBuf, mWrite, mWriteCount, previous);
  }

  double mSampleRate = 0.0;
  int mMaxBlock = 0;

  Character mChar = Character::Drop;
  bool mHasChar = false;
  int mXfade = 0;
  int mSearch = 0;
  int mCorrWin = 0;
  bool mWsola = false;
  double mDLo = 0.0;
  double mDHi = 0.0;
  double mBand = 0.0; // POLY fixed-grain splice-jump unit (= delay band width)
  bool mFixedGrain = false; // POLY: splice by mBand instead of the pitch period
  int mLatency = 0;

  std::vector<double> mBuf;
  PitchTracker mTracker;
  std::vector<double> mRefWin;
  std::vector<double> mCorrScratch;
  std::vector<double> mCandWin;
  std::vector<double> mCandSq; // mCandWin squared, for the SSE2 splice scan
  size_t mWrite = 0;
  unsigned long long mWriteCount = 0;

  double mRatio = 1.0;
  double mPeriod = 0.0;
  int mPeriodUpdate = 1;
  int mPeriodCountdown = 1;

  double mDelay = 0.0;
  double mDelayNew = 0.0;
  bool mFading = false;
  int mFadePos = 0;

  // Test only: run the pre-blocking tracker and splice scan (same output, old cost).
  bool mReferenceKernels = false;

  // Test/introspection only, behaviour-neutral. Splice COUNT alone cannot
  // characterise this engine: 1.2.1 shipped a cadence-capped regression test that
  // passed while the sound was broken, because the damage was in splice ALIGNMENT.
  //
  // Cadence is deliberately NOT exposed here, tempting as it looks. A rigidly
  // constant splice spacing was the fingerprint of the defeated search, but the
  // FIXED engine is near-rigid too - preferring the nearest acceptable lag is what
  // makes it so. An assertion on cadence spread would therefore pass on both, which
  // is the trap that has already cost two releases. Alignment is the real signal.
  unsigned long long mSpliceStarts = 0;
  double mSpliceCorrSum = 0.0;
  unsigned long long mSpliceCorrCount = 0;
  // Off in the product: the correlation costs about 2 x mCorrWin reads per splice and never reaches the audio.
  bool mMeasureSpliceCorr = false;

  // Called at every accepted splice, after mDelayNew is settled.
  void _NoteSplice()
  {
    ++mSpliceStarts;
    if (mWsola && mMeasureSpliceCorr)
    {
      mSpliceCorrSum += _SpliceCorr(mDelayNew);
      ++mSpliceCorrCount;
    }
  }

public:
  unsigned long long SpliceStarts() const { return mSpliceStarts; }
  // Test hooks. DebugWsolaRefineRange is the live extract-once path;
  // DebugWsolaRefineRangeNested is the pre-extract oracle. Both leave the read
  // pointer and fade state untouched.
  double DebugDelay() const { return mDelay; }
  double DebugWsolaRefineRange(double cand, int lagMin, int lagMax, bool preferNearest)
  {
    return _WsolaRefineRange(cand, lagMin, lagMax, preferNearest);
  }
  double DebugWsolaRefineRangeNested(double cand, int lagMin, int lagMax, bool preferNearest)
  {
    return _WsolaRefineRangeNested(cand, lagMin, lagMax, preferNearest);
  }
  // Pre-blocking oracles: the same extract-once search, and the tracker, one lag at a time. The
  // Estimate hooks leave mPeriod alone; DebugCorrScratch holds the last search's correlations.
  double DebugWsolaRefineRangeReference(double cand, int lagMin, int lagMax, bool preferNearest)
  {
    const bool was = mReferenceKernels;
    mReferenceKernels = true;
    const double picked = _WsolaRefineRange(cand, lagMin, lagMax, preferNearest);
    mReferenceKernels = was;
    return picked;
  }
  const std::vector<double>& DebugCorrScratch() const { return mCorrScratch; }
  PeriodEstimate DebugEstimatePeriod() { return _EstimatePeriod(mPeriod); }
  PeriodEstimate DebugEstimatePeriodReference() { return _EstimatePeriodReference(mPeriod); }
  double DebugPeriod() const { return mPeriod; }
  void DebugSetReferenceKernels(bool on) { mReferenceKernels = on; }
  void DebugSetMeasureSpliceCorr(bool on) { mMeasureSpliceCorr = on; }
  unsigned long long SpliceCorrCount() const { return mSpliceCorrCount; }
  // Mean normalized cross-correlation achieved across splices since Reset(), counted only
  // while DebugSetMeasureSpliceCorr(true) is on (otherwise 1.0). 1.0
  // means every join was perfectly waveform-aligned. Only meaningful for WSOLA
  // characters (DROP/POLY); INSTANT does not search, and reports 1.0.
  //
  // The MEAN, not the minimum: a single splice during engine warm-up or a near
  // -silent passage correlates poorly for reasons that have nothing to do with the
  // splice logic, so the worst case is too noisy to assert on (measured swinging
  // negative even on a healthy engine).
  double MeanSpliceCorr() const
  {
    return mSpliceCorrCount ? mSpliceCorrSum / static_cast<double>(mSpliceCorrCount) : 1.0;
  }
};

} // namespace effect
} // namespace dsp
