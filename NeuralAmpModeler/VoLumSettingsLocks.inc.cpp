// PRE/POST save-to-slot, lock + dirty state, effect/mode snapshot helpers.
// Tail-#included via VoLumSettings.inc.cpp (NOT a separate TU); file-size hygiene.

void NeuralAmpModeler::_VolumSavePreToSlot(volum::VoLumAmpSettings& s)
{
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
  s.prePitchActive = GetParam(kPrePitchActive)->Bool();
  s.prePitchMode = GetParam(kPrePitchMode)->Int();
  s.prePitchSemitones = GetParam(kPrePitchSemitones)->Value();
  s.prePitchMix = GetParam(kPrePitchMix)->Value();
  s.prePitchOctDown = GetParam(kPrePitchOctDown)->Value();
  s.prePitchOctUp = GetParam(kPrePitchOctUp)->Value();
  s.prePitchDry = GetParam(kPrePitchDry)->Value();
  s.prePitchVoicing = GetParam(kPrePitchVoicing)->Int();
  s.prePitchLevel = GetParam(kPrePitchLevel)->Value();
  s.prePitchTransChar = GetParam(kPrePitchTransChar)->Int();
  for (int mode = 0; mode < volum::kVoLumPitchModeCount; ++mode)
    s.prePitchModes[mode] = mVolumPrePitchModes[mode];
}

void NeuralAmpModeler::_VolumSavePostToSlot(volum::VoLumAmpSettings& s)
{
  s.postValid = true;
  s.postDelayActive = GetParam(kDelayActive)->Bool();
  s.postDelayTime = GetParam(kDelayTime)->Value();
  s.postDelayFeedback = GetParam(kDelayFeedback)->Value();
  s.postDelayMix = GetParam(kDelayMix)->Value();
  s.postDelayMode = GetParam(kDelayMode)->Int();
  s.postDelayTone = GetParam(kDelayTone)->Value();
  s.postDelayAge = GetParam(kDelayAge)->Value();
  s.postDelayPingPong = GetParam(kDelayPingPong)->Bool();
  s.postDelaySync = GetParam(kDelaySync)->Bool();
  s.postDelayDivision = GetParam(kDelayDivision)->Int();
  s.postReverbActive = GetParam(kReverbActive)->Bool();
  s.postReverbMix = GetParam(kReverbMix)->Value();
  s.postReverbDecay = GetParam(kReverbDecay)->Value();
  s.postReverbTone = GetParam(kReverbTone)->Value();
  s.postReverbPreDelay = GetParam(kReverbPreDelay)->Value();
  s.postReverbShimmer = GetParam(kReverbShimmer)->Value();
  s.postReverbMode = GetParam(kReverbMode)->Int();
  s.postReverbSubMode = GetParam(kReverbSubMode)->Int();
  s.postTremoloActive = GetParam(kTremoloActive)->Bool();
  s.postTremoloMode = GetParam(kTremoloMode)->Int();
  s.postTremoloRate = GetParam(kTremoloRate)->Value();
  s.postTremoloDepth = GetParam(kTremoloDepth)->Value();
  s.postTremoloShape = GetParam(kTremoloShape)->Value();
  s.postTremoloMix = GetParam(kTremoloMix)->Value();
  s.postTremoloCrossover = GetParam(kTremoloCrossover)->Value();
  s.postTremoloSync = GetParam(kTremoloSync)->Bool();
  s.postTremoloDivision = GetParam(kTremoloDivision)->Int();
  for (int mode = 0; mode < volum::kVoLumDelayModeCount; ++mode)
    s.postDelayModes[mode] = mVolumEffectSettings.delayModes[mode];
  for (int mode = 0; mode < volum::kVoLumReverbModeCount; ++mode)
    s.postReverbModes[mode] = mVolumEffectSettings.reverbModes[mode];
  for (int subMode = 0; subMode < 3; ++subMode)
    s.postOktaverbSubModes[subMode] = mVolumEffectSettings.oktaverbSubModes[subMode];
  for (int mode = 0; mode < volum::kVoLumTremoloModeCount; ++mode)
    s.postTremoloModes[mode] = mVolumEffectSettings.tremoloModes[mode];
}

