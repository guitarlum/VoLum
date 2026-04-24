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

#include "VoLumFractalArt.h"
