#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace volum
{

inline constexpr const char* kPrePedalsFolderName = "PrePedals";
inline constexpr int kPreCaptureEmptyIndex = 0;
inline constexpr int kPreCaptureMaxParamIndex = 127;

struct PrePedalCapture
{
  std::string filename;
  std::string label;
};

inline std::string PrePedalCaptureLabelFromFilename(const std::filesystem::path& path)
{
  std::string label = path.stem().string();
  const auto dash = label.find('-');
  if (dash != std::string::npos && dash > 0)
    label = label.substr(0, dash);
  return label;
}

inline std::vector<PrePedalCapture> DiscoverPrePedalCaptures(const std::filesystem::path& rigsRoot)
{
  namespace fs = std::filesystem;

  std::vector<PrePedalCapture> captures;
  const fs::path preDir = rigsRoot / kPrePedalsFolderName;
  std::error_code ec;
  if (!fs::is_directory(preDir, ec))
    return captures;

  for (const auto& entry : fs::directory_iterator(preDir, ec))
  {
    if (!entry.is_regular_file(ec) || entry.path().extension() != ".nam")
      continue;

    captures.push_back({entry.path().filename().string(), PrePedalCaptureLabelFromFilename(entry.path())});
  }

  std::sort(captures.begin(), captures.end(), [](const PrePedalCapture& a, const PrePedalCapture& b) {
    return a.filename < b.filename;
  });
  return captures;
}

inline int ClampPreCaptureIndex(int captureIdx, int captureCount)
{
  const int maxIdx = std::clamp(captureCount, 0, kPreCaptureMaxParamIndex);
  return std::clamp(captureIdx, kPreCaptureEmptyIndex, maxIdx);
}

} // namespace volum