void NeuralAmpModeler::_VolumSaveCurrentToSettings()
{
  // A focused custom amp (F6) keeps its own scene on this instance, keyed by its
  // stable id. Redirect the live snapshot there so we never clobber the underlying
  // factory amp slot (mVolumAmpIdx) while a custom amp is active.
  volum::VoLumAmpSettings* target = &mVolumAmpSettings[mVolumAmpIdx];
  if (mVolumCustomMainIdx >= 0)
  {
    const std::string id = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
    if (!id.empty())
      target = &_VolumCustomScene(id);
  }
  auto& s = *target;
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
  mVolumPrePitchMode = GetParam(kPrePitchMode)->Int();
  _VolumSavePrePitchModeSnapshot(std::clamp(mVolumPrePitchMode, 0, volum::kVoLumPitchModeCount - 1));
  if (!mVolumPreLocked)
    _VolumSavePreToSlot(s);
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
  // Custom SUPPORT partner cab/channel live on the (MAIN) scene next to
  // supportCustomId so they survive preset save + recall. The factory
  // supportSpeakerIdx/supportChannelIdx params are meaningless for a custom
  // support amp, so mirror the live runtime selection here instead.
  if (mVolumCustomSupportIdx >= 0)
  {
    s.supportCustomSlot = mVolumCustomSupportSlot;
    s.supportCustomChannel = mVolumCustomSupportChannel;
  }

  mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
  mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
  mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
  mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
  mVolumEffectSettings.tremoloMode = GetParam(kTremoloMode)->Int();
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  _VolumSaveReverbModeSnapshot(std::clamp(mVolumEffectSettings.reverbMode, 0, volum::kVoLumReverbModeCount - 1));
  _VolumSaveTremoloModeSnapshot(std::clamp(mVolumEffectSettings.tremoloMode, 0, volum::kVoLumTremoloModeCount - 1));

  if (!mVolumPostLocked)
    _VolumSavePostToSlot(s);

  // Always mirror live PRE/POST into the dedicated live-lock snapshots when the
  // corresponding lock is engaged. These live OUTSIDE the per-amp settings array
  // so persisting them never mutates any amp slot. When unlocked the snapshot is
  // stale; we ignore it because unlock falls back to the per-amp slot.
  if (mVolumPreLocked)
    _VolumSavePreToSlot(mVolumLiveLockedPre);
  if (mVolumPostLocked)
    _VolumSavePostToSlot(mVolumLiveLockedPost);
}

void NeuralAmpModeler::_VolumSetPreLocked(bool locked)
{
  if (mVolumPreLocked == locked)
    return;
  if (locked)
  {
    // Capture the current live PRE into the dedicated lock snapshot so the
    // shutdown/reload path can persist exactly what the user is hearing.
    _VolumSavePreToSlot(mVolumLiveLockedPre);
  }
  else
  {
    // Unlock restores the focused amp's scene (custom amps keep their own scene
    // in the content library, factory amps use mVolumAmpSettings[mVolumAmpIdx]).
    _VolumRestorePreFromSlot(_VolumActiveScene());
  }
  mVolumPreLocked = locked;
  mVolumPreLockUiDirty = locked && _VolumIsPreDirty();
  mVolumSettingsDirty = true;
  if (auto* pGfx = GetUI())
    _UpdateVoLumLayout(pGfx);
}

void NeuralAmpModeler::_VolumSetPostLocked(bool locked)
{
  if (mVolumPostLocked == locked)
    return;
  if (locked)
  {
    _VolumSavePostToSlot(mVolumLiveLockedPost);
  }
  else
  {
    _VolumRestorePostFromSlot(_VolumActiveScene());
  }
  mVolumPostLocked = locked;
  mVolumPostLockUiDirty = locked && _VolumIsPostDirty();
  mVolumSettingsDirty = true;
  if (auto* pGfx = GetUI())
    _UpdateVoLumLayout(pGfx);
}

bool NeuralAmpModeler::_VolumIsPreDirty() const
{
  // While locked, compare the persisted overlay snapshot against the active amp
  // slot. Re-reading live params can drift from the slot after reload (param
  // normalization / effect-restore ordering), which left the store arrow stuck
  // on the origin amp even when its saved scene matched the carried overlay.
  auto& scene = const_cast<NeuralAmpModeler*>(this)->_VolumActiveScene();
  if (mVolumPreLocked)
    return !volum::PreBlockEquals(mVolumLiveLockedPre, scene);

  auto* self = const_cast<NeuralAmpModeler*>(this);
  self->_VolumSavePrePitchModeSnapshot(std::clamp(GetParam(kPrePitchMode)->Int(), 0, volum::kVoLumPitchModeCount - 1));
  volum::VoLumAmpSettings live;
  self->_VolumSavePreToSlot(live);
  return !volum::PreBlockEquals(live, scene);
}

bool NeuralAmpModeler::_VolumIsPostDirty() const
{
  auto* self = const_cast<NeuralAmpModeler*>(this);
  auto& scene = self->_VolumActiveScene();
  if (mVolumPostLocked)
    return !volum::PostBlockEquals(mVolumLiveLockedPost, scene);

  const int delayMode = std::clamp(GetParam(kDelayMode)->Int(), 0, volum::kVoLumDelayModeCount - 1);
  const int reverbMode = std::clamp(GetParam(kReverbMode)->Int(), 0, volum::kVoLumReverbModeCount - 1);
  self->_VolumSaveDelayModeSnapshot(delayMode);
  self->_VolumSaveReverbModeSnapshot(reverbMode);
  volum::VoLumAmpSettings live;
  self->_VolumSavePostToSlot(live);
  return !volum::PostBlockEquals(live, scene);
}

void NeuralAmpModeler::_VolumApplyLiveLockSnapshots()
{
  // Apply at init only: restore the live PRE/POST that the user was hearing
  // before shutdown without touching `mVolumAmpSettings[*]`. The lock guards in
  // `_VolumRestoreFromSettings` mean an init pass with the lock engaged leaves
  // live PRE/POST at their struct defaults; this call patches that gap. Once
  // applied the snapshots stay in sync via `_VolumSaveCurrentToSettings()`.
  if (mVolumPreLocked)
    _VolumRestorePreFromSlot(mVolumLiveLockedPre);
  if (mVolumPostLocked)
    _VolumRestorePostFromSlot(mVolumLiveLockedPost);
}

void NeuralAmpModeler::_VolumStorePreToCurrentAmp()
{
  auto& scene = _VolumActiveScene();
  _VolumSavePreToSlot(scene);
  const auto& s = scene;
  const bool shouldLoadPreNam1 = volum::ShouldLoadPrePedalCapture(s.preNam1Active, s.preNam1Capture);
  const bool shouldLoadPreNam2 = volum::ShouldLoadPrePedalCapture(s.preNam2Active, s.preNam2Capture);
  mVolumPreNeedsLoad[0].store(shouldLoadPreNam1);
  mVolumPreNeedsLoad[1].store(shouldLoadPreNam2);
  mShouldRemovePreModel[0].store(!shouldLoadPreNam1);
  mShouldRemovePreModel[1].store(!shouldLoadPreNam2);
  mVolumSettingsDirty = true;
  mVolumPreLockUiDirty = false;
  if (auto* pGfx = GetUI())
    _UpdateVoLumLayout(pGfx);
}

void NeuralAmpModeler::_VolumStorePostToCurrentAmp()
{
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  _VolumSaveReverbModeSnapshot(std::clamp(mVolumEffectSettings.reverbMode, 0, volum::kVoLumReverbModeCount - 1));
  _VolumSavePostToSlot(_VolumActiveScene());
  mVolumSettingsDirty = true;
  mVolumPostLockUiDirty = false;
  if (auto* pGfx = GetUI())
    _UpdateVoLumLayout(pGfx);
}

