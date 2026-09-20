#pragma once

// Pure click protocol for the Dual Amp hero: the decision VoLumHeroImageControl
// makes on a mouse-down, and the SUPPORT-lane focus clamp the plugin applies
// straight afterwards in _VolumClampSupportFocus.
//
// Extracted so the math can be unit-tested without an IGraphics/IControl
// instance (mirrors the VoLumAmpListScroll.h / VoLumTriptychLayout.h split).
// Both halves live here on purpose. They used to be written separately and
// disagreed: the hero would only open the support picker on a lane that was
// already focused, while the clamp refused focus to a lane holding no amp. A
// lane therefore had to contain an amp before the user could put one in it, and
// the empty "ADD AMP" lane was a dead end on every platform. Keeping the clamp
// out of the unit under test is what let that ship, so ApplyHeroClick runs the
// full round trip.

namespace volum
{
namespace dualamp
{

struct LaneState
{
  bool dualActive = false;
  bool supportFocused = false;
  bool hasSupportAmp = false;
  bool pickerOpen = false;
};

enum class ClickAction
{
  None,
  ToggleDual,
  FocusMain,
  FocusSupport,
  OpenPicker,
  DismissPicker,
};

struct ClickResult
{
  ClickAction action = ClickAction::None;
  // What the hero should set its own focus flag to, before running the action.
  bool nextSupportFocused = false;
};

// SUPPORT focus is only meaningful on a lane that holds an amp: the cab row and
// the amp-knob group are shared between lanes and read this flag to decide which
// lane they describe.
inline bool ClampSupportFocus(bool supportFocused, bool hasSupportAmp)
{
  return supportFocused && hasSupportAmp;
}

// One mouse-down on the hero. `hitDualChip` wins over `hitSupportHalf`; the
// caller is responsible for the geometry that produces both.
inline ClickResult DecideHeroClick(const LaneState& state, bool hitDualChip, bool hitSupportHalf)
{
  ClickResult result;
  result.nextSupportFocused = state.supportFocused;

  if (hitDualChip)
  {
    result.action = ClickAction::ToggleDual;
    return result;
  }

  if (!state.dualActive)
    return result;

  if (!hitSupportHalf)
  {
    result.nextSupportFocused = false;
    result.action = state.pickerOpen ? ClickAction::DismissPicker : ClickAction::FocusMain;
    return result;
  }

  if (state.pickerOpen)
  {
    result.nextSupportFocused = ClampSupportFocus(true, state.hasSupportAmp);
    result.action = ClickAction::DismissPicker;
    return result;
  }

  // An empty lane offers exactly one action, and its art says so ("ADD AMP" /
  // "Choose support amp"). Opening the picker on the first click is both what
  // the user guide documents and the only reachable behaviour: focus cannot be
  // the prerequisite for filling a lane that is denied focus until it is full.
  if (!state.hasSupportAmp)
  {
    result.nextSupportFocused = false;
    result.action = ClickAction::OpenPicker;
    return result;
  }

  // A populated lane keeps the deliberate two-click protocol: focus it, then
  // click again to swap the amp. That keeps a single stray click on the SUPPORT
  // art from covering the lane the user was looking at.
  result.nextSupportFocused = true;
  result.action = state.supportFocused ? ClickAction::OpenPicker : ClickAction::FocusSupport;
  return result;
}

// The full round trip: the hero's decision, the focus write, and the clamp the
// plugin applies from _UpdateVoLumLayout before the next click can arrive.
inline LaneState ApplyHeroClick(const LaneState& state, bool hitDualChip, bool hitSupportHalf,
                                ClickAction* outAction = nullptr)
{
  const ClickResult result = DecideHeroClick(state, hitDualChip, hitSupportHalf);
  if (outAction)
    *outAction = result.action;

  LaneState next = state;
  next.supportFocused = result.nextSupportFocused;

  switch (result.action)
  {
    case ClickAction::ToggleDual:
      next.dualActive = !state.dualActive;
      break;
    case ClickAction::OpenPicker:
      next.pickerOpen = true;
      break;
    case ClickAction::DismissPicker:
      next.pickerOpen = false;
      break;
    case ClickAction::None:
    case ClickAction::FocusMain:
    case ClickAction::FocusSupport:
      break;
  }

  next.supportFocused = ClampSupportFocus(next.supportFocused, next.hasSupportAmp);
  return next;
}

} // namespace dualamp
} // namespace volum
