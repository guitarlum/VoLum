#pragma once

// Procedural fractal art generators extracted from VoLumColorHelpers / VoLumCoreControls.

#include "IControls.h"
#include "VoLumAmpeteCatalog.h"
#include "VoLumColorHelpers.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <unordered_set>

using namespace iplug;
using namespace igraphics;

// ---------------------------------------------------------------------------
// Shared "atmosphere" helpers for the 1.2.0 art glow-up. These mirror the JS
// helpers in tools/art_1_2_0_preview.html (bloom / dust / glow / gradient
// stroke) so hero art gains depth. Canvas radial/linear gradients + shadowBlur
// are emulated with concentric translucent fills and layered-alpha strokes.
// Gold accents are hero/card-size only; callers must not use gold in minis.
// ---------------------------------------------------------------------------
namespace volumart
{
static const IColor kTeal(255, 120, 210, 220), kMid(255, 100, 180, 200), kDim(255, 80, 150, 170),
                    kBlue(255, 150, 205, 245), kGold(255, 200, 165, 87), kGoldHi(255, 252, 222, 145);

inline float Frand(unsigned& s) { s = s * 1664525u + 1013904223u; return (float)s / (float)0xFFFFFFFFu; }

inline IColor WithA(const IColor& c, float a)
{
  int ai = (int)(a * 255.f);
  return IColor(ai < 0 ? 0 : (ai > 255 ? 255 : ai), c.R, c.G, c.B);
}

inline IColor Mix(const IColor& a, const IColor& b, float t)
{
  if (t < 0.f) t = 0.f; else if (t > 1.f) t = 1.f;
  return IColor((int)(a.A + (b.A - a.A) * t), (int)(a.R + (b.R - a.R) * t), (int)(a.G + (b.G - a.G) * t),
                (int)(a.B + (b.B - a.B) * t));
}

// Soft radial bloom. Matches the preview harness's single smooth radial gradient
// (peak at centre -> peak*0.3 at 0.6R -> transparent at R) instead of stacking
// opaque circles, so the light source stays dim and sits in the background with
// no hard centre hotspot.
inline void Bloom(IGraphics& g, float cx, float cy, float R, const IColor& c, float peak)
{
  if (R <= 0.f || peak <= 0.f)
    return;
  const int a0 = std::min(255, (int)(peak * 255.f));
  const int a1 = std::min(255, (int)(peak * 0.3f * 255.f));
  g.PathCircle(cx, cy, R);
  g.PathFill(IPattern::CreateRadialGradient(cx, cy, R,
                                            {{IColor(a0, c.R, c.G, c.B), 0.f},
                                             {IColor(a1, c.R, c.G, c.B), 0.6f},
                                             {IColor(0, c.R, c.G, c.B), 1.f}}));
}

// Drifting dust dots inside a radius. Hero-only (callers gate on big).
inline void Dust(IGraphics& g, unsigned seed, float cx, float cy, float R, int n)
{
  unsigned s = seed;
  for (int i = 0; i < n; i++)
  {
    const float a = Frand(s) * 6.28318f, rr = R * std::sqrt(Frand(s));
    const float px = cx + rr * cosf(a), py = cy + rr * sinf(a);
    const int al = (int)((0.08f + 0.28f * Frand(s)) * 255.f);
    const IColor& c = (Frand(s) > 0.75f) ? kTeal : kMid;
    g.FillCircle(IColor(al, c.R, c.G, c.B), px, py, Frand(s) * 1.3f + 0.3f);
  }
}

// Glowing line: wide faint pass under a bright core.
inline void GlowLine(IGraphics& g, const IColor& glowCol, const IColor& core, float x1, float y1, float x2, float y2,
                     float w, float glowW)
{
  g.DrawLine(WithA(glowCol, (glowCol.A / 255.f) * 0.32f), x1, y1, x2, y2, nullptr, w + glowW);
  g.DrawLine(core, x1, y1, x2, y2, nullptr, w);
}

// Glowing dot: two faint halo passes under a bright core.
inline void GlowDot(IGraphics& g, const IColor& glowCol, const IColor& core, float cx, float cy, float r, float glowR)
{
  g.FillCircle(WithA(glowCol, 0.26f), cx, cy, r + glowR);
  g.FillCircle(WithA(glowCol, 0.45f), cx, cy, r + glowR * 0.5f);
  g.FillCircle(core, cx, cy, r);
}

// Gradient polyline stroke c0->c1 (alpha ~0.85), optional faint teal glow underlay.
inline void GradStroke(IGraphics& g, const std::vector<std::pair<float, float>>& P, const IColor& c0, const IColor& c1,
                       float lw, bool glowOn)
{
  if (glowOn)
    for (size_t i = 1; i < P.size(); i++)
      g.DrawLine(WithA(kTeal, 0.22f), P[i - 1].first, P[i - 1].second, P[i].first, P[i].second, nullptr, lw + 4.f);
  for (size_t i = 1; i < P.size(); i++)
  {
    const float t = (float)i / (float)P.size();
    g.DrawLine(WithA(Mix(c0, c1, t), 0.85f), P[i - 1].first, P[i - 1].second, P[i].first, P[i].second, nullptr, lw);
  }
}
} // namespace volumart

// Map an amp index to a fractal art case. Use this everywhere amp art is drawn so
// that adding amps keeps existing visual identity intact (see kAmpFractalCase).
inline int FractalCaseForAmp(int ampIdx)
{
  if (ampIdx < 0 || ampIdx >= volum::kAmpCount)
    return 0;
  return volum::kAmpFractalCase[ampIdx];
}

