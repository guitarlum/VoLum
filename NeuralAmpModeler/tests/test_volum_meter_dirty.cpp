#include "third_party/doctest.h"

#include "../VoLumMeterDirty.h"

#include "../iPlug2/IPlug/IPlugUtilities.h"

using iplug::DBToAmp;

TEST_CASE("NamMeter: same clipped avg below the floor does not need dirty")
{
  // Mic noise / decay between -90 and -70 dB all paint as empty (clipped to 0).
  const double a = volum::NamMeterClippedAvgFromAmp(DBToAmp(-80.0));
  const double b = volum::NamMeterClippedAvgFromAmp(DBToAmp(-90.0));
  CHECK(a == 0.0);
  CHECK(b == 0.0);
  CHECK_FALSE(volum::NamMeterNeedsDirty(a, b, false, false, /*covered=*/false));

  // Old IVPeakAvgMeterControl path: every sender packet always dirtied.
  const bool oldAlwaysDirty = true;
  CHECK_FALSE(oldAlwaysDirty == volum::NamMeterNeedsDirty(a, b, false, false, false));
}

TEST_CASE("NamMeter: a visible avg change needs dirty")
{
  const double quiet = volum::NamMeterClippedAvgFromAmp(DBToAmp(-80.0));
  const double loud = volum::NamMeterClippedAvgFromAmp(DBToAmp(-20.0));
  CHECK(quiet == 0.0);
  CHECK(loud > 0.0);
  CHECK(volum::NamMeterNeedsDirty(quiet, loud, false, false, false));
}

TEST_CASE("NamMeter: safety flag change needs dirty when uncovered")
{
  const double v = volum::NamMeterClippedAvgFromAmp(DBToAmp(-20.0));
  CHECK(volum::NamMeterNeedsDirty(v, v, false, true, false));
  CHECK_FALSE(volum::NamMeterNeedsDirty(v, v, true, true, false));
}

TEST_CASE("NamMeter: covered never needs dirty")
{
  const double quiet = volum::NamMeterClippedAvgFromAmp(DBToAmp(-80.0));
  const double loud = volum::NamMeterClippedAvgFromAmp(DBToAmp(-12.0));
  CHECK_FALSE(volum::NamMeterNeedsDirty(quiet, loud, false, false, /*covered=*/true));
  CHECK_FALSE(volum::NamMeterNeedsDirty(loud, loud, false, true, /*covered=*/true));
}
