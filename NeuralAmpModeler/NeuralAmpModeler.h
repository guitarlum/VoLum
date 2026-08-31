#pragma once

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/Delay.h"
#include "../AudioDSPTools/dsp/Reverb.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/wav.h"
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"
#include "../NeuralAmpModelerCore/NAM/slimmable.h"

#include "Colors.h"
#include "ToneStack.h"
#include "VoLumDualAmpPlan.h"
#include "VoLumPreEffects.h"
#include "VoLumPitchShifter.h"

#include "config.h"
#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
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
#include "VoLumTremolo.h"
#include "VoLumChorus.h"
#include "VoLumLatencyReport.h"
#include "VoLumMidi.h"
#include "VoLumProcessingPlan.h"
#include "VoLumUiSyncPlan.h"
#include "VoLumDspStagingWdl.h"
#include "VoLumContentStore.h" // 1.2.0 custom-content backend (F5-F8) + kDirectSlot
#include "VoLumUpdateCheck.h"
#include "VoLumUpdateState.h"
#include "VoLumPlayModel.h"
#include "VoLumRigRepair.h" // 1.3.0 delete / Pack-replace of a sounding library id
#include "VoLumPack.h" // 1.3.0 .volumpack export / import

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

// EParams + kNumParams live in VoLumParams.h so test TUs can read the real
// param count without the IGraphics/iPlug plugin headers. Single source of
// truth; order is serialization-sensitive (see VoLumParams.h).
#include "VoLumParams.h"

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
  kCtrlTagVoLumUpdateBadge,
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
  kCtrlTagVoLumPitchCard,
  kCtrlTagVoLumCompCard,
  kCtrlTagVoLumPreNam1Card,
  kCtrlTagVoLumPreNam2Card,
  kCtrlTagVoLumPreCaptureMenu,
  kCtrlTagVoLumPreChainConnector1,
  kCtrlTagVoLumPreChainConnector2,
  kCtrlTagVoLumPreChainConnector3,
  kCtrlTagVoLumChorusCard,
  kCtrlTagVoLumDelayCard,
  kCtrlTagVoLumReverbCard,
  kCtrlTagVoLumTremoloCard,
  kCtrlTagVoLumChainConnector,
  kCtrlTagVoLumChainConnector2,
  kCtrlTagVoLumChainConnector3,
  kCtrlTagVoLumSubRowText,
  kCtrlTagVoLumNoiseGate,
  kCtrlTagVoLumEQ,
  kCtrlTagVoLumTuner,
  kCtrlTagVoLumMetronome,
  kCtrlTagVoLumMetronomeButton,
  // 1.2.0 BYO + presets (UI shells)
  kCtrlTagVoLumPresetBar,
  kCtrlTagVoLumIrMenu,
  kCtrlTagVoLumPresetMenu,
  kCtrlTagVoLumCustomOverlay,
  kCtrlTagVoLumConfirm,
  kCtrlTagVoLumPlaySurface,
  kCtrlTagVoLumModeToggle,
  kCtrlTagVoLumPackOverlay,
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

  // VoLum: if the encapsulated model is a slimmable container (A2), select its
  // Lite (val < 0.5) or Full (val >= 0.5) slice. Plain (non-slimmable) models
  // no-op gracefully. The container prepares the inactive slice under its own
  // mutex; call this off the audio thread (loader thread / staging), not in
  // ProcessBlock.
  void SetSlimmableSize(const double val)
  {
    if (auto* slim = dynamic_cast<nam::SlimmableModel*>(mEncapsulated.get()))
      slim->SetSlimmableSize(val);
  };

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
  void ProcessMidiMsg(const iplug::IMidiMsg& msg) override;
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

  // Shared headless Sound recall used by MIDI and PLAY. Returns false without
  // changing the sounding rig when either id cannot be resolved.
  bool VolumRecallSound(const std::string& ampId, const std::string& presetId);

