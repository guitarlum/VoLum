// Restore-from-slot, apply-amp-settings, DSP caches, settings-file I/O, lite mode.
// Tail-#included via VoLumSettings.inc.cpp (NOT a separate TU); file-size hygiene.

void NeuralAmpModeler::_VolumRestorePreFromSlot(const volum::VoLumAmpSettings& s)
{
  // Guard so the kPrePitchMode set below cannot re-enter the per-mode save /
  // restore dance and clobber the per-amp values we are loading.
  struct PreRestoreGuard
  {
    bool& flag;
    bool prev;
    explicit PreRestoreGuard(bool& f)
    : flag(f)
    , prev(f)
    {
      flag = true;
    }
    ~PreRestoreGuard() { flag = prev; }
  } preGuard(mVolumPreRestoreInProgress);

  for (int mode = 0; mode < volum::kVoLumPitchModeCount; ++mode)
    mVolumPrePitchModes[mode] = s.prePitchModes[mode];
  mVolumPrePitchMode = std::clamp(s.prePitchMode, 0, volum::kVoLumPitchModeCount - 1);

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };

  setParam(kPreCompActive, s.preCompActive ? 1.0 : 0.0);
  setParam(kPreCompAmount, s.preCompAmount);
  setParam(kPreCompRatio, s.preCompRatio);
  setParam(kPreCompAttack, s.preCompAttack);
  setParam(kPreCompRelease, s.preCompRelease);
  setParam(kPreCompMix, s.preCompMix);
  setParam(kPreCompLevel, s.preCompLevel);
  setParam(kPreNam1Active, s.preNam1Active ? 1.0 : 0.0);
  setParam(kPreNam1Capture, s.preNam1Capture);
  setParam(kPreNam1Gain, s.preNam1Gain);
  setParam(kPreNam1Bass, s.preNam1Bass);
  setParam(kPreNam1Mid, s.preNam1Mid);
  setParam(kPreNam1MidFreq, s.preNam1MidFreq);
  setParam(kPreNam1Treble, s.preNam1Treble);
  setParam(kPreNam1Level, s.preNam1Level);
  setParam(kPreNam2Active, s.preNam2Active ? 1.0 : 0.0);
  setParam(kPreNam2Capture, s.preNam2Capture);
  setParam(kPreNam2Gain, s.preNam2Gain);
  setParam(kPreNam2Bass, s.preNam2Bass);
  setParam(kPreNam2Mid, s.preNam2Mid);
  setParam(kPreNam2MidFreq, s.preNam2MidFreq);
  setParam(kPreNam2Treble, s.preNam2Treble);
  setParam(kPreNam2Level, s.preNam2Level);
  setParam(kPrePitchActive, s.prePitchActive ? 1.0 : 0.0);
  setParam(kPrePitchMode, s.prePitchMode);
  setParam(kPrePitchSemitones, s.prePitchSemitones);
  setParam(kPrePitchMix, s.prePitchMix);
  setParam(kPrePitchOctDown, s.prePitchOctDown);
  setParam(kPrePitchOctUp, s.prePitchOctUp);
  setParam(kPrePitchDry, s.prePitchDry);
  setParam(kPrePitchVoicing, s.prePitchVoicing);
  setParam(kPrePitchLevel, s.prePitchLevel);
  setParam(kPrePitchTransChar, s.prePitchTransChar);

  const bool shouldLoadPreNam1 = volum::ShouldLoadPrePedalCapture(s.preNam1Active, s.preNam1Capture);
  const bool shouldLoadPreNam2 = volum::ShouldLoadPrePedalCapture(s.preNam2Active, s.preNam2Capture);
  mVolumPreNeedsLoad[0].store(shouldLoadPreNam1);
  mVolumPreNeedsLoad[1].store(shouldLoadPreNam2);
  mShouldRemovePreModel[0].store(!shouldLoadPreNam1);
  mShouldRemovePreModel[1].store(!shouldLoadPreNam2);

  // Recall the current pitch mode's shared knobs from the just-loaded snapshot
  // (mirrors the tremolo per-mode restore in _VolumRestorePostFromSlot).
  _VolumRestorePrePitchModeSnapshot(mVolumPrePitchMode);
}

