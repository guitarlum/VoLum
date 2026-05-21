#include "VoLumChunkVersion.h"
#include "VoLumChunkLayout.h"
#include "VoLumJsonMigration.h"

// Unserialization
//
// Tail-included from NeuralAmpModeler.cpp; this is NOT a standalone translation
// unit. iPlug2 project files only list NeuralAmpModeler.cpp; by tail-including
// this file plus VoLumLoader.inc.cpp / VoLumSettings.inc.cpp we keep the per-
// platform project files unchanged while splitting the 4.5k-line plugin .cpp
// into navigable chunks. Adding this file to vcxproj / pbxproj as a real TU is
// tracked as a 1.1 hygiene follow-up.
//
// This plugin is used in important places, so we need to be considerate when
// attempting to unserialize. If the project was last saved with a legacy
// version, then we need it to "update" to the current version is as
// reasonable a way as possible.
//
// In order to handle older versions, the pattern is:
// 1. Implement unserialization for every version into a version-specific
//    struct (Let's use our friend nlohmann::json. Why not?)
// 2. Implement an "update" from each struct to the next one.
// 3. Implement assigning the data contained in the current struct to the
//    current plugin configuration.
//
// This way, a constant amount of effort is required every time the
// serialization changes instead of having to implement a current
// unserialization for each past version.

// Add new unserialization versions to the top, then add logic to the class method at the bottom.

// Boilerplate

void NeuralAmpModeler::_UnserializeApplyConfig(nlohmann::json& config)
{
  auto getParamByName = [&](std::string& name) {
    // Could use a map but eh
    for (int i = 0; i < kNumParams; i++)
    {
      iplug::IParam* param = GetParam(i);
      if (strcmp(param->GetName(), name.c_str()) == 0)
      {
        return param;
      }
    }
    // else
    return (iplug::IParam*)nullptr;
  };
  TRACE
  ENTER_PARAMS_MUTEX
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    std::string name = it.key();
    iplug::IParam* pParam = getParamByName(name);
    if (pParam != nullptr)
    {
      if (!it->is_number())
      {
        iplug::Trace(TRACELOC, "%s SKIPPED-NON-NUMERIC", name.c_str());
        continue;
      }
      pParam->Set(*it);
      iplug::Trace(TRACELOC, "%s %f", pParam->GetName(), pParam->Value());
    }
    else
    {
      iplug::Trace(TRACELOC, "%s NOT-FOUND", name.c_str());
    }
  }
  OnParamReset(iplug::EParamSource::kPresetRecall);
  LEAVE_PARAMS_MUTEX

  mNAMPath.Set(config.value("NAMPath", "").c_str());
  mIRPath.Set(config.value("IRPath", "").c_str());

  if (mNAMPath.GetLength())
  {
    _StageModel(mNAMPath);
  }
  if (mIRPath.GetLength())
  {
    _StageIR(mIRPath);
  }
}

// Unserialize NAM Path, IR path, then named keys
int _UnserializePathsAndExpectedKeys(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config,
                                     std::vector<std::string>& paramNames)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());

  for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
  {
    double v = 0.0;
    pos = chunk.Get(&v, pos);
    config[*it] = v;
  }
  return pos;
}

void _RenameKeys(nlohmann::json& j, std::unordered_map<std::string, std::string> newNames)
{
  // Assumes no aliasing!
  for (auto it = newNames.begin(); it != newNames.end(); ++it)
  {
    volum::RenameJsonKeyIfPresent(j, it->first.c_str(), it->second.c_str());
  }
}

// v0.7.12

void _UpdateConfigFrom_0_7_12(nlohmann::json& config)
{
  // Fill me in once something changes!
}

int _GetConfigFrom_0_7_12(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_12(config);
  return pos;
}

// v0.7.14 (adds VoLum "AmpeteRig"; 13 param doubles after paths — keep 0.7.12..0.7.13 on 12 doubles)

void _UpdateConfigFrom_0_7_14(nlohmann::json& config)
{
  std::unordered_map<std::string, std::string> renames{{"AmpeteRig", "RigFile"}};
  _RenameKeys(config, renames);
  _UpdateConfigFrom_0_7_12(config);
}

int _GetConfigFrom_0_7_14(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "AmpeteRig"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_7_14(config);
  return pos;
}

