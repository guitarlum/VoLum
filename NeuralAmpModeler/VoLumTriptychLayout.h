#pragma once

#include "VoLumTriptychState.h"

namespace volum::triptych_layout
{
struct Rect
{
  float L = 0.f;
  float T = 0.f;
  float R = 0.f;
  float B = 0.f;

  Rect() = default;
  Rect(float left, float top, float right, float bottom) : L(left), T(top), R(right), B(bottom) {}

  float W() const { return R - L; }
  float H() const { return B - T; }
  float MW() const { return (L + R) / 2.f; }
  float MH() const { return (T + B) / 2.f; }

  template<typename TargetRect>
  TargetRect As() const { return TargetRect(L, T, R, B); }
};

static constexpr float kTriptychW = 620.f;
static constexpr float kTriptychH = 196.f;
static constexpr float kSideStripW = 100.f;
static constexpr float kGap = 10.f;
static constexpr float kAmpExpandedW = 400.f;
static constexpr float kSectionExpandedW = 430.f;
static constexpr float kCollapsedAmpW = 70.f;

static constexpr float kCardPad = 14.f;
static constexpr float kPreCardGap = 8.f;
static constexpr float kPostCardGap = 10.f;
static constexpr float kCardTopInset = 24.f;
static constexpr float kCardBottomInset = 8.f;
static constexpr float kConnectorHalfH = 6.f;

struct Frames
{
  Rect pre;
  Rect amp;
  Rect post;
};

struct PreCards
{
  Rect pitch;
  Rect comp;
  Rect nam1;
  Rect nam2;
  Rect connector1;
  Rect connector2;
  Rect connector3;
};

struct PostCards
{
  Rect delay;
  Rect reverb;
  Rect connector;
};

inline Rect BoundsForCenter(float centerX, float top)
{
  return Rect(centerX - kTriptychW / 2.f, top, centerX + kTriptychW / 2.f, top + kTriptychH);
}

template<typename T>
inline Rect FromRect(const T& rect)
{
  return Rect(rect.L, rect.T, rect.R, rect.B);
}

inline Frames ComputeFrames(const Rect& bounds, EVoLumSection expandedSection)
{
  const float cx = bounds.MW();
  Rect preRect;
  Rect ampRect;
  Rect postRect;

  if (expandedSection == EVoLumSection::AMP)
  {
    const float totalW = kSideStripW + kGap + kAmpExpandedW + kGap + kSideStripW;
    const float left = cx - totalW / 2.f;
    preRect = Rect(left, bounds.T, left + kSideStripW, bounds.B);
    ampRect = Rect(preRect.R + kGap, bounds.T, preRect.R + kGap + kAmpExpandedW, bounds.B);
    postRect = Rect(ampRect.R + kGap, bounds.T, ampRect.R + kGap + kSideStripW, bounds.B);
  }
  else if (expandedSection == EVoLumSection::POST)
  {
    const float totalW = kSideStripW + kGap + kCollapsedAmpW + kGap + kSectionExpandedW;
    const float left = cx - totalW / 2.f;
    preRect = Rect(left, bounds.T, left + kSideStripW, bounds.B);
    ampRect = Rect(preRect.R + kGap, bounds.T, preRect.R + kGap + kCollapsedAmpW, bounds.B);
    postRect = Rect(ampRect.R + kGap, bounds.T, ampRect.R + kGap + kSectionExpandedW, bounds.B);
  }
  else
  {
    const float totalW = kSectionExpandedW + kGap + kCollapsedAmpW + kGap + kSideStripW;
    const float left = cx - totalW / 2.f;
    preRect = Rect(left, bounds.T, left + kSectionExpandedW, bounds.B);
    ampRect = Rect(preRect.R + kGap, bounds.T, preRect.R + kGap + kCollapsedAmpW, bounds.B);
    postRect = Rect(ampRect.R + kGap, bounds.T, ampRect.R + kGap + kSideStripW, bounds.B);
  }

  return {preRect, ampRect, postRect};
}

inline PreCards ComputePreCards(const Rect& preRect)
{
  const float cardTop = preRect.T + kCardTopInset;
  const float cardBot = preRect.B - kCardBottomInset;
  const float cardH = cardBot - cardTop;
  const float cardL = preRect.L + kCardPad;

  // Mixed-width 4-card PRE strip: slim PITCH + slim COMP utility pedals, wider NAM 1 / NAM 2
  // amp blocks (they carry the focus art and longer names). Widths sum to the inner span minus
  // the three inter-card gaps. Slim cards take ~41% combined, wide cards ~59%.
  const float innerCards = preRect.W() - kCardPad * 2.f - kPreCardGap * 3.f;
  const float slimW = innerCards * 0.205f;
  const float wideW = innerCards * 0.295f;

  const Rect pitch(cardL, cardTop, cardL + slimW, cardTop + cardH);
  const Rect comp(pitch.R + kPreCardGap, cardTop, pitch.R + kPreCardGap + slimW, cardTop + cardH);
  const Rect nam1(comp.R + kPreCardGap, cardTop, comp.R + kPreCardGap + wideW, cardTop + cardH);
  const Rect nam2(nam1.R + kPreCardGap, cardTop, nam1.R + kPreCardGap + wideW, cardTop + cardH);
  const Rect connector1(pitch.R, preRect.MH() - kConnectorHalfH, comp.L, preRect.MH() + kConnectorHalfH);
  const Rect connector2(comp.R, preRect.MH() - kConnectorHalfH, nam1.L, preRect.MH() + kConnectorHalfH);
  const Rect connector3(nam1.R, preRect.MH() - kConnectorHalfH, nam2.L, preRect.MH() + kConnectorHalfH);

  return {pitch, comp, nam1, nam2, connector1, connector2, connector3};
}

inline PostCards ComputePostCards(const Rect& postRect)
{
  const float cardTop = postRect.T + kCardTopInset;
  const float cardBot = postRect.B - kCardBottomInset;
  const float cardH = cardBot - cardTop;
  const float cardW = (postRect.W() - kCardPad * 2.f - kPostCardGap) / 2.f;
  const float cardL = postRect.L + kCardPad;

  const Rect delay(cardL, cardTop, cardL + cardW, cardTop + cardH);
  const Rect reverb(delay.R + kPostCardGap, cardTop, delay.R + kPostCardGap + cardW, cardTop + cardH);
  const Rect connector(delay.R, postRect.MH() - kConnectorHalfH, reverb.L, postRect.MH() + kConnectorHalfH);

  return {delay, reverb, connector};
}

} // namespace volum::triptych_layout
