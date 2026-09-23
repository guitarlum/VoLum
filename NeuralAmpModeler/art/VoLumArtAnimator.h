#pragma once

// PLAY art animation API. One animator per stage panel, created by
// art/VoLumArtDispatch.h only for arts whose registry flag is on
// (art/VoLumArtRegistry.h), owned and driven by VoLumPlaySurfaceControl.
//
// Contract for an animator:
//  - Prepare(g, owner, r) runs with no clip and no transform set. Build every
//    cached layer here (StartLayer / EndLayer), once. Return false to be called
//    again next frame for optional extras; Draw must cope while they are missing.
//  - Draw(g, r, m) draws the whole art for this frame. The clip is already r.
//    Never StartLayer / ResumeLayer here, never recompute a fractal here.
//    Deterministic: a pure function of (r, m) and what Prepare cached.
//  - Rest rule: m.IsRest() => the output equals Draw<Amp>Hero(g, r) within 2/255.
//  - m.bloom is PLAY's glow lift: draw your base with ArtLayer::DrawLit so it
//    brightens with the signal like every static art does.
//  - EBlend::Add is only valid here in Draw (opaque framebuffer). Inside a layer
//    it adds onto transparent black and loses the colour.
//  - DropLayers() forgets every layer; the next Prepare rebuilds them.

#include "VoLumArtCommon.h"
#include "VoLumArtMotion.h"

#include <memory>

namespace volumart
{
struct ArtLayer
{
  ILayerPtr layer;

  bool Ok(IGraphics& g) const { return layer && g.CheckLayer(layer); }

  template <class Paint>
  void Build(IGraphics& g, IControl* owner, const IRECT& r, Paint&& paint)
  {
    g.StartLayer(owner, r);
    paint();
    layer = g.EndLayer();
  }

  // w >= 1 is the exact 1:1 blit the static PLAY art uses.
  void Draw(IGraphics& g, const IRECT& r, float w = 1.f) const
  {
    if (!layer || w <= 0.f)
      return;
    if (w >= 1.f)
    {
      g.DrawBitmap(layer->GetBitmap(), r, 0, 0, nullptr);
      return;
    }
    const IBlend blend(EBlend::Default, w);
    g.DrawBitmap(layer->GetBitmap(), r, 0, 0, &blend);
  }

  // Adds src * w^2 (NanoVG's Add is alpha-weighted twice).
  void DrawAdd(IGraphics& g, const IRECT& r, float w) const
  {
    if (!layer || w <= 0.f)
      return;
    const IBlend add(EBlend::Add, w);
    g.DrawBitmap(layer->GetBitmap(), r, 0, 0, &add);
  }

  // The base art as PLAY lights it: exact blit, plus the bloom when sounding.
  void DrawLit(IGraphics& g, const IRECT& r, float bloom) const
  {
    Draw(g, r);
    DrawAdd(g, r, bloom);
  }

  // The layer under an affine matrix (see AboutPoint). An identity matrix at
  // w >= 1 is Draw(), never a resampled blit.
  void DrawXform(IGraphics& g, const IRECT& r, const IMatrix& m, float w = 1.f, bool additive = false) const
  {
    if (!layer || w <= 0.f)
      return;
    const bool identity = m.mXX == 1.0 && m.mYX == 0.0 && m.mXY == 0.0 && m.mYY == 1.0 && m.mTX == 0.0 && m.mTY == 0.0;
    if (identity && !additive)
    {
      Draw(g, r, w);
      return;
    }
    const IBlend blend(additive ? EBlend::Add : EBlend::Default, w);
    g.PathTransformSave();
    g.PathTransformMatrix(m);
    g.DrawBitmap(layer->GetBitmap(), r, 0, 0, &blend);
    g.PathTransformRestore();
  }

  void Reset() { layer = nullptr; }
};

class ArtAnimator
{
public:
  virtual ~ArtAnimator() = default;
  virtual bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) = 0;
  virtual void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) = 0;
  virtual void DropLayers() = 0;
};

// Class A: the whole static art in one layer, motion drawn on top of it.
// Subclasses implement DrawExtras (skipped at rest) and optionally PrepareExtras
// (geometry only; it runs after the layer is built, still with no clip).
template <void (*kDrawHero)(IGraphics&, const IRECT&)>
class AdditiveArtAnimator : public ArtAnimator
{
public:
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    // A local copy: MSVC cannot call a function-pointer template argument from a lambda.
    void (*drawHero)(IGraphics&, const IRECT&) = kDrawHero;
    if (!mFull.Ok(g))
      mFull.Build(g, owner, r, [&g, &r, drawHero] { drawHero(g, r); });
    PrepareExtras(r);
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    if (m.IsRest())
    {
      mFull.DrawLit(g, r, m.bloom);
      return;
    }
    const float keep = 1.f - std::clamp(BaseDim(m), 0.f, 1.f);
    mFull.Draw(g, r, keep);
    mFull.DrawAdd(g, r, m.bloom * keep);
    DrawExtras(g, r, m);
  }

  void DropLayers() override { mFull.Reset(); }