// v0.7.15 (same params as 0.7.14, per-amp settings appended after SerializeParams)

void _UpdateConfigFrom_0_7_15(nlohmann::json& config)
{
  _UpdateConfigFrom_0_7_14(config);
}

int _GetConfigFrom_0_7_15(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = _GetConfigFrom_0_7_14(chunk, startPos, config);
  return pos;
}

// v0.8.1 (PRE pedalboard params added)

void _UpdateConfigFrom_0_8_1(nlohmann::json& config)
{
  _UpdateConfigFrom_0_7_15(config);
}

int _GetConfigFrom_0_8_1(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "IRToggle",
    "DelayActive", "DelayTime", "DelayFeedback", "DelayMix", "DelayMode",
    "ReverbActive", "ReverbMix", "ReverbDecay", "ReverbTone", "ReverbPreDelay", "ReverbShimmer", "ReverbMode",
    "BoostActive", "BoostDrive", "BoostTone", "BoostLevel",
    "PreCompActive", "PreCompAmount", "PreCompRatio", "PreCompAttack", "PreCompRelease", "PreCompMix", "PreCompLevel",
    "PreNam1Active", "PreNam1Capture", "PreNam1Gain", "PreNam1Bass", "PreNam1Mid", "PreNam1MidFreq", "PreNam1Treble", "PreNam1Level",
    "PreNam2Active", "PreNam2Capture", "PreNam2Gain", "PreNam2Bass", "PreNam2Mid", "PreNam2MidFreq", "PreNam2Treble", "PreNam2Level",
    "CalibrateInput", "InputCalibrationLevel", "OutputMode", "RigFile"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_8_1(config);
  return pos;
}

// v0.8.3 (Dual Amp stereo params added)

void _UpdateConfigFrom_0_8_3(nlohmann::json& config)
{
  _UpdateConfigFrom_0_8_1(config);
  // v0.8.3 itself didn't add params past what the loader names; the v0.9.0 jump is handled
  // by _MigrateDelayReverbToV0_9_0 from _UnserializeStateWithKnownVersion for ALL pre-0.9.0
  // chunks (so 0.5.0-0.8.3 all migrate, not just 0.8.3).
}

// v0.9.0 chunk migration: implemented in VoLumJsonMigration.h as
// volum::MigrateDelayReverbToV0_9_0 so it's reachable from tests without exposing the
// file-static _GetConfigFrom_X chain.

int _GetConfigFrom_0_8_3(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "IRToggle",
    "DelayActive", "DelayTime", "DelayFeedback", "DelayMix", "DelayMode",
    "ReverbActive", "ReverbMix", "ReverbDecay", "ReverbTone", "ReverbPreDelay", "ReverbShimmer", "ReverbMode",
    "BoostActive", "BoostDrive", "BoostTone", "BoostLevel",
    "PreCompActive", "PreCompAmount", "PreCompRatio", "PreCompAttack", "PreCompRelease", "PreCompMix", "PreCompLevel",
    "PreNam1Active", "PreNam1Capture", "PreNam1Gain", "PreNam1Bass", "PreNam1Mid", "PreNam1MidFreq", "PreNam1Treble", "PreNam1Level",
    "PreNam2Active", "PreNam2Capture", "PreNam2Gain", "PreNam2Bass", "PreNam2Mid", "PreNam2MidFreq", "PreNam2Treble", "PreNam2Level",
    "CalibrateInput", "InputCalibrationLevel", "OutputMode", "RigFile",
    "DualAmpActive", "DualAmpRoute", "MainAmpPan", "SupportAmp", "SupportSpeaker", "SupportChannel",
    "SupportInput", "SupportThreshold", "SupportBass", "SupportMiddle", "SupportTreble", "SupportOutput",
    "SupportNoiseGateActive", "SupportToneStack", "SupportAmpPan"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_8_3(config);
  return pos;
}

// v0.9.0 (effect-staging: DelayTone/Age/PingPong + ReverbSubMode;
// new delay mode order Digital/Analog/Reverse).
// v0.9.1 reuses this byte layout and remaps ReverbSubMode meaning after load.

void _UpdateConfigFrom_0_9_0(nlohmann::json& /*config*/)
{
  // No-op: 0.9.0+ share the current param byte layout, so chunks saved here need no field changes.
}

