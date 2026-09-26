#pragma once

#include <algorithm>
#include <cstdlib>
#include <limits>

// Local-only realtime deadline checks for the burst tests. A block timed by the wall clock also counts
// any OS preemption that lands inside it, so the absolute check repeats the whole measured sequence and
// keeps the best run's worst block: a real overrun shows in every run, a preemption in one.
namespace volum_test
{
constexpr int kDeadlineRuns = 3;

inline bool OnCi()
{
  return std::getenv("CI") != nullptr || std::getenv("GITHUB_ACTIONS") != nullptr;
}

inline double DeadlineUs(double frames, double sampleRate)
{
  return 1e6 * frames / sampleRate;
}

// Hosted runners share cores and skip the absolute check, so one run feeds only the ratio there.
inline int DeadlineRuns()
{
  return OnCi() ? 1 : kDeadlineRuns;
}

// measure(run) performs one full measured sequence and returns its worst block in microseconds; run 0
// is the one the caller's ratio check uses. Returns the smallest of the runs' worst blocks.
template <typename Measure>
double MinWorstBlockUs(int runs, Measure&& measure)
{
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < runs; ++run)
    best = std::min(best, measure(run));
  return best;
}
} // namespace volum_test
