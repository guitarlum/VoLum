#include "third_party/doctest.h"
#include "../config.h"

#include "../VoLumParams.h" // the real enum: a hand-copied stub is exactly the drift this file was extracted to prevent

#include "../VoLumKeyboardModel.h"

TEST_CASE("Knob wheel accumulator emits one step per whole delta")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(1.0) == 1);
  CHECK(wheel.ResidualForTests() == doctest::Approx(0.0));
}

TEST_CASE("Knob wheel accumulator combines smooth-scroll fractions")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(0.25) == 0);
  CHECK(wheel.OnDelta(0.25) == 0);
  CHECK(wheel.OnDelta(0.25) == 0);
  CHECK(wheel.OnDelta(0.25) == 1);
  CHECK(wheel.ResidualForTests() == doctest::Approx(0.0));
}

TEST_CASE("Knob wheel accumulator drops stale remainder on direction change")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(0.4) == 0);
  CHECK(wheel.OnDelta(-0.5) == 0);
  CHECK(wheel.ResidualForTests() == doctest::Approx(-0.5));
  CHECK(wheel.OnDelta(-0.6) == -1);
  CHECK(wheel.ResidualForTests() == doctest::Approx(-0.1));
}

TEST_CASE("Knob wheel accumulator emits multiple steps for large deltas")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(3.7) == 3);
  CHECK(wheel.ResidualForTests() == doctest::Approx(0.7));
}
