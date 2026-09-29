#pragma once

// Generic PLAY motion for custom-amp arts (all six DrawCustomAmpArt styles).
// Every style is centred and stays inside the inscribed circle (spirograph peak
// 1.1 * 0.42 * min(w, h)), so the whole art can turn about its centre without
// clipping. Class C: while sounding the art turns slowly (neighbouring styles in
// opposite directions) and breathes with the level; a pick flashes it.
// DrawCustomAmpArt lives in VoLumFractalArt.h, which art files may not include,
// so the caller hands the paint in.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

#include <functional>

namespace volumart::art
{
using CustomArtPaint = std::function<void(IGraphics&, const IRECT&)>;

class CustomArtAnimator final : public ArtAnimator
{
public:
  CustomArtAnimator(int style, CustomArtPaint paint)
  : mDir((((style % 6) + 6) % 6) % 2 == 0 ? 1.f : -1.f)
  , mPaint(std::move(paint))
  {
  }

  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    if (!mFull.Ok(g))
      mFull.Build(g, owner, r, [&] { mPaint(g, r); });
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    const float wake = m.Wake();
    if (m.IsRest() || wake <= 0.f)
    {
      mFull.DrawLit(g, r, m.bloom);
      return;
    }
    const float t = static_cast<float>(m.clock);
    const float turn = mDir * 6.f * t;
    const float s = 1.f + 0.012f * m.energy * std::sin(1.6f * t) + 0.03f * m.pick;
    const IMatrix live = AboutPoint(r.MW(), r.MH(), turn, s, s);
    mFull.Draw(g, r, 1.f - wake);
    mFull.DrawXform(g, r, live, wake);
    mFull.DrawXform(g, r, live, std::min(1.f, m.bloom + 0.25f * m.pick), true);
  }

  void DropLayers() override { mFull.Reset(); }

private:
  float mDir;
  CustomArtPaint mPaint;
  ArtLayer mFull;
};
} // namespace volumart::art