// Helper: draw mini fractal art for amp strip/pedal card (idx selects variant)
inline void DrawStripMiniFractal(IGraphics& g, const IRECT& r, int idx,
                                  const IColor& bright = IColor(60, 120, 210, 220),
                                  const IColor& dim = IColor(30, 80, 150, 170))
{
  float cx = r.MW(), cy = r.MH();
  float scale = std::min(r.W(), r.H());
  float sz = scale * 0.42f;
  bool big = (scale > 40.f);
  int depth = big ? 5 : 3;
  int dots = big ? 2000 : 200;
  int curves = big ? 60 : 30;
  float tk = big ? 1.5f : 1.f;
  switch (idx % 15) {
    case 0: { std::vector<int> turns; for(int i=0;i<(big?8:6);i++){std::vector<int> n2;for(auto t:turns)n2.push_back(t);n2.push_back(1);for(int j=(int)turns.size()-1;j>=0;j--)n2.push_back(1-turns[j]);turns=n2;} float step=big?scale*0.012f:1.8f,px2=cx-scale*0.1f,py2=cy+scale*0.06f;int dir=0;const float dx[]={step,0,-step,0},dy[]={0,-step,0,step};for(int i=0;i<(int)turns.size();i++){float nx=px2+dx[dir],ny=py2+dy[dir];g.DrawLine(bright,px2,py2,nx,ny,nullptr,tk);px2=nx;py2=ny;dir=(dir+(turns[i]?1:3))%4;} break; }
    case 1: { struct T{float x1,y1,x2,y2,x3,y3;};std::vector<T> ts;ts.push_back({cx,cy-sz,cx-sz,cy+sz*0.7f,cx+sz,cy+sz*0.7f});for(int d=0;d<depth;d++){std::vector<T> n2;for(auto&t:ts){g.DrawLine(dim,t.x1,t.y1,t.x2,t.y2,nullptr,tk);g.DrawLine(dim,t.x2,t.y2,t.x3,t.y3,nullptr,tk);g.DrawLine(dim,t.x3,t.y3,t.x1,t.y1,nullptr,tk);n2.push_back({t.x1,t.y1,(t.x1+t.x2)/2,(t.y1+t.y2)/2,(t.x3+t.x1)/2,(t.y3+t.y1)/2});n2.push_back({(t.x1+t.x2)/2,(t.y1+t.y2)/2,t.x2,t.y2,(t.x2+t.x3)/2,(t.y2+t.y3)/2});n2.push_back({(t.x3+t.x1)/2,(t.y3+t.y1)/2,(t.x2+t.x3)/2,(t.y2+t.y3)/2,t.x3,t.y3});}ts=n2;} break; }
    case 2: { float px2=0,py2=0;unsigned rng=42;float fscale=scale*0.07f,fh=scale*0.035f;for(int i=0;i<dots;i++){rng=rng*1103515245+12345;float rv=(float)(rng%1000)/1000.f;float nx,ny;if(rv<0.01f){nx=0;ny=0.16f*py2;}else if(rv<0.86f){nx=0.85f*px2+0.04f*py2;ny=-0.04f*px2+0.85f*py2+1.6f;}else if(rv<0.93f){nx=0.2f*px2-0.26f*py2;ny=0.23f*px2+0.22f*py2+1.6f;}else{nx=-0.15f*px2+0.28f*py2;ny=0.26f*px2+0.24f*py2+0.44f;}px2=nx;py2=ny;float sx=cx+px2*fscale,sy=r.B-2.f-py2*fh;if(sx>r.L&&sx<r.R&&sy>r.T&&sy<r.B)g.FillRect(dim,IRECT(sx,sy,sx+1.f,sy+1.f));} break; }
    case 3: { float ra=scale*0.02f,ang=0,pvx=cx,pvy=cy;for(int i=0;i<(big?120:40);i++){float a=ang*3.14159f/180.f;float x2=cx+ra*cosf(a),y2=cy+ra*sinf(a);g.DrawLine(bright,pvx,pvy,x2,y2,nullptr,tk);pvx=x2;pvy=y2;ang+=8.f;ra+=scale*0.003f;} break; }
    case 4: { for(int j=0;j<curves;j++){float t1=j*6.28f/curves,t2=(j+1)*6.28f/curves;g.DrawLine(bright,cx+sinf(3*t1)*sz,cy+sinf(4*t1)*sz*0.8f,cx+sinf(3*t2)*sz,cy+sinf(4*t2)*sz*0.8f,nullptr,tk);} break; }
    case 5: { struct S{float x1,y1,x2,y2;};std::vector<S> segs;for(int i=0;i<3;i++){float a1=(i*120.f-90.f)*3.14159f/180.f,a2=((i+1)*120.f-90.f)*3.14159f/180.f;segs.push_back({cx+sz*cosf(a1),cy+sz*sinf(a1),cx+sz*cosf(a2),cy+sz*sinf(a2)});}for(int d=0;d<(big?4:2);d++){std::vector<S> n2;for(auto&s:segs){float dx2=s.x2-s.x1,dy2=s.y2-s.y1;float ax=s.x1+dx2/3,ay=s.y1+dy2/3,bx=s.x1+dx2*2/3,by=s.y1+dy2*2/3;float px2=(s.x1+s.x2)/2-dy2*0.2887f,py2=(s.y1+s.y2)/2+dx2*0.2887f;n2.push_back({s.x1,s.y1,ax,ay});n2.push_back({ax,ay,px2,py2});n2.push_back({px2,py2,bx,by});n2.push_back({bx,by,s.x2,s.y2});}segs=n2;}for(auto&s:segs)g.DrawLine(dim,s.x1,s.y1,s.x2,s.y2,nullptr,tk); break; }
    case 6: { struct B{float x,y,a,l;int d;};int maxD=big?7:4;std::vector<B> stk;stk.push_back({cx,r.B-2.f,-90.f,sz*1.2f,0});while(!stk.empty()){auto b=stk.back();stk.pop_back();if(b.d>maxD||b.l<1.5f)continue;float rad=b.a*3.14159f/180.f,ex=b.x+b.l*cosf(rad),ey=b.y+b.l*sinf(rad);g.DrawLine(dim,b.x,b.y,ex,ey,nullptr,tk);stk.push_back({ex,ey,b.a-28.f,b.l*0.65f,b.d+1});stk.push_back({ex,ey,b.a+28.f,b.l*0.65f,b.d+1});} break; }
    case 7: { auto drawH=[&](auto&&self,float x,float y,float half,int dep)->void{if(dep<=0||half<1.f)return;g.DrawLine(dim,x-half,y,x+half,y,nullptr,tk);g.DrawLine(dim,x-half,y-half,x-half,y+half,nullptr,tk);g.DrawLine(dim,x+half,y-half,x+half,y+half,nullptr,tk);float nh=half*0.5f;self(self,x-half,y-half,nh,dep-1);self(self,x-half,y+half,nh,dep-1);self(self,x+half,y-half,nh,dep-1);self(self,x+half,y+half,nh,dep-1);};drawH(drawH,cx,cy,std::min(r.W(),r.H())*0.42f,big?6:4); break; }
    case 8: { struct Seg{float x1,y1,x2,y2;};std::vector<Seg> segs;segs.push_back({cx-sz*2,cy+sz*0.3f,cx+sz*2,cy+sz*0.3f});for(int d=0;d<(big?10:6);d++){std::vector<Seg> n2;for(auto&s:segs){float mx=(s.x1+s.x2)/2+(s.y2-s.y1)/2,my=(s.y1+s.y2)/2-(s.x2-s.x1)/2;n2.push_back({s.x1,s.y1,mx,my});n2.push_back({mx,my,s.x2,s.y2});}segs=n2;}for(auto&s:segs)g.DrawLine(bright,s.x1,s.y1,s.x2,s.y2,nullptr,0.8f); break; }
    case 9: { float step=big?2.f:3.f;float pw=r.W()*0.85f,ph=r.H()*0.85f;float pl=cx-pw/2,pt=cy-ph/2;for(float px=0;px<pw;px+=step)for(float py=0;py<ph;py+=step){double cr=-0.745+(px/pw-0.5)*0.008,ci=0.186+(py/ph-0.5)*0.008;double zr=0,zi=0;int it=0;while(zr*zr+zi*zi<4&&it<40){double t=zr*zr-zi*zi+cr;zi=2*zr*zi+ci;zr=t;it++;}if(it<40&&it>3)g.FillRect(IColor(it*4,80+it,std::min(255,180+it*2),220),IRECT(pl+px,pt+py,pl+px+step-0.5f,pt+py+step-0.5f));} break; }
    case 10: { float step=big?2.f:3.f;float pw=r.W()*0.85f,ph=r.H()*0.85f;float pl=cx-pw/2,pt=cy-ph/2;for(float px=0;px<pw;px+=step)for(float py=0;py<ph;py+=step){double zr=(px/pw-0.5)*3,zi=(py/ph-0.5)*2.4;int it=0;while(zr*zr+zi*zi<4&&it<30){double t=zr*zr-zi*zi-0.7;zi=2*zr*zi+0.27015;zr=t;it++;}if(it<30&&it>2)g.FillRect(IColor(it*6,70+it*2,std::min(255,160+it*3),220),IRECT(pl+px,pt+py,pl+px+step-0.5f,pt+py+step-0.5f));} break; }
    case 11: { const double a=-1.4,b=1.6,c=1.0,d=0.75;double x=0.0,y=0.0;float sc2=std::min(r.W(),r.H())*0.28f;for(int i=0;i<(big?8000:2200);i++){double nx=sin(a*y)+c*cos(a*x),ny=sin(b*x)+d*cos(b*y);x=nx;y=ny;if(i<120)continue;float px=cx+(float)x*sc2,py=cy-(float)y*sc2;if(px>r.L&&px<r.R&&py>r.T&&py<r.B){int al=28+(i&95);g.FillRect(IColor(al,75+(i%90),165+(i%85),215),IRECT(px,py,px+1.f,py+1.f));}} break; }
    case 12: { float step=big?2.f:3.f;float pw=r.W()*0.85f,ph=r.H()*0.85f;float pl=cx-pw/2,pt=cy-ph/2;for(float px=0;px<pw;px+=step)for(float py=0;py<ph;py+=step){double cr=-1.75+(px/pw)*0.15,ci=-0.08+(py/ph)*0.12;double zr=0,zi=0;int it=0;while(zr*zr+zi*zi<4&&it<40){double t=zr*zr-zi*zi+cr;zi=fabs(2*zr*zi)+ci;zr=t;it++;}if(it<40&&it>2)g.FillRect(IColor(it*5,100+it*2,std::min(255,170+it*2),230),IRECT(pl+px,pt+py,pl+px+step-0.5f,pt+py+step-0.5f));} break; }
    case 13: { struct P{float x,y,r2;};std::vector<P> ps;ps.push_back({cx,cy,sz});for(int d=0;d<(big?3:2);d++){std::vector<P> n2;for(auto&p:ps){for(int i=0;i<5;i++){float a1=(i*72.f-90)*3.14159f/180,a2=((i+1)*72.f-90)*3.14159f/180;g.DrawLine(dim,p.x+p.r2*cosf(a1),p.y+p.r2*sinf(a1),p.x+p.r2*cosf(a2),p.y+p.r2*sinf(a2),nullptr,tk);}float nr=p.r2*0.382f;n2.push_back({p.x,p.y,nr});for(int i=0;i<5;i++){float a=(i*72.f-90)*3.14159f/180;n2.push_back({p.x+(p.r2-nr)*cosf(a),p.y+(p.r2-nr)*sinf(a),nr});}}ps=n2;} break; }
    default: { // case 14 - Lichtenberg discharge (Diezel Herbert)
      // Two opposing seeds with branching arcs
      struct Pt { float x, y; };
      std::vector<Pt> pts; pts.push_back({cx, r.B - 1.f}); pts.push_back({cx, r.T + 1.f});
      unsigned rng = 0xBEEFu;
      int iters = big ? 220 : 90;
      float step = big ? 2.4f : 1.4f;
      for (int i = 0; i < iters; i++) {
        rng = rng * 1664525u + 1013904223u;
        Pt& parent = pts[rng % pts.size()];
        rng = rng * 1664525u + 1013904223u;
        bool fromBottom = (parent.y > cy);
        float baseAng = fromBottom ? -90.f : 90.f;
        float jitter = ((float)(rng % 100) / 100.f - 0.5f) * 110.f;
        float ang = (baseAng + jitter) * 3.14159f / 180.f;
        Pt next{parent.x + cosf(ang) * step, parent.y + sinf(ang) * step};
        if (next.x < r.L + 1.f || next.x > r.R - 1.f || next.y < r.T + 1.f || next.y > r.B - 1.f) continue;
        IColor col = (i < iters / 4) ? bright : dim;
        g.DrawLine(col, parent.x, parent.y, next.x, next.y, nullptr, (i < iters / 6) ? tk : 1.f);
        pts.push_back(next);
      }
      break;
    }
  }
}