void NeuralAmpModeler::_VolumRestorePostFromSlot(volum::VoLumAmpSettings& s)
{
  if (!s.postValid)
  {
    const volum::VoLumAmpSettings defaults;
    s.postValid = true;
    s.postDelayActive = defaults.postDelayActive;
    s.postDelayTime = defaults.postDelayTime;
    s.postDelayFeedback = defaults.postDelayFeedback;
    s.postDelayMix = defaults.postDelayMix;
    s.postDelayMode = defaults.postDelayMode;
    s.postDelayTone = defaults.postDelayTone;
    s.postDelayAge = defaults.postDelayAge;
    s.postDelayPingPong = defaults.postDelayPingPong;
    s.postDelaySync = defaults.postDelaySync;
    s.postDelayDivision = defaults.postDelayDivision;
    s.postReverbActive = defaults.postReverbActive;
    s.postReverbMix = defaults.postReverbMix;
    s.postReverbDecay = defaults.postReverbDecay;
    s.postReverbTone = defaults.postReverbTone;
    s.postReverbPreDelay = defaults.postReverbPreDelay;
    s.postReverbShimmer = defaults.postReverbShimmer;
    s.postReverbMode = defaults.postReverbMode;
    s.postReverbSubMode = defaults.postReverbSubMode;
    s.postTremoloActive = defaults.postTremoloActive;
    s.postTremoloMode = defaults.postTremoloMode;
    s.postTremoloRate = defaults.postTremoloRate;
    s.postTremoloDepth = defaults.postTremoloDepth;
    s.postTremoloShape = defaults.postTremoloShape;
    s.postTremoloMix = defaults.postTremoloMix;
    s.postTremoloCrossover = defaults.postTremoloCrossover;
    s.postTremoloSync = defaults.postTremoloSync;
    s.postTremoloDivision = defaults.postTremoloDivision;
    s.postChorusActive = defaults.postChorusActive;
    s.postChorusMode = defaults.postChorusMode;
    s.postChorusRate = defaults.postChorusRate;
    s.postChorusDepth = defaults.postChorusDepth;
    s.postChorusTone = defaults.postChorusTone;
    s.postChorusWidth = defaults.postChorusWidth;
    s.postChorusMix = defaults.postChorusMix;
    for (int mode = 0; mode < volum::kVoLumChorusModeCount; ++mode)
      s.postChorusModes[mode] = defaults.postChorusModes[mode];
    for (int mode = 0; mode < volum::kVoLumDelayModeCount; ++mode)
      s.postDelayModes[mode] = defaults.postDelayModes[mode];
    for (int mode = 0; mode < volum::kVoLumReverbModeCount; ++mode)
      s.postReverbModes[mode] = defaults.postReverbModes[mode];
    for (int subMode = 0; subMode < 3; ++subMode)
      s.postOktaverbSubModes[subMode] = defaults.postOktaverbSubModes[subMode];
    for (int mode = 0; mode < volum::kVoLumTremoloModeCount; ++mode)
      s.postTremoloModes[mode] = defaults.postTremoloModes[mode];
  }

  for (int mode = 0; mode < volum::kVoLumDelayModeCount; ++mode)
    mVolumEffectSettings.delayModes[mode] = s.postDelayModes[mode];
  for (int mode = 0; mode < volum::kVoLumReverbModeCount; ++mode)
    mVolumEffectSettings.reverbModes[mode] = s.postReverbModes[mode];
  for (int subMode = 0; subMode < 3; ++subMode)
    mVolumEffectSettings.oktaverbSubModes[subMode] = s.postOktaverbSubModes[subMode];
  for (int mode = 0; mode < volum::kVoLumTremoloModeCount; ++mode)
    mVolumEffectSettings.tremoloModes[mode] = s.postTremoloModes[mode];
  for (int mode = 0; mode < volum::kVoLumChorusModeCount; ++mode)
    mVolumEffectSettings.chorusModes[mode] = s.postChorusModes[mode];

  struct PostRestoreGuard
  {
    bool& flag;
    bool prev;
    explicit PostRestoreGuard(bool& f)
    : flag(f)
    , prev(f)
    {
      flag = true;
    }
    ~PostRestoreGuard() { flag = prev; }
  } postGuard(mVolumPostRestoreInProgress);

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };

  setParam(kDelayActive, s.postDelayActive ? 1.0 : 0.0);
  setParam(kDelayTime, s.postDelayTime);
  setParam(kDelayFeedback, s.postDelayFeedback);
  setParam(kDelayMix, s.postDelayMix);
  setParam(kDelayMode, s.postDelayMode);
  setParam(kDelayTone, s.postDelayTone);
  setParam(kDelayAge, s.postDelayAge);
  setParam(kDelayPingPong, s.postDelayPingPong ? 1.0 : 0.0);
  setParam(kDelaySync, s.postDelaySync ? 1.0 : 0.0);
  setParam(kDelayDivision, s.postDelayDivision);
  setParam(kReverbActive, s.postReverbActive ? 1.0 : 0.0);
  setParam(kReverbMix, s.postReverbMix);
  setParam(kReverbDecay, s.postReverbDecay);
  setParam(kReverbTone, s.postReverbTone);
  setParam(kReverbPreDelay, s.postReverbPreDelay);
  setParam(kReverbShimmer, s.postReverbShimmer);
  setParam(kReverbMode, s.postReverbMode);
  setParam(kReverbSubMode, s.postReverbSubMode);
  setParam(kTremoloActive, s.postTremoloActive ? 1.0 : 0.0);
  setParam(kTremoloMode, s.postTremoloMode);
  setParam(kTremoloRate, s.postTremoloRate);
  setParam(kTremoloDepth, s.postTremoloDepth);
  setParam(kTremoloShape, s.postTremoloShape);
  setParam(kTremoloMix, s.postTremoloMix);
  setParam(kTremoloCrossover, s.postTremoloCrossover);
  setParam(kTremoloSync, s.postTremoloSync ? 1.0 : 0.0);
  setParam(kTremoloDivision, s.postTremoloDivision);
  setParam(kChorusActive, s.postChorusActive ? 1.0 : 0.0);
  setParam(kChorusMode, s.postChorusMode);
  setParam(kChorusRate, s.postChorusRate);
  setParam(kChorusDepth, s.postChorusDepth);
  setParam(kChorusTone, s.postChorusTone);
  setParam(kChorusWidth, s.postChorusWidth);
  setParam(kChorusMix, s.postChorusMix);
  mVolumEffectSettings.delayActive = s.postDelayActive;
  mVolumEffectSettings.delayMode = s.postDelayMode;
  mVolumEffectSettings.reverbActive = s.postReverbActive;
  mVolumEffectSettings.reverbMode = s.postReverbMode;
  mVolumEffectSettings.tremoloMode = s.postTremoloMode;
  mVolumEffectSettings.chorusMode = s.postChorusMode;
  const int restoredDelayMode = std::clamp(s.postDelayMode, 0, volum::kVoLumDelayModeCount - 1);
  const int restoredReverbMode = std::clamp(s.postReverbMode, 0, volum::kVoLumReverbModeCount - 1);
  const int restoredTremoloMode = std::clamp(s.postTremoloMode, 0, volum::kVoLumTremoloModeCount - 1);
  const int restoredChorusMode = std::clamp(s.postChorusMode, 0, volum::kVoLumChorusModeCount - 1);
  _VolumSaveDelayModeSnapshot(restoredDelayMode);
  _VolumSaveReverbModeSnapshot(restoredReverbMode);
  _VolumSaveTremoloModeSnapshot(restoredTremoloMode);
  _VolumSaveChorusModeSnapshot(restoredChorusMode);
  _VolumRestoreDelayModeSnapshot(restoredDelayMode);
  _VolumRestoreReverbModeSnapshot(restoredReverbMode);
  _VolumRestoreTremoloModeSnapshot(restoredTremoloMode);
  _VolumRestoreChorusModeSnapshot(restoredChorusMode);
}

