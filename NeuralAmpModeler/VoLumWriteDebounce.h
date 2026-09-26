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
    // This burst only. Anchoring on the previous write makes the first edit
    // after a long idle flush immediately, because that write is already > 2 s old.
    return (nowMs - mDirtySinceMs) >= kMaxIntervalMs;
  }

  void markWritten(double /*nowMs*/) { mDirty = false; }

  bool isDirty() const { return mDirty; }

  void reset()
  {
    mDirty = false;
    mLastChangeMs = 0.0;
    mDirtySinceMs = 0.0;
  }

private:
  bool mDirty = false;
  double mLastChangeMs = 0.0;
  double mDirtySinceMs = 0.0;
};

} // namespace volum
