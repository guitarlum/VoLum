#pragma once

namespace volum
{
inline constexpr int kAmpeteRigCount = 16;

// File names inside rigs/Ampete One/
inline constexpr const char* kAmpeteFiles[kAmpeteRigCount] = {"AMP-Ampt-1.nam", "AMP-Ampt-2.nam", "AMP-Ampt-3.nam",
  "AMP-Ampt-4.nam", "G12-Ampt-1.nam", "G12-Ampt-2.nam", "G12-Ampt-3.nam", "G12-Ampt-4.nam", "G65-Ampt-1.nam",
  "G65-Ampt-2.nam", "G65-Ampt-3.nam", "G65-Ampt-4.nam", "V30-Ampt-1.nam", "V30-Ampt-2.nam", "V30-Ampt-3.nam",
  "V30-Ampt-4.nam"};

inline constexpr const char* kAmpeteLabels[kAmpeteRigCount] = {"AMP 1", "AMP 2", "AMP 3", "AMP 4", "G12 1", "G12 2",
  "G12 3", "G12 4", "G65 1", "G65 2", "G65 3", "G65 4", "V30 1", "V30 2", "V30 3", "V30 4"};
} // namespace volum