void NeuralAmpModeler::_VolumRestoreFromSettings(int ampIdx)
{
  _VolumApplyAmpSettings(mVolumAmpSettings[ampIdx]);
}

// Apply an arbitrary settings snapshot to the live params/members. Factory amps
// pass mVolumAmpSettings[ampIdx]; custom amps (F6) and preset recall (F5) pass a
// scene/preset struct that lives outside the factory array.
void NeuralAmpModeler::_VolumApplyAmpSettings(volum::VoLumAmpSettings& s)
{
  mVolumSpeakerIdx = s.speakerIdx;
  mVolumChannelIdx = s.channelIdx;

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };

  setParam(kInputLevel, s.inputLevel);
  setParam(kNoiseGateThreshold, s.gateThreshold);
  setParam(kToneBass, s.toneBass);
  setParam(kToneMid, s.toneMid);
  setParam(kToneTreble, s.toneTreble);
  setParam(kOutputLevel, s.outputLevel);
  setParam(kNoiseGateActive, s.noiseGateActive ? 1.0 : 0.0);
  setParam(kEQActive, s.eqActive ? 1.0 : 0.0);
  if (!mVolumPreLocked)
    _VolumRestorePreFromSlot(s);
  setParam(kDualAmpActive, s.dualAmpActive ? 1.0 : 0.0);
  setParam(kDualAmpRoute, s.dualAmpRoute);
  setParam(kMainAmpPan, s.mainAmpPan);
  setParam(kSupportAmpIdx, s.supportAmpIdx);
  setParam(kSupportSpeakerIdx, s.supportSpeakerIdx);
  setParam(kSupportChannelIdx, s.supportChannelIdx);
  setParam(kSupportInputLevel, s.supportInputLevel);
  setParam(kSupportNoiseGateThreshold, s.supportGateThreshold);
  setParam(kSupportToneBass, s.supportToneBass);
  setParam(kSupportToneMid, s.supportToneMid);
  setParam(kSupportToneTreble, s.supportToneTreble);
  setParam(kSupportOutputLevel, s.supportOutputLevel);
  setParam(kSupportNoiseGateActive, s.supportNoiseGateActive ? 1.0 : 0.0);
  setParam(kSupportEQActive, s.supportEqActive ? 1.0 : 0.0);
  setParam(kSupportAmpPan, s.supportAmpPan);
  mSupportPolarityInvert.store(s.supportPolarityInvert);

  // Migrate degenerate dual rigs (both lanes centered + support polarity inverted)
  // that phase-cancel to near silence: split them hard L/R like the dual toggle
  // would. setParam bypasses OnParamChange, so do it explicitly here on restore /
  // preset recall too. (Heals "custom amp makes no sound in dual mode".)
  if (volum::DegenerateDualNeedsPanHeal(s))
  {
    setParam(kMainAmpPan, -1.0);
    setParam(kSupportAmpPan, 1.0);
  }

  // F6 dual amp: resolve a custom SUPPORT partner by stable id. An orphaned id
  // (the amp was deleted / is missing on this machine) falls back to "(none)"
  // per the removal matrix; clearing it on the scene heals the ref on next save.
  if (!s.supportCustomId.empty())
  {
    const int sidx = volum::custom::CustomAmpIndexById(s.supportCustomId);
    mVolumCustomSupportIdx = sidx;
    if (sidx >= 0)
    {
      const auto amp = volum::custom::CustomAmpAt(sidx);
      int sl = volum::custom::kDirectSlot, ch = 1;
      // Prefer the saved custom support cab/channel when it still resolves on
      // this amp; only fall back to the default capture for legacy scenes (no
      // saved selection) or an orphaned slot/channel.
      bool restored = false;
      if (s.supportCustomChannel >= 1 && volum::custom::SlotAssigned(s.supportCustomSlot))
      {
        const auto chs = volum::custom::AmpSlotChannels(amp, s.supportCustomSlot);
        if (std::find(chs.begin(), chs.end(), s.supportCustomChannel) != chs.end())
        {
          sl = s.supportCustomSlot;
          ch = s.supportCustomChannel;
          restored = true;
        }
      }
      if (!restored)
        volum::content::DefaultCaptureSelection(amp, sl, ch);
      mVolumCustomSupportSlot = sl;
      mVolumCustomSupportChannel = ch;
      // Keep the scene in lock-step so the next save/equality check matches.
      s.supportCustomSlot = sl;
      s.supportCustomChannel = ch;
    }
    else
    {
      s.supportCustomId.clear();
    }
  }
  else
  {
    mVolumCustomSupportIdx = -1;
  }

  _VolumRefreshSupportChannels();
  mVolumSupportNeedsLoad.store(true);

  if (!mVolumPostLocked)
    _VolumRestorePostFromSlot(s);

  // Deliberately no SetSelected here. This is a restore path, and mVolumSpeakerIdx
  // is a raw persisted index: for a custom lane the resolver still has to snap it to
  // a slot the channel actually carries, and when SUPPORT is focused it belongs to
  // the other lane entirely - the row is shared. Every caller of this function ends
  // in _VolumApplyFocusedLaneCabs, which writes names, enables, IR chip and
  // selection together for the lane that is actually on screen.
  if (auto* pGfx = GetUI())
    _UpdateVoLumLayout(pGfx);

  // F7: re-resolve each lane's custom IR cab (orphaned id -> baked cab fallback).
  // The SUPPORT lane owns its own convolver, so restore it independently.
  _VolumApplyActiveIr(s.activeIrId, false);
  _VolumApplyActiveIr(s.supportActiveIrId, true);

  // setParam above bypasses OnParamChange, so the cached DSP gains and tone-stack
  // coefficients would stay stale (e.g. OUTPUT recalled from -inf shows 0 dB but
  // stays silent until a manual knob nudge). Re-apply them explicitly.
  _VolumApplyDspCaches();
}