int _GetConfigFrom_0_9_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  // Param order matches the EParams enum at v0.9.0+.
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "IRToggle",
    "DelayActive", "DelayTime", "DelayFeedback", "DelayMix", "DelayMode",
    "DelayTone", "DelayAge", "DelayPingPong",
    "ReverbActive", "ReverbMix", "ReverbDecay", "ReverbTone", "ReverbPreDelay", "ReverbShimmer", "ReverbMode",
    "ReverbSubMode",
    "BoostActive", "BoostDrive", "BoostTone", "BoostLevel",
    "PreCompActive", "PreCompAmount", "PreCompRatio", "PreCompAttack", "PreCompRelease", "PreCompMix", "PreCompLevel",
    "PreNam1Active", "PreNam1Capture", "PreNam1Gain", "PreNam1Bass", "PreNam1Mid", "PreNam1MidFreq", "PreNam1Treble", "PreNam1Level",
    "PreNam2Active", "PreNam2Capture", "PreNam2Gain", "PreNam2Bass", "PreNam2Mid", "PreNam2MidFreq", "PreNam2Treble", "PreNam2Level",
    "CalibrateInput", "InputCalibrationLevel", "OutputMode", "RigFile",
    "DualAmpActive", "DualAmpRoute", "MainAmpPan", "SupportAmp", "SupportSpeaker", "SupportChannel",
    "SupportInput", "SupportThreshold", "SupportBass", "SupportMiddle", "SupportTreble", "SupportOutput",
    "SupportNoiseGateActive", "SupportToneStack", "SupportAmpPan"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_9_0(config);
  return pos;
}

// VoLum 0.5.0 (Reverb, Delay, Boost added)

void _UpdateConfigFrom_0_5_0(nlohmann::json& config)
{
  _UpdateConfigFrom_0_7_15(config);
  config["ReverbPreDelay"] = 20.0;
  config["ReverbShimmer"] = 0.5;
}

int _GetConfigFrom_0_5_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input",
    "Threshold",
    "Bass",
    "Middle",
    "Treble",
    "Output",
    "NoiseGateActive",
    "ToneStack",
    "IRToggle",
    "DelayActive",
    "DelayTime",
    "DelayFeedback",
    "DelayMix",
    "DelayMode",
    "ReverbActive",
    "ReverbMix",
    "ReverbDecay",
    "ReverbTone",
    "ReverbMode",
    "BoostActive",
    "BoostDrive",
    "BoostTone",
    "BoostLevel",
    "CalibrateInput",
    "InputCalibrationLevel",
    "OutputMode",
    "AmpeteRig"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_5_0(config);
  return pos;
}

// VoLum 0.6.0 (Reverb pre-delay, shimmer, Oktaverb mode)

void _UpdateConfigFrom_0_6_0(nlohmann::json& config)
{
  _UpdateConfigFrom_0_7_15(config);
}

int _GetConfigFrom_0_6_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input",
    "Threshold",
    "Bass",
    "Middle",
    "Treble",
    "Output",
    "NoiseGateActive",
    "ToneStack",
    "IRToggle",
    "DelayActive",
    "DelayTime",
    "DelayFeedback",
    "DelayMix",
    "DelayMode",
    "ReverbActive",
    "ReverbMix",
    "ReverbDecay",
    "ReverbTone",
    "ReverbPreDelay",
    "ReverbShimmer",
    "ReverbMode",
    "BoostActive",
    "BoostDrive",
    "BoostTone",
    "BoostLevel",
    "CalibrateInput",
    "InputCalibrationLevel",
    "OutputMode",
    "AmpeteRig"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_6_0(config);
  return pos;
}

// VoLum 0.7.0 (renames "AmpeteRig" -> "RigFile"; same layout as 0.6.0 otherwise)

void _UpdateConfigFrom_0_7_0(nlohmann::json& config)
{
  _UpdateConfigFrom_0_7_12(config);
}

