// VoLum settings persistence + per-amp / per-mode snapshot helpers.
//
// Tail-included from NeuralAmpModeler.cpp (NOT a separate TU); pure file-size
// hygiene split. Contains save/restore of mVolumAmpSettings, the delay/reverb
// mode snapshots, and the legacy + dual-amp JSON settings file I/O.

void NeuralAmpModeler::_VolumSaveCurrentToSettings()
{
  auto& s = mVolumAmpSettings[mVolumAmpIdx];
  s.speakerIdx = mVolumSpeakerIdx;
  s.channelIdx = mVolumChannelIdx;
  s.inputLevel = GetParam(kInputLevel)->Value();
  s.gateThreshold = GetParam(kNoiseGateThreshold)->Value();
  s.toneBass = GetParam(kToneBass)->Value();
  s.toneMid = GetParam(kToneMid)->Value();
  s.toneTreble = GetParam(kToneTreble)->Value();
  s.outputLevel = GetParam(kOutputLevel)->Value();
  s.noiseGateActive = GetParam(kNoiseGateActive)->Bool();
  s.eqActive = GetParam(kEQActive)->Bool();
  s.preCompActive = GetParam(kPreCompActive)->Bool();
  s.preCompAmount = GetParam(kPreCompAmount)->Value();
  s.preCompRatio = GetParam(kPreCompRatio)->Value();
  s.preCompAttack = GetParam(kPreCompAttack)->Value();
  s.preCompRelease = GetParam(kPreCompRelease)->Value();
  s.preCompMix = GetParam(kPreCompMix)->Value();
  s.preCompLevel = GetParam(kPreCompLevel)->Value();
  s.preNam1Active = GetParam(kPreNam1Active)->Bool();
  s.preNam1Capture = GetParam(kPreNam1Capture)->Int();
  s.preNam1Gain = GetParam(kPreNam1Gain)->Value();
  s.preNam1Bass = GetParam(kPreNam1Bass)->Value();
  s.preNam1Mid = GetParam(kPreNam1Mid)->Value();
  s.preNam1MidFreq = GetParam(kPreNam1MidFreq)->Value();
  s.preNam1Treble = GetParam(kPreNam1Treble)->Value();
  s.preNam1Level = GetParam(kPreNam1Level)->Value();
  s.preNam2Active = GetParam(kPreNam2Active)->Bool();
  s.preNam2Capture = GetParam(kPreNam2Capture)->Int();
  s.preNam2Gain = GetParam(kPreNam2Gain)->Value();
  s.preNam2Bass = GetParam(kPreNam2Bass)->Value();
  s.preNam2Mid = GetParam(kPreNam2Mid)->Value();
  s.preNam2MidFreq = GetParam(kPreNam2MidFreq)->Value();
  s.preNam2Treble = GetParam(kPreNam2Treble)->Value();
  s.preNam2Level = GetParam(kPreNam2Level)->Value();
  s.dualAmpActive = GetParam(kDualAmpActive)->Bool();
  s.dualAmpRoute = GetParam(kDualAmpRoute)->Int();
  s.mainAmpPan = GetParam(kMainAmpPan)->Value();
  s.supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
  s.supportSpeakerIdx = GetParam(kSupportSpeakerIdx)->Int();
  s.supportChannelIdx = GetParam(kSupportChannelIdx)->Int();
  s.supportInputLevel = GetParam(kSupportInputLevel)->Value();
  s.supportGateThreshold = GetParam(kSupportNoiseGateThreshold)->Value();
  s.supportToneBass = GetParam(kSupportToneBass)->Value();
  s.supportToneMid = GetParam(kSupportToneMid)->Value();
  s.supportToneTreble = GetParam(kSupportToneTreble)->Value();
  s.supportOutputLevel = GetParam(kSupportOutputLevel)->Value();
  s.supportNoiseGateActive = GetParam(kSupportNoiseGateActive)->Bool();
  s.supportEqActive = GetParam(kSupportEQActive)->Bool();
  s.supportAmpPan = GetParam(kSupportAmpPan)->Value();
  s.supportPolarityInvert = mSupportPolarityInvert.load();

  mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
  mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
  mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
  mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  _VolumSaveReverbModeSnapshot(std::clamp(mVolumEffectSettings.reverbMode, 0, volum::kVoLumReverbModeCount - 1));

  // POST per-amp persistence: mirror the live POST EParam values into the current
  // amp's slot so switching amps preserves both visible values and hidden mode snapshots.
  s.postValid = true;
  s.postDelayActive = GetParam(kDelayActive)->Bool();
  s.postDelayTime = GetParam(kDelayTime)->Value();
  s.postDelayFeedback = GetParam(kDelayFeedback)->Value();
  s.postDelayMix = GetParam(kDelayMix)->Value();
  s.postDelayMode = GetParam(kDelayMode)->Int();
  s.postDelayTone = GetParam(kDelayTone)->Value();
  s.postDelayAge = GetParam(kDelayAge)->Value();
  s.postDelayPingPong = GetParam(kDelayPingPong)->Bool();
  s.postReverbActive = GetParam(kReverbActive)->Bool();
  s.postReverbMix = GetParam(kReverbMix)->Value();
  s.postReverbDecay = GetParam(kReverbDecay)->Value();
  s.postReverbTone = GetParam(kReverbTone)->Value();
  s.postReverbPreDelay = GetParam(kReverbPreDelay)->Value();
  s.postReverbShimmer = GetParam(kReverbShimmer)->Value();
  s.postReverbMode = GetParam(kReverbMode)->Int();
  s.postReverbSubMode = GetParam(kReverbSubMode)->Int();
  for (int mode = 0; mode < volum::kVoLumDelayModeCount; ++mode)
    s.postDelayModes[mode] = mVolumEffectSettings.delayModes[mode];
  for (int mode = 0; mode < volum::kVoLumReverbModeCount; ++mode)
    s.postReverbModes[mode] = mVolumEffectSettings.reverbModes[mode];
  for (int subMode = 0; subMode < 3; ++subMode)
    s.postOktaverbSubModes[subMode] = mVolumEffectSettings.oktaverbSubModes[subMode];
}

