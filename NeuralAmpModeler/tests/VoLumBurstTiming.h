#pragma once

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <vector>

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
  #if defined(__APPLE__)
    #include <pthread.h>
    #include <pthread/qos.h>
  #endif
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

// Hosts run the audio callback on a time-critical thread on a performance core. A loaded hybrid machine
// otherwise parks the test thread on an efficiency core for the whole sequence, which costs as much as
// the 2x regression a budget check has to catch. Windows ignores the priority and QoS hints under load,
// so the thread is also pinned to the top efficiency class there. Linux gets no hint (none unprivileged).
class ScopedRealtimeThread
{
public:
  ScopedRealtimeThread()
  {
#if defined(_WIN32)
    mSavedPriority = GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  #if defined(THREAD_POWER_THROTTLING_CURRENT_VERSION)
    mSavedThrottling.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    if (!GetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &mSavedThrottling, sizeof(mSavedThrottling)))
    {
      mSavedThrottling = {};
      mSavedThrottling.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    }
    THREAD_POWER_THROTTLING_STATE state{};
    state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = 0;
    SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &state, sizeof(state));
  #endif
  #if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0A00
    const DWORD_PTR fastest = FastestCoreMask();
    if (fastest != 0)
      mSavedAffinity = SetThreadAffinityMask(GetCurrentThread(), fastest);
  #endif
#elif defined(__APPLE__)
    pthread_get_qos_class_np(pthread_self(), &mSavedQos, &mSavedRelPriority);
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
  }
  ~ScopedRealtimeThread()
  {
#if defined(_WIN32)
    if (mSavedAffinity != 0)
      SetThreadAffinityMask(GetCurrentThread(), mSavedAffinity);
  #if defined(THREAD_POWER_THROTTLING_CURRENT_VERSION)
    SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &mSavedThrottling, sizeof(mSavedThrottling));
  #endif
    SetThreadPriority(GetCurrentThread(), mSavedPriority);
#elif defined(__APPLE__)
    pthread_set_qos_class_self_np(mSavedQos, mSavedRelPriority);
#endif
  }
  ScopedRealtimeThread(const ScopedRealtimeThread&) = delete;
  ScopedRealtimeThread& operator=(const ScopedRealtimeThread&) = delete;

private:
#if defined(_WIN32)
  // Logical processors of group 0 in the highest efficiency class: the performance cores of a hybrid CPU,
  // every processor of a uniform one.
  static DWORD_PTR FastestCoreMask()
  {
  #if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0A00
    ULONG len = 0;
    GetSystemCpuSetInformation(nullptr, 0, &len, GetCurrentProcess(), 0);
    std::vector<unsigned char> buf(len);
    if (len == 0
        || !GetSystemCpuSetInformation(
          reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buf.data()), len, &len, GetCurrentProcess(), 0))
      return 0;
    BYTE top = 0;
    DWORD_PTR mask = 0;
    for (int pass = 0; pass < 2; ++pass)
      for (ULONG off = 0; off < len;)
      {
        const auto* e = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(buf.data() + off);
        if (e->Size == 0)
          break;
        if (e->Type == CpuSetInformation && e->CpuSet.Group == 0 && e->CpuSet.LogicalProcessorIndex < 64)
        {
          if (pass == 0)
            top = std::max(top, e->CpuSet.EfficiencyClass);
          else if (e->CpuSet.EfficiencyClass == top)
            mask |= DWORD_PTR{1} << e->CpuSet.LogicalProcessorIndex;
        }
        off += e->Size;
      }
    return mask;
  #else
    return 0;
  #endif
  }

  int mSavedPriority = THREAD_PRIORITY_NORMAL;
  DWORD_PTR mSavedAffinity = 0;
  #if defined(THREAD_POWER_THROTTLING_CURRENT_VERSION)
  THREAD_POWER_THROTTLING_STATE mSavedThrottling{};
  #endif
#elif defined(__APPLE__)
  qos_class_t mSavedQos = QOS_CLASS_DEFAULT;
  int mSavedRelPriority = 0;
#endif
};

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
