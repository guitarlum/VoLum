#pragma once

// VoLumDiagLog.h: a small, size-capped diagnostic log at
// %LOCALAPPDATA%\VoLum\volum.log (Application Support on macOS).
//
// Why: 1.2.x bug reports arrive as "it broke after the update" with nothing to go
// on. Startup, sample rate and block size, model/IR load outcomes with paths, chunk
// decode results and settings migrations are exactly the facts that turn such a
// report into a diagnosis, and they are all knowable at the moment they happen.
//
// Rules this file exists to enforce:
//
//   - NEVER call it from the audio thread. Every entry opens a file. The audio
//     thread must stay lock-free and allocation-free, so anything it wants logged
//     has to be latched and emitted from the main thread.
//   - Bounded on disk. The log rolls to volum.log.1 at kMaxBytes and keeps exactly
//     one generation, so worst case is 2 * kMaxBytes forever - no unbounded growth
//     on a machine that runs VoLum daily for a year.
//   - Never fatal. A log that throws, blocks, or fails a load because a directory
//     is read-only would be worse than no log at all. Every operation swallows its
//     errors.
//
// The rotation decision itself is a pure function (ShouldRotateLog) so the policy
// is unit-testable without touching a real file.

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>

namespace volum
{
namespace diag
{

inline constexpr std::uintmax_t kMaxLogBytes = 512 * 1024; // one rolled generation kept

// Rotate before writing when the pending line would push the file past the cap.
// Checked before rather than after so a single entry can never leave the file over
// the limit, and an oversized entry on an empty file still gets written once.
inline bool ShouldRotateLog(std::uintmax_t currentBytes, std::size_t pendingBytes, std::uintmax_t maxBytes)
{
  if (currentBytes == 0)
    return false;
  return currentBytes + static_cast<std::uintmax_t>(pendingBytes) > maxBytes;
}

inline std::string TimestampNow()
{
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto secs = clock::to_time_t(now);
  const auto ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % std::chrono::seconds(1);
  std::tm tm {};
#ifdef _WIN32
  localtime_s(&tm, &secs);
#else
  localtime_r(&secs, &tm);
#endif
  std::ostringstream os;
  os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
  return os.str();
}

// A process-wide log with an injectable path, so doctests write into a temp dir and
// the plugin resolves the real one once at startup.
class Log
{
public:
  static Log& Instance()
  {
    static Log inst;
    return inst;
  }

  // Enabling is explicit: a plugin instance that cannot resolve a writable path
  // simply stays silent rather than scattering files next to the host binary.
  void Open(const std::filesystem::path& path)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mPath = path;
    mEnabled = !path.empty();
    if (!mEnabled)
      return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
      mEnabled = false;
  }

  void Close()
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mEnabled = false;
    mPath.clear();
  }

  bool enabled() const { return mEnabled; }
  std::filesystem::path path() const { return mPath; }
  std::filesystem::path rolledPath() const { return RolledPath(mPath); }

  static std::filesystem::path RolledPath(const std::filesystem::path& p)
  {
    if (p.empty())
      return {};
    std::filesystem::path r = p;
    r += ".1";
    return r;
  }

  void Write(const char* category, const std::string& message)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mEnabled)
      return;
    const std::string line = TimestampNow() + "  [" + (category ? category : "?") + "] " + message + "\n";
    try
    {
      std::error_code ec;
      const auto size = std::filesystem::file_size(mPath, ec);
      const std::uintmax_t cur = ec ? 0u : size;
      if (ShouldRotateLog(cur, line.size(), kMaxLogBytes))
      {
        std::error_code rec;
        std::filesystem::remove(RolledPath(mPath), rec);
        std::filesystem::rename(mPath, RolledPath(mPath), rec);
        if (rec)
          std::filesystem::remove(mPath, rec); // rename blocked: drop the old generation
      }
      std::ofstream out(mPath, std::ios::app | std::ios::binary);
      if (out)
        out << line;
    }
    catch (...)
    {
      // A diagnostic log must never be the reason something fails.
    }
  }

private:
  mutable std::mutex mMutex;
  std::filesystem::path mPath;
  bool mEnabled = false;
};

inline void Write(const char* category, const std::string& message)
{
  Log::Instance().Write(category, message);
}

} // namespace diag
} // namespace volum

// Main-thread only. The name is deliberately loud: if you find yourself reaching for
// this inside ProcessBlock, latch the fact and log it from OnIdle instead.
#define VOLUM_LOG(category, message) ::volum::diag::Write(category, message)