// Re-apply every DSP value cached in a plugin member that OnParamChange would
// normally refresh. Mirrors the cached cases in OnParamChange and the locked set
// in volum::dsp_cache::kRestoreReappliedCaches. Cheap, non-audio-thread.
void NeuralAmpModeler::_VolumApplyDspCaches()
{
  _SetInputGain();
  _SetOutputGain();
  _SetSupportOutputGain();
  if (mToneStack)
  {
    mToneStack->SetParam("bass", GetParam(kToneBass)->Value());
    mToneStack->SetParam("middle", GetParam(kToneMid)->Value());
    mToneStack->SetParam("treble", GetParam(kToneTreble)->Value());
  }
  if (mSupportToneStack)
  {
    mSupportToneStack->SetParam("bass", GetParam(kSupportToneBass)->Value());
    mSupportToneStack->SetParam("middle", GetParam(kSupportToneMid)->Value());
    mSupportToneStack->SetParam("treble", GetParam(kSupportToneTreble)->Value());
  }
}

void NeuralAmpModeler::_VolumSaveSettingsToFile()
{
  _VolumSaveEffectSettings();
  // Keep the shared legacy file readable by already-installed older VoLum builds. New dual-amp
  // fields live in a sidecar that older builds do not know about, avoiding crashes when users
  // run a newer standalone and then open an older VST3 in a DAW.
  nlohmann::json j = volum::VolumUserSettingsToJson(
    mVolumAmpSettings.data(), volum::kAmpCount, mVolumAmpIdx, &mVolumEffectSettings,
    /*includeDualAmp=*/false, mVolumPreLocked, mVolumPostLocked, mVolumPreLocked ? &mVolumLiveLockedPre : nullptr,
    mVolumPostLocked ? &mVolumLiveLockedPost : nullptr, mVolumLiteMode.load(), GetParam(kCalibrateInput)->Bool(),
    GetParam(kInputCalibrationLevel)->Value());
  nlohmann::json dualAmpJson = volum::VolumDualAmpUserSettingsToJson(mVolumAmpSettings.data(), volum::kAmpCount);

  // 1.2.0 additive session refs (ignored by older builds): the focused custom
  // MAIN amp + active preset so the next launch re-selects them. Per-amp IR /
  // custom-support refs already round-trip inside each scene's JSON.
  j["volumCustomMainId"] = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
  j["volumActivePresetId"] = mVolumActivePresetId;
  // Standalone has no DAW project chunk; persist the same per-instance MIDI
  // channel field in its instance settings equivalent.
  j["midiCh"] = mVolumMidiChannel.load();
  j["volumUiMode"] = volum::UiModeToString(mVolumUiMode);
  // 1.2.1: the same selection for every amp, not only the focused one. The single
  // key above describes whichever amp was in focus when the file was written, so
  // every other amp reopened reading "No Preset" - and an exit from an amp with
  // nothing recalled wrote an empty id, losing the focused one too. The live pair is
  // folded in rather than relying on the map alone, so a save that lands between a
  // recall and the next amp switch still records it.
  {
    auto activePresetIdsByOwner = mVolumActivePresetIdByOwner;
    if (mVolumHasRecalledSnapshot && !mVolumActivePresetId.empty())
      activePresetIdsByOwner[_VolumActiveOwnerKey()] = mVolumActivePresetId;
    j["volumActivePresetIdByOwner"] = volum::VolumActivePresetIdsToJson(activePresetIdsByOwner);
  }
  // 1.3.0: custom-amp live scenes. Machine-global here for the same reason the
  // per-factory-amp scenes are: the standalone window IS the machine. Plugins get
  // theirs from the DAW chunk and never write this file (see the guard in
  // _VolumSaveSettingsToFile's callers), so a project cannot move the standalone's
  // knobs and a catalog write cannot move anybody's.
  if (!mVolumCustomScenes.empty())
    j["volumCustomScenes"] = volum::CustomScenesToJson(mVolumCustomScenes);

  namespace fs = std::filesystem;
  fs::path settingsPath = volum::VolumUserSettingsFilePath();
  fs::path dualAmpSettingsPath = volum::VolumDualAmpSettingsFilePath();
  if (settingsPath.empty())
  {
    if (mVolumRigsRoot.empty())
      return;
    settingsPath = volum::content::PathFromUtf8(mVolumRigsRoot) / "volum-settings.json";
    dualAmpSettingsPath = volum::content::PathFromUtf8(mVolumRigsRoot) / "volum-dual-amp-settings.json";
  }

  std::error_code ec;
  if (!volum::WriteJsonAtomically(settingsPath, j, ec))
  {
    std::cerr << "VoLum: write failed for settings file: " << settingsPath.string() << " (" << ec.message() << ")"
              << std::endl;
    return;
  }

  if (!volum::WriteJsonAtomically(dualAmpSettingsPath, dualAmpJson, ec))
  {
    std::cerr << "VoLum: write failed for dual-amp settings file: " << dualAmpSettingsPath.string() << " ("
              << ec.message() << ")" << std::endl;
    return;
  }

  // Persist the shared content library too. Since 1.3.0 the scenes travel above,
  // in this file - the library only needs flushing for catalog edits, and Save()
  // is a locked read-modify-write merge, so doing it here cannot drop a sibling's
  // items. No-op when no base dir is set.
  volum::content::GlobalContentStore().Save();
}

