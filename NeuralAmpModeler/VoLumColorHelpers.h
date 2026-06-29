#pragma once

#include "IControls.h"
#include "ITextEntryControl.h"
#include "VoLumAmpeteCatalog.h"
#include <cmath>
#include <functional>
#include <algorithm>
#include <string>
#include <vector>

using namespace iplug;
using namespace igraphics;

inline void ClearVoLumKnobSelection(IControl* control)
{
  if (!control)
    return;

  if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(control->GetDelegate()))
    plugin->_ClearVoLumKnobSelection();
}

// Art Deco Noir palette
namespace VoLumColors
{
const IColor BG(255, 17, 17, 24);
const IColor SIDEBAR_BG(255, 17, 17, 24);
const IColor SIDEBAR_BG2(255, 12, 12, 20);
const IColor SIDEBAR_BORDER(50, 200, 162, 78);
const IColor ITEM_HOVER(10, 200, 162, 78);
const IColor ITEM_SEL_BG(18, 200, 162, 78);
const IColor ITEM_SEL_BORDER(51, 200, 162, 78);
// High-contrast body copy on charcoal (avoid greyed-out look on poor panels)
const IColor TEXT_DIM(255, 232, 218, 200);
const IColor TEXT_MED(255, 245, 232, 218);
const IColor TEXT_BRIGHT(255, 255, 248, 238);
const IColor GOLD(255, 252, 222, 145);
const IColor GOLD_DIM(255, 235, 210, 145);
const IColor METER_GREEN(255, 42, 138, 42);
const IColor DIVIDER(30, 200, 162, 78);
const IColor FRAME(72, 200, 162, 78);
const IColor CORNER(255, 200, 162, 78);
const IColor BTN_OFF_BG(5, 200, 162, 78);
const IColor BTN_OFF_BORDER(40, 200, 162, 78);
const IColor BTN_OFF_TEXT(255, 244, 228, 210);
const IColor BTN_CAB_ON_BG(72, 115, 88, 52);
const IColor BTN_CAB_ON_BORDER(235, 225, 195, 115);
const IColor BTN_AMP_ON_BG(78, 48, 125, 118);
const IColor BTN_AMP_ON_BORDER(215, 165, 230, 220);
const IColor BTN_AMP_ON_TEXT(255, 252, 255, 248);
const IColor HERO_BG(255, 12, 12, 18);
const IColor HERO_BORDER(50, 200, 162, 78);
const IColor HERO_CORNER(102, 200, 162, 78);

const IColor TEAL(255, 91, 196, 196);
const IColor TEAL_DIM(255, 75, 162, 162);
const IColor AMBER(255, 232, 168, 92);
const IColor CREAM(255, 237, 227, 208);
const IColor CREAM_DIM(255, 166, 149, 124);

// Custom-amp procedural art palette: the cyan/turquoise identity used for the
// hero, sidebar thumbnails, and builder art picker alike. Selection is conveyed
// by a brighter version of THIS hue (plus the gold border/badge), never a hue
// shift - the old picker mixed gold + periwinkle on select and read as "off".
const IColor CUSTOM_ART_BRIGHT(220, 120, 210, 220);
const IColor CUSTOM_ART_DIM(205, 100, 178, 208);

// Custom IR active cab highlight: a warm copper accent, distinct from the teal
// "No Cab" (BTN_AMP_ON) and the gold stock cabs (BTN_CAB_ON) while staying in
// the warm half of the palette.
const IColor BTN_IR_ON_BG(82, 196, 122, 80);
const IColor BTN_IR_ON_BORDER(235, 226, 156, 112);
const IColor BTN_IR_ON_TEXT(255, 255, 236, 214);
} // namespace VoLumColors

// Helper: draw L-shaped corner accent (2 lines)
inline void DrawCornerAccent(IGraphics& g, float x, float y, float size, bool flipH, bool flipV,
                             const IColor& col = VoLumColors::CORNER)
{
  float dx = flipH ? -size : size;
  float dy = flipV ? -size : size;
  g.DrawLine(col, x, y, x + dx, y);
  g.DrawLine(col, x, y, x, y + dy);
}

// Helper: draw a small diamond (rotated square)
inline void DrawDiamond(IGraphics& g, float cx, float cy, float halfSize, const IColor& col)
{
  g.DrawLine(col, cx, cy - halfSize, cx + halfSize, cy);
  g.DrawLine(col, cx + halfSize, cy, cx, cy + halfSize);
  g.DrawLine(col, cx, cy + halfSize, cx - halfSize, cy);
  g.DrawLine(col, cx - halfSize, cy, cx, cy - halfSize);
}

