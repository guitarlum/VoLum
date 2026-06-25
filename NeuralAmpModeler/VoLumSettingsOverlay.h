#pragma once

// VoLum settings overlay chrome controls.
//
// - VoLumSettingsBackdropControl: full-window dim that frames the panel.
// - VoLumSettingsGroupFrameControl: thin rounded frame around a group of
//   related settings rows.
// - VoLumSettingsFooterSepControl / VoLumSettingsVertRuleControl: hairlines.
// - VoLumSettingsCloseControl: the X button at the top-right of the panel.
//
// Extracted from VoLumCoreControls.h on the 1.0 hygiene split.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

/** Full-window dim + explicit panel rect (must match layout math in NAMSettingsPageControl). */
class VoLumSettingsBackdropControl : public IControl
{
public:
  VoLumSettingsBackdropControl(const IRECT& fullBounds, const IRECT& panelRect)
  : IControl(fullBounds)
  , mPanel(panelRect)
  {
    // Must receive hits: if ignored, dim/panel â€œemptyâ€ pixels fall through to main UI and can steal
    // mouse up/down when the cursor moves quickly (settings appears to close at random).
    mIgnoreMouse = false;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    // Consume clicks on overlay shell (dim + filler); interactive children sit above in z-order.
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(185, 8, 10, 14), mRECT);
    const IRECT& p = mPanel;
    // Slightly lifted panel so it reads clearly over the dim layer
    DrawPanelDepth(g, p);
    g.DrawRect(VoLumColors::FRAME, p);
    g.DrawRect(IColor(90, 200, 180, 100), p.GetPadded(2.f));
    const float cs = 18.f;
    const float m = 8.f;
    DrawCornerAccent(g, p.L + m, p.T + m, cs, false, false);
    DrawCornerAccent(g, p.R - m, p.T + m, cs, true, false);
    DrawCornerAccent(g, p.L + m, p.B - m, cs, false, true);
    DrawCornerAccent(g, p.R - m, p.B - m, cs, true, true);
  }

private:
  IRECT mPanel;
};

/** Subtle frame behind grouped settings controls (ignores mouse so widgets on top still hit-test). */
class VoLumSettingsGroupFrameControl : public IControl
{
public:
  explicit VoLumSettingsGroupFrameControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    // Group card: lifted panel depth + gold border + inner hairline.
    DrawPanelDepth(g, mRECT);
    g.DrawRect(IColor(89, 200, 162, 78), mRECT);
    g.DrawRect(IColor(31, 200, 162, 78), mRECT.GetPadded(3.f));
  }
};

/** Thin horizontal rule above settings footer (mouse passes through). */
class VoLumSettingsFooterSepControl : public IControl
{
public:
  explicit VoLumSettingsFooterSepControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const IColor c(31, 200, 162, 78);
    g.FillRect(c, mRECT);
  }
};

/** Thin vertical rule between settings columns (mouse passes through). */
class VoLumSettingsVertRuleControl : public IControl
{
public:
  explicit VoLumSettingsVertRuleControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float t = mRECT.H() * 0.12f;
    const IColor mid(90, 200, 162, 78);
    const IColor end(35, 200, 162, 78);
    g.DrawLine(end, cx, mRECT.T, cx, mRECT.T + t, nullptr, 1.f);
    g.DrawLine(mid, cx, mRECT.T + t, cx, mRECT.B - t, nullptr, 1.f);
    g.DrawLine(end, cx, mRECT.B - t, cx, mRECT.B, nullptr, 1.f);
  }
};

/** Gold â€œÃ—â€ close control (no grey SVG) for the settings overlay. */
class VoLumSettingsCloseControl : public IControl
{
public:
  VoLumSettingsCloseControl(const IRECT& bounds, IActionFunction actionFunc)
  : IControl(bounds, kNoParameter, nullptr)
  , mCloseAction(std::move(actionFunc))
  {
  }

