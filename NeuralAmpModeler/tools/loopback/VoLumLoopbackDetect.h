#pragma once

#include <cmath>
#include <cstddef>

namespace volum
{

// Result of searching a capture buffer for the returning tone burst. It is a pair
// rather than a bare index because index 0 is a legitimate hit: a driver can hand
// back the burst in the very block it went out.
struct Crossing
{
  bool found = false;
  size_t index = 0;
};

// First sample at or after `from` whose magnitude reaches `threshold`.
inline Crossing FindFirstCrossing(const float* samples, size_t count, size_t from, double threshold)
{
  for (size_t i = from; i < count; ++i)
    if (std::fabs(static_cast<double>(samples[i])) >= threshold)
      return {true, i};
  return {};
}

} // namespace volum
