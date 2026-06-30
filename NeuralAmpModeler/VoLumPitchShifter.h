#pragma once

// VoLum Pitch pedal engine (PRE, before compressor).
//
// Time-domain pitch shifter: a SINGLE variable-speed read pointer over a ring
// buffer, with PERIOD-SYNCHRONOUS splices (the read pointer jumps by whole signal
// periods, crossfaded) so a sustained note does not amplitude-modulate ("warble")
// and, crucially, does NOT drift in pitch ("detunes while it rings"). This is the
// katjaas/Whammy-style low-latency delay-line shifter, refined with a WSOLA
// (cross-correlation) splice search in DROP mode.
//
// Why not a phase vocoder: for low guitar strings at low latency an STFT vocoder
// either drifts or needs ~64-85 ms. Why not the old fixed-grain dual-tap: its
// grain was not period-aligned, so it warbled and ran sharp on upshifts. Measured
// against a commercial reference (NDSP Archetype Rabea X transpose), this engine
// matches it on pitch accuracy (~0.5 cents), drift (~3 cents) and warble, at
// roughly half the old latency. See _spike/refs/RESEARCH-NOTES.md (local).
//
// Three CHARACTERs trade latency vs accuracy / monophonic vs polyphonic:
//   DROP:    WSOLA splice search, PERIOD-synchronous -> exact mono pitch even at
//            +/-12, ~17 ms. Monophonic (one period estimate).
//   INSTANT: period-sync, no search -> tightest ~8.6 ms. Monophonic.
//   POLY:    FIXED-grain WSOLA splice (no pitch estimate) -> tracks CHORDS, not
//            just single notes, at ~14 ms. Clean-room reimplementation of the NDSP
//            Archetype Rabea X "transpose": DYNAMIC RE (Frida differential coverage
//            + Stalker on the PACE-encrypted binary) proved it is a time-domain
//            single-pointer granular shifter (no FFT/IPP, no phase vocoder) whose
//            TRUE impulse latency is ~2-8 ms (shift-dependent), constant across host
//            block sizes -> a genuine low-latency algorithm. The key insight ported
//            here: the read pointer is clamped to >= xfade by the splice, so the
//            WSOLA search/correlation windows are HISTORY reads that need not inflate
//            the delay floor (latency = xfade + 0.5*band, not +search+corr). That
//            collapsed POLY from ~49 ms to ~14 ms while keeping exact pitch and
//            polyphony. See _spike/refs/RESEARCH-NOTES.md Phase 6 (local, gitignored).
//
// Latency = read-pointer headroom (group delay). The dry path is delayed by the
// same amount so dry/wet stay aligned; the host compensates the total via PDC.
// Latency depends on CHARACTER, so the plugin re-reports it when character/mode
// changes (per-mode latency reporting).
//
// Two modes share the engine:
//   Transpose: one voice at 2^(semitones/12), with dry/wet MIX and LEVEL.
//   Octaver:   dry + (-1 octave voice * octDown) + (+1 octave voice * octUp),
//              with a Vintage (gritty/filtered) vs Modern (clean) voicing.
//              Octaver voices use DROP character for stability.
//
// Mono in/out (numChannels == 1; VoLum is mono internally).

#include "../AudioDSPTools/dsp/dsp.h"