// ===========================================================================
// VoLum 1.2.0 UI modernization design tokens
// ---------------------------------------------------------------------------
// Codified spacing/type scale, a single selection-state language, and reusable
// depth-drawing helpers so views stop hand-rolling sizes/colours and large dark
// regions gain tactile depth. Identity hues (TEAL = SUPPORT lane, copper =
// BYO/custom content) stay as secondary tints; selection is ALWAYS the brass
// treatment below.
// ===========================================================================
namespace VoLumColors
{
// One selection treatment marks the active item in any mutually-exclusive group
// (cab row, sidebar, art picker, mode lists, steppers).
const IColor SEL_BG(40, 200, 162, 78);        // brass wash behind the active item
const IColor SEL_BG_SOFT(20, 200, 162, 78);   // hover wash (telegraph the target)
const IColor SEL_BORDER(235, 226, 156, 112);  // bright brass outline
const IColor SEL_GLOW(64, 252, 222, 145);     // soft brass glow under selection
const IColor SEL_TEXT(255, 255, 244, 224);    // cream text on selected

// Destructive-action accent (delete/remove) - distinct from amber caution.
const IColor DANGER(255, 228, 92, 80);      // red outline / label
const IColor DANGER_FILL(78, 228, 92, 80);  // red button wash
const IColor DANGER_GLOW(54, 228, 92, 80);  // red modal glow

// Panel / well depth.
const IColor PANEL_TOP(255, 23, 23, 31);   // panel gradient top (slightly lifted)
const IColor PANEL_BOT(255, 13, 13, 19);   // panel gradient bottom
const IColor WELL_DARK(255, 9, 9, 14);     // recessed well base
const IColor INNER_SHADOW(120, 0, 0, 0);   // inset top shadow
const IColor RIM_LIGHT(26, 255, 248, 238); // faint top inner highlight
} // namespace VoLumColors

namespace VoLumMetrics
{
// Spacing scale (px in the design's logical units).
constexpr float kXs = 4.f;
constexpr float kSm = 8.f;
constexpr float kMd = 12.f;
constexpr float kLg = 18.f;
constexpr float kXl = 28.f;
// Corner radii.
constexpr float kRadius = 4.f;
constexpr float kRadiusLg = 8.f;
} // namespace VoLumMetrics

// Type scale: one display face for hero/brand, a bold caption/label face, a wide
// techy face for numeric readouts, and a calm body face. Use these instead of
// constructing IText inline so weight/face hierarchy stays consistent.
namespace VoLumType
{
inline IText Display(float size, const IColor& c, EAlign a = EAlign::Center)
{
  return IText(size, c, "Poiret-One", a, EVAlign::Middle);
}
inline IText Caption(float size, const IColor& c, EAlign a = EAlign::Center)
{
  return IText(size, c, "Josefin-Bold", a, EVAlign::Middle);
}
inline IText Label(float size, const IColor& c, EAlign a = EAlign::Center)
{
  return IText(size, c, "Josefin-Bold", a, EVAlign::Middle);
}
inline IText Value(float size, const IColor& c, EAlign a = EAlign::Center)
{
  return IText(size, c, "Michroma-Regular", a, EVAlign::Middle);
}
inline IText Body(float size, const IColor& c, EAlign a = EAlign::Center)
{
  return IText(size, c, "Josefin-Sans", a, EVAlign::Middle);
}
} // namespace VoLumType

// Fill a rect with a vertical gradient.
inline void FillVGradient(IGraphics& g, const IRECT& r, const IColor& top, const IColor& bot)
{
  g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, {{top, 0.f}, {bot, 1.f}}));
}

// Panel: subtle top-lit vertical gradient + 1px top inner highlight + bottom shadow line.
inline void DrawPanelDepth(IGraphics& g, const IRECT& r, float roundness = 0.f)
{
  if (roundness > 0.f)
    g.PathRoundRect(r, roundness);
  else
    g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B,
                                            {{VoLumColors::PANEL_TOP, 0.f}, {VoLumColors::PANEL_BOT, 1.f}}));
  g.DrawLine(VoLumColors::RIM_LIGHT, r.L + 1.5f, r.T + 1.f, r.R - 1.5f, r.T + 1.f);
  g.DrawLine(IColor(64, 0, 0, 0), r.L + 1.5f, r.B - 1.f, r.R - 1.5f, r.B - 1.f);
}

// Recessed rectangular well (inputs, list backgrounds, readouts).
inline void DrawInsetWell(IGraphics& g, const IRECT& r, float roundness = 3.f)
{
  g.FillRoundRect(VoLumColors::WELL_DARK, r, roundness);
  const IRECT top = IRECT(r.L, r.T, r.R, r.T + r.H() * 0.5f);
  g.PathRoundRect(top, roundness, roundness, 0.f, 0.f);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, top.B,
                                            {{VoLumColors::INNER_SHADOW, 0.f}, {COLOR_TRANSPARENT, 1.f}}));
  g.DrawLine(IColor(20, 255, 248, 238), r.L + 1.f, r.B - 1.f, r.R - 1.f, r.B - 1.f);
}

