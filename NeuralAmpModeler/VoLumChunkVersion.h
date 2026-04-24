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

inline bool ChunkUses0500SerializedConfig(const ChunkVersion& version)
{
  return version >= ChunkVersion(0, 5, 0) && !(version >= ChunkVersion(0, 7, 0));
}

// VoLum 0.1.x-0.4.x uses the same serialized param layout as NAM 0.7.15.
inline bool ChunkUses0715SerializedConfig(const ChunkVersion& version)
{
  const bool isVolum01to04 = version >= ChunkVersion(0, 1, 0) && !(version >= ChunkVersion(0, 5, 0));
  return version >= ChunkVersion(0, 7, 15) || isVolum01to04;
}

} // namespace volum
