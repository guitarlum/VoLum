#pragma once

// VoLum master-bus soft clip: last-resort peak limiting, not a musical saturator.
// Knee at ~+2.9 dBFS (1.4 linear) and ceiling ~+6.0 dBFS (2.0 linear). Design notes:
// `.cursor/plans/volum_master_safety_stage_b363e36c.plan.md`.

#include <cmath>

namespace volum
{

// Stateless, RT-safe: no heap, no locks, no statics. NaN in propagates; finite in stays finite (no Inf).
inline double SoftSafetyClip(double x)
{
  if (std::isnan(x))
    return x;

  constexpr double knee = 1.4;
  constexpr double ceil = 2.0;
  const double ax = std::fabs(x);
  if (ax < knee)
    return x;

  constexpr double span = ceil - knee;
  const double y = knee + span * std::tanh((ax - knee) / span);
  return std::copysign(y, x);
}

} // namespace volum
