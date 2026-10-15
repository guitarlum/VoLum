#pragma once

// The library notice is a modal box. iPlug's box drops the mouse capture without
// ending a knob drag's host gesture, so OnIdle holds the notice back while a control
// is captured. A control that never releases its capture must not hold it back
// forever: after kNoticeCaptureWaitMs of unbroken capture the notice shows anyway.

#include <chrono>

namespace volum::ui
{
// Longer than any ordinary click or short drag, short enough that a stuck capture
// still gets its notice.
constexpr double kNoticeCaptureWaitMs = 5000.0;

class NoticeDeferral
{
public:
  // True when the pending notice may open now.
  bool ShouldShowAt(bool captured, double nowMs)
  {
    if (!captured)
    {
      mWaiting = false;
      return true;
    }
    if (!mWaiting || nowMs < mSinceMs)
    {
      mWaiting = true;
      mSinceMs = nowMs;
      return false;
    }
    if (nowMs - mSinceMs < kNoticeCaptureWaitMs)
      return false;
    mWaiting = false;
    return true;
  }

  bool ShouldShow(bool captured)
  {
    return ShouldShowAt(
      captured, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count());
  }

  // No notice is pending: the next one starts its own wait.
  void Reset() { mWaiting = false; }

private:
  double mSinceMs = 0.0;
  bool mWaiting = false;
};
} // namespace volum::ui
