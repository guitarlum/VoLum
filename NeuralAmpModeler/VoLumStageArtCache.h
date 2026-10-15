#pragma once

// Cache key for a PLAY stage art layer. A layer is a bitmap of one rect, so it
// is only reusable while the art and its pixel size both match: the panel
// changes width between mono and dual, and a bitmap built for one width is
// stretched when blitted into the other.

#include <cmath>

namespace volum
{

struct StageArtKey
{
  int art = -1;
  bool custom = false;
  int pixelW = 0;
  int pixelH = 0;
};

inline bool operator==(const StageArtKey& a, const StageArtKey& b)
{
  return a.art == b.art && a.custom == b.custom && a.pixelW == b.pixelW && a.pixelH == b.pixelH;
}

inline bool operator!=(const StageArtKey& a, const StageArtKey& b)
{
  return !(a == b);
}

// Same rounding as IGraphics::StartLayer: ceil(scale * ceil(logical)), on the
// pixel-aligned rect.
inline StageArtKey MakeStageArtKey(int art, bool custom, float alignedW, float alignedH, float backingScale)
{
  StageArtKey key;
  key.art = art;
  key.custom = custom;
  key.pixelW = static_cast<int>(std::ceil(backingScale * std::ceil(alignedW)));
  key.pixelH = static_cast<int>(std::ceil(backingScale * std::ceil(alignedH)));
  return key;
}

// An empty cache (no layer built yet) never matches.
inline bool StageArtLayerMatches(const StageArtKey& cached, const StageArtKey& wanted)
{
  return cached.pixelW > 0 && cached.pixelH > 0 && cached == wanted;
}

} // namespace volum