private:
  // Allocates mInputPointers and mOutputPointers
  void _AllocateIOPointers(const size_t nChans);
  // Moves DSP modules from staging area to the main area.
  // Also deletes DSP modules that are flagged for removal.
  // Exists so that we don't try to use a DSP module that's only
  // partially-instantiated.
  void _ApplyDSPStaging();
  // VoLum: advance both lanes' deferred IR swaps once their replacement capture is
  // staged (or the wait deadline expires). A removal fires by flagging the convolver
  // for deletion; an addition is held back instead, so the out params tell the
  // caller which lanes must not promote their staged IR this block.
  // Audio thread, mStagingMutex held.
  void _VolumStepDeferredIrSwaps(bool& holdMainIr, bool& holdSupportIr);
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
  void _VolumSetSupportCustom(int customIdx);
  void _VolumApplyFocusedLaneCabs();
  // Single entry point that pushes restored backend state into the freshly built
  // controls. The editor rebuilds every control from constructor defaults on each
  // open, so a reopen must re-derive the whole visible selection rather than
  // assume a control already holds the right value. Call this after any restore
  // (editor open, DAW chunk load, session re-focus).
  void _VolumSyncUiFromState();
  // Apply one resolved UiSyncPlan to the cab row + channel stepper, and write the
  // resolved custom routing back into the runtime caches.
  void _VolumApplyUiSyncPlan(const volum::UiSyncPlan& plan, bool support);
  // Build the plan input for the given lane from live backend state.
  volum::UiSyncInput _VolumMakeUiSyncInput(bool support, const volum::custom::CustomAmp& customAmp);
  // Push the given lane's active custom IR onto the shared speaker row's IR chip
  // (empty/orphaned id -> chip off). Per-lane custom IR display.
  void _VolumReflectLaneIrChip(bool support);
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
  // Applies the persisted live PRE/POST lock snapshots to the live params when
  // the corresponding lock is engaged. Intended for one-shot use on init paths
  // (settings load + chunk unserialize); does NOT touch per-amp slots.
  void _VolumApplyLiveLockSnapshots();
  void _VolumRestoreFromSettings(int ampIdx);
  // Apply an arbitrary settings snapshot to the live params (factory slot, custom
  // amp scene, or recalled preset). Mutable because POST restore normalizes the
  // mode snapshots back into the struct (see _VolumRestorePostFromSlot).
  void _VolumApplyAmpSettings(volum::VoLumAmpSettings& s);
  // Re-applies every DSP value that is cached in a plugin member and therefore
  // only refreshed inside OnParamChange. Programmatic restores (preset recall,
  // amp switch, session/DAW restore) push params via SendParameterValueFromDelegate
  // which bypasses OnParamChange, so the cached gains/tone coefficients would
  // otherwise stay stale (e.g. output stuck at silence until a manual knob nudge).
  // See volum::dsp_cache::kRestoreReappliedCaches for the locked param set.
  void _VolumApplyDspCaches();
  void _VolumSaveSettingsToFile();
  void _VolumSaveCalibrationDefaults();
  void _VolumLoadSettingsFromFile();
  // VoLum: set the machine-global A2 Lite/Full mode, persist it, and reload all
  // four NAM lanes so the new slice is applied through the async staging path.
  void _VolumSetLiteMode(bool lite);
  bool _VolumIsLiteMode() const { return mVolumLiteMode.load(); }
  void _VolumCheckForUpdatesNow();
  void _VolumSetAutoUpdateCheck(bool enabled);
  void _VolumUseAvailableUpdate();
  void _VolumSaveEffectSettings();
  void _VolumRestoreEffectSettings();
  void _VolumSaveDelayModeSnapshot(int mode);
  void _VolumRestoreDelayModeSnapshot(int mode);
  void _VolumSaveReverbModeSnapshot(int mode);
  void _VolumRestoreReverbModeSnapshot(int mode);
  void _VolumSaveOktaverbSubModeSnapshot(int subMode);
  void _VolumRestoreOktaverbSubModeSnapshot(int subMode);
  void _VolumSaveTremoloModeSnapshot(int mode);
  void _VolumRestoreTremoloModeSnapshot(int mode);
  void _VolumSaveChorusModeSnapshot(int mode);
  void _VolumRestoreChorusModeSnapshot(int mode);
  void _VolumSavePrePitchModeSnapshot(int mode);
  void _VolumRestorePrePitchModeSnapshot(int mode);
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
  // Single funnel for a user-initiated boolean param toggle, shared by the mouse
  // (hero DUAL chip, pedal pills) and keyboard paths so both notify the host,
  // run OnParamChange side effects, AND mark the preset dirty. Returns the new
  // value. Callers append any path-specific UI/focus updates.
  bool _VolumUserToggleParam(int paramIdx);
  bool _CycleVoLumKeyboardSpeaker(int direction);
  void _UpdateVoLumKeyboardFocusHint();
  int _DefaultVoLumKeyboardKnobForFocus() const;
  int _RememberedVoLumKeyboardKnobForFocus() const;
  void _RememberVoLumKeyboardKnob(int paramIdx);
  void _SyncVoLumExactEntry();
  void _HideVoLumExactEntry();
  void _HideControlGroup(iplug::igraphics::IGraphics* pGfx, const char* group, bool hide);
  // Full one-time UI build/attach pass (the body of the constructor's layout
  // lambda); defined in VoLumLayoutBuild.inc.cpp.
  void _BuildVoLumLayout(iplug::igraphics::IGraphics* pGraphics);
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
  iplug::sample** _VolumProcessPreChain(iplug::sample** preAmpPointers, const volum::ProcessingPlan& processingPlan,
                                        const size_t numChannelsInternal, const int nFrames, const double sampleRate);
  iplug::sample** _VolumProcessMainAmpChain(iplug::sample** preAmpPointers, const volum::ProcessingPlan& processingPlan,
                                            const size_t numChannelsInternal, const int nFrames,
                                            const double sampleRate);
  iplug::sample* _VolumProcessDualAmpSupportLane(const volum::ProcessingPlan& processingPlan,
                                                 const size_t numChannelsInternal, const int nFrames,
                                                 const double sampleRate);
  void _VolumProcessPostChain(iplug::sample** outputs, const volum::ProcessingPlan& processingPlan,
                              const size_t numChannelsExternalOut, const int nFrames, const double sampleRate);
  void _VolumLoaderThreadMain();
  void _VolumRequestSupportModelLoad();
  void _VolumSetPreNamCapture(int slot, int captureIdx);
  void _VolumShowPreCaptureMenu(int slot, const iplug::igraphics::IRECT& anchorRect);
  void _VolumShowManageCustomPedals(int preSlot = -1);
  void _VolumShowPresetMenu();
  void _VolumSetUiMode(volum::UiMode mode);
  void _VolumRefreshPlaySurface();
  bool _VolumTogglePlayBypass(const char* paramName);
  void _VolumAssignPlaySound(int slot, const volum::SoundChoice& sound);
  void _VolumClearPlaySound(int slot);
  // Select a factory amp exactly as clicking its sidebar row does (scene restore +
  // capture reload + chrome). Shared by the sidebar, the keyboard, and the
  // delete-while-playing fallback.
  // snapshotOutgoing=false skips folding the live knobs into the outgoing lane's
  // slot; the delete path uses it, because the amp those knobs belong to is gone.
  void _VolumSelectFactoryAmp(int ampIdx, bool snapshotOutgoing = true);
  void _VolumSelectCustomAmp(int customIdx);

  // --- Delete / Pack-replace of a library id this instance is playing ----------
  // Snapshot the sounding rig in library terms for volum::rig::PlanDelete.
  volum::rig::SoundingRig _VolumSnapshotSoundingRig() const;
  // Plan the repair for an about-to-happen delete (or Pack replace) and remember
  // it. Returns the confirm-dialog body, which names the in-use case and the
  // destination. Planned before the catalog mutation because a pedal's rig
  // reference is its capture index, which the delete takes with it.
  std::string _VolumPlanLibraryDelete(volum::rig::LibraryKind kind, const std::string& id,
                                      const std::string& displayName);
  std::string _VolumPlanLibraryReplace(volum::rig::LibraryKind kind, const std::string& id,
                                       const std::string& displayName);
  // Run the plan remembered by the two above; clears it. No-op when the deleted id
  // was not sounding, which is the common case.
  void _VolumApplyPendingRigRepair();
  void _VolumApplyRigRepair(const volum::rig::RigRepairPlan& plan);
  // Re-derive the rig after a catalog change made by *another* instance, i.e. when
  // an id this instance is still playing has disappeared from the library. Called
  // on the next moment this instance needs that id.
  void _VolumRepairRigForMissingContent();

  // --- Pack export / import (Gear -> Settings) ---------------------------------
  // The Pack modal asks the questions; these three do the IO. Export/import
  // return an error string for the modal's status line, or "" on success (and on a
  // cancelled file dialog, which is not a failure).
  std::string _VolumExportPack(const volum::pack::ExportSelection& selection);
  volum::pack::PackContents _VolumPickPack();
  std::string _VolumImportPack(const volum::pack::PackContents& pack, volum::pack::ImportVerb verb, bool alsoSettings);
  // Library ids this instance's rig is playing, so an import preview can name what
  // it would have to reload.
  std::vector<std::string> _VolumSoundingLibraryIds() const;
  // Lanes playing an id the import replaced move to the new payload rather than to
  // the delete fallback: a confirmed replace is not a delete.
  void _VolumReloadReplacedLibraryIds(const std::vector<std::string>& ids);
  // Push a custom main amp's named cabs (empty slots disabled), Custom-IR state,
  // and channel labels into the shared speaker row + channel stepper (display
  // only; no model load). mVolumCustomMainIdx tracks the focused custom main amp
  // (-1 = a factory amp is active).
  void _VolumApplyCustomMainCabs(int customIdx, bool supportLane = false);
  void _VolumSetCustomChannelStepper(int customIdx, bool supportLane, int channel);
  // F7 custom IR: the mutable settings of the currently active lane (factory amp
  // slot, or the focused custom amp's scene). activeIrId/supportActiveIrId/
  // supportCustomId all live here (support fields belong to the MAIN scene).
  volum::VoLumAmpSettings& _VolumActiveScene();
  // Display name of the MAIN amp lane: the focused custom amp when one is active,
  // otherwise the factory catalog name. Single source for every place that labels
  // the AMP box (hero, sub-row, triptych spine, preset manage subtitle).
  const char* _VolumMainAmpDisplayName() const;
  // True when the dual-amp SUPPORT lane currently owns the shared cab/IR controls.
  bool _VolumSupportFocused() { return GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport; }
  // True when the SUPPORT lane actually has an amp: a factory amp or a custom
  // partner. Its default is "(none)".
  bool _VolumHasSupportAmp();
  // Drops SUPPORT focus when that lane has no amp. Focusing an empty lane pointed
  // the shared cab row, the channel stepper and the S shortcut at something that
  // does not exist: the row jumped to a phantom cab, the stepper read "---", and S
  // edited a parameter nothing was listening to.
  void _VolumClampSupportFocus();
  // Stage the custom IR at library index irIdx into the given lane's convolver,
  // enable that lane's IR toggle, record its IR id on the active scene, and
  // reflect it in the cab row when that lane is the one displayed.
  // interactive=true surfaces a message box when the lane's custom amp has no
  // DIRECT capture to convolve; the restore path passes false (silent skip).
  void _VolumSelectIR(int irIdx, bool support, bool interactive = true);
  // Drop the given lane's custom IR (back to the baked cab) and clear its IR id.
  // deferToCabSwap delays the convolver removal until the replacement cab capture
  // is staged, so the two swap on one block instead of leaving a gap of raw amp.
  void _VolumClearIR(bool support, bool deferToCabSwap = false);
  // Re-resolve a scene/preset's IR id for a lane on recall: stage it, or fall back
  // to the baked cab when the id is empty/orphaned (the referenced IR was deleted).
  void _VolumApplyActiveIr(const std::string& irId, bool support);
  // Force the given lane's amp onto its DIRECT / No-Cab capture so a custom IR
  // convolves the raw amp instead of an already-cabbed signal. Returns true when
  // that queued a capture load, which the IR must wait for before it convolves.
  bool _VolumForceDirectCapture(bool support);
  // Standalone session restore (custom MAIN focus + active preset), run once when
  // the UI opens so the cab controls exist for a custom-amp re-focus.
  void _VolumRestoreSessionSelection();
  // After a Manage-panel mutation, drop the live custom IR + recover to a real
  // cab if the active IR was deleted (or otherwise orphaned). No-op otherwise.
  void _VolumReconcileActiveIr();
  // Drop the custom IR convolver and select the first available real cab (one of
  // the baked cabs); only land on DIRECT / No-Cab when no real cab exists.
  void _VolumFallbackToAvailableCab();
  // Flags the header preset strip "(unsaved)" for rig edits that bypass the
  // kUI param hook (cab/channel/IR/polarity changes set members or use kDelegate).
  void _VolumMarkPresetDirty();
  // Resets the active amp's params to shipped factory defaults and clears any
  // recalled preset (preset dropdown "Default" row).
  void _VolumResetAmpToFactory();

  // --- F5 presets (per-amp bank, owner-keyed in the content registry) --------
  // Owner key for the currently focused amp: "factory:<idx>" or a custom amp id.
  std::string _VolumActiveOwnerKey() const;
  // Install the capture/apply hooks so registry preset ops read and write the real
  // live settings, and record this instance as their owner.
  void _VolumInstallPresetHooks();
  // Claim the process-global preset bridge (hooks + active owner key) for this
  // instance. Called by every preset operation, because the bridge is shared by all
  // instances in the host and the last one to claim it wins.
  std::string _VolumClaimPresetOps();
  void _VolumSyncPresetOwner();
  // Refresh the header bar's list/selection/dirty state for the active amp.
  void _VolumRefreshPresetBar();
  // Headless Sound recall shared by MIDI and the preset UI. Validates the amp
  // and named preset before changing the sounding rig.
  bool _VolumRecallSound(const std::string& ampId, const std::string& presetId);
  void _VolumRefreshMidiSettingsChrome();
  void _VolumSetMidiChannel(int channel);
  // Save the live scene as a new named preset; returns its bank index (-1 fail).
  int _VolumSavePresetAs(const std::string& name);
  // Overwrite preset `index` in the active bank with the live scene.
  void _VolumOverwritePreset(int index);
  // Recall preset `index`: apply its snapshot to the live chain, retain it as the
  // recalled snapshot (drives the equality-based "(unsaved)" flag), update the bar.
  void _VolumRecallPreset(int index);
  void _VolumRecallUserPreset(int index);
  void _VolumRecallFactoryPreset();
  // Apply a recalled snapshot to the live chain and retain it (called by the
  // bridge apply hook so Manage/menu/bar recalls share one path).
  void _VolumApplyRecalledPreset(const volum::VoLumAmpSettings& s);
  // Re-evaluate the "(unsaved)" flag from a live-vs-snapshot equality test.
  void _VolumRecomputePresetDirty();
  // The snapshot of the last recalled/saved preset for the active amp, and
  // whether one is active. Cleared on amp switch / factory reset.
  volum::VoLumAmpSettings mVolumRecalledSnapshot;
  bool mVolumHasRecalledSnapshot = false;
  std::string mVolumActivePresetId;
  // Per-owner memory of the active preset + its recalled snapshot so switching
  // back to an amp re-shows the preset it had selected (keyed by owner key:
  // "factory:<idx>" or a custom amp id). Populated on recall/save/overwrite and
  // restored in _VolumSyncPresetOwner; pruned when a preset is deleted/missing.
  std::unordered_map<std::string, std::string> mVolumActivePresetIdByOwner;
  std::unordered_map<std::string, volum::VoLumAmpSettings> mVolumRecalledSnapshotByOwner;
  std::vector<volum::FactoryPreset> mVolumFactoryPresets;
  volum::UiMode mVolumUiMode = volum::UiMode::Build;
  int mVolumLastRecalledPlaySlot = -1;
  // This instance's live scene per custom amp id - the custom-amp equivalent of
  // mVolumAmpSettings[ampIdx]. Before 1.3.0 this map lived in the shared content
  // library, so one instance's catalog write moved another instance's knobs; the
  // sounding rig belongs to the instance (DAW chunk / standalone settings) now.
  // Seeded on first touch from a pre-1.3.0 library's customScenes, so an upgrade
  // keeps the knobs the user left behind.
  std::map<std::string, volum::VoLumAmpSettings> mVolumCustomScenes;
  volum::VoLumAmpSettings& _VolumCustomScene(const std::string& ampId);
  // The repair planned for the delete/replace the confirm dialog is asking about.
  // Held between the plan and the user's answer; empty repairs mean "nothing this
  // instance is playing is affected".
  volum::rig::RigRepairPlan mVolumPendingRigRepair;
  // Record/forget the active preset for the current owner key.
  void _VolumRememberActivePreset();
  void _VolumForgetActivePreset();
  // Standalone (volum-settings.json) restore: the focused custom MAIN amp id and
  // active preset id from the last session, re-applied once when the UI opens.
  std::string mVolumRestoreCustomMainId;
  std::string mVolumRestorePresetId;
  bool mVolumDidRestorePresetSelection = false;
  void _VolumHidePreCaptureMenu();
  int _VolumGetPreCaptureCount() const;
  const char* _VolumGetPreCaptureLabel(int captureIdx) const;
  const char* _VolumGetPreCaptureShortLabel(int captureIdx, const char* fallback) const;
  std::string _VolumGetPreCaptureFilename(int captureIdx) const;
  // Absolute path to stage for a PRE-capture index: factory captures resolve under
  // rigs/PrePedals; custom-pedal indices (>= kCustomPedalIndexBase) resolve from
  // the content library. Returns "" when nothing maps to the index.
  std::string _VolumGetPreCaptureLoadPath(int captureIdx) const;
  // Ordered, selectable PRE-capture indices for cycling: 0 (EMPTY), factory
  // 1..N, then each imported pedal's stable legacy index.
  // Stable scratch buffer so _VolumGetPreCaptureLabel can return a const char*
  // for custom pedal names (which are not in the factory label vector).
  mutable std::string mVolumPreCaptureLabelScratch;

