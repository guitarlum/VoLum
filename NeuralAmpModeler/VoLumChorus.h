#pragma once

// VoLumChorus.h
//
// POST chorus for VoLum (first POST pedal: Chorus -> Delay -> Reverb -> Tremolo).
//
// Header-only DSP in the VoLum source tree (mirrors VoLumTremolo.h /
// VoLumPitchShifter.h placement). Operates in place on the POST bus, which is
// stereo in the standalone and on 1-2 / 2-2 tracks and mono on a 1-1 track.
//
// Four voices on one shared engine:
//
//   - CLASSIC  : Juno-60-style BBD stereo chorus. One voice per side, 1.6 ms
//                minimum delay, triangle LFO with rounded corners, the right
//                LFO offset by WIDTH x 180 deg. DEPTH is the hardware delay
//                sweep (0..6 ms peak-to-peak) anchored at the 1.6 ms minimum,
//                so RATE scales the detune exactly as on the unit: 3.7 ms at
//                0.51 Hz is Juno I (6.6 cents), at 0.86 Hz Juno II (11 cents).
//                This is the one mode where DEPTH is not cents: a cents law
//                shrinks the sweep as RATE rises and loses the Juno's 3:1
//                comb travel.
//   - WARPED   : tape warble. 5 ms base, sine wow + slow random drift + ~9 Hz
//                flutter, gentle tape saturation. WIDTH turns the right wow
//                phase up to 90 deg and gives each side its own drift.
//   - CLEAR    : Dimension-style. Two voices on one sine LFO in anti-phase and
//                a stereo cross-mix (each side is its own voice minus a share
//                of the other). WIDTH is the side level of that matrix. The
//                mono sum is the two voices at 1/sqrt(2); their anti-phase
//                pitch wobble cancels there, so mono stays clean. A slow random
//                wander of the shared LFO rate keeps the motion from ticking.
//   - ENSEMBLE : 80s rack tri-stereo chorus. Three slow voices 120 deg apart on
//                staggered delays (7.0 / 5.5 / 8.5 ms), panned L / C / R around
//                the centred dry (centre voice -6 dB), each with its own slow
//                drift. WIDTH pulls the outer voices toward the centre (0 = all
//                three centred).
//
// Shared engine: DEPTH is cents of peak detune (except CLASSIC), converted to a
// delay excursion from the current RATE so the strength is the same at any
// rate; the excursion is capped at 12 ms, so the deepest settings saturate at
// the slowest rates. Voices are summed with power (1/sqrt(N)) normalization,
// never averaged. TONE is a 2-pole Butterworth low-pass on the wet bus,
// 3..12 kHz. MIX is an equal-power dry/wet blend that glides like the other
// knobs and lands exactly on its end stops: MIX = 0 is then bit-identical
// passthrough and MIX = 1 has no dry residue. Knobs are smoothed per 32-sample
// sub-block and ramped per sample; LFOs run as phasor recurrences (no
// per-sample sin). A 1-1 track hears the left voice (CLEAR: the mono-clean
// sum, ENSEMBLE: all three voices at unequal weights), never a fold-down of
// anti-phase voices. A mode change ducks the wet for ~1.3 ms, resets the voice
// state and fades back in over 10 ms; the lines keep running. Reset() clears
// the lines and is called by the host on the bypass edge.
//
// Built from published literature and measurements only: Dattorro, "Effect
// Design Part 2" (JAES 1997); Smith, "Physical Audio Signal Processing";
// public Juno-60 chorus measurements (A. Hunt); Dimension D / rack tri-chorus
// descriptions (F. Anwander); BBD LFO corner smoothing (R. McClellan).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace volum
{

inline constexpr int kVoLumChorusModeCount = 4;
inline constexpr int kVoLumChorusModeClassic = 0;
inline constexpr int kVoLumChorusModeWarped = 1;
inline constexpr int kVoLumChorusModeClear = 2;
inline constexpr int kVoLumChorusModeEnsemble = 3;
// Ships bypassed on WARPED: the widest, most obviously "chorus" voice, so the
// first thing a user hears after switching the card on is unambiguous.
inline constexpr int kVoLumChorusModeDefault = kVoLumChorusModeWarped;

inline const char* VoLumChorusModeName(int mode)
{
  switch (mode)
  {
    case kVoLumChorusModeClassic: return "Classic";
    case kVoLumChorusModeWarped: return "Warped";
    case kVoLumChorusModeClear: return "Clear";
    case kVoLumChorusModeEnsemble: return "Ensemble";
    default: return "Warped";
  }
}

class ChorusDSP
{
public:
  enum Mode
  {
    kClassic = kVoLumChorusModeClassic,
    kWarped = kVoLumChorusModeWarped,
    kClear = kVoLumChorusModeClear,
    kEnsemble = kVoLumChorusModeEnsemble,
    kNumModes = kVoLumChorusModeCount,
  };

  // Knob (0..1) to real units, for display as well as for the DSP.
  static double RateHz(int mode, double knob)
  {
    const ModeTuning& t = _Tuning(mode);
    return t.rateMinHz * std::pow(t.rateMaxHz / t.rateMinHz, _Clamp01(knob));
  }
  // Peak detune in cents; for CLASSIC the delay sweep in ms peak-to-peak.
  static double DepthAmount(int mode, double knob) { return _Tuning(mode).depthMax * _Clamp01(knob); }
  static bool DepthIsSweepMs(int mode) { return _SanitizeMode(mode) == kClassic; }
  static double ToneHz(double knob) { return kToneMinHz * std::pow(kToneMaxHz / kToneMinHz, _Clamp01(knob)); }

  void Prepare(double sampleRate, int /*maxBlockSize*/, int /*numChannels*/)
  {
    mSampleRate = _ValidRate(sampleRate) ? sampleRate : 48000.0;
    _Allocate(mSampleRate);
    Reset();
  }

  // All five knobs are 0..1; the per-mode tables below turn them into Hz,
  // cents and ms so one knob row can drive four voices.
  void SetParams(double rate, double depth, double tone, double width, double mix, int mode, double sampleRate)
  {
    mRateT = _Clamp01(rate);
    mDepthT = _Clamp01(depth);
    mToneT = _Clamp01(tone);
    mWidthT = _Clamp01(width);
    mMixT = _Clamp01(mix);
    mModeTarget = _SanitizeMode(mode);
    // Prepare sized the lines for at least 192 kHz. A faster rate arriving here
    // clamps the longest delays instead of growing the line off the audio path.
    if (_ValidRate(sampleRate) && sampleRate != mSampleRate)
    {
      mSampleRate = sampleRate;
      Reset();
    }
    // Straight after Reset nothing is sounding yet, so the voice switches now.
    // Mid-stream, _BeginSubBlock ducks the wet, switches and fades back in.
    if (mSnap && mModeTarget != mMode)
    {
      mMode = mModeTarget;
      _ResetVoice();
    }
  }

  // Clears the lines and snaps every knob: the bypass edge and a fresh start.
  void Reset()
  {
    for (auto& line : mLine)
      std::fill(line.begin(), line.end(), 0.0);
    mWrite = 0;
    mRateS = mRateT;
    mDepthS = mDepthT;
    mToneS = mToneT;
    mWidthS = mWidthT;
    mMixS = mMixT;
    mMode = mModeTarget;
    mDuckLevel = 1.0;
    mMinRead = 1e300;
    _ResetVoice();
    mSnap = true;
  }

  // Smallest delay any voice asked for since Reset, in ms (a test probe for the
  // 1 ms floor that every excursion budget is built on).
  double MinReadDelayMs() const { return mMinRead * 1000.0 / mSampleRate; }

  void Process(double** io, int numChannels, int numFrames)
  {
    if (io == nullptr || numChannels <= 0 || numFrames <= 0 || mMask == 0)
      return;
    const int chans = std::min(numChannels, 2);
    for (int c = 0; c < chans; ++c)
      if (io[c] == nullptr)
        return;

    for (int offset = 0; offset < numFrames; offset += kSubBlock)
    {
      const int n = std::min(kSubBlock, numFrames - offset);
      double* ch[2] = {io[0] + offset, (chans > 1) ? io[1] + offset : nullptr};
      _BeginSubBlock(n);
      switch (mMode)
      {
        case kClassic: _RunClassic(ch, chans, n); break;
        case kWarped: _RunWarped(ch, chans, n); break;
        case kClear: _RunClear(ch, chans, n); break;
        case kEnsemble:
        default: _RunEnsemble(ch, chans, n); break;
      }
      _FlushState();
    }
  }

private:
  struct ModeTuning
  {
    double rateMinHz;
    double rateMaxHz;
    double depthMax; // cents (CLASSIC: ms peak-to-peak)
    double baseMs;
    double hpfHz; // wet high-pass, 0 = none
    double driftMinSec;
    double driftMaxSec;
  };

  static const ModeTuning& _Tuning(int mode)
  {
    static const ModeTuning kTuning[kNumModes] = {
      {0.10, 10.0, 6.0, 1.6, 0.0, 4.0, 8.0}, // CLASSIC: Juno sweep, no drift
      {0.20, 6.0, 50.0, 5.0, 0.0, 1.5, 4.0}, // WARPED: tape wow, busy drift
      {0.10, 2.0, 20.0, 7.5, 80.0, 4.0, 9.0}, // CLEAR: Dimension, 80 Hz wet HPF
      {0.15, 3.0, 25.0, 5.5, 90.0, 3.0, 7.0}, // ENSEMBLE: tri-stereo rack
    };
    return kTuning[_SanitizeMode(mode)];
  }

  // Slow random wander: cosine-eased moves between random targets in [-1, 1],
  // each lasting a random time in [minSec, maxSec]. Smooth in value and slope.
  struct Drift
  {
    uint32_t state = 1;
    double from = 0.0;
    double to = 0.0;
    double pos = 0.0;
    double period = 4.0;
    double minSec = 4.0;
    double maxSec = 8.0;

    void Seed(uint32_t seed, double lo, double hi)
    {
      state = seed ? seed : 1u;
      minSec = lo;
      maxSec = hi;
      from = 0.0;
      to = 2.0 * _Uniform() - 1.0;
      period = minSec + (maxSec - minSec) * _Uniform();
      pos = 0.0;
    }

    double Advance(double dtSec)
    {
      pos += dtSec / period;
      while (pos >= 1.0)
      {
        pos -= 1.0;
        from = to;
        to = 2.0 * _Uniform() - 1.0;
        period = minSec + (maxSec - minSec) * _Uniform();
      }
      return from + (to - from) * (0.5 - 0.5 * std::cos(M_PI * pos));
    }

    double _Uniform()
    {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      return static_cast<double>(state >> 8) * (1.0 / 16777216.0);
    }
  };

  // Sine/cosine oscillator by rotation; renormalized once per sub-block.
  struct Phasor
  {
    double c = 1.0;
    double s = 0.0;
    double rc = 1.0;
    double rs = 0.0;

    void SetHz(double hz, double sampleRate)
    {
      const double w = 2.0 * M_PI * hz / sampleRate;
      rc = std::cos(w);
      rs = std::sin(w);
      const double g = 1.0 / std::sqrt(c * c + s * s);
      c *= g;
      s *= g;
    }
    void Tick()
    {
      const double nc = c * rc - s * rs;
      s = s * rc + c * rs;
      c = nc;
    }
  };

  // Per-sample linear ramp toward a per-sub-block target.
  struct Ramp
  {
    double v = 0.0;
    double d = 0.0;
    void To(double target, double invN, bool snap)
    {
      if (snap)
      {
        v = target;
        d = 0.0;
      }
      else
        d = (target - v) * invN;
    }
    double Tick()
    {
      v += d;
      return v;
    }
  };

  enum class MixKind
  {
    kDry,
    kWet,
    kBlend
  };

  static constexpr int kSubBlock = 32;
  static constexpr int kNumDrift = 3;
  static constexpr int kNumRamps = 8;
  static constexpr double kToneMinHz = 3000.0;
  static constexpr double kToneMaxHz = 12000.0;
  static constexpr double kKnobSmoothSec = 0.03;
  static constexpr double kMinDelayMs = 1.0;
  static constexpr double kMaxExcursionMs = 12.0;
  // Longest read: WARPED centre (1 + 12 + 4 ms) plus the same excursion again.
  static constexpr double kMaxDelayMs = 40.0;
  static constexpr double kMinLineRate = 192000.0;
  static constexpr double kMixSmoothSec = 0.02;
  static constexpr double kMixSnap = 1e-4;
  // Mode switch: the wet ducks to silence over two sub-blocks, the voice state
  // resets while nothing of it is audible, then the new voice fades in.
  static constexpr double kDuckDownPerSubBlock = 0.5;
  static constexpr double kDuckUpSec = 0.01;
  static constexpr double kWarpedFlutterHz = 9.3;
  static constexpr double kClearCross = 0.35; // Dimension-style share of the opposite voice
  static constexpr double kEnsembleBaseMs[3] = {7.0, 5.5, 8.5}; // L, C, R voices
  // Centre voice level against the outer ones (-6 dB). It shares the centre
  // with the dry, so a quieter centre keeps the image wide at MIX 0.5.
  static constexpr double kEnsembleCentre = 0.5;
  // A 1-1 track hears all three voices with unequal weights: equal weights on
  // three sines 120 deg apart would cancel the pitch motion of low notes.
  static constexpr double kEnsembleMono[3] = {1.0, 0.5, 0.35};

  static double _EnsembleNorm() { return 1.0 / std::sqrt(1.0 + 0.5 * kEnsembleCentre * kEnsembleCentre); }
  static double _EnsembleMonoNorm()
  {
    return 1.0
           / std::sqrt(kEnsembleMono[0] * kEnsembleMono[0] + kEnsembleMono[1] * kEnsembleMono[1]
                       + kEnsembleMono[2] * kEnsembleMono[2]);
  }

  static int _SanitizeMode(int mode) { return (mode < 0 || mode >= kNumModes) ? kVoLumChorusModeDefault : mode; }
  // NaN-safe: std::clamp passes NaN through, and a NaN knob would latch in the smoothing.
  static double _Clamp01(double v) { return v > 0.0 ? (v < 1.0 ? v : 1.0) : 0.0; }
  static bool _ValidRate(double sampleRate) { return std::isfinite(sampleRate) && sampleRate > 0.0; }
  static double _Flush(double v) { return (std::isfinite(v) && std::abs(v) >= 1e-20) ? v : 0.0; }

  // Sine-LFO excursion (half peak-to-peak, samples) for a peak detune in cents.
  double _SineExcursion(double cents, double hz) const
  {
    const double ratio = std::pow(2.0, cents / 1200.0) - 1.0;
    return std::min(ratio / (2.0 * M_PI * hz), kMaxExcursionMs * 0.001) * mSampleRate;
  }

  void _Allocate(double sampleRate)
  {
    const double lineRate = std::max(sampleRate, kMinLineRate);
    const size_t need = static_cast<size_t>(kMaxDelayMs * 0.001 * lineRate) + 8;
    size_t size = 1;
    while (size < need)
      size <<= 1;
    for (auto& line : mLine)
      line.assign(size, 0.0);
    mMask = static_cast<int>(size) - 1;
    mWrite = 0;
  }

  // Voice state only (LFOs, drift, smoothers, filters). The lines keep running,
  // so a mode switch never replays or drops the input history.
  void _ResetVoice()
  {
    mLfo = Phasor{};
    mFlutter[0] = mFlutter[1] = Phasor{};
    mTriPhase = 0.0;
    const ModeTuning& t = _Tuning(mMode);
    for (int i = 0; i < kNumDrift; ++i)
      mDrift[i].Seed(0x9E3779B9u * static_cast<uint32_t>(i + 1) + 0x632BE5ABu, t.driftMinSec, t.driftMaxSec);
    for (int c = 0; c < 2; ++c)
    {
      mSmooth1[c] = mSmooth2[c] = 0.0;
      mZ1[c] = mZ2[c] = 0.0;
      mHpX[c] = mHpY[c] = 0.0;
    }
    mSnapVoice = true;
  }

  void _BeginSubBlock(int n)
  {
    const bool snap = mSnap;
    const double invN = 1.0 / static_cast<double>(n);
    const double k = snap ? 1.0 : 1.0 - std::exp(-static_cast<double>(n) / (kKnobSmoothSec * mSampleRate));
    mRateS += (mRateT - mRateS) * k;
    mDepthS += (mDepthT - mDepthS) * k;
    mToneS += (mToneT - mToneS) * k;
    mWidthS += (mWidthT - mWidthS) * k;
    // MIX glides like every other knob and lands exactly on its target once it
    // is inaudibly close, so MIX 0 becomes bit-identical and MIX 1 fully wet.
    const double kMix = snap ? 1.0 : 1.0 - std::exp(-static_cast<double>(n) / (kMixSmoothSec * mSampleRate));
    mMixS += (mMixT - mMixS) * kMix;
    if (std::abs(mMixS - mMixT) < kMixSnap)
      mMixS = mMixT;

    const double mixStart = snap ? mMixS : mMixPrev;
    if (mixStart <= 0.0 && mMixS <= 0.0)
      mMixKind = MixKind::kDry;
    else if (mixStart >= 1.0 && mMixS >= 1.0)
      mMixKind = MixKind::kWet;
    else
      mMixKind = MixKind::kBlend;
    mDryGain.To(std::cos(mMixS * M_PI * 0.5), invN, snap);
    mWetGain.To(std::sin(mMixS * M_PI * 0.5), invN, snap);
    mMixPrev = mMixS;

    double duck = mDuckLevel;
    if (mModeTarget != mMode)
    {
      if (duck <= 0.0)
      {
        mMode = mModeTarget;
        _ResetVoice();
      }
      else
        duck = std::max(0.0, duck - kDuckDownPerSubBlock);
    }
    else
      duck = std::min(1.0, duck + static_cast<double>(n) / (kDuckUpSec * mSampleRate));
    mDuck.To(duck, invN, snap);
    mDuckLevel = duck;

    const bool snapVoice = snap || mSnapVoice;
    _SetTone(ToneHz(mToneS));
    const double hpf = _Tuning(mMode).hpfHz;
    mHpA = (hpf > 0.0) ? std::exp(-2.0 * M_PI * hpf / mSampleRate) : 0.0;

    const double ms = mSampleRate * 0.001;
    const double dt = static_cast<double>(n) / mSampleRate;
    const double rateHz = RateHz(mMode, mRateS);
    const double depth = DepthAmount(mMode, mDepthS);
    switch (mMode)
    {
      case kClassic:
      {
        mTriInc = rateHz / mSampleRate;
        mRamp[0].To(depth * ms, invN, snapVoice); // sweep, samples p-p
        mRamp[1].To(0.5 * mWidthS, invN, snapVoice); // right LFO offset, cycles
        // Two one-poles on the delay round the triangle's corners, like a BBD
        // averaging its clock over the line: about 6 ms per pole at 0.63 Hz.
        const double tau = std::clamp(0.004 / rateHz, 0.0005, 0.008);
        mSmoothCoef = 1.0 - std::exp(-1.0 / (tau * mSampleRate));
        break;
      }
      case kWarped:
      {
        mLfo.SetHz(rateHz, mSampleRate);
        const double wow = _SineExcursion(depth, rateHz);
        const double drift = std::min(0.8 * wow, 4.0 * ms);
        const double flutter = _SineExcursion(0.12 * depth, kWarpedFlutterHz);
        // The right side mixes two drifts as cos*L + sin*R, up to sqrt(2) x drift.
        const double centre =
          std::max(_Tuning(kWarped).baseMs * ms, kMinDelayMs * ms + wow + std::sqrt(2.0) * drift + flutter);
        const double theta = mWidthS * M_PI * 0.5;
        mFlutter[0].SetHz(kWarpedFlutterHz, mSampleRate);
        mFlutter[1].SetHz(kWarpedFlutterHz * (1.0 + 0.11 * mWidthS), mSampleRate);
        mRamp[0].To(centre, invN, snapVoice);
        mRamp[1].To(wow, invN, snapVoice);
        mRamp[2].To(drift, invN, snapVoice);
        mRamp[3].To(flutter, invN, snapVoice);
        mRamp[4].To(std::cos(theta), invN, snapVoice);
        mRamp[5].To(std::sin(theta), invN, snapVoice);
        mRamp[6].To(mDrift[0].Advance(dt), invN, snapVoice);
        mRamp[7].To(mDrift[1].Advance(dt), invN, snapVoice);
        break;
      }
      case kClear:
      {
        // The drift moves the shared LFO rate, so the voices stay exactly
        // anti-phase and the mono sum stays free of pitch wobble.
        mLfo.SetHz(rateHz * (1.0 + 0.15 * mDrift[0].Advance(dt)), mSampleRate);
        const double excursion = _SineExcursion(depth, rateHz);
        const double centre = std::max(_Tuning(kClear).baseMs * ms, kMinDelayMs * ms + excursion);
        const double w = mWidthS;
        const double m2 = 0.5 * (1.0 - kClearCross) * (1.0 - kClearCross);
        const double s2 = 0.5 * (1.0 + kClearCross) * (1.0 + kClearCross);
        mRamp[0].To(centre, invN, snapVoice);
        mRamp[1].To(excursion, invN, snapVoice);
        mRamp[2].To(w, invN, snapVoice);
        mRamp[3].To(1.0 / std::sqrt(m2 + w * w * s2), invN, snapVoice);
        break;
      }
      case kEnsemble:
      default:
      {
        mLfo.SetHz(rateHz, mSampleRate);
        const double excursion = _SineExcursion(depth, rateHz);
        const double drift = std::min(0.25 * excursion, 3.0 * ms);
        const double shift = std::max(0.0, kMinDelayMs * ms + excursion + drift - kEnsembleBaseMs[1] * ms);
        // Constant-power pan of the outer voices at -WIDTH / +WIDTH, centre
        // voice fixed, so the per-side power sum is the same at every WIDTH.
        const double norm = _EnsembleNorm();
        mRamp[0].To(excursion, invN, snapVoice);
        mRamp[1].To(drift, invN, snapVoice);
        mRamp[2].To(shift, invN, snapVoice);
        mRamp[3].To(norm * std::sqrt(0.5 * (1.0 + mWidthS)), invN, snapVoice); // outer voice, own side
        mRamp[4].To(norm * std::sqrt(0.5 * (1.0 - mWidthS)), invN, snapVoice); // outer voice, far side
        mRamp[5].To(mDrift[0].Advance(dt), invN, snapVoice);
        mRamp[6].To(mDrift[1].Advance(dt), invN, snapVoice);
        mRamp[7].To(mDrift[2].Advance(dt), invN, snapVoice);
        break;
      }
    }
    mSnapSmooth = snapVoice;
    mSnap = false;
    mSnapVoice = false;
  }

  void _SetTone(double hz)
  {
    const double fc = std::min(hz, 0.45 * mSampleRate);
    const double w0 = 2.0 * M_PI * fc / mSampleRate;
    const double cw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * 0.70710678118654752);
    const double a0 = 1.0 + alpha;
    mB0 = 0.5 * (1.0 - cw) / a0;
    mB1 = (1.0 - cw) / a0;
    mA1 = -2.0 * cw / a0;
    mA2 = (1.0 - alpha) / a0;
  }

  void _FlushState()
  {
    for (int c = 0; c < 2; ++c)
    {
      mZ1[c] = _Flush(mZ1[c]);
      mZ2[c] = _Flush(mZ2[c]);
      mHpX[c] = _Flush(mHpX[c]);
      mHpY[c] = _Flush(mHpY[c]);
    }
  }

  // Every mode writes all three lines (a 1-1 track copies L into R and mid), so
  // whichever voice a mode switch lands on reads current history.
  void _WriteInputs(double* const* ch, int chans, int i)
  {
    const size_t w = static_cast<size_t>(mWrite);
    const double l = _Flush(ch[0][i]);
    const double r = (chans > 1) ? _Flush(ch[1][i]) : l;
    mLine[0][w] = l;
    mLine[1][w] = r;
    mLine[2][w] = 0.5 * (l + r);
  }

  // 4-point Hermite read `delaySamples` behind the write head, clamped into the
  // line so a corrupt parameter can never index out of bounds.
  double _Read(int line, double delaySamples)
  {
    if (delaySamples < mMinRead)
      mMinRead = delaySamples;
    const double* buf = mLine[line].data();
    const double d = std::clamp(delaySamples, 2.0, static_cast<double>(mMask - 3));
    const int i0 = static_cast<int>(d);
    const double f = d - static_cast<double>(i0);
    const int r0 = (mWrite - i0) & mMask;
    const double ym1 = buf[(r0 + 1) & mMask];
    const double y0 = buf[r0];
    const double y1 = buf[(r0 - 1) & mMask];
    const double y2 = buf[(r0 - 2) & mMask];
    const double c1 = 0.5 * (y1 - ym1);
    const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
    const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
  }

  struct FrameGains
  {
    double dry;
    double wet;
    double duck;
  };

  FrameGains _TickGains() { return {mDryGain.Tick(), mWetGain.Tick(), mDuck.Tick()}; }

  // Wet high-pass (if the mode has one), TONE low-pass, the mode-switch duck,
  // then the MIX blend.
  void _Emit(double& sample, int c, double wet, const FrameGains& g)
  {
    if (mHpA > 0.0)
    {
      const double h = mHpA * (mHpY[c] + wet - mHpX[c]);
      mHpX[c] = wet;
      mHpY[c] = h;
      wet = h;
    }
    const double y = mB0 * wet + mZ1[c];
    mZ1[c] = mB1 * wet - mA1 * y + mZ2[c];
    mZ2[c] = mB0 * wet - mA2 * y;
    switch (mMixKind)
    {
      case MixKind::kDry: break;
      case MixKind::kWet: sample = y * g.duck; break;
      case MixKind::kBlend: sample = sample * g.dry + y * (g.wet * g.duck); break;
    }
  }

  void _Advance() { mWrite = (mWrite + 1) & mMask; }

  static double _Tri01(double phase)
  {
    const double u = phase - std::floor(phase);
    return (u < 0.5) ? 2.0 * u : 2.0 - 2.0 * u;
  }

  // Tape-style soft saturation: a rational tanh at low drive, near-unity gain.
  static double _TapeSat(double x)
  {
    constexpr double kDrive = 0.6;
    const double u = std::clamp(kDrive * x, -3.0, 3.0);
    return u * (27.0 + u * u) / (27.0 + 9.0 * u * u) * (1.0 / kDrive);
  }

  void _RunClassic(double* const* ch, int chans, int n)
  {
    const double minDelay = _Tuning(kClassic).baseMs * mSampleRate * 0.001;
    for (int i = 0; i < n; ++i)
    {
      const double sweep = mRamp[0].Tick();
      const double offset = mRamp[1].Tick();
      const FrameGains g = _TickGains();
      _WriteInputs(ch, chans, i);
      mTriPhase += mTriInc;
      if (mTriPhase >= 1.0)
        mTriPhase -= 1.0;
      for (int c = 0; c < chans; ++c)
      {
        const double target = minDelay + sweep * _Tri01((c == 0) ? mTriPhase : mTriPhase + offset);
        if (mSnapSmooth)
          mSmooth1[c] = mSmooth2[c] = target;
        mSmooth1[c] += (target - mSmooth1[c]) * mSmoothCoef;
        mSmooth2[c] += (mSmooth1[c] - mSmooth2[c]) * mSmoothCoef;
        _Emit(ch[c][i], c, _Read(c, mSmooth2[c]), g);
      }
      mSnapSmooth = false;
      _Advance();
    }
  }

  void _RunWarped(double* const* ch, int chans, int n)
  {
    for (int i = 0; i < n; ++i)
    {
      const double centre = mRamp[0].Tick();
      const double wow = mRamp[1].Tick();
      const double drift = mRamp[2].Tick();
      const double flutter = mRamp[3].Tick();
      const double cosW = mRamp[4].Tick();
      const double sinW = mRamp[5].Tick();
      const double driftL = mRamp[6].Tick();
      const double driftR = mRamp[7].Tick();
      const FrameGains g = _TickGains();
      _WriteInputs(ch, chans, i);
      mLfo.Tick();
      mFlutter[0].Tick();
      mFlutter[1].Tick();
      const double dL = centre + wow * mLfo.s + drift * driftL + flutter * mFlutter[0].s;
      _Emit(ch[0][i], 0, _TapeSat(_Read(0, dL)), g);
      if (chans > 1)
      {
        // WIDTH 0 leaves every right-side term equal to the left one.
        const double wowR = mLfo.s * cosW + mLfo.c * sinW;
        const double dR = centre + wow * wowR + drift * (cosW * driftL + sinW * driftR) + flutter * mFlutter[1].s;
        _Emit(ch[1][i], 1, _TapeSat(_Read(1, dR)), g);
      }
      _Advance();
    }
  }

  void _RunClear(double* const* ch, int chans, int n)
  {
    const double mid = 0.5 * (1.0 - kClearCross);
    const double side = 0.5 * (1.0 + kClearCross);
    const double monoGain = std::sqrt(2.0) / (1.0 - kClearCross);
    for (int i = 0; i < n; ++i)
    {
      const double centre = mRamp[0].Tick();
      const double excursion = mRamp[1].Tick();
      const double width = mRamp[2].Tick();
      const double gain = mRamp[3].Tick();
      const FrameGains g = _TickGains();
      _WriteInputs(ch, chans, i);
      mLfo.Tick();
      const double a = _Read(0, centre + excursion * mLfo.s);
      const double b = _Read(1, centre - excursion * mLfo.s);
      // L = a - k b, R = b - k a, expressed as mid/side so WIDTH scales the side.
      const double m = mid * (a + b);
      if (chans > 1)
      {
        const double s = side * (a - b);
        _Emit(ch[0][i], 0, gain * (m + width * s), g);
        _Emit(ch[1][i], 1, gain * (m - width * s), g);
      }
      else
        _Emit(ch[0][i], 0, monoGain * m, g);
      _Advance();
    }
  }

  void _RunEnsemble(double* const* ch, int chans, int n)
  {
    constexpr double kC120 = -0.5;
    constexpr double kS120 = 0.86602540378443865;
    const double ms = mSampleRate * 0.001;
    const double baseL = kEnsembleBaseMs[0] * ms;
    const double baseC = kEnsembleBaseMs[1] * ms;
    const double baseR = kEnsembleBaseMs[2] * ms;
    const double centreGain = kEnsembleCentre * std::sqrt(0.5) * _EnsembleNorm();
    const double monoNorm = _EnsembleMonoNorm();
    for (int i = 0; i < n; ++i)
    {
      const double excursion = mRamp[0].Tick();
      const double drift = mRamp[1].Tick();
      const double shift = mRamp[2].Tick();
      const double nearGain = mRamp[3].Tick();
      const double farGain = mRamp[4].Tick();
      const double drift0 = mRamp[5].Tick();
      const double drift1 = mRamp[6].Tick();
      const double drift2 = mRamp[7].Tick();
      const FrameGains g = _TickGains();
      _WriteInputs(ch, chans, i);
      mLfo.Tick();
      const double lfoL = mLfo.s;
      const double lfoC = mLfo.s * kC120 + mLfo.c * kS120;
      const double lfoR = mLfo.s * kC120 - mLfo.c * kS120;
      const double vL = _Read(0, baseL + shift + excursion * lfoL + drift * drift0);
      const double vC = _Read(2, baseC + shift + excursion * lfoC + drift * drift1);
      const double vR = _Read(1, baseR + shift + excursion * lfoR + drift * drift2);
      if (chans > 1)
      {
        _Emit(ch[0][i], 0, nearGain * vL + centreGain * vC + farGain * vR, g);
        _Emit(ch[1][i], 1, farGain * vL + centreGain * vC + nearGain * vR, g);
      }
      else
        _Emit(ch[0][i], 0, monoNorm * (kEnsembleMono[0] * vL + kEnsembleMono[1] * vC + kEnsembleMono[2] * vR), g);
      _Advance();
    }
  }

  double mSampleRate = 48000.0;
  int mMode = kVoLumChorusModeDefault;
  int mModeTarget = kVoLumChorusModeDefault;

  // Knob targets (from SetParams) and their smoothed values.
  double mRateT = 0.44, mDepthT = 0.36, mToneT = 0.21, mWidthT = 0.6, mMixT = 0.0;
  double mRateS = 0.44, mDepthS = 0.36, mToneS = 0.21, mWidthS = 0.6, mMixS = 0.0;
  double mMixPrev = 0.0;
  bool mSnap = true;
  bool mSnapVoice = true;
  bool mSnapSmooth = true;
  MixKind mMixKind = MixKind::kDry;
  Ramp mDryGain;
  Ramp mWetGain;
  Ramp mDuck;
  double mDuckLevel = 1.0;
  Ramp mRamp[kNumRamps];

  Phasor mLfo;
  Phasor mFlutter[2];
  Drift mDrift[kNumDrift];
  double mTriPhase = 0.0;
  double mTriInc = 0.0;
  double mSmoothCoef = 1.0;
  double mSmooth1[2] = {0.0, 0.0};
  double mSmooth2[2] = {0.0, 0.0};

  double mB0 = 1.0, mB1 = 0.0, mA1 = 0.0, mA2 = 0.0;
  double mZ1[2] = {0.0, 0.0};
  double mZ2[2] = {0.0, 0.0};
  double mHpA = 0.0;
  double mHpX[2] = {0.0, 0.0};
  double mHpY[2] = {0.0, 0.0};
  double mMinRead = 1e300;

  // L, R and mid (ENSEMBLE centre voice) lines; power-of-two size.
  std::vector<double> mLine[3];
  int mMask = 0;
  int mWrite = 0;
};

} // namespace volum
