#pragma once

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/Delay.h"
#include "../AudioDSPTools/dsp/Reverb.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/wav.h"
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"

#include "Colors.h"
#include "ToneStack.h"
#include "VoLumDualAmpPlan.h"
#include "VoLumPreEffects.h"

#include "config.h"
#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// VoLum: catalog, settings, tuner/metronome (upstream-equivalent file fence)
#include "VoLumAmpeteCatalog.h"
#include "VoLumPrePedalCaptures.h"
#include "VoLumSettingsFileIO.h"
#include "VoLumTriptychState.h"
#include "VoLumUserSettingsIO.h"
#include "VoLumTunerDSP.h"
#include "VoLumMetronomeDSP.h"
#include "VoLumProcessingPlan.h"

const int kNumPresets = 1;
// The plugin is mono inside
constexpr size_t kNumChannelsInternal = 1;

class NAMSender : public iplug::IPeakAvgSender<>
{
public:
  NAMSender()
  : iplug::IPeakAvgSender<>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f)
  {
  }
};

enum EParams
{
  // These need to be the first ones because I use their indices to place
  // their rects in the GUI.
  kInputLevel = 0,
  kNoiseGateThreshold,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
  // The rest is fine though.
  kNoiseGateActive,
  kEQActive,
  kIRToggle,
  // Delay (POST)
  kDelayActive,
  kDelayTime,
  kDelayFeedback,
  kDelayMix,
  kDelayMode,
  // Effect staging (delay)
  kDelayTone,
  kDelayAge,
  kDelayPingPong,
  // Reverb (POST)
  kReverbActive,
  kReverbMix,
  kReverbDecay,
  kReverbTone,
  kReverbPreDelay,
  kReverbShimmer,
  kReverbMode,
  // Effect staging (reverb - Oktaverb sub-mode only)
  kReverbSubMode,
  // Reserved legacy boost params. Keep serialized indices stable; PRE captures replaced this planned DSP block.
  kBoostActive,
  kBoostDrive,
  kBoostTone,
  kBoostLevel,
  // PRE pedalboard
  kPreCompActive,
  kPreCompAmount,
  kPreCompRatio,
  kPreCompAttack,
  kPreCompRelease,
  kPreCompMix,
  kPreCompLevel,
  kPreNam1Active,
  kPreNam1Capture,
  kPreNam1Gain,
  kPreNam1Bass,
  kPreNam1Mid,
  kPreNam1MidFreq,
  kPreNam1Treble,
  kPreNam1Level,
  kPreNam2Active,
  kPreNam2Capture,
  kPreNam2Gain,
  kPreNam2Bass,
  kPreNam2Mid,
  kPreNam2MidFreq,
  kPreNam2Treble,
  kPreNam2Level,
  // Input calibration
  kCalibrateInput,
  kInputCalibrationLevel,
  kOutputMode,
  kVoLumAmpeteRig,
  // VoLum: dual-amp and support-lane parameters
  kDualAmpActive,
  kDualAmpRoute,
  kMainAmpPan,
  kSupportAmpIdx,
  kSupportSpeakerIdx,
  kSupportChannelIdx,
  kSupportInputLevel,
  kSupportNoiseGateThreshold,
  kSupportToneBass,
  kSupportToneMid,
  kSupportToneTreble,
  kSupportOutputLevel,
  kSupportNoiseGateActive,
  kSupportEQActive,
  kSupportAmpPan,
  kNumParams
};

const int numKnobs = 6;

enum ECtrlTags
{
  kCtrlTagModelFileBrowser = 0,
  kCtrlTagIRFileBrowser,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagOutputMeterR,
  kCtrlTagSettingsBox,
  kCtrlTagOutputMode,
  kCtrlTagCalibrateInput,
  kCtrlTagInputCalibrationLevel,
  kCtrlTagVoLumAmpList,
  kCtrlTagVoLumSpeakerRow,
  kCtrlTagVoLumHeroImage,
  kCtrlTagVoLumAmpName,
  kCtrlTagVoLumKeyboardHint,
  kCtrlTagVoLumFooter,
  kCtrlTagVoLumExactEntry,
  kCtrlTagVoLumChannelStep,
  kCtrlTagVoLumSupportChannelStep,
  kCtrlTagVoLumSupportAmpMenu,
  kCtrlTagVoLumDualAmpRoute,
  kCtrlTagVoLumTriptych,
  kCtrlTagVoLumBoostCard,
  kCtrlTagVoLumCompCard,
  kCtrlTagVoLumPreNam1Card,
  kCtrlTagVoLumPreNam2Card,
  kCtrlTagVoLumPreCaptureMenu,
  kCtrlTagVoLumPreChainConnector1,
  kCtrlTagVoLumPreChainConnector2,
  kCtrlTagVoLumDelayCard,
  kCtrlTagVoLumReverbCard,
  kCtrlTagVoLumChainConnector,
  kCtrlTagVoLumSubRowText,
  kCtrlTagVoLumNoiseGate,
  kCtrlTagVoLumEQ,
  kCtrlTagVoLumTuner,
  kCtrlTagVoLumMetronome,
  kCtrlTagVoLumMetronomeButton,
  kNumCtrlTags
};

enum EMsgTags
{
  // These tags are used from UI -> DSP
  kMsgTagClearModel = 0,
  kMsgTagClearIR,
  kMsgTagHighlightColor,
  // The following tags are from DSP -> UI
  kMsgTagLoadFailed,
  kMsgTagLoadedModel,
  kMsgTagLoadedIR,
  kNumMsgTags
};

// Get the sample rate of a NAM model.
// Sometimes, the model doesn't know its own sample rate; this wrapper guesses 48k based on the way that most
// people have used NAM in the past.
double GetNAMSampleRate(const std::unique_ptr<nam::DSP>& model)
{
  // Some models are from when we didn't have sample rate in the model.
  // For those, this wraps with the assumption that they're 48k models, which is probably true.
  const double assumedSampleRate = 48000.0;
  const double reportedEncapsulatedSampleRate = model->GetExpectedSampleRate();
  const double encapsulatedSampleRate =
    reportedEncapsulatedSampleRate <= 0.0 ? assumedSampleRate : reportedEncapsulatedSampleRate;
  return encapsulatedSampleRate;
};

class ResamplingNAM : public nam::DSP
{
public:
  // Resampling wrapper around the NAM models
  ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated, const double expected_sample_rate)
  : nam::DSP(1, 1, expected_sample_rate)
  , mEncapsulated(std::move(encapsulated))
  , mResampler(GetNAMSampleRate(mEncapsulated))
  {
    // Assign the encapsulated object's processing function to this object's member so that the resampler can use it:
    auto ProcessBlockFunc = [&](NAM_SAMPLE** input, NAM_SAMPLE** output, int numFrames) {
      mEncapsulated->process(input, output, numFrames);
    };
    mBlockProcessFunc = ProcessBlockFunc;

    // Get the other information from the encapsulated NAM so that we can tell the outside world about what we're
    // holding.
    if (mEncapsulated->HasLoudness())
    {
      SetLoudness(mEncapsulated->GetLoudness());
    }
    if (mEncapsulated->HasInputLevel())
    {
      SetInputLevel(mEncapsulated->GetInputLevel());
    }
    if (mEncapsulated->HasOutputLevel())
    {
      SetOutputLevel(mEncapsulated->GetOutputLevel());
    }

    // NOTE: prewarm samples doesn't mean anything--we can prewarm the encapsulated model as it likes and be good to
    // go.
    // _prewarm_samples = 0;

    // And be ready
    int maxBlockSize = 2048; // Conservative
    Reset(expected_sample_rate, maxBlockSize);
  };

  ~ResamplingNAM() = default;

  void prewarm() override { mEncapsulated->prewarm(); };

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    if (num_frames > mMaxExternalBlockSize)
      // We can afford to be careful
      throw std::runtime_error("More frames were provided than the max expected!");

    if (!NeedToResample())
    {
      mEncapsulated->process(input, output, num_frames);
    }
    else
    {
      mResampler.ProcessBlock(input, output, num_frames, mBlockProcessFunc);
    }
  };

  void process(NAM_SAMPLE* input, NAM_SAMPLE* output, const int num_frames)
  {
    NAM_SAMPLE* inputPtrs[1] = {input};
    NAM_SAMPLE* outputPtrs[1] = {output};
    process(inputPtrs, outputPtrs, num_frames);
  };

  int GetLatency() const { return NeedToResample() ? mResampler.GetLatency() : 0; };

  void Reset(const double sampleRate, const int maxBlockSize) override
  {
    mExpectedSampleRate = sampleRate;
    mMaxExternalBlockSize = maxBlockSize;
    mResampler.Reset(sampleRate, maxBlockSize);

    // Allocations in the encapsulated model (HACK)
    // Stolen some code from the resampler; it'd be nice to have these exposed as methods? :)
    const double mUpRatio = sampleRate / GetEncapsulatedSampleRate();
    const auto maxEncapsulatedBlockSize = static_cast<int>(std::ceil(static_cast<double>(maxBlockSize) / mUpRatio));
    mEncapsulated->ResetAndPrewarm(sampleRate, maxEncapsulatedBlockSize);
  };

  // So that we can let the world know if we're resampling (useful for debugging)
  double GetEncapsulatedSampleRate() const { return GetNAMSampleRate(mEncapsulated); };

private:
  bool NeedToResample() const { return GetExpectedSampleRate() != GetEncapsulatedSampleRate(); };
  // The encapsulated NAM
  std::unique_ptr<nam::DSP> mEncapsulated;

  // The resampling wrapper
  dsp::ResamplingContainer<NAM_SAMPLE, 1, 12> mResampler;

  // Used to check that we don't get too large a block to process.
  int mMaxExternalBlockSize = 0;

  // This function is defined to conform to the interface expected by the iPlug2 resampler.
  std::function<void(NAM_SAMPLE**, NAM_SAMPLE**, int)> mBlockProcessFunc;
};

class NeuralAmpModeler final : public iplug::Plugin
{
public:
  NeuralAmpModeler(const iplug::InstanceInfo& info);
  ~NeuralAmpModeler();

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;

  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  void OnUIClose() override;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }

  void OnParamChange(int paramIdx) override;
  void OnParamChangeUI(int paramIdx, iplug::EParamSource source) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  // Allocates mInputPointers and mOutputPointers
  void _AllocateIOPointers(const size_t nChans);
  // Moves DSP modules from staging area to the main area.
  // Also deletes DSP modules that are flagged for removal.
  // Exists so that we don't try to use a DSP module that's only
  // partially-instantiated.
  void _ApplyDSPStaging();
  // Deallocates mInputPointers and mOutputPointers
  void _DeallocateIOPointers();
  // Fallback used when no main NAM model is loaded.
  void _FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels, const size_t numFrames);
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  void _InitToneStack();
  // Loads a NAM model and stores it to mStagedNAM
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile);
public:
  void _VolumRefreshChannels();
  void _VolumRefreshSupportChannels();
  void _VolumApplyDualAmpFocus();
  void _VolumShowSupportAmpMenu(const iplug::igraphics::IRECT& anchorRect);
  void _VolumHideSupportAmpMenu();
  void _VolumSetSupportAmp(int ampIdx);
  void _VolumSaveCurrentToSettings();
  void _VolumSavePreToSlot(volum::VoLumAmpSettings& s);
  void _VolumSavePostToSlot(volum::VoLumAmpSettings& s);
  void _VolumRestorePreFromSlot(const volum::VoLumAmpSettings& s);
  void _VolumRestorePostFromSlot(volum::VoLumAmpSettings& s);
  void _VolumSetPreLocked(bool locked);
  void _VolumSetPostLocked(bool locked);
  bool _VolumIsPreLocked() const { return mVolumPreLocked; }
  bool _VolumIsPostLocked() const { return mVolumPostLocked; }
  bool _VolumIsPreDirty() const;
  bool _VolumIsPostDirty() const;
  void _VolumStorePreToCurrentAmp();
  void _VolumStorePostToCurrentAmp();
  void _VolumRefreshPrePostLockChrome(int paramIdx);
  void _VolumRestoreFromSettings(int ampIdx);
  void _VolumSaveSettingsToFile();
  void _VolumLoadSettingsFromFile();
  void _VolumSaveEffectSettings();
  void _VolumRestoreEffectSettings();
  void _VolumSaveDelayModeSnapshot(int mode);
  void _VolumRestoreDelayModeSnapshot(int mode);
  void _VolumSaveReverbModeSnapshot(int mode);
  void _VolumRestoreReverbModeSnapshot(int mode);
  void _VolumSaveOktaverbSubModeSnapshot(int subMode);
  void _VolumRestoreOktaverbSubModeSnapshot(int subMode);
  void _SelectVoLumKnob(int paramIdx);
  bool _SelectAdjacentVoLumKnob(int currentParamIdx, int direction);
  void _ClearVoLumKnobSelection();
  void _PromptVoLumKnobExactEntry(int paramIdx, const char* label);
  bool _HandleVoLumSelectedKnobKey(const iplug::IKeyPress& key);
  std::string _GetVoLumKnobHintText(int paramIdx) const;
  bool _HandleVoLumKeyboardFocusKey(const iplug::IKeyPress& key);
  bool _SwitchVoLumKeyboardSection(EVoLumSection section);
  bool _CycleVoLumKeyboardTarget(int direction);
  bool _ActivateVoLumKeyboardTarget();
  bool _ToggleVoLumKeyboardTarget();
  bool _CycleVoLumKeyboardSpeaker(int direction);
  void _UpdateVoLumKeyboardFocusHint();
  int _DefaultVoLumKeyboardKnobForFocus() const;
  int _RememberedVoLumKeyboardKnobForFocus() const;
  void _RememberVoLumKeyboardKnob(int paramIdx);
  void _SyncVoLumExactEntry();
  void _HideVoLumExactEntry();
  void _HideControlGroup(iplug::igraphics::IGraphics* pGfx, const char* group, bool hide);
  void _UpdateVoLumLayout(iplug::igraphics::IGraphics* pGfx = nullptr);
  void _ToggleVoLumTuner();
  void _ToggleVoLumMetronomePanel();
  void _VolumRefreshPrePedalCaptures();
  void _VolumRequestPreNamLoad(int slot);
  void _VolumStartLoader();
  void _VolumStopLoader();
  void _VolumQueueMainModelLoad(std::string fileToLoad, int ampIdx, std::string rigsRoot);
  void _VolumQueueMainPrefetch(std::string fileToLoad);
  void _VolumQueueSupportModelLoad(std::string fileToLoad, int ampIdx);
  void _VolumQueuePreNamLoad(int slot, std::string fileToLoad);
  void _VolumDrainLoaderResults();
  // VoLum: ProcessBlock helpers (tail-included in VoLumProcessBlock.inc.cpp)
  sample** _VolumProcessPreChain(sample** preAmpPointers, const volum::ProcessingPlan& processingPlan,
                                 const size_t numChannelsInternal, const int nFrames, const double sampleRate);
  void _VolumProcessPostChain(iplug::sample** outputs, const volum::ProcessingPlan& processingPlan,
                              const size_t numChannelsExternalOut, const int nFrames, const double sampleRate);
  void _VolumLoaderThreadMain();
  void _VolumRequestSupportModelLoad();
  void _VolumCyclePreNamCapture(int slot, int direction);
  void _VolumSetPreNamCapture(int slot, int captureIdx);
  void _VolumShowPreCaptureMenu(int slot, const iplug::igraphics::IRECT& anchorRect);
  void _VolumHidePreCaptureMenu();
  int _VolumGetPreCaptureCount() const;
  const char* _VolumGetPreCaptureLabel(int captureIdx) const;
  const char* _VolumGetPreCaptureShortLabel(int captureIdx, const char* fallback) const;
  std::string _VolumGetPreCaptureFilename(int captureIdx) const;