#include <algorithm>
#include <array>
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
  // Period-search frequency bounds for the autocorrelation pitch estimate.
  static constexpr double kPmaxFreq = 600.0;
  static constexpr double kPminFreq = 70.0;

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
    const int tmax = static_cast<int>(std::ceil(mSampleRate / kPminFreq));
    // Max delay any read can touch: candidate jump up to dHi, plus the WSOLA
    // search and correlation window read further back, plus the period estimate
    // needs 2*tmax of history.
    const int maxReadDelay = static_cast<int>(std::ceil(worst.dHi)) + worst.search + worst.corrWin + 2;
    const int historyNeed = std::max(maxReadDelay, 2 * tmax + 2);
    const size_t need = static_cast<size_t>(historyNeed + mMaxBlock + 32);
    if (mBuf.size() < need)
      mBuf.assign(need, 0.0);

    mPeriodScratch.assign(static_cast<size_t>(2 * tmax + 4), 0.0);
    mRefWin.assign(static_cast<size_t>(std::max(worst.corrWin, 1)), 0.0);

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
  }

  // ratio = output_freq / input_freq (2^(semitones/12)).
  void SetRatio(double ratio) { mRatio = std::clamp(ratio, 0.25, 4.0); }

  void Process(const DSP_SAMPLE* in, DSP_SAMPLE* out, size_t numFrames)
  {
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

      if (--mPeriodCountdown <= 0)
      {
        mPeriodCountdown = mPeriodUpdate;
        _UpdatePeriod();
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
          if (mWsola)
            cand = _WsolaRefine(cand);
          if (cand >= static_cast<double>(mXfade) + 1.0)
          {
            mDelayNew = cand;
            mFading = true;
            mFadePos = 0;
          }
        }
        else if (f > 1.0 && mDelay < mDLo)
        {
          const int k = std::max(1, static_cast<int>(std::lround((mDHi - mDelay) / P)));
          double cand = mDelay + k * P; // larger delay (jump back / duplicate)
          if (mWsola)
            cand = _WsolaRefine(cand);
          mDelayNew = cand;
          mFading = true;
          mFadePos = 0;
        }
      }

      mWrite = (mWrite + 1) % sz;
    }
  }

