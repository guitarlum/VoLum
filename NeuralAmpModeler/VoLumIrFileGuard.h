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

#include "../AudioDSPTools/dsp/wav.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

// The convolver's WAV parser trusts the sizes a file declares: a header that
// claims 2 GB of audio makes it allocate (or throw std::length_error) before it
// ever notices the file is 60 bytes long. This walks the RIFF chunk list first,
// against the real file size, and refuses what the parser must never see. It
// returns true when the header is sane or cannot be judged here (the parser then
// reports the real error); false with `outMessage` for a header it can prove bad.
inline bool IrWavHeaderSane(const std::string& path, std::string& outMessage)
{
  std::error_code ec;
  const auto fsPath = std::filesystem::u8path(path);
  const std::uintmax_t fileBytes = std::filesystem::file_size(fsPath, ec);
  if (ec)
    return true;
  std::ifstream f(fsPath, std::ios::binary);
  if (!f.is_open())
    return true;

  unsigned char riff[12];
  f.read(reinterpret_cast<char*>(riff), 12);
  if (f.gcount() != 12 || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0)
    return true;

  auto le32 = [](const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
           | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
  };
  bool haveFmt = false;
  std::uint32_t bytesPerSample = 0;
  std::uintmax_t pos = 12;
  while (pos + 8 <= fileBytes)
  {
    unsigned char hdr[8];
    f.seekg(static_cast<std::streamoff>(pos));
    f.read(reinterpret_cast<char*>(hdr), 8);
    if (f.gcount() != 8)
      return true;
    const std::uint32_t size = le32(hdr + 4);
    if (std::memcmp(hdr, "fmt ", 4) == 0)
    {
      unsigned char fmt[16];
      if (size < 16)
        return true;
      f.read(reinterpret_cast<char*>(fmt), 16);
      if (f.gcount() != 16)
        return true;
      if (static_cast<std::int32_t>(le32(fmt + 4)) <= 0)
      {
        outMessage = "WAV file has an invalid sample rate.";
        return false;
      }
      bytesPerSample = (static_cast<std::uint32_t>(fmt[14]) | (static_cast<std::uint32_t>(fmt[15]) << 8)) / 8u;
      haveFmt = true;
    }
    else if (std::memcmp(hdr, "data", 4) == 0)
    {
      if (!haveFmt)
        return true;
      if (size > fileBytes - (pos + 8))
      {
        outMessage = "WAV file is truncated.";
        return false;
      }
      if (size == 0 || size < bytesPerSample)
      {
        outMessage = "WAV file contains no audio.";
        return false;
      }
      return true;
    }
    pos += 8u + static_cast<std::uintmax_t>(size) + (size & 1u);
  }
  return true;
}

using IrWavLoader = std::function<dsp::wav::LoadReturnCode(const char*, std::vector<float>&, double&)>;

// Import gate shared with the convolver: decode through the real WAV parser
// before the file is copied into VoLum's library. Malformed RIFF/WAVE structures
// use the established user-facing message; valid-but-unsupported WAV encodings
// retain the parser's more specific explanation. Nothing escapes: a parser that
// throws (allocation, length) is a rejected file, never a dead host.
inline bool IrFileValidForImport(const std::string& path, std::string& outMessage,
                                 const IrWavLoader& load = IrWavLoader(dsp::wav::Load))
{
  try
  {
    if (!IrFileSizeAcceptable(path, outMessage))
      return false;
    if (!IrWavHeaderSane(path, outMessage))
      return false;

    std::vector<float> audio;
    double sampleRate = 0.0;
    const auto rc = load(path.c_str(), audio, sampleRate);
    if (rc == dsp::wav::LoadReturnCode::SUCCESS)
    {
      if (audio.empty())
      {
        outMessage = "WAV file contains no audio.";
        return false;
      }
      if (!(sampleRate > 0.0))
      {
        outMessage = "WAV file has an invalid sample rate.";
        return false;
      }
      return true;
    }

    switch (rc)
    {
      case dsp::wav::LoadReturnCode::ERROR_NOT_RIFF:
      case dsp::wav::LoadReturnCode::ERROR_NOT_WAVE:
      case dsp::wav::LoadReturnCode::ERROR_MISSING_FMT:
      case dsp::wav::LoadReturnCode::ERROR_INVALID_FILE:
      case dsp::wav::LoadReturnCode::ERROR_OTHER: outMessage = "File is not a WAV file."; break;
      default: outMessage = dsp::wav::GetMsgForLoadReturnCode(rc); break;
    }
    return false;
  }
  catch (...)
  {
    outMessage = "File is not a readable WAV file.";
    return false;
  }
}

// The one banner line after an IR import. "Too large" keeps the floor: other
// rejections are appended, never substituted, and every rejected file is named
// (a handful, then a count) so the user knows which picks to retry.
inline std::string IrImportRejectionSummary(const std::vector<std::string>& tooLarge,
                                            const std::vector<std::pair<std::string, std::string>>& invalid)
{
  auto namesOf = [](const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size() && i < 2; ++i)
      out += (i ? ", " : "") + ("\"" + v[i] + "\"");
    if (v.size() > 2)
      out += " and " + std::to_string(v.size() - 2) + " more";
    return out;
  };
  std::string large;
  if (!tooLarge.empty())
    large =
      tooLarge.size() == 1
        ? ("\"" + tooLarge.front() + "\" is too large for an IR - skipped.")
        : (std::to_string(tooLarge.size()) + " files were too large for IRs - skipped: " + namesOf(tooLarge) + ".");
  std::string bad;
  if (invalid.size() == 1 && tooLarge.empty())
    bad = "\"" + invalid.front().first + "\": " + invalid.front().second;
  else if (invalid.size() == 1)
    bad = "\"" + invalid.front().first + "\" is not a usable WAV.";
  else if (!invalid.empty())
  {
    std::vector<std::string> names;
    for (const auto& item : invalid)
      names.push_back(item.first);
    bad = std::to_string(invalid.size()) + " files are not usable WAVs - skipped: " + namesOf(names) + ".";
  }
  if (!large.empty() && !bad.empty())
    return large + " " + bad;
  return large.empty() ? bad : large;
}

} // namespace volum