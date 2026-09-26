#pragma once

// VoLum: coalesce standalone settings (and calibration-default) disk writes.
// Clock is injected so tests drive time without sleeping.
//
// Rule (owner-locked): write after 500 ms with no further change, and at least
// every 2 s while changes keep arriving. Close / quit / Pack flush bypass the
// timer and write immediately.

namespace volum
{

class WriteDebounce
{
public:
  static constexpr double kQuietMs = 500.0;
  static constexpr double kMaxIntervalMs = 2000.0;

  void dirty(double nowMs)
  {
    if (!mDirty)
      mDirtySinceMs = nowMs;
    mDirty = true;
    mLastChangeMs = nowMs;
  }

  // force: close / quit / Pack — write now if anything is pending.
  bool shouldWrite(double nowMs, bool force = false) const
  {
    if (!mDirty)
      return false;
    if (force)
      return true;
    if ((nowMs - mLastChangeMs) >= kQuietMs)
      return true;
    const double anchor = mHasWritten ? mLastWriteMs : mDirtySinceMs;
    return (nowMs - anchor) >= kMaxIntervalMs;
  }

  void markWritten(double nowMs)
  {
    mDirty = false;
    mHasWritten = true;
    mLastWriteMs = nowMs;
  }

  bool isDirty() const { return mDirty; }

  void reset()
  {
    mDirty = false;
    mHasWritten = false;
    mLastChangeMs = 0.0;
    mLastWriteMs = 0.0;
    mDirtySinceMs = 0.0;
  }

private:
  bool mDirty = false;
  bool mHasWritten = false;
  double mLastChangeMs = 0.0;
  double mLastWriteMs = 0.0;
  double mDirtySinceMs = 0.0;
};

} // namespace volum
