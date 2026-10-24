#pragma once

// Peak residual of a live render against the committed Windows reference.
// Windows Release writes the file only when VOLUM_WRITE_PLATFORM_NULL=1, and
// otherwise requires the file to match the live render byte for byte. Linux
// requires the peak |live-windows| to be <= -100 dBFS, with 0 dBFS meaning
// amplitude 1, including ASan. Other platforms leave the file alone.
//
// Linux exact hashes are pinned to one reference build: the CI Linux tests job
// on ubuntu-22.04. The identity is the glibc version plus the compiler
// __VERSION__ string. A different identity skips only the hash compare.

#include "third_party/doctest.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#if defined(__linux__)
  #include <gnu/libc-version.h>
#endif

namespace volum::test
{
inline constexpr double kWindowsNullPeakDb = -100.0;

// glibc version + compiler __VERSION__, filled from the ubuntu-22.04 archive
// GCC that the Linux tests job runs. Updated when that job's log shows a
// different string.
inline const char* LinuxHashReferenceIdentity()
{
  return "glibc 2.35 | 15.2.0";
}

inline std::string LinuxRuntimeIdentity()
{
#if defined(__linux__)
  if (const char* fake = std::getenv("VOLUM_LINUX_HASH_IDENTITY_FAKE"))
    return std::string(fake);
  return std::string("glibc ") + gnu_get_libc_version() + " | " + __VERSION__;
#else
  return {};
#endif
}

// True when this Linux build must CHECK the exact pin. A mismatch prints both
// identities and skips the compare, unless VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1,
// which fails the case. Windows and macOS do not call this.
inline bool RequireLinuxHash(const char* caseName)
{
#if !defined(__linux__)
  (void)caseName;
  return true;
#else
  static bool announced = false;
  if (!announced)
  {
    announced = true;
    MESSAGE(std::string("Linux hash identity runtime=\"") + LinuxRuntimeIdentity() + "\" reference=\""
            + LinuxHashReferenceIdentity() + "\"");
  }
  if (LinuxRuntimeIdentity() == LinuxHashReferenceIdentity())
    return true;
  const char* require = std::getenv("VOLUM_REQUIRE_LINUX_HASH_REFERENCE");
  const bool required = require != nullptr && std::strcmp(require, "1") == 0;
  MESSAGE(std::string("Linux hash identity mismatch for ") + caseName + ": runtime=\"" + LinuxRuntimeIdentity()
          + "\" reference=\"" + LinuxHashReferenceIdentity() + "\"");
  if (required)
    FAIL_CHECK("VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1 but this build is not the Linux hash reference");
  return required;
#endif
}

inline bool WindowsNullActive()
{
#if defined(_WIN32) && defined(NDEBUG)
  return true;
#elif defined(__linux__)
  return true;
#else
  return false;
#endif
}

inline std::filesystem::path WindowsNullPath(const char* name)
{
  std::string slug;
  for (const char* p = name; *p; ++p)
  {
    const unsigned char c = static_cast<unsigned char>(*p);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '+')
      slug.push_back(static_cast<char>(c));
    else
      slug.push_back('_');
  }
  // .wav so the vendor-ref hook treats the capture as a binary, the same way it skips rigs.
  return std::filesystem::path(__FILE__).parent_path() / "golden" / "platform-null" / (slug + ".wav");
}

inline void WriteFloat64Wav(const std::filesystem::path& path, const double* samples, std::uint64_t count)
{
  const std::uint32_t dataBytes = static_cast<std::uint32_t>(count * sizeof(double));
  const std::uint32_t riffSize = 36u + dataBytes;
  const std::uint16_t formatIeee = 3;
  const std::uint16_t channels = 1;
  const std::uint32_t rate = 48000;
  const std::uint32_t byteRate = rate * sizeof(double);
  const std::uint16_t align = sizeof(double);
  const std::uint16_t bits = 64;
  const std::uint32_t fmtSize = 16;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write("RIFF", 4);
  out.write(reinterpret_cast<const char*>(&riffSize), 4);
  out.write("WAVE", 4);
  out.write("fmt ", 4);
  out.write(reinterpret_cast<const char*>(&fmtSize), 4);
  out.write(reinterpret_cast<const char*>(&formatIeee), 2);
  out.write(reinterpret_cast<const char*>(&channels), 2);
  out.write(reinterpret_cast<const char*>(&rate), 4);
  out.write(reinterpret_cast<const char*>(&byteRate), 4);
  out.write(reinterpret_cast<const char*>(&align), 2);
  out.write(reinterpret_cast<const char*>(&bits), 2);
  out.write("data", 4);
  out.write(reinterpret_cast<const char*>(&dataBytes), 4);
  out.write(reinterpret_cast<const char*>(samples), static_cast<std::streamsize>(dataBytes));
  REQUIRE_MESSAGE(out.good(), path.string());
}