private:
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
      // splice. Dynamic RE of the NDSP Archetype Rabea X transpose (RESEARCH-NOTES
      // Phase 6) measured its TRUE impulse latency at ~2-8 ms (shift-dependent, NOT
      // the 71-sample reported PDC) using a short delay band; constant across host
      // block sizes => a genuine low-latency algorithm, not in-block lookahead.
      // Replicating that here: the latency floor is xfade + 0.5*band, NOT
      // xfade+search+corr+0.5*band - the splice already clamps the read pointer to
      // >= xfade, so search/corr are history reads that do NOT raise latency (see the
      // fixed-grain dLo below). That lets us use a GENEROUS search/corr (robust splice
      // alignment, incl. low-E octave-down) at zero latency cost and a much shorter
      // band. Band 20 ms is the floor at which WSOLA splices stay rare enough to keep
      // every chord voice (shorter bands splice so often that WSOLA's per-splice
      // dominant-period locking cancels inner voices). Net: ~49 ms -> ~14 ms, still
      // exact pitch (<=3 cents) and polyphonic. (Verified in _spike volum_poly_sim.py.)
      t.wsola = true;
      t.fixedGrain = true;
      t.xfade = std::max(8, static_cast<int>(std::lround(sr * 0.004)));
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

  double _ReadAtDelay(double delay) const
  {
    const double sz = static_cast<double>(mBuf.size());
    double rp = static_cast<double>(mWrite) - delay;
    while (rp < 0.0)
      rp += sz;
    while (rp >= sz)
      rp -= sz;
    const double fl = std::floor(rp);
    const size_t i0 = static_cast<size_t>(fl) % mBuf.size();
    const size_t i1 = (i0 + 1) % mBuf.size();
    const double frac = rp - fl;
    return mBuf[i0] * (1.0 - frac) + mBuf[i1] * frac;
  }

  // WSOLA: pick the splice offset in [-search, search] around `cand` whose window
  // best matches (normalized cross-correlation) the window we are leaving, so the
  // crossfade joins waveform-aligned segments -> minimal warble and zero pitch
  // bias (self-correcting; does not depend on an exact period estimate).
  double _WsolaRefine(double cand)
  {
    const int win = mCorrWin;
    if (win <= 0 || static_cast<long long>(mWriteCount) < static_cast<long long>(mDHi) + win + mSearch + 4)
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
    for (int lag = -mSearch; lag <= mSearch; ++lag)
    {
      const double dc = cand + lag;
      if (dc < static_cast<double>(mXfade) + 1.0)
        continue;
      double dot = 0.0;
      double sn = 0.0;
      for (int j = 0; j < win; ++j)
      {
        const double v = _ReadAtDelay(dc + j);
        dot += mRefWin[static_cast<size_t>(j)] * v;
        sn += v * v;
      }
      const double cc = dot / (rn * (std::sqrt(sn) + 1e-9));
      if (cc > bestC)
      {
        bestC = cc;
        bestLag = lag;
      }
    }
    return cand + bestLag;
  }

  // Autocorrelation period estimate over recent history. Keeps the last estimate
  // on unvoiced/weak input. Runs ~every 10 ms, not per sample.
  void _UpdatePeriod()
  {
    const int tmin = std::max(2, static_cast<int>(mSampleRate / kPmaxFreq));
    const int tmax = static_cast<int>(mSampleRate / kPminFreq);
    const int L = tmax;
    const int span = tmax + L;
    if (static_cast<long long>(mWriteCount) < span + 2 || static_cast<int>(mPeriodScratch.size()) < span)
      return;
    for (int k = 0; k < span; ++k)
      mPeriodScratch[static_cast<size_t>(k)] = _ReadAtDelay(static_cast<double>(k));
    double e = 0.0;
    for (int k = 0; k < L; ++k)
      e += mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k)];
    if (e < 1e-7)
      return;
    double best = 0.0;
    int bestLag = 0;
    for (int lag = tmin; lag < tmax; ++lag)
    {
      double r = 0.0;
      for (int k = 0; k < L; ++k)
        r += mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k + lag)];
      if (r > best)
      {
        best = r;
        bestLag = lag;
      }
    }
    if (bestLag <= 0 || best < 0.35 * e)
      return;
    double refined = static_cast<double>(bestLag);
    if (bestLag > tmin && bestLag < tmax - 1)
    {
      double rm = 0.0, rp = 0.0;
      for (int k = 0; k < L; ++k)
      {
        rm += mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k + bestLag - 1)];
        rp += mPeriodScratch[static_cast<size_t>(k)] * mPeriodScratch[static_cast<size_t>(k + bestLag + 1)];
      }
      const double denom = (rm + rp - 2.0 * best);
      if (std::abs(denom) > 1e-9)
        refined = bestLag + 0.5 * (rm - rp) / denom;
    }
    if (refined > 2.0)
      mPeriod = refined;
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
  std::vector<double> mPeriodScratch;
  std::vector<double> mRefWin;
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
};

class VoLumPitch
{
public:
  enum class Mode
  {
    Transpose = 0,
    Octaver = 1
  };
  enum class Voicing
  {
    Vintage = 0,
    Modern = 1
  };
  using Character = GranularVoice::Character;

  static constexpr int kNumVoices = 2; // octaver uses both (down/up); transpose uses voice 0.

