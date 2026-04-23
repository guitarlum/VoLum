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

class VoLumKnobSelectionClearControl : public IControl
{
public:
  using ClearCallback = std::function<void()>;

  VoLumKnobSelectionClearControl(const IRECT& bounds, ClearCallback callback)
  : IControl(bounds)
  , mCallback(callback)
  {
  }

  void Draw(IGraphics& g) override {}

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mCallback)
      mCallback();
  }

private:
  ClearCallback mCallback;
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

    IText subText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
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

    // Ensure fractal icon cache is the right size
    if ((int)mIconLayers.size() != mNumAmps)
      mIconLayers.resize(mNumAmps);

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

      IRECT iconArea(paddedRow.L + pad, paddedRow.MH() - iconSize / 2.f,
                     paddedRow.L + pad + iconSize, paddedRow.MH() + iconSize / 2.f);

      // Cached fractal thumbnail (deterministic per amp index + bounds)
      if (!mIconLayers[i] || g.CheckLayer(mIconLayers[i]))
      {
        g.StartLayer(this, iconArea);
        IColor thmBright(200, 120, 210, 220);
        IColor thmDim(100, 100, 180, 200);
        DrawMiniFractal(g, iconArea, i, thmBright, thmDim);
        mIconLayers[i] = g.EndLayer();
      }

      g.DrawRect(IColor(i == mSelected ? 80 : 58, 120, 195, 210), iconArea);
      g.DrawLayer(mIconLayers[i]);

      IRECT nameArea = paddedRow.GetReducedFromLeft(pad + iconSize + 8.f);
      IColor nameCol = (i == mSelected) ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED;
      IText nameText(13.f, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
      g.DrawText(nameText, mAmpNames[i].c_str(), nameArea);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ClearVoLumKnobSelection(this);

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
    float cx = r.MW(), cy = r.MH(), sz = r.W() * 0.38f;
    switch (idx % 14)
    {
      case 0: // Dragon curve mini (6 iterations)
      {
        std::vector<int> turns;
        for (int i = 0; i < 6; i++) {
          std::vector<int> n2;
          for (auto t : turns) n2.push_back(t);
          n2.push_back(1);
          for (int j=(int)turns.size()-1;j>=0;j--) n2.push_back(1-turns[j]);
          turns = n2;
        }
        float step = 1.8f, px2 = cx-3.f, py2 = cy+2.f; int dir = 0;
        const float dx[]={step,0,-step,0}, dy[]={0,-step,0,step};
        for (int i=0;i<(int)turns.size();i++) {
          float nx=px2+dx[dir],ny=py2+dy[dir];
          g.DrawLine(bright, px2,py2,nx,ny,nullptr,1.f);
          px2=nx;py2=ny; dir=(dir+(turns[i]?1:3))%4;
        }
        break;
      }
      case 1: // Sierpinski triangle mini (3 depth)
      {
        struct T{float x1,y1,x2,y2,x3,y3;};
        std::vector<T> ts; ts.push_back({cx,cy-sz,cx-sz,cy+sz*0.7f,cx+sz,cy+sz*0.7f});
        for (int d=0;d<3;d++) {
          std::vector<T> n2;
          for (auto&t:ts) {
            g.DrawLine(d<1?bright:dim,t.x1,t.y1,t.x2,t.y2,nullptr,1.f);
            g.DrawLine(d<1?bright:dim,t.x2,t.y2,t.x3,t.y3,nullptr,1.f);
            g.DrawLine(d<1?bright:dim,t.x3,t.y3,t.x1,t.y1,nullptr,1.f);
            n2.push_back({t.x1,t.y1,(t.x1+t.x2)/2,(t.y1+t.y2)/2,(t.x3+t.x1)/2,(t.y3+t.y1)/2});
            n2.push_back({(t.x1+t.x2)/2,(t.y1+t.y2)/2,t.x2,t.y2,(t.x2+t.x3)/2,(t.y2+t.y3)/2});
            n2.push_back({(t.x3+t.x1)/2,(t.y3+t.y1)/2,(t.x2+t.x3)/2,(t.y2+t.y3)/2,t.x3,t.y3});
          }
          ts=n2;
        }
        break;
      }
      case 2: // Fern mini (scatter dots)
      {
        float px2=0,py2=0; unsigned rng=42;
        for (int i=0;i<300;i++) {
          rng=rng*1103515245+12345; float rv=(float)(rng%1000)/1000.f;
          float nx,ny;
          if(rv<0.01f){nx=0;ny=0.16f*py2;}
          else if(rv<0.86f){nx=0.85f*px2+0.04f*py2;ny=-0.04f*px2+0.85f*py2+1.6f;}
          else if(rv<0.93f){nx=0.2f*px2-0.26f*py2;ny=0.23f*px2+0.22f*py2+1.6f;}
          else{nx=-0.15f*px2+0.28f*py2;ny=0.26f*px2+0.24f*py2+0.44f;}
          px2=nx;py2=ny;
          float sx=cx+px2*3.f, sy=r.B-2.f-py2*1.8f;
          if(sx>r.L&&sx<r.R&&sy>r.T&&sy<r.B) g.FillRect(bright,IRECT(sx,sy,sx+1.f,sy+1.f));
        }
        break;
      }
      case 3: // Golden spiral mini
      {
        float ra=1.5f,ang=0,pvx=cx,pvy=cy;
        for(int i=0;i<40;i++){
          float a=ang*3.14159f/180.f;
          float x2=cx+ra*cosf(a),y2=cy+ra*sinf(a);
          g.DrawLine(bright,pvx,pvy,x2,y2,nullptr,1.f);
          pvx=x2;pvy=y2;ang+=15.f;ra+=0.18f;
        }
        break;
      }
      case 4: // Lissajous mini
      {
        for(int j=0;j<30;j++){
          float t1=j*6.28f/30.f,t2=(j+1)*6.28f/30.f;
          g.DrawLine(bright,cx+sinf(3*t1)*sz,cy+sinf(4*t1)*sz*0.8f,cx+sinf(3*t2)*sz,cy+sinf(4*t2)*sz*0.8f,nullptr,1.f);
        }
        break;
      }
      case 5: // Koch snowflake mini (2 depth)
      {
        struct S{float x1,y1,x2,y2;};
        std::vector<S> segs;
        for(int i=0;i<3;i++){
          float a1=(i*120.f-90.f)*3.14159f/180.f,a2=((i+1)*120.f-90.f)*3.14159f/180.f;
          segs.push_back({cx+sz*cosf(a1),cy+sz*sinf(a1),cx+sz*cosf(a2),cy+sz*sinf(a2)});
        }
        for(int d=0;d<2;d++){
          std::vector<S> n2;
          for(auto&s:segs){
            float dx2=s.x2-s.x1,dy2=s.y2-s.y1;
            float ax=s.x1+dx2/3,ay=s.y1+dy2/3,bx=s.x1+dx2*2/3,by=s.y1+dy2*2/3;
            float px2=(s.x1+s.x2)/2-dy2*0.2887f,py2=(s.y1+s.y2)/2+dx2*0.2887f;
            n2.push_back({s.x1,s.y1,ax,ay}); n2.push_back({ax,ay,px2,py2});
            n2.push_back({px2,py2,bx,by}); n2.push_back({bx,by,s.x2,s.y2});
          }
          segs=n2;
        }
        for(auto&s:segs) g.DrawLine(bright,s.x1,s.y1,s.x2,s.y2,nullptr,1.f);
        break;
      }
      case 6: // Fractal tree mini (4 depth)
      {
        struct B{float x,y,a,l;int d;};
        std::vector<B> stk; stk.push_back({cx,r.B-2.f,-90.f,sz*1.2f,0});
        while(!stk.empty()){
          auto b=stk.back();stk.pop_back();
          if(b.d>4||b.l<1.5f)continue;
          float rad=b.a*3.14159f/180.f,ex=b.x+b.l*cosf(rad),ey=b.y+b.l*sinf(rad);
          g.DrawLine(b.d<2?bright:dim,b.x,b.y,ex,ey,nullptr,1.f);
          stk.push_back({ex,ey,b.a-28.f,b.l*0.65f,b.d+1});
          stk.push_back({ex,ey,b.a+28.f,b.l*0.65f,b.d+1});
        }
        break;
      }
      case 7: // H-tree mini (Marshall JMP 2203)
      {
        auto drawH = [&](auto&& self, float x, float y, float half, int dep) -> void {
          if (dep <= 0 || half < 0.8f) return;
          float x0 = x - half, x1 = x + half;
          float y0 = y - half, y1 = y + half;
          const IColor& col = (dep >= 3) ? bright : dim;
          g.DrawLine(col, x0, y, x1, y, nullptr, 1.f);
          g.DrawLine(col, x0, y0, x0, y1, nullptr, 1.f);
          g.DrawLine(col, x1, y0, x1, y1, nullptr, 1.f);
          float nh = half * 0.5f;
          self(self, x0, y0, nh, dep - 1);
          self(self, x0, y1, nh, dep - 1);
          self(self, x1, y0, nh, dep - 1);
          self(self, x1, y1, nh, dep - 1);
        };
        float half = std::min(r.W(), r.H()) * 0.42f;
        drawH(drawH, cx, cy, half, 4);
        break;
      }
      case 8: // Levy C mini (6 depth)
      {
        struct S{float x1,y1,x2,y2;};
        std::vector<S> segs; segs.push_back({cx-sz,cy+sz*0.3f,cx+sz,cy+sz*0.3f});
        for(int d=0;d<6;d++){
          std::vector<S> n2;
          for(auto&s:segs){float mx=(s.x1+s.x2)/2+(s.y2-s.y1)/2,my=(s.y1+s.y2)/2-(s.x2-s.x1)/2;n2.push_back({s.x1,s.y1,mx,my});n2.push_back({mx,my,s.x2,s.y2});}
          segs=n2;
        }
        for(auto&s:segs) g.DrawLine(bright,s.x1,s.y1,s.x2,s.y2,nullptr,0.8f);
        break;
      }
      case 9: // Mandelbrot mini (pixel grid)
      {
        float step=2.f;
        for(float px=r.L+1;px<r.R-1;px+=step)for(float py=r.T+1;py<r.B-1;py+=step){
          double cr=-0.745+((px-r.L)/r.W()-0.5)*0.008,ci=0.186+((py-r.T)/r.H()-0.5)*0.008;
          double zr=0,zi=0;int it=0;while(zr*zr+zi*zi<4&&it<30){double t=zr*zr-zi*zi+cr;zi=2*zr*zi+ci;zr=t;it++;}
          if(it<30&&it>3) g.FillRect(IColor(it*8,80+it*3,180+it*2,220),IRECT(px,py,px+step-0.5f,py+step-0.5f));
        }
        break;
      }
      case 10: // Julia mini
      {
        float step=2.f;
        for(float px=r.L+1;px<r.R-1;px+=step)for(float py=r.T+1;py<r.B-1;py+=step){
          double zr=((px-r.L)/r.W()-0.5)*3,zi=((py-r.T)/r.H()-0.5)*2.4;int it=0;
          while(zr*zr+zi*zi<4&&it<25){double t=zr*zr-zi*zi-0.7;zi=2*zr*zi+0.27015;zr=t;it++;}
          if(it<25&&it>2) g.FillRect(IColor(it*10,70+it*4,160+it*4,220),IRECT(px,py,px+step-0.5f,py+step-0.5f));
        }
        break;
      }
      case 11: // Clifford attractor mini (Sebago)
      {
        const double a = -1.4, b = 1.6, c = 1.0, d = 0.75;
        double x = 0.0, y = 0.0;
        float scale = std::min(r.W(), r.H()) * 0.21f;
        for (int i = 0; i < 2200; i++)
        {
          double nx = sin(a * y) + c * cos(a * x);
          double ny = sin(b * x) + d * cos(b * y);
          x = nx;
          y = ny;
          if (i < 120) continue;
          float px = cx + (float)x * scale;
          float py = cy - (float)y * scale;
          if (px < r.L || px > r.R || py < r.T || py > r.B) continue;
          int al = 70 + (i % 90);
          g.FillRect(IColor(al, 90 + (i % 80), 175 + (i % 70), 220), IRECT(px, py, px + 1.f, py + 1.f));
        }
        break;
      }
      case 12: // Burning Ship mini
      {
        float step=2.f;
        for(float px=r.L+1;px<r.R-1;px+=step)for(float py=r.T+1;py<r.B-1;py+=step){
          double cr=-1.75+((px-r.L)/r.W())*0.15,ci=-0.08+((py-r.T)/r.H())*0.12;
          double zr=0,zi=0;int it=0;while(zr*zr+zi*zi<4&&it<30){double t=zr*zr-zi*zi+cr;zi=fabs(2*zr*zi)+ci;zr=t;it++;}
          if(it<30&&it>2) g.FillRect(IColor(it*8,100+it*3,170+it*2,230),IRECT(px,py,px+step-0.5f,py+step-0.5f));
        }
        break;
      }
      case 13: // Pentaflake mini (2 depth)
      {
        struct P{float x,y,r;};
        std::vector<P> ps; ps.push_back({cx,cy,sz});
        for(int d=0;d<2;d++){
          std::vector<P> n2;
          for(auto&p:ps){
            for(int i=0;i<5;i++){float a1=(i*72.f-90)*3.14159f/180,a2=((i+1)*72.f-90)*3.14159f/180;
              g.DrawLine(d==0?bright:dim,p.x+p.r*cosf(a1),p.y+p.r*sinf(a1),p.x+p.r*cosf(a2),p.y+p.r*sinf(a2),nullptr,1.f);}
            float nr=p.r*0.382f;n2.push_back({p.x,p.y,nr});
            for(int i=0;i<5;i++){float a=(i*72.f-90)*3.14159f/180;n2.push_back({p.x+(p.r-nr)*cosf(a),p.y+(p.r-nr)*sinf(a),nr});}
          }
          ps=n2;
        }
        break;
      }
      case 14: // Logistic Bifurcation mini
      {
        float xMin = 2.4f, xMax = 4.0f;
        for (int i = 0; i < 400; i++) {
            float r = xMin + (xMax - xMin) * (i / 400.0f);
            float x = 0.5f;
            for (int j = 0; j < 40; j++) x = r * x * (1.f - x);
            for (int j = 0; j < 10; j++) {
                x = r * x * (1.f - x);
                float px = cx - sz + (i / 400.0f) * 2.f * sz;
                float py = cy + sz - x * 2.f * sz;
                g.FillRect(dim, IRECT(px, py, px+1.f, py+1.f));
            }
        }
        break;
      }
      case 15: // Lichtenberg mini
      {
        struct Pt { float x, y; };
        std::vector<Pt> pts;
        pts.push_back({cx, cy + sz});
        unsigned int rng = 12345;
        for (int i = 0; i < 150; i++) {
            rng = rng * 1103515245 + 12345;
            int parentIdx = rng % pts.size();
            float angle = -90.f + (float)(rng % 120) - 60.f;
            float rad = angle * 3.14159f / 180.f;
            float len = sz * 0.2f;
            Pt next = {pts[parentIdx].x + len * cosf(rad), pts[parentIdx].y + len * sinf(rad)};
            g.DrawLine(dim, pts[parentIdx].x, pts[parentIdx].y, next.x, next.y, nullptr, 1.f);
            pts.push_back(next);
        }
        break;
      }
      case 16: // Cantor Dust mini
      {
        auto drawC = [&](auto&& self, float x, float y, float len, int d) -> void {
            if (d == 0) { g.DrawLine(dim, x, y, x + len, y, nullptr, 1.f); return; }
            float nl = len / 3.f;
            self(self, x, y - 2.f, nl, d - 1);
            self(self, x + 2.f * nl, y + 2.f, nl, d - 1);
        };
        drawC(drawC, cx - sz, cy, sz * 2.f, 3);
        break;
      }
    }
  }

  int mNumAmps = 0;
  int mSelected = 0;
  int mHovered = -1;
  std::vector<std::string> mAmpNames;
  std::vector<std::string> mAmpAbbrs;
  SelectionCallback mCallback;
  std::vector<ILayerPtr> mIconLayers;
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
    IText sectionText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const float directLblW = 50.f;
    const float cabLblW = 66.f;

    float totalW = directLblW + labelGap + btnW + divGap + cabLblW + labelGap + 3 * btnW + 2 * gap;
    float x = mRECT.MW() - totalW / 2.f;
    float btnY = mRECT.MH() - btnH / 2.f;
    // IV/Josefin bold caps sit visually high in short rects — nudge text area down for optical centering
    const float btnTextNudgeY = 3.f;

    // "DIRECT" label
    IRECT directLabel(x, btnY + btnTextNudgeY, x + directLblW, btnY + btnH);
    g.DrawText(sectionText, "DIRECT", directLabel);
    x += directLblW + labelGap;

    // AMP button (index 0) -- teal/cyan accent when active
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (0 == mSelected);
      g.FillRoundRect(isOn ? VoLumColors::BTN_AMP_ON_BG : VoLumColors::BTN_OFF_BG, btn, 3.f);
      g.DrawRoundRect(isOn ? VoLumColors::BTN_AMP_ON_BORDER : VoLumColors::BTN_OFF_BORDER, btn, 3.f);
      IText btnText(14.f, isOn ? VoLumColors::BTN_AMP_ON_TEXT : VoLumColors::BTN_OFF_TEXT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnText, labels[0], IRECT(btn.L, btn.T + btnTextNudgeY, btn.R, btn.B));
      mBtnRects[0] = btn;
      x += btnW;
    }

    // Divider
    float divX = x + divGap / 2.f;
    g.DrawLine(IColor(60, 200, 162, 78), divX, btnY + 4.f, divX, btnY + btnH - 4.f);
    x += divGap;

    // "CABINET" label
    IRECT cabLabel(x, btnY + btnTextNudgeY, x + cabLblW, btnY + btnH);
    g.DrawText(sectionText, "CABINET", cabLabel);
    x += cabLblW + labelGap;

    // G12, G65, V30 buttons (indices 1-3) -- gold accent when active
    for (int i = 1; i < 4; i++)
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (i == mSelected);
      g.FillRoundRect(isOn ? VoLumColors::BTN_CAB_ON_BG : VoLumColors::BTN_OFF_BG, btn, 3.f);
      g.DrawRoundRect(isOn ? VoLumColors::BTN_CAB_ON_BORDER : VoLumColors::BTN_OFF_BORDER, btn, 3.f);
      IColor cabTextCol = isOn ? VoLumColors::BTN_AMP_ON_TEXT : VoLumColors::BTN_OFF_TEXT;
      IText btnTextCab(14.f, cabTextCol, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnTextCab, labels[i], IRECT(btn.L, btn.T + btnTextNudgeY, btn.R, btn.B));
      mBtnRects[i] = btn;
      x += btnW + gap;
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ClearVoLumKnobSelection(this);

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
      // Cached procedural art: only recompute when bounds or amp index change
      if (!mArtLayer || g.CheckLayer(mArtLayer) || mCachedArtIdx != mAmpIdx)
      {
        g.StartLayer(this, mRECT);
        g.FillRect(VoLumColors::HERO_BG, mRECT);
        g.DrawRect(VoLumColors::HERO_BORDER, mRECT);
        const float cs = 16.f;
        const float m = 6.f;
        DrawCornerAccent(g, mRECT.L + m, mRECT.T + m, cs, false, false, VoLumColors::HERO_CORNER);
        DrawCornerAccent(g, mRECT.R - m, mRECT.T + m, cs, true, false, VoLumColors::HERO_CORNER);
        DrawCornerAccent(g, mRECT.L + m, mRECT.B - m, cs, false, true, VoLumColors::HERO_CORNER);
        DrawCornerAccent(g, mRECT.R - m, mRECT.B - m, cs, true, true, VoLumColors::HERO_CORNER);
        DrawGeometricArt(g);
        mArtLayer = g.EndLayer();
        mCachedArtIdx = mAmpIdx;
      }
      g.DrawLayer(mArtLayer);
    }
  }

  void SetPlaceholder(const char* text, int ampIdx = 0)
  {
    mPlaceholder = text;
    if (mAmpIdx != ampIdx)
      mArtLayer.reset();
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
      case 7: // H-tree (Marshall JMP 2203 — centered in hero)
      {
        auto drawH = [&](auto&& self, float x, float y, float half, int dep) -> void {
          if (dep <= 0 || half < 2.f) return;
          float x0 = x - half, x1 = x + half;
          float y0 = y - half, y1 = y + half;
          IColor lc = (dep >= 4) ? bright : ((dep >= 2) ? mid : dim);
          g.DrawLine(lc, x0, y, x1, y, nullptr, (dep >= 3) ? tk : tkThin);
          g.DrawLine(lc, x0, y0, x0, y1, nullptr, (dep >= 3) ? tk : tkThin);
          g.DrawLine(lc, x1, y0, x1, y1, nullptr, (dep >= 3) ? tk : tkThin);
          float nh = half * 0.5f;
          self(self, x0, y0, nh, dep - 1);
          self(self, x0, y1, nh, dep - 1);
          self(self, x1, y0, nh, dep - 1);
          self(self, x1, y1, nh, dep - 1);
        };
        float maxHalf = std::min(w * 0.38f, h * 0.42f);
        drawH(drawH, cx, cy, maxHalf, 6);
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
      case 11: // Clifford attractor (Sebago — fills rectangular viewport)
      {
        const double a = -1.4, b = 1.6, c = 1.0, d = 0.75;
        double x = 0.0, y = 0.0;
        float plotW = w * 0.88f, plotH = h * 0.88f;
        float scale = std::min(plotW, plotH) * 0.32f;
        for (int i = 0; i < 48000; i++)
        {
          double nx = sin(a * y) + c * cos(a * x);
          double ny = sin(b * x) + d * cos(b * y);
          x = nx;
          y = ny;
          if (i < 800) continue;
          float px = cx + (float)x * scale;
          float py = cy - (float)y * scale;
          if (px < mRECT.L + 4.f || px > mRECT.R - 4.f || py < mRECT.T + 4.f || py > mRECT.B - 4.f) continue;
          int alpha = 28 + (i & 95);
          float dot = 1.f + (float)((i >> 3) % 3) * 0.35f;
          g.FillRect(IColor(alpha, 75 + (i % 90), 165 + (i % 85), 215), IRECT(px, py, px + dot, py + dot));
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
      case 14: // Logistic Bifurcation
      {
        float xMin = 2.4f, xMax = 4.0f;
        float plotW = w * 0.8f, plotH = h * 0.8f;
        float plotL = cx - plotW / 2.f, plotT = cy - plotH / 2.f;
        for (int i = 0; i < 8000; i++) {
            float r = xMin + (xMax - xMin) * (i / 8000.0f);
            float x = 0.5f;
            for (int j = 0; j < 100; j++) x = r * x * (1.f - x); // settle
            for (int j = 0; j < 30; j++) {
                x = r * x * (1.f - x);
                float px = plotL + (i / 8000.0f) * plotW;
                float py = plotT + plotH - x * plotH;
                int alpha = 40 + (j % 4) * 20;
                g.FillRect(IColor(alpha, 80, 200, 180), IRECT(px, py, px+1.f, py+1.f));
            }
        }
        break;
      }
      case 15: // Lichtenberg
      {
        struct Pt { float x, y; };
        std::vector<Pt> pts;
        pts.push_back({cx, mRECT.B - 15.f});
        unsigned int rng = 12345;
        for (int i = 0; i < 2000; i++) {
            rng = rng * 1103515245 + 12345;
            int parentIdx = rng % pts.size();
            float angle = -90.f + (float)(rng % 180) - 90.f;
            float rad = angle * 3.14159f / 180.f;
            float len = 8.f;
            Pt next = {pts[parentIdx].x + len * cosf(rad), pts[parentIdx].y + len * sinf(rad)};
            
            if (next.x > mRECT.L + 10.f && next.x < mRECT.R - 10.f && next.y > mRECT.T + 10.f && next.y < mRECT.B - 10.f) {
                IColor col = (i < 100) ? bright : ((i < 500) ? mid : dim);
                g.DrawLine(col, pts[parentIdx].x, pts[parentIdx].y, next.x, next.y, nullptr, (i < 200) ? tk : tkThin);
                pts.push_back(next);
            }
        }
        break;
      }
      case 16: // Cantor Dust
      {
        auto drawC = [&](auto&& self, float x, float y, float len, int d, float yOffset) -> void {
            if (d == 0) { 
                g.DrawLine(dim, x, y, x + len, y, nullptr, tkThin); 
                return; 
            }
            float nl = len / 3.f;
            IColor col = (d >= 4) ? bright : ((d >= 2) ? mid : dim);
            g.DrawLine(col, x, y, x + len, y, nullptr, (d >= 3) ? tk : tkThin);
            self(self, x, y - yOffset, nl, d - 1, yOffset * 0.8f);
            self(self, x + 2.f * nl, y + yOffset, nl, d - 1, yOffset * 0.8f);
        };
        drawC(drawC, cx - w * 0.35f, cy, w * 0.7f, 5, h * 0.15f);
        break;
      }
    }

  }

  bool mHasBitmap = false;
  IBitmap mBitmap;
  std::string mPlaceholder = "A1";
  std::string mName = "Ampete One";
  int mAmpIdx = 0;
  ILayerPtr mArtLayer;
  int mCachedArtIdx = -1;
};

class VoLumModePickerControl : public IControl
{
public:
  VoLumModePickerControl(const IRECT& bounds, int paramIdx, const std::vector<std::string>& modes)
  : IControl(bounds, paramIdx)
  , mModes(modes)
  {}

  void Draw(IGraphics& g) override
  {
    int selected = static_cast<int>(GetValue() * (mModes.size() - 1));
    float itemH = mRECT.H() / static_cast<float>(mModes.size());

    // Left border
    g.DrawLine(VoLumColors::FRAME, mRECT.L, mRECT.T, mRECT.L, mRECT.B, nullptr, 1.f);

    for (size_t i = 0; i < mModes.size(); ++i)
    {
      IRECT itemArea = IRECT(mRECT.L + 12.f, mRECT.T + i * itemH, mRECT.R, mRECT.T + (i + 1) * itemH);
      bool isSelected = (i == selected);

      if (isSelected) {
        g.FillRect(VoLumColors::AMBER, itemArea.GetPadded(-1.f));
      }

      IColor textCol = isSelected ? IColor(255, 26, 18, 8) : VoLumColors::CREAM_DIM;
      IText text(10.f, textCol, "Michroma-Regular", EAlign::Near, EVAlign::Middle);
      IRECT textArea = itemArea;
      textArea.L += 6.f; // manually indent instead of GetTranslated which shifts the whole rect
      g.DrawText(text, mModes[i].c_str(), textArea);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    float itemH = mRECT.H() / static_cast<float>(mModes.size());
    int clickedIdx = static_cast<int>((y - mRECT.T) / itemH);
    if (clickedIdx >= 0 && clickedIdx < static_cast<int>(mModes.size()))
    {
      SetValue(static_cast<double>(clickedIdx) / (mModes.size() - 1));
      SetDirty(true);
    }
  }

private:
  std::vector<std::string> mModes;
};

class VoLumSubRowTextControl : public IControl
{
public:
  VoLumSubRowTextControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (mName.empty()) return;
    const IRECT nameArea = IRECT(mRECT.L + 18.f, mRECT.T + 8.f, mRECT.R - 18.f, mRECT.T + 36.f);

    // If it's the Amp name, use gold. If effect, use cream
    IColor col = mIsAmp ? VoLumColors::GOLD : VoLumColors::CREAM;
    g.DrawText(IText(21.f, col, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               mName.c_str(), nameArea);

    // Gold divider with diamond below the name
    float cy = nameArea.B + 8.f;
    float cx = mRECT.MW();
    g.DrawLine(IColor(51, 200, 162, 78), cx - 50.f, cy, cx - 6.f, cy);
    g.DrawLine(IColor(51, 200, 162, 78), cx + 6.f, cy, cx + 50.f, cy);
    DrawDiamond(g, cx, cy, 3.f, VoLumColors::GOLD_DIM);
  }

  void SetName(const char* name, bool isAmp)
  {
    mName = name;
    mIsAmp = isAmp;
    SetDirty(false);
  }

private:
  std::string mName;
  bool mIsAmp = true;
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
    const IRECT nameArea = IRECT(mRECT.L + 18.f, mRECT.T + 8.f, mRECT.R - 18.f, mRECT.T + 36.f);
    g.DrawText(IText(21.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               mName.c_str(), nameArea);

    // Gold divider with diamond below the name
    float cy = nameArea.B + 8.f;
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
    const auto align = EAlign::Center;
    const float size = mIsChannel ? 12.f : 13.f;
    IText text(size, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", align, EVAlign::Middle);
    IRECT area = mRECT;
    g.DrawText(text, mLabel.c_str(), area);
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
    IText text(11.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    int len = (int)mLabel.size();
    float charH = 12.f;
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

/** Thin horizontal hairline (full-width segment in layout). */
class VoLumHorizontalRuleControl : public IControl
{
public:
  explicit VoLumHorizontalRuleControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(72, 200, 162, 78), mRECT);
  }
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
    IText text(12.5f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
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
    IColor leftCol = mMouseOverLeft ? VoLumColors::GOLD : VoLumColors::TEXT_BRIGHT;
    IText arrowText(17.f, leftCol, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowText, "<", leftArea);

    // Right arrow: >
    IRECT rightArea(mRECT.R - arrowW, mRECT.T, mRECT.R, mRECT.B);
    IColor rightCol = mMouseOverRight ? VoLumColors::GOLD : VoLumColors::TEXT_BRIGHT;
    IText arrowTextR(17.f, rightCol, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowTextR, ">", rightArea);

    // Center: channel number
    IRECT center(mRECT.L + arrowW, mRECT.T, mRECT.R - arrowW, mRECT.B);
    const char* label = (n > 0 && mSelected >= 0 && mSelected < n) ? mLabels[mSelected].c_str() : "---";
    IText labelText(19.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(labelText, label, center);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ClearVoLumKnobSelection(this);

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

class VoLumKeyboardHintControl : public IControl
{
public:
  explicit VoLumKeyboardHintControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (mHintText.empty())
      return;

    g.FillRoundRect(IColor(168, 14, 16, 22), mRECT, 7.f);
    g.DrawRoundRect(IColor(72, 200, 162, 78), mRECT, 7.f, nullptr, 1.f);

    const IText titleText(12.5f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const IText detailText(11.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Center, EVAlign::Middle);

    const IRECT inner = mRECT.GetPadded(-18.f, -6.f, -18.f, -6.f);
    const IRECT top = inner.GetFromTop(17.f);
    const IRECT bottom = IRECT(inner.L, top.B + 2.f, inner.R, inner.B);
    g.DrawText(titleText, mHintTitle.c_str(), top);
    g.DrawText(detailText, mHintDetail.c_str(), bottom);
  }

  void SetHintText(const char* hintText)
  {
    mHintText = (hintText && hintText[0]) ? hintText : "";
    mHintTitle.clear();
    mHintDetail.clear();

    if (!mHintText.empty())
    {
      const std::string divider = "  |  ";
      const auto firstSplit = mHintText.find(divider);
      if (firstSplit == std::string::npos)
      {
        mHintTitle = mHintText;
      }
      else
      {
        mHintTitle = mHintText.substr(0, firstSplit);
        mHintDetail = mHintText.substr(firstSplit + divider.size());
      }
    }

    SetDirty(false);
  }

private:
  std::string mHintText;
  std::string mHintTitle;
  std::string mHintDetail;
};

class VoLumExactEntryControl : public IControl
{
public:
  VoLumExactEntryControl(const IRECT& bounds, int paramIdx, const char* label = "")
  : IControl(bounds, paramIdx)
  , mLabel(label ? label : "")
  {
    mIgnoreMouse = false;
    mText = IText(22.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle, 0.f,
                  IColor(245, 14, 16, 22), VoLumColors::TEXT_BRIGHT);
    SetTextEntryLength(12);
  }

  void Draw(IGraphics& g) override
  {
    if (mHide)
      return;

    g.FillRect(IColor(182, 8, 10, 14), mRECT);

    const IRECT panel = GetPanelRect();
    const IRECT frame = panel.GetPadded(10.f);
    const IRECT entry = GetEntryRect();
    const IRECT titleRect(panel.L + 24.f, panel.T + 18.f, panel.R - 24.f, panel.T + 38.f);
    const IRECT rangeRect(panel.L + 24.f, titleRect.B + 8.f, panel.R - 24.f, titleRect.B + 26.f);
    const IRECT hintRect(panel.L + 24.f, entry.B + 10.f, panel.R - 24.f, entry.B + 26.f);

    g.FillRoundRect(IColor(255, 22, 22, 30), panel, 10.f);
    g.DrawRoundRect(VoLumColors::FRAME, panel, 10.f, nullptr, 1.2f);
    g.DrawRoundRect(IColor(90, 200, 180, 100), frame, 8.f, nullptr, 1.f);

    DrawCornerAccent(g, panel.L + 11.f, panel.T + 11.f, 16.f, false, false);
    DrawCornerAccent(g, panel.R - 11.f, panel.T + 11.f, 16.f, true, false);
    DrawCornerAccent(g, panel.L + 11.f, panel.B - 11.f, 16.f, false, true);
    DrawCornerAccent(g, panel.R - 11.f, panel.B - 11.f, 16.f, true, true);

    g.DrawText(IText(14.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               mLabel.empty() ? "EXACT VALUE" : mLabel.c_str(), titleRect);
    g.DrawText(IText(11.5f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               mRangeText.c_str(), rangeRect);

    g.FillRoundRect(IColor(235, 14, 16, 22), entry, 6.f);
    g.DrawRoundRect(IColor(72, 200, 162, 78), entry, 6.f, nullptr, 1.f);
    g.DrawRoundRect(IColor(36, 200, 162, 78), entry.GetPadded(2.f), 5.f, nullptr, 1.f);

    g.DrawText(IText(10.5f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               "Type a number, press Enter to apply, Esc to cancel", hintRect);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mHide)
      return;

    auto* ui = GetUI();
    if (!ui)
      return;

    if (!GetPanelRect().Contains(x, y))
    {
      Hide(true);
      if (auto* textEntry = ui->GetTextEntryControl())
        textEntry->DismissEdit();
      return;
    }

    if (!mEditing && GetEntryRect().Contains(x, y))
      StartEntry();
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    mEditing = false;
    Hide(true);
    IControl::OnTextEntryCompletion(str, valIdx);
  }

  void SetValueFromUserInput(double value, int valIdx) override
  {
    mEditing = false;
    Hide(true);
    IControl::SetValueFromUserInput(value, valIdx);
  }

  void SetLabel(const char* label)
  {
    mLabel = label ? label : "";
    SetDirty(false);
  }

  void ShowForParam(int paramIdx, const char* label = nullptr)
  {
    SetParamIdx(paramIdx);
    SetLabel(label);
    BuildRangeText();
    Hide(false);
    mEditing = false;
    SetDirty(false);
  }

  void StartEntry()
  {
    if (mHide)
      return;

    auto* ui = GetUI();
    if (!ui)
      return;

    WDL_String currentText;
    if (const auto* pParam = GetParam())
      pParam->GetDisplay(currentText, false);

    mEditing = true;
    BuildRangeText();
    ui->CreateTextEntry(*this, mText, GetEntryRect(), currentText.Get());
    SetDirty(false);
  }

  bool IsEditing() const { return mEditing; }

  void SyncTextEntryState()
  {
    auto* ui = GetUI();
    if (!ui)
      return;

    if (mEditing && ui->GetControlInTextEntry() != this)
    {
      mEditing = false;
      Hide(true);
      SetDirty(false);
    }
  }

  void Hide(bool hide) override
  {
    IControl::Hide(hide);
    if (hide)
      mEditing = false;
  }

private:
  IRECT GetPanelRect() const
  {
    return mRECT.GetCentredInside(340.f, 156.f);
  }

  IRECT GetEntryRect() const
  {
    const IRECT panel = GetPanelRect();
    return IRECT(panel.L + 28.f, panel.T + 64.f, panel.R - 28.f, panel.T + 106.f);
  }

  void BuildRangeText()
  {
    mRangeText.clear();
    if (const auto* pParam = GetParam())
    {
      WDL_String minText;
      WDL_String maxText;
      pParam->GetDisplay(pParam->GetMin(), false, minText, false);
      pParam->GetDisplay(pParam->GetMax(), false, maxText, false);
      mRangeText = "Range ";
      mRangeText += minText.Get();
      mRangeText += " to ";
      mRangeText += maxText.Get();
      if (const char* label = pParam->GetLabel(); label && label[0])
      {
        mRangeText += " ";
        mRangeText += label;
      }
    }
  }

  std::string mLabel;
  std::string mRangeText;
  bool mEditing = false;
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

    IText text(13.5f, VoLumColors::TEXT_BRIGHT, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
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

/** Full-window dim + explicit panel rect (must match layout math in NAMSettingsPageControl). */
class VoLumSettingsBackdropControl : public IControl
{
public:
  VoLumSettingsBackdropControl(const IRECT& fullBounds, const IRECT& panelRect)
  : IControl(fullBounds)
  , mPanel(panelRect)
  {
    // Must receive hits: if ignored, dim/panel “empty” pixels fall through to main UI and can steal
    // mouse up/down when the cursor moves quickly (settings appears to close at random).
    mIgnoreMouse = false;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;
    // Consume clicks on overlay shell (dim + filler); interactive children sit above in z-order.
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(185, 8, 10, 14), mRECT);
    const IRECT& p = mPanel;
    // Slightly lifted panel so it reads clearly over the dim layer
    g.FillRect(IColor(255, 22, 22, 30), p);
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
    // Match ui-mockup/settings-overlay-mockup.html output card (group-fill + gold border + inner hairline).
    g.FillRect(IColor(235, 20, 20, 26), mRECT);
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

/** Gold “×” close control (no grey SVG) for the settings overlay. */
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
    (void) x;
    (void) y;
    (void) mod;
    if (mCloseAction)
      mCloseAction(this);
  }

private:
  IActionFunction mCloseAction;
};

//==============================================================================
// Reverb & Delay Extension Controls (PRE / AMP / POST)
//==============================================================================

#include "VoLumTriptychState.h"

// Forward declare DrawMiniFractal if needed, or we can just keep the layout logic in _AttachVoLumGraphics
// and use simple controls. Let's make VoLumTriptychControl very simple.

class VoLumChainConnectorControl : public IControl
{
public:
  VoLumChainConnectorControl(const IRECT& bounds) : IControl(bounds) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    g.DrawLine(VoLumColors::TEAL.WithOpacity(0.55f), mRECT.L, mRECT.MH(), mRECT.R, mRECT.MH(), nullptr, 1.f);
    g.FillCircle(VoLumColors::TEAL, mRECT.MW(), mRECT.MH(), 2.f);
  }
};

class VoLumTriptychControl : public IControl
{
public:
  using StateCallback = std::function<void(EVoLumSection, EVoLumEffectFocus)>;

  VoLumTriptychControl(const IRECT& bounds, StateCallback cb)
  : IControl(bounds)
  , mCallback(std::move(cb))
  {}

  void Draw(IGraphics& g) override
  {
    const float stripW = 30.f;
    const EVoLumSection displaySection =
      (mExpandedSection == EVoLumSection::PRE) ? EVoLumSection::AMP : mExpandedSection;
    const float expandedW = (displaySection == EVoLumSection::AMP) ? 400.f : 460.f;
    const float gap = 10.f;
    const float cx = mRECT.MW();

    // Calculate layout
    IRECT preRect, ampRect, postRect;

    if (displaySection == EVoLumSection::AMP)
    {
      const float totalW = stripW + gap + expandedW + gap + stripW;
      const float left = cx - totalW / 2.f;
      preRect = IRECT(left, mRECT.T, left + stripW, mRECT.B);
      ampRect = IRECT(preRect.R + gap, mRECT.T, preRect.R + gap + expandedW, mRECT.B);
      postRect = IRECT(ampRect.R + gap, mRECT.T, ampRect.R + gap + stripW, mRECT.B);
    }
    else // POST
    {
      const float ampStripW = 70.f;
      const float preStripW = stripW;
      const float totalW = preStripW + gap + ampStripW + gap + expandedW;
      const float left = cx - totalW / 2.f;
      preRect = IRECT(left, mRECT.T, left + preStripW, mRECT.B);
      ampRect = IRECT(preRect.R + gap, mRECT.T, preRect.R + gap + ampStripW, mRECT.B);
      postRect = IRECT(ampRect.R + gap, mRECT.T, ampRect.R + gap + expandedW, mRECT.B);
    }

    mPreRect = preRect;
    mAmpRect = ampRect;
    mPostRect = postRect;

    // Draw the sections
    _DrawStrip(g, preRect, "PRE", false, false, false);
    
    // AMP: we draw the strip, or we draw the hero frame
    if (displaySection == EVoLumSection::AMP) {
      // Just the frame, the VoLumHeroImageControl will draw on top (it's attached)
      // Actually we must resize the HeroImageControl!
    } else {
      _DrawAmpStrip(g, ampRect);
    }

    _DrawStrip(g, postRect, "POST", displaySection == EVoLumSection::POST, mPostActive, true);
  }
  
  void SetState(bool preActive, bool postActive, int ampIdx, const char* ampName)
  {
    (void) preActive;
    mPreActive = false;
    mPostActive = postActive;
    mAmpIdx = ampIdx;
    mAmpName = ampName;
    SetDirty(false);
  }
  
  void SetExpandedSection(EVoLumSection s)
  {
    mExpandedSection = (s == EVoLumSection::PRE) ? EVoLumSection::AMP : s;
    SetDirty(false);
  }
  
  IRECT GetPostExpandedRect() const { return mPostRect; }
  IRECT GetPreExpandedRect() const { return mPreRect; }
  
private:
  void _DrawStrip(IGraphics& g, const IRECT& r, const char* label, bool expanded, bool active, bool isPost)
  {
    if (expanded)
    {
      g.FillRect(IColor(255, 22, 25, 33), r);
      g.DrawRect(VoLumColors::FRAME, r);
      const float cs = 8.f;
      DrawCornerAccent(g, r.L + 4.f, r.T + 4.f, cs, false, false, VoLumColors::TEAL_DIM);
      DrawCornerAccent(g, r.R - 4.f, r.T + 4.f, cs, true, false, VoLumColors::TEAL_DIM);
      DrawCornerAccent(g, r.L + 4.f, r.B - 4.f, cs, false, true, VoLumColors::TEAL_DIM);
      DrawCornerAccent(g, r.R - 4.f, r.B - 4.f, cs, true, true, VoLumColors::TEAL_DIM);
      IText txt(9.f, VoLumColors::TEAL, "Michroma-Regular", EAlign::Near, EVAlign::Middle);
      g.DrawText(txt, label, r.GetPadded(-8.f).GetFromTop(20.f).GetTranslated(16.f, 0.f));
      g.FillCircle(VoLumColors::TEAL, r.L + 12.f, r.T + 14.f, 3.f);
    }
    else
    {
      bool dormant = !active;
      if (dormant)
      {
        g.DrawRect(IColor(153, 43, 47, 55), r);
        const float cs = 6.f;
        DrawCornerAccent(g, r.L + 3.f, r.T + 3.f, cs, false, false, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
        DrawCornerAccent(g, r.R - 3.f, r.T + 3.f, cs, true, false, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
        DrawCornerAccent(g, r.L + 3.f, r.B - 3.f, cs, false, true, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
        DrawCornerAccent(g, r.R - 3.f, r.B - 3.f, cs, true, true, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
      }
      else
      {
        g.FillRect(IColor(255, 22, 25, 33), r); // bg-strip
        g.DrawRect(VoLumColors::FRAME, r);
      }

      g.FillCircle(active ? VoLumColors::TEAL : IColor(255, 42, 48, 52), r.MW(), r.T + 12.f, 3.5f);

      IText t(dormant ? 8.f : 10.f, dormant ? VoLumColors::CREAM_DIM : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      // Vertical text drawing fallback
      float charH = 12.f;
      float totalH = strlen(label) * charH;
      float ty = r.MH() - totalH / 2.0f;
      for (size_t i = 0; i < strlen(label); i++) {
        char ch[2] = { label[i], 0 };
        g.DrawText(t, ch, IRECT(r.L, ty + i * charH, r.R, ty + (i + 1) * charH));
      }
    }
  }

  void _DrawAmpStrip(IGraphics& g, const IRECT& r)
  {
    g.FillRect(IColor(255, 22, 25, 33), r); // bg-strip
    g.DrawRect(VoLumColors::FRAME, r);
    
    // Draw mini fractal
    // This is a quick mock. We can instantiate an AmpList control or duplicate DrawMiniFractal if needed.
    IText t(10.f, VoLumColors::CREAM, "Michroma-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(t, "A", r.GetFromTop(40.f).GetVShifted(80.f));
    g.DrawText(t, "M", r.GetFromTop(40.f).GetVShifted(95.f));
    g.DrawText(t, "P", r.GetFromTop(40.f).GetVShifted(110.f));

    g.FillCircle(VoLumColors::TEAL, r.MW(), r.B - 12.f, 3.5f);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mAmpRect.Contains(x, y) && mExpandedSection != EVoLumSection::AMP)
    {
      mExpandedSection = EVoLumSection::AMP;
      if (mCallback) mCallback(mExpandedSection, EVoLumEffectFocus::AMP);
      SetDirty(false);
    }
    else if (mPostRect.Contains(x, y) && mExpandedSection != EVoLumSection::POST)
    {
      mExpandedSection = EVoLumSection::POST;
      if (mCallback) mCallback(mExpandedSection, EVoLumEffectFocus::DELAY); // Default to Delay when expanding POST
      SetDirty(false);
    }
  }
  
  StateCallback mCallback;
  bool mPreActive = false;
  bool mPostActive = false;
  int mAmpIdx = 0;
  std::string mAmpName;
  EVoLumSection mExpandedSection = EVoLumSection::AMP;

  IRECT mPreRect;
  IRECT mAmpRect;
  IRECT mPostRect;
};

class VoLumPedalCardControl : public IControl
{
public:
  using ClickCallback = std::function<void(VoLumPedalCardControl*, bool isBypassClick)>;

  VoLumPedalCardControl(const IRECT& bounds, EVoLumEffectFocus effect, const char* name, int fractalCase, int activeParamIdx, ClickCallback cb)
  : IControl(bounds, activeParamIdx)
  , mEffect(effect)
  , mName(name)
  , mFractalCase(fractalCase)
  , mActiveParamIdx(activeParamIdx)
  , mCallback(std::move(cb))
  {
  }

  void Draw(IGraphics& g) override
  {
    bool focused = mIsFocused;
    bool bypassed = (GetValue() < 0.5); // Control is bound to active param, so 0 = bypassed, 1 = active

    // Card background
    g.FillRect(IColor(255, 35, 39, 48), mRECT.GetFromTop(mRECT.H() / 2.f));
    g.FillRect(IColor(255, 23, 26, 32), mRECT.GetFromBottom(mRECT.H() / 2.f));
    
    IColor borderCol = focused ? VoLumColors::AMBER : (bypassed ? VoLumColors::FRAME : VoLumColors::TEAL_DIM);
    g.DrawRoundRect(borderCol, mRECT, 4.f);

    // Corners
    const float cs = 8.f;
    DrawCornerAccent(g, mRECT.L + 4.f, mRECT.T + 4.f, cs, false, false, borderCol);
    DrawCornerAccent(g, mRECT.R - 4.f, mRECT.T + 4.f, cs, true, false, borderCol);
    DrawCornerAccent(g, mRECT.L + 4.f, mRECT.B - 4.f, cs, false, true, borderCol);
    DrawCornerAccent(g, mRECT.R - 4.f, mRECT.B - 4.f, cs, true, true, borderCol);

    // Name
    IColor nameCol = focused ? VoLumColors::AMBER : (bypassed ? VoLumColors::CREAM_DIM : VoLumColors::CREAM);
    IText nameTxt(11.f, nameCol, "Michroma-Regular", EAlign::Center, EVAlign::Top);
    const IRECT titleRect(mRECT.L + 12.f, mRECT.T + 8.f, mRECT.R - 12.f, mRECT.T + 24.f);
    g.DrawText(nameTxt, mName.c_str(), titleRect);

    if (focused)
    {
      g.DrawLine(VoLumColors::AMBER, mRECT.L + 28.f, titleRect.B + 2.f, mRECT.R - 28.f, titleRect.B + 2.f);
    }

    // Art box
    IRECT artRect(mRECT.L + 12.f, mRECT.T + 22.f, mRECT.R - 12.f, mRECT.T + 92.f);
    g.DrawRect(IColor(60, 91, 196, 196), artRect, nullptr, 1.f);
    
    // Draw mini fractal inside art box
    IText artTxt(10.f, VoLumColors::TEAL, "Michroma-Regular", EAlign::Center, EVAlign::Middle);
    const char* artName = _GetArtName();
    g.DrawText(artTxt, artName, artRect);

    // Preset Label
    IText presetTxt(10.f, bypassed ? VoLumColors::CREAM_DIM : VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Bottom);
    const std::string presetName = _GetPresetName();
    const IRECT presetRect(mRECT.L + 12.f, mRECT.B - 28.f, mRECT.R - 18.f, mRECT.B - 8.f);
    g.DrawText(presetTxt, presetName.c_str(), presetRect);

    // Bypass LED
    IRECT ledRect(mRECT.R - 14.f, mRECT.B - 14.f, mRECT.R - 6.f, mRECT.B - 6.f);
    g.FillCircle(bypassed ? IColor(255, 42, 48, 52) : VoLumColors::TEAL, ledRect.MW(), ledRect.MH(), 4.f);
    mLedRect = ledRect;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    bool isBypass = mLedRect.Contains(x, y);
    if (mCallback)
      mCallback(this, isBypass);
  }
  
  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    // Make sure we catch hovers so cursor can change, but we don't necessarily need to draw anything
  }

  void SetFocused(bool focused)
  {
    mIsFocused = focused;
    SetDirty(false);
  }

  EVoLumEffectFocus GetEffect() const { return mEffect; }

private:
  const char* _GetArtName() const
  {
    switch (mEffect)
    {
      case EVoLumEffectFocus::DELAY: return "CANTOR DUST";
      case EVoLumEffectFocus::REVERB: return "LICHTENBERG";
      default: return "BYPASS";
    }
  }

  std::string _GetPresetName()
  {
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate());
    if (!plugin)
      return (mEffect == EVoLumEffectFocus::DELAY) ? "DIGITAL . 380 ms" : "HALL . 50%";

    WDL_String modeText;
    WDL_String summary;
    switch (mEffect)
    {
      case EVoLumEffectFocus::DELAY:
        plugin->GetParam(kDelayMode)->GetDisplay(modeText);
        summary.SetFormatted(64, "%s . %.0f ms", modeText.Get(), plugin->GetParam(kDelayTime)->Value());
        return summary.Get();
      case EVoLumEffectFocus::REVERB:
        plugin->GetParam(kReverbMode)->GetDisplay(modeText);
        summary.SetFormatted(64, "%s . %.0f %%", modeText.Get(), plugin->GetParam(kReverbMix)->Value() * 100.0);
        return summary.Get();
      default:
        return "BYPASS";
    }
  }

  EVoLumEffectFocus mEffect;
  std::string mName;
  int mFractalCase;
  int mActiveParamIdx;
  bool mIsFocused = false;
  IRECT mLedRect;
  ClickCallback mCallback;
};