private:
  friend class NAMKnobControl;

  EVoLumSection mVolumExpandedSection = EVoLumSection::AMP;
  EVoLumEffectFocus mVolumFocusedEffect = EVoLumEffectFocus::AMP;
  bool mVolumDualAmpFocusedSupport = false;
  // Index of the focused custom MAIN amp (display-only), or -1 when a factory
  // amp is active. Drives the custom-aware cabinet row / channel stepper.
  int mVolumCustomMainIdx = -1;
  // Selected (slot, channel) within the focused custom MAIN amp, used to resolve
  // which manifest .nam to stage. Only meaningful when mVolumCustomMainIdx >= 0.
  int mVolumCustomMainSlot = volum::custom::kDirectSlot;
  int mVolumCustomMainChannel = 1;
  // Index of the custom SUPPORT amp (dual-amp partner), or -1 when the support
  // partner is a factory amp / none.
  int mVolumCustomSupportIdx = -1;
  int mVolumCustomSupportSlot = volum::custom::kDirectSlot;
  int mVolumCustomSupportChannel = 1;

  int mVolumAmpIdx = 0;
  int mVolumSpeakerIdx = 3; // V30 default
  int mVolumChannelIdx = 0;
  int mVolumSelectedKnobParamIdx = iplug::kNoParameter;
  std::string mVolumSelectedKnobHintText;
  // Size must match volum::keyboard::kTargetCount (9). Literal here to avoid
  // pulling VoLumKeyboardModel.h into this header before EParams is declared.
  std::array<int, 9> mVolumLastKeyboardKnobByTarget = {iplug::kNoParameter, iplug::kNoParameter, iplug::kNoParameter,
                                                       iplug::kNoParameter, iplug::kNoParameter, iplug::kNoParameter,
                                                       iplug::kNoParameter, iplug::kNoParameter, iplug::kNoParameter};
  // Reverb sub-mode pill is currently shown for Oktaverb only.
  // Delay AGE knob label and knob/value controls swap per mode (GRIT/WEAR/AGE/BLOOM) and
  // pick up a per-mode tooltip explaining what the knob actually does in that mode.
  // All pointers are non-owning views into the IGraphics tree; nullptr until UI is attached.
  class VoLumSubModePillControl* mVolumReverbSubModePill = nullptr;
  class VoLumKnobLabelControl* mVolumDelayAgeLabel = nullptr;
  iplug::igraphics::IControl* mVolumDelayAgeKnob = nullptr;
  iplug::igraphics::IControl* mVolumDelayAgeValue = nullptr;
  // Tremolo tempo-sync DIVISION stepper (shown in the RATE slot when Sync is on).
  // Non-owning view; refreshed from kTremoloDivision in _UpdateVoLumLayout.
  class VoLumChannelStepControl* mVolumTremoloDivStep = nullptr;
  // Delay tempo-sync DIVISION stepper (shown in the TIME slot when Sync is on).
  class VoLumChannelStepControl* mVolumDelayDivStep = nullptr;
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
  std::string mVolumRequestedMainFile;
  std::string mVolumMainLoadError;

  std::atomic<bool> mVolumNeedsLoad{false};
  std::atomic<bool> mVolumIsLoading{false};
  std::atomic<bool> mVolumMainLoadFailed{false};
  // Set when host state was restored into an already-open editor, consumed by the
  // next OnIdle. UnserializeState runs on the host's thread, and the applier it
  // wants writes IGraphics controls, so the call has to cross to the UI thread.
  std::atomic<bool> mVolumUiSyncPending{false};
  // Audio-thread MIDI ingress. Only an int crosses this capacity-one latest-wins
  // handoff; content-library resolution happens in OnIdle.
  volum::MidiLatestWinsQueue mVolumMidiQueue;
  std::atomic<int> mVolumMidiChannel{volum::kMidiOmniChannel};
  // VoLum: when set, the next main-lane load in OnIdle bypasses the
  // same-path short-circuit so an A2 Lite/Full toggle re-stages the main model
  // even though its file path is unchanged.
  std::atomic<bool> mVolumForceMainReload{false};
  std::atomic<bool> mVolumPreNeedsLoad[2]{{false}, {false}};
  std::atomic<bool> mVolumPreIsLoading[2]{{false}, {false}};
  // VoLum: machine-global A2 Lite mode. false = Full (best quality, default),
  // true = Lite (smaller slice, lower CPU). Persisted in volum-settings.json
  // (NOT the plugin chunk), applied to every lane at model load time. Read on
  // the loader thread, written on the main thread -> atomic.
  std::atomic<bool> mVolumLiteMode{false};
  bool mVolumInitComplete = false;
  // Last report pushed to the Settings page, so the OnIdle poll only touches the UI
  // when a number actually moved.
  volum::LatencyReport mVolumLastLatencyReport{};
  bool mVolumSettingsDirty = false;
  bool mVolumCalibrationDefaultsDirty = false;
  // Set true while _VolumRestoreReverbModeSnapshot is mid-flight so the cascading
  // OnParamChange / OnParamChangeUI handlers triggered by setParam (which calls
  // SendParameterValueFromDelegate -> OnParamChangeUI) don't re-enter snapshot save /
  // restore logic and corrupt the Oktaverb sub-mode storage.
  bool mVolumReverbRestoreInProgress = false;
  // Same re-entrancy guard for the tremolo per-mode snapshot restore cascade.
  bool mVolumTremoloRestoreInProgress = false;
  // ...and for the chorus per-mode snapshot restore cascade.
  bool mVolumChorusRestoreInProgress = false;
  // Live working store for PRE Pitch per-mode knob memory (PRE has no effect-
  // settings struct like POST, so the live snapshots live here). Synced to/from
  // each amp's prePitchModes via the PRE save/restore-to-slot helpers.
  int mVolumPrePitchMode = volum::kVoLumPitchModeTranspose;
  volum::PitchModeSnapshot mVolumPrePitchModes[volum::kVoLumPitchModeCount];
  // Set true while a PRE/amp restore is applying live params so the kPrePitchMode
  // handler does not re-enter the per-mode save/restore mid-restore.
  bool mVolumPreRestoreInProgress = false;
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

  enum class VoLumLoadKind
  {
    Main,
    MainPrefetch,
    Support,
    Pre
  };
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

  template <typename Pred>
  void _VolumDropQueuedLoadRequests(Pred pred)
  {
    mVolumLoadRequests.erase(
      std::remove_if(mVolumLoadRequests.begin(), mVolumLoadRequests.end(), pred), mVolumLoadRequests.end());
  }

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
  // Live PRE/POST snapshots while a lock is engaged. These mirror the live
  // params but live OUTSIDE the per-amp `mVolumAmpSettings` array so amp slot
  // contents are never silently mutated by lock-driven amp switching.
  volum::VoLumAmpSettings mVolumLiveLockedPre;
  volum::VoLumAmpSettings mVolumLiveLockedPost;

  void _VolumLoadUpdateState();
  void _VolumStartUpdateCheck(bool manual);
  void _VolumConsumeUpdateResult();
  void _VolumRefreshUpdateUi();
  volum::update::UpdateState mVolumUpdateState;
  std::shared_ptr<volum::update::AsyncResult> mVolumUpdateResult;
  bool mVolumUpdateStateLoaded = false;
  bool mVolumUpdateCheckInFlight = false;
  int mVolumUpdateFooterTicks = 0;

  // Tuner & Metronome DSP
  volum::TunerDSP mTunerDSP;
  volum::MetronomeDSP mMetronomeDSP;
  // Loads an IR and stores it to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  // support=true stages into the dual-amp SUPPORT lane's convolver (mSupportIR);
  // false targets the MAIN amp's convolver (mIR). Per-lane custom IR (spec 3.2).
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath, bool support = false);

  // VoLum 1.2.1 per-IR shaping. _VolumApplyIrShaping runs on the audio thread and
  // applies the focused IR's trim + low/high cut to one lane after the convolver.
  // _VolumPushIrShaping copies the active IR's library settings into that lane's
  // atomics (call after an IR becomes active or its panel is edited).
  // _VolumMigrateIrTrims auto-normalizes pre-1.2.1 IRs (no stored trim) once by
  // measuring their .wav energy, then persists - retroactively fixing quiet IRs.
  iplug::sample** _VolumApplyIrShaping(iplug::sample** in, const size_t numChannels, const int nFrames,
                                       const double sampleRate, const bool support);
  void _VolumPushIrShaping(bool support);
  // Main thread: apply a shaping push that a deferred cab switch held back, once
  // that switch has landed and the lane's convolver matches the shaping again.
  void _VolumFlushDeferredIrShaping();
  void _VolumMigrateIrTrims();

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

  // Plugin PDC plus, in the standalone, the audio device's own round trip.
  volum::LatencyReport _VolumLatencyReport() const;

  // Push the latency report to the Settings page when it has changed. Called from
  // OnIdle because iPlug2's standalone host runs OnReset() BEFORE openStream(), so
  // the report taken there can never see the driver's latency (see the definition).
  // `force` re-sends an unchanged report, for when the editor was just rebuilt.
  void _VolumRefreshLatencyReport(bool force = false);

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
  // PRE Pitch pedal engine. Configured off the audio thread in OnReset because
  // Configure() allocates; the audio thread try-locks mPrePitchMutex and passes
  // dry through while a reset/reconfigure is in progress.
  dsp::effect::VoLumPitch mPitch;
  mutable std::mutex mPrePitchMutex;
  dsp::effect::VoLumPreEq mPreEq[2];
  recursive_linear_filter::Level mPreInputGain[2];
  recursive_linear_filter::Level mPreOutputGain[2];
  dsp::effect::Delay mDelay;
  dsp::effect::Reverb mReverb;
  volum::TremoloDSP mTremolo;
  volum::ChorusDSP mChorus;
  // The model actually being used:
  std::unique_ptr<ResamplingNAM> mModel;
  std::unique_ptr<ResamplingNAM> mSupportModel;
  std::unique_ptr<ResamplingNAM> mPreModel[2];
  // And the IR. The MAIN amp and the dual-amp SUPPORT lane each get their own
  // convolver so a custom IR is local to one lane (per-lane custom IR, spec 3.2).
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  std::unique_ptr<dsp::ImpulseResponse> mSupportIR;
  // Manages switching what DSP is being used.
  std::unique_ptr<ResamplingNAM> mStagedModel;
  std::unique_ptr<ResamplingNAM> mStagedSupportModel;
  std::unique_ptr<ResamplingNAM> mStagedPreModel[2];
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR;
  std::unique_ptr<dsp::ImpulseResponse> mStagedSupportIR;
  // Flags to take away the modules at a safe time.
  std::atomic<bool> mShouldRemoveModel = false;
  std::atomic<bool> mShouldRemoveSupportModel = false;
  std::atomic<bool> mShouldRemovePreModel[2]{{false}, {false}};
  std::atomic<bool> mShouldRemoveIR = false;
  std::atomic<bool> mShouldRemoveSupportIR = false;
  // VoLum: whether a SUPPORT amp is selected at all, owned solely by
  // _VolumRequestSupportModelLoad - the one place that decides whether a support
  // model should exist. ProcessBlock cannot answer this from kSupportAmpIdx,
  // because a custom (library) partner has no factory index and parks that param
  // at -1; testing the param alone silenced the entire custom dual-amp lane.
  std::atomic<bool> mVolumSupportSelected = false;
  // VoLum: a cab-source switch changes the lane's capture and its IR together, but
  // the capture loads asynchronously, so whichever side is ready first waits for the
  // other and both land on one block. Dropping the IR early exposes raw, cab-less
  // amp; adding it early stacks it on a baked cab that is still live. Set only when
  // a capture load is actually coming - a pick that reuses the live capture switches
  // at once. The block counters bound each wait (see StepDeferredIrSwap).
  std::atomic<bool> mVolumDeferredRemoveIR = false;
  std::atomic<bool> mVolumDeferredRemoveSupportIR = false;
  std::atomic<int> mVolumDeferredRemoveIrBlocks = 0;
  std::atomic<int> mVolumDeferredRemoveSupportIrBlocks = 0;
  std::atomic<bool> mVolumDeferredApplyIR = false;
  std::atomic<bool> mVolumDeferredApplySupportIR = false;
  std::atomic<int> mVolumDeferredApplyIrBlocks = 0;
  std::atomic<int> mVolumDeferredApplySupportIrBlocks = 0;
  std::atomic<int> mVolumDeferredIrMaxBlocks = 512;
  // Main thread only: a lane whose IR trim and cuts are waiting for its deferred
  // swap to land, in either direction. [0] = MAIN, [1] = SUPPORT.
  bool mVolumIrShapingPushPending[2]{false, false};

  std::atomic<bool> mNewModelLoadedInDSP = false;
  std::atomic<bool> mModelCleared = false;
  bool mPostEffectsClearedForMissingModel = false;
  // Previous-block POST plan flags. Used to detect a true -> false edge for Delay or
  // Reverb and call Reset() on that edge so re-enabling the effect later does not
  // replay stale tails. Reset to false in OnReset.
  bool mPostDelayWasActive = false;
  bool mPostReverbWasActive = false;
  bool mPostTremoloWasActive = false;
  bool mPostChorusWasActive = false;
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

  // Per-IR shaping (VoLum 1.2.1). The focused IR's library settings (trim + low/
  // high cut) are pushed to the atomics below off the audio thread when an IR is
  // selected or its panel is edited; the audio thread reads them lock-free and
  // applies trim + cuts on the IR lane, after the convolver and before the DC
  // blocker. A cut Hz of 0 bypasses that filter. Not a DAW parameter.
  recursive_linear_filter::HighPass mIrLowCut; // MAIN low-cut (high-pass)
  recursive_linear_filter::LowPass mIrHighCut; // MAIN high-cut (low-pass)
  recursive_linear_filter::HighPass mSupportIrLowCut; // SUPPORT low-cut
  recursive_linear_filter::LowPass mSupportIrHighCut; // SUPPORT high-cut
  std::atomic<double> mIrTrimLin{1.0};
  std::atomic<double> mSupportIrTrimLin{1.0};
  std::atomic<double> mIrLowCutHz{0.0};
  std::atomic<double> mIrHighCutHz{0.0};
  std::atomic<double> mSupportIrLowCutHz{0.0};
  std::atomic<double> mSupportIrHighCutHz{0.0};

  dsp::noise_gate::Trigger mSupportNoiseGateTrigger;
  dsp::noise_gate::Gain mSupportNoiseGateGain;
  std::vector<iplug::sample> mDualMainLaneBuffer;
  std::vector<iplug::sample> mDualSupportLaneBuffer;
  std::vector<iplug::sample> mDualMainAlignedBuffer;
  std::vector<iplug::sample> mDualSupportAlignedBuffer;
  volum::DualAmpDelayLine<iplug::sample> mDualMainLatencyDelay;
  volum::DualAmpDelayLine<iplug::sample> mDualSupportLatencyDelay;

  // VoLum: live/staged path pairs commit with staged models/IR in _ApplyDSPStaging (see VoLumDspStagingWdl.h).
  volum::dsp_staging::WdlStagedPathPair mNAMPaths;
  volum::dsp_staging::WdlStagedPathPair mIRPaths;
  volum::dsp_staging::WdlStagedPathPair mSupportIRPaths;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  NAMSender mInputSender, mOutputSender;
  // Right-channel output meter sender (used in dual-amp/stereo mode for the second OUT bar).
  NAMSender mOutputSenderR;
};