  // Allocates - call OFF the audio thread (OnReset), never from ProcessBlock.
  void Configure(double sampleRate, int maxBlockSize)
  {
    mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    mMaxBlock = std::max(maxBlockSize, 64);
    const bool sameConfig = mConfigured && mSampleRate == mConfiguredSampleRate && mMaxBlock <= mConfiguredMaxBlock;
    if (!sameConfig)
    {
      for (auto& voice : mVoices)
        voice.Configure(mSampleRate, mMaxBlock);
      mConfiguredSampleRate = mSampleRate;
      mConfiguredMaxBlock = mMaxBlock;
      mConfigured = true;
    }
    _ApplyCharacters();
    const double fc = 3200.0;
    mVintageLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fc / mSampleRate);
    _AllocateScratch();
  }

  int Latency() const { return mLatency; }
  bool Configured() const { return mConfigured; }

  // Reported latency for a given mode/character at a sample rate, computed without
  // touching the live (audio-thread-updated) state. Mirrors _ApplyCharacters:
  // Octaver always runs DROP-grade voices; Transpose follows the character pill.
  // Use this for host PDC / UI readouts on the main thread so the value reflects the
  // CURRENT params immediately instead of lagging the audio thread by one block.
  static int LatencyFor(Mode mode, Character transChar, double sampleRate)
  {
    const Character c = (mode == Mode::Transpose) ? transChar : Character::Drop;
    return GranularVoice::LatencyFor(c, sampleRate);
  }

  void Reset()
  {
    for (auto& voice : mVoices)
      voice.Reset();
    std::fill(mDryRing.begin(), mDryRing.end(), static_cast<DSP_SAMPLE>(0));
    mDryWrite = 0;
    mVintageLpState = {0.0, 0.0};
  }

  void SetParams(Mode mode, double semitones, double mix01, double octDown01, double octUp01, double dry01,
                 Voicing voicing, double levelDb, Character transChar = Character::Instant)
  {
    mMode = mode;
    mTransChar = transChar;
    mSemitones = std::clamp(semitones, -24.0, 24.0);
    mMix = std::clamp(mix01, 0.0, 1.0);
    mOctDown = std::clamp(octDown01, 0.0, 1.0);
    mOctUp = std::clamp(octUp01, 0.0, 1.0);
    mDry = std::clamp(dry01, 0.0, 1.0);
    mVoicing = voicing;
    const double clampedDb = std::clamp(levelDb, -20.0, 20.0);
    mLevel = std::pow(10.0, clampedDb / 20.0);
    _ApplyCharacters();
  }

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, size_t numChannels, size_t numFrames)
  {
    _PrepareIO(numChannels, numFrames);

    if (!mConfigured || numChannels == 0)
    {
      for (size_t c = 0; c < numChannels; ++c)
        std::copy(inputs[c], inputs[c] + numFrames, mOut[c].begin());
      return _Pointers(numChannels);
    }

    DSP_SAMPLE* in = inputs[0];

    // Delay dry by engine latency so it stays time-aligned with the wet voices.
    const size_t ringLen = mDryRing.size();
    const size_t lat = static_cast<size_t>(std::min(mLatency, static_cast<int>(ringLen) - 1));
    for (size_t i = 0; i < numFrames; ++i)
    {
      mDryRing[mDryWrite] = in[i];
      mDryScratch[i] = mDryRing[(mDryWrite + ringLen - lat) % ringLen];
      mDryWrite = (mDryWrite + 1) % ringLen;
    }

    if (mMode == Mode::Transpose)
    {
      mVoices[0].SetRatio(std::pow(2.0, mSemitones / 12.0));
      mVoices[0].Process(in, mWet0.data(), numFrames);
      for (size_t i = 0; i < numFrames; ++i)
      {
        const double y = mDryScratch[i] * (1.0 - mMix) + static_cast<double>(mWet0[i]) * mMix;
        mOut[0][i] = static_cast<DSP_SAMPLE>(y * mLevel);
      }
    }
    else // Octaver
    {
      mVoices[0].SetRatio(0.5);
      mVoices[1].SetRatio(2.0);
      mVoices[0].Process(in, mWet0.data(), numFrames);
      mVoices[1].Process(in, mWet1.data(), numFrames);
      for (size_t i = 0; i < numFrames; ++i)
      {
        double down = static_cast<double>(mWet0[i]);
        double up = static_cast<double>(mWet1[i]);
        if (mVoicing == Voicing::Vintage)
        {
          down = _VintageShape(down, 0);
          up = _VintageShape(up, 1);
        }
        const double wet = down * mOctDown + up * mOctUp;
        const double y = mDryScratch[i] * mDry + wet;
        mOut[0][i] = static_cast<DSP_SAMPLE>(y * mLevel);
      }
    }

    for (size_t c = 0; c < numChannels; ++c)
      for (size_t i = 0; i < numFrames; ++i)
        if (!std::isfinite(static_cast<double>(mOut[c][i])))
          mOut[c][i] = static_cast<DSP_SAMPLE>(0);

    return _Pointers(numChannels);
  }