void NeuralAmpModeler::_VolumSaveCalibrationDefaults()
{
  namespace fs = std::filesystem;
  const fs::path settingsPath = volum::VolumUserSettingsFilePath();
  if (settingsPath.empty())
    return;

  // Multiple plugin instances in one host may edit this machine-global default.
  // Only direct UI edits call this function; project/preset restores never do.
  static std::mutex calibrationSettingsMutex;
  std::lock_guard<std::mutex> lock(calibrationSettingsMutex);

  nlohmann::json j = nlohmann::json::object();
  std::error_code ec;
  if (fs::exists(settingsPath, ec))
  {
    try
    {
      std::ifstream in(settingsPath);
      in >> j;
      if (!j.is_object())
        return;
    }
    catch (...)
    {
      std::cerr << "VoLum: calibration defaults not saved because volum-settings.json is unreadable" << std::endl;
      return;
    }
  }

  if (!j.contains("version"))
    j["version"] = volum::kVoLumUserSettingsVersion;
  j["CalibrateInput"] = GetParam(kCalibrateInput)->Bool();
  j["InputCalibrationLevel"] = GetParam(kInputCalibrationLevel)->Value();
  if (!volum::WriteJsonAtomically(settingsPath, j, ec))
    std::cerr << "VoLum: calibration defaults write failed: " << ec.message() << std::endl;
}

