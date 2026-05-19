#include "third_party/doctest.h"
#include "../VoLumLevelMute.h"

#include <cmath>

TEST_CASE("LevelMute: literal minimum maps to silence")
{
  CHECK(volum::DbToAmpWithMuteFloor(-20.0, -20.0) == doctest::Approx(0.0));
  CHECK(volum::DbToAmpWithMuteFloor(-40.0, -40.0) == doctest::Approx(0.0));
}

TEST_CASE("LevelMute: values above minimum keep normal dB gain")
{
  CHECK(volum::DbToAmpWithMuteFloor(0.0, -40.0) == doctest::Approx(1.0));
  CHECK(volum::DbToAmpWithMuteFloor(-20.0, -40.0) == doctest::Approx(0.1));
  CHECK(volum::DbToAmpWithMuteFloor(-19.9, -20.0) == doctest::Approx(std::pow(10.0, -19.9 / 20.0)));
}

TEST_CASE("LevelMute: epsilon protects exact minimum comparisons only")
{
  CHECK(volum::IsLevelMuteValue(-20.0 + volum::kLevelMuteEpsilonDb * 0.5, -20.0));
  CHECK_FALSE(volum::IsLevelMuteValue(-20.0 + 0.1, -20.0));
}