private:
  // Push the current mode/character into the voices and recompute reported
  // latency. Cheap (no allocation); safe from the audio thread.
  void _ApplyCharacters()
  {
    if (mMode == Mode::Transpose)
    {
      mVoices[0].SetCharacter(mTransChar);
      mLatency = mVoices[0].Latency();
    }
    else
    {
      mVoices[0].SetCharacter(Character::Drop);
      mVoices[1].SetCharacter(Character::Drop);
      mLatency = mVoices[0].Latency();
    }
  }

  double _VintageShape(double x, int idx)
  {
    const double driven = std::tanh(x * 1.8);
    mVintageLpState[idx] = mVintageLpCoeff * driven + (1.0 - mVintageLpCoeff) * mVintageLpState[idx];
    return mVintageLpState[idx];
  }

  void _AllocateScratch()
  {
    const size_t cap = static_cast<size_t>(mMaxBlock);
    if (mWet0.size() < cap)
    {
      mWet0.assign(cap, static_cast<DSP_SAMPLE>(0));
      mWet1.assign(cap, static_cast<DSP_SAMPLE>(0));
      mDryScratch.assign(cap, static_cast<DSP_SAMPLE>(0));
    }
    // Dry ring must cover the worst-case latency + a block. POLY (chord-capable)
    // is the highest-latency character, so size for it.
    const int worstLatency = std::max(
      GranularVoice::LatencyFor(Character::Drop, mSampleRate), GranularVoice::LatencyFor(Character::Poly, mSampleRate));
    const size_t ringNeed = static_cast<size_t>(worstLatency) + cap + 2;
    if (mDryRing.size() < ringNeed)
    {
      mDryRing.assign(ringNeed, static_cast<DSP_SAMPLE>(0));
      mDryWrite = 0;
    }
    for (size_t c = 0; c < kMaxChannels; ++c)
      if (mOut[c].size() < cap)
        mOut[c].assign(cap, static_cast<DSP_SAMPLE>(0));
  }

  void _PrepareIO(size_t numChannels, size_t numFrames)
  {
    if (static_cast<int>(numFrames) > mMaxBlock || mWet0.size() < numFrames)
    {
      mMaxBlock = std::max(mMaxBlock, static_cast<int>(numFrames));
      for (auto& voice : mVoices)
        voice.Configure(mSampleRate, mMaxBlock);
      _ApplyCharacters();
      _AllocateScratch();
    }
    (void)numChannels;
  }

  DSP_SAMPLE** _Pointers(size_t numChannels)
  {
    for (size_t c = 0; c < numChannels && c < kMaxChannels; ++c)
      mOutPtrs[c] = mOut[c].data();
    return mOutPtrs.data();
  }

  static constexpr size_t kMaxChannels = 2;

  std::array<GranularVoice, kNumVoices> mVoices;

  double mSampleRate = 0.0;
  double mConfiguredSampleRate = 0.0;
  int mMaxBlock = 0;
  int mConfiguredMaxBlock = 0;
  int mLatency = 0;
  bool mConfigured = false;

  Mode mMode = Mode::Transpose;
  Voicing mVoicing = Voicing::Modern;
  Character mTransChar = Character::Drop;
  double mSemitones = 0.0;
  double mMix = 1.0;
  double mOctDown = 0.0;
  double mOctUp = 0.0;
  double mDry = 1.0;
  double mLevel = 1.0;

  double mVintageLpCoeff = 0.3;
  std::array<double, kNumVoices> mVintageLpState{0.0, 0.0};

  std::vector<DSP_SAMPLE> mWet0, mWet1, mDryScratch, mDryRing;
  size_t mDryWrite = 0;
  std::array<std::vector<DSP_SAMPLE>, kMaxChannels> mOut;
  std::array<DSP_SAMPLE*, kMaxChannels> mOutPtrs{nullptr, nullptr};
};

} // namespace effect
} // namespace dsp
