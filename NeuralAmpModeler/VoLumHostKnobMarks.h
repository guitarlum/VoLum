#pragma once

#include <atomic>
#include <cstdint>

#include "VoLumModeTransition.h"
#include "VoLumParams.h"

// Host-written per-mode knobs. A host (or an AU validator) can set a mode and the
// knobs that belong to it in one burst, before the deferred mode transaction runs.
// The knobs it wrote are its explicit values: the transaction must not recall the
// incoming mode's remembered value over them. Marks are set from the audio thread
// (lock-free, no allocation) and consumed by the main-thread transaction, which
// ends the burst: a write that was not followed by a mode change in the same
// window is not carried into a later one.
namespace volum
{
// Dense id of a knob that a per-mode snapshot saves and restores, or -1.
// Reverb and Oktaverb share their five knobs, so they share ids.
constexpr int HostKnobId(int paramIdx)
{
  switch (paramIdx)
  {
    case kPrePitchMix: return 0;
    case kPrePitchDry: return 1;
    case kPrePitchLevel: return 2;
    case kPrePitchVoicing: return 3;
    case kDelayTime: return 4;
    case kDelayFeedback: return 5;
    case kDelayMix: return 6;
    case kDelayTone: return 7;
    case kDelayAge: return 8;
    case kDelayPingPong: return 9;
    case kReverbMix: return 10;
    case kReverbDecay: return 11;
    case kReverbTone: return 12;
    case kReverbPreDelay: return 13;
    case kReverbShimmer: return 14;
    case kTremoloRate: return 15;
    case kTremoloDepth: return 16;
    case kTremoloShape: return 17;
    case kTremoloMix: return 18;
    case kTremoloCrossover: return 19;
    case kChorusRate: return 20;
    case kChorusDepth: return 21;
    case kChorusTone: return 22;
    case kChorusWidth: return 23;
    case kChorusMix: return 24;
    default: return -1;
  }
}

constexpr int kHostKnobCount = 25;

// Param index for each dense id.
constexpr int kHostKnobParams[kHostKnobCount] = {
  kPrePitchMix,   kPrePitchDry, kPrePitchLevel, kPrePitchVoicing, kDelayTime,   kDelayFeedback,    kDelayMix,
  kDelayTone,     kDelayAge,    kDelayPingPong, kReverbMix,       kReverbDecay, kReverbTone,       kReverbPreDelay,
  kReverbShimmer, kTremoloRate, kTremoloDepth,  kTremoloShape,    kTremoloMix,  kTremoloCrossover, kChorusRate,
  kChorusDepth,   kChorusTone,  kChorusWidth,   kChorusMix};

constexpr std::uint32_t HostKnobBit(int paramIdx)
{
  const int id = HostKnobId(paramIdx);
  return id < 0 ? 0u : (1u << static_cast<unsigned>(id));
}

// Knobs a mode change recalls, as marks. Reverb and Oktaverb share theirs.
constexpr std::uint32_t HostKnobMaskForTargets(unsigned targets)
{
  std::uint32_t mask = 0;
  const auto has = [targets](ModeSnapshotTarget t) { return (targets & ModeSnapshotBit(t)) != 0; };
  if (has(ModeSnapshotTarget::PrePitch))
    mask |= 0x0000000Fu; // ids 0-3
  if (has(ModeSnapshotTarget::Delay))
    mask |= 0x000003F0u; // ids 4-9
  if (has(ModeSnapshotTarget::Reverb) || has(ModeSnapshotTarget::Oktaverb))
    mask |= 0x00007C00u; // ids 10-14
  if (has(ModeSnapshotTarget::Tremolo))
    mask |= 0x000F8000u; // ids 15-19
  if (has(ModeSnapshotTarget::Chorus))
    mask |= 0x01F00000u; // ids 20-24
  return mask;
}

class HostKnobMarks
{
public:
  void Mark(int paramIdx) noexcept { mMask.fetch_or(HostKnobBit(paramIdx), std::memory_order_release); }
  std::uint32_t Take() noexcept { return mMask.exchange(0, std::memory_order_acquire); }
  // The host wrote this knob after the transaction took its window.
  bool IsMarked(int paramIdx) const noexcept
  {
    return (mMask.load(std::memory_order_acquire) & HostKnobBit(paramIdx)) != 0;
  }
  // A transaction that has to wait (Retry) hands its window back.
  void Return(std::uint32_t mask) noexcept { mMask.fetch_or(mask, std::memory_order_release); }
  // Ends a transaction. Marks written while it ran were handled by the recall's own
  // IsMarked check; only those belonging to a mode change that is still pending (or
  // retrying) may reach the next transaction, so nothing is attributed to a later,
  // unrelated mode change.
  void EndTransaction(std::uint32_t retryKnobs, unsigned stillPendingTargets) noexcept
  {
    const std::uint32_t late = Take();
    Return(retryKnobs | (late & HostKnobMaskForTargets(stillPendingTargets)));
  }

private:
  std::atomic<std::uint32_t> mMask{0};
};
} // namespace volum