void NeuralAmpModeler::_VolumLoadSettingsFromFile()
{
  namespace fs = std::filesystem;
  const fs::path userPath = volum::VolumUserSettingsFilePath();
  const fs::path dualAmpUserPath = volum::VolumDualAmpSettingsFilePath();
  fs::path legacyPath;
  fs::path dualAmpLegacyPath;
  if (!mVolumRigsRoot.empty())
  {
    legacyPath = volum::content::PathFromUtf8(mVolumRigsRoot) / "volum-settings.json";
    dualAmpLegacyPath = volum::content::PathFromUtf8(mVolumRigsRoot) / "volum-dual-amp-settings.json";
  }

  fs::path settingsPath;
  if (!userPath.empty() && fs::exists(userPath))
    settingsPath = userPath;
  else if (!legacyPath.empty() && fs::exists(legacyPath))
    settingsPath = legacyPath;
  else
    return;

  try
  {
    std::ifstream in(settingsPath);
    nlohmann::json j;
    in >> j;

    bool settingsHealed = false;
    bool haveLivePreSnapshot = false;
    bool haveLivePostSnapshot = false;
    volum::VoLumAmpSettings parsedLivePre;
    volum::VoLumAmpSettings parsedLivePost;
    bool parsedLiteMode = false;
    bool parsedCalibrateInput = kDefaultCalibrateInput;
    double parsedInputCalibrationLevel = kDefaultInputCalibrationLevel;
    volum::VolumUserSettingsFromJson(j, mVolumAmpSettings.data(), volum::kAmpCount, &mVolumAmpIdx,
                                     &mVolumEffectSettings, &settingsHealed, &mVolumPreLocked, &mVolumPostLocked,
                                     &parsedLivePre, &parsedLivePost, &haveLivePreSnapshot, &haveLivePostSnapshot,
                                     &parsedLiteMode, &parsedCalibrateInput, &parsedInputCalibrationLevel);
    mVolumLiteMode.store(parsedLiteMode);
    GetParam(kCalibrateInput)->Set(parsedCalibrateInput ? 1.0 : 0.0);
    GetParam(kInputCalibrationLevel)->Set(parsedInputCalibrationLevel);
    if (haveLivePreSnapshot)
      mVolumLiveLockedPre = parsedLivePre;
    if (haveLivePostSnapshot)
      mVolumLiveLockedPost = parsedLivePost;
    // 1.2.0 session refs: stash the focused custom MAIN amp + active preset for
    // re-selection once the UI opens (see OnUIOpen). Absent on older files.
    if (j.contains("volumCustomMainId") && j["volumCustomMainId"].is_string())
      mVolumRestoreCustomMainId = j["volumCustomMainId"].get<std::string>();
#if defined(APP_API)
    mVolumUiMode = volum::UiModeFromMachineSettings(true, j, mVolumUiMode);
#endif
    if (j.contains("volumActivePresetId") && j["volumActivePresetId"].is_string())
      mVolumRestorePresetId = j["volumActivePresetId"].get<std::string>();
    if (j.contains("midiCh") && j["midiCh"].is_number_integer())
      mVolumMidiChannel.store(std::clamp(j["midiCh"].get<int>(), 0, 16));
    // 1.2.1 per-amp selections. Absent in files written by 1.2.0, in which case the
    // single id above still restores the amp that was focused, exactly as before.
    if (j.contains("volumActivePresetIdByOwner"))
      mVolumActivePresetIdByOwner = volum::VolumActivePresetIdsFromJson(j["volumActivePresetIdByOwner"]);
    // 1.3.0 custom-amp scenes. A new plugin insert reads them here too, which is
    // how "a new insert inherits initial scenes from the machine settings file"
    // stays true for custom amps now that they behave like factory amps; a project
    // chunk then overrides whatever it carries.
    if (j.contains("volumCustomScenes"))
      mVolumCustomScenes = volum::CustomScenesFromJson(j["volumCustomScenes"]);
    if (volum::HasDualAmpUserSettings(j))
      settingsHealed = true; // Rewrite shared settings without new-only dual-amp fields.

    fs::path dualAmpSettingsPath;
    if (!dualAmpUserPath.empty() && fs::exists(dualAmpUserPath))
      dualAmpSettingsPath = dualAmpUserPath;
    else if (!dualAmpLegacyPath.empty() && fs::exists(dualAmpLegacyPath))
      dualAmpSettingsPath = dualAmpLegacyPath;

    if (!dualAmpSettingsPath.empty())
    {
      std::ifstream dualIn(dualAmpSettingsPath);
      nlohmann::json dualAmpJson;
      dualIn >> dualAmpJson;
      bool dualAmpSettingsHealed = false;
      volum::VolumUserSettingsFromJson(
        dualAmpJson, mVolumAmpSettings.data(), volum::kAmpCount, nullptr, nullptr, &dualAmpSettingsHealed);
      settingsHealed = settingsHealed || dualAmpSettingsHealed;
    }

    if (settingsHealed)
      mVolumSettingsDirty = true;
    // Global effect defaults must not clobber POST params when a lock snapshot will
    // restore the carried scene immediately after _VolumRestoreFromSettings().
    if (!mVolumPostLocked)
      _VolumRestoreEffectSettings();
  }
  catch (...)
  {
    std::cerr << "Failed to read volum-settings.json" << std::endl;
  }
}

