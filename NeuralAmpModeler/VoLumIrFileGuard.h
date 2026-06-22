#pragma once

// VoLum custom-IR file-size guard.
//
// The convolver (dsp::ImpulseResponse) only ever uses the first ~8192 samples
// (~170 ms at 48 kHz), so a multi-megabyte WAV - e.g. a whole song dropped in by
// mistake - just wastes load-time RAM/CPU decoding and resampling data that never
// reaches the output. We reject obviously-too-large files at the import / select
// entry points with a clear message instead of silently chewing through them.
//
// The byte threshold is intentionally generous: a 10-second stereo 24-bit 96 kHz
// capture is ~5.8 MB, so 64 MB only ever trips on pathological / wrong-file picks.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

namespace volum
{

inline constexpr std::uintmax_t kMaxIrFileBytes = 64ull * 1024ull * 1024ull; // 64 MB

// Pure predicate (no filesystem) so the threshold is unit-testable.
inline bool IrFileBytesAcceptable(std::uintmax_t bytes)
{
  return bytes <= kMaxIrFileBytes;
}

inline std::string IrTooLargeMessage(std::uintmax_t bytes)
{
  const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
  const double capMb = static_cast<double>(kMaxIrFileBytes) / (1024.0 * 1024.0);
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "This impulse response is too large (%.0f MB, max %.0f MB).\n\nVoLum cabinet IRs are short "
                "speaker captures - only the first fraction of a second is used. Please choose a smaller WAV file.",
                mb, capMb);
  return std::string(buf);
}

// Returns true when the file at `path` is a sane size to load as an IR. On a
// too-large file returns false and fills `outMessage` with a user-facing reason.
// If the size cannot be determined (e.g. missing file), returns true and lets the
// regular loader report the real error.
inline bool IrFileSizeAcceptable(const std::string& path, std::string& outMessage)
{
  std::error_code ec;
  const std::uintmax_t bytes = std::filesystem::file_size(std::filesystem::u8path(path), ec);
  if (ec)
    return true;
  if (IrFileBytesAcceptable(bytes))
    return true;
  outMessage = IrTooLargeMessage(bytes);
  return false;
}

} // namespace volum
