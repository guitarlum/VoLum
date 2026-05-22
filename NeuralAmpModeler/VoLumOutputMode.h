#pragma once

namespace volum
{

inline constexpr const char* kOutputModeLabels[] = {"Raw", "Normalized", "Calibrated"};
inline constexpr int kOutputModeCount = 3;
inline constexpr int kOutputModeDefault = 1; // Normalized
inline constexpr double kOutputModeTargetLoudnessDb = -18.0;

struct OutputModeModelInfo
{
  bool hasLoudness = false;
  double loudness = 0.0;
  bool hasOutputLevel = false;
  double outputLevel = 0.0;
};

inline double ComputeOutputModeGainDb(double knobDb, int outputMode, const OutputModeModelInfo& model,
                                      double inputCalibrationLevel)
{
  switch (outputMode)
  {
    case 1: // Normalized
      if (model.hasLoudness)
        return knobDb + (kOutputModeTargetLoudnessDb - model.loudness);
      break;
    case 2: // Calibrated
      if (model.hasOutputLevel)
        return knobDb + (model.outputLevel - inputCalibrationLevel);
      break;
    case 0: // Raw
    default: break;
  }
  return knobDb;
}

} // namespace volum
