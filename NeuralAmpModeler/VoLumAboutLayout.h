#pragma once

// SYSTEM About card: pin the update action row to the card bottom so a short
// leftover never clips "Check for updates automatically" / "Check now".

#include <algorithm>

namespace volum
{

struct AboutLayout
{
  float versionT = 0.f;
  float versionB = 0.f;
  float url1T = 0.f;
  float url1B = 0.f;
  float url2T = 0.f;
  float url2B = 0.f;
  float noticeT = 0.f;
  float noticeB = 0.f;
  float actionT = 0.f;
  float actionB = 0.f;
  float actionSplitX = 0.f;
  bool actionFits = false;
};

inline constexpr float kAboutRowH = 14.f;
inline constexpr float kAboutGap = 6.f;
inline constexpr float kAboutActionH = 22.f;
inline constexpr float kAboutCheckNowW = 110.f;

inline AboutLayout LayoutAboutCard(float bodyW, float bodyH)
{
  AboutLayout l;
  const float actionB = bodyH;
  const float actionT = std::max(0.f, actionB - kAboutActionH);
  l.actionT = actionT;
  l.actionB = actionB;
  l.actionSplitX = std::max(0.f, bodyW - kAboutCheckNowW);
  l.actionFits = actionT >= 0.f && (actionB - actionT) >= kAboutActionH - 0.5f && actionB <= bodyH + 0.5f;

  // A 76 px SYSTEM leftover cannot hold version, two links, a 16 px update
  // pill, and the action row. Drop links from the bottom until the pill sits
  // fully above the action row. A 96 px card still keeps both links.
  const float noticeLimit = actionT - kAboutGap;
  const float pillH = 16.f;
  float y = 0.f;
  l.versionT = y;
  l.versionB = y + kAboutRowH;
  y = l.versionB;

  auto placeLink = [&](float& top, float& bottom) {
    const float next = y + kAboutRowH;
    if (next + kAboutGap + pillH <= noticeLimit + 0.01f)
    {
      top = y;
      bottom = next;
      y = next;
    }
    else
    {
      top = y;
      bottom = y;
    }
  };
  placeLink(l.url1T, l.url1B);
  placeLink(l.url2T, l.url2B);
  l.noticeT = y + kAboutGap;
  l.noticeB = std::max(l.noticeT, noticeLimit);
  return l;
}

} // namespace volum
