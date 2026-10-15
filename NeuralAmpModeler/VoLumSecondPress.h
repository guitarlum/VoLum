#pragma once

// A second quick press on the same spot reaches a control as OnMouseDblClick, not
// as a second OnMouseDown (Windows WM_LBUTTONDBLCLK, macOS clickCount 2). The stock
// IControl::OnMouseDblClick resets a bound parameter to its default and otherwise
// does nothing, so a fast second click on a stepper, arrow or pill went missing, or
// snapped the value back.
//
// iPlug hit-tests the double-click afresh. When the first press opened, closed or
// re-laid out a surface, the double-click lands on whatever is under the cursor now,
// which never saw the first press: double-clicking an IR in a dropdown closes the
// dropdown on the first press, and the second must not toggle the pedal underneath.
// Windows only pairs two clicks that hit the same window; VoLum is one window, so a
// control replays a double-click as a press only when it took the first press too.
// iPlug's stock buttons and switches set mDblAsSingleClick, which replays every
// double-click as a press; VoLum's clear it and use this gate instead.

#include <chrono>

namespace volum::ui
{
// Above the whole range of the Windows double-click speed slider (200-900 ms) and
// the macOS default.
constexpr double kSecondPressWindowMs = 1000.0;
// A replay calls the press handler right after the Take; a Take that did not replay
// must not swallow the arming of a genuine press that comes much later.
constexpr double kReplayEntryWindowMs = 500.0;

class SecondPressGate
{
public:
  // Arms when the press handler returns, so a slow first press (a preset recall)
  // does not use up the window before the queued double-click is dispatched.
  //
  // A press that a successful Take just replayed is the second click, not a first
  // press, so it does not arm. Re-arming there let a later first press that an
  // overlay consumed pair with the next double-click and press the control.
  class Scope
  {
  public:
    explicit Scope(SecondPressGate& gate)
    : mGate(gate)
    , mReplay(gate.ConsumeReplayAt(NowMs()))
    {
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    ~Scope()
    {
      if (!mReplay)
        mGate.ArmAt(NowMs());
    }

  private:
    SecondPressGate& mGate;
    bool mReplay;
  };

  [[nodiscard]] Scope Press() { return Scope(*this); }

  // True once per armed press, within the window.
  bool Take() { return TakeAt(NowMs()); }

  void ArmAt(double nowMs)
  {
    mArmedAtMs = nowMs;
    mArmed = true;
  }

  bool TakeAt(double nowMs)
  {
    const bool taken = mArmed && nowMs >= mArmedAtMs && nowMs - mArmedAtMs <= kSecondPressWindowMs;
    mArmed = false;
    mReplay = taken;
    mReplayAtMs = nowMs;
    return taken;
  }

  // True once for the press that follows a successful Take straight away. A Take
  // whose handler does not replay a press leaves the mark behind; it lapses.
  bool ConsumeReplayAt(double nowMs)
  {
    const bool replay = mReplay && nowMs >= mReplayAtMs && nowMs - mReplayAtMs <= kReplayEntryWindowMs;
    mReplay = false;
    return replay;
  }

private:
  static double NowMs()
  {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  double mArmedAtMs = 0.0;
  double mReplayAtMs = 0.0;
  bool mArmed = false;
  bool mReplay = false;
};

// Replays a double-click as a press inside the host edit gesture that
// IGraphics::OnMouseDown gives a real mouse-down on a parameter-bound control.
template <typename Control, typename Mod>
void PressAgain(Control& control, float x, float y, const Mod& mod)
{
  const int paramIdx = control.GetParamIdx();
  if (paramIdx >= 0)
    control.GetDelegate()->BeginInformHostOfParamChangeFromUI(paramIdx);
  control.OnMouseDown(x, y, mod);
  if (paramIdx >= 0)
    control.GetDelegate()->EndInformHostOfParamChangeFromUI(paramIdx);
}
} // namespace volum::ui