void NeuralAmpModeler::_VolumSaveEffectSettings()
{
  mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
  mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
  mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
  mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
  mVolumEffectSettings.tremoloMode = GetParam(kTremoloMode)->Int();
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  _VolumSaveReverbModeSnapshot(std::clamp(mVolumEffectSettings.reverbMode, 0, volum::kVoLumReverbModeCount - 1));
  _VolumSaveTremoloModeSnapshot(std::clamp(mVolumEffectSettings.tremoloMode, 0, volum::kVoLumTremoloModeCount - 1));
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
  setParam(kTremoloMode, fx.tremoloMode);
  _VolumRestoreTremoloModeSnapshot(std::clamp(fx.tremoloMode, 0, volum::kVoLumTremoloModeCount - 1));
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
  struct RestoreGuard
  {
    bool& flag;
    bool prev;
    explicit RestoreGuard(bool& f)
    : flag(f)
    , prev(f)
    {
      flag = true;
    }
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

void NeuralAmpModeler::_VolumSaveTremoloModeSnapshot(int mode)
{
  auto& s = mVolumEffectSettings.tremoloModes[std::clamp(mode, 0, volum::kVoLumTremoloModeCount - 1)];
  s.rate = GetParam(kTremoloRate)->Value();
  s.depth = GetParam(kTremoloDepth)->Value();
  s.shape = GetParam(kTremoloShape)->Value();
  s.mix = GetParam(kTremoloMix)->Value();
  s.crossover = GetParam(kTremoloCrossover)->Value();
}

void NeuralAmpModeler::_VolumRestoreTremoloModeSnapshot(int mode)
{
  // Same re-entrancy guard as the reverb path: the setParam cascade below sends
  // values via SendParameterValueFromDelegate -> OnParamChangeUI for the tremolo
  // knobs and would otherwise write the partially-restored state back into the
  // snapshot we are loading from.
  struct RestoreGuard
  {
    bool& flag;
    bool prev;
    explicit RestoreGuard(bool& f)
    : flag(f)
    , prev(f)
    {
      flag = true;
    }
    ~RestoreGuard() { flag = prev; }
  } guard(mVolumTremoloRestoreInProgress);

  // Per-knob double-click "reset to default" lands on the design value for the
  // CURRENT tremolo mode, matching the delay/reverb behavior.
  const volum::VoLumEffectSettings designDefaults;
  const int clampedMode = std::clamp(mode, 0, volum::kVoLumTremoloModeCount - 1);
  const auto& d = designDefaults.tremoloModes[clampedMode];
  GetParam(kTremoloRate)->SetDefault(d.rate);
  GetParam(kTremoloDepth)->SetDefault(d.depth);
  GetParam(kTremoloShape)->SetDefault(d.shape);
  GetParam(kTremoloMix)->SetDefault(d.mix);
  GetParam(kTremoloCrossover)->SetDefault(d.crossover);

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  const auto& s = mVolumEffectSettings.tremoloModes[clampedMode];
  setParam(kTremoloRate, s.rate);
  setParam(kTremoloDepth, s.depth);
  setParam(kTremoloShape, s.shape);
  setParam(kTremoloMix, s.mix);
  setParam(kTremoloCrossover, s.crossover);
}

void NeuralAmpModeler::_VolumSavePrePitchModeSnapshot(int mode)
{
  auto& s = mVolumPrePitchModes[std::clamp(mode, 0, volum::kVoLumPitchModeCount - 1)];
  s.mix = GetParam(kPrePitchMix)->Value();
  s.dry = GetParam(kPrePitchDry)->Value();
  s.level = GetParam(kPrePitchLevel)->Value();
  s.voicing = GetParam(kPrePitchVoicing)->Int();
}

void NeuralAmpModeler::_VolumRestorePrePitchModeSnapshot(int mode)
{
  struct RestoreGuard
  {
    bool& flag;
    bool prev;
    explicit RestoreGuard(bool& f)
    : flag(f)
    , prev(f)
    {
      flag = true;
    }
    ~RestoreGuard() { flag = prev; }
  } guard(mVolumPreRestoreInProgress);

  const int clampedMode = std::clamp(mode, 0, volum::kVoLumPitchModeCount - 1);
  const auto& s = mVolumPrePitchModes[clampedMode];

  // Per-knob double-click defaults: the shared pitch knobs reset to the live
  // snapshot's value for the current mode (there is no design-guide table for
  // pitch, so the recalled value IS the default).
  GetParam(kPrePitchMix)->SetDefault(s.mix);
  GetParam(kPrePitchDry)->SetDefault(s.dry);
  GetParam(kPrePitchLevel)->SetDefault(s.level);
  GetParam(kPrePitchVoicing)->SetDefault(static_cast<double>(std::clamp(s.voicing, 0, 1)));

  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  setParam(kPrePitchMix, s.mix);
  setParam(kPrePitchDry, s.dry);
  setParam(kPrePitchLevel, s.level);
  setParam(kPrePitchVoicing, static_cast<double>(std::clamp(s.voicing, 0, 1)));
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
  struct RestoreGuard
  {
    bool& flag;
    bool prev;
    explicit RestoreGuard(bool& f)
    : flag(f)
    , prev(f)
    {
      flag = true;
    }
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