// Circular recessed well behind a knob body (top-shadowed, gently lifted centre).
inline void DrawKnobWell(IGraphics& g, float cx, float cy, float radius)
{
  g.FillCircle(IColor(120, 0, 0, 0), cx, cy + 1.5f, radius + 2.5f);
  g.PathCircle(cx, cy, radius);
  g.PathFill(IPattern::CreateRadialGradient(cx, cy - radius * 0.35f, radius * 1.25f,
                                            {{IColor(255, 24, 24, 32), 0.f}, {IColor(255, 9, 9, 14), 1.f}}));
  g.PathCircle(cx, cy, radius);
  g.PathFill(IPattern::CreateLinearGradient(cx, cy - radius, cx, cy + radius * 0.2f,
                                            {{IColor(110, 0, 0, 0), 0.f}, {COLOR_TRANSPARENT, 1.f}}));
}

// Soft radial glow disc (selection underlays, hero frame breathing room).
inline void DrawSoftGlowCircle(IGraphics& g, float cx, float cy, float radius, const IColor& col)
{
  g.PathCircle(cx, cy, radius);
  g.PathFill(IPattern::CreateRadialGradient(cx, cy, radius, {{col, 0.f}, {COLOR_TRANSPARENT, 1.f}}));
}

// ===========================================================================
// Selection language (single source of truth)
// ---------------------------------------------------------------------------
// VoLum has three historical selection dialects: brass (SEL_*, cab row /
// metronome), list-teal (ITEM_SEL_*, sidebar + menus), and amber (mode pickers
// and sub-mode pills). Each was hand-drawn at every call site, so a new
// mutually-exclusive control could ship with a missing or mismatched highlight.
// DrawVoLumSelection draws the active/hover background chrome for ONE item in a
// group given an explicit style, so the look is defined once. Text colour stays
// the caller's responsibility (it depends on content); SelectionInkColor()
// returns the matching on-selection text colour per style.
// ===========================================================================
enum class VoLumSelectionStyle
{
  Brass,       // SEL_* brass wash + outline (cab row, time-sig grid)
  ListTeal,    // ITEM_SEL_* teal wash + outline (sidebar list, menus)
  AmberPicker, // solid amber fill (mode pickers + sub-mode pills)
};

// Draw the selection/hover background for one item. `roundness <= 0` draws square
// corners (matches the mode picker); `inset` pads the fill in from the item rect.
inline void DrawVoLumSelection(IGraphics& g, const IRECT& item, bool active, bool hovered,
                               VoLumSelectionStyle style, float roundness = 3.f, float inset = 1.f)
{
  const IRECT fill = item.GetPadded(-inset);
  const IColor amberHover(48, 226, 165, 78); // soft amber wash to telegraph the target

  switch (style)
  {
    case VoLumSelectionStyle::AmberPicker:
      if (active)
      {
        if (roundness > 0.f)
          g.FillRoundRect(VoLumColors::AMBER, fill, roundness);
        else
          g.FillRect(VoLumColors::AMBER, fill);
      }
      else if (hovered)
      {
        if (roundness > 0.f)
          g.FillRoundRect(amberHover, fill, roundness);
        else
          g.FillRect(amberHover, fill);
      }
      break;

    case VoLumSelectionStyle::ListTeal:
      if (active)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, fill, roundness);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, fill, roundness);
      }
      else if (hovered)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, fill, roundness);
      }
      break;

    case VoLumSelectionStyle::Brass:
      if (active)
      {
        g.FillRoundRect(VoLumColors::SEL_BG, fill, roundness);
        g.DrawRoundRect(VoLumColors::SEL_BORDER, fill, roundness);
      }
      else if (hovered)
      {
        g.FillRoundRect(VoLumColors::SEL_BG_SOFT, fill, roundness);
      }
      break;
  }
}

// On-selection text colour matching DrawVoLumSelection's fill per style.
inline IColor SelectionInkColor(VoLumSelectionStyle style, bool active)
{
  if (!active)
    return VoLumColors::TEXT_BRIGHT;
  switch (style)
  {
    case VoLumSelectionStyle::AmberPicker: return IColor(255, 26, 18, 8); // dark ink on amber
    case VoLumSelectionStyle::Brass: return VoLumColors::SEL_TEXT;
    case VoLumSelectionStyle::ListTeal: return VoLumColors::SEL_TEXT;
  }
  return VoLumColors::TEXT_BRIGHT;
}

// Edge vignette over a large dark region: transparent centre darkening to the corners.
inline void DrawVignette(IGraphics& g, const IRECT& r, int strength = 64)
{
  const float rad = std::max(r.W(), r.H()) * 0.72f;
  g.PathRect(r);
  g.PathFill(IPattern::CreateRadialGradient(
    r.MW(), r.MH(), rad,
    {{COLOR_TRANSPARENT, 0.f}, {COLOR_TRANSPARENT, 0.62f}, {IColor(strength, 0, 0, 0), 1.f}}));
}

#include "VoLumFractalArt.h"