int _GetConfigFrom_0_7_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input",
    "Threshold",
    "Bass",
    "Middle",
    "Treble",
    "Output",
    "NoiseGateActive",
    "ToneStack",
    "IRToggle",
    "DelayActive",
    "DelayTime",
    "DelayFeedback",
    "DelayMix",
    "DelayMode",
    "ReverbActive",
    "ReverbMix",
    "ReverbDecay",
    "ReverbTone",
    "ReverbPreDelay",
    "ReverbShimmer",
    "ReverbMode",
    "BoostActive",
    "BoostDrive",
    "BoostTone",
    "BoostLevel",
    "CalibrateInput",
    "InputCalibrationLevel",
    "OutputMode",
    "RigFile"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_7_0(config);
  return pos;
}

// 0.7.10

void _UpdateConfigFrom_0_7_10(nlohmann::json& config)
{
  // Note: "OutNorm" is Bool-like in v0.7.10, but "OutputMode" is enum.
  // This works because 0 is "Raw" (cf OutNorm false) and 1 is "Calibrated" (cf OutNorm true).
  std::unordered_map<std::string, std::string> newNames{{"OutNorm", "OutputMode"}};
  _RenameKeys(config, newNames);
  // There are new parameters. If they're not included, then 0.7.12 is ok, but future ones might not be.
  config[kCalibrateInputParamName] = (double)kDefaultCalibrateInput;
  config[kInputCalibrationLevelParamName] = kDefaultInputCalibrationLevel;
  _UpdateConfigFrom_0_7_12(config);
}

int _GetConfigFrom_0_7_10(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};
  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_10(config);
  return pos;
}

// Earlier than 0.7.10 (Assumed to be 0.7.3-0.7.9)

void _UpdateConfigFrom_Earlier(nlohmann::json& config)
{
  std::unordered_map<std::string, std::string> newNames{{"Gate", "Threshold"}};
  _RenameKeys(config, newNames);
  _UpdateConfigFrom_0_7_10(config);
}

int _GetConfigFrom_Earlier(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Gate", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_Earlier(config);
  return pos;
}

//==============================================================================

int NeuralAmpModeler::_UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  // We already got through the header before calling this.
  int pos = startPos;

  // Get the version
  WDL_String wVersion;
  pos = chunk.GetStr(wVersion, pos);
  std::string versionStr(wVersion.Get());
  volum::ChunkVersion version(versionStr);
  // Act accordingly
  nlohmann::json config;

  if (version >= volum::ChunkVersion(0, 9, 0))
  {
    pos = _GetConfigFrom_0_9_0(chunk, pos, config);
  }
  else if (version >= volum::ChunkVersion(0, 8, 3))
  {
    pos = _GetConfigFrom_0_8_3(chunk, pos, config);
  }
  else if (version >= volum::ChunkVersion(0, 8, 1))
  {
    pos = _GetConfigFrom_0_8_1(chunk, pos, config);
  }
  else if (volum::ChunkUses0700SerializedConfig(version))
  {
    pos = _GetConfigFrom_0_7_0(chunk, pos, config);
  }
  else if (volum::ChunkUses0600SerializedConfig(version))
  {
    pos = _GetConfigFrom_0_6_0(chunk, pos, config);
  }
  else if (volum::ChunkUses0500SerializedConfig(version))
  {
    pos = _GetConfigFrom_0_5_0(chunk, pos, config);
  }
  else if (volum::ChunkUses0715SerializedConfig(version))
  {
    pos = _GetConfigFrom_0_7_15(chunk, pos, config);
  }
  else if (version >= volum::ChunkVersion(0, 7, 14))
  {
    pos = _GetConfigFrom_0_7_14(chunk, pos, config);
  }
  else if (version >= volum::ChunkVersion(0, 7, 12))
  {
    pos = _GetConfigFrom_0_7_12(chunk, pos, config);
  }
  else if (version >= volum::ChunkVersion(0, 7, 10))
  {
    pos = _GetConfigFrom_0_7_10(chunk, pos, config);
  }
  else if (version >= volum::ChunkVersion(0, 7, 9))
  {
    pos = _GetConfigFrom_Earlier(chunk, pos, config);
  }
  else
  {
    // You shouldn't be here...
    assert(false);
  }
  // v0.9.0 migration: applied for ALL pre-0.9.0 chunks regardless of which
  // _GetConfigFrom_X_X_X path produced the dict above. Each pre-0.9.0 loader's per-version
  // _UpdateConfigFrom only chains backwards (predecessor), so this single call is what
  // brings the loaded config up to current schema (delay mode reorder + new EParams).
  if (!(version >= volum::ChunkVersion(0, 9, 0)))
    volum::MigrateDelayReverbToV0_9_0(config);
  if (volum::ShouldRemapOktaverbSubModeForChunkVersion(version))
    volum::MigrateOktaverbSubModeToV0_9_1(config);
  if (volum::ShouldRemapReverbMixForChunkVersion(version))
    volum::MigrateReverbMixToEqualPowerV0_9_3(config);
  _UnserializeApplyConfig(config);

