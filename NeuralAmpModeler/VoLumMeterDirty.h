#pragma once

// Pure decode/compare helpers for NAMMeterControl's dirty gate (ticket 12 / U6).
// The drawn fill uses only the clipped average in GetValue domain (-70..-0.01 dB);
// peak values are stored but not painted by NAMMeterControl::DrawPeak.

#include "../iPlug2/IPlug/IPlugUtilities.h"

#include <cmath>

namespace volum
{

inline constexpr float kNamMeterMinDb = -70.0f;
inline constexpr float kNamMeterMaxDb = -0.01f;

// Same mapping as IVPeakAvgMeterControl::OnMsgFromDelegate for the avg channel.
inline double NamMeterClippedAvgFromAmp(double avgAmp, float lowDb = kNamMeterMinDb, float highDb = kNamMeterMaxDb)
{
  using iplug::AmpToDB;
  using iplug::Clip;
  const double avgDb = AmpToDB(avgAmp);
  const double lowPointAbs = std::fabs(static_cast<double>(lowDb));
  const double rangeDb = std::fabs(static_cast<double>(highDb) - static_cast<double>(lowDb));
  const double linearAvgPos = (avgDb + lowPointAbs) / rangeDb;
  return Clip(linearAvgPos, 0., 1.);
}

// True when OnMsgFromDelegate should call SetDirty. Covered meters update values
// but must not dirty (PLAY paints over them; BUILD return uses SetAllControlsDirty).
inline bool NamMeterNeedsDirty(double prevClippedAvg, double newClippedAvg, bool prevSafety, bool newSafety,
                               bool covered)
{
  if (covered)
    return false;
  return prevClippedAvg != newClippedAvg || prevSafety != newSafety;
}

} // namespace volum