protected:
  virtual void PrepareExtras(const IRECT& r) { (void)r; }
  // How far the static art steps back (0..1) while the extras play; 0 at rest.
  virtual float BaseDim(const ArtMotion& m) const
  {
    (void)m;
    return 0.f;
  }
  virtual void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) = 0;

  ArtLayer mFull;
};

// ---- Cookbook: cheap per-frame primitives -----------------------------------
// One path, one draw call per colour: use these for anything live. Never use
// them inside Draw<Amp>Hero or a rest path (batched output differs from
// individual DrawLine calls where strokes overlap).

inline IStrokeOptions RoundStroke()
{
  IStrokeOptions o;
  o.mCapOption = ELineCap::Round;
  o.mJoinOption = ELineJoin::Round;
  return o;
}

// nSegs independent segments, xyxy = {x0, y0, x1, y1, ...}.
inline void BatchLines(IGraphics& g, const IColor& c, float width, const float* xyxy, int nSegs,
                       const IBlend* blend = nullptr)
{
  if (nSegs <= 0)
    return;
  g.PathClear();
  for (int i = 0; i < nSegs; ++i)
  {
    g.PathMoveTo(xyxy[4 * i], xyxy[4 * i + 1]);
    g.PathLineTo(xyxy[4 * i + 2], xyxy[4 * i + 3]);
  }
  g.PathStroke(c, width, RoundStroke(), blend);
}

// One open polyline through n points, xy = {x0, y0, x1, y1, ...}.
inline void BatchPolyline(IGraphics& g, const IColor& c, float width, const float* xy, int n,
                          const IBlend* blend = nullptr)
{
  if (n < 2)
    return;
  g.PathClear();
  g.PathMoveTo(xy[0], xy[1]);
  for (int i = 1; i < n; ++i)
    g.PathLineTo(xy[2 * i], xy[2 * i + 1]);
  g.PathStroke(c, width, RoundStroke(), blend);
}

// n filled circles, xyr = {x, y, r, ...}, one fill.
inline void BatchDots(IGraphics& g, const IColor& c, const float* xyr, int n, const IBlend* blend = nullptr)
{
  if (n <= 0)
    return;
  g.PathClear();
  for (int i = 0; i < n; ++i)
    g.PathCircle(xyr[3 * i], xyr[3 * i + 1], xyr[3 * i + 2]);
  g.PathFill(c, IFillOptions(), blend);
}

// GlowDot's look (two halos under a core), added at weight w.
inline void GlowDotAdd(IGraphics& g, const IColor& glowCol, const IColor& core, float x, float y, float r, float glowR,
                       float w)
{
  if (w <= 0.f)
    return;
  const IBlend add(EBlend::Add, std::min(w, 1.f));
  g.FillCircle(WithA(glowCol, 0.26f), x, y, r + glowR, &add);
  g.FillCircle(WithA(glowCol, 0.45f), x, y, r + glowR * 0.5f, &add);
  g.FillCircle(core, x, y, r, &add);
}

// Bloom's soft radial light, added at weight w (light that shines, never shades).
inline void BloomAdd(IGraphics& g, float cx, float cy, float R, const IColor& c, float peak, float w = 1.f)
{
  if (R <= 0.f || peak <= 0.f || w <= 0.f)
    return;
  const IBlend add(EBlend::Add, std::min(w, 1.f));
  g.PathCircle(cx, cy, R);
  g.PathFill(IPattern::CreateRadialGradient(
               cx, cy, R, {{WithA(c, peak), 0.f}, {WithA(c, peak * 0.3f), 0.6f}, {WithA(c, 0.f), 1.f}}),
             IFillOptions(), &add);
}

// Rotate (degrees), scale and x-skew (degrees) about (px, py).
inline IMatrix AboutPoint(float px, float py, float rotDeg, float sx = 1.f, float sy = 1.f, float skewXDeg = 0.f)
{
  IMatrix m;
  m.Translate(px, py);
  if (rotDeg != 0.f)
    m.Rotate(rotDeg);
  if (skewXDeg != 0.f)
    m.Skew(skewXDeg, 0.f);
  if (sx != 1.f || sy != 1.f)
    m.Scale(sx, sy);
  m.Translate(-px, -py);
  return m;
}
} // namespace volumart
