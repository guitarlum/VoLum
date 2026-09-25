#pragma once

// Procedural fractal art generators extracted from VoLumColorHelpers / VoLumCoreControls.

#include "IControls.h"
#include "VoLumAmpeteCatalog.h"
#include "VoLumColorHelpers.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include "art/VoLumArtCommon.h"
#include "art/VoLumArtDispatch.h"

using namespace iplug;
using namespace igraphics;

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
  switch (idx % 15)
  {
    case 0:
    {
      std::vector<int> turns;
      for (int i = 0; i < (big ? 8 : 6); i++)
      {
        std::vector<int> n2;
        for (auto t : turns)
          n2.push_back(t);
        n2.push_back(1);
        for (int j = (int)turns.size() - 1; j >= 0; j--)
          n2.push_back(1 - turns[j]);
        turns = n2;
      }
      float step = big ? scale * 0.012f : 1.8f, px2 = cx - scale * 0.1f, py2 = cy + scale * 0.06f;
      int dir = 0;
      const float dx[] = {step, 0, -step, 0}, dy[] = {0, -step, 0, step};
      for (int i = 0; i < (int)turns.size(); i++)
      {
        float nx = px2 + dx[dir], ny = py2 + dy[dir];
        g.DrawLine(bright, px2, py2, nx, ny, nullptr, tk);
        px2 = nx;
        py2 = ny;
        dir = (dir + (turns[i] ? 1 : 3)) % 4;
      }
      break;
    }
    case 1:
    {
      struct T
      {
        float x1, y1, x2, y2, x3, y3;
      };
      std::vector<T> ts;
      ts.push_back({cx, cy - sz, cx - sz, cy + sz * 0.7f, cx + sz, cy + sz * 0.7f});
      for (int d = 0; d < depth; d++)
      {
        std::vector<T> n2;
        for (auto& t : ts)
        {
          g.DrawLine(dim, t.x1, t.y1, t.x2, t.y2, nullptr, tk);
          g.DrawLine(dim, t.x2, t.y2, t.x3, t.y3, nullptr, tk);
          g.DrawLine(dim, t.x3, t.y3, t.x1, t.y1, nullptr, tk);
          n2.push_back({t.x1, t.y1, (t.x1 + t.x2) / 2, (t.y1 + t.y2) / 2, (t.x3 + t.x1) / 2, (t.y3 + t.y1) / 2});
          n2.push_back({(t.x1 + t.x2) / 2, (t.y1 + t.y2) / 2, t.x2, t.y2, (t.x2 + t.x3) / 2, (t.y2 + t.y3) / 2});
          n2.push_back({(t.x3 + t.x1) / 2, (t.y3 + t.y1) / 2, (t.x2 + t.x3) / 2, (t.y2 + t.y3) / 2, t.x3, t.y3});
        }
        ts = n2;
      }
      break;
    }
    case 2:
    {
      float px2 = 0, py2 = 0;
      unsigned rng = 42;
      float fscale = scale * 0.07f, fh = scale * 0.035f;
      for (int i = 0; i < dots; i++)
      {
        rng = rng * 1103515245 + 12345;
        float rv = (float)(rng % 1000) / 1000.f;
        float nx, ny;
        if (rv < 0.01f)
        {
          nx = 0;
          ny = 0.16f * py2;
        }
        else if (rv < 0.86f)
        {
          nx = 0.85f * px2 + 0.04f * py2;
          ny = -0.04f * px2 + 0.85f * py2 + 1.6f;
        }
        else if (rv < 0.93f)
        {
          nx = 0.2f * px2 - 0.26f * py2;
          ny = 0.23f * px2 + 0.22f * py2 + 1.6f;
        }
        else
        {
          nx = -0.15f * px2 + 0.28f * py2;
          ny = 0.26f * px2 + 0.24f * py2 + 0.44f;
        }
        px2 = nx;
        py2 = ny;
        float sx = cx + px2 * fscale, sy = r.B - 2.f - py2 * fh;
        if (sx > r.L && sx < r.R && sy > r.T && sy < r.B)
          g.FillRect(dim, IRECT(sx, sy, sx + 1.f, sy + 1.f));
      }
      break;
    }
    case 3:
    {
      float ra = scale * 0.02f, ang = 0, pvx = cx, pvy = cy;
      for (int i = 0; i < (big ? 120 : 40); i++)
      {
        float a = ang * 3.14159f / 180.f;
        float x2 = cx + ra * cosf(a), y2 = cy + ra * sinf(a);
        g.DrawLine(bright, pvx, pvy, x2, y2, nullptr, tk);
        pvx = x2;
        pvy = y2;
        ang += 8.f;
        ra += scale * 0.003f;
      }
      break;
    }
    case 4:
    {
      for (int j = 0; j < curves; j++)
      {
        float t1 = j * 6.28f / curves, t2 = (j + 1) * 6.28f / curves;
        g.DrawLine(bright, cx + sinf(3 * t1) * sz, cy + sinf(4 * t1) * sz * 0.8f, cx + sinf(3 * t2) * sz,
                   cy + sinf(4 * t2) * sz * 0.8f, nullptr, tk);
      }
      break;
    }
    case 5:
    {
      struct S
      {
        float x1, y1, x2, y2;
      };
      std::vector<S> segs;
      for (int i = 0; i < 3; i++)
      {
        float a1 = (i * 120.f - 90.f) * 3.14159f / 180.f, a2 = ((i + 1) * 120.f - 90.f) * 3.14159f / 180.f;
        segs.push_back({cx + sz * cosf(a1), cy + sz * sinf(a1), cx + sz * cosf(a2), cy + sz * sinf(a2)});
      }
      for (int d = 0; d < (big ? 4 : 2); d++)
      {
        std::vector<S> n2;
        for (auto& s : segs)
        {
          float dx2 = s.x2 - s.x1, dy2 = s.y2 - s.y1;
          float ax = s.x1 + dx2 / 3, ay = s.y1 + dy2 / 3, bx = s.x1 + dx2 * 2 / 3, by = s.y1 + dy2 * 2 / 3;
          float px2 = (s.x1 + s.x2) / 2 - dy2 * 0.2887f, py2 = (s.y1 + s.y2) / 2 + dx2 * 0.2887f;
          n2.push_back({s.x1, s.y1, ax, ay});
          n2.push_back({ax, ay, px2, py2});
          n2.push_back({px2, py2, bx, by});
          n2.push_back({bx, by, s.x2, s.y2});
        }
        segs = n2;
      }
      for (auto& s : segs)
        g.DrawLine(dim, s.x1, s.y1, s.x2, s.y2, nullptr, tk);
      break;
    }
    case 6:
    {
      struct B
      {
        float x, y, a, l;
        int d;
      };
      int maxD = big ? 7 : 4;
      std::vector<B> stk;
      stk.push_back({cx, r.B - 2.f, -90.f, sz * 1.2f, 0});
      while (!stk.empty())
      {
        auto b = stk.back();
        stk.pop_back();
        if (b.d > maxD || b.l < 1.5f)
          continue;
        float rad = b.a * 3.14159f / 180.f, ex = b.x + b.l * cosf(rad), ey = b.y + b.l * sinf(rad);
        g.DrawLine(dim, b.x, b.y, ex, ey, nullptr, tk);
        stk.push_back({ex, ey, b.a - 28.f, b.l * 0.65f, b.d + 1});
        stk.push_back({ex, ey, b.a + 28.f, b.l * 0.65f, b.d + 1});
      }
      break;
    }
    case 7:
    {
      auto drawH = [&](auto&& self, float x, float y, float half, int dep) -> void {
        if (dep <= 0 || half < 1.f)
          return;
        g.DrawLine(dim, x - half, y, x + half, y, nullptr, tk);
        g.DrawLine(dim, x - half, y - half, x - half, y + half, nullptr, tk);
        g.DrawLine(dim, x + half, y - half, x + half, y + half, nullptr, tk);
        float nh = half * 0.5f;
        self(self, x - half, y - half, nh, dep - 1);
        self(self, x - half, y + half, nh, dep - 1);
        self(self, x + half, y - half, nh, dep - 1);
        self(self, x + half, y + half, nh, dep - 1);
      };
      drawH(drawH, cx, cy, std::min(r.W(), r.H()) * 0.42f, big ? 6 : 4);
      break;
    }
    case 8:
    {
      struct Seg
      {
        float x1, y1, x2, y2;
      };
      std::vector<Seg> segs;
      segs.push_back({cx - sz * 2, cy + sz * 0.3f, cx + sz * 2, cy + sz * 0.3f});
      for (int d = 0; d < (big ? 10 : 6); d++)
      {
        std::vector<Seg> n2;
        for (auto& s : segs)
        {
          float mx = (s.x1 + s.x2) / 2 + (s.y2 - s.y1) / 2, my = (s.y1 + s.y2) / 2 - (s.x2 - s.x1) / 2;
          n2.push_back({s.x1, s.y1, mx, my});
          n2.push_back({mx, my, s.x2, s.y2});
        }
        segs = n2;
      }
      for (auto& s : segs)
        g.DrawLine(bright, s.x1, s.y1, s.x2, s.y2, nullptr, 0.8f);
      break;
    }
    case 9:
    {
      float step = big ? 2.f : 3.f;
      float pw = r.W() * 0.85f, ph = r.H() * 0.85f;
      float pl = cx - pw / 2, pt = cy - ph / 2;
      for (float px = 0; px < pw; px += step)
        for (float py = 0; py < ph; py += step)
        {
          double cr = -0.745 + (px / pw - 0.5) * 0.008, ci = 0.186 + (py / ph - 0.5) * 0.008;
          double zr = 0, zi = 0;
          int it = 0;
          while (zr * zr + zi * zi < 4 && it < 40)
          {
            double t = zr * zr - zi * zi + cr;
            zi = 2 * zr * zi + ci;
            zr = t;
            it++;
          }
          if (it < 40 && it > 3)
            g.FillRect(IColor(it * 4, 80 + it, std::min(255, 180 + it * 2), 220),
                       IRECT(pl + px, pt + py, pl + px + step - 0.5f, pt + py + step - 0.5f));
        }
      break;
    }
    case 10:
    {
      float step = big ? 2.f : 3.f;
      float pw = r.W() * 0.85f, ph = r.H() * 0.85f;
      float pl = cx - pw / 2, pt = cy - ph / 2;
      for (float px = 0; px < pw; px += step)
        for (float py = 0; py < ph; py += step)
        {
          double zr = (px / pw - 0.5) * 3, zi = (py / ph - 0.5) * 2.4;
          int it = 0;
          while (zr * zr + zi * zi < 4 && it < 30)
          {
            double t = zr * zr - zi * zi - 0.7;
            zi = 2 * zr * zi + 0.27015;
            zr = t;
            it++;
          }
          if (it < 30 && it > 2)
            g.FillRect(IColor(it * 6, 70 + it * 2, std::min(255, 160 + it * 3), 220),
                       IRECT(pl + px, pt + py, pl + px + step - 0.5f, pt + py + step - 0.5f));
        }
      break;
    }
    case 11:
    {
      const double a = -1.4, b = 1.6, c = 1.0, d = 0.75;
      double x = 0.0, y = 0.0;
      float sc2 = std::min(r.W(), r.H()) * 0.28f;
      for (int i = 0; i < (big ? 8000 : 2200); i++)
      {
        double nx = sin(a * y) + c * cos(a * x), ny = sin(b * x) + d * cos(b * y);
        x = nx;
        y = ny;
        if (i < 120)
          continue;
        float px = cx + (float)x * sc2, py = cy - (float)y * sc2;
        if (px > r.L && px < r.R && py > r.T && py < r.B)
        {
          int al = 28 + (i & 95);
          g.FillRect(IColor(al, 75 + (i % 90), 165 + (i % 85), 215), IRECT(px, py, px + 1.f, py + 1.f));
        }
      }
      break;
    }
    case 12:
    {
      float step = big ? 2.f : 3.f;
      float pw = r.W() * 0.85f, ph = r.H() * 0.85f;
      float pl = cx - pw / 2, pt = cy - ph / 2;
      for (float px = 0; px < pw; px += step)
        for (float py = 0; py < ph; py += step)
        {
          double cr = -1.75 + (px / pw) * 0.15, ci = -0.08 + (py / ph) * 0.12;
          double zr = 0, zi = 0;
          int it = 0;
          while (zr * zr + zi * zi < 4 && it < 40)
          {
            double t = zr * zr - zi * zi + cr;
            zi = fabs(2 * zr * zi) + ci;
            zr = t;
            it++;
          }
          if (it < 40 && it > 2)
            g.FillRect(IColor(it * 5, 100 + it * 2, std::min(255, 170 + it * 2), 230),
                       IRECT(pl + px, pt + py, pl + px + step - 0.5f, pt + py + step - 0.5f));
        }
      break;
    }
    case 13:
    {
      struct P
      {
        float x, y, r2;
      };
      std::vector<P> ps;
      ps.push_back({cx, cy, sz});
      for (int d = 0; d < (big ? 3 : 2); d++)
      {
        std::vector<P> n2;
        for (auto& p : ps)
        {
          for (int i = 0; i < 5; i++)
          {
            float a1 = (i * 72.f - 90) * 3.14159f / 180, a2 = ((i + 1) * 72.f - 90) * 3.14159f / 180;
            g.DrawLine(dim, p.x + p.r2 * cosf(a1), p.y + p.r2 * sinf(a1), p.x + p.r2 * cosf(a2), p.y + p.r2 * sinf(a2),
                       nullptr, tk);
          }
          float nr = p.r2 * 0.382f;
          n2.push_back({p.x, p.y, nr});
          for (int i = 0; i < 5; i++)
          {
            float a = (i * 72.f - 90) * 3.14159f / 180;
            n2.push_back({p.x + (p.r2 - nr) * cosf(a), p.y + (p.r2 - nr) * sinf(a), nr});
          }
        }
        ps = n2;
      }
      break;
    }
    default:
    { // case 14 - Lichtenberg discharge (Diezel Herbert)
      // Two opposing seeds with branching arcs
      struct Pt
      {
        float x, y;
      };
      std::vector<Pt> pts;
      pts.push_back({cx, r.B - 1.f});
      pts.push_back({cx, r.T + 1.f});
      unsigned rng = 0xBEEFu;
      int iters = big ? 220 : 90;
      float step = big ? 2.4f : 1.4f;
      for (int i = 0; i < iters; i++)
      {
        rng = rng * 1664525u + 1013904223u;
        Pt& parent = pts[rng % pts.size()];
        rng = rng * 1664525u + 1013904223u;
        bool fromBottom = (parent.y > cy);
        float baseAng = fromBottom ? -90.f : 90.f;
        float jitter = ((float)(rng % 100) / 100.f - 0.5f) * 110.f;
        float ang = (baseAng + jitter) * 3.14159f / 180.f;
        Pt next{parent.x + cosf(ang) * step, parent.y + sinf(ang) * step};
        if (next.x < r.L + 1.f || next.x > r.R - 1.f || next.y < r.T + 1.f || next.y > r.B - 1.f)
          continue;
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
      for (int i = 0; i < 6; i++)
      {
        std::vector<int> n2;
        for (auto t : turns)
          n2.push_back(t);
        n2.push_back(1);
        for (int j = (int)turns.size() - 1; j >= 0; j--)
          n2.push_back(1 - turns[j]);
        turns = n2;
      }
      float step = 1.8f, px2 = cx - 3.f, py2 = cy + 2.f;
      int dir = 0;
      const float dx[] = {step, 0, -step, 0}, dy[] = {0, -step, 0, step};
      for (int i = 0; i < (int)turns.size(); i++)
      {
        float nx = px2 + dx[dir], ny = py2 + dy[dir];
        g.DrawLine(bright, px2, py2, nx, ny, nullptr, 1.f);
        px2 = nx;
        py2 = ny;
        dir = (dir + (turns[i] ? 1 : 3)) % 4;
      }
      break;
    }
    case 1: // Bad Cat Mini Cat - two cat eyes (blue/teal only at thumbnail)
    {
      const float mn = std::min(r.W(), r.H());
      const float sep = mn * 0.24f, ew = mn * 0.17f, eh = ew * 0.5f;
      auto eye = [&](float exc) {
        std::vector<float> xs, ys;
        const int NS = 10;
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
        for (size_t i = 1; i < xs.size(); i++)
          g.DrawLine(bright, xs[i - 1], ys[i - 1], xs[i], ys[i], nullptr, 1.f);
        g.DrawLine(bright, xs.back(), ys.back(), xs.front(), ys.front(), nullptr, 1.f);
        g.FillEllipse(bright, IRECT(exc - ew * 0.13f, cy - eh * 0.92f, exc + ew * 0.13f, cy + eh * 0.92f));
      };
      eye(cx - sep);
      eye(cx + sep);
      break;
    }
    case 2: // Fern mini (scatter dots)
    {
      float px2 = 0, py2 = 0;
      unsigned rng = 42;
      for (int i = 0; i < 300; i++)
      {
        rng = rng * 1103515245 + 12345;
        float rv = (float)(rng % 1000) / 1000.f;
        float nx, ny;
        if (rv < 0.01f)
        {
          nx = 0;
          ny = 0.16f * py2;
        }
        else if (rv < 0.86f)
        {
          nx = 0.85f * px2 + 0.04f * py2;
          ny = -0.04f * px2 + 0.85f * py2 + 1.6f;
        }
        else if (rv < 0.93f)
        {
          nx = 0.2f * px2 - 0.26f * py2;
          ny = 0.23f * px2 + 0.22f * py2 + 1.6f;
        }
        else
        {
          nx = -0.15f * px2 + 0.28f * py2;
          ny = 0.26f * px2 + 0.24f * py2 + 0.44f;
        }
        px2 = nx;
        py2 = ny;
        float sx = cx + px2 * 3.f, sy = r.B - 2.f - py2 * 1.8f;
        if (sx > r.L && sx < r.R && sy > r.T && sy < r.B)
          g.FillRect(bright, IRECT(sx, sy, sx + 1.f, sy + 1.f));
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
      for (int j = 0; j < 30; j++)
      {
        float t1 = j * 6.28f / 30.f, t2 = (j + 1) * 6.28f / 30.f;
        g.DrawLine(bright, cx + sinf(3 * t1) * sz, cy + sinf(4 * t1) * sz * 0.8f, cx + sinf(3 * t2) * sz,
                   cy + sinf(4 * t2) * sz * 0.8f, nullptr, 1.f);
      }
      break;
    }
    case 5: // Koch snowflake mini (bigger, 3 depth) (Lichtlaerm Prometheus)
    {
      struct S
      {
        float x1, y1, x2, y2;
      };
      std::vector<S> segs;
      const float rr = std::min(r.W(), r.H()) * 0.46f;
      for (int i = 0; i < 3; i++)
      {
        float a1 = (i * 120.f - 90.f) * 3.14159f / 180.f, a2 = ((i + 1) * 120.f - 90.f) * 3.14159f / 180.f;
        segs.push_back({cx + rr * cosf(a1), cy + rr * sinf(a1), cx + rr * cosf(a2), cy + rr * sinf(a2)});
      }
      for (int d = 0; d < 3; d++)
      {
        std::vector<S> n2;
        for (auto& s : segs)
        {
          float dx2 = s.x2 - s.x1, dy2 = s.y2 - s.y1;
          float ax = s.x1 + dx2 / 3, ay = s.y1 + dy2 / 3, bx = s.x1 + dx2 * 2 / 3, by = s.y1 + dy2 * 2 / 3;
          float px2 = (s.x1 + s.x2) / 2 - dy2 * 0.2887f, py2 = (s.y1 + s.y2) / 2 + dx2 * 0.2887f;
          n2.push_back({s.x1, s.y1, ax, ay});
          n2.push_back({ax, ay, px2, py2});
          n2.push_back({px2, py2, bx, by});
          n2.push_back({bx, by, s.x2, s.y2});
        }
        segs = n2;
      }
      for (auto& s : segs)
        g.DrawLine(bright, s.x1, s.y1, s.x2, s.y2, nullptr, 1.f);
      break;
    }
    case 6: // Fractal tree mini (4 depth)
    {
      struct B
      {
        float x, y, a, l;
        int d;
      };
      std::vector<B> stk;
      stk.push_back({cx, r.B - 2.f, -90.f, sz * 1.2f, 0});
      while (!stk.empty())
      {
        auto b = stk.back();
        stk.pop_back();
        if (b.d > 4 || b.l < 1.5f)
          continue;
        float rad = b.a * 3.14159f / 180.f, ex = b.x + b.l * cosf(rad), ey = b.y + b.l * sinf(rad);
        g.DrawLine(b.d < 2 ? bright : dim, b.x, b.y, ex, ey, nullptr, 1.f);
        stk.push_back({ex, ey, b.a - 28.f, b.l * 0.65f, b.d + 1});
        stk.push_back({ex, ey, b.a + 28.f, b.l * 0.65f, b.d + 1});
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
      struct S
      {
        float x1, y1, x2, y2;
      };
      std::vector<S> segs;
      segs.push_back({cx - sz, cy + sz * 0.3f, cx + sz, cy + sz * 0.3f});
      for (int d = 0; d < 6; d++)
      {
        std::vector<S> n2;
        for (auto& s : segs)
        {
          float mx = (s.x1 + s.x2) / 2 + (s.y2 - s.y1) / 2, my = (s.y1 + s.y2) / 2 - (s.x2 - s.x1) / 2;
          n2.push_back({s.x1, s.y1, mx, my});
          n2.push_back({mx, my, s.x2, s.y2});
        }
        segs = n2;
      }
      for (auto& s : segs)
        g.DrawLine(bright, s.x1, s.y1, s.x2, s.y2, nullptr, 0.8f);
      break;
    }
    case 9: // Mandelbrot mini (pixel grid)
    {
      float step = 2.f;
      for (float px = r.L + 1; px < r.R - 1; px += step)
        for (float py = r.T + 1; py < r.B - 1; py += step)
        {
          double cr = -0.745 + ((px - r.L) / r.W() - 0.5) * 0.008, ci = 0.186 + ((py - r.T) / r.H() - 0.5) * 0.008;
          double zr = 0, zi = 0;
          int it = 0;
          while (zr * zr + zi * zi < 4 && it < 30)
          {
            double t = zr * zr - zi * zi + cr;
            zi = 2 * zr * zi + ci;
            zr = t;
            it++;
          }
          if (it < 30 && it > 3)
            g.FillRect(
              IColor(it * 8, 80 + it * 3, 180 + it * 2, 220), IRECT(px, py, px + step - 0.5f, py + step - 0.5f));
        }
      break;
    }
    case 10: // Julia mini
    {
      float step = 2.f;
      for (float px = r.L + 1; px < r.R - 1; px += step)
        for (float py = r.T + 1; py < r.B - 1; py += step)
        {
          double zr = ((px - r.L) / r.W() - 0.5) * 3, zi = ((py - r.T) / r.H() - 0.5) * 2.4;
          int it = 0;
          while (zr * zr + zi * zi < 4 && it < 25)
          {
            double t = zr * zr - zi * zi - 0.7;
            zi = 2 * zr * zi + 0.27015;
            zr = t;
            it++;
          }
          if (it < 25 && it > 2)
            g.FillRect(
              IColor(it * 10, 70 + it * 4, 160 + it * 4, 220), IRECT(px, py, px + step - 0.5f, py + step - 0.5f));
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
        if (i < 120)
          continue;
        float px = cx + (float)x * scale;
        float py = cy - (float)y * scale;
        if (px < r.L || px > r.R || py < r.T || py > r.B)
          continue;
        int al = 70 + (i % 90);
        g.FillRect(IColor(al, 90 + (i % 80), 175 + (i % 70), 220), IRECT(px, py, px + 1.f, py + 1.f));
      }
      break;
    }
    case 12: // Burning Ship mini
    {
      float step = 2.f;
      for (float px = r.L + 1; px < r.R - 1; px += step)
        for (float py = r.T + 1; py < r.B - 1; py += step)
        {
          double cr = -1.75 + ((px - r.L) / r.W()) * 0.15, ci = -0.08 + ((py - r.T) / r.H()) * 0.12;
          double zr = 0, zi = 0;
          int it = 0;
          while (zr * zr + zi * zi < 4 && it < 30)
          {
            double t = zr * zr - zi * zi + cr;
            zi = fabs(2 * zr * zi) + ci;
            zr = t;
            it++;
          }
          if (it < 30 && it > 2)
            g.FillRect(
              IColor(it * 8, 100 + it * 3, 170 + it * 2, 230), IRECT(px, py, px + step - 0.5f, py + step - 0.5f));
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
        g.DrawLine(bright, cx + cosf(a) * R * 1.05f, cyS + sinf(a) * R * 1.05f, cx + cosf(a) * R * 1.4f,
                   cyS + sinf(a) * R * 1.4f, nullptr, 1.f);
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
          if (i == steps)
          {
            nx = x1;
            ny = y1;
          }
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

// Factory hero art for a fractal case (callers pass FractalCaseForAmp(ampIdx)).
// Each art lives in its own art/VoLumArt<Amp>.h; see art/VoLumArtDispatch.h.
inline void DrawHeroFractalArt(IGraphics& g, const IRECT& rect, int ampIdx)
{
  volumart::DrawFactoryHeroArt(g, rect, ampIdx % 15);
}
