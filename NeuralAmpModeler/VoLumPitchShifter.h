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
// against a commercial low-latency shifter, this engine matches it on pitch
// accuracy (~0.5 cents), drift (~3 cents) and warble, at roughly half the old
// latency.
//
// Three CHARACTERs trade latency vs accuracy / monophonic vs polyphonic:
//   DROP:    WSOLA splice search, PERIOD-synchronous -> exact mono pitch even at
//            +/-12, ~17 ms. Monophonic (one period estimate).
//   INSTANT: period-sync, no search -> tightest ~8.6 ms. Monophonic.
//   POLY:    FIXED-grain WSOLA splice (no pitch estimate) -> tracks CHORDS, not
//            just single notes, at ~14 ms. A time-domain single-pointer granular
//            shifter (no FFT, no phase vocoder). What keeps it cheap: the read
//            pointer is clamped to >= xfade by the splice, so the WSOLA search and
//            correlation windows are HISTORY reads that need not inflate the delay
//            floor (latency = xfade + 0.5*band, not +search+corr). That collapsed
//            POLY from ~49 ms to ~14 ms while keeping exact pitch and polyphony.
//
// SPLICE ALIGNMENT IS THE THING TO PROTECT. Two separate releases tried to cure a
// POLY upshift crackle by reasoning about splice CADENCE, and both were wrong:
// a shortened grain is harmless as long as the join is waveform-aligned, while a
// perfectly regular cadence still crackles if the joins land at arbitrary phase.
// Constrain WSOLA's SEARCH RANGE, never its RESULT - clamping the result throws
// away the alignment it just found. `tests/test_volum_pitch_artifacts.cpp` measures
// this directly (non-harmonic residual, impulsiveness, per-splice correlation)
// instead of counting splices.
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

#include "VoLumLevelMute.h"
#include "VoLumPitchVoice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dsp
{
namespace effect
{

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
  // Hosts occasionally grow their callback block without another OnReset. Reserve
  // a generous block off the audio thread so Process never has to allocate.
  static constexpr int kRealtimeBlockReserve = 8192;

  // Allocates - call OFF the audio thread (OnReset), never from ProcessBlock.
  void Configure(double sampleRate, int maxBlockSize)
  {
    mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    mMaxBlock = std::max({maxBlockSize, 64, kRealtimeBlockReserve});
    const bool sameConfig = mConfigured && mSampleRate == mConfiguredSampleRate && mMaxBlock <= mConfiguredMaxBlock;
    if (!sameConfig)
    {
      for (auto& voice : mVoices)
        voice.Configure(mSampleRate, mMaxBlock);
      mConfiguredSampleRate = mSampleRate;
      mConfiguredMaxBlock = mMaxBlock;
      mConfigured = true;
      mVoicesInStep = true; // Configure resets both voices
    }
    _ApplyCharacters();
    const double fc = 3200.0;
    mVintageLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fc / mSampleRate);
    _AllocateScratch();
  }

  int Latency() const { return mLatency; }
  bool Configured() const { return mConfigured; }
  int PreparedBlockSize() const { return mMaxBlock; }
  // Test only: the pre-blocking tracker and splice search, each voice tracking on its own.
  void DebugSetReferenceKernels(bool on)
  {
    mReferenceKernels = on;
    for (auto& voice : mVoices)
      voice.DebugSetReferenceKernels(on);
  }

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
    mVoicesInStep = true;
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
    // Mute floor matches kPrePitchLevel Init min (-20 dB) and the −∞ display
    // contract. ProcessBlock passes the raw param dB; this is the mapping.
    const double clampedDb = std::clamp(levelDb, -20.0, 20.0);
    mLevel = volum::DbToAmpWithMuteFloor(clampedDb, -20.0);
    _ApplyCharacters();
  }

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, size_t numChannels, size_t numFrames)
  {
    // Never allocate/reconfigure from the audio thread. An unexpectedly huge
    // callback passes through dry for that block; the normal reserve covers all
    // practical host sizes.
    if (!_PrepareIO(numChannels, numFrames))
      return inputs;

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
      size_t readAt = mDryWrite + ringLen - lat;
      if (readAt >= ringLen)
        readAt -= ringLen;
      mDryScratch[i] = mDryRing[readAt];
      if (++mDryWrite >= ringLen)
        mDryWrite = 0;
    }

    if (mMode == Mode::Transpose)
    {
      mVoices[0].SetRatio(std::pow(2.0, mSemitones / 12.0));
      mVoices[0].Process(in, mWet0.data(), numFrames);
      // Voice 1 skipped this input, so its ring is stale until the next Reset.
      if (numFrames > 0)
        mVoicesInStep = false;
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
      // Both voices hold the same ring and reach each period update at the same sample, so the up
      // voice replays the down voice's estimates. Voice 0 runs its whole block first, which is
      // why the estimates are logged in order rather than shared as one value.
      if (mVoicesInStep && !mReferenceKernels && mVoices[0].TrackerMatches(mVoices[1]))
      {
        GranularVoice::PeriodLog log{mPeriodLog.data(), mPeriodLog.size(), 0};
        mVoices[0].ProcessRecordingPeriods(in, mWet0.data(), numFrames, log);
        mVoices[1].ProcessReplayingPeriods(in, mWet1.data(), numFrames, log);
      }
      else
      {
        mVoices[0].Process(in, mWet0.data(), numFrames);
        mVoices[1].Process(in, mWet1.data(), numFrames);
      }
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
    // One slot per period update a block can hold. A shorter log only costs the up voice its own
    // estimates past the end, never a wrong one.
    const size_t updateEvery = static_cast<size_t>(std::max(1L, std::lround(mSampleRate * 0.01)));
    const size_t logNeed = cap / updateEvery + 2;
    if (mPeriodLog.size() < logNeed)
      mPeriodLog.assign(logNeed, 0.0);
  }

  bool _PrepareIO(size_t numChannels, size_t numFrames) const
  {
    return numChannels <= kMaxChannels && static_cast<int>(numFrames) <= mMaxBlock && mWet0.size() >= numFrames;
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

  // Both voices were reset together and have processed the same input since.
  bool mVoicesInStep = false;
  std::vector<double> mPeriodLog;
  bool mReferenceKernels = false;
  std::array<std::vector<DSP_SAMPLE>, kMaxChannels> mOut;
  std::array<DSP_SAMPLE*, kMaxChannels> mOutPtrs{nullptr, nullptr};
};

} // namespace effect
} // namespace dsp
