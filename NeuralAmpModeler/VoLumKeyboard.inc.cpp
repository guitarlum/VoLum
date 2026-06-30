// VoLumKeyboard.inc.cpp: on-screen keyboard navigation + exact-entry member functions
// Extracted from NeuralAmpModeler.cpp for file-size hygiene. Tail-#included
// into the NeuralAmpModeler translation unit; not a separate build target.

bool NeuralAmpModeler::_HandleVoLumKeyboardFocusKey(const IKeyPress& key)
{
  constexpr int kTabKey = '\t';
  constexpr int kSpaceKey = ' ';
  if (key.VK == '1')
    return _SwitchVoLumKeyboardSection(EVoLumSection::PRE);
  if (key.VK == '2')
    return _SwitchVoLumKeyboardSection(EVoLumSection::AMP);
  if (key.VK == '3')
    return _SwitchVoLumKeyboardSection(EVoLumSection::POST);
  if (key.VK == 't' || key.VK == 'T')
  {
    _ToggleVoLumTuner();
    return true;
  }
  if (key.VK == 'm' || key.VK == 'M')
  {
    _ToggleVoLumMetronomePanel();
    return true;
  }
  if (key.VK == 'h' || key.VK == 'H')
  {
    if (auto* pGfx = GetUI())
      if (auto* settings = pGfx->GetControlWithTag(kCtrlTagSettingsBox))
        settings->As<NAMSettingsPageControl>()->HideAnimated(false);
    return true;
  }
  if (key.VK == 's' || key.VK == 'S')
    return _CycleVoLumKeyboardSpeaker(key.S ? -1 : 1);

  if (key.VK == kTabKey)
    return _CycleVoLumKeyboardTarget(key.S ? -1 : 1);

  if (mVolumSelectedKnobParamIdx != kNoParameter)
    return false;

  if (key.VK == kVK_RETURN)
    return _ActivateVoLumKeyboardTarget();
  if (key.VK == kSpaceKey)
    return _ToggleVoLumKeyboardTarget();

  return false;
}

bool NeuralAmpModeler::_SwitchVoLumKeyboardSection(EVoLumSection section)
{
  _ClearVoLumKnobSelection();
  mVolumExpandedSection = section;

  switch (section)
  {
    case EVoLumSection::PRE:
      mVolumFocusedEffect = EVoLumEffectFocus::COMP;
      mVolumDualAmpFocusedSupport = false;
      break;
    case EVoLumSection::AMP:
      mVolumFocusedEffect = EVoLumEffectFocus::AMP;
      mVolumDualAmpFocusedSupport = false;
      break;
    case EVoLumSection::POST:
      mVolumFocusedEffect = EVoLumEffectFocus::DELAY;
      mVolumDualAmpFocusedSupport = false;
      break;
  }

  _UpdateVoLumLayout();
  _UpdateVoLumKeyboardFocusHint();
  return true;
}

bool NeuralAmpModeler::_CycleVoLumKeyboardTarget(int direction)
{
  _ClearVoLumKnobSelection();

  auto wrap = [](int value, int count) { return (value + count) % count; };

  switch (mVolumExpandedSection)
  {
    case EVoLumSection::PRE:
    {
      constexpr EVoLumEffectFocus targets[4] = {
        EVoLumEffectFocus::PITCH,
        EVoLumEffectFocus::COMP,
        EVoLumEffectFocus::PRE_NAM1,
        EVoLumEffectFocus::PRE_NAM2,
      };
      int current = 0;
      for (int i = 0; i < 4; ++i)
        if (targets[i] == mVolumFocusedEffect)
          current = i;
      mVolumFocusedEffect = targets[wrap(current + direction, 4)];
      mVolumDualAmpFocusedSupport = false;
      break;
    }
    case EVoLumSection::AMP:
    {
      mVolumFocusedEffect = EVoLumEffectFocus::AMP;
      mVolumDualAmpFocusedSupport = GetParam(kDualAmpActive)->Bool() ? !mVolumDualAmpFocusedSupport : false;
      break;
    }
    case EVoLumSection::POST:
    {
      const bool delayFocus = mVolumFocusedEffect == EVoLumEffectFocus::DELAY;
      mVolumFocusedEffect = delayFocus ? EVoLumEffectFocus::REVERB : EVoLumEffectFocus::DELAY;
      mVolumDualAmpFocusedSupport = false;
      break;
    }
  }

  _UpdateVoLumLayout();
  _UpdateVoLumKeyboardFocusHint();
  return true;
}

