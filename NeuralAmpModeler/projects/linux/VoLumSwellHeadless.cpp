// Headless SWELL does not define swell_oswindow_maximize. That function lives
// in swell-generic-gdk.cpp, which is the real window backend (ticket 07).
// This no-op lets the standalone link without GDK.

#include "swell.h"

void swell_oswindow_maximize(HWND hwnd, bool wantmax)
{
  (void)hwnd;
  (void)wantmax;
}
