#pragma once

// Fitting a factory art into a PLAY Dual Amp lane (pure). The arts are composed for
// a wide panel; a Dual lane is about square (309 x 312 at 900 x 600) and a few arts
// reach past its edges. Such an art measures its own extent and draws, static and
// moving alike, under LaneFit's transform (see LaneFitted in VoLumArtAnimator.h).
// Mono PLAY and BUILD are wider than kLaneFitMaxAspect and always get the identity.

#include <algorithm>

namespace volumart
{
struct ArtBox
{
  float L = 0.f, T = 0.f, R = 0.f, B = 0.f;

  float W() const { return R - L; }
  float H() const { return B - T; }
  bool Contains(const ArtBox& o) const { return o.L >= L && o.R <= R && o.T >= T && o.B <= B; }
  // Grows the box to take in a circle of radius rad at (x, y).
  void Add(float x, float y, float rad)
  {
    L = std::min(L, x - rad);
    R = std::max(R, x + rad);
    T = std::min(T, y - rad);
    B = std::max(B, y + rad);
  }
};

// x' = s x + tx, y' = s y + ty.
struct ArtLaneFit
{
  float s = 1.f, tx = 0.f, ty = 0.f;

  bool Identity() const { return s == 1.f && tx == 0.f && ty == 0.f; }
  float X(float x) const { return s * x + tx; }
  float Y(float y) const { return s * y + ty; }
  ArtBox Map(const ArtBox& b) const { return {X(b.L), Y(b.T), X(b.R), Y(b.B)}; }
  // The art-space region that lands on b (for the lane: what stays visible).
  ArtBox Unmap(const ArtBox& b) const { return {(b.L - tx) / s, (b.T - ty) / s, (b.R - tx) / s, (b.B - ty) / s}; }
};

// PLAY's Dual lane is ~1:1; BUILD's Dual lane art is ~1.36:1, mono PLAY and BUILD ~2.1:1.
inline constexpr float kLaneFitMaxAspect = 1.2f;
inline constexpr float kLaneFitMargin = 6.f;

inline bool IsNarrowLane(float w, float h)
{
  return w > 0.f && h > 0.f && w < kLaneFitMaxAspect * h;
}

// Shrinks (never enlarges) and moves `art`, the art's extent as drawn into `lane`, so it
// sits inside the lane less `margin`: centred across, and holding the point anchorY of
// its height (0 top, 1 bottom) where it was as far as the lane allows. Identity when the
// lane is not narrow or the art already fits.
inline ArtLaneFit LaneFit(const ArtBox& lane, const ArtBox& art, float margin, float anchorY)
{
  ArtLaneFit f;
  if (!IsNarrowLane(lane.W(), lane.H()) || art.W() <= 0.f || art.H() <= 0.f)
    return f;
  const ArtBox room{lane.L + margin, lane.T + margin, lane.R - margin, lane.B - margin};
  if (room.W() <= 0.f || room.H() <= 0.f || room.Contains(art))
    return f;
  f.s = std::min(1.f, std::min(room.W() / art.W(), room.H() / art.H()));
  f.tx = 0.5f * (room.L + room.R) - f.s * 0.5f * (art.L + art.R);
  const float ay = art.T + anchorY * art.H();
  f.ty = std::min(std::max(ay - f.s * ay, room.T - f.s * art.T), room.B - f.s * art.B);
  return f;
}
} // namespace volumart
