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
static constexpr int kDualAmpPerAmpSettingsBytes = static_cast<int>(sizeof(int) * 16 + sizeof(double) * 32);
static constexpr int kCurrentPerAmpSettingsBytes = kDualAmpPerAmpSettingsBytes;

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
  return remainingBytes >= kDualAmpPerAmpSettingsBytes * ampCount;
}

} // namespace volum
