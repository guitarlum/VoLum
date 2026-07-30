// VoLumLayoutRuntime.inc.cpp: _HideControlGroup + _UpdateVoLumLayout runtime layout member functions
// Extracted from NeuralAmpModeler.cpp for file-size hygiene. Tail-#included
// into the NeuralAmpModeler translation unit; not a separate build target.

void NeuralAmpModeler::_HideControlGroup(iplug::igraphics::IGraphics* pGfx, const char* group, bool hide)
{
  if (pGfx)
  {
    pGfx->ForAllControlsFunc([group, hide](iplug::igraphics::IControl* c) {
      if (c->GetGroup() && std::strcmp(c->GetGroup(), group) == 0)
      {
        c->Hide(hide);
      }
    });
  }
}

void NeuralAmpModeler::_UpdateVoLumLayout(iplug::igraphics::IGraphics* pGfx)
{
  if (!pGfx)
    pGfx = GetUI();
  if (pGfx)
  {
    _HideControlGroup(pGfx, "AMP_KNOBS", true);
    _HideControlGroup(pGfx, "SUPPORT_AMP_KNOBS", true);
    _HideControlGroup(pGfx, "REVERB_KNOBS", true);
    _HideControlGroup(pGfx, "REVERB_SHIMMER", true);
    _HideControlGroup(pGfx, "REVERB_PREDELAY", true);
    _HideControlGroup(pGfx, "REVERB_SUBTOGGLE", true);
    _HideControlGroup(pGfx, "REVERB_POWER", true);
    _HideControlGroup(pGfx, "DELAY_KNOBS", true);
    _HideControlGroup(pGfx, "DELAY_TIME", true);
    _HideControlGroup(pGfx, "DELAY_DIV", true);
    _HideControlGroup(pGfx, "DELAY_SYNC", true);
    _HideControlGroup(pGfx, "DELAY_PINGPONG", true);
    _HideControlGroup(pGfx, "DELAY_POWER", true);
    _HideControlGroup(pGfx, "TREMOLO_KNOBS", true);
    _HideControlGroup(pGfx, "TREMOLO_RATE", true);
    _HideControlGroup(pGfx, "TREMOLO_DIV", true);
    _HideControlGroup(pGfx, "TREMOLO_XOVER", true);
    _HideControlGroup(pGfx, "TREMOLO_SYNC", true);
    _HideControlGroup(pGfx, "TREMOLO_POWER", true);
    _HideControlGroup(pGfx, "COMP_KNOBS", true);
    _HideControlGroup(pGfx, "COMP_POWER", true);
    _HideControlGroup(pGfx, "PRE_NAM1_KNOBS", true);
    _HideControlGroup(pGfx, "PRE_NAM1_POWER", true);
    _HideControlGroup(pGfx, "PRE_NAM2_KNOBS", true);
    _HideControlGroup(pGfx, "PRE_NAM2_POWER", true);
    _HideControlGroup(pGfx, "PITCH_POWER", true);
    _HideControlGroup(pGfx, "PITCH_MODE_PICKER", true);
    _HideControlGroup(pGfx, "PITCH_TRANSPOSE_KNOBS", true);
    _HideControlGroup(pGfx, "PITCH_OCTAVER_KNOBS", true);
    _HideControlGroup(pGfx, "PITCH_VOICING", true);
    _HideControlGroup(pGfx, "PITCH_TRANSCHAR", true);
    _HideControlGroup(pGfx, "MAIN_LANE_TOGGLES", true);
    _HideControlGroup(pGfx, "SUPPORT_LANE_TOGGLES", true);
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumPreCaptureMenu))
      menu->Hide(true);
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumSupportAmpMenu))
      menu->Hide(true);

    // Hide/show the correct group based on focused effect. Pedal bypass affects
    // DSP only: visible controls stay editable so users can prepare settings
    // (including by mouse wheel) before engaging the block.
    switch (mVolumFocusedEffect)
    {
      case EVoLumEffectFocus::AMP:
      {
        const bool dualActive = GetParam(kDualAmpActive)->Bool();
        if (!dualActive)
          mVolumDualAmpFocusedSupport = false;
        const bool supportFocus = dualActive && mVolumDualAmpFocusedSupport;
        _HideControlGroup(pGfx, supportFocus ? "SUPPORT_AMP_KNOBS" : "AMP_KNOBS", false);
        break;
      }
      case EVoLumEffectFocus::REVERB:
      {
        const int reverbMode = GetParam(kReverbMode)->Int();
        const bool isHall = reverbMode == volum::kVoLumReverbModeHall;
        const bool isPlate = reverbMode == volum::kVoLumReverbModePlate;
        const bool isOktaverb = reverbMode == volum::kVoLumReverbModeOktaverb;
        _HideControlGroup(pGfx, "REVERB_POWER", false);
        _HideControlGroup(pGfx, "REVERB_KNOBS", false);
        _HideControlGroup(pGfx, "REVERB_PREDELAY", false);
        _HideControlGroup(pGfx, "REVERB_SHIMMER", !isOktaverb);
        // 3-way sub-mode pill is only visible for Oktaverb modes.
        _HideControlGroup(pGfx, "REVERB_SUBTOGGLE", !isOktaverb);
        if (mVolumReverbSubModePill)
        {
          if (isOktaverb)
            mVolumReverbSubModePill->SetLabels({"HALO", "SHIMMER", "BLOOM"});
        }
        (void)isHall;
        (void)isPlate;
        break;
      }
      case EVoLumEffectFocus::DELAY:
      {
        const int delayMode = GetParam(kDelayMode)->Int();
        const bool isReverse = delayMode == volum::kVoLumDelayModeReverse;
        const bool delaySync = GetParam(kDelaySync)->Bool();
        _HideControlGroup(pGfx, "DELAY_POWER", false);
        _HideControlGroup(pGfx, "DELAY_KNOBS", false);
        _HideControlGroup(pGfx, "DELAY_SYNC", false);
        // TIME knob and the DIVISION stepper share slot 1: one shows at a time.
        _HideControlGroup(pGfx, "DELAY_TIME", delaySync);
        _HideControlGroup(pGfx, "DELAY_DIV", !delaySync);
        if (mVolumDelayDivStep)
          mVolumDelayDivStep->SetChannels(
            {"1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16"}, GetParam(kDelayDivision)->Int());
        // Ping-pong has no meaning for reversed taps; hide that control row when Reverse.
        _HideControlGroup(pGfx, "DELAY_PINGPONG", isReverse);
        // The shared kDelayAge slot does meaningfully different things per mode. Swap the
        // visible label and the knob/value tooltip so the user can read what the knob does
        // without having to consult the design guide.
        const char* ageLabel = "AGE";
        const char* ageTip = "Adds character to the delay tail (effect varies by mode).";
        switch (delayMode)
        {
          case volum::kVoLumDelayModeDigital:
            ageLabel = "GRIT";
            ageTip =
              "Digital mode: adds bit-crush quantisation and a tape-machine noise "
              "floor on top of the repeats. At 0 the wet signal is bit-perfect.";
            break;
          case volum::kVoLumDelayModeAnalog:
            ageLabel = "WEAR";
            ageTip =
              "Analog mode: increases BBD chorus depth, HF darkness and compander "
              "softness. 0.5 is classic Memory Man, 1.0 is heavy chorused wear.";
            break;
          case volum::kVoLumDelayModeReverse:
            ageLabel = "BLOOM";
            ageTip =
              "Reverse mode: softens the old edge-faded reverse slice toward a "
              "smooth sin^2 swell. Higher = more pad-like bloom.";
            break;
          default: break;
        }
        if (mVolumDelayAgeLabel)
          mVolumDelayAgeLabel->SetLabel(ageLabel);
        if (mVolumDelayAgeKnob)
          mVolumDelayAgeKnob->SetTooltip(ageTip);
        if (mVolumDelayAgeValue)
          mVolumDelayAgeValue->SetTooltip(ageTip);
        break;
      }
      case EVoLumEffectFocus::PITCH:
      {
        const bool isTranspose = GetParam(kPrePitchMode)->Int() == volum::kVoLumPitchModeTranspose;
        _HideControlGroup(pGfx, "PITCH_POWER", false);
        _HideControlGroup(pGfx, "PITCH_MODE_PICKER", false);
        _HideControlGroup(pGfx, "PITCH_TRANSPOSE_KNOBS", !isTranspose);
        _HideControlGroup(pGfx, "PITCH_OCTAVER_KNOBS", isTranspose);
        // Vintage/Modern voicing only applies to the Octaver engine; Drop/Instant
        // character only applies to Transpose (they share the same slot).
        _HideControlGroup(pGfx, "PITCH_VOICING", isTranspose);
        _HideControlGroup(pGfx, "PITCH_TRANSCHAR", !isTranspose);
        break;
      }
      case EVoLumEffectFocus::COMP:
        _HideControlGroup(pGfx, "COMP_POWER", false);
        _HideControlGroup(pGfx, "COMP_KNOBS", false);
        break;
      case EVoLumEffectFocus::PRE_NAM1:
        _HideControlGroup(pGfx, "PRE_NAM1_POWER", false);
        _HideControlGroup(pGfx, "PRE_NAM1_KNOBS", false);
        break;
      case EVoLumEffectFocus::PRE_NAM2:
        _HideControlGroup(pGfx, "PRE_NAM2_POWER", false);
        _HideControlGroup(pGfx, "PRE_NAM2_KNOBS", false);
        break;
      case EVoLumEffectFocus::TREMOLO:
      {
        const bool sync = GetParam(kTremoloSync)->Bool();
        const bool harmonic = GetParam(kTremoloMode)->Int() == volum::kVoLumTremoloModeHarmonic;
        _HideControlGroup(pGfx, "TREMOLO_POWER", false);
        _HideControlGroup(pGfx, "TREMOLO_KNOBS", false);
        _HideControlGroup(pGfx, "TREMOLO_SYNC", false);
        // RATE knob and the DIVISION stepper share slot 1: one shows at a time.
        _HideControlGroup(pGfx, "TREMOLO_RATE", sync);
        _HideControlGroup(pGfx, "TREMOLO_DIV", !sync);
        // Band-split CROSSOVER knob only matters in Harmonic mode.
        _HideControlGroup(pGfx, "TREMOLO_XOVER", !harmonic);
        if (mVolumTremoloDivStep)
          mVolumTremoloDivStep->SetChannels(
            {"1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16"}, GetParam(kTremoloDivision)->Int());
        break;
      }
    }

    bool ampExpanded = (mVolumExpandedSection == EVoLumSection::AMP);
    const bool dualActiveNow = GetParam(kDualAmpActive)->Bool();
    // Before the snapshot, not after: _VolumApplyDualAmpFocus below clamps focus off a
    // SUPPORT lane that has no amp, and reading the flag first left that lane's toggles
    // on screen for a whole pass while focus, the hint bar and the arrow keys had
    // already moved back to MAIN. Idempotent, so clamping twice costs nothing.
    _VolumClampSupportFocus();
    const bool supportFocusNow = dualActiveNow && mVolumDualAmpFocusedSupport;
    _HideControlGroup(pGfx, "MAIN_LANE_TOGGLES", !ampExpanded || supportFocusNow);
    _HideControlGroup(pGfx, "SUPPORT_LANE_TOGGLES", !ampExpanded || !supportFocusNow);

    // Stereo OUT meter: right-channel bar is only visible when dual amp is active.
    if (auto* meterR = pGfx->GetControlWithTag(kCtrlTagOutputMeterR))
      meterR->Hide(!dualActiveNow);
    _VolumApplyDualAmpFocus();

    if (auto* hero = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
    {
      auto* heroImage = hero->As<VoLumHeroImageControl>();
      heroImage->SetDualAmpState(
        GetParam(kDualAmpActive)->Bool(), mVolumDualAmpFocusedSupport, GetParam(kSupportAmpIdx)->Int());
      const auto& customAmps = volum::custom::MockCustomAmps();
      if (mVolumCustomSupportIdx >= 0 && mVolumCustomSupportIdx < static_cast<int>(customAmps.size()))
        heroImage->SetSupportCustom(true, volum::custom::CustomAmpArt(mVolumCustomSupportIdx),
                                    customAmps[(size_t)mVolumCustomSupportIdx].c_str());
      else
        heroImage->SetSupportCustom(false, 0, "");
      hero->Hide(!ampExpanded);
    }

    // Update Sub-row text
    if (auto* subTextCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
    {
      auto* subText = subTextCtrl->As<VoLumSubRowTextControl>();
      if (mVolumExpandedSection == EVoLumSection::AMP)
      {
        if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
        {
          const auto& customAmps = volum::custom::MockCustomAmps();
          const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
          if (mVolumCustomSupportIdx >= 0 && mVolumCustomSupportIdx < static_cast<int>(customAmps.size()))
            subText->SetName(customAmps[(size_t)mVolumCustomSupportIdx].c_str(), true);
          else
            subText->SetName(supportAmpIdx >= 0 && supportAmpIdx < volum::kAmpCount
                               ? volum::kAmps[supportAmpIdx].displayName
                               : "Choose support amp",
                             true);
        }
        else if (mVolumCustomMainIdx >= 0)
        {
          const auto& customAmps = volum::custom::MockCustomAmps();
          if (mVolumCustomMainIdx < static_cast<int>(customAmps.size()))
            subText->SetName(customAmps[(size_t)mVolumCustomMainIdx].c_str(), true);
        }
        else
          subText->SetName(volum::kAmps[mVolumAmpIdx].displayName, true);
      }
      else if (mVolumFocusedEffect == EVoLumEffectFocus::PITCH)
        subText->SetName("Pitch", false);
      else if (mVolumFocusedEffect == EVoLumEffectFocus::COMP)
        subText->SetName("Compressor", false);
      else if (mVolumFocusedEffect == EVoLumEffectFocus::PRE_NAM1)
        subText->SetName("NAM Pedal 1", false);
      else if (mVolumFocusedEffect == EVoLumEffectFocus::PRE_NAM2)
        subText->SetName("NAM Pedal 2", false);
      else if (mVolumFocusedEffect == EVoLumEffectFocus::REVERB)
        subText->SetName("Reverb", false);
      else if (mVolumFocusedEffect == EVoLumEffectFocus::DELAY)
        subText->SetName("Delay", false);
      else if (mVolumFocusedEffect == EVoLumEffectFocus::TREMOLO)
        subText->SetName("Tremolo", false);
    }

    // Inform the Triptych of the current states
    if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych))
    {
      auto* trip = tripCtrl->As<VoLumTriptychControl>();
      const bool preActive = GetParam(kPrePitchActive)->Bool() || GetParam(kPreCompActive)->Bool()
                             || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
      trip->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), mVolumAmpIdx,
                     _VolumMainAmpDisplayName(),
                     _VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), "NAM 1"),
                     _VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), "NAM 2"));
      trip->SetExpandedSection(mVolumExpandedSection);

      // Update Pedal Cards visibility, layout, and state based on whether POST is expanded
      bool preExpanded = (mVolumExpandedSection == EVoLumSection::PRE);
      bool postExpanded = (mVolumExpandedSection == EVoLumSection::POST);

      if (preExpanded)
      {
        const IRECT tripBounds = trip->GetRECT();
        const auto frames =
          volum::triptych_layout::ComputeFrames(volum::triptych_layout::FromRect(tripBounds), EVoLumSection::PRE);
        const auto cards = volum::triptych_layout::ComputePreCards(frames.pre);

        if (auto* pitchCard = pGfx->GetControlWithTag(kCtrlTagVoLumPitchCard))
          pitchCard->SetTargetAndDrawRECTs(cards.pitch.As<IRECT>());
        if (auto* compCard = pGfx->GetControlWithTag(kCtrlTagVoLumCompCard))
          compCard->SetTargetAndDrawRECTs(cards.comp.As<IRECT>());
        if (auto* preCard = pGfx->GetControlWithTag(kCtrlTagVoLumPreNam1Card))
          preCard->SetTargetAndDrawRECTs(cards.nam1.As<IRECT>());
        if (auto* preCard = pGfx->GetControlWithTag(kCtrlTagVoLumPreNam2Card))
          preCard->SetTargetAndDrawRECTs(cards.nam2.As<IRECT>());
        if (auto* link = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector1))
          link->SetTargetAndDrawRECTs(cards.connector1.As<IRECT>());
        if (auto* link = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector2))
          link->SetTargetAndDrawRECTs(cards.connector2.As<IRECT>());
        if (auto* link = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector3))
          link->SetTargetAndDrawRECTs(cards.connector3.As<IRECT>());
      }

      if (postExpanded)
      {
        const IRECT tripBounds = trip->GetRECT();
        const auto frames =
          volum::triptych_layout::ComputeFrames(volum::triptych_layout::FromRect(tripBounds), EVoLumSection::POST);
        const auto cards = volum::triptych_layout::ComputePostCards(frames.post);

        if (auto* delayCard = pGfx->GetControlWithTag(kCtrlTagVoLumDelayCard))
          delayCard->SetTargetAndDrawRECTs(cards.delay.As<IRECT>());
        if (auto* reverbCard = pGfx->GetControlWithTag(kCtrlTagVoLumReverbCard))
          reverbCard->SetTargetAndDrawRECTs(cards.reverb.As<IRECT>());
        if (auto* tremoloCard = pGfx->GetControlWithTag(kCtrlTagVoLumTremoloCard))
          tremoloCard->SetTargetAndDrawRECTs(cards.tremolo.As<IRECT>());
        if (auto* linkCard = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector))
          linkCard->SetTargetAndDrawRECTs(cards.connector1.As<IRECT>());
        if (auto* linkCard = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector2))
          linkCard->SetTargetAndDrawRECTs(cards.connector2.As<IRECT>());
      }

      if (auto* delayCard = pGfx->GetControlWithTag(kCtrlTagVoLumDelayCard))
      {
        delayCard->Hide(!postExpanded);
        if (postExpanded)
        {
          auto* card = delayCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::DELAY);
          card->SetActiveState(GetParam(kDelayActive)->Bool());
        }
      }
      if (auto* reverbCard = pGfx->GetControlWithTag(kCtrlTagVoLumReverbCard))
      {
        reverbCard->Hide(!postExpanded);
        if (postExpanded)
        {
          auto* card = reverbCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::REVERB);
          card->SetActiveState(GetParam(kReverbActive)->Bool());
        }
      }
      if (auto* tremoloCard = pGfx->GetControlWithTag(kCtrlTagVoLumTremoloCard))
      {
        tremoloCard->Hide(!postExpanded);
        if (postExpanded)
        {
          auto* card = tremoloCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::TREMOLO);
          card->SetActiveState(GetParam(kTremoloActive)->Bool());
        }
      }
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector))
      {
        chain->Hide(!postExpanded);
      }
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector2))
      {
        chain->Hide(!postExpanded);
      }
      if (auto* pitchCard = pGfx->GetControlWithTag(kCtrlTagVoLumPitchCard))
      {
        pitchCard->Hide(!preExpanded);
        if (preExpanded)
        {
          auto* card = pitchCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::PITCH);
          card->SetActiveState(GetParam(kPrePitchActive)->Bool());
        }
      }
      if (auto* compCard = pGfx->GetControlWithTag(kCtrlTagVoLumCompCard))
      {
        compCard->Hide(!preExpanded);
        if (preExpanded)
        {
          auto* card = compCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::COMP);
          card->SetActiveState(GetParam(kPreCompActive)->Bool());
        }
      }
      if (auto* preCard = pGfx->GetControlWithTag(kCtrlTagVoLumPreNam1Card))
      {
        preCard->Hide(!preExpanded);
        if (preExpanded)
        {
          auto* card = preCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::PRE_NAM1);
          card->SetActiveState(GetParam(kPreNam1Active)->Bool());
        }
      }
      if (auto* preCard = pGfx->GetControlWithTag(kCtrlTagVoLumPreNam2Card))
      {
        preCard->Hide(!preExpanded);
        if (preExpanded)
        {
          auto* card = preCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::PRE_NAM2);
          card->SetActiveState(GetParam(kPreNam2Active)->Bool());
        }
      }
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector1))
        chain->Hide(!preExpanded);
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector2))
        chain->Hide(!preExpanded);
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector3))
        chain->Hide(!preExpanded);
    }
  }
}
