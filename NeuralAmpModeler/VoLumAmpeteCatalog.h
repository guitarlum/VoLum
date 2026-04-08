#pragma once

namespace volum
{
struct AmpInfo
{
  const char* folderName;
  const char* displayName;
};

inline constexpr int kAmpCount = 14;
inline constexpr AmpInfo kAmps[kAmpCount] = {
  {"Ampete One", "Ampete One"},
  {"Bad Cat mini Cat", "Bad Cat mini Cat"},
  {"Brunetti XL 2", "Brunetti XL 2"},
  {"Fryette Deliverance 120", "Fryette Deliv. 120"},
  {"H&K TriAmp Mk2", "H&K TriAmp Mk2"},
  {"Lichtlaerm Prometheus", "Lichtlaerm Prom."},
  {"Marshall 2204 1982", "Marshall 2204"},
  {"Marshall JMP 2203 1976", "Marshall JMP 2203"},
  {"Marshall JVM 210H OD1", "Marshall JVM"},
  {"Orange OD120 1975", "Orange OD120"},
  {"Orange ORS100 1972", "Orange ORS100"},
  {"Sebago Texas Flood", "Sebago Texas Fl."},
  {"Soldano SLO100", "Soldano SLO100"},
  {"THC Sunset", "THC Sunset"},
};

inline constexpr const char* kSpeakerPrefixes[4] = {"AMP", "G12", "G65", "V30"};

// Legacy constants for backward compatibility with serialized state (v0.7.14)
inline constexpr int kAmpeteRigCount = 16;
inline constexpr const char* kAmpeteFiles[kAmpeteRigCount] = {"AMP-Ampt-1.nam", "AMP-Ampt-2.nam", "AMP-Ampt-3.nam",
  "AMP-Ampt-4.nam", "G12-Ampt-1.nam", "G12-Ampt-2.nam", "G12-Ampt-3.nam", "G12-Ampt-4.nam", "G65-Ampt-1.nam",
  "G65-Ampt-2.nam", "G65-Ampt-3.nam", "G65-Ampt-4.nam", "V30-Ampt-1.nam", "V30-Ampt-2.nam", "V30-Ampt-3.nam",
  "V30-Ampt-4.nam"};
inline constexpr const char* kAmpeteLabels[kAmpeteRigCount] = {"AMP 1", "AMP 2", "AMP 3", "AMP 4", "G12 1", "G12 2",
  "G12 3", "G12 4", "G65 1", "G65 2", "G65 3", "G65 4", "V30 1", "V30 2", "V30 3", "V30 4"};
} // namespace volum