#if VOLUM_AMPETE_PRODUCT
  // v0.7.15+ and VoLum 0.1.x: read per-amp settings after the params
  if (volum::ChunkUses0700SerializedConfig(version) || volum::ChunkUses0600SerializedConfig(version) ||
      volum::ChunkUses0500SerializedConfig(version) || volum::ChunkUses0715SerializedConfig(version))
  {
    // SerializeParams wrote kNumParams doubles; now read the per-amp block
    volum::VoLumChunkSelection selection;
    pos = volum::GetVoLumChunkSelection(chunk, pos, selection);
    mVolumAmpIdx = selection.ampIdx;
    mVolumSpeakerIdx = selection.speakerIdx;
    mVolumChannelIdx = selection.channelIdx;

    const int remainingPerAmpBytes = chunk.Size() - pos;
    const bool hasPreAmpSettings = volum::ChunkHasExtendedPerAmpSettings(remainingPerAmpBytes, volum::kAmpCount);
    const bool hasDualAmpSettings = volum::ChunkHasDualAmpPerAmpSettings(remainingPerAmpBytes, volum::kAmpCount);
    const bool hasSupportPolaritySettings =
      volum::ChunkHasSupportPolarityPerAmpSettings(remainingPerAmpBytes, volum::kAmpCount);
    const bool hasPostPerAmpSettings = volum::ChunkHasPostPerAmpSettings(remainingPerAmpBytes, volum::kAmpCount);
    const bool hasPostSnapshotPerAmpSettings =
      volum::ChunkHasPostSnapshotPerAmpSettings(remainingPerAmpBytes, volum::kAmpCount);
    const bool hasPrePostLockFlags =
      volum::ChunkHasPrePostLockFlags(remainingPerAmpBytes, volum::kAmpCount);

    for (int i = 0; i < volum::kAmpCount; i++)
    {
      auto& s = mVolumAmpSettings[i];
      pos = volum::GetLegacyPerAmpSettings(chunk, pos, s);
      if (hasPreAmpSettings)
      {
        pos = volum::GetExtendedPerAmpSettings(chunk, pos, s, version >= volum::ChunkVersion(0, 8, 1));
        if (volum::ShouldResetPreCaptureSlotsForChunkVersion(version))
          volum::ResetPreCaptureSlots(s);
      }
      if (hasDualAmpSettings)
        pos = volum::GetDualAmpPerAmpSettings(chunk, pos, s, hasSupportPolaritySettings);
      if (hasPostPerAmpSettings)
        pos = volum::GetPostPerAmpSettings(chunk, pos, s, hasPostSnapshotPerAmpSettings);
      // hasPostPerAmpSettings == false: legacy chunk pre-dating per-amp POST. Per
      // user policy "we don't have to migrate, we can reset", postValid stays at the
      // struct default (false), so _VolumRestoreFromSettings initializes a meaningful
      // factory POST scene instead of inheriting the previously selected amp.
    }

    if (hasPrePostLockFlags)
      pos = volum::GetPrePostLockFlags(chunk, pos, mVolumPreLocked, mVolumPostLocked);
    else
    {
      mVolumPreLocked = false;
      mVolumPostLocked = false;
    }

    mVolumInitComplete = false;
    _VolumRestoreFromSettings(mVolumAmpIdx);
    _VolumRefreshChannels();
    mVolumNeedsLoad.store(true);
    mVolumInitComplete = true;
  }
#endif

  return pos;
}

int NeuralAmpModeler::_UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  nlohmann::json config;
  int pos = _GetConfigFrom_Earlier(chunk, startPos, config);
  _UnserializeApplyConfig(config);
  return pos;
}