void NeuralAmpModeler::_VolumSetLiteMode(bool lite)
{
  if (mVolumLiteMode.load() == lite)
    return;
  mVolumLiteMode.store(lite);
  // Persist the machine-global choice immediately (JSON, not the plugin chunk).
  _VolumSaveSettingsToFile();
  // Re-apply the new slice to every lane by requesting a reload through the
  // proven async staging path; the loader picks up mVolumLiteMode and calls
  // SetSlimmableSize before Reset. Non-slimmable lanes simply reload unchanged.
  // The main lane normally skips a same-path reload, so force it here.
  mVolumForceMainReload.store(true);
  mVolumNeedsLoad.store(true);
  mVolumSupportNeedsLoad.store(true);
  mVolumPreNeedsLoad[0].store(true);
  mVolumPreNeedsLoad[1].store(true);
}

// ---------------------------------------------------------------------------
// F5 presets: per-amp named snapshots in the content registry, owner-keyed by
// the focused amp (factory:<idx> or a custom amp id). Save/overwrite capture the
// live scene; recall applies a stored snapshot and retains it so the header bar
// "(unsaved)" flag is a live-vs-snapshot equality test (returning to the preset
// exactly clears the flag).
// ---------------------------------------------------------------------------

std::string NeuralAmpModeler::_VolumActiveOwnerKey() const
{
  if (mVolumCustomMainIdx >= 0)
  {
    const std::string id = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
    if (!id.empty())
      return id;
  }
  return volum::content::FactoryOwnerKey(mVolumAmpIdx);
}
