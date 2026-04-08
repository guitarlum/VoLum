#pragma once

#include "IControls.h"
#include "VoLumAmpeteCatalog.h"
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace iplug;
using namespace igraphics;

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
const IColor TEXT_DIM(255, 180, 162, 122);
const IColor TEXT_MED(255, 215, 190, 130);
const IColor TEXT_BRIGHT(255, 242, 235, 212);
const IColor GOLD(255, 240, 200, 112);
const IColor GOLD_DIM(255, 182, 158, 82);
const IColor METER_GREEN(255, 42, 138, 42);
const IColor DIVIDER(30, 200, 162, 78);
const IColor FRAME(72, 200, 162, 78);
const IColor CORNER(255, 200, 162, 78);
const IColor BTN_OFF_BG(5, 200, 162, 78);
const IColor BTN_OFF_BORDER(40, 200, 162, 78);
const IColor BTN_OFF_TEXT(255, 180, 162, 122);
const IColor BTN_CAB_ON_BG(30, 200, 162, 78);
const IColor BTN_CAB_ON_BORDER(220, 200, 162, 78);
const IColor BTN_AMP_ON_BG(20, 120, 200, 180);
const IColor BTN_AMP_ON_BORDER(180, 120, 200, 180);
const IColor BTN_AMP_ON_TEXT(255, 140, 220, 200);
const IColor HERO_BG(255, 12, 12, 18);
const IColor HERO_BORDER(50, 200, 162, 78);
const IColor HERO_CORNER(102, 200, 162, 78);
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

class VoLumBackgroundControl : public IControl
{
public:
  VoLumBackgroundControl(const IRECT& bounds, float sidebarWidth)
  : IControl(bounds)
  , mSidebarWidth(sidebarWidth)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(VoLumColors::BG, mRECT);

    // Sidebar
    IRECT sidebar(mRECT.L, mRECT.T, mRECT.L + mSidebarWidth, mRECT.B);
    g.FillRect(VoLumColors::SIDEBAR_BG, sidebar);
    g.DrawLine(VoLumColors::SIDEBAR_BORDER, sidebar.R, sidebar.T, sidebar.R, sidebar.B);

    // Outer gold frame
    g.DrawRect(VoLumColors::FRAME, mRECT);

    // L-shaped corner accents
    const float cs = 22.f;
    const float m = 6.f;
    DrawCornerAccent(g, mRECT.L + m, mRECT.T + m, cs, false, false);
    DrawCornerAccent(g, mRECT.R - m, mRECT.T + m, cs, true, false);
    DrawCornerAccent(g, mRECT.L + m, mRECT.B - m, cs, false, true);
    DrawCornerAccent(g, mRECT.R - m, mRECT.B - m, cs, true, true);
  }

private:
  float mSidebarWidth;
};

class VoLumLogoControl : public IControl
{
public:
  VoLumLogoControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText logoText(30.f, VoLumColors::GOLD, "Poiret-One", EAlign::Center, EVAlign::Middle);
    IRECT logoArea = mRECT.GetFromTop(34.f).GetVShifted(2.f);
    g.DrawText(logoText, "VoLum", logoArea);

    IText subText(9.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    IRECT subArea = mRECT.GetFromTop(32.f).GetVShifted(22.f);
    g.DrawText(subText, "NAM PLAYER", subArea);

    // Stepped Art Deco decoration
    float cy = subArea.B + 8.f;
    float cx = mRECT.MW();
    g.DrawLine(IColor(38, 200, 162, 78), cx - 30.f, cy, cx + 30.f, cy);
    g.DrawLine(IColor(38, 200, 162, 78), cx - 18.f, cy + 3.f, cx + 18.f, cy + 3.f);
    DrawDiamond(g, cx, cy + 9.f, 3.f, VoLumColors::GOLD_DIM);
  }
};

class VoLumAmpListControl : public IControl
{
public:
  using SelectionCallback = std::function<void(int ampIdx)>;

  VoLumAmpListControl(const IRECT& bounds, int numAmps, const char** ampNames, const char** ampAbbrs,
                      SelectionCallback cb)
  : IControl(bounds)
  , mNumAmps(numAmps)
  , mCallback(cb)
  {
    for (int i = 0; i < numAmps; i++)
    {
      mAmpNames.push_back(ampNames[i]);
      mAmpAbbrs.push_back(ampAbbrs[i]);
    }
  }