inline void DrawSidebarMiniFractal(IGraphics& g, const IRECT& r, int idx, const IColor& bright, const IColor& dim)
{
    float cx = r.MW(), cy = r.MH(), sz = r.W() * 0.38f;
    switch (idx % 15)
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
      case 1: // Bad Cat Mini Cat - two cat eyes (blue/teal only at thumbnail)
      {
        const float mn = std::min(r.W(), r.H());
        const float sep = mn * 0.24f, ew = mn * 0.17f, eh = ew * 0.5f;
        auto eye = [&](float exc) {
          std::vector<float> xs, ys; const int NS = 10;
          auto q = [&](float ax, float ay, float bx, float by, float dx3, float dy3) {
            for (int i = 0; i <= NS; i++)
            {
              const float t = (float)i / NS, mt = 1.f - t;
              xs.push_back(exc + mt * mt * ax + 2.f * mt * t * bx + t * t * dx3);
              ys.push_back(cy + mt * mt * ay + 2.f * mt * t * by + t * t * dy3);
            }
          };
          q(-ew, 0.f, 0.f, -eh, ew, 0.f);
          q(ew, 0.f, 0.f, eh, -ew, 0.f);
          for (size_t i = 1; i < xs.size(); i++) g.DrawLine(bright, xs[i - 1], ys[i - 1], xs[i], ys[i], nullptr, 1.f);
          g.DrawLine(bright, xs.back(), ys.back(), xs.front(), ys.front(), nullptr, 1.f);
          g.FillEllipse(bright, IRECT(exc - ew * 0.13f, cy - eh * 0.92f, exc + ew * 0.13f, cy + eh * 0.92f));
        };
        eye(cx - sep);
        eye(cx + sep);
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
      case 3: // Spiral galaxy mini (Fryette Deliverance)
      {
        const float maxR = std::min(r.W(), r.H()) * 0.42f;
        for (int arm = 0; arm < 2; arm++)
        {
          const float off = arm * 3.14159f;
          for (int i = 0; i < 60; i++)
          {
            const float t = (float)i / 60.f;
            const float th = off + t * 3.2f * 3.14159f;
            const float rr = maxR * powf(t, 0.7f);
            const float px = cx + rr * cosf(th), py = cy + rr * sinf(th);
            if (px > r.L && px < r.R && py > r.T && py < r.B)
              g.FillRect((t < 0.5f) ? bright : dim, IRECT(px, py, px + 1.f, py + 1.f));
          }
        }
        g.FillCircle(bright, cx, cy, 1.6f); // thumbnail stays blue: no gold accent at 22px
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
      case 5: // Koch snowflake mini (bigger, 3 depth) (Lichtlaerm Prometheus)
      {
        struct S{float x1,y1,x2,y2;};
        std::vector<S> segs;
        const float rr = std::min(r.W(), r.H()) * 0.46f;
        for(int i=0;i<3;i++){
          float a1=(i*120.f-90.f)*3.14159f/180.f,a2=((i+1)*120.f-90.f)*3.14159f/180.f;
          segs.push_back({cx+rr*cosf(a1),cy+rr*sinf(a1),cx+rr*cosf(a2),cy+rr*sinf(a2)});
        }
        for(int d=0;d<3;d++){
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
      case 7: // Marshall JMP 2203 - Triforce (moved here; blue/teal only at thumbnail)
      {
        const float S = std::min(r.W(), r.H()) * 0.4f;
        const float ccy = cy + r.H() * 0.03f;
        const float ax = cx, ay = ccy - S;
        const float bx = cx - S * 0.866f, by = ccy + S * 0.5f;
        const float dx2 = cx + S * 0.866f, dy2 = ccy + S * 0.5f;
        const float mABx = (ax + bx) / 2.f, mABy = (ay + by) / 2.f;
        const float mACx = (ax + dx2) / 2.f, mACy = (ay + dy2) / 2.f;
        const float mBCx = (bx + dx2) / 2.f, mBCy = (by + dy2) / 2.f;
        auto tri = [&](float x1, float y1, float x2, float y2, float x3, float y3) {
          g.DrawLine(bright, x1, y1, x2, y2, nullptr, 1.f);
          g.DrawLine(bright, x2, y2, x3, y3, nullptr, 1.f);
          g.DrawLine(bright, x3, y3, x1, y1, nullptr, 1.f);
        };
        tri(ax, ay, mABx, mABy, mACx, mACy);
        tri(mABx, mABy, bx, by, mBCx, mBCy);
        tri(mACx, mACy, mBCx, mBCy, dx2, dy2);
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
      case 13: // Dark Sun eclipse mini (THC Sunset) - teal only
      {
        const float horizon = r.T + r.H() * 0.60f;
        const float R = std::min(r.W(), r.H()) * 0.24f;
        const float cyS = horizon - R * 1.15f;
        g.FillCircle(IColor(235, 10, 12, 16), cx, cyS, R); // dark eclipse core
        for (int i = 0; i < 10; i++) // teal corona ticks
        {
          const float a = (float)i / 10.f * 6.28318f;
          g.DrawLine(bright, cx + cosf(a) * R * 1.05f, cyS + sinf(a) * R * 1.05f,
                     cx + cosf(a) * R * 1.4f, cyS + sinf(a) * R * 1.4f, nullptr, 1.f);
        }
        g.DrawCircle(bright, cx, cyS, R, nullptr, 1.5f); // teal rim
        g.DrawLine(dim, cx, cyS + R, cx, horizon, nullptr, 1.f); // light shaft
        g.DrawLine(bright, r.L, horizon, r.R, horizon, nullptr, 1.f); // horizon
        break;
      }
      case 14: // Lichtenberg mini (Diezel Herbert) - readable bolt at sidebar size
      {
        auto drawBolt = [&](float x0, float y0, float x1, float y1, bool flip) {
          const int steps = 5;
          float px = x0;
          float py = y0;
          for (int i = 1; i <= steps; ++i)
          {
            const float t = (float)i / (float)steps;
            float nx = x0 + (x1 - x0) * t + ((i & 1) ? -1.f : 1.f) * sz * 0.22f * (flip ? -1.f : 1.f);
            float ny = y0 + (y1 - y0) * t;
            if (i == steps) { nx = x1; ny = y1; }
            g.DrawLine(i < 3 ? bright : dim, px, py, nx, ny, nullptr, 1.2f);

            if (i >= 2 && i <= 4)
            {
              float bx = nx + (flip ? -1.f : 1.f) * sz * 0.38f;
              float by = ny + ((i & 1) ? -1.f : 1.f) * sz * 0.18f;
              if (bx > r.L + 1.f && bx < r.R - 1.f && by > r.T + 1.f && by < r.B - 1.f)
                g.DrawLine(dim, nx, ny, bx, by, nullptr, 0.8f);
            }

            px = nx;
            py = ny;
          }
        };

        drawBolt(cx - sz * 0.45f, r.B - 2.f, cx + sz * 0.12f, cy + sz * 0.05f, false);
        drawBolt(cx + sz * 0.45f, r.T + 2.f, cx - sz * 0.10f, cy - sz * 0.04f, true);
        break;
      }
    }
}

// Custom-amp art: 4 distinct, instantly-recognizable procedural styles the user
// assigns in the builder. Scales to the rect, so one generator serves the hero,
// the sidebar thumbnail, and the builder swatch. artId is taken mod 4.
inline void DrawCustomAmpArt(IGraphics& g, const IRECT& rect, int artId, const IColor& bright, const IColor& dim)
{
  const float cx = rect.MW(), cy = rect.MH();
  const float scale = std::min(rect.W(), rect.H());
  const bool big = scale > 40.f;
  const float tk = big ? 2.f : 1.f;
  const float R = scale * 0.42f;
  switch (((artId % 6) + 6) % 6)
  {
    case 0: // concentric hexagon rings
    {
      const int rings = big ? 6 : 3;
      for (int k = 1; k <= rings; k++)
      {
        const float rr = R * (float)k / rings;
        float px = 0.f, py = 0.f;
        for (int i = 0; i <= 6; i++)
        {
          const float a = (i * 60.f - 90.f) * 3.14159265f / 180.f;
          const float nx = cx + rr * cosf(a), ny = cy + rr * sinf(a);
          if (i > 0)
            g.DrawLine((k % 2) ? bright : dim, px, py, nx, ny, nullptr, tk);
          px = nx;
          py = ny;
        }
      }
      break;
    }
    case 1: // radial sunburst spokes
    {
      const int spokes = big ? 36 : 16;
      for (int i = 0; i < spokes; i++)
      {
        const float a = i * 6.28318531f / spokes;
        const float r0 = R * 0.16f, r1 = R * ((i % 3 == 0) ? 1.f : 0.68f);
        g.DrawLine((i % 2) ? bright : dim, cx + r0 * cosf(a), cy + r0 * sinf(a), cx + r1 * cosf(a), cy + r1 * sinf(a),
                   nullptr, tk);
      }
      g.DrawCircle(dim, cx, cy, R * 0.16f, nullptr, tk);
      break;
    }
    case 2: // phyllotaxis sunflower dots
    {
      const int n = big ? 460 : 90;
      const float golden = 2.39996323f;
      const float dotR = big ? 1.8f : 1.f;
      for (int i = 0; i < n; i++)
      {
        const float rr = R * sqrtf((float)i / n);
        const float a = i * golden;
        g.FillCircle((i & 3) ? bright : dim, cx + rr * cosf(a), cy + rr * sinf(a), dotR);
      }
      break;
    }
    case 3: // rotating nested squares (pinwheel)
    {
      const int layers = big ? 9 : 5;
      for (int k = 0; k < layers; k++)
      {
        const float rr = R * (1.f - (float)k / (layers + 1));
        const float rot = k * 14.f * 3.14159265f / 180.f;
        float px = 0.f, py = 0.f;
        for (int i = 0; i <= 4; i++)
        {
          const float a = rot + (i * 90.f + 45.f) * 3.14159265f / 180.f;
          const float nx = cx + rr * cosf(a), ny = cy + rr * sinf(a);
          if (i > 0)
            g.DrawLine((k % 2) ? bright : dim, px, py, nx, ny, nullptr, tk);
          px = nx;
          py = ny;
        }
      }
      break;
    }
    case 4: // spirograph rosette (hypotrochoid)
    {
      const float Rr = R;
      const float rr = R * 0.4f;
      const float d = R * 0.5f;
      const float kk = (Rr - rr) / rr;
      const int steps = big ? 260 : 110;
      const float turns = big ? 6.2832f * 5.f : 6.2832f * 3.f;
      float px = 0.f, py = 0.f;
      for (int i = 0; i <= steps; i++)
      {
        const float t = turns * (float)i / steps;
        const float nx = cx + (Rr - rr) * cosf(t) + d * cosf(kk * t);
        const float ny = cy + (Rr - rr) * sinf(t) - d * sinf(kk * t);
        if (i > 0)
          g.DrawLine(((i / 14) % 2) ? bright : dim, px, py, nx, ny, nullptr, tk * 0.85f);
        px = nx;
        py = ny;
      }
      break;
    }
    case 5: // concentric hexagram / star mandala (overlapping triangles)
    {
      const int layers = big ? 4 : 2;
      for (int k = 1; k <= layers; k++)
      {
        const float rr = R * (float)k / layers;
        for (int tri = 0; tri < 2; tri++)
        {
          const float baseRot = (tri == 0 ? -90.f : 30.f) * 3.14159265f / 180.f;
          float px = 0.f, py = 0.f;
          for (int i = 0; i <= 3; i++)
          {
            const float a = baseRot + (i * 120.f) * 3.14159265f / 180.f;
            const float nx = cx + rr * cosf(a), ny = cy + rr * sinf(a);
            if (i > 0)
              g.DrawLine(tri ? dim : bright, px, py, nx, ny, nullptr, tk);
            px = nx;
            py = ny;
          }
        }
      }
      break;
    }
    default: break;
  }
}

inline void DrawHeroFractalArt(IGraphics& g, const IRECT& rect, int ampIdx)
{
    float cx = rect.MW();
    float cy = rect.MH();
    float w = rect.W();
    float h = rect.H();

    IColor bright(150, 120, 210, 220);
    IColor mid(80, 100, 180, 200);
    IColor dim(45, 80, 150, 170);
    const float tk = 2.f;
    const float tkThin = 1.5f;

    switch (ampIdx % 15)
    {
      case 0: // Ampete One - Dragon Nebula (dragon curve, left-anchored across ~2/3 width)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        std::vector<int> turns;
        for (int i = 0; i < 12; i++) // 12 iters -> landscape orientation (matches the classic 1.1.0 layout)
        {
          std::vector<int> next;
          for (auto t : turns) next.push_back(t);
          next.push_back(1);
          for (int j = (int)turns.size() - 1; j >= 0; j--) next.push_back(1 - turns[j]);
          turns = next;
        }
        std::vector<std::pair<float, float>> raw;
        raw.push_back({0.f, 0.f});
        {
          float px = 0.f, py = 0.f; int dir = 0;
          const float dxs[] = {1, 0, -1, 0}, dys[] = {0, -1, 0, 1};
          for (int i = 0; i < (int)turns.size(); i++) { px += dxs[dir]; py += dys[dir]; raw.push_back({px, py}); dir = (dir + (turns[i] ? 1 : 3)) % 4; }
        }
        float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
        for (auto& p : raw) { mnx = std::min(mnx, p.first); mny = std::min(mny, p.second); mxx = std::max(mxx, p.first); mxy = std::max(mxy, p.second); }
        const float extX = mxx - mnx, extY = mxy - mny;
        // Left-anchored: span ~60% of the width (capped by height), off-centre like the classic layout.
        const float S = std::min((w * 0.60f) / (extX > 0.f ? extX : 1.f), (h * 0.90f) / (extY > 0.f ? extY : 1.f));
        const float artW = extX * S, artH = extY * S;
        const float ox = rect.L + w * 0.05f, oy = cy - artH * 0.5f;
        const float acx = ox + artW * 0.5f, acy = oy + artH * 0.5f; // art centre for atmosphere
        Bloom(g, acx, acy, R * 0.6f, kTeal, 0.17f);
        unsigned ds = 7u; Dust(g, ds, acx, acy, R * 0.5f, 80);
        std::vector<std::pair<float, float>> P; P.reserve(raw.size());
        for (auto& p : raw) P.push_back({ox + (p.first - mnx) * S, oy + (p.second - mny) * S});
        GradStroke(g, P, kMid, kGoldHi, 2.f, true);
        GlowDot(g, kGold, kGoldHi, P.back().first, P.back().second, 4.5f, 10.f);
        break;
      }
      case 1: // Bad Cat Mini Cat - Eyes in the Dark (two glowing cat eyes over a starfield)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        const float sep = R * 0.24f, ew = R * 0.17f, eh = ew * 0.5f;
        Bloom(g, cx, cy, R * 0.75f, kTeal, 0.10f);
        {
          unsigned s = 4u;
          for (int i = 0; i < 44; i++)
          {
            const float px = rect.L + Frand(s) * w, py = rect.T + Frand(s) * h;
            const int al = (int)((0.06f + 0.14f * Frand(s)) * 255.f);
            g.FillCircle(IColor(al, 120, 210, 220), px, py, Frand(s) * 1.f + 0.3f);
          }
        }
        auto drawEye = [&](float exc) {
          std::vector<float> xs, ys;
          const int NS = 16;
          auto q = [&](float ax, float ay, float bx, float by, float dx3, float dy3) {
            for (int i = 0; i <= NS; i++)
            {
              const float t = (float)i / NS, mt = 1.f - t;
              xs.push_back(exc + mt * mt * ax + 2.f * mt * t * bx + t * t * dx3);
              ys.push_back(cy + mt * mt * ay + 2.f * mt * t * by + t * t * dy3);
            }
          };
          q(-ew, 0.f, 0.f, -eh, ew, 0.f);
          q(ew, 0.f, 0.f, eh, -ew, 0.f);
          g.FillConvexPolygon(WithA(kTeal, 0.30f), xs.data(), ys.data(), (int)xs.size());
          for (size_t i = 1; i < xs.size(); i++)
            g.DrawLine(WithA(kTeal, 0.85f), xs[i - 1], ys[i - 1], xs[i], ys[i], nullptr, 2.f);
          g.DrawLine(WithA(kTeal, 0.85f), xs.back(), ys.back(), xs.front(), ys.front(), nullptr, 2.f);
          const float pw = ew * 0.13f, ph = eh * 0.92f;
          g.FillEllipse(WithA(kGold, 0.35f), IRECT(exc - pw * 2.4f, cy - ph * 1.15f, exc + pw * 2.4f, cy + ph * 1.15f));
          g.FillEllipse(WithA(kGold, 0.95f), IRECT(exc - pw, cy - ph, exc + pw, cy + ph));
        };
        drawEye(cx - sep);
        drawEye(cx + sep);
        break;
      }
      case 2: // Brunetti XL 2 - Verdant Fern (layered fern, teal->gold tips, ground bloom)
      {
        using namespace volumart;
        Bloom(g, cx, rect.T + h * 0.86f, std::min(w, h) * 0.8f, kTeal, 0.13f);
        auto fern = [&](int n, float fh, float fsc, float bx, float by, IColor base, IColor tip, float rr, float a, bool gold) {
          float px = 0.f, py = 0.f; unsigned s = 42u;
          for (int i = 0; i < n; i++)
          {
            s = s * 1103515245u + 12345u; float rv = (float)(s % 1000) / 1000.f; float nx, ny;
            if (rv < 0.01f) { nx = 0.f; ny = 0.16f * py; }
            else if (rv < 0.86f) { nx = 0.85f * px + 0.04f * py; ny = -0.04f * px + 0.85f * py + 1.6f; }
            else if (rv < 0.93f) { nx = 0.2f * px - 0.26f * py; ny = 0.23f * px + 0.22f * py + 1.6f; }
            else { nx = -0.15f * px + 0.28f * py; ny = 0.26f * px + 0.24f * py + 0.44f; }
            px = nx; py = ny; if (i == 0) continue;
            const float sx = bx + px * fsc, sy = by - py * fh;
            if (sx < rect.L - 4.f || sx > rect.R + 4.f || sy < rect.T - 4.f || sy > rect.B + 4.f) continue;
            IColor c = Mix(base, tip, std::min(1.f, py / 9.f));
            if (gold && py > 7.6f) c = Mix(c, kGoldHi, (py - 7.6f) / 2.4f);
            g.FillCircle(WithA(c, a), sx, sy, rr);
          }
        };
        // Horizontal scale tied to height at the 1.1.0 fern ratio (~2.53:1 horiz:vert per unit,
        // i.e. 38px:15px) so the fronds keep their classic proportions and do not stretch with
        // the hero panel width or cramp when height-tied 1:1.
        fern(6000, (h * 0.78f) / 10.5f, h * 0.188f, rect.L + w * 0.55f, rect.T + h * 0.9f, IColor(255, 45, 90, 110), kMid, 0.9f, 0.32f, false);
        fern(11000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.1f, 0.7f, false);
        fern(9000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.15f, 0.55f, true);
        break;
      }
      case 3: // Spiral galaxy (Fryette Deliverance)
      {
        // Two logarithmic-spiral particle arms over a soft radial bloom, with a
        // glowing gold core. Deep and atmospheric.
        const float maxR = std::min(w, h) * 0.46f;
        for (int b = 6; b >= 1; b--)
          g.FillCircle(IColor(6, 120, 210, 220), cx, cy, maxR * (float)b / 6.f);
        unsigned rng = 5u;
        auto frand = [&]() -> float { rng = rng * 1664525u + 1013904223u; return (float)rng / (float)0xFFFFFFFFu; };
        for (int arm = 0; arm < 2; arm++)
        {
          const float off = arm * 3.14159f;
          for (int i = 0; i < 560; i++)
          {
            const float t = (float)i / 560.f;
            const float theta = off + t * 4.2f * 3.14159f;
            const float rr = maxR * powf(t, 0.7f);
            const float px = cx + rr * cosf(theta) + (frand() - 0.5f) * rr * 0.12f;
            const float py = cy + rr * sinf(theta) + (frand() - 0.5f) * rr * 0.12f;
            if (px < rect.L || px > rect.R || py < rect.T || py > rect.B) continue;
            const int al = (int)(255.f * (0.2f + 0.6f * (1.f - t)));
            g.FillCircle((t < 0.5f) ? IColor(al, 120, 210, 220) : IColor(al, 100, 180, 200), px, py, (t < 0.2f) ? 1.4f : 1.f);
          }
        }
        g.FillCircle(IColor(50, 200, 165, 87), cx, cy, 10.f);
        g.FillCircle(IColor(90, 200, 165, 87), cx, cy, 6.f);
        g.FillCircle(IColor(235, 252, 222, 145), cx, cy, 4.f);
        break;
      }
      case 4: // H&K TriAmp Mk2 - Lissajous Nebula (knot over bloom, shadow echo, gradient stroke, glowing core)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        Bloom(g, cx, cy, R * 0.55f, kTeal, 0.16f);
        unsigned ds = 4u; Dust(g, ds, cx, cy, R * 0.5f, 70);
        auto knot = [&](float sc, IColor c0, IColor c1, float lw, bool gl) {
          std::vector<std::pair<float, float>> P;
          for (int i = 0; i <= 720; i++)
          {
            const float t = i * 6.28318f / 720.f;
            P.push_back({cx + sinf(3.f * t + 0.5f) * w * sc, cy + sinf(4.f * t) * h * sc * 1.06f});
          }
          GradStroke(g, P, c0, c1, lw, gl);
        };
        knot(0.34f, IColor(255, 60, 120, 150), kMid, 4.f, false);
        knot(0.4f, kTeal, kMid, 2.1f, true);
        break;
      }
      case 5: // Lichtlaerm Prometheus - Koch Deep (snowflake over bloom+dust, gradient stroke into gold, glowing core)
      {
        using namespace volumart;
        const float R = std::min(w * 0.46f, h * 0.5f);
        Bloom(g, cx, cy, R * 1.2f, kTeal, 0.15f);
        unsigned ds = 7u; Dust(g, ds, cx, cy, R * 1.1f, 60);
        struct Seg { float x1, y1, x2, y2; };
        std::vector<Seg> segs;
        for (int i = 0; i < 3; i++)
        {
          const float a1 = (i * 120.f - 90.f) * 3.14159f / 180.f, a2 = ((i + 1) * 120.f - 90.f) * 3.14159f / 180.f;
          segs.push_back({cx + R * cosf(a1), cy + R * sinf(a1), cx + R * cosf(a2), cy + R * sinf(a2)});
        }
        for (int depth = 0; depth < 5; depth++)
        {
          std::vector<Seg> next;
          for (auto& s : segs)
          {
            const float dx = s.x2 - s.x1, dy = s.y2 - s.y1;
            const float ax = s.x1 + dx / 3.f, ay = s.y1 + dy / 3.f, bx = s.x1 + dx * 2.f / 3.f, by = s.y1 + dy * 2.f / 3.f;
            const float px2 = (s.x1 + s.x2) / 2.f - dy * 0.2887f, py2 = (s.y1 + s.y2) / 2.f + dx * 0.2887f;
            next.push_back({s.x1, s.y1, ax, ay}); next.push_back({ax, ay, px2, py2});
            next.push_back({px2, py2, bx, by}); next.push_back({bx, by, s.x2, s.y2});
          }
          segs = next;
        }
        std::vector<std::pair<float, float>> P; P.push_back({segs[0].x1, segs[0].y1});
        for (auto& s : segs) P.push_back({s.x2, s.y2});
        GradStroke(g, P, kTeal, kGoldHi, 1.9f, true);
        GlowDot(g, kGold, kGoldHi, cx, cy, 3.5f, 10.f);
        break;
      }
      case 6: // Marshall 2204 - Windswept Glow (tree leaned into a gust over a dawn glow, glowing embers)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        Bloom(g, cx, rect.B, R * 0.8f, kGold, 0.12f);
        Bloom(g, cx, cy, R * 0.6f, kTeal, 0.06f);
        const float lean = 12.f;
        struct Br { float x, y, a, l; int d; };
        auto grow = [&](int maxD, auto cb) {
          std::vector<Br> stk; stk.push_back({cx, rect.T + h * 0.94f, -90.f, R * 0.34f, 0});
          while (!stk.empty())
          {
            Br b = stk.back(); stk.pop_back();
            if (b.d > maxD || b.l < 2.5f) continue;
            const float rad = b.a * 3.14159f / 180.f, ex = b.x + b.l * cosf(rad), ey = b.y + b.l * sinf(rad);
            cb(b, ex, ey);
            const float sp = 22.f + b.d * 4.f;
            stk.push_back({ex, ey, b.a - sp + lean, b.l * 0.67f, b.d + 1});
            stk.push_back({ex, ey, b.a + sp + lean, b.l * 0.67f, b.d + 1});
          }
        };
        grow(9, [&](const Br& b, float ex, float ey) {
          const IColor c = WithA(Mix(kDim, kTeal, std::max(0.f, 1.f - b.d / 6.f)), 0.9f);
          if (b.d < 3) GlowLine(g, kTeal, c, b.x, b.y, ex, ey, 2.6f, 4.f);
          else g.DrawLine(c, b.x, b.y, ex, ey, nullptr, 1.f);
        });
        grow(9, [&](const Br& b, float ex, float ey) {
          if (b.d >= 7) GlowDot(g, kGold, (b.d % 2) ? kGoldHi : kTeal, ex, ey, 1.7f, 4.f);
        });
        {
          unsigned s = 9u;
          for (int i = 0; i < 26; i++)
          {
            const float px = rect.L + w * (0.55f + 0.4f * Frand(s)), py = rect.T + h * (0.1f + 0.5f * Frand(s));
            GlowDot(g, kGold, Frand(s) > 0.5f ? kGoldHi : kTeal, px, py, Frand(s) * 1.6f + 0.5f, 4.f);
          }
        }
        break;
      }
      case 7: // Marshall JMP 2203 - Depth Triforce (stacked offset triforce outlines receding teal->gold)
      {
        using namespace volumart;
        const float cy2 = cy + h * 0.02f, baseS = std::min(w * 0.34f, h * 0.42f), R = std::min(w, h);
        Bloom(g, cx, cy2, R * 0.5f, kGold, 0.10f);
        auto M = [](float x1, float y1, float x2, float y2) { return std::make_pair((x1 + x2) / 2.f, (y1 + y2) / 2.f); };
        auto triOutline = [&](float ccx, float ccy, float S, const IColor& col, float thick, bool glow) {
          const float Ax = ccx, Ay = ccy - S, Bx = ccx - S * 0.866f, By = ccy + S * 0.5f, Cx = ccx + S * 0.866f, Cy = ccy + S * 0.5f;
          const auto mAB = M(Ax, Ay, Bx, By), mAC = M(Ax, Ay, Cx, Cy), mBC = M(Bx, By, Cx, Cy);
          struct Tri { float x1, y1, x2, y2, x3, y3; };
          const Tri T[3] = {{Ax, Ay, mAB.first, mAB.second, mAC.first, mAC.second},
                            {mAB.first, mAB.second, Bx, By, mBC.first, mBC.second},
                            {mAC.first, mAC.second, mBC.first, mBC.second, Cx, Cy}};
          for (auto& t : T)
          {
            if (glow)
            {
              GlowLine(g, kGold, col, t.x1, t.y1, t.x2, t.y2, thick, 5.f);
              GlowLine(g, kGold, col, t.x2, t.y2, t.x3, t.y3, thick, 5.f);
              GlowLine(g, kGold, col, t.x3, t.y3, t.x1, t.y1, thick, 5.f);
            }
            else
            {
              g.DrawLine(col, t.x1, t.y1, t.x2, t.y2, nullptr, thick);
              g.DrawLine(col, t.x2, t.y2, t.x3, t.y3, nullptr, thick);
              g.DrawLine(col, t.x3, t.y3, t.x1, t.y1, nullptr, thick);
            }
          }
        };
        const int K = 6;
        for (int i = K - 1; i >= 0; --i) // back (teal, small, high) -> front (gold, full, glowing)
        {
          const float dt = (float)i / (float)(K - 1);
          const float S = baseS * (1.f - dt * 0.14f), off = dt * R * 0.07f;
          const IColor col = WithA(Mix(kGold, kTeal, dt), 0.85f - dt * 0.5f);
          triOutline(cx, cy2 - off, S, col, i == 0 ? 2.2f : 1.2f, i == 0);
        }
        break;
      }
      case 8: // Marshall JVM 210H - Levy Nebula (Levy C over bloom+dust, gradient stroke into gold, glow)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        Bloom(g, cx, cy, R * 0.55f, kTeal, 0.16f);
        unsigned ds = 8u; Dust(g, ds, cx, cy, R * 0.5f, 70);
        struct Seg { float x1, y1, x2, y2; };
        std::vector<Seg> segs; segs.push_back({-1.f, 0.f, 1.f, 0.f});
        for (int depth = 0; depth < 11; depth++)
        {
          std::vector<Seg> next;
          for (auto& s : segs)
          {
            const float mx = (s.x1 + s.x2) / 2.f + (s.y2 - s.y1) / 2.f, my = (s.y1 + s.y2) / 2.f - (s.x2 - s.x1) / 2.f;
            next.push_back({s.x1, s.y1, mx, my}); next.push_back({mx, my, s.x2, s.y2});
          }
          segs = next;
        }
        std::vector<std::pair<float, float>> raw; raw.push_back({segs[0].x1, segs[0].y1});
        for (auto& s : segs) raw.push_back({s.x2, s.y2});
        float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
        for (auto& p : raw) { mnx = std::min(mnx, p.first); mny = std::min(mny, p.second); mxx = std::max(mxx, p.first); mxy = std::max(mxy, p.second); }
        const float bcx = (mnx + mxx) / 2.f, bcy = (mny + mxy) / 2.f, ext = std::max(mxx - mnx, mxy - mny);
        const float S = R * 1.25f / (ext > 0.f ? ext : 1.f);
        std::vector<std::pair<float, float>> P; for (auto& p : raw) P.push_back({cx + (p.first - bcx) * S, cy + (p.second - bcy) * S});
        GradStroke(g, P, kTeal, kGoldHi, 1.6f, true);
        break;
      }
      case 9: // Orange OD120 - Ember Bulb (Mandelbrot bulb, teal-dominant with only faintly warm tips, no gold embers)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
        Bloom(g, cx, cy, R * 0.55f, kTeal, 0.12f);
        unsigned ds = 6u; Dust(g, ds, cx, cy, std::min(pw, ph) * 0.5f, 90); // stars kept inside the art footprint
        const float step = 2.f;
        for (float px = 0; px < pw; px += step)
          for (float py = 0; py < ph; py += step)
          {
            double cr = -0.745 + (px / pw - 0.5) * 0.01, ci = 0.186 + (py / ph - 0.5) * 0.01;
            double zr = 0, zi = 0; int it = 0;
            while (zr * zr + zi * zi < 4.0 && it < 40) { double t = zr * zr - zi * zi + cr; zi = 2 * zr * zi + ci; zr = t; it++; }
            if (it < 40 && it > 2) { const float f = (float)it / 40.f; g.FillCircle(WithA(Mix(kTeal, kGold, f * 0.28f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f); }
          }
        break;
      }
      case 10: // Orange ORS100 - Julia Nebula (compact Julia, teal with only faint warm tips, no centre dot)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
        Bloom(g, cx, cy, R * 0.5f, kTeal, 0.12f);
        unsigned ds = 2u; Dust(g, ds, cx, cy, std::min(pw, ph) * 0.5f, 70); // stars kept inside the art footprint
        const float step = 2.f;
        for (float px = 0; px < pw; px += step)
          for (float py = 0; py < ph; py += step)
          {
            double zr = (px / pw - 0.5) * 3.0, zi = (py / ph - 0.5) * 2.4; int it = 0;
            while (zr * zr + zi * zi < 4.0 && it < 32) { double t = zr * zr - zi * zi - 0.7; zi = 2 * zr * zi + 0.27015; zr = t; it++; }
            if (it < 32 && it > 2) { const float f = (float)it / 32.f; g.FillCircle(WithA(Mix(kTeal, kGold, f * 0.3f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f); }
          }
        break;
      }
      case 11: // Sebago Texas Flood - Clifford Nebula (attractor over bloom+dust, teal->gold density, glowing core)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        Bloom(g, cx, cy, R * 0.5f, kTeal, 0.16f);
        unsigned ds = 5u; Dust(g, ds, cx, cy, R * 0.5f, 70);
        const double a = -1.4, b = 1.6, c = 1.0, d = 0.75;
        double X = 0.0, Y = 0.0; const float S = R * 0.22f; const int N = 9000;
        for (int i = 0; i < N; i++)
        {
          double nx = sin(a * Y) + c * cos(a * X), ny = sin(b * X) + d * cos(b * Y);
          X = nx; Y = ny; if (i <= 120) continue;
          const float px = cx + (float)X * S, py = cy - (float)Y * S;
          if (px < rect.L || px > rect.R || py < rect.T || py > rect.B) continue;
          const float f = (float)i / (float)N;
          g.FillCircle(WithA(Mix(kTeal, kGold, f), 0.35f), px, py, 0.9f);
        }
        break;
      }
      case 12: // Soldano SLO100 - Beacon Sweep (Burning Ship "sea" lit by a lighthouse's rotating double beam)
      {
        using namespace volumart;
        const float R = std::min(w, h);
        const float bx = cx, by = rect.T + h * 0.44f; // beacon
        Bloom(g, bx, by, R * 0.5f, kTeal, 0.08f);
        const float step = 2.f, pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
        for (float px = 0; px < pw; px += step)
          for (float py = 0; py < ph; py += step)
          {
            double cr = -1.75 + (px / pw) * 0.15, ci = -0.08 + (py / ph) * 0.12;
            double zr = 0, zi = 0; int it = 0;
            while (zr * zr + zi * zi < 4.0 && it < 64) { double t = zr * zr - zi * zi + cr; zi = fabs(2.0 * zr * zi) + ci; zr = t; it++; }
            if (it < 64 && it > 3)
            {
              // Deeper dark contrast on the "sea": squared curve pushes mids/darks toward near-black
              // ink while the teal highlight peak stays where it was (not brighter).
              const float f = (float)it / 64.f, fc = f * f;
              static const IColor kInk(255, 10, 20, 28);
              g.FillCircle(WithA(Mix(kInk, kTeal, fc), 0.32f + 0.52f * fc), pl + px, pt + py, step * 0.8f);
            }
          }
        // Volumetric light cone: gradient triangle (beam colour -> transparent) from the beacon.
        const IColor beam = kGold; // hero is always large -> gold light
        auto beamCone = [&](float ang, float spread, float len, float a) {
          const float ex = bx + cosf(ang) * len, ey = by + sinf(ang) * len;
          g.PathTriangle(bx, by, bx + cosf(ang - spread) * len, by + sinf(ang - spread) * len,
                         bx + cosf(ang + spread) * len, by + sinf(ang + spread) * len);
          g.PathFill(IPattern::CreateLinearGradient(bx, by, ex, ey,
                                                    {{WithA(beam, a), 0.f},
                                                     {WithA(beam, a * 0.25f), 0.7f},
                                                     {IColor(0, beam.R, beam.G, beam.B), 1.f}}));
        };
        const float PIf = 3.14159f;
        beamCone(PIf * 0.14f, 0.055f, R * 1.0f, 0.085f);       // main beam (dimmed)
        beamCone(PIf * 0.14f + PIf, 0.055f, R * 0.9f, 0.038f); // opposing back beam
        beamCone(PIf * 0.86f, 0.05f, R * 0.85f, 0.048f);       // secondary sweep
        GlowDot(g, beam, kGoldHi, bx, by, 2.2f, 6.f);
        break;
      }
      case 13: // THC Sunset - "Dark Sun" (Dark Souls eclipse: dark core, ring-of-fire corona, light shaft)
      {
        using namespace volumart;
        const IColor sGold(255, 252, 222, 145), sOrange(255, 233, 138, 90), sMag(255, 201, 74, 140), sTeal(255, 120, 210, 220);
        const float horizon = rect.T + h * 0.60f;
        const float R0 = std::min(w, h) * 0.17f, cyS = horizon - R0 * 2.35f;
        const float TAU = 6.28318f;
        // synthwave sky
        g.PathRect(IRECT(rect.L, rect.T, rect.R, horizon));
        g.PathFill(IPattern::CreateLinearGradient(rect.L, rect.T, rect.L, horizon,
                                                  {{WithA(sTeal, 0.04f), 0.f}, {WithA(sMag, 0.09f), 0.72f}, {WithA(sOrange, 0.15f), 1.f}}));
        Bloom(g, cx, cyS, R0 * 3.2f, sOrange, 0.11f);
        // ring-of-fire corona: irregular jagged flame spikes around the rim
        unsigned seed = 31u;
        for (int i = 0; i < 96; i++)
        {
          const float a = (float)i / 96.f * TAU;
          const float len = R0 * (1.02f + Frand(seed) * 0.16f + 0.04f * sinf(a * 7.f));
          const float ix = cx + cosf(a) * R0 * 0.99f, iy = cyS + sinf(a) * R0 * 0.99f;
          const float ox = cx + cosf(a) * len, oy = cyS + sinf(a) * len;
          const IColor& col = (Frand(seed) > 0.5f) ? sGold : sOrange;
          g.DrawLine(WithA(col, 0.18f), ix, iy, ox, oy, nullptr, 3.0f); // glow
          g.DrawLine(WithA(col, 0.5f + 0.45f * Frand(seed)), ix, iy, ox, oy, nullptr, 1.4f);
        }
        // dark eclipse core: near-black disc with a faint warm inner glow
        g.PathCircle(cx, cyS, R0);
        g.PathFill(IPattern::CreateRadialGradient(cx, cyS, R0,
                                                  {{WithA(sOrange, 0.16f), 0.f}, {IColor(235, 12, 12, 17), 0.45f}, {IColor(250, 7, 8, 12), 1.f}}));
        // bright rim + faint inner ring (annular "darksign" look)
        g.DrawCircle(WithA(sGold, 0.22f), cx, cyS, R0, nullptr, 6.f);
        g.DrawCircle(WithA(sGold, 0.95f), cx, cyS, R0, nullptr, 2.f);
        g.DrawCircle(WithA(sGold, 0.40f), cx, cyS, R0 * 0.86f, nullptr, 1.f);
        // light shaft down to the horizon (gradient beam + dust)
        const float topY = cyS + R0 * 0.92f, wt = R0 * 0.20f, wb = R0 * 0.05f;
        g.PathClear();
        g.PathMoveTo(cx - wt, topY);
        g.PathLineTo(cx + wt, topY);
        g.PathLineTo(cx + wb, horizon);
        g.PathLineTo(cx - wb, horizon);
        g.PathClose();
        g.PathFill(IPattern::CreateLinearGradient(cx, topY, cx, horizon, {{WithA(sGold, 0.34f), 0.f}, {WithA(sGold, 0.f), 1.f}}));
        g.DrawLine(WithA(sGold, 0.5f), cx - wt * 0.7f, topY, cx - wb, horizon, nullptr, 1.f);
        g.DrawLine(WithA(sGold, 0.5f), cx + wt * 0.7f, topY, cx + wb, horizon, nullptr, 1.f);
        Dust(g, 7u, cx, (topY + horizon) * 0.5f, wt * 1.5f, 10);
        // praise-burst at the base of the shaft on the horizon
        for (int k = -4; k <= 4; k++)
        {
          const float a = -1.5708f + k * 0.16f, l = R0 * (0.5f - std::abs(k) * 0.05f);
          g.DrawLine(WithA(sGold, 0.5f), cx, horizon, cx + cosf(a) * l, horizon + sinf(a) * l, nullptr, 1.f);
        }
        // glowing horizon + perspective grid floor
        g.DrawLine(WithA(sOrange, 0.5f), rect.L, horizon, rect.R, horizon, nullptr, 4.f);
        g.DrawLine(WithA(sGold, 0.9f), rect.L, horizon, rect.R, horizon, nullptr, tkThin);
        for (int i = 1; i <= 8; i++)
        {
          const float t = (float)i / 8.f, yy = horizon + (rect.B - horizon) * t * t;
          g.DrawLine(IColor((int)(255.f * (0.26f * (1.f - t) + 0.08f)), 120, 210, 220), rect.L, yy, rect.R, yy, nullptr, 1.f);
        }
        for (int i = -12; i <= 12; i++)
        {
          const float fx = cx + ((float)i / 12.f) * w * 0.55f;
          g.DrawLine(IColor(38, 120, 210, 220), cx + ((float)i / 12.f) * w * 0.05f, horizon, fx, rect.B, nullptr, 1.f);
        }
        break;
      }
      case 14: // Diezel Herbert Mk1 - Lichtenberg Glow (dual-seed discharge over a teal bloom, glowing terminals)
      {
        // Two opposing seeds (top and bottom), DLA-style branching toward random
        // attractor points distributed across the panel. Result: dual branching
        // electric arcs evoking high-voltage transformer discharge.
        volumart::Bloom(g, cx, cy, std::min(w, h) * 0.55f, volumart::kTeal, 0.16f);
        struct Pt { float x, y; };
        std::vector<Pt> pts;
        pts.reserve(1500);
        pts.push_back({cx, rect.B - 16.f});
        pts.push_back({cx, rect.T + 16.f});

        // Spatial hash to skip near-duplicate placements (keeps the drawing crisp)
        const float cellSz = 6.f;
        std::unordered_set<long long> grid;
        auto cellKey = [&](float x, float y) -> long long {
          long long ix = (long long)std::floor(x / cellSz);
          long long iy = (long long)std::floor(y / cellSz);
          return (ix << 32) ^ (long long)(unsigned long long)iy;
        };
        grid.insert(cellKey(pts[0].x, pts[0].y));
        grid.insert(cellKey(pts[1].x, pts[1].y));

        // Randomly distributed attractors - bias toward column near center
        constexpr int kAttractors = 360;
        std::vector<Pt> targets;
        targets.reserve(kAttractors);
        unsigned rng = 0xBEEFu;
        auto frand = [&]() -> float {
          rng = rng * 1664525u + 1013904223u;
          return (float)rng / (float)0xFFFFFFFFu;
        };
        for (int i = 0; i < kAttractors; ++i)
        {
          targets.push_back({cx + (frand() - 0.5f) * w * 0.72f,
                             rect.T + 18.f + frand() * (h - 36.f)});
        }

        constexpr int kIters = 1400;
        for (int i = 0; i < kIters; ++i)
        {
          const Pt& target = targets[(int)(frand() * kAttractors) % kAttractors];

          // Sample-based nearest search (fast enough; pts grows to ~1.4k)
          int trials = std::min(48, (int)pts.size());
          int nearestIdx = 0;
          float bestD = 1e30f;
          for (int k = 0; k < trials; ++k)
          {
            int idx2 = (int)(frand() * pts.size()) % (int)pts.size();
            float dx = pts[idx2].x - target.x;
            float dy = pts[idx2].y - target.y;
            float d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; nearestIdx = idx2; }
          }

          const Pt& parent = pts[nearestIdx];
          float dx = target.x - parent.x;
          float dy = target.y - parent.y;
          float dist = std::sqrt(dx * dx + dy * dy);
          if (dist < 0.001f) continue;
          float ang = std::atan2(dy, dx) + (frand() - 0.5f) * 0.6f;
          float len = 9.f + frand() * 5.f;
          Pt next{parent.x + std::cos(ang) * len, parent.y + std::sin(ang) * len};
          if (next.x < rect.L + 14.f || next.x > rect.R - 14.f
              || next.y < rect.T + 12.f || next.y > rect.B - 12.f) continue;

          long long key = cellKey(next.x, next.y);
          if (!grid.insert(key).second) continue;

          IColor col;
          float thickness;
          if (i < 90)        { col = bright; thickness = 2.4f; }
          else if (i < 380)  { col = mid;    thickness = 1.5f; }
          else               { col = dim;    thickness = 1.0f; }
          g.DrawLine(col, parent.x, parent.y, next.x, next.y, nullptr, thickness);
          pts.push_back(next);
        }

        // Glowing terminals (a whisper of gold only at hero size).
        volumart::GlowDot(g, volumart::kTeal, volumart::kTeal, cx, rect.B - 16.f, 2.6f, 5.f);
        volumart::GlowDot(g, volumart::kTeal, volumart::kTeal, cx, rect.T + 16.f, 2.6f, 5.f);
        g.FillCircle(volumart::WithA(volumart::kGoldHi, 0.85f), cx, rect.B - 16.f, 1.2f);
        g.FillCircle(volumart::WithA(volumart::kGoldHi, 0.85f), cx, rect.T + 16.f, 1.2f);
        break;
      }
    }

}
