#include "third_party/doctest.h"
#include "../VoLumNanGuard.h"

#include <cmath>
#include <limits>
#include <vector>

TEST_CASE("NanGuard: NaN/Inf samples are replaced with 0 and reported")
{
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double posInf = std::numeric_limits<double>::infinity();
  const double negInf = -posInf;
  std::vector<double> buf = {0.5, nan, -0.5, posInf, 0.25, negInf};
  CHECK(volum::ScrubNonFiniteInPlace(buf.data(), buf.size()));
  CHECK(buf[0] == doctest::Approx(0.5));
  CHECK(buf[1] == 0.0);
  CHECK(buf[2] == doctest::Approx(-0.5));
  CHECK(buf[3] == 0.0);
  CHECK(buf[4] == doctest::Approx(0.25));
  CHECK(buf[5] == 0.0);
}

TEST_CASE("NanGuard: multi-channel overload scrubs every channel")
{
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> chL = {0.1, 0.2, nan, 0.4};
  std::vector<double> chR = {0.5, 0.6, 0.7, 0.8};
  double* channels[2] = {chL.data(), chR.data()};
  CHECK(volum::ScrubNonFiniteInPlace(channels, 2, chL.size()));
  CHECK(chL[2] == 0.0);
  CHECK(chR[2] == doctest::Approx(0.7));
}

TEST_CASE("NanGuard: empty buffer is a no-op")
{
  std::vector<double> buf;
  CHECK_FALSE(volum::ScrubNonFiniteInPlace(buf.data(), buf.size()));
}

TEST_CASE("NanGuard: null buffer is a no-op")
{
  CHECK_FALSE(volum::ScrubNonFiniteInPlace(static_cast<double*>(nullptr), 0));
  CHECK_FALSE(volum::ScrubNonFiniteInPlace(static_cast<float*>(nullptr), 16));
}

TEST_CASE("NanGuard: float overload works the same as double")
{
  const float nanF = std::numeric_limits<float>::quiet_NaN();
  std::vector<float> buf = {0.1f, nanF, 0.3f};
  CHECK(volum::ScrubNonFiniteInPlace(buf.data(), buf.size()));
  CHECK(buf[0] == doctest::Approx(0.1f));
  CHECK(buf[1] == 0.0f);
  CHECK(buf[2] == doctest::Approx(0.3f));
}
