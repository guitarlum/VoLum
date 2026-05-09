#include "third_party/doctest.h"
#include "../VoLumMasterSafety.h"
#include <cmath>

TEST_CASE("MasterSafety: passthrough below knee")
{
  for (double x = -1.39; x <= 1.39 + 1e-12; x += 0.01)
  {
    CHECK(volum::SoftSafetyClip(x) == doctest::Approx(x));
  }
}

TEST_CASE("MasterSafety: bounded above ceiling")
{
  const double caps[] = {-1000, -100, -10, -3, -2.0001, 2.0001, 3, 10, 100, 1000};
  for (double v : caps)
  {
    CHECK(std::fabs(volum::SoftSafetyClip(v)) <= 2.0 + 1e-9);
  }
  for (double ax = 1.41; ax <= 2000.0; ax += 0.73)
  {
    CHECK(std::fabs(volum::SoftSafetyClip(ax)) <= 2.0 + 1e-9);
    CHECK(std::fabs(volum::SoftSafetyClip(-ax)) <= 2.0 + 1e-9);
  }
}

TEST_CASE("MasterSafety: sign symmetry")
{
  for (double x = -5.0; x <= 5.0 + 1e-12; x += 0.17)
  {
    const double fx = volum::SoftSafetyClip(x);
    CHECK(volum::SoftSafetyClip(-x) == doctest::Approx(-fx));
  }
}

TEST_CASE("MasterSafety: continuous at knee")
{
  constexpr double knee = 1.4;
  {
    const double yBelow = volum::SoftSafetyClip(knee - 1e-6);
    const double yAbove = volum::SoftSafetyClip(knee + 1e-6);
    CHECK(std::fabs(yBelow - yAbove) < 1e-3);
  }
  {
    const double yBelow = volum::SoftSafetyClip(-knee - 1e-6);
    const double yAbove = volum::SoftSafetyClip(-knee + 1e-6);
    CHECK(std::fabs(yBelow - yAbove) < 1e-3);
  }
}

TEST_CASE("MasterSafety: monotonic non-decreasing")
{
  double prev = volum::SoftSafetyClip(-3.0);
  for (double x = -3.0 + 0.003; x <= 3.0 + 1e-12; x += 0.003)
  {
    const double y = volum::SoftSafetyClip(x);
    CHECK(y >= prev);
    prev = y;
  }
}

TEST_CASE("MasterSafety: no NaN/Inf for finite input")
{
  const double samples[] = {0.0,
                            1e-300,
                            1e-12,
                            -0.5,
                            1.39,
                            -1.395,
                            1.4,
                            -1.4,
                            1.41,
                            -100.0,
                            1.0e100,
                            std::nextafter(1.0e100, 0.0)};
  for (double x : samples)
  {
    CHECK(std::isfinite(volum::SoftSafetyClip(x)));
  }
}

TEST_CASE("MasterSafety: idempotent above ceiling")
{
  // Very hot levels (e.g. 1000) still move on a second pass along the tanh knee. Near the knee,
  // |f(f(x)) − f(x)| is tiny because the curve is approximately identity there.
  const double x = 1.501;
  const double y1 = volum::SoftSafetyClip(x);
  const double y2 = volum::SoftSafetyClip(y1);
  CHECK(std::isfinite(y1));
  CHECK(std::isfinite(y2));
  CHECK(std::fabs(y1) <= 2.0 + 1e-9);
  CHECK(std::fabs(y2) <= 2.0 + 1e-9);
  CHECK(std::fabs(y2 - y1) < 1e-3);
}