bool NeuralAmpModeler::_ActivateVoLumKeyboardTarget()
{
  const int paramIdx = _RememberedVoLumKeyboardKnobForFocus();
  if (paramIdx == kNoParameter)
    return false;

  _SelectVoLumKnob(paramIdx);
  return true;
}

bool NeuralAmpModeler::_ToggleVoLumKeyboardTarget()
{
  int paramIdx = kNoParameter;
  switch (mVolumFocusedEffect)
  {
    case EVoLumEffectFocus::AMP:
      if (mVolumExpandedSection == EVoLumSection::AMP)
        paramIdx = kDualAmpActive;
      break;
    case EVoLumEffectFocus::PITCH: paramIdx = kPrePitchActive; break;
    case EVoLumEffectFocus::COMP: paramIdx = kPreCompActive; break;
    case EVoLumEffectFocus::PRE_NAM1: paramIdx = kPreNam1Active; break;
    case EVoLumEffectFocus::PRE_NAM2: paramIdx = kPreNam2Active; break;
    case EVoLumEffectFocus::DELAY: paramIdx = kDelayActive; break;
    case EVoLumEffectFocus::REVERB: paramIdx = kReverbActive; break;
    case EVoLumEffectFocus::TREMOLO: paramIdx = kTremoloActive; break;
  }

  if (paramIdx == kNoParameter)
    return false;

  // Route through the shared funnel so the keyboard marks the preset dirty
  // exactly like the mouse chip/pill does (parity), then apply the keyboard's
  // own focus + layout updates.
  const bool next = _VolumUserToggleParam(paramIdx);

  if (paramIdx == kDualAmpActive)
    mVolumDualAmpFocusedSupport = next;

  _UpdateVoLumLayout();
  _UpdateVoLumKeyboardFocusHint();
  return true;
}

bool NeuralAmpModeler::_VolumUserToggleParam(int paramIdx)
{
  const bool next = !GetParam(paramIdx)->Bool();
  GetParam(paramIdx)->Set(next ? 1.0 : 0.0);
  SendParameterValueFromDelegate(paramIdx, GetParam(paramIdx)->GetNormalized(), true);
  OnParamChange(paramIdx);
  // The mouse paths historically called this; the keyboard path skipped it,
  // which is exactly the dual/COMP/DELAY/REVERB "keyboard toggle doesn't mark
  // dirty" bug. Centralising it here keeps both inputs in lock-step.
  _VolumMarkPresetDirty();
  return next;
}

bool NeuralAmpModeler::_CycleVoLumKeyboardSpeaker(int direction)
{
  if (mVolumExpandedSection != EVoLumSection::AMP)
    return false;

  constexpr int kSpeakerCount = 4;
  if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
  {
    const int current = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, kSpeakerCount - 1);
    const int next = (current + direction + kSpeakerCount) % kSpeakerCount;
    GetParam(kSupportSpeakerIdx)->Set(next);
    SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
    mVolumSettingsDirty = true;
    _VolumRefreshSupportChannels();
    mVolumSupportNeedsLoad.store(true);
  }
  else
  {
    const int current = std::clamp(mVolumSpeakerIdx, 0, kSpeakerCount - 1);
    const int next = (current + direction + kSpeakerCount) % kSpeakerCount;
    mVolumSpeakerIdx = next;
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = next;
    mVolumSettingsDirty = true;
    _VolumRefreshChannels();
    mVolumNeedsLoad.store(true);
  }
  _VolumMarkPresetDirty();

  if (auto* pGfx = GetUI())
  {
    if (auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      spkCtrl->As<VoLumSpeakerRowControl>()->SetSelected(GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport
                                                           ? GetParam(kSupportSpeakerIdx)->Int()
                                                           : mVolumSpeakerIdx);
    _UpdateVoLumLayout(pGfx);
  }
  _UpdateVoLumKeyboardFocusHint();
  return true;
}

