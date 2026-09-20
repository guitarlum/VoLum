#include "third_party/doctest.h"

#include "../VoLumDualAmpInput.h"

using namespace volum::dualamp;

namespace
{
LaneState DualWithEmptySupport()
{
  LaneState s;
  s.dualActive = true;
  return s;
}

LaneState DualWithSupportAmp()
{
  LaneState s;
  s.dualActive = true;
  s.hasSupportAmp = true;
  return s;
}

constexpr bool kChip = true;
constexpr bool kNotChip = false;
constexpr bool kSupportHalf = true;
constexpr bool kMainHalf = false;
} // namespace

TEST_CASE("An empty SUPPORT lane opens the picker on the very first click")
{
  // Shipped in v1.2.2 and reported from macOS as "my clicks aren't registering":
  // the hero only opened the picker on a lane that was already focused, and the
  // clamp refused focus to a lane holding no amp. A user who had never chosen a
  // support amp could not choose one.
  const auto decision = DecideHeroClick(DualWithEmptySupport(), kNotChip, kSupportHalf);
  CHECK(decision.action == ClickAction::OpenPicker);
}

TEST_CASE("The focus clamp cannot starve the picker across repeated clicks")
{
  // The round trip is the part that broke. Each click ends with the clamp the
  // plugin applies from _UpdateVoLumLayout, so a protocol that depends on focus
  // surviving an empty lane deadlocks. Clicking forever must never stop working.
  LaneState state = DualWithEmptySupport();
  for (int i = 0; i < 4; ++i)
  {
    ClickAction action = ClickAction::None;
    state = ApplyHeroClick(state, kNotChip, kSupportHalf, &action);
    // Odd clicks open the picker, even clicks dismiss it again. Neither is ever
    // a no-op, and the picker is reachable from any point in the cycle.
    CHECK(action == (i % 2 == 0 ? ClickAction::OpenPicker : ClickAction::DismissPicker));
    CHECK(state.pickerOpen == (i % 2 == 0));
    // The clamp still holds: an empty lane never claims focus.
    CHECK_FALSE(state.supportFocused);
  }
}

TEST_CASE("A populated SUPPORT lane keeps the two-click focus-then-swap protocol")
{
  LaneState state = DualWithSupportAmp();

  ClickAction first = ClickAction::None;
  state = ApplyHeroClick(state, kNotChip, kSupportHalf, &first);
  CHECK(first == ClickAction::FocusSupport);
  CHECK(state.supportFocused);
  CHECK_FALSE(state.pickerOpen);

  ClickAction second = ClickAction::None;
  state = ApplyHeroClick(state, kNotChip, kSupportHalf, &second);
  CHECK(second == ClickAction::OpenPicker);
  CHECK(state.pickerOpen);
  CHECK(state.supportFocused);
}

TEST_CASE("A click on the SUPPORT lane with the picker open closes it")
{
  LaneState empty = DualWithEmptySupport();
  empty.pickerOpen = true;
  CHECK(DecideHeroClick(empty, kNotChip, kSupportHalf).action == ClickAction::DismissPicker);

  LaneState populated = DualWithSupportAmp();
  populated.supportFocused = true;
  populated.pickerOpen = true;
  const auto decision = DecideHeroClick(populated, kNotChip, kSupportHalf);
  CHECK(decision.action == ClickAction::DismissPicker);
  // Dismissing from the SUPPORT half leaves the lane focused; it is still the
  // lane the user is looking at.
  CHECK(decision.nextSupportFocused);
}

TEST_CASE("A click on the MAIN lane moves focus and dismisses an open picker")
{
  LaneState state = DualWithSupportAmp();
  state.supportFocused = true;

  ClickAction focusBack = ClickAction::None;
  state = ApplyHeroClick(state, kNotChip, kMainHalf, &focusBack);
  CHECK(focusBack == ClickAction::FocusMain);
  CHECK_FALSE(state.supportFocused);

  state.pickerOpen = true;
  state.supportFocused = true;
  ClickAction dismiss = ClickAction::None;
  state = ApplyHeroClick(state, kNotChip, kMainHalf, &dismiss);
  CHECK(dismiss == ClickAction::DismissPicker);
  CHECK_FALSE(state.pickerOpen);
  CHECK_FALSE(state.supportFocused);
}

TEST_CASE("The DUAL chip wins over the lane underneath it")
{
  // The chip sits inside the MAIN lane when dual is engaged, and inside the
  // mono hero when it is not. Either way it must toggle rather than focus.
  LaneState state = DualWithSupportAmp();
  ClickAction action = ClickAction::None;
  state = ApplyHeroClick(state, kChip, kMainHalf, &action);
  CHECK(action == ClickAction::ToggleDual);
  CHECK_FALSE(state.dualActive);

  LaneState mono;
  ClickAction monoAction = ClickAction::None;
  mono = ApplyHeroClick(mono, kChip, kMainHalf, &monoAction);
  CHECK(monoAction == ClickAction::ToggleDual);
  CHECK(mono.dualActive);
}

TEST_CASE("A mono hero ignores lane clicks entirely")
{
  LaneState mono; // dualActive = false
  CHECK(DecideHeroClick(mono, kNotChip, kSupportHalf).action == ClickAction::None);
  CHECK(DecideHeroClick(mono, kNotChip, kMainHalf).action == ClickAction::None);
}

TEST_CASE("SUPPORT focus survives only on a lane that holds an amp")
{
  CHECK(ClampSupportFocus(/*supportFocused=*/true, /*hasSupportAmp=*/true));
  CHECK_FALSE(ClampSupportFocus(true, false));
  CHECK_FALSE(ClampSupportFocus(false, true));
  CHECK_FALSE(ClampSupportFocus(false, false));
}

TEST_CASE("Losing the support amp drops a focus the lane can no longer justify")
{
  // Switching MAIN to an amp whose scene carries no partner drops the support
  // amp while SUPPORT is still focused. The next click must behave as an empty
  // lane, not as a focused one.
  LaneState state = DualWithSupportAmp();
  state.supportFocused = true;
  state.hasSupportAmp = false;

  ClickAction action = ClickAction::None;
  state = ApplyHeroClick(state, kNotChip, kSupportHalf, &action);
  CHECK(action == ClickAction::OpenPicker);
  CHECK_FALSE(state.supportFocused);
}
