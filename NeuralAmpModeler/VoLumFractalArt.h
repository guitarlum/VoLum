#pragma once

// Procedural fractal art generators extracted from VoLumColorHelpers / VoLumCoreControls.

#include "IControls.h"
#include "VoLumColorHelpers.h"
#include <cmath>
#include <vector>
#include <algorithm>

using namespace iplug;
using namespace igraphics;

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
  switch (idx % 14) {
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
    default: { struct P{float x,y,r2;};std::vector<P> ps;ps.push_back({cx,cy,sz});for(int d=0;d<(big?3:2);d++){std::vector<P> n2;for(auto&p:ps){for(int i=0;i<5;i++){float a1=(i*72.f-90)*3.14159f/180,a2=((i+1)*72.f-90)*3.14159f/180;g.DrawLine(dim,p.x+p.r2*cosf(a1),p.y+p.r2*sinf(a1),p.x+p.r2*cosf(a2),p.y+p.r2*sinf(a2),nullptr,tk);}float nr=p.r2*0.382f;n2.push_back({p.x,p.y,nr});for(int i=0;i<5;i++){float a=(i*72.f-90)*3.14159f/180;n2.push_back({p.x+(p.r2-nr)*cosf(a),p.y+(p.r2-nr)*sinf(a),nr});}}ps=n2;} break; }
  }
}

inline void DrawSidebarMiniFractal(IGraphics& g, const IRECT& r, int idx, const IColor& bright, const IColor& dim)
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

    switch (ampIdx % 14)
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
          float sy = rect.B - 15.f - py * 15.f;
          if (sx > rect.L && sx < rect.R && sy > rect.T && sy < rect.B)
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
        stack.push_back({cx, rect.B - 15.f, -90.f, 52.f, 0});
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
      case 7: // H-tree (Marshall JMP 2203)
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
      case 11: // Clifford attractor (Sebago)
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
          if (px < rect.L + 4.f || px > rect.R - 4.f || py < rect.T + 4.f || py > rect.B - 4.f) continue;
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
        pts.push_back({cx, rect.B - 15.f});
        unsigned int rng = 12345;
        for (int i = 0; i < 2000; i++) {
            rng = rng * 1103515245 + 12345;
            int parentIdx = rng % pts.size();
            float angle = -90.f + (float)(rng % 180) - 90.f;
            float rad = angle * 3.14159f / 180.f;
            float len = 8.f;
            Pt next = {pts[parentIdx].x + len * cosf(rad), pts[parentIdx].y + len * sinf(rad)};
            
            if (next.x > rect.L + 10.f && next.x < rect.R - 10.f && next.y > rect.T + 10.f && next.y < rect.B - 10.f) {
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