inline std::vector<double> ReadFloat64Wav(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  const std::string missing = "missing Windows null render " + path.string();
  REQUIRE_MESSAGE(in.good(), missing);
  char riff[4] = {};
  char wave[4] = {};
  char fmt[4] = {};
  char data[4] = {};
  std::uint32_t riffSize = 0;
  std::uint32_t fmtSize = 0;
  std::uint16_t format = 0;
  std::uint16_t channels = 0;
  std::uint32_t rate = 0;
  std::uint32_t byteRate = 0;
  std::uint16_t align = 0;
  std::uint16_t bits = 0;
  std::uint32_t dataBytes = 0;
  in.read(riff, 4);
  in.read(reinterpret_cast<char*>(&riffSize), 4);
  in.read(wave, 4);
  in.read(fmt, 4);
  in.read(reinterpret_cast<char*>(&fmtSize), 4);
  in.read(reinterpret_cast<char*>(&format), 2);
  in.read(reinterpret_cast<char*>(&channels), 2);
  in.read(reinterpret_cast<char*>(&rate), 4);
  in.read(reinterpret_cast<char*>(&byteRate), 4);
  in.read(reinterpret_cast<char*>(&align), 2);
  in.read(reinterpret_cast<char*>(&bits), 2);
  in.read(data, 4);
  in.read(reinterpret_cast<char*>(&dataBytes), 4);
  REQUIRE(std::memcmp(riff, "RIFF", 4) == 0);
  REQUIRE(std::memcmp(wave, "WAVE", 4) == 0);
  REQUIRE(std::memcmp(fmt, "fmt ", 4) == 0);
  REQUIRE(std::memcmp(data, "data", 4) == 0);
  REQUIRE(format == 3);
  REQUIRE(channels == 1);
  REQUIRE(bits == 64);
  REQUIRE(fmtSize == 16);
  REQUIRE(dataBytes % sizeof(double) == 0);
  (void)riffSize;
  (void)rate;
  (void)byteRate;
  (void)align;
  std::vector<double> samples(dataBytes / sizeof(double));
  in.read(reinterpret_cast<char*>(samples.data()), static_cast<std::streamsize>(dataBytes));
  REQUIRE(in.good());
  return samples;
}

inline double PeakResidualDb(const double* live, const double* reference, std::size_t count)
{
  double peak = 0.0;
  for (std::size_t i = 0; i < count; ++i)
  {
    const double d = std::abs(live[i] - reference[i]);
    if (d > peak)
      peak = d;
  }
  if (peak == 0.0)
    return -std::numeric_limits<double>::infinity();
  return 20.0 * std::log10(peak);
}

template <typename Sample>
void CheckWindowsNull(const char* name, const std::vector<Sample>& samples)
{
  static_assert(sizeof(Sample) == sizeof(double), "Windows null renders are raw doubles");
  const char* writeEnv = std::getenv("VOLUM_WRITE_PLATFORM_NULL");
  if (!WindowsNullActive() && writeEnv == nullptr)
    return;

  namespace fs = std::filesystem;
  const fs::path path = WindowsNullPath(name);
  const bool write = writeEnv != nullptr && std::strcmp(writeEnv, "1") == 0;
#if defined(_WIN32) && defined(NDEBUG)
  if (write)
  {
    fs::create_directories(path.parent_path());
    WriteFloat64Wav(path, reinterpret_cast<const double*>(samples.data()), samples.size());
    return;
  }
#else
  (void)write;
#endif

  if (!WindowsNullActive())
    return;

  const std::vector<double> reference = ReadFloat64Wav(path);
  REQUIRE(reference.size() == samples.size());
  const double db = PeakResidualDb(reinterpret_cast<const double*>(samples.data()), reference.data(), samples.size());
  MESSAGE(std::string(name) << " peak residual dBFS=" << db);
#if defined(_WIN32)
  CHECK(db == -std::numeric_limits<double>::infinity());
#else
  CHECK(db <= kWindowsNullPeakDb);
#endif
}
} // namespace volum::test
