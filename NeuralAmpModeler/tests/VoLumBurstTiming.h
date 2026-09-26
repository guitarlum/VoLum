#pragma once

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #if defined(_M_X64) || defined(_M_IX86)
    #include <intrin.h>
  #endif
#else
  #include <time.h>
#endif

// Local-only realtime deadline checks for the burst tests. The absolute check asks whether a block's own
// work fits the deadline, so it reads the test thread's CPU time: on a shared laptop the wall clock also
// counts whole milliseconds spent running other processes inside one block. The whole measured sequence
// is still repeated and the best run's worst block kept, for anything the thread clock cannot filter.
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

// CPU time this thread has used, in microseconds. Windows counts it in TSC cycles, calibrated once against
// the steady clock; elsewhere CLOCK_THREAD_CPUTIME_ID. Falls back to the wall clock where neither exists.
inline double ThreadCpuUs()
{
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
  static const double cyclesPerUs = [] {
    const auto t0 = std::chrono::steady_clock::now();
    const unsigned long long c0 = __rdtsc();
    while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(50)) {}
    const unsigned long long c1 = __rdtsc();
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    return static_cast<double>(c1 - c0) / us;
  }();
  ULONG64 cycles = 0;
  QueryThreadCycleTime(GetCurrentThread(), &cycles);
  return static_cast<double>(cycles) / cyclesPerUs;
#elif !defined(_WIN32)
  timespec ts{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return static_cast<double>(ts.tv_sec) * 1e6 + static_cast<double>(ts.tv_nsec) * 1e-3;
#else
  return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
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