  void Draw(IGraphics& g) override
  {
    const float itemH = mRECT.H() / (float)mNumAmps;
    const float iconSize = 22.f;
    const float pad = 8.f;

    for (int i = 0; i < mNumAmps; i++)
    {
      IRECT row(mRECT.L, mRECT.T + i * itemH, mRECT.R, mRECT.T + (i + 1) * itemH);
      IRECT paddedRow = row.GetPadded(-2.f, -0.5f, -2.f, -0.5f);

      if (i == mSelected)
      {
        g.FillRect(VoLumColors::ITEM_SEL_BG, paddedRow);
        g.DrawRect(VoLumColors::ITEM_SEL_BORDER, paddedRow);
      }
      else if (i == mHovered)
      {
        g.FillRect(VoLumColors::ITEM_HOVER, paddedRow);
        g.DrawRect(IColor(20, 200, 162, 78), paddedRow);
      }

      // Mini fractal thumbnail
      IRECT iconArea(paddedRow.L + pad, paddedRow.MH() - iconSize / 2.f,
                     paddedRow.L + pad + iconSize, paddedRow.MH() + iconSize / 2.f);

      IColor thmBright = (i == mSelected) ? IColor(200, 120, 210, 220) : IColor(120, 80, 150, 170);
      IColor thmDim = (i == mSelected) ? IColor(100, 100, 180, 200) : IColor(60, 60, 120, 140);
      g.DrawRect(IColor(i == mSelected ? 80 : 40, 100, 180, 200), iconArea);
      DrawMiniFractal(g, iconArea, i, thmBright, thmDim);

      // Amp name -- always prominent
      IRECT nameArea = paddedRow.GetReducedFromLeft(pad + iconSize + 8.f);
      IColor nameCol = (i == mSelected) ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED;
      IText nameText(13.f, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
      g.DrawText(nameText, mAmpNames[i].c_str(), nameArea);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    int idx = HitTestItem(y);
    if (idx >= 0 && idx < mNumAmps)
    {
      mSelected = idx;
      if (mCallback)
        mCallback(idx);
      SetDirty(false);
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    int idx = HitTestItem(y);
    if (idx != mHovered)
    {
      mHovered = idx;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mHovered = -1;
    SetDirty(false);
  }

  void SetSelected(int idx)
  {
    mSelected = idx;
    SetDirty(false);
  }

  int GetSelected() const { return mSelected; }

private:
  int HitTestItem(float y)
  {
    float itemH = mRECT.H() / (float)mNumAmps;
    int idx = (int)((y - mRECT.T) / itemH);
    return (idx >= 0 && idx < mNumAmps) ? idx : -1;
  }

  void DrawMiniFractal(IGraphics& g, const IRECT& r, int idx, const IColor& bright, const IColor& dim)
  {
    float cx = r.MW(), cy = r.MH(), sz = r.W() * 0.35f;
    switch (idx % 14)
    {
      case 0: // Dragon mini: zigzag
        g.DrawLine(bright, cx-sz, cy, cx-sz*0.3f, cy-sz*0.6f, nullptr, 1.5f);
        g.DrawLine(dim, cx-sz*0.3f, cy-sz*0.6f, cx+sz*0.3f, cy+sz*0.4f, nullptr, 1.5f);
        g.DrawLine(bright, cx+sz*0.3f, cy+sz*0.4f, cx+sz, cy-sz*0.2f, nullptr, 1.5f);
        break;
      case 1: // Sierpinski mini: triangle
        g.DrawLine(bright, cx, cy-sz, cx-sz, cy+sz*0.7f, nullptr, 1.5f);
        g.DrawLine(bright, cx-sz, cy+sz*0.7f, cx+sz, cy+sz*0.7f, nullptr, 1.5f);
        g.DrawLine(bright, cx+sz, cy+sz*0.7f, cx, cy-sz, nullptr, 1.5f);
        g.DrawLine(dim, cx-sz*0.5f, cy-sz*0.15f, cx+sz*0.5f, cy-sz*0.15f, nullptr, 1.f);
        break;
      case 2: // Fern mini: line with branches
        g.DrawLine(bright, cx, cy+sz, cx, cy-sz, nullptr, 1.5f);
        g.DrawLine(dim, cx, cy-sz*0.2f, cx+sz*0.6f, cy-sz*0.6f, nullptr, 1.f);
        g.DrawLine(dim, cx, cy+sz*0.2f, cx-sz*0.5f, cy-sz*0.1f, nullptr, 1.f);
        break;
      case 3: // Spiral mini
        for (int j = 0; j < 12; j++) {
          float a1 = j * 30.f * 3.14159f / 180.f, a2 = (j+1) * 30.f * 3.14159f / 180.f;
          float r1 = 2.f + j * 0.5f, r2 = 2.f + (j+1) * 0.5f;
          g.DrawLine(bright, cx+r1*cosf(a1), cy+r1*sinf(a1), cx+r2*cosf(a2), cy+r2*sinf(a2), nullptr, 1.5f);
        }
        break;
      case 4: // Lissajous mini
        for (int j = 0; j < 20; j++) {
          float t1 = j * 6.28f / 20.f, t2 = (j+1) * 6.28f / 20.f;
          g.DrawLine(bright, cx+sinf(3*t1)*sz, cy+sinf(4*t1)*sz*0.8f, cx+sinf(3*t2)*sz, cy+sinf(4*t2)*sz*0.8f, nullptr, 1.5f);
        }
        break;
      case 5: // Koch mini: star shape
        for (int j = 0; j < 6; j++) {
          float a = j * 60.f * 3.14159f / 180.f;
          g.DrawLine(bright, cx, cy, cx+sz*cosf(a), cy+sz*sinf(a), nullptr, 1.5f);
        }
        break;
      case 6: // Tree mini
        g.DrawLine(bright, cx, cy+sz, cx, cy-sz*0.2f, nullptr, 1.5f);
        g.DrawLine(dim, cx, cy-sz*0.2f, cx-sz*0.6f, cy-sz, nullptr, 1.f);
        g.DrawLine(dim, cx, cy-sz*0.2f, cx+sz*0.6f, cy-sz, nullptr, 1.f);
        break;
      case 7: // Hilbert mini: zigzag path
        g.DrawLine(bright, cx-sz, cy+sz, cx-sz, cy-sz, nullptr, 1.5f);
        g.DrawLine(dim, cx-sz, cy-sz, cx+sz, cy-sz, nullptr, 1.f);
        g.DrawLine(bright, cx+sz, cy-sz, cx+sz, cy+sz, nullptr, 1.5f);
        break;
      case 8: // Levy C mini: jagged line
        g.DrawLine(bright, cx-sz, cy+sz*0.5f, cx-sz*0.3f, cy-sz*0.5f, nullptr, 1.5f);
        g.DrawLine(dim, cx-sz*0.3f, cy-sz*0.5f, cx+sz*0.3f, cy+sz*0.3f, nullptr, 1.f);
        g.DrawLine(bright, cx+sz*0.3f, cy+sz*0.3f, cx+sz, cy-sz*0.3f, nullptr, 1.5f);
        break;
      case 9: // Mandelbrot mini: concentric circles
        g.DrawArc(bright, cx, cy, sz*0.4f, 0.f, 360.f, nullptr, 1.5f);
        g.DrawArc(dim, cx-sz*0.3f, cy, sz*0.6f, 0.f, 360.f, nullptr, 1.f);
        break;
      case 10: // Julia mini: figure-8
        g.DrawArc(bright, cx-sz*0.3f, cy, sz*0.5f, 0.f, 360.f, nullptr, 1.5f);
        g.DrawArc(dim, cx+sz*0.3f, cy, sz*0.5f, 0.f, 360.f, nullptr, 1.f);
        break;
      case 11: // Vicsek mini: cross
        g.DrawLine(bright, cx-sz, cy, cx+sz, cy, nullptr, 1.5f);
        g.DrawLine(bright, cx, cy-sz, cx, cy+sz, nullptr, 1.5f);
        g.DrawLine(dim, cx-sz*0.5f, cy-sz*0.5f, cx+sz*0.5f, cy+sz*0.5f, nullptr, 1.f);
        g.DrawLine(dim, cx+sz*0.5f, cy-sz*0.5f, cx-sz*0.5f, cy+sz*0.5f, nullptr, 1.f);
        break;
      case 12: // Burning ship mini: flame shape
        g.DrawLine(bright, cx, cy+sz, cx-sz*0.4f, cy-sz*0.3f, nullptr, 1.5f);
        g.DrawLine(bright, cx-sz*0.4f, cy-sz*0.3f, cx, cy-sz, nullptr, 1.5f);
        g.DrawLine(dim, cx, cy-sz, cx+sz*0.4f, cy-sz*0.3f, nullptr, 1.f);
        g.DrawLine(dim, cx+sz*0.4f, cy-sz*0.3f, cx, cy+sz, nullptr, 1.f);
        break;
      case 13: // Pentaflake mini: pentagon
        for (int j = 0; j < 5; j++) {
          float a1 = (j*72.f-90.f)*3.14159f/180.f, a2 = ((j+1)*72.f-90.f)*3.14159f/180.f;
          g.DrawLine(bright, cx+sz*cosf(a1), cy+sz*sinf(a1), cx+sz*cosf(a2), cy+sz*sinf(a2), nullptr, 1.5f);
        }
        break;
    }
  }

  int mNumAmps = 0;
  int mSelected = 0;
  int mHovered = -1;
  std::vector<std::string> mAmpNames;
  std::vector<std::string> mAmpAbbrs;
  SelectionCallback mCallback;
};

class VoLumSpeakerRowControl : public IControl
{
public:
  using ChangeCallback = std::function<void(int speakerIdx)>;

  VoLumSpeakerRowControl(const IRECT& bounds, ChangeCallback cb = nullptr)
  : IControl(bounds)
  , mCallback(cb)
  {
    mSelected = 3;
  }

  void Draw(IGraphics& g) override
  {
    const char* labels[] = {"AMP", "G12", "G65", "V30"};
    const float btnW = 52.f;
    const float btnH = 26.f;
    const float gap = 6.f;
    const float divGap = 12.f;
    const float labelGap = 6.f;

    // Single line: DIRECT [AMP] | CABINET [G12] [G65] [V30]
    IText sectionText(12.f, IColor(255, 248, 222, 140), "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const float directLblW = 50.f;
    const float cabLblW = 66.f;

    float totalW = directLblW + labelGap + btnW + divGap + cabLblW + labelGap + 3 * btnW + 2 * gap;
    float x = mRECT.MW() - totalW / 2.f;
    float btnY = mRECT.MH() - btnH / 2.f;

    // "DIRECT" label
    IRECT directLabel(x, btnY, x + directLblW, btnY + btnH);
    g.DrawText(sectionText, "DIRECT", directLabel);
    x += directLblW + labelGap;

    // AMP button (index 0) -- teal/cyan accent when active
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (0 == mSelected);
      g.FillRoundRect(isOn ? VoLumColors::BTN_AMP_ON_BG : VoLumColors::BTN_OFF_BG, btn, 3.f);
      g.DrawRoundRect(isOn ? VoLumColors::BTN_AMP_ON_BORDER : VoLumColors::BTN_OFF_BORDER, btn, 3.f);
      IText btnText(13.f, isOn ? VoLumColors::BTN_AMP_ON_TEXT : VoLumColors::BTN_OFF_TEXT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnText, labels[0], btn);
      mBtnRects[0] = btn;
      x += btnW;
    }

    // Divider
    float divX = x + divGap / 2.f;
    g.DrawLine(IColor(60, 200, 162, 78), divX, btnY + 4.f, divX, btnY + btnH - 4.f);
    x += divGap;

    // "CABINET" label
    IRECT cabLabel(x, btnY, x + cabLblW, btnY + btnH);
    g.DrawText(sectionText, "CABINET", cabLabel);
    x += cabLblW + labelGap;

    // G12, G65, V30 buttons (indices 1-3) -- gold accent when active
    for (int i = 1; i < 4; i++)
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (i == mSelected);
      g.FillRoundRect(isOn ? VoLumColors::BTN_CAB_ON_BG : VoLumColors::BTN_OFF_BG, btn, 3.f);
      g.DrawRoundRect(isOn ? VoLumColors::BTN_CAB_ON_BORDER : VoLumColors::BTN_OFF_BORDER, btn, 3.f);
      IColor cabTextCol = isOn ? IColor(255, 250, 220, 130) : VoLumColors::BTN_OFF_TEXT;
      IText btnTextCab(13.f, cabTextCol, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnTextCab, labels[i], btn);
      mBtnRects[i] = btn;
      x += btnW + gap;
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    for (int i = 0; i < 4; i++)
    {
      if (mBtnRects[i].Contains(x, y) && i != mSelected)
      {
        mSelected = i;
        if (mCallback)
          mCallback(i);
        SetDirty(false);
        return;
      }
    }
  }

  int GetSelected() const { return mSelected; }

  void SetSelected(int idx)
  {
    mSelected = idx;
    SetDirty(false);
  }

private:
  int mSelected = 3;
  IRECT mBtnRects[4];
  ChangeCallback mCallback;
};

class VoLumHeroImageControl : public IControl
{
public:
  VoLumHeroImageControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (mHasBitmap)
    {
      float imgW = (float)mBitmap.W();
      float imgH = (float)mBitmap.H() / (float)std::max(1, mBitmap.N());
      float scale = std::min(mRECT.W() / imgW, mRECT.H() / imgH);
      float drawW = imgW * scale;
      float drawH = imgH * scale;
      IRECT centered(mRECT.MW() - drawW / 2.f, mRECT.MH() - drawH / 2.f,
                      mRECT.MW() + drawW / 2.f, mRECT.MH() + drawH / 2.f);
      g.DrawFittedBitmap(mBitmap, centered);
    }
    else
    {
      g.FillRect(VoLumColors::HERO_BG, mRECT);
      g.DrawRect(VoLumColors::HERO_BORDER, mRECT);

      const float cs = 16.f;
      const float m = 6.f;
      DrawCornerAccent(g, mRECT.L + m, mRECT.T + m, cs, false, false, VoLumColors::HERO_CORNER);
      DrawCornerAccent(g, mRECT.R - m, mRECT.T + m, cs, true, false, VoLumColors::HERO_CORNER);
      DrawCornerAccent(g, mRECT.L + m, mRECT.B - m, cs, false, true, VoLumColors::HERO_CORNER);
      DrawCornerAccent(g, mRECT.R - m, mRECT.B - m, cs, true, true, VoLumColors::HERO_CORNER);

      DrawGeometricArt(g);
    }
  }

  void SetPlaceholder(const char* text, int ampIdx = 0)
  {
    mPlaceholder = text;
    mAmpIdx = ampIdx;
    mHasBitmap = false;
    SetDirty(false);
  }

  void SetName(const char* name)
  {
    mName = name;
    SetDirty(false);
  }

  void SetBitmap(const IBitmap& bitmap)
  {
    mBitmap = bitmap;
    mHasBitmap = true;
    SetDirty(false);
  }

private:
  void DrawGeometricArt(IGraphics& g)
  {
    float cx = mRECT.MW();
    float cy = mRECT.MH();
    float w = mRECT.W();
    float h = mRECT.H();

    IColor bright(150, 120, 210, 220);
    IColor mid(80, 100, 180, 200);
    IColor dim(45, 80, 150, 170);
    const float tk = 2.f;
    const float tkThin = 1.5f;

    switch (mAmpIdx % 14)
    {
      case 0: // Dragon curve
      {
        std::vector<int> turns;
        for (int i = 0; i < 12; i++)
        {
          std::vector<int> next;
          for (auto t : turns) next.push_back(t);
          next.push_back(1);
          for (int j = (int)turns.size() - 1; j >= 0; j--) next.push_back(1 - turns[j]);
          turns = next;
        }
        float step = 3.2f;
        float px = cx - 40.f, py = cy + 20.f;
        int dir = 0;
        const float dx[] = {step, 0, -step, 0};
        const float dy[] = {0, -step, 0, step};
        for (int i = 0; i < (int)turns.size(); i++)
        {
          float nx = px + dx[dir], ny = py + dy[dir];
          int alpha = 50 + (int)(80.f * i / turns.size());
          g.DrawLine(IColor(alpha, 120, 210, 220), px, py, nx, ny, nullptr, tkThin);
          px = nx; py = ny;
          dir = (dir + (turns[i] ? 1 : 3)) % 4;
        }
        break;
      }
      case 1: // Sierpinski triangle
      {
        struct Tri { float x1,y1,x2,y2,x3,y3; };
        std::vector<Tri> tris;
        float sz = std::min(w * 0.4f, h * 0.8f);
        tris.push_back({cx, cy - sz * 0.45f, cx - sz * 0.5f, cy + sz * 0.45f, cx + sz * 0.5f, cy + sz * 0.45f});
        for (int depth = 0; depth < 5; depth++)
        {
          std::vector<Tri> next;
          IColor col = (depth < 2) ? bright : ((depth < 4) ? mid : dim);
          for (auto& t : tris)
          {
            g.DrawLine(col, t.x1, t.y1, t.x2, t.y2, nullptr, tk);
            g.DrawLine(col, t.x2, t.y2, t.x3, t.y3, nullptr, tk);
            g.DrawLine(col, t.x3, t.y3, t.x1, t.y1, nullptr, tk);
            float mx12=(t.x1+t.x2)/2,my12=(t.y1+t.y2)/2;
            float mx23=(t.x2+t.x3)/2,my23=(t.y2+t.y3)/2;
            float mx31=(t.x3+t.x1)/2,my31=(t.y3+t.y1)/2;
            next.push_back({t.x1,t.y1,mx12,my12,mx31,my31});
            next.push_back({mx12,my12,t.x2,t.y2,mx23,my23});
            next.push_back({mx31,my31,mx23,my23,t.x3,t.y3});
          }
          tris = next;
        }
        break;
      }
      case 2: // Barnsley fern
      {
        float px = 0, py = 0;
        unsigned int rng = 42;
        for (int i = 0; i < 12000; i++)
        {
          rng = rng * 1103515245 + 12345;
          float r = (float)(rng % 1000) / 1000.f;
          float nx, ny;
          if (r < 0.01f) { nx = 0; ny = 0.16f * py; }
          else if (r < 0.86f) { nx = 0.85f*px + 0.04f*py; ny = -0.04f*px + 0.85f*py + 1.6f; }
          else if (r < 0.93f) { nx = 0.2f*px - 0.26f*py; ny = 0.23f*px + 0.22f*py + 1.6f; }
          else { nx = -0.15f*px + 0.28f*py; ny = 0.26f*px + 0.24f*py + 0.44f; }
          px = nx; py = ny;
          float sx = cx + px * 38.f;
          float sy = mRECT.B - 15.f - py * 15.f;
          if (sx > mRECT.L && sx < mRECT.R && sy > mRECT.T && sy < mRECT.B)
          {
            int alpha = 40 + (int)(py * 8.f);
            g.FillRect(IColor(alpha, 80, 200, 180), IRECT(sx, sy, sx + 1.5f, sy + 1.5f));
          }
        }
        break;
      }
      case 3: // Golden spiral
      {
        float phi = 1.6180339887f;
        float r = 8.f;
        float angle = 0.f;
        float prevX = cx, prevY = cy;
        for (int i = 0; i < 200; i++)
        {
          float a = angle * 3.14159f / 180.f;
          float x = cx + r * cosf(a);
          float y = cy + r * sinf(a);
          int alpha = 40 + (int)(90.f * (1.f - (float)i / 200.f));
          g.DrawLine(IColor(alpha, 120, 210, 220), prevX, prevY, x, y, nullptr, tk);
          prevX = x; prevY = y;
          angle += 8.f;
          r += 0.45f;
        }
        DrawDiamond(g, cx, cy, 4.f, bright);
        break;
      }
      case 4: // Lissajous knot
      {
        float prevX2 = 0, prevY2 = 0;
        bool first = true;
        for (int i = 0; i <= 600; i++)
        {
          float t = i * 6.28318f / 600.f;
          float x2 = cx + sinf(3.f * t + 0.5f) * (w * 0.36f);
          float y2 = cy + sinf(4.f * t) * (h * 0.38f);
          if (!first)
          {
            int alpha = 50 + (int)(80.f * (0.5f + 0.5f * sinf(t * 2.f)));
            g.DrawLine(IColor(alpha, 120, 210, 220), prevX2, prevY2, x2, y2, nullptr, tk);
          }
          prevX2 = x2; prevY2 = y2; first = false;
        }
        break;
      }
      case 5: // Koch snowflake
      {
        struct Seg { float x1,y1,x2,y2; };
        std::vector<Seg> segs;
        float r = std::min(w * 0.35f, h * 0.42f);
        for (int i = 0; i < 3; i++)
        {
          float a1 = (i * 120.f - 90.f) * 3.14159f / 180.f;
          float a2 = ((i+1) * 120.f - 90.f) * 3.14159f / 180.f;
          segs.push_back({cx+r*cosf(a1),cy+r*sinf(a1),cx+r*cosf(a2),cy+r*sinf(a2)});
        }
        for (int depth = 0; depth < 4; depth++)
        {
          std::vector<Seg> next;
          for (auto& s : segs)
          {
            float dx=s.x2-s.x1,dy=s.y2-s.y1;
            float ax=s.x1+dx/3.f,ay=s.y1+dy/3.f;
            float bx=s.x1+dx*2.f/3.f,by=s.y1+dy*2.f/3.f;
            float px2=(s.x1+s.x2)/2.f-dy*0.2887f;
            float py2=(s.y1+s.y2)/2.f+dx*0.2887f;
            next.push_back({s.x1,s.y1,ax,ay});
            next.push_back({ax,ay,px2,py2});
            next.push_back({px2,py2,bx,by});
            next.push_back({bx,by,s.x2,s.y2});
          }
          segs = next;
        }
        for (auto& s : segs)
          g.DrawLine(mid, s.x1, s.y1, s.x2, s.y2, nullptr, tkThin);
        break;
      }
      case 6: // Fractal tree
      {
        struct Branch { float x, y, angle, len; int depth; };
        std::vector<Branch> stack;
        stack.push_back({cx, mRECT.B - 15.f, -90.f, 52.f, 0});
        while (!stack.empty())
        {
          auto b = stack.back(); stack.pop_back();
          if (b.depth > 8 || b.len < 2.5f) continue;
          float rad = b.angle * 3.14159f / 180.f;
          float ex = b.x + b.len * cosf(rad);
          float ey = b.y + b.len * sinf(rad);
          IColor col = (b.depth < 2) ? bright : ((b.depth < 5) ? mid : dim);
          g.DrawLine(col, b.x, b.y, ex, ey, nullptr, (b.depth < 3) ? tk : tkThin);
          float spread = 22.f + b.depth * 4.f;
          stack.push_back({ex, ey, b.angle - spread, b.len * 0.67f, b.depth + 1});
          stack.push_back({ex, ey, b.angle + spread, b.len * 0.67f, b.depth + 1});
        }
        break;
      }
      case 7: // Hilbert curve
      {
        int order = 5;
        int n = 1 << order;
        float cellW = (w * 0.75f) / n;
        float cellH = (h * 0.85f) / n;
        float offX = cx - (n * cellW) / 2.f + cellW / 2.f;
        float offY = cy - (n * cellH) / 2.f + cellH / 2.f;
        auto d2xy = [](int n, int d, int& x, int& y) {
          x = y = 0;
          for (int s = 1; s < n; s *= 2)
          {
            int rx = 1 & (d / 2);
            int ry = 1 & (d ^ rx);
            if (ry == 0) { if (rx == 1) { x = s - 1 - x; y = s - 1 - y; } int t = x; x = y; y = t; }
            x += s * rx;
            y += s * ry;
            d /= 4;
          }
        };
        int total = n * n;
        int px2, py2;
        d2xy(n, 0, px2, py2);
        float prevX2 = offX + px2 * cellW, prevY2 = offY + py2 * cellH;
        for (int i = 1; i < total; i++)
        {
          d2xy(n, i, px2, py2);
          float nx = offX + px2 * cellW, ny = offY + py2 * cellH;
          int alpha = 40 + (int)(90.f * i / total);
          g.DrawLine(IColor(alpha, 120, 210, 220), prevX2, prevY2, nx, ny, nullptr, tkThin);
          prevX2 = nx; prevY2 = ny;
        }
        break;
      }
      case 8: // Levy C curve
      {
        struct Seg { float x1,y1,x2,y2; };
        std::vector<Seg> segs;
        segs.push_back({cx - 80.f, cy + 20.f, cx + 80.f, cy + 20.f});
        for (int depth = 0; depth < 12; depth++)
        {
          std::vector<Seg> next;
          for (auto& s : segs)
          {
            float mx = (s.x1 + s.x2) / 2.f + (s.y2 - s.y1) / 2.f;
            float my = (s.y1 + s.y2) / 2.f - (s.x2 - s.x1) / 2.f;
            next.push_back({s.x1, s.y1, mx, my});
            next.push_back({mx, my, s.x2, s.y2});
          }
          segs = next;
        }
        for (int i = 0; i < (int)segs.size(); i++)
        {
          int alpha = 30 + (int)(100.f * i / segs.size());
          g.DrawLine(IColor(alpha, 100, 190, 210), segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2, nullptr, 1.f);
        }
        break;
      }
      case 9: // Mandelbrot zoom
      {
        double zoomCx = -0.745, zoomCy = 0.186;
        float plotW = w * 0.85f, plotH = h * 0.85f;
        float plotL = cx - plotW / 2.f, plotT2 = cy - plotH / 2.f;
        float step = 3.f;
        for (float px = 0; px < plotW; px += step)
          for (float py = 0; py < plotH; py += step)
          {
            double cr = zoomCx + (px / plotW - 0.5) * 0.008;
            double ci = zoomCy + (py / plotH - 0.5) * 0.008;
            double zr = 0, zi = 0;
            int iter = 0;
            while (zr * zr + zi * zi < 4.0 && iter < 60) { double t = zr*zr - zi*zi + cr; zi = 2*zr*zi + ci; zr = t; iter++; }
            if (iter < 60 && iter > 5)
              g.FillRect(IColor(std::min(140, iter * 5), 80 + iter, std::min(255, 180 + iter * 2), 220),
                IRECT(plotL + px, plotT2 + py, plotL + px + step - 0.5f, plotT2 + py + step - 0.5f));
          }
        break;
      }
      case 10: // Julia set
      {
        double jcr = -0.7, jci = 0.27015;
        float plotW = w * 0.85f, plotH = h * 0.85f;
        float plotL = cx - plotW / 2.f, plotT2 = cy - plotH / 2.f;
        float step = 3.f;
        for (float px = 0; px < plotW; px += step)
          for (float py = 0; py < plotH; py += step)
          {
            double zr = (px / plotW - 0.5) * 3.0;
            double zi = (py / plotH - 0.5) * 2.4;
            int iter = 0;
            while (zr * zr + zi * zi < 4.0 && iter < 50) { double t = zr*zr - zi*zi + jcr; zi = 2*zr*zi + jci; zr = t; iter++; }
            if (iter < 50 && iter > 3)
              g.FillRect(IColor(std::min(140, iter * 6), 70 + iter * 2, std::min(255, 160 + iter * 3), 220),
                IRECT(plotL + px, plotT2 + py, plotL + px + step - 0.5f, plotT2 + py + step - 0.5f));
          }
        break;
      }
      case 11: // Vicsek fractal (cross)
      {
        struct Sq { float x, y, sz; };
        std::vector<Sq> squares;
        float sz = std::min(w * 0.35f, h * 0.7f);
        squares.push_back({cx, cy, sz});
        for (int depth = 0; depth < 4; depth++)
        {
          std::vector<Sq> next;
          IColor col = (depth < 1) ? bright : ((depth < 3) ? mid : dim);
          for (auto& s : squares)
          {
            float half = s.sz / 2.f;
            IRECT r(s.x - half, s.y - half, s.x + half, s.y + half);
            g.DrawRect(col, r, nullptr, tk);
            float ns = s.sz / 3.f;
            next.push_back({s.x, s.y, ns});
            next.push_back({s.x - s.sz / 3.f, s.y, ns});
            next.push_back({s.x + s.sz / 3.f, s.y, ns});
            next.push_back({s.x, s.y - s.sz / 3.f, ns});
            next.push_back({s.x, s.y + s.sz / 3.f, ns});
          }
          squares = next;
        }
        break;
      }
      case 12: // Burning Ship fractal
      {
        float plotW = w * 0.85f, plotH = h * 0.85f;
        float plotL = cx - plotW / 2.f, plotT2 = cy - plotH / 2.f;
        float step = 3.f;
        for (float px = 0; px < plotW; px += step)
          for (float py = 0; py < plotH; py += step)
          {
            double cr = -1.75 + (px / plotW) * 0.15;
            double ci = -0.08 + (py / plotH) * 0.12;
            double zr = 0, zi = 0;
            int iter = 0;
            while (zr * zr + zi * zi < 4.0 && iter < 60)
            {
              double t = zr * zr - zi * zi + cr;
              zi = fabs(2.0 * zr * zi) + ci;
              zr = t;
              iter++;
            }
            if (iter < 60 && iter > 3)
              g.FillRect(IColor(std::min(150, iter * 6), 100 + iter * 2, std::min(255, 170 + iter * 2), 230),
                IRECT(plotL + px, plotT2 + py, plotL + px + step - 0.5f, plotT2 + py + step - 0.5f));
          }
        break;
      }
      case 13: // Pentaflake (pentagon fractal)
      {
        float r = std::min(w * 0.32f, h * 0.42f);
        struct Pent { float x, y, r; };
        std::vector<Pent> pents;
        pents.push_back({cx, cy, r});
        for (int depth = 0; depth < 3; depth++)
        {
          std::vector<Pent> next;
          IColor col = (depth == 0) ? bright : ((depth == 1) ? mid : dim);
          for (auto& p : pents)
          {
            for (int i = 0; i < 5; i++)
            {
              float a1 = (i * 72.f - 90.f) * 3.14159f / 180.f;
              float a2 = ((i + 1) * 72.f - 90.f) * 3.14159f / 180.f;
              g.DrawLine(col, p.x + p.r * cosf(a1), p.y + p.r * sinf(a1),
                              p.x + p.r * cosf(a2), p.y + p.r * sinf(a2), nullptr, tk);
            }
            float nr = p.r * 0.382f;
            next.push_back({p.x, p.y, nr});
            for (int i = 0; i < 5; i++)
            {
              float a = (i * 72.f - 90.f) * 3.14159f / 180.f;
              next.push_back({p.x + (p.r - nr) * cosf(a), p.y + (p.r - nr) * sinf(a), nr});
            }
          }
          pents = next;
        }
        break;
      }
    }

  }

  bool mHasBitmap = false;
  IBitmap mBitmap;
  std::string mPlaceholder = "A1";
  std::string mName = "Ampete One";
  int mAmpIdx = 0;
};

class VoLumAmpNameControl : public IControl
{
public:
  VoLumAmpNameControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText nameText(26.f, VoLumColors::GOLD, "Poiret-One", EAlign::Center, EVAlign::Middle);
    IRECT textArea = mRECT.GetFromTop(28.f);
    g.DrawText(nameText, mName.c_str(), textArea);

    // Gold divider with diamond below the name
    float cy = textArea.B + 6.f;
    float cx = mRECT.MW();
    g.DrawLine(IColor(51, 200, 162, 78), cx - 50.f, cy, cx - 6.f, cy);
    g.DrawLine(IColor(51, 200, 162, 78), cx + 6.f, cy, cx + 50.f, cy);
    DrawDiamond(g, cx, cy, 3.f, VoLumColors::GOLD_DIM);
  }

  void SetName(const char* name)
  {
    mName = name;
    SetDirty(false);
  }

private:
  std::string mName = "Ampete One";
};

class VoLumMeterControl : public IControl
{
public:
  VoLumMeterControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(255, 10, 10, 16), mRECT);
    g.DrawRect(IColor(20, 200, 162, 78), mRECT);

    float fillH = mRECT.H() * mLevel;
    if (fillH > 1.f)
    {
      IRECT fill(mRECT.L + 1.f, mRECT.B - fillH, mRECT.R - 1.f, mRECT.B - 1.f);
      g.FillRect(VoLumColors::METER_GREEN, fill);
    }
  }

