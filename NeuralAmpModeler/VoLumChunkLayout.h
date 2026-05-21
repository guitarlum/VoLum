#pragma once

#include <cstddef>

namespace volum
{

// Binary chunk sizes for the VoLum per-amp block appended after serialized params.
// These are byte-layout contracts: changing them means old DAW sessions need a
// new explicit reader branch in Unserialization.cpp.
static constexpr int kPerAmpSettingsHeaderBytes = static_cast<int>(sizeof(int) * 3);
static constexpr int kLegacyPerAmpSettingsBytes = static_cast<int>(sizeof(int) * 4 + sizeof(double) * 6);
static constexpr int kPrePedalPerAmpSettingsBytes = static_cast<int>(sizeof(int) * 9 + sizeof(double) * 24);
static constexpr int kDualAmpPerAmpSettingsBytesV1 = static_cast<int>(sizeof(int) * 16 + sizeof(double) * 32);
static constexpr int kDualAmpPerAmpSettingsBytes = static_cast<int>(sizeof(int) * 17 + sizeof(double) * 32);
// POST per-amp live values appended after the dual-amp tail. Layout:
//   ints (7):   postValid, postDelayActive, postDelayMode, postDelayPingPong,
//               postReverbActive, postReverbMode, postReverbSubMode
//   doubles(10): postDelayTime, postDelayFeedback, postDelayMix, postDelayTone, postDelayAge,
//               postReverbMix, postReverbDecay, postReverbTone, postReverbPreDelay, postReverbShimmer
static constexpr int kPostPerAmpLiveSettingsBytes = static_cast<int>(sizeof(int) * 7 + sizeof(double) * 10);
// POST per-mode snapshots appended after the live POST values:
//   delay modes: 3 * (time, feedback, mix, tone, age, pingPong)
//   reverb modes: 3 * (mix, decay, tone, preDelay, shimmer, subMode)
//   Oktaverb sub-modes: 3 * (mix, decay, tone, preDelay, shimmer)
static constexpr int kPostPerAmpSnapshotSettingsBytes = static_cast<int>(sizeof(int) * 6 + sizeof(double) * 45);
static constexpr int kPostPerAmpSettingsBytes = kPostPerAmpLiveSettingsBytes + kPostPerAmpSnapshotSettingsBytes;
static constexpr int kDualAmpPlusPostLivePerAmpSettingsBytes = kDualAmpPerAmpSettingsBytes + kPostPerAmpLiveSettingsBytes;
static constexpr int kDualAmpPlusPostPerAmpSettingsBytes = kDualAmpPerAmpSettingsBytes + kPostPerAmpSettingsBytes;
static constexpr int kCurrentPerAmpSettingsBytes = kDualAmpPlusPostPerAmpSettingsBytes;
// Global PRE/POST lock flags appended after the per-amp array (VoLum 1.0.2+).
static constexpr int kPrePostLockFlagsBytes = static_cast<int>(sizeof(int) * 2);

inline int LegacyPerAmpSettingsPayloadBytes(int ampCount)
{
  return kLegacyPerAmpSettingsBytes * ampCount;
}

inline int CurrentPerAmpSettingsPayloadBytes(int ampCount)
{
  return kCurrentPerAmpSettingsBytes * ampCount;
}

inline bool ChunkHasExtendedPerAmpSettings(int remainingBytes, int ampCount)
{
  return remainingBytes > LegacyPerAmpSettingsPayloadBytes(ampCount);
}

inline bool ChunkHasDualAmpPerAmpSettings(int remainingBytes, int ampCount)
{
  return remainingBytes >= kDualAmpPerAmpSettingsBytesV1 * ampCount;
}

inline bool ChunkHasSupportPolarityPerAmpSettings(int remainingBytes, int ampCount)
{
  return remainingBytes >= kDualAmpPerAmpSettingsBytes * ampCount;
}

inline bool ChunkHasPostPerAmpSettings(int remainingBytes, int ampCount)
{
  return remainingBytes >= kDualAmpPlusPostLivePerAmpSettingsBytes * ampCount;
}

inline bool ChunkHasPostSnapshotPerAmpSettings(int remainingBytes, int ampCount)
{
  return remainingBytes >= kDualAmpPlusPostPerAmpSettingsBytes * ampCount;
}

inline bool ChunkHasPrePostLockFlags(int remainingBytes, int ampCount)
{
  return remainingBytes >= CurrentPerAmpSettingsPayloadBytes(ampCount) + kPrePostLockFlagsBytes;
}

} // namespace volum
