#pragma once

#include <cmath>

namespace volum
{

constexpr double kLevelMuteEpsilonDb = 1.0e-9;

inline bool IsLevelMuteValue(double levelDb, double minimumDb)
{
  return levelDb <= minimumDb + kLevelMuteEpsilonDb;
}

inline double DbToAmpWithMuteFloor(double levelDb, double minimumDb)
{
  if (IsLevelMuteValue(levelDb, minimumDb))
    return 0.0;
  return std::pow(10.0, levelDb / 20.0);
}

} // namespace volum
