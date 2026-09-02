#pragma once

// Pack modal geometry: status sits above the go button without overlapping
// the "also including" band, and the list has a reserved scrollbar gutter.

#include <algorithm>

namespace volum
{
namespace packui
{

inline constexpr float kGoH = 30.f;
inline constexpr float kGoBottomPad = 16.f;
inline constexpr float kStatusH = 16.f;
inline constexpr float kAlsoH = 50.f;
inline constexpr float kAlsoGap = 8.f;
inline constexpr float kListPad = 8.f;

// SYSTEM Content library card: buttons + two help lines. The mid-row card
// must be at least this body height after _AddCard's pad and caption.
inline constexpr float kPackRowBtnH = 28.f;
inline constexpr float kPackRowBtnToHelp = 8.f;
inline constexpr float kPackRowHelpLineH = 14.f;
inline constexpr float kPackRowHelpLineGap = 13.f;
inline constexpr float kSettingsCardPadY = 12.f;
inline constexpr float kSettingsCardCapH = 24.f;

inline float PackRowMinBodyH()
{
  return kPackRowBtnH + kPackRowBtnToHelp + kPackRowHelpLineH + kPackRowHelpLineGap;
}

inline float SettingsCardBodyH(float cardH)
{
  return cardH - 2.f * kSettingsCardPadY - kSettingsCardCapH;
}

inline float SystemMidRowH()
{
  return std::max(120.f, PackRowMinBodyH() + 2.f * kSettingsCardPadY + kSettingsCardCapH);
}

struct PackChrome
{
  float goT = 0.f;
  float goB = 0.f;
  float statusT = 0.f;
  float statusB = 0.f;
  float alsoT = 0.f;
  float alsoB = 0.f;
  float listB = 0.f;
  bool statusAboveAlso = false;
};

inline PackChrome LayoutPackChrome(float boxT, float boxB, bool showAlso)
{
  PackChrome c;
  c.goB = boxB - kGoBottomPad;
  c.goT = c.goB - kGoH;
  c.statusB = c.goT - 4.f;
  c.statusT = c.statusB - kStatusH;
  if (showAlso)
  {
    c.alsoB = c.statusT - kAlsoGap;
    c.alsoT = c.alsoB - kAlsoH;
    c.listB = c.alsoT - kListPad;
  }
  else
  {
    c.alsoT = c.alsoB = c.statusT;
    c.listB = c.statusT - kListPad;
  }
  c.statusAboveAlso = !showAlso || c.statusT >= c.alsoB - 0.5f;
  return c;
}

} // namespace packui
} // namespace volum