  void SetLevel(float level)
  {
    mLevel = std::clamp(level, 0.f, 1.f);
    SetDirty(false);
  }

private:
  float mLevel = 0.35f;
};

class VoLumKnobLabelControl : public IControl
{
public:
  VoLumKnobLabelControl(const IRECT& bounds, const char* label, bool isChannel = false)
  : IControl(bounds)
  , mLabel(label)
  , mIsChannel(isChannel)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText text(13.f, IColor(255, 248, 222, 140), "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, mLabel.c_str(), mRECT);
  }

private:
  std::string mLabel;
  bool mIsChannel;
};

// Vertical text label (draws each character stacked)
class VoLumVerticalLabelControl : public IControl
{
public:
  VoLumVerticalLabelControl(const IRECT& bounds, const char* label)
  : IControl(bounds)
  , mLabel(label)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText text(8.f, VoLumColors::GOLD_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    int len = (int)mLabel.size();
    float charH = 10.f;
    float totalH = len * charH;
    float startY = mRECT.MH() - totalH / 2.f;
    for (int i = 0; i < len; i++)
    {
      char ch[2] = {mLabel[i], 0};
      IRECT charArea(mRECT.L, startY + i * charH, mRECT.R, startY + (i + 1) * charH);
      g.DrawText(text, ch, charArea);
    }
  }

private:
  std::string mLabel;
};