void NeuralAmpModeler::_UpdateVoLumKeyboardFocusHint()
{
  if (mVolumSelectedKnobParamIdx != kNoParameter)
    return;

  const char* target = "Main amp";
  const char* action = "Space dual amp";
  const char* nav = "Up/Down amp  |  Left/Right channel  |  Tab target";
  switch (mVolumFocusedEffect)
  {
    case EVoLumEffectFocus::AMP:
      target = (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport) ? "Support amp" : "Main amp";
      action = "Space dual amp";
      nav = "Up/Down amp  |  Left/Right channel  |  S cab  |  Tab target";
      break;
    case EVoLumEffectFocus::PITCH:
      target = "Pitch";
      action = "Space on/off";
      nav = "Left/Right or Tab target";
      break;
    case EVoLumEffectFocus::COMP:
      target = "Compressor";
      action = "Space on/off";
      nav = "Left/Right or Tab target";
      break;
    case EVoLumEffectFocus::PRE_NAM1:
      target = "NAM 1";
      action = "Space on/off";
      nav = "Left/Right or Tab target";
      break;
    case EVoLumEffectFocus::PRE_NAM2:
      target = "NAM 2";
      action = "Space on/off";
      nav = "Left/Right or Tab target";
      break;
    case EVoLumEffectFocus::DELAY:
      target = "Delay";
      action = "Space on/off";
      nav = "Left/Right or Tab target";
      break;
    case EVoLumEffectFocus::REVERB:
      target = "Reverb";
      action = "Space on/off";
      nav = "Left/Right or Tab target";
      break;
    case EVoLumEffectFocus::TREMOLO:
      target = "Tremolo";
      action = "Space on/off";
      nav = "Left/Right or Tab target";
      break;
  }

  WDL_String line;
  line.SetFormatted(512, "%s  |  %s  |  Enter edit  |  %s", target, nav, action);

  if (auto* pGfx = GetUI())
    if (auto* hint = pGfx->GetControlWithTag(kCtrlTagVoLumKeyboardHint))
      hint->As<VoLumKeyboardHintControl>()->SetHintText(line.Get());
}

int NeuralAmpModeler::_DefaultVoLumKeyboardKnobForFocus() const
{
  switch (mVolumFocusedEffect)
  {
    case EVoLumEffectFocus::AMP:
      return (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport) ? kSupportInputLevel : kInputLevel;
    case EVoLumEffectFocus::PITCH:
      return GetParam(kPrePitchMode)->Int() == volum::kVoLumPitchModeTranspose ? kPrePitchSemitones : kPrePitchOctDown;
    case EVoLumEffectFocus::COMP: return kPreCompAmount;
    case EVoLumEffectFocus::PRE_NAM1: return kPreNam1Gain;
    case EVoLumEffectFocus::PRE_NAM2: return kPreNam2Gain;
    case EVoLumEffectFocus::DELAY: return GetParam(kDelaySync)->Bool() ? kDelayFeedback : kDelayTime;
    case EVoLumEffectFocus::REVERB: return kReverbMix;
    case EVoLumEffectFocus::TREMOLO: return GetParam(kTremoloSync)->Bool() ? kTremoloDepth : kTremoloRate;
  }
  return kNoParameter;
}

