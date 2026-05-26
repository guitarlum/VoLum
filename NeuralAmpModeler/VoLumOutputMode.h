#pragma once

namespace volum
{

inline constexpr const char* kOutputModeLabels[] = {"Raw", "Normalized", "Calibrated"};
inline constexpr int kOutputModeCount = 3;
inline constexpr int kOutputModeRaw = 0;
inline constexpr int kOutputModeNormalized = 1;
inline constexpr int kOutputModeCalibrated = 2;
inline constexpr int kOutputModeDefault = kOutputModeNormalized;
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
    case kOutputModeNormalized:
      if (model.hasLoudness)
        return knobDb + (kOutputModeTargetLoudnessDb - model.loudness);
      break;
    case kOutputModeCalibrated:
      if (model.hasOutputLevel)
        return knobDb + (model.outputLevel - inputCalibrationLevel);
      break;
    case kOutputModeRaw:
    default: break;
  }
  return knobDb;
}

} // namespace volum