  void Draw(IGraphics& g) override
  {
    const IColor c = mMouseIsOver ? VoLumColors::GOLD : VoLumColors::GOLD_DIM;
    const float inset = 8.f;
    const float t = mMouseIsOver ? 2.25f : 1.75f;
    const float x0 = mRECT.L + inset;
    const float y0 = mRECT.T + inset;
    const float x1 = mRECT.R - inset;
    const float y1 = mRECT.B - inset;
    g.DrawLine(c, x0, y0, x1, y1, nullptr, t);
    g.DrawLine(c, x1, y0, x0, y1, nullptr, t);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    if (mCloseAction)
      mCloseAction(this);
  }

private:
  IActionFunction mCloseAction;
};

class VoLumSettingsShortcutInfoControl : public IControl
{
public:
  explicit VoLumSettingsShortcutInfoControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    // Match the three settings cards: lifted panel depth + gold border + inner hairline.
    DrawPanelDepth(g, mRECT);
    g.DrawRect(IColor(89, 200, 162, 78), mRECT);
    g.DrawRect(IColor(31, 200, 162, 78), mRECT.GetPadded(3.f));

    const IRECT inner = mRECT.GetPadded(-16.f, -8.f, -16.f, -8.f);
    const IText titleText(13.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Top);
    const IText keyText(11.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Top);
    const IText descText(10.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Top);
    const IText capText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top);

    g.DrawText(titleText, "Shortcut info", inner.GetFromTop(16.f));
    const IRECT body = inner.GetReducedFromTop(20.f);
    const float gap = 18.f;
    const float colW = (body.W() - 2.f * gap) / 3.f;
    const IRECT navCol(body.L, body.T, body.L + colW, body.B);
    const IRECT editCol(navCol.R + gap, body.T, navCol.R + gap + colW, body.B);
    const IRECT toolCol(editCol.R + gap, body.T, body.R, body.B);

    g.DrawLine(
      VoLumColors::FRAME.WithOpacity(0.55f), navCol.R + gap * 0.5f, body.T + 1.f, navCol.R + gap * 0.5f, body.B - 1.f);
    g.DrawLine(VoLumColors::FRAME.WithOpacity(0.55f), editCol.R + gap * 0.5f, body.T + 1.f, editCol.R + gap * 0.5f,
               body.B - 1.f);

    const float rowH = 12.f;
    auto drawPair = [&](const IRECT& col, int row, float keyW, const char* key, const char* desc) {
      const float y = col.T + 14.f + row * rowH;
      g.DrawText(keyText, key, IRECT(col.L, y, col.L + keyW, y + rowH));
      g.DrawText(descText, desc, IRECT(col.L + keyW + 5.f, y, col.R, y + rowH));
    };

    // One key-column width per section (widest key in that column) so every
    // description in a column starts at the same x. Previously each row used its
    // own keyW, so Navigate/Edit descriptions stair-stepped while Tools (all 16)
    // happened to line up.
    const float navKeyW = 42.f; // "Arrows"
    const float editKeyW = 34.f; // "Enter" / "Space"
    const float toolKeyW = 16.f; // single letters

    g.DrawText(capText, "Navigate", IRECT(navCol.L, navCol.T, navCol.R, navCol.T + 12.f));
    drawPair(navCol, 0, navKeyW, "1/2/3", "PRE / AMP / POST");
    drawPair(navCol, 1, navKeyW, "Tab", "target focus");
    drawPair(navCol, 2, navKeyW, "Arrows", "amp / channel");

    g.DrawText(capText, "Edit", IRECT(editCol.L, editCol.T, editCol.R, editCol.T + 12.f));
    drawPair(editCol, 0, editKeyW, "Enter", "edit");
    drawPair(editCol, 1, editKeyW, "Space", "toggle");
    drawPair(editCol, 2, editKeyW, "S", "cab");
    drawPair(editCol, 3, editKeyW, "Esc", "close");

    g.DrawText(capText, "Tools", IRECT(toolCol.L, toolCol.T, toolCol.R, toolCol.T + 12.f));
    drawPair(toolCol, 0, toolKeyW, "T", "tuner");
    drawPair(toolCol, 1, toolKeyW, "M", "metronome");
    drawPair(toolCol, 2, toolKeyW, "H", "settings");
  }
};