int NeuralAmpModeler::_RememberedVoLumKeyboardKnobForFocus() const
{
  using namespace volum::keyboard;
  const int remembered = mVolumLastKeyboardKnobByTarget[TargetIndex(mVolumFocusedEffect, mVolumDualAmpFocusedSupport)];
  switch (mVolumFocusedEffect)
  {
    case EVoLumEffectFocus::AMP:
      return GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport
               ? RememberedOrFirst(kSupportAmpParams, remembered)
               : (GetParam(kDualAmpActive)->Bool() ? RememberedOrFirst(kMainAmpDualParams, remembered)
                                                   : RememberedOrFirst(kMainAmpMonoParams, remembered));
    case EVoLumEffectFocus::PITCH:
      return GetParam(kPrePitchMode)->Int() == volum::kVoLumPitchModeTranspose
               ? RememberedOrFirst(kPitchTransposeParams, remembered)
               : RememberedOrFirst(kPitchOctaverParams, remembered);
    case EVoLumEffectFocus::COMP: return RememberedOrFirst(kCompParams, remembered);
    case EVoLumEffectFocus::PRE_NAM1: return RememberedOrFirst(kPreNam1Params, remembered);
    case EVoLumEffectFocus::PRE_NAM2: return RememberedOrFirst(kPreNam2Params, remembered);
    case EVoLumEffectFocus::DELAY: return RememberedOrFirst(kDelayParams, remembered);
    case EVoLumEffectFocus::REVERB:
      return GetParam(kReverbMode)->Int() == volum::kVoLumReverbModeOktaverb
               ? RememberedOrFirst(kOktaverbParams, remembered)
               : RememberedOrFirst(kReverbParams, remembered);
    case EVoLumEffectFocus::TREMOLO:
      return GetParam(kTremoloMode)->Int() == volum::kVoLumTremoloModeHarmonic
               ? RememberedOrFirst(kTremoloHarmonicParams, remembered)
               : RememberedOrFirst(kTremoloParams, remembered);
  }
  return _DefaultVoLumKeyboardKnobForFocus();
}

void NeuralAmpModeler::_RememberVoLumKeyboardKnob(int paramIdx)
{
  const int target = volum::keyboard::TargetIndex(mVolumFocusedEffect, mVolumDualAmpFocusedSupport);
  if (target >= 0 && target < static_cast<int>(mVolumLastKeyboardKnobByTarget.size()))
    mVolumLastKeyboardKnobByTarget[target] = paramIdx;
}

