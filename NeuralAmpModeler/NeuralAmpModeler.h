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

#if VOLUM_AMPETE_PRODUCT
#include "VoLumAmpeteCatalog.h"
#include "VoLumTriptychState.h"
#include "VoLumUserSettingsIO.h"
#include "VoLumTunerDSP.h"
#include "VoLumMetronomeDSP.h"
#endif

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
  // Reverb (POST)
  kReverbActive,
  kReverbMix,
  kReverbDecay,
  kReverbTone,
  kReverbPreDelay,
  kReverbShimmer,
  kReverbMode,
  // Boost (PRE - stub for future)
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
#if VOLUM_AMPETE_PRODUCT
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
#endif
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
#if VOLUM_AMPETE_PRODUCT
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
#endif
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
#if VOLUM_AMPETE_PRODUCT
public:
  void _VolumRefreshChannels();
  void _VolumRefreshSupportChannels();
  void _VolumApplyDualAmpFocus();
  void _VolumShowSupportAmpMenu(const iplug::igraphics::IRECT& anchorRect);
  void _VolumHideSupportAmpMenu();
  void _VolumSetSupportAmp(int ampIdx);
  void _VolumSaveCurrentToSettings();
  void _VolumRestoreFromSettings(int ampIdx);
  void _VolumSaveSettingsToFile();
  void _VolumLoadSettingsFromFile();
  void _VolumSaveEffectSettings();
  void _VolumRestoreEffectSettings();
  void _VolumSaveDelayModeSnapshot(int mode);
  void _VolumRestoreDelayModeSnapshot(int mode);
  void _VolumSaveReverbModeSnapshot(int mode);
  void _VolumRestoreReverbModeSnapshot(int mode);
  void _SelectVoLumKnob(int paramIdx);
  bool _SelectAdjacentVoLumKnob(int currentParamIdx, int direction);
  void _ClearVoLumKnobSelection();
  void _PromptVoLumKnobExactEntry(int paramIdx, const char* label);
  bool _HandleVoLumSelectedKnobKey(const iplug::IKeyPress& key);
  std::string _GetVoLumKnobHintText(int paramIdx) const;
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
  void _VolumQueueSupportModelLoad(std::string fileToLoad, int ampIdx);
  void _VolumQueuePreNamLoad(int slot, std::string fileToLoad);
  void _VolumDrainLoaderResults();
  void _VolumLoaderThreadMain();
  void _VolumRequestSupportModelLoad();
  void _VolumCyclePreNamCapture(int slot, int direction);
  void _VolumSetPreNamCapture(int slot, int captureIdx);
  void _VolumShowPreCaptureMenu(int slot, const iplug::igraphics::IRECT& anchorRect);
  void _VolumHidePreCaptureMenu();
  int _VolumGetPreCaptureCount() const;
  const char* _VolumGetPreCaptureLabel(int captureIdx) const;
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
  std::vector<std::string> mVolumChannelFiles;
  std::vector<std::string> mVolumChannelLabels;
  std::vector<std::string> mVolumSupportChannelFiles;
  std::vector<std::string> mVolumSupportChannelLabels;
  std::vector<std::string> mVolumPreCaptureFiles;
  std::vector<std::string> mVolumPreCaptureLabels;
  std::string mVolumRigsRoot;
  std::string mVolumLastLoadedFile;
  std::string mVolumLastLoadedSupportFile;

  std::atomic<bool> mVolumNeedsLoad{false};
  std::atomic<bool> mVolumIsLoading{false};
  std::atomic<bool> mVolumPreNeedsLoad[2]{{false}, {false}};
  std::atomic<bool> mVolumPreIsLoading[2]{{false}, {false}};
  bool mVolumInitComplete = false;
  bool mVolumSettingsDirty = false;
  std::atomic<bool> mVolumSupportNeedsLoad{false};
  std::atomic<bool> mVolumSupportIsLoading{false};
  std::atomic<bool> mVolumDualAmpOutputHot{false};

  enum class VoLumLoadKind { Main, Support, Pre };
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
    std::unique_ptr<ResamplingNAM> model;
  };

  std::thread mVolumLoaderThread;
  std::mutex mVolumLoaderMutex;
  std::condition_variable mVolumLoaderCv;
  std::deque<VoLumLoadRequest> mVolumLoadRequests;
  std::deque<VoLumLoadResult> mVolumLoadResults;
  std::atomic<bool> mVolumLoaderStop{false};

  // Per-amp cache: parsed dspData keyed by filename, avoids re-parsing JSON
  std::unordered_map<std::string, nam::dspData> mVolumDspCache;
  int mVolumCachedAmpIdx = -1;

  // Per-amp settings: remembered across amp switches and sessions
  std::array<volum::VoLumAmpSettings, volum::kAmpCount> mVolumAmpSettings;
  volum::VoLumEffectSettings mVolumEffectSettings;

  // Tuner & Metronome DSP
  volum::TunerDSP mTunerDSP;
  volum::MetronomeDSP mMetronomeDSP;
#endif
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

  // Tone stack modules
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mToneStack;
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mSupportToneStack;

  // Post-IR filters
  recursive_linear_filter::HighPass mHighPass;
  recursive_linear_filter::HighPass mSupportHighPass;
  //  recursive_linear_filter::LowPass mLowPass;

#if VOLUM_AMPETE_PRODUCT
  dsp::noise_gate::Trigger mSupportNoiseGateTrigger;
  dsp::noise_gate::Gain mSupportNoiseGateGain;
  std::vector<iplug::sample> mDualMainLaneBuffer;
  std::vector<iplug::sample> mDualSupportLaneBuffer;
#endif

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