class VoLumDividerControl : public IControl
{
public:
  VoLumDividerControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override { g.FillRect(VoLumColors::DIVIDER, mRECT); }
};

class VoLumFooterControl : public IControl
{
public:
  VoLumFooterControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText text(10.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, mText.c_str(), mRECT);
  }

  void SetText(const char* text)
  {
    mText = text;
    SetDirty(false);
  }

private:
  std::string mText = "(no rig loaded)";
};

// Art Deco channel stepper: gold-themed [<] Ch 1 [>]
class VoLumChannelStepControl : public IControl
{
public:
  using ChangeCallback = std::function<void(int newChannelIdx)>;

  VoLumChannelStepControl(const IRECT& bounds, ChangeCallback cb)
  : IControl(bounds, kNoParameter)
  , mCallback(cb)
  {
  }

  void SetChannels(const std::vector<std::string>& labels, int selected)
  {
    mLabels = labels;
    mSelected = labels.empty() ? 0 : std::clamp(selected, 0, (int)labels.size() - 1);
    SetDirty(false);
  }

  int GetSelected() const { return mSelected; }
  int GetNumChannels() const { return (int)mLabels.size(); }

  void Draw(IGraphics& g) override
  {
    const int n = (int)mLabels.size();
    const float arrowW = 18.f;

    // Minimal Art Deco: thin gold lines flanking the channel number
    float lineY = mRECT.MH();
    g.DrawLine(IColor(50, 200, 162, 78), mRECT.L, lineY, mRECT.L + arrowW - 4.f, lineY);
    g.DrawLine(IColor(50, 200, 162, 78), mRECT.R - arrowW + 4.f, lineY, mRECT.R, lineY);

    // Left arrow: <
    IRECT leftArea(mRECT.L, mRECT.T, mRECT.L + arrowW, mRECT.B);
    IColor leftCol = mMouseOverLeft ? VoLumColors::GOLD : VoLumColors::TEXT_MED;
    IText arrowText(16.f, leftCol, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowText, "<", leftArea);

    // Right arrow: >
    IRECT rightArea(mRECT.R - arrowW, mRECT.T, mRECT.R, mRECT.B);
    IColor rightCol = mMouseOverRight ? VoLumColors::GOLD : VoLumColors::TEXT_MED;
    IText arrowTextR(16.f, rightCol, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowTextR, ">", rightArea);

    // Center: channel number
    IRECT center(mRECT.L + arrowW, mRECT.T, mRECT.R - arrowW, mRECT.B);
    const char* label = (n > 0 && mSelected >= 0 && mSelected < n) ? mLabels[mSelected].c_str() : "---";
    IText labelText(18.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(labelText, label, center);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const int n = (int)mLabels.size();
    if (n < 1)
      return;

    const float hitW = 22.f;
    if (x < mRECT.L + hitW)
      mSelected = (mSelected - 1 + n) % n;
    else if (x > mRECT.R - hitW)
      mSelected = (mSelected + 1) % n;
    else
      return;

    if (mCallback)
      mCallback(mSelected);
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    const float hitW = 22.f;
    bool left = (x < mRECT.L + hitW);
    bool right = (x > mRECT.R - hitW);
    if (left != mMouseOverLeft || right != mMouseOverRight)
    {
      mMouseOverLeft = left;
      mMouseOverRight = right;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mMouseOverLeft = false;
    mMouseOverRight = false;
    SetDirty(false);
  }

private:
  int mSelected = 0;
  bool mMouseOverLeft = false;
  bool mMouseOverRight = false;
  std::vector<std::string> mLabels;
  ChangeCallback mCallback;
};

class VoLumParamValueControl : public IControl
{
public:
  VoLumParamValueControl(const IRECT& bounds, int paramIdx, const char* suffix = "")
  : IControl(bounds, paramIdx)
  , mSuffix(suffix)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    WDL_String str;
    const auto* pParam = GetParam();
    if (pParam)
    {
      pParam->GetDisplay(str);
      if (mSuffix[0])
      {
        str.Append(" ");
        str.Append(mSuffix);
      }
    }

    IText text(11.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, str.Get(), mRECT);
  }

  void SetValueFromDelegate(double value, int valIdx) override
  {
    IControl::SetValueFromDelegate(value, valIdx);
    SetDirty(false);
  }

private:
  const char* mSuffix;
};
