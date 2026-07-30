#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace volum
{

// Semantic version from plug state chunk header (e.g. "0.7.15", "0.1.0").
class ChunkVersion
{
public:
  ChunkVersion(int major, int minor, int patch)
  : mMajor(major)
  , mMinor(minor)
  , mPatch(patch)
  {
  }

  explicit ChunkVersion(const std::string& versionStr)
  {
    std::istringstream stream(versionStr);
    std::string token;
    std::vector<int> parts;

    while (std::getline(stream, token, '.'))
      parts.push_back(std::stoi(token));

    if (parts.size() != 3)
      throw std::invalid_argument("Version string must have exactly 3 dot-separated segments");

    mMajor = parts[0];
    mMinor = parts[1];
    mPatch = parts[2];
  }

  bool operator>=(const ChunkVersion& other) const
  {
    if (GetMajor() > other.GetMajor())
      return true;
    if (GetMajor() < other.GetMajor())
      return false;
    if (GetMinor() > other.GetMinor())
      return true;
    if (GetMinor() < other.GetMinor())
      return false;
    return GetPatch() >= other.GetPatch();
  }

  int GetMajor() const { return mMajor; }
  int GetMinor() const { return mMinor; }
  int GetPatch() const { return mPatch; }

private:
  int mMajor = 0;
  int mMinor = 0;
  int mPatch = 0;
};

// Parses a chunk version without throwing.
//
// The ChunkVersion(std::string) constructor above uses std::stoi and throws on
// anything it cannot parse. That is fine for tests and for strings we produced
// ourselves, but the state chunk is attacker- and corruption-controlled data
// arriving through UnserializeState, which hosts call across a C++ ABI boundary
// with no exception barrier (see IPlugVST3_Common.h). A project file carrying a
// valid VoLum header and a version of "", "1.2", "1.x.0" or a 20-digit number
// therefore took the host down instead of being rejected.
//
// Stricter than stoi on purpose: stoi accepted "1.2.3junk" and negative
// components. Every version VoLum or NAM has ever serialized is digits and dots
// (PLUG_VERSION_STR, e.g. "1.2.1"; oldest NAM chunks, e.g. "0.7.15"), so
// requiring exactly that costs no real chunk and rejects garbage earlier.
inline bool TryParseChunkVersion(const std::string& versionStr, ChunkVersion& out)
{
  // Any real component is tiny; the cap keeps accumulation far from overflow.
  constexpr long long kMaxComponent = 1000000;

  int parts[3] = {0, 0, 0};
  int partCount = 0;
  size_t i = 0;

  for (;;)
  {
    if (partCount == 3)
      return false; // a fourth segment

    long long value = 0;
    size_t digits = 0;
    while (i < versionStr.size() && versionStr[i] >= '0' && versionStr[i] <= '9')
    {
      value = value * 10 + (versionStr[i] - '0');
      if (value > kMaxComponent)
        return false;
      ++digits;
      ++i;
    }

    if (digits == 0)
      return false; // empty string, empty segment, or a non-digit lead

    parts[partCount++] = static_cast<int>(value);

    if (i == versionStr.size())
      break;
    if (versionStr[i] != '.')
      return false; // trailing junk
    ++i;
  }

  if (partCount != 3)
    return false;

  out = ChunkVersion(parts[0], parts[1], parts[2]);
  return true;
}

inline bool ChunkUses0500SerializedConfig(const ChunkVersion& version)
{
  return version >= ChunkVersion(0, 5, 0) && !(version >= ChunkVersion(0, 6, 0));
}

inline bool ChunkUses0700SerializedConfig(const ChunkVersion& version)
{
  return version >= ChunkVersion(0, 7, 0) && !(version >= ChunkVersion(0, 7, 9));
}

inline bool ChunkUses0600SerializedConfig(const ChunkVersion& version)
{
  return version >= ChunkVersion(0, 6, 0) && !(version >= ChunkVersion(0, 7, 0));
}

// VoLum 0.1.x-0.4.x uses the same serialized param layout as NAM 0.7.15.
inline bool ChunkUses0715SerializedConfig(const ChunkVersion& version)
{
  const bool isVolum01to04 = version >= ChunkVersion(0, 1, 0) && !(version >= ChunkVersion(0, 5, 0));
  return version >= ChunkVersion(0, 7, 15) || isVolum01to04;
}

inline bool ShouldRemapOktaverbSubModeForChunkVersion(const ChunkVersion& version)
{
  return !(version >= ChunkVersion(0, 9, 1));
}

// 0.9.3: reverb Mix law switched from additive to equal-power crossfade and Hall /
// Plate gained a wet-bus trim. Stored ReverbMix on pre-0.9.3 chunks must be remapped
// through RemapReverbMixToEqualPowerV0_9_3 to preserve perceived wet level.
inline bool ShouldRemapReverbMixForChunkVersion(const ChunkVersion& version)
{
  return !(version >= ChunkVersion(0, 9, 3));
}

} // namespace volum