private:
  friend class NAMKnobControl;

  EVoLumSection mVolumExpandedSection = EVoLumSection::AMP;
  EVoLumEffectFocus mVolumFocusedEffect = EVoLumEffectFocus::AMP;
  bool mVolumDualAmpFocusedSupport = false;

  int mVolumAmpIdx = 0;
  int mVolumSpeakerIdx = 3; // V30 default
  int mVolumChannelIdx = 0;
  int mVolumSelectedKnobParamIdx = iplug::kNoParameter;
  std::string mVolumSelectedKnobHintText;
  std::array<int, 7> mVolumLastKeyboardKnobByTarget = {
    iplug::kNoParameter, iplug::kNoParameter, iplug::kNoParameter, iplug::kNoParameter,
    iplug::kNoParameter, iplug::kNoParameter, iplug::kNoParameter
  };
  // Reverb sub-mode pill is currently shown for Oktaverb only.
  // Delay AGE knob label and knob/value controls swap per mode (GRIT/WEAR/AGE/BLOOM) and
  // pick up a per-mode tooltip explaining what the knob actually does in that mode.
  // All pointers are non-owning views into the IGraphics tree; nullptr until UI is attached.
  class VoLumSubModePillControl* mVolumReverbSubModePill = nullptr;
  class VoLumKnobLabelControl* mVolumDelayAgeLabel = nullptr;
  iplug::igraphics::IControl* mVolumDelayAgeKnob = nullptr;
  iplug::igraphics::IControl* mVolumDelayAgeValue = nullptr;
  std::vector<std::string> mVolumChannelFiles;
  std::vector<std::string> mVolumChannelLabels;
  std::vector<std::string> mVolumSupportChannelFiles;
  std::vector<std::string> mVolumSupportChannelLabels;
  std::vector<std::string> mVolumPreCaptureFiles;
  std::vector<std::string> mVolumPreCaptureLabels;
  std::vector<std::string> mVolumPreCaptureShortLabels;
  std::vector<volum::PrePedalCaptureGroup> mVolumPreCaptureGroups;
  std::string mVolumRigsRoot;
  std::string mVolumLastLoadedFile;
  std::string mVolumLastLoadedSupportFile;

  std::atomic<bool> mVolumNeedsLoad{false};
  std::atomic<bool> mVolumIsLoading{false};
  std::atomic<bool> mVolumPreNeedsLoad[2]{{false}, {false}};
  std::atomic<bool> mVolumPreIsLoading[2]{{false}, {false}};
  bool mVolumInitComplete = false;
  bool mVolumSettingsDirty = false;
  // Set true while _VolumRestoreReverbModeSnapshot is mid-flight so the cascading
  // OnParamChange / OnParamChangeUI handlers triggered by setParam (which calls
  // SendParameterValueFromDelegate -> OnParamChangeUI) don't re-enter snapshot save /
  // restore logic and corrupt the Oktaverb sub-mode storage.
  bool mVolumReverbRestoreInProgress = false;
  // Set true while restoring per-amp POST values. Amp restore sets live POST params,
  // including mode params; without this guard, those mode changes re-enter the
  // global mode-snapshot restore path and overwrite the per-amp values being loaded.
  bool mVolumPostRestoreInProgress = false;
  std::atomic<bool> mVolumSupportNeedsLoad{false};
  std::atomic<bool> mVolumSupportIsLoading{false};
  std::atomic<bool> mVolumDualAmpOutputHot{false};
  std::atomic<bool> mSupportPolarityInvert{false};
  // Master safety stage telemetry: held briefly after any final post-FX sample crosses
  // the soft-clip knee (~+2.9 dBFS). Read by OnIdle for the footer and OUT meter warning.
  std::atomic<bool> mMasterSafetyEngaged{false};
  int mMasterSafetyHoldSamples = 0;

  enum class VoLumLoadKind { Main, MainPrefetch, Support, Pre };
  struct VoLumLoadRequest
  {
    VoLumLoadKind kind = VoLumLoadKind::Main;
    int slot = -1;
    int ampIdx = -1;
    std::string fileToLoad;
    std::string rigsRoot;
    double sampleRate = 0.0;
    int blockSize = 0;
  };
  struct VoLumLoadResult
  {
    VoLumLoadKind kind = VoLumLoadKind::Main;
    int slot = -1;
    std::string path;
    std::string error;
    double sampleRate = 0.0;
    int blockSize = 0;
    std::unique_ptr<ResamplingNAM> model;
  };

  std::thread mVolumLoaderThread;
  std::mutex mVolumLoaderMutex;
  std::condition_variable mVolumLoaderCv;
  std::deque<VoLumLoadRequest> mVolumLoadRequests;
  std::deque<VoLumLoadResult> mVolumLoadResults;
  std::atomic<bool> mVolumLoaderStop{false};

  // Parsed NAM configs keyed by full path. Small LRU keeps switch-back fast without retaining every rig.
  static constexpr size_t kVolumDspCacheMaxEntries = 8;
  std::unordered_map<std::string, nam::dspData> mVolumDspCache;
  std::deque<std::string> mVolumDspCacheOrder;
  std::string mVolumLoadingMainPath;
  std::string mVolumLoadingSupportPath;
  std::string mVolumLoadingPrePath[2];

  // Per-amp settings: remembered across amp switches and sessions
  std::array<volum::VoLumAmpSettings, volum::kAmpCount> mVolumAmpSettings;
  volum::VoLumEffectSettings mVolumEffectSettings;
  bool mVolumPreLocked = false;
  bool mVolumPostLocked = false;
  bool mVolumPreLockUiDirty = false;
  bool mVolumPostLockUiDirty = false;

  // Tuner & Metronome DSP
  volum::TunerDSP mTunerDSP;
  volum::MetronomeDSP mMetronomeDSP;
  // Loads an IR and stores it to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath);

  bool _HaveModel() const { return this->mModel != nullptr; };
  // Prepare the input & output buffers
  void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
  // Manage pointers
  void _PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, applying input level.
  // :param nChansIn: In from external
  // :param nChansOut: Out to the internal of the DSP routine
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
  // Copy the output to the output buffer, applying output level.
  // :param nChansIn: In from internal
  // :param nChansOut: Out to external
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut);
  // Resetting for models and IRs, called by OnReset
  void _ResetModelAndIR(const double sampleRate, const int maxBlockSize);

  void _SetInputGain();
  void _SetOutputGain();
  // Mirror of _SetOutputGain for the support lane: factors in the support model's loudness
  // (when OutputMode is Normalized) or its calibration level (when Calibrated) so support
  // matches main at identical knob settings, identical models, and identical OutputMode.
  void _SetSupportOutputGain();

  // See: Unserialization.cpp
  void _UnserializeApplyConfig(nlohmann::json& config);
  // 0.7.9 and later
  int _UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos);
  // Hopefully 0.7.3-0.7.8, but no gurantees
  int _UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos);

  // Update all controls that depend on a model
  void _UpdateControlsFromModel();

  // Make sure that the latency is reported correctly.
  void _UpdateLatency();

  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                     const size_t nChansIn, const size_t nChansOut);

  // Member data

  // Input arrays to NAM
  std::vector<std::vector<iplug::sample>> mInputArray;
  // Output from NAM
  std::vector<std::vector<iplug::sample>> mOutputArray;
  // Pointer versions
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;

  // Input and output gain
  double mInputGain = 1.0;
  double mOutputGain = 1.0;
  // Cached support-lane equivalent of mOutputGain. Recomputed by _SetSupportOutputGain when
  // kSupportOutputLevel, kOutputMode, or the support model itself changes.
  double mSupportOutputGain = 1.0;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  dsp::effect::VoLumCompressor mPreCompressor;
  dsp::effect::VoLumPreEq mPreEq[2];
  recursive_linear_filter::Level mPreInputGain[2];
  recursive_linear_filter::Level mPreOutputGain[2];
  dsp::effect::Delay mDelay;
  dsp::effect::Reverb mReverb;
  // The model actually being used:
  std::unique_ptr<ResamplingNAM> mModel;
  std::unique_ptr<ResamplingNAM> mSupportModel;
  std::unique_ptr<ResamplingNAM> mPreModel[2];
  // And the IR
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  // Manages switching what DSP is being used.
  std::unique_ptr<ResamplingNAM> mStagedModel;
  std::unique_ptr<ResamplingNAM> mStagedSupportModel;
  std::unique_ptr<ResamplingNAM> mStagedPreModel[2];
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR;
  // Flags to take away the modules at a safe time.
  std::atomic<bool> mShouldRemoveModel = false;
  std::atomic<bool> mShouldRemoveSupportModel = false;
  std::atomic<bool> mShouldRemovePreModel[2]{{false}, {false}};
  std::atomic<bool> mShouldRemoveIR = false;

  std::atomic<bool> mNewModelLoadedInDSP = false;
  std::atomic<bool> mModelCleared = false;
  bool mPostEffectsClearedForMissingModel = false;
  // Previous-block POST plan flags. Used to detect a true -> false edge for Delay or
  // Reverb and call Reset() on that edge so re-enabling the effect later does not
  // replay stale tails. Reset to false in OnReset.
  bool mPostDelayWasActive = false;
  bool mPostReverbWasActive = false;
  // Serializes writes from non-audio threads (UnserializeState path -> _StageModel /
  // _StageIR) against the audio-thread read/move in _ApplyDSPStaging. The VoLum
  // worker-queue path drains on the audio thread already, so it does not need this
  // mutex; it is for the legacy NAM staging entry points only.
  mutable std::mutex mStagingMutex;

  // Tone stack modules
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mToneStack;
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mSupportToneStack;

  // Post-IR filters
  recursive_linear_filter::HighPass mHighPass;
  recursive_linear_filter::HighPass mSupportHighPass;
  //  recursive_linear_filter::LowPass mLowPass;

  dsp::noise_gate::Trigger mSupportNoiseGateTrigger;
  dsp::noise_gate::Gain mSupportNoiseGateGain;
  std::vector<iplug::sample> mDualMainLaneBuffer;
  std::vector<iplug::sample> mDualSupportLaneBuffer;
  std::vector<iplug::sample> mDualMainAlignedBuffer;
  std::vector<iplug::sample> mDualSupportAlignedBuffer;
  volum::DualAmpDelayLine<iplug::sample> mDualMainLatencyDelay;
  volum::DualAmpDelayLine<iplug::sample> mDualSupportLatencyDelay;

  // Path to model's config.json or model.nam
  WDL_String mNAMPath;
  // Path to IR (.wav file)
  WDL_String mIRPath;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  NAMSender mInputSender, mOutputSender;
  // Right-channel output meter sender (used in dual-amp/stereo mode for the second OUT bar).
  NAMSender mOutputSenderR;
};