void NeuralAmpModeler::_VolumSaveEffectSettings()
{
  mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
  mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
  mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
  mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  _VolumSaveReverbModeSnapshot(std::clamp(mVolumEffectSettings.reverbMode, 0, volum::kVoLumReverbModeCount - 1));
}

void NeuralAmpModeler::_VolumRestoreEffectSettings()
{
  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  const auto& fx = mVolumEffectSettings;
  setParam(kDelayActive, fx.delayActive ? 1.0 : 0.0);
  setParam(kDelayMode, fx.delayMode);
  _VolumRestoreDelayModeSnapshot(std::clamp(fx.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  setParam(kReverbActive, fx.reverbActive ? 1.0 : 0.0);
  setParam(kReverbMode, fx.reverbMode);
  _VolumRestoreReverbModeSnapshot(std::clamp(fx.reverbMode, 0, volum::kVoLumReverbModeCount - 1));
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumSaveDelayModeSnapshot(int mode)
{
  auto& s = mVolumEffectSettings.delayModes[std::clamp(mode, 0, volum::kVoLumDelayModeCount - 1)];
  s.time = GetParam(kDelayTime)->Value();
  s.feedback = GetParam(kDelayFeedback)->Value();
  s.mix = GetParam(kDelayMix)->Value();
  s.tone = GetParam(kDelayTone)->Value();
  s.age = GetParam(kDelayAge)->Value();
  s.pingPong = GetParam(kDelayPingPong)->Bool();
}

void NeuralAmpModeler::_VolumRestoreDelayModeSnapshot(int mode)
{
  // Per-knob double-click "reset to default" should land on the design-guide value for the
  // CURRENT mode (e.g. Analog.age=0.5, Reverse Bloom=0.0), not the static InitDouble default.
  // We update each delay knob's mDefault to the per-mode design value here, which is also
  // a natural place since it runs on every mode switch and on initial settings restore.
  // Note: SetDefault() also overwrites mValue with the new default — we therefore call
  // SetDefault() FIRST and then apply the user's saved snapshot value via Set(). The final
  // SendParameterValueFromDelegate carries the saved value through to the UI.
  const volum::VoLumEffectSettings designDefaults;
  const int clampedMode = std::clamp(mode, 0, volum::kVoLumDelayModeCount - 1);
  const auto& d = designDefaults.delayModes[clampedMode];
  GetParam(kDelayTime)->SetDefault(d.time);
  GetParam(kDelayFeedback)->SetDefault(d.feedback);
  GetParam(kDelayMix)->SetDefault(d.mix);
  GetParam(kDelayTone)->SetDefault(d.tone);
  GetParam(kDelayAge)->SetDefault(d.age);
  GetParam(kDelayPingPong)->SetDefault(d.pingPong ? 1.0 : 0.0);

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  const auto& s = mVolumEffectSettings.delayModes[clampedMode];
  setParam(kDelayTime, s.time);
  setParam(kDelayFeedback, s.feedback);
  setParam(kDelayMix, s.mix);
  setParam(kDelayTone, s.tone);
  setParam(kDelayAge, s.age);
  setParam(kDelayPingPong, s.pingPong ? 1.0 : 0.0);
}

void NeuralAmpModeler::_VolumSaveReverbModeSnapshot(int mode)
{
  auto& s = mVolumEffectSettings.reverbModes[std::clamp(mode, 0, volum::kVoLumReverbModeCount - 1)];
  s.mix = GetParam(kReverbMix)->Value();
  s.decay = GetParam(kReverbDecay)->Value();
  s.tone = GetParam(kReverbTone)->Value();
  s.preDelay = GetParam(kReverbPreDelay)->Value();
  s.shimmer = GetParam(kReverbShimmer)->Value();
  s.subMode = GetParam(kReverbSubMode)->Int();
  if (mode == volum::kVoLumReverbModeOktaverb)
    _VolumSaveOktaverbSubModeSnapshot(s.subMode);
}

void NeuralAmpModeler::_VolumRestoreReverbModeSnapshot(int mode)
{
  // Guard so the setParam cascade below cannot re-enter our own snapshot save / restore
  // logic. Without this, switching reverb modes triggers OnParamChangeUI for kReverbMix
  // / kReverbSubMode etc. mid-restore, which writes the partially-restored state back
  // into the snapshot we are loading from. RAII saves and restores the previous value
  // so nested calls (this -> _VolumRestoreOktaverbSubModeSnapshot) keep the guard set.
  struct RestoreGuard {
    bool& flag;
    bool prev;
    explicit RestoreGuard(bool& f) : flag(f), prev(f) { flag = true; }
    ~RestoreGuard() { flag = prev; }
  } guard(mVolumReverbRestoreInProgress);

  // Mirror the delay-side per-mode default update so that double-clicking a reverb knob
  // resets it to the design-guide value for the currently selected mode.
  const volum::VoLumEffectSettings designDefaults;
  const int clampedMode = std::clamp(mode, 0, volum::kVoLumReverbModeCount - 1);
  const auto& d = designDefaults.reverbModes[clampedMode];
  const int restoredSubMode = clampedMode == volum::kVoLumReverbModeOktaverb
                                ? std::clamp(mVolumEffectSettings.reverbModes[clampedMode].subMode, 0, 2)
                                : std::clamp(d.subMode, 0, 2);
  if (clampedMode == volum::kVoLumReverbModeOktaverb)
  {
    const auto& subDefaults = designDefaults.oktaverbSubModes[restoredSubMode];
    GetParam(kReverbMix)->SetDefault(subDefaults.mix);
    GetParam(kReverbDecay)->SetDefault(subDefaults.decay);
    GetParam(kReverbTone)->SetDefault(subDefaults.tone);
    GetParam(kReverbPreDelay)->SetDefault(subDefaults.preDelay);
    GetParam(kReverbShimmer)->SetDefault(subDefaults.shimmer);
  }
  else
  {
    GetParam(kReverbMix)->SetDefault(d.mix);
    GetParam(kReverbDecay)->SetDefault(d.decay);
    GetParam(kReverbTone)->SetDefault(d.tone);
    GetParam(kReverbPreDelay)->SetDefault(d.preDelay);
    GetParam(kReverbShimmer)->SetDefault(d.shimmer);
  }
  GetParam(kReverbSubMode)->SetDefault(static_cast<double>(restoredSubMode));

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  const auto& s = mVolumEffectSettings.reverbModes[clampedMode];
  if (clampedMode == volum::kVoLumReverbModeOktaverb)
  {
    setParam(kReverbSubMode, static_cast<double>(restoredSubMode));
    _VolumRestoreOktaverbSubModeSnapshot(restoredSubMode);
  }
  else
  {
    setParam(kReverbMix, s.mix);
    setParam(kReverbDecay, s.decay);
    setParam(kReverbTone, s.tone);
    setParam(kReverbPreDelay, s.preDelay);
    setParam(kReverbShimmer, s.shimmer);
    setParam(kReverbSubMode, static_cast<double>(std::clamp(s.subMode, 0, 2)));
  }
}

void NeuralAmpModeler::_VolumSaveOktaverbSubModeSnapshot(int subMode)
{
  auto& s = mVolumEffectSettings.oktaverbSubModes[std::clamp(subMode, 0, 2)];
  s.mix = GetParam(kReverbMix)->Value();
  s.decay = GetParam(kReverbDecay)->Value();
  s.tone = GetParam(kReverbTone)->Value();
  s.preDelay = GetParam(kReverbPreDelay)->Value();
  s.shimmer = GetParam(kReverbShimmer)->Value();
}

void NeuralAmpModeler::_VolumRestoreOktaverbSubModeSnapshot(int subMode)
{
  // Same RAII guard as _VolumRestoreReverbModeSnapshot: the setParam calls below send
  // values via SendParameterValueFromDelegate, which triggers OnParamChangeUI for the
  // reverb knobs and would otherwise overwrite the snapshot we are restoring from.
  struct RestoreGuard {
    bool& flag;
    bool prev;
    explicit RestoreGuard(bool& f) : flag(f), prev(f) { flag = true; }
    ~RestoreGuard() { flag = prev; }
  } guard(mVolumReverbRestoreInProgress);

  const volum::VoLumEffectSettings designDefaults;
  const int clampedSubMode = std::clamp(subMode, 0, 2);
  const auto& d = designDefaults.oktaverbSubModes[clampedSubMode];
  GetParam(kReverbMix)->SetDefault(d.mix);
  GetParam(kReverbDecay)->SetDefault(d.decay);
  GetParam(kReverbTone)->SetDefault(d.tone);
  GetParam(kReverbPreDelay)->SetDefault(d.preDelay);
  GetParam(kReverbShimmer)->SetDefault(d.shimmer);

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  const auto& s = mVolumEffectSettings.oktaverbSubModes[clampedSubMode];
  setParam(kReverbMix, s.mix);
  setParam(kReverbDecay, s.decay);
  setParam(kReverbTone, s.tone);
  setParam(kReverbPreDelay, s.preDelay);
  setParam(kReverbShimmer, s.shimmer);
}

void NeuralAmpModeler::_VolumRestoreFromSettings(int ampIdx)
{
  auto& s = mVolumAmpSettings[ampIdx];
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
  _VolumRefreshSupportChannels();
  const bool shouldLoadPreNam1 = volum::ShouldLoadPrePedalCapture(s.preNam1Active, s.preNam1Capture);
  const bool shouldLoadPreNam2 = volum::ShouldLoadPrePedalCapture(s.preNam2Active, s.preNam2Capture);
  mVolumPreNeedsLoad[0].store(shouldLoadPreNam1);
  mVolumPreNeedsLoad[1].store(shouldLoadPreNam2);
  mShouldRemovePreModel[0].store(!shouldLoadPreNam1);
  mShouldRemovePreModel[1].store(!shouldLoadPreNam2);
  mVolumSupportNeedsLoad.store(true);

  // POST per-amp restore. postValid==false means this slot is new / legacy and has no
  // saved POST scene yet. Initialize it to the meaningful factory POST defaults instead
  // of inheriting whatever amp was previously selected; after this point every amp has
  // an explicit POST scene.
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
    s.postReverbActive = defaults.postReverbActive;
    s.postReverbMix = defaults.postReverbMix;
    s.postReverbDecay = defaults.postReverbDecay;
    s.postReverbTone = defaults.postReverbTone;
    s.postReverbPreDelay = defaults.postReverbPreDelay;
    s.postReverbShimmer = defaults.postReverbShimmer;
    s.postReverbMode = defaults.postReverbMode;
    s.postReverbSubMode = defaults.postReverbSubMode;
    for (int mode = 0; mode < volum::kVoLumDelayModeCount; ++mode)
      s.postDelayModes[mode] = defaults.postDelayModes[mode];
    for (int mode = 0; mode < volum::kVoLumReverbModeCount; ++mode)
      s.postReverbModes[mode] = defaults.postReverbModes[mode];
    for (int subMode = 0; subMode < 3; ++subMode)
      s.postOktaverbSubModes[subMode] = defaults.postOktaverbSubModes[subMode];
  }

  for (int mode = 0; mode < volum::kVoLumDelayModeCount; ++mode)
    mVolumEffectSettings.delayModes[mode] = s.postDelayModes[mode];
  for (int mode = 0; mode < volum::kVoLumReverbModeCount; ++mode)
    mVolumEffectSettings.reverbModes[mode] = s.postReverbModes[mode];
  for (int subMode = 0; subMode < 3; ++subMode)
    mVolumEffectSettings.oktaverbSubModes[subMode] = s.postOktaverbSubModes[subMode];

  struct PostRestoreGuard {
    bool& flag;
    bool prev;
    explicit PostRestoreGuard(bool& f) : flag(f), prev(f) { flag = true; }
    ~PostRestoreGuard() { flag = prev; }
  } postGuard(mVolumPostRestoreInProgress);

  setParam(kDelayActive, s.postDelayActive ? 1.0 : 0.0);
  setParam(kDelayTime, s.postDelayTime);
  setParam(kDelayFeedback, s.postDelayFeedback);
  setParam(kDelayMix, s.postDelayMix);
  setParam(kDelayMode, s.postDelayMode);
  setParam(kDelayTone, s.postDelayTone);
  setParam(kDelayAge, s.postDelayAge);
  setParam(kDelayPingPong, s.postDelayPingPong ? 1.0 : 0.0);
  setParam(kReverbActive, s.postReverbActive ? 1.0 : 0.0);
  setParam(kReverbMix, s.postReverbMix);
  setParam(kReverbDecay, s.postReverbDecay);
  setParam(kReverbTone, s.postReverbTone);
  setParam(kReverbPreDelay, s.postReverbPreDelay);
  setParam(kReverbShimmer, s.postReverbShimmer);
  setParam(kReverbMode, s.postReverbMode);
  setParam(kReverbSubMode, s.postReverbSubMode);
  mVolumEffectSettings.delayActive = s.postDelayActive;
  mVolumEffectSettings.delayMode = s.postDelayMode;
  mVolumEffectSettings.reverbActive = s.postReverbActive;
  mVolumEffectSettings.reverbMode = s.postReverbMode;
  const int restoredDelayMode = std::clamp(s.postDelayMode, 0, volum::kVoLumDelayModeCount - 1);
  const int restoredReverbMode = std::clamp(s.postReverbMode, 0, volum::kVoLumReverbModeCount - 1);
  _VolumSaveDelayModeSnapshot(restoredDelayMode);
  _VolumSaveReverbModeSnapshot(restoredReverbMode);
  _VolumRestoreDelayModeSnapshot(restoredDelayMode);
  _VolumRestoreReverbModeSnapshot(restoredReverbMode);

  // Update speaker row UI if available
  if (auto* pGfx = GetUI())
  {
    if (auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      spkCtrl->As<VoLumSpeakerRowControl>()->SetSelected(mVolumSpeakerIdx);
    _UpdateVoLumLayout(pGfx);
  }
}

void NeuralAmpModeler::_VolumSaveSettingsToFile()
{
  _VolumSaveEffectSettings();
  // Keep the shared legacy file readable by already-installed older VoLum builds. New dual-amp
  // fields live in a sidecar that older builds do not know about, avoiding crashes when users
  // run a newer standalone and then open an older VST3 in a DAW.
  nlohmann::json j = volum::VolumUserSettingsToJson(mVolumAmpSettings.data(), volum::kAmpCount, mVolumAmpIdx,
                                                    &mVolumEffectSettings, /*includeDualAmp=*/false);
  nlohmann::json dualAmpJson = volum::VolumDualAmpUserSettingsToJson(mVolumAmpSettings.data(), volum::kAmpCount);

  namespace fs = std::filesystem;
  fs::path settingsPath = volum::VolumUserSettingsFilePath();
  fs::path dualAmpSettingsPath = volum::VolumDualAmpSettingsFilePath();
  if (settingsPath.empty())
  {
    if (mVolumRigsRoot.empty())
      return;
    settingsPath = fs::path(mVolumRigsRoot) / "volum-settings.json";
    dualAmpSettingsPath = fs::path(mVolumRigsRoot) / "volum-dual-amp-settings.json";
  }

  std::error_code ec;
  const fs::path parent = settingsPath.parent_path();
  if (!parent.empty())
    fs::create_directories(parent, ec);

  std::ofstream out(settingsPath, std::ios::out | std::ios::trunc);
  if (!out)
  {
    std::cerr << "VoLum: cannot open settings file for write: " << settingsPath.string() << std::endl;
    return;
  }
  out << j.dump(2);
  if (!out.good())
    std::cerr << "VoLum: write failed for settings file: " << settingsPath.string() << std::endl;

  std::ofstream dualOut(dualAmpSettingsPath, std::ios::out | std::ios::trunc);
  if (!dualOut)
  {
    std::cerr << "VoLum: cannot open dual-amp settings file for write: " << dualAmpSettingsPath.string() << std::endl;
    return;
  }
  dualOut << dualAmpJson.dump(2);
  if (!dualOut.good())
    std::cerr << "VoLum: write failed for dual-amp settings file: " << dualAmpSettingsPath.string() << std::endl;
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
    legacyPath = fs::path(mVolumRigsRoot) / "volum-settings.json";
    dualAmpLegacyPath = fs::path(mVolumRigsRoot) / "volum-dual-amp-settings.json";
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
    volum::VolumUserSettingsFromJson(
      j, mVolumAmpSettings.data(), volum::kAmpCount, &mVolumAmpIdx, &mVolumEffectSettings, &settingsHealed);
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
    _VolumRestoreEffectSettings();
  }
  catch (...)
  {
    std::cerr << "Failed to read volum-settings.json" << std::endl;
  }
}