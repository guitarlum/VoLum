#pragma once

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

#if defined(_WIN32)
  #include <process.h>
#else
  #include <unistd.h>
#endif

// Scratch files of every test live under one root unique to this test process (PID plus a random suffix).
// %TEMP% / TMPDIR is shared by every worktree and every parallel run on the machine, and the tests clear
// their folders before use, so a fixed name lets one run wipe another's files mid-test. Removed at exit.
namespace volum_test
{
inline const std::filesystem::path& ProcessTempRoot()
{
  struct Root
  {
    Root()
    {
#if defined(_WIN32)
      const unsigned long long pid = static_cast<unsigned long long>(_getpid());
#else
      const unsigned long long pid = static_cast<unsigned long long>(getpid());
#endif
      std::random_device rd;
      const unsigned long long salt =
        static_cast<unsigned long long>(rd())
        ^ static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
      char suffix[9];
      std::snprintf(suffix, sizeof(suffix), "%08llx", salt & 0xffffffffull);
      path = std::filesystem::temp_directory_path() / ("volum-tests-" + std::to_string(pid) + "-" + suffix);
      std::filesystem::create_directories(path);
    }
    ~Root()
    {
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
    }
    Root(const Root&) = delete;
    Root& operator=(const Root&) = delete;

    std::filesystem::path path;
  };
  static const Root root;
  return root.path;
}
} // namespace volum_test