bool NeuralAmpModeler::_SelectAdjacentVoLumKnob(int currentParamIdx, int direction)
{
  using namespace volum::keyboard;
  switch (mVolumFocusedEffect)
  {
    case EVoLumEffectFocus::DELAY: return SelectAdjacentFromList(this, kDelayParams, currentParamIdx, direction);
    case EVoLumEffectFocus::REVERB:
    {
      const int reverbMode = GetParam(kReverbMode)->Int();
      if (reverbMode == volum::kVoLumReverbModeOktaverb)
        return SelectAdjacentFromList(this, kOktaverbParams, currentParamIdx, direction);
      return SelectAdjacentFromList(this, kReverbParams, currentParamIdx, direction);
    }
    case EVoLumEffectFocus::AMP:
      if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
        return SelectAdjacentFromList(this, kSupportAmpParams, currentParamIdx, direction);
      if (GetParam(kDualAmpActive)->Bool())
        return SelectAdjacentFromList(this, kMainAmpDualParams, currentParamIdx, direction);
      return SelectAdjacentFromList(this, kMainAmpMonoParams, currentParamIdx, direction);
    case EVoLumEffectFocus::PITCH:
      return GetParam(kPrePitchMode)->Int() == volum::kVoLumPitchModeTranspose
               ? SelectAdjacentFromList(this, kPitchTransposeParams, currentParamIdx, direction)
               : SelectAdjacentFromList(this, kPitchOctaverParams, currentParamIdx, direction);
    case EVoLumEffectFocus::COMP: return SelectAdjacentFromList(this, kCompParams, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM1: return SelectAdjacentFromList(this, kPreNam1Params, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM2: return SelectAdjacentFromList(this, kPreNam2Params, currentParamIdx, direction);
    case EVoLumEffectFocus::TREMOLO:
      return GetParam(kTremoloMode)->Int() == volum::kVoLumTremoloModeHarmonic
               ? SelectAdjacentFromList(this, kTremoloHarmonicParams, currentParamIdx, direction)
               : SelectAdjacentFromList(this, kTremoloParams, currentParamIdx, direction);
  }
  return false;
}

void NeuralAmpModeler::_SelectVoLumKnob(int paramIdx)
{
  mVolumSelectedKnobParamIdx = paramIdx;
  mVolumSelectedKnobHintText.clear();
  _RememberVoLumKeyboardKnob(paramIdx);

  if (auto* pGfx = GetUI())
  {
    pGfx->ForAllControlsFunc([paramIdx](IControl* pControl) {
      if (auto* pKnob = dynamic_cast<NAMKnobControl*>(pControl))
      {
        pKnob->SetSelectedForKeyboard(pKnob->GetParamIdx() == paramIdx);
      }
    });

    mVolumSelectedKnobHintText = _GetVoLumKnobHintText(paramIdx);

    if (auto* hint = pGfx->GetControlWithTag(kCtrlTagVoLumKeyboardHint))
      hint->As<VoLumKeyboardHintControl>()->SetHintText(mVolumSelectedKnobHintText.c_str());

    _SyncVoLumExactEntry();
  }
}

void NeuralAmpModeler::_ClearVoLumKnobSelection()
{
  mVolumSelectedKnobParamIdx = kNoParameter;
  mVolumSelectedKnobHintText.clear();

  if (auto* pGfx = GetUI())
  {
    pGfx->ForAllControlsFunc([](IControl* pControl) {
      if (auto* pKnob = dynamic_cast<NAMKnobControl*>(pControl))
        pKnob->SetSelectedForKeyboard(false);
    });

    if (auto* hint = pGfx->GetControlWithTag(kCtrlTagVoLumKeyboardHint))
      hint->As<VoLumKeyboardHintControl>()->SetHintText(nullptr);

    _HideVoLumExactEntry();
  }
}

void NeuralAmpModeler::_PromptVoLumKnobExactEntry(int paramIdx, const char* label)
{
  _SelectVoLumKnob(paramIdx);

  if (auto* pGfx = GetUI())
  {
    if (auto* entry = pGfx->GetControlWithTag(kCtrlTagVoLumExactEntry))
    {
      auto* exact = entry->As<VoLumExactEntryControl>();
      exact->ShowForParam(paramIdx, label);
      exact->StartEntry();
    }
  }
}

bool NeuralAmpModeler::_HandleVoLumSelectedKnobKey(const IKeyPress& key)
{
  if (mVolumSelectedKnobParamIdx == kNoParameter)
    return false;

  if (auto* pGfx = GetUI())
  {
    if (auto* pControl = pGfx->GetControlWithParamIdx(mVolumSelectedKnobParamIdx))
    {
      if (auto* pKnob = dynamic_cast<NAMKnobControl*>(pControl))
      {
        const bool handled = pKnob->HandleKeyboardInput(key);

        if (handled && key.VK == kVK_ESCAPE)
        {
          mVolumSelectedKnobParamIdx = kNoParameter;
          _UpdateVoLumKeyboardFocusHint();
        }

        if (handled)
          _SyncVoLumExactEntry();

        return handled;
      }
    }
  }

  mVolumSelectedKnobParamIdx = kNoParameter;
  return false;
}

void NeuralAmpModeler::_SyncVoLumExactEntry()
{
  if (auto* pGfx = GetUI())
  {
    if (auto* entry = pGfx->GetControlWithTag(kCtrlTagVoLumExactEntry))
    {
      auto* exact = entry->As<VoLumExactEntryControl>();
      if (!exact)
        return;

      exact->SyncTextEntryState();

      if (mVolumSelectedKnobParamIdx == kNoParameter)
      {
        exact->Hide(true);
        return;
      }

      if (exact->IsHidden())
        return;

      if (auto* pControl = pGfx->GetControlWithParamIdx(mVolumSelectedKnobParamIdx))
      {
        if (auto* pKnob = dynamic_cast<NAMKnobControl*>(pControl))
          exact->ShowForParam(mVolumSelectedKnobParamIdx, pKnob->GetKeyboardLabel());
      }
    }
  }
}

void NeuralAmpModeler::_HideVoLumExactEntry()
{
  if (auto* pGfx = GetUI())
  {
    if (auto* entry = pGfx->GetControlWithTag(kCtrlTagVoLumExactEntry))
    {
      if (auto* exact = entry->As<VoLumExactEntryControl>())
        exact->Hide(true);
    }
  }
}

