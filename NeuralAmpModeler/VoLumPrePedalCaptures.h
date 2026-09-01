#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "VoLumPaths.h"

namespace volum
{

inline constexpr const char* kPrePedalsFolderName = "PrePedals";
inline constexpr int kPreCaptureEmptyIndex = 0;
inline constexpr int kPreCaptureMaxParamIndex = 127;

enum class PrePedalCaptureGroup
{
  None,
  Klon,
  TsBoost,
  Distortion,
  Fuzz,
};

struct PrePedalCapture
{
  std::string filename;
  std::string label;
  std::string shortLabel;
  PrePedalCaptureGroup group = PrePedalCaptureGroup::None;
  int sortRank = 0;
};

struct PrePedalCaptureMetadata
{
  const char* filename;
  const char* label;
  const char* shortLabel;
  PrePedalCaptureGroup group;
  int sortRank;
};

inline constexpr PrePedalCaptureMetadata kPrePedalCaptureMetadata[] = {
  {"FX-Minotaur-Klon-1.nam", "Klon", "Klon", PrePedalCaptureGroup::Klon, 10},
  {"FX-PettyJohn-Myth-1.nam", "PettyJohn Myth", "Myth", PrePedalCaptureGroup::Klon, 20},
  {"FX-OriginEffects-Halcyon-1.nam", "Halcyon TS", "TS", PrePedalCaptureGroup::TsBoost, 30},
  {"FX-OriginEffects-Halcyon-2.nam", "Halcyon TS +Gain", "TS+", PrePedalCaptureGroup::TsBoost, 40},
  {"FX-PettyJohn-Mash-1.nam", "PettyJohn Mash", "Mash", PrePedalCaptureGroup::TsBoost, 50},
  {"FX-OriginEffects-Revival-1.nam", "Revival Drive", "Revi", PrePedalCaptureGroup::Distortion, 60},
  {"FX-Beetronics-Fatbee-1.nam", "Fatbee", "FatB", PrePedalCaptureGroup::Fuzz, 70},
  {"FX-PettyJohn-Nuke-1.nam", "PettyJohn Nuke", "Nuke", PrePedalCaptureGroup::Fuzz, 80},
  {"FX-JHS-Bender-1.nam", "JHS Bender", "Bndr", PrePedalCaptureGroup::Fuzz, 90},
};

inline const PrePedalCaptureMetadata* GetPrePedalCaptureMetadata(const std::string& filename)
{
  for (const auto& metadata : kPrePedalCaptureMetadata)
    if (filename == metadata.filename)
      return &metadata;
  return nullptr;
}

inline const char* PrePedalCaptureGroupLabel(PrePedalCaptureGroup group)
{
  switch (group)
  {
    case PrePedalCaptureGroup::Klon: return "Klon";
    case PrePedalCaptureGroup::TsBoost: return "TS / Boost";
    case PrePedalCaptureGroup::Distortion: return "Distortion";
    case PrePedalCaptureGroup::Fuzz: return "Fuzz";
    default: return "Other";
  }
}

inline std::string PrePedalCaptureFallbackLabelFromFilename(const std::filesystem::path& path)
{
  return volum::PathStemUtf8(path);
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

    const std::string filename = volum::PathLeafUtf8(entry.path());
    if (const auto* metadata = GetPrePedalCaptureMetadata(filename))
      captures.push_back({filename, metadata->label, metadata->shortLabel, metadata->group, metadata->sortRank});
    else
      captures.push_back({filename, PrePedalCaptureFallbackLabelFromFilename(entry.path()),
                          PrePedalCaptureFallbackLabelFromFilename(entry.path()), PrePedalCaptureGroup::None, 10000});
  }

  std::sort(captures.begin(), captures.end(), [](const PrePedalCapture& a, const PrePedalCapture& b) {
    if (a.sortRank != b.sortRank)
      return a.sortRank < b.sortRank;
    return a.filename < b.filename;
  });
  return captures;
}

inline int ClampPreCaptureIndex(int captureIdx, int captureCount)
{
  const int maxIdx = std::clamp(captureCount, 0, kPreCaptureMaxParamIndex);
  return std::clamp(captureIdx, kPreCaptureEmptyIndex, maxIdx);
}

inline bool ShouldLoadPrePedalCapture(bool active, int captureIdx)
{
  return active && captureIdx > kPreCaptureEmptyIndex;
}

} // namespace volum
