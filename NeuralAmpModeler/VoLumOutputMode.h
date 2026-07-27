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

// Input-side counterpart of the calibrated output mode.
//
// Input calibration only means something when the model declares the level it was
// captured at (`input_level_dbu`). Given that, the offset lines the user's
// interface level up with the capture level so the model sees the same drive the
// original amp did. Without it there is nothing to line up against and the knob
// value passes through untouched - which is why calibration looks like it does
// nothing on models whose trainer did not measure an input level (including every
// factory rig VoLum ships, and any capture whose `input_level_dbu` is null).
inline double ComputeInputGainDb(double knobDb, bool calibrateEnabled, bool modelHasInputLevel,
                                 double modelInputLevelDbu, double interfaceCalibrationDbu)
{
  if (!calibrateEnabled || !modelHasInputLevel)
    return knobDb;
  return knobDb + (interfaceCalibrationDbu - modelInputLevelDbu);
}

// Why the input-calibration card is live or dead, in the user's words.
//
// Graying the controls out was technically honest and practically useless: the
// reported symptom was "input calibration seems to do nothing", which is exactly
// what an unexplained disabled control looks like. The card now says which side of
// the pairing is missing.
inline const char* InputCalibrationHelpText(bool modelHasInputLevel)
{
  return modelHasInputLevel ? "Aligns your interface level to the capture"
                            : "This model has no capture level";
}

inline const char* InputCalibrationTooltip(bool modelHasInputLevel)
{
  return modelHasInputLevel
           ? "Line the model up with your interface: enter the analog level (dBu RMS) that reaches 0 dBFS in "
             "your host, and VoLum offsets the input so the model sees the drive it was captured with."
           : "Unavailable: this capture does not record the analog level it was made at (input_level_dbu), so "
             "there is nothing to align your interface level against. VoLum's bundled rigs are in this "
             "category; captures trained with an input level set will enable it.";
}

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
