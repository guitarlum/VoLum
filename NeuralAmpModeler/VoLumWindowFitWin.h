#pragma once

// Windows glue for VoLumWindowFit.h: keeps the standalone's corner-grip scale limit
// matched to the work area of the monitor the window is on.

#if defined(OS_WIN) && defined(APP_API)

  #include "IGraphics.h"
  #include "VoLumWindowFit.h"

  #include <windows.h>

namespace volum
{

// Cheap enough for every idle tick: one monitor query and two window rects. Run from
// the idle rather than at layout so dragging the window to another monitor moves the
// limit with it.
inline void ApplyWorkAreaScaleLimit(iplug::igraphics::IGraphics* pGraphics)
{
  using namespace iplug::igraphics;

  if (!pGraphics)
    return;

  HWND plugWnd = static_cast<HWND>(pGraphics->GetWindow());
  if (!plugWnd)
    return;

  HWND topWnd = GetAncestor(plugWnd, GA_ROOT);
  if (!topWnd)
    topWnd = plugWnd;

  MONITORINFO info = {};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfo(MonitorFromWindow(topWnd, MONITOR_DEFAULTTONEAREST), &info))
    return;

  RECT outer = {};
  RECT client = {};
  if (!GetWindowRect(topWnd, &outer) || !GetClientRect(plugWnd, &client))
    return;

  // Everything the window carries around the plugin view: title bar, borders, menu.
  const float chromeW = static_cast<float>((outer.right - outer.left) - client.right);
  const float chromeH = static_cast<float>((outer.bottom - outer.top) - client.bottom);

  const float maxScale = MaxDrawScaleForWorkArea(
    static_cast<float>(info.rcWork.right - info.rcWork.left), static_cast<float>(info.rcWork.bottom - info.rcWork.top),
    chromeW, chromeH, static_cast<float>(pGraphics->Width()), static_cast<float>(pGraphics->Height()),
    pGraphics->GetScreenScale(), static_cast<float>(DEFAULT_MIN_DRAW_SCALE),
    static_cast<float>(DEFAULT_MAX_DRAW_SCALE));
  pGraphics->SetScaleConstraints(static_cast<float>(DEFAULT_MIN_DRAW_SCALE), maxScale);
}

} // namespace volum

#endif
