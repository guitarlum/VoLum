// VoLum settings persistence + per-amp / per-mode snapshot helpers.
//
// Tail-included from NeuralAmpModeler.cpp (NOT a separate TU); pure file-size
// hygiene split. Contains save/restore of mVolumAmpSettings, the delay/reverb
// mode snapshots, and the legacy + dual-amp JSON settings file I/O.

#include "VoLumPrePostLock.h"

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
}

void NeuralAmpModeler::_VolumSaveCurrentToSettings()
{
  // A focused custom amp (F6) keeps its own scene in the content library, keyed
  // by its stable id. Redirect the live snapshot there so we never clobber the
  // underlying factory amp slot (mVolumAmpIdx) while a custom amp is active.
  volum::VoLumAmpSettings* target = &mVolumAmpSettings[mVolumAmpIdx];
  if (mVolumCustomMainIdx >= 0)
  {
    const std::string id = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
    if (!id.empty())
      target = &volum::content::GlobalContentStore().reg().customScenes[id];
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
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  _VolumSaveReverbModeSnapshot(std::clamp(mVolumEffectSettings.reverbMode, 0, volum::kVoLumReverbModeCount - 1));

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

  volum::VoLumAmpSettings live;
  const_cast<NeuralAmpModeler*>(this)->_VolumSavePreToSlot(live);
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

void NeuralAmpModeler::_VolumRestorePreFromSlot(const volum::VoLumAmpSettings& s)
{
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

  const bool shouldLoadPreNam1 = volum::ShouldLoadPrePedalCapture(s.preNam1Active, s.preNam1Capture);
  const bool shouldLoadPreNam2 = volum::ShouldLoadPrePedalCapture(s.preNam2Active, s.preNam2Capture);
  mVolumPreNeedsLoad[0].store(shouldLoadPreNam1);
  mVolumPreNeedsLoad[1].store(shouldLoadPreNam2);
  mShouldRemovePreModel[0].store(!shouldLoadPreNam1);
  mShouldRemovePreModel[1].store(!shouldLoadPreNam2);
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
  if (s.dualAmpActive && s.supportPolarityInvert && std::abs(s.mainAmpPan) < 1e-3 && std::abs(s.supportAmpPan) < 1e-3)
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

  // Update speaker row UI if available
  if (auto* pGfx = GetUI())
  {
    if (auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      spkCtrl->As<VoLumSpeakerRowControl>()->SetSelected(mVolumSpeakerIdx);
    _UpdateVoLumLayout(pGfx);
  }

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
    mVolumPostLocked ? &mVolumLiveLockedPost : nullptr, mVolumLiteMode.load());
  nlohmann::json dualAmpJson = volum::VolumDualAmpUserSettingsToJson(mVolumAmpSettings.data(), volum::kAmpCount);

  // 1.2.0 additive session refs (ignored by older builds): the focused custom
  // MAIN amp + active preset so the next launch re-selects them. Per-amp IR /
  // custom-support refs already round-trip inside each scene's JSON.
  j["volumCustomMainId"] = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
  j["volumActivePresetId"] = mVolumActivePresetId;

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

  // Persist the shared content library too (custom-amp scenes accumulate live
  // knob edits via _VolumSaveCurrentToSettings). No-op when no base dir is set.
  volum::content::GlobalContentStore().Save();
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
    bool haveLivePreSnapshot = false;
    bool haveLivePostSnapshot = false;
    volum::VoLumAmpSettings parsedLivePre;
    volum::VoLumAmpSettings parsedLivePost;
    bool parsedLiteMode = false;
    volum::VolumUserSettingsFromJson(j, mVolumAmpSettings.data(), volum::kAmpCount, &mVolumAmpIdx,
                                     &mVolumEffectSettings, &settingsHealed, &mVolumPreLocked, &mVolumPostLocked,
                                     &parsedLivePre, &parsedLivePost, &haveLivePreSnapshot, &haveLivePostSnapshot,
                                     &parsedLiteMode);
    mVolumLiteMode.store(parsedLiteMode);
    if (haveLivePreSnapshot)
      mVolumLiveLockedPre = parsedLivePre;
    if (haveLivePostSnapshot)
      mVolumLiveLockedPost = parsedLivePost;
    // 1.2.0 session refs: stash the focused custom MAIN amp + active preset for
    // re-selection once the UI opens (see OnUIOpen). Absent on older files.
    if (j.contains("volumCustomMainId") && j["volumCustomMainId"].is_string())
      mVolumRestoreCustomMainId = j["volumCustomMainId"].get<std::string>();
    if (j.contains("volumActivePresetId") && j["volumActivePresetId"].is_string())
      mVolumRestorePresetId = j["volumActivePresetId"].get<std::string>();
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

void NeuralAmpModeler::_VolumInstallPresetHooks()
{
  // Capture: sync live params into the active scene, then hand back a copy so a
  // preset records the complete current rig (incl. the id-based custom refs that
  // live on the scene, not on params).
  volum::custom::PresetCaptureHook() = [this]() -> volum::VoLumAmpSettings {
    _VolumSaveCurrentToSettings();
    return _VolumActiveScene();
  };
  volum::custom::PresetApplyHook() = [this](const volum::VoLumAmpSettings& s) { _VolumApplyRecalledPreset(s); };
}

void NeuralAmpModeler::_VolumRememberActivePreset()
{
  const std::string key = _VolumActiveOwnerKey();
  if (mVolumHasRecalledSnapshot && !mVolumActivePresetId.empty())
  {
    mVolumActivePresetIdByOwner[key] = mVolumActivePresetId;
    mVolumRecalledSnapshotByOwner[key] = mVolumRecalledSnapshot;
  }
  else
  {
    mVolumActivePresetIdByOwner.erase(key);
    mVolumRecalledSnapshotByOwner.erase(key);
  }
}

void NeuralAmpModeler::_VolumForgetActivePreset()
{
  mVolumHasRecalledSnapshot = false;
  mVolumActivePresetId.clear();
  mVolumActivePresetIdByOwner.erase(_VolumActiveOwnerKey());
  mVolumRecalledSnapshotByOwner.erase(_VolumActiveOwnerKey());
}

void NeuralAmpModeler::_VolumSyncPresetOwner()
{
  const std::string key = _VolumActiveOwnerKey();
  volum::custom::SetActivePresetOwner(key);
  // Restore the preset this amp last had selected (if any) so switching back to
  // an amp re-shows its active preset instead of blanking the bar. The id is
  // validated against the live bank in _VolumRefreshPresetBar, so a since-deleted
  // preset simply shows nothing selected.
  auto itId = mVolumActivePresetIdByOwner.find(key);
  auto itSnap = mVolumRecalledSnapshotByOwner.find(key);
  if (itId != mVolumActivePresetIdByOwner.end() && !itId->second.empty()
      && itSnap != mVolumRecalledSnapshotByOwner.end())
  {
    mVolumActivePresetId = itId->second;
    mVolumRecalledSnapshot = itSnap->second;
    mVolumHasRecalledSnapshot = true;
  }
  else
  {
    mVolumHasRecalledSnapshot = false;
    mVolumActivePresetId.clear();
  }
}

void NeuralAmpModeler::_VolumRefreshPresetBar()
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar);
  if (!pb)
    return;
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  auto* bar = pb->As<VoLumPresetBarControl>();
  bar->SetList(volum::custom::MockPresetsForAmp(mVolumAmpIdx)); // clears selection + dirty

  if (mVolumHasRecalledSnapshot && !mVolumActivePresetId.empty())
  {
    const auto& banks = volum::content::GlobalContentStore().reg().presetBanks;
    auto it = banks.find(_VolumActiveOwnerKey());
    bool found = false;
    if (it != banks.end())
      for (const auto& pr : it->second)
        if (pr.id == mVolumActivePresetId)
        {
          bar->SelectName(pr.name.c_str());
          found = true;
          break;
        }
    if (found)
      _VolumRecomputePresetDirty();
    else
      _VolumForgetActivePreset(); // preset was deleted out from under us
  }
}

int NeuralAmpModeler::_VolumSavePresetAs(const std::string& name)
{
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  const int idx = volum::custom::AddPreset(mVolumAmpIdx, name); // captures live via hook
  if (idx < 0)
    return idx;
  // The freshly saved preset becomes the active, clean recalled snapshot.
  mVolumActivePresetId = volum::custom::PresetIdAt(idx);
  mVolumRecalledSnapshot = _VolumActiveScene(); // hook already synced live -> scene
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
  return idx;
}

void NeuralAmpModeler::_VolumOverwritePreset(int index)
{
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  volum::custom::OverwritePreset(mVolumAmpIdx, index); // captures live via hook
  mVolumActivePresetId = volum::custom::PresetIdAt(index);
  mVolumRecalledSnapshot = _VolumActiveScene();
  mVolumHasRecalledSnapshot = true;
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
  _VolumRefreshPresetBar();
}

void NeuralAmpModeler::_VolumRecallPreset(int index)
{
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  mVolumActivePresetId = volum::custom::PresetIdAt(index);
  volum::custom::RecallPreset(mVolumAmpIdx, index); // -> apply hook -> _VolumApplyRecalledPreset
  _VolumRefreshPresetBar();
}

void NeuralAmpModeler::_VolumApplyRecalledPreset(const volum::VoLumAmpSettings& s)
{
  _VolumActiveScene() = s; // make the live scene equal the preset
  _VolumApplyAmpSettings(_VolumActiveScene());
  _VolumRefreshChannels();
  // Re-derive the scene from the now-live params so the retained baseline matches
  // exactly what _VolumRecomputePresetDirty() will read back (avoids a spurious
  // "(unsaved)" right after recall from param normalization).
  _VolumSaveCurrentToSettings();
  mVolumRecalledSnapshot = _VolumActiveScene();
  mVolumHasRecalledSnapshot = true;
  mVolumNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  _VolumRememberActivePreset();
}

void NeuralAmpModeler::_VolumRecomputePresetDirty()
{
  if (!mVolumInitComplete)
    return;
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar);
  if (!pb)
    return;
  auto* bar = pb->As<VoLumPresetBarControl>();
  if (!mVolumHasRecalledSnapshot)
  {
    bar->SetDirtyState(false);
    return;
  }
  _VolumSaveCurrentToSettings();
  const bool dirty = !volum::AmpSettingsEqual(_VolumActiveScene(), mVolumRecalledSnapshot);
  bar->SetDirtyState(dirty);
}