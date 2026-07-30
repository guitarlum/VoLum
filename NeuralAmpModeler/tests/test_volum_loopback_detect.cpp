#include "third_party/doctest.h"
#include "../tools/loopback/VoLumLoopbackDetect.h"

#include <vector>

TEST_CASE("LoopbackDetect: a burst returned in the emit block itself is found at index 0")
{
  // The bug this pins: reporting the hit as a bare index makes 0 indistinguishable
  // from "nothing found", so a zero-latency loop reads as a dead cable.
  const std::vector<float> samples = {0.9f, 0.0f, 0.0f};
  const volum::Crossing hit = volum::FindFirstCrossing(samples.data(), samples.size(), 0, 0.01);
  CHECK(hit.found);
  CHECK(hit.index == 0);
}

TEST_CASE("LoopbackDetect: silence below threshold is not a hit")
{
  const std::vector<float> samples = {0.0f, 0.004f, -0.002f, 0.0f};
  const volum::Crossing hit = volum::FindFirstCrossing(samples.data(), samples.size(), 0, 0.01);
  CHECK_FALSE(hit.found);
  CHECK(hit.index == 0);
}

TEST_CASE("LoopbackDetect: search starts at the emit index and ignores earlier audio")
{
  // Anything before the burst went out cannot be the burst coming back, so a loud
  // sample there must not be timed as the round trip.
  const std::vector<float> samples = {0.9f, 0.0f, 0.0f, 0.5f, 0.0f};
  const volum::Crossing hit = volum::FindFirstCrossing(samples.data(), samples.size(), 2, 0.01);
  CHECK(hit.found);
  CHECK(hit.index == 3);
}

TEST_CASE("LoopbackDetect: an inverted return counts, since a cable may flip polarity")
{
  const std::vector<float> samples = {0.0f, -0.4f, 0.4f};
  const volum::Crossing hit = volum::FindFirstCrossing(samples.data(), samples.size(), 0, 0.01);
  CHECK(hit.found);
  CHECK(hit.index == 1);
}

TEST_CASE("LoopbackDetect: threshold is inclusive")
{
  // 0.5 is exact in both float and double, so this pins the boundary rule rather
  // than the rounding of a decimal literal.
  const std::vector<float> samples = {0.5f};
  CHECK(volum::FindFirstCrossing(samples.data(), samples.size(), 0, 0.5).found);
  CHECK_FALSE(volum::FindFirstCrossing(samples.data(), samples.size(), 0, 0.75).found);
}

TEST_CASE("LoopbackDetect: an emit index past the captured data is not a hit")
{
  // Guards the loop bound: a capture that stopped short of the emit point must
  // report nothing rather than read past the end.
  const std::vector<float> samples = {0.9f, 0.9f};
  const volum::Crossing hit = volum::FindFirstCrossing(samples.data(), samples.size(), 5, 0.01);
  CHECK_FALSE(hit.found);
}
