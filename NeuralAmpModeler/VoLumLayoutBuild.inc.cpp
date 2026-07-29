// VoLumLayoutBuild.inc.cpp: _BuildVoLumLayout - the full one-time UI build/attach
// pass. Extracted verbatim from the constructor's mLayoutFunc lambda body for
// file-size hygiene; tail-#included into the NeuralAmpModeler TU (not a separate
// build target). Behaviour is identical: the lambda now just forwards here.

void NeuralAmpModeler::_BuildVoLumLayout(IGraphics* pGraphics)
{
    // Diagonal, aspect-locked scaling via the bottom-right corner grip. This is
    // the only resize affordance (standalone and plugin alike): the standalone
    // window is intentionally a fixed, non-sizable frame (see main.rc), so there
    // is no native edge-resize and therefore no off-aspect letterbox/white-frame.
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    // Opt-in perf overlay (frame time / FPS) for profiling UI smoothness.
    // Launch the standalone with VOLUM_SHOW_FPS=1 to enable; off by default.
    if (const char* fps = std::getenv("VOLUM_SHOW_FPS"); fps && fps[0] && fps[0] != '0')
      pGraphics->ShowFPSDisplay(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);
    pGraphics->LoadFont("Poiret-One", POIRETONE_FN);
#ifdef OS_WIN
    // NanoVG/GL2 on Windows renders these small Josefin caps thinner than macOS/Metal,
    // so load one weight heavier there to match the macOS readability.
    pGraphics->LoadFont("Josefin-Sans", JOSEFINSANS_BOLD_FN);
    pGraphics->LoadFont("Josefin-Bold", JOSEFINSANS_BOLD_HEAVY_FN);
#else
    pGraphics->LoadFont("Josefin-Sans", JOSEFINSANS_FN);
    pGraphics->LoadFont("Josefin-Bold", JOSEFINSANS_BOLD_FN);
#endif

    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);

    const auto b = pGraphics->GetBounds();

    // VoLum: Variant F UI layout (sidebar, triptych, hero, knob row)
    // Sidebar trimmed (amp-list names never used the full width); the reclaimed
    // space becomes margin so the widened triptych is not cramped against it.
    const float sidebarW = 178.f;
    const float mainL = b.L + sidebarW;
    const float mainR = b.R;
    const float mainW = mainR - mainL;
    const float mainCX = mainL + mainW / 2.f;

    pGraphics->AttachControl(new VoLumBackgroundControl(b, sidebarW));
    pGraphics->AttachControl(new VoLumKnobSelectionClearControl(IRECT(mainL, b.T, mainR, b.B), [this]() {
      _ClearVoLumKnobSelection();
      _VolumHidePreCaptureMenu();
      _VolumHideSupportAmpMenu();
      if (auto* pGfx = GetUI())
      {
        if (auto* ir = pGfx->GetControlWithTag(kCtrlTagVoLumIrMenu))
          ir->Hide(true);
        if (auto* pm = pGfx->GetControlWithTag(kCtrlTagVoLumPresetMenu))
          pm->Hide(true);
      }
    }));

    // Sidebar: logo
    const IRECT logoArea(b.L, b.T + 8.f, b.L + sidebarW, b.T + 48.f);
    pGraphics->AttachControl(new VoLumLogoControl(logoArea));

    // Sidebar: amp list (names + abbreviations from catalog)
    static const char* ampNames[volum::kAmpCount];
    static const char* ampAbbrs[volum::kAmpCount] = {
      "A1", "BC", "BX", "DH", "FD", "HK", "LP", "M4", "MJ", "MV", "O1", "O2", "ST", "SL", "TC"};
    for (int i = 0; i < volum::kAmpCount; i++)
      ampNames[i] = volum::kAmps[i].displayName;

    const IRECT ampListArea(b.L + 6.f, logoArea.B + 4.f, b.L + sidebarW - 6.f, b.B - 8.f);
    pGraphics->AttachControl(
      new VoLumAmpListControl(
        ampListArea, volum::kAmpCount, ampNames, ampAbbrs,
        [this](int ampIdx) {
          _VolumSaveCurrentToSettings();
          mVolumAmpIdx = ampIdx;
          mVolumCustomMainIdx = -1; // back on a factory amp
          _VolumRestoreFromSettings(ampIdx);
          _VolumRefreshChannels();
          mVolumNeedsLoad.store(true);
#ifdef APP_API
          // Coalesce the disk write: OnIdle() flushes mVolumSettingsDirty.
          // Writing synchronously here serialized all amps + dual-amp state
          // and atomically wrote two JSON files on every selection, which
          // stalled the UI thread (very visible on held arrow-key repeats).
          mVolumSettingsDirty = true;
#endif

          auto* pGfx = GetUI();
          if (!pGfx)
            return;
          auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage)->As<VoLumHeroImageControl>();
          auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText)->As<VoLumSubRowTextControl>();
          if (nameCtrl && mVolumExpandedSection == EVoLumSection::AMP)
            nameCtrl->SetName(volum::kAmps[ampIdx].displayName, true);
          if (heroCtrl)
          {
            char ph[4] = {volum::kAmps[ampIdx].displayName[0], (char)('0' + (ampIdx % 10)), 0, 0};
            heroCtrl->SetPlaceholder(ph, ampIdx);
            heroCtrl->SetName(volum::kAmps[ampIdx].displayName);
          }
          // Restore the factory cab labels (a custom amp may have overridden them).
          if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
            spk->As<VoLumSpeakerRowControl>()->SetFactoryCabs();

          // F5: refresh the header preset strip to this amp's preset bank.
          _VolumSyncPresetOwner();
          _VolumRefreshPresetBar();

          if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych))
          {
            auto* trip = tripCtrl->As<VoLumTriptychControl>();
            const bool preActive =
              GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
            trip->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), ampIdx,
                           volum::kAmps[ampIdx].displayName,
                           _VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), "NAM 1"),
                           _VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), "NAM 2"));
            mVolumPreLockUiDirty = mVolumPreLocked && _VolumIsPreDirty();
            mVolumPostLockUiDirty = mVolumPostLocked && _VolumIsPostDirty();
            trip->SetDirty(false);
          }
        }),
      kCtrlTagVoLumAmpList);

    // F6: populate the sidebar CUSTOM section (custom amps render as real list
    // entries below the factory amps) and wire its +/edit/delete affordances.
    if (auto* ampListCtrl = pGraphics->GetControlWithTag(kCtrlTagVoLumAmpList))
    {
      auto* ampList = ampListCtrl->As<VoLumAmpListControl>();
      ampList->SetCustomAmps(volum::custom::MockCustomAmps(), volum::custom::MockCustomAmpArts());
      ampList->SetCustomCallbacks(
        // select a custom amp (mock): drive the hero/preset strip only
        [this](int customIdx) { _VolumSelectCustomAmp(customIdx); },
        // + add a custom amp -> open the free-form builder
        [this]() {
          if (auto* pGfx = GetUI())
            if (auto* ov = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
              ov->As<VoLumCustomOverlayControl>()->ShowBuilder(false, nullptr);
        },
        // pen: edit an existing custom amp -> reopen the builder
        [this](int customIdx) {
          const auto& names = volum::custom::MockCustomAmps();
          const char* nm = (customIdx >= 0 && customIdx < (int)names.size()) ? names[customIdx].c_str() : nullptr;
          if (auto* pGfx = GetUI())
            if (auto* ov = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
              ov->As<VoLumCustomOverlayControl>()->ShowBuilder(true, nm, customIdx);
        },
        // bin: confirm, then delete the custom amp from the live session list.
        [this](int customIdx) {
          auto* pGfx = GetUI();
          if (!pGfx)
            return;
          const auto& names = volum::custom::MockCustomAmps();
          const std::string nm =
            (customIdx >= 0 && customIdx < (int)names.size()) ? names[(size_t)customIdx] : std::string();
          auto doDelete = [this, customIdx]() {
            volum::custom::RemoveCustomAmp(customIdx);
            auto* pGfx2 = GetUI();
            if (!pGfx2)
              return;
            if (auto* al = pGfx2->GetControlWithTag(kCtrlTagVoLumAmpList))
            {
              auto* list = al->As<VoLumAmpListControl>();
              list->SetCustomAmps(volum::custom::MockCustomAmps(), volum::custom::MockCustomAmpArts());
              list->SetCustomSelected(-1);
            }
            // Selection cleared -> revert the hero/name from the (now-deleted)
            // custom amp back to the active factory amp.
            if (auto* heroCtrl = pGfx2->GetControlWithTag(kCtrlTagVoLumHeroImage))
            {
              auto* h = heroCtrl->As<VoLumHeroImageControl>();
              char ph[4] = {volum::kAmps[mVolumAmpIdx].displayName[0], (char)('0' + (mVolumAmpIdx % 10)), 0, 0};
              h->SetPlaceholder(ph, mVolumAmpIdx);
              h->SetName(volum::kAmps[mVolumAmpIdx].displayName);
            }
            if (auto* nameCtrl = pGfx2->GetControlWithTag(kCtrlTagVoLumSubRowText))
              if (mVolumExpandedSection == EVoLumSection::AMP)
                nameCtrl->As<VoLumSubRowTextControl>()->SetName(volum::kAmps[mVolumAmpIdx].displayName, true);
            // The deleted custom amp may have been the focused main; fall back to
            // the active factory amp so the preset bar shows the right bank.
            if (mVolumCustomMainIdx == customIdx)
              mVolumCustomMainIdx = -1;
            else if (mVolumCustomMainIdx > customIdx)
              --mVolumCustomMainIdx;
            // The deleted amp may also have been the dual SUPPORT partner; keep
            // mVolumCustomSupportIdx valid (RemoveCustomAmp already drops the
            // supportCustomId references in stored scenes).
            if (mVolumCustomSupportIdx == customIdx)
              mVolumCustomSupportIdx = -1;
            else if (mVolumCustomSupportIdx > customIdx)
              --mVolumCustomSupportIdx;
            _VolumSyncPresetOwner();
            _VolumRefreshPresetBar();
          };
          if (auto* dlg = pGfx->GetControlWithTag(kCtrlTagVoLumConfirm))
            dlg->As<VoLumConfirmDialogControl>()->Show(
              "Delete?", "Delete custom amp \"" + nm + "\"? This cannot be undone.", doDelete);
          else
            doDelete();
        });
    }

    // Vertically center the detail content in the right panel
    const float speakerH = 48.f;
    const float heroW = 434.f;
    const float heroH = 166.f;
    // Amp name block: title + gold rule + diamond; needs enough H so the rule is not clipped.
    const float nameH = 54.f;
    const float gapAfterAmpName = 12.f;
    const float ampToKnobHairlineH = 2.f;
    const float gapAfterHairline = 16.f;
    const float knobDiam = 58.f;
    const float labelH = 20.f;
    const float valueH = 18.f;
    const float toggleH = 34.f;
    const float hintH = 44.f;
    const float hintGap = 10.f;
    const float footerH = 18.f;

    const float contentH = speakerH + 6.f + heroH + 4.f + nameH + gapAfterAmpName + ampToKnobHairlineH
                           + gapAfterHairline + labelH + knobDiam + valueH + 2.f + 10.f + toggleH + hintGap + hintH
                           + 6.f + footerH;
    const float contentTop = b.T + (b.H() - contentH) / 2.f;

    // Speaker mode row
    float yPos = contentTop;
    const IRECT speakerArea(mainL, yPos, mainR, yPos + speakerH);
    pGraphics->AttachControl(
      new VoLumSpeakerRowControl(
        speakerArea,
        [this](int speakerIdx) {
          // Custom amp focused (MAIN or the custom SUPPORT partner): the cab row
          // is display-only. Switching cabs just retargets that lane's channel
          // stepper (no model load).
          const bool supportFocus = GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport;
          const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
          // Picking any baked cab retires the focused lane's active Custom IR: the
          // two cab sources are mutually exclusive. Without this the IR keeps forcing
          // the amp to DIRECT/No-Cab on the next reconcile, so the chosen cab looked
          // like it was never remembered (item: custom amp forgets its speaker).
          const std::string& laneIrId =
            supportFocus ? _VolumActiveScene().supportActiveIrId : _VolumActiveScene().activeIrId;
          if (!laneIrId.empty())
          {
            // Only wait for a capture that is actually coming. An active IR already
            // holds the lane on DIRECT, so picking No Cab reuses the live capture and
            // nothing gets staged - deferring there would hold the IR for the whole
            // bounded wait and make the switch feel stuck.
            const int newSlot = (speakerIdx == 0) ? volum::custom::kDirectSlot : (speakerIdx - 1);
            const bool captureChanges =
              customLane >= 0
                ? newSlot
                    != (supportFocus ? mVolumCustomSupportSlot
                                     : ((mVolumSpeakerIdx == 0) ? volum::custom::kDirectSlot : (mVolumSpeakerIdx - 1)))
                : speakerIdx != (supportFocus ? GetParam(kSupportSpeakerIdx)->Int() : mVolumSpeakerIdx);
            _VolumClearIR(supportFocus, /*deferToCabSwap=*/captureChanges);
          }
          if (customLane >= 0)
          {
            // Channel-first: the row only enables cabs that carry the current gain
            // stage, so a cab pick keeps the channel and just retargets the slot.
            // _VolumApplyCustomMainCabs re-gates the row/stepper and stages the .nam.
            const int slot = (speakerIdx == 0) ? volum::custom::kDirectSlot : (speakerIdx - 1);
            if (supportFocus)
              mVolumCustomSupportSlot = slot;
            else
              mVolumSpeakerIdx = speakerIdx; // gain stage (channelIdx) unchanged
            _VolumApplyCustomMainCabs(customLane, supportFocus);
            mVolumSettingsDirty = true;
            _VolumMarkPresetDirty();
            return;
          }
          // Per-amp cab: when SUPPORT lane is focused while Dual Amp is on, the speaker
          // row drives the support lane's cab; otherwise it drives the MAIN lane.
          if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
          {
            GetParam(kSupportSpeakerIdx)->Set(speakerIdx);
            SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
            mVolumSettingsDirty = true;
            _VolumRefreshSupportChannels();
            mVolumSupportNeedsLoad.store(true);
          }
          else
          {
            mVolumSpeakerIdx = speakerIdx;
            mVolumAmpSettings[mVolumAmpIdx].speakerIdx = speakerIdx;
            mVolumSettingsDirty = true;
            _VolumRefreshChannels();
            mVolumNeedsLoad.store(true);
          }
          _VolumMarkPresetDirty();
        }),
      kCtrlTagVoLumSpeakerRow);

    // F7: the speaker row's IR button opens the anchored Custom IR cab dropdown.
    if (auto* spkCtrl = pGraphics->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      spkCtrl->As<VoLumSpeakerRowControl>()->SetIrMenuCallback([this](const IRECT& anchor) {
        auto* pGfx = GetUI();
        if (!pGfx)
          return;
        auto* raw = pGfx->GetControlWithTag(kCtrlTagVoLumIrMenu);
        if (!raw)
          return;
        if (!raw->IsHidden())
        {
          raw->Hide(true);
          return;
        }
        auto* menu = raw->As<VoLumListMenuControl>();
        const auto& irs = volum::custom::MockIRLibrary();
        // Rows: each custom IR + a single "Manage custom IRs..." entry. Switching
        // back to a baked cab/DIRECT in the speaker row clears any custom IR, so
        // there is no explicit "no IR" row.
        std::vector<VoLumListMenuControl::Row> rows;
        for (int i = 0; i < (int)irs.size(); i++)
          rows.push_back({irs[(size_t)i], i, false, false});
        rows.push_back({"Manage custom IRs...", VoLumListMenuControl::kManage, true, false});

        const float w = 230.f;
        const auto bounds = pGfx->GetBounds();
        const float top = anchor.B + 4.f;
        const float h = std::min(VoLumListMenuControl::MenuHeight(rows.size()), std::max(110.f, bounds.B - top - 8.f));
        float l = anchor.L;
        if (l + w > bounds.R - 4.f)
          l = bounds.R - 4.f - w;
        menu->SetMenuRect(IRECT(l, top, l + w, top + h));
        // Highlight the focused lane's active IR from its scene id (the shared row
        // chip can lag behind the actual lane state, so trust the scene).
        int selectedIr = VoLumListMenuControl::kNone;
        const std::string& laneIrId =
          _VolumSupportFocused() ? _VolumActiveScene().supportActiveIrId : _VolumActiveScene().activeIrId;
        if (!laneIrId.empty())
        {
          const int idx = volum::custom::IRIndexById(laneIrId);
          if (idx >= 0 && idx < (int)irs.size())
            selectedIr = idx;
        }
        menu->SetRows(rows, selectedIr);
        menu->Hide(false);
      });

    yPos += speakerH + 6.f;

    // Triptych (PRE | AMP | POST)
    const auto triptychBounds = volum::triptych_layout::BoundsForCenter(mainCX, yPos);
    const IRECT triptychArea = triptychBounds.As<IRECT>();

    auto* triptych = new VoLumTriptychControl(triptychArea, [this](EVoLumSection sec, EVoLumEffectFocus focus) {
      mVolumExpandedSection = sec;
      mVolumFocusedEffect = focus;
      _UpdateVoLumLayout();
      _UpdateVoLumKeyboardFocusHint();
    });
    pGraphics->AttachControl(triptych, kCtrlTagVoLumTriptych);

    // Re-attach VoLumHeroImageControl so the procedural art can actually draw
    // The triptych provides a space for it, but the hero control holds the fractal caching logic.
    // It should be centered within the AMP-expanded area of the triptych.
    // When AMP is expanded, the center of the expanded section is exactly at `mainCX`
    const float newHeroW = volum::triptych_layout::kAmpExpandedW;
    const IRECT heroArea(mainCX - newHeroW / 2.f, yPos, mainCX + newHeroW / 2.f, triptychArea.B);
    auto* hero = new VoLumHeroImageControl(
      heroArea,
      [this](bool supportFocused) {
        mVolumDualAmpFocusedSupport = supportFocused;
        mVolumFocusedEffect = EVoLumEffectFocus::AMP;
        _VolumApplyFocusedLaneCabs();
        _UpdateVoLumLayout();
        _UpdateVoLumKeyboardFocusHint();
      },
      [this](const IRECT& anchor) {
        // Only allow the picker when Dual Amp is on; mono mode shouldn't surface a Support amp menu.
        if (GetParam(kDualAmpActive)->Bool())
          _VolumShowSupportAmpMenu(anchor);
      },
      // DUAL chip — toggle the global Dual Amp parameter through the shared funnel
      // (host notify + OnParamChange + mark dirty), then refresh the focus hint.
      [this]() {
        _VolumUserToggleParam(kDualAmpActive);
        _UpdateVoLumKeyboardFocusHint();
      },
      // Dismiss the support-amp dropdown when the user clicks elsewhere on the hero (e.g. on
      // the MAIN panel) so the menu doesn't stay floating after a focus change.
      [this]() { _VolumHideSupportAmpMenu(); },
      // Picker visibility check — lets the hero treat any support-panel click as "close" while
      // the menu is open, regardless of focus state.
      [this]() {
        if (auto* pGfx = GetUI())
          if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumSupportAmpMenu))
            return !menu->IsHidden();
        return false;
      });
    pGraphics->AttachControl(hero, kCtrlTagVoLumHeroImage);

    // PAN knobs live in the bottom-right of each lane's hero panel. Visibility is toggled in
    // _VolumApplyDualAmpFocus — mono mode hides both. They use volumPanKnobStyle which has a
    // transparent background so the knob blends into the hero art instead of punching a square
    // dark patch through it.
    pGraphics->AttachControl(
      new VoLumPanKnobControl(hero->GetMainPanKnobSlot(), kMainAmpPan, volumPanKnobStyle), -1, "MAIN_PAN_KNOB");
    pGraphics->AttachControl(
      new VoLumPanKnobControl(hero->GetSupportPanKnobSlot(), kSupportAmpPan, volumPanKnobStyleSupport), -1,
      "SUPPORT_PAN_KNOB");
    pGraphics->AttachControl(new VoLumSupportPolarityControl(
                               hero->GetSupportPolarityToggleSlot(), [this]() { return mSupportPolarityInvert.load(); },
                               [this]() {
                                 const bool next = !mSupportPolarityInvert.load();
                                 mSupportPolarityInvert.store(next);
                                 mVolumAmpSettings[mVolumAmpIdx].supportPolarityInvert = next;
                                 mVolumSettingsDirty = true;
                                 _VolumMarkPresetDirty();
                                 if (auto* pGfx = GetUI())
                                   pGfx->SetAllControlsDirty();
                               }),
                             -1, "SUPPORT_POLARITY_TOGGLE");

    const auto preCards = volum::triptych_layout::ComputePreCards(
      volum::triptych_layout::ComputeFrames(triptychBounds, EVoLumSection::PRE).pre);
    const auto postCards = volum::triptych_layout::ComputePostCards(
      volum::triptych_layout::ComputeFrames(triptychBounds, EVoLumSection::POST).post);

    auto onPedalClick = [this](VoLumPedalCardControl* card, bool isBypassClick) {
      (void)isBypassClick;
      const EVoLumEffectFocus eff = card->GetEffect();
      mVolumFocusedEffect = eff;
      _UpdateVoLumLayout();
      _UpdateVoLumKeyboardFocusHint();
    };

    auto* delayCard = new VoLumPedalCardControl(postCards.delay.As<IRECT>(), EVoLumEffectFocus::DELAY, onPedalClick);
    auto* reverbCard = new VoLumPedalCardControl(postCards.reverb.As<IRECT>(), EVoLumEffectFocus::REVERB, onPedalClick);
    auto* tremoloCard =
      new VoLumPedalCardControl(postCards.tremolo.As<IRECT>(), EVoLumEffectFocus::TREMOLO, onPedalClick);
    auto* chainLink = new VoLumChainConnectorControl(postCards.connector1.As<IRECT>());
    auto* chainLink2 = new VoLumChainConnectorControl(postCards.connector2.As<IRECT>());
    auto* pitchCard = new VoLumPedalCardControl(preCards.pitch.As<IRECT>(), EVoLumEffectFocus::PITCH, onPedalClick);
    auto* compCard = new VoLumPedalCardControl(preCards.comp.As<IRECT>(), EVoLumEffectFocus::COMP, onPedalClick);
    auto* preNam1Card = new VoLumPedalCardControl(preCards.nam1.As<IRECT>(), EVoLumEffectFocus::PRE_NAM1, onPedalClick);
    auto* preNam2Card = new VoLumPedalCardControl(preCards.nam2.As<IRECT>(), EVoLumEffectFocus::PRE_NAM2, onPedalClick);
    auto* preChainLink1 = new VoLumChainConnectorControl(preCards.connector1.As<IRECT>());
    auto* preChainLink2 = new VoLumChainConnectorControl(preCards.connector2.As<IRECT>());
    auto* preChainLink3 = new VoLumChainConnectorControl(preCards.connector3.As<IRECT>());

    pGraphics->AttachControl(pitchCard, kCtrlTagVoLumPitchCard)->Hide(true);
    pGraphics->AttachControl(preChainLink1, kCtrlTagVoLumPreChainConnector1)->Hide(true);
    pGraphics->AttachControl(compCard, kCtrlTagVoLumCompCard)->Hide(true);
    pGraphics->AttachControl(preChainLink2, kCtrlTagVoLumPreChainConnector2)->Hide(true);
    pGraphics->AttachControl(preNam1Card, kCtrlTagVoLumPreNam1Card)->Hide(true);
    pGraphics->AttachControl(preChainLink3, kCtrlTagVoLumPreChainConnector3)->Hide(true);
    pGraphics->AttachControl(preNam2Card, kCtrlTagVoLumPreNam2Card)->Hide(true);
    pGraphics->AttachControl(delayCard, kCtrlTagVoLumDelayCard)->Hide(true);
    pGraphics->AttachControl(chainLink, kCtrlTagVoLumChainConnector)->Hide(true);
    pGraphics->AttachControl(reverbCard, kCtrlTagVoLumReverbCard)->Hide(true);
    pGraphics->AttachControl(chainLink2, kCtrlTagVoLumChainConnector2)->Hide(true);
    pGraphics->AttachControl(tremoloCard, kCtrlTagVoLumTremoloCard)->Hide(true);

    yPos += volum::triptych_layout::kTriptychH + 4.f;

    // Sub-row text (Replaces Amp Name / Focus Header)
    const IRECT subRowArea(mainL, yPos, mainR, yPos + 54.f);
    pGraphics->AttachControl(new VoLumSubRowTextControl(subRowArea), kCtrlTagVoLumSubRowText);
    yPos += 54.f + 12.f; // Name + rule + gap

    // ---- Knobs: [Channel] | [Input, Gate] | [Bass, Mid, Treble] | [Output] ----
    const float colW = 64.f;
    const float divW = 12.f;
    const float knobRowTop = yPos;
    const float knobT = knobRowTop + 20.f; // labelH
    const float totalW = 7 * colW + 3 * divW + 20.f;
    const float rowLeft = mainCX - totalW / 2.f;

    auto knobX = [&](int slot) -> float {
      float x = rowLeft;
      int dividers = 0;
      if (slot > 0)
        dividers++;
      if (slot > 2)
        dividers++;
      if (slot > 5)
        dividers++;
      return x + slot * colW + dividers * divW;
    };

    auto drawDivider = [&](float afterSlotRight, const char* group) {
      float dx = afterSlotRight + divW / 2.f - 1.f;
      auto ctrl = new VoLumDividerControl(IRECT(dx, knobT + 4.f, dx + 2.f, knobT + knobDiam - 4.f));
      pGraphics->AttachControl(ctrl, -1, group);
    };

    auto drawKnobCol = [&](int slot, const char* label, int paramId, const char* suffix, const char* group,
                           bool center_offset = false, int centerSlots = 3, int centerStart = 2,
                           float centerOffset = 0.f, float centerColW = 80.f, const char* tooltip = nullptr,
                           float knobDiamOverride = 0.f) {
      float customColW = center_offset ? centerColW : colW;
      float cx = center_offset ? (mainCX + centerOffset - (centerSlots * customColW) / 2.f
                                  + (slot - centerStart) * customColW + (customColW / 2.f))
                               : knobX(slot) + (colW / 2.f);
      // Optional smaller knob (e.g. dense 6-knob pitch rows); vertically centered
      // against the standard knob band so the value baseline stays aligned.
      const float effDiam = knobDiamOverride > 0.f ? knobDiamOverride : knobDiam;
      float kL = cx - (effDiam / 2.f);
      float kT = knobT + (knobDiam - effDiam) / 2.f;

      // Use a wider label rect (-40.f to +40.f = 80px wide) to prevent "FEEDBACK" clipping
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(IRECT(cx - 40.f, knobRowTop, cx + 40.f, knobRowTop + 20.f), label), -1, group);
      auto* knob = new VoLumDialKnobControl(
        IRECT(kL, kT, kL + effDiam, kT + effDiam), paramId, "", volumKnobStyle, knobBackgroundBitmap);
      pGraphics->AttachControl(knob, -1, group);
      if (tooltip)
        knob->SetTooltip(tooltip);
      knob->SetSelectedForKeyboard(mVolumSelectedKnobParamIdx == paramId);
      pGraphics->AttachControl(
        new VoLumParamValueControl(
          IRECT(cx - 30.f, knobT + knobDiam + 2.f, cx + 30.f, knobT + knobDiam + 2.f + valueH), paramId, suffix),
        -1, group);
    };

    // AMP KNOBS
    {
      float cx = knobX(0);
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(IRECT(cx, knobRowTop, cx + colW, knobRowTop + 20.f), "CHANNEL", true), -1,
        "AMP_KNOBS");
      float stepH = 28.f;
      float stepTop = knobT + (knobDiam - stepH) / 2.f;
      auto* channelStep =
        new VoLumChannelStepControl(IRECT(cx, stepTop, cx + colW, stepTop + stepH), [this](int newIdx) {
          // When a custom MAIN amp is focused, the stepper lists the amp-WIDE gain
          // stages (channel-first). Selecting a channel snaps the cab to one that
          // carries it (No Cab last resort) and stages the .nam via the shared path.
          const bool supportFocus = GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport;
          if (mVolumCustomMainIdx >= 0 && !supportFocus)
          {
            const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
            const auto chans = volum::custom::AssignedChannels(amp);
            // Clear an orphaned IR before the channel resolves to a non-DIRECT stage.
            if (newIdx >= 0 && newIdx < (int)chans.size() && !_VolumActiveScene().activeIrId.empty()
                && !volum::custom::ChannelHasDirect(amp, chans[(size_t)newIdx]))
              _VolumClearIR(false, /*deferToCabSwap=*/true); // the new channel's capture is about to load
            mVolumChannelIdx = std::clamp(newIdx, 0, std::max(0, (int)chans.size() - 1));
            _VolumApplyCustomMainCabs(mVolumCustomMainIdx, false);
            mVolumSettingsDirty = true;
            _VolumMarkPresetDirty();
            return;
          }
          mVolumChannelIdx = newIdx;
          mVolumAmpSettings[mVolumAmpIdx].channelIdx = newIdx;
          mVolumSettingsDirty = true;
          mVolumNeedsLoad.store(true);
          _VolumMarkPresetDirty();
        });
      channelStep->SetChannels(mVolumChannelLabels, mVolumChannelIdx);
      pGraphics->AttachControl(channelStep, kCtrlTagVoLumChannelStep, "AMP_KNOBS");
    }
    drawDivider(knobX(0) + colW, "AMP_KNOBS");
    drawKnobCol(1, "INPUT", kInputLevel, "dB", "AMP_KNOBS", false);
    drawKnobCol(2, "GATE", kNoiseGateThreshold, "dB", "AMP_KNOBS", false);
    drawDivider(knobX(2) + colW, "AMP_KNOBS");
    drawKnobCol(3, "BASS", kToneBass, "", "AMP_KNOBS", false);
    drawKnobCol(4, "MID", kToneMid, "", "AMP_KNOBS", false);
    drawKnobCol(5, "TREBLE", kToneTreble, "", "AMP_KNOBS", false);
    drawDivider(knobX(5) + colW, "AMP_KNOBS");
    drawKnobCol(6, "OUTPUT", kOutputLevel, "dB", "AMP_KNOBS", false);

    // SUPPORT AMP KNOBS — identical layout to AMP_KNOBS, just bound to support params.
    // Visibility is toggled on lane focus so the user sees one row at a time in the same slots.
    {
      float cx = knobX(0);
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(IRECT(cx, knobRowTop, cx + colW, knobRowTop + 20.f), "CHANNEL", true), -1,
        "SUPPORT_AMP_KNOBS");
      float stepH = 28.f;
      float stepTop = knobT + (knobDiam - stepH) / 2.f;
      auto* supChannelStep =
        new VoLumChannelStepControl(IRECT(cx, stepTop, cx + colW, stepTop + stepH), [this](int newIdx) {
          // Custom SUPPORT partner: the stepper lists the amp-WIDE gain stages
          // (channel-first). Selecting a channel snaps the cab to one that carries
          // it and stages the .nam via the shared path. The custom support loader
          // resolves from mVolumCustomSupportChannel, set inside _VolumApplyCustomMainCabs.
          if (mVolumCustomSupportIdx >= 0)
          {
            const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
            const auto chans = volum::custom::AssignedChannels(amp);
            if (newIdx >= 0 && newIdx < (int)chans.size())
            {
              const int chosen = chans[(size_t)newIdx];
              if (!_VolumActiveScene().supportActiveIrId.empty() && !volum::custom::ChannelHasDirect(amp, chosen))
                _VolumClearIR(true, /*deferToCabSwap=*/true); // the new channel's capture is about to load
              mVolumCustomSupportChannel = chosen; // resolver keeps this channel
            }
            _VolumApplyCustomMainCabs(mVolumCustomSupportIdx, true);
            mVolumSupportNeedsLoad.store(true);
            mVolumSettingsDirty = true;
            _VolumMarkPresetDirty();
            return;
          }
          GetParam(kSupportChannelIdx)->Set(newIdx);
          SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
          mVolumSupportNeedsLoad.store(true);
          mVolumSettingsDirty = true;
          _VolumMarkPresetDirty();
        });
      supChannelStep->SetChannels(mVolumSupportChannelLabels, GetParam(kSupportChannelIdx)->Int());
      pGraphics->AttachControl(supChannelStep, kCtrlTagVoLumSupportChannelStep, "SUPPORT_AMP_KNOBS");
    }
    drawDivider(knobX(0) + colW, "SUPPORT_AMP_KNOBS");
    drawKnobCol(1, "INPUT", kSupportInputLevel, "dB", "SUPPORT_AMP_KNOBS", false);
    drawKnobCol(2, "GATE", kSupportNoiseGateThreshold, "dB", "SUPPORT_AMP_KNOBS", false);
    drawDivider(knobX(2) + colW, "SUPPORT_AMP_KNOBS");
    drawKnobCol(3, "BASS", kSupportToneBass, "", "SUPPORT_AMP_KNOBS", false);
    drawKnobCol(4, "MID", kSupportToneMid, "", "SUPPORT_AMP_KNOBS", false);
    drawKnobCol(5, "TREBLE", kSupportToneTreble, "", "SUPPORT_AMP_KNOBS", false);
    drawDivider(knobX(5) + colW, "SUPPORT_AMP_KNOBS");
    drawKnobCol(6, "OUTPUT", kSupportOutputLevel, "dB", "SUPPORT_AMP_KNOBS", false);

    // REVERB KNOBS (Centered)
    const float effectKnobOffset = -38.f;
    const float effectColW = 70.f;
    drawKnobCol(1, "MIX", kReverbMix, "%", "REVERB_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(2, "DECAY", kReverbDecay, "s", "REVERB_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(3, "TONE", kReverbTone, "", "REVERB_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(4, "PRE-DLY", kReverbPreDelay, "ms", "REVERB_PREDELAY", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(5, "INTENSITY", kReverbShimmer, "%", "REVERB_SHIMMER", true, 5, 1, effectKnobOffset, effectColW);
    IRECT reverbPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    pGraphics->AttachControl(
      new VoLumModePickerControl(reverbPickerRect, kReverbMode, {"HALL", "PLATE", "OKTAVERB"}), -1, "REVERB_KNOBS");

    // Reverb sub-mode pill is currently used by Oktaverb only. Keep the reusable pill UI,
    // including the slimmer row and hover feedback, but do not expose placeholder modes.
    const float subPillW = 256.f;
    // Slimmer than the AMP-row toggleH (34) — the row carries text-only pill labels and a
    // single slide-switch, so a tighter 28 px height keeps it from feeling visually heavy.
    const float subPillH = 28.f;
    const float subPillY = knobT + knobDiam + valueH + 18.f;
    IRECT reverbSubPillRect(mainCX - subPillW / 2.f, subPillY, mainCX + subPillW / 2.f, subPillY + subPillH);
    auto* reverbSubPill = new VoLumSubModePillControl(reverbSubPillRect, kReverbSubMode, {"HALO", "SHIMMER", "BLOOM"});
    pGraphics->AttachControl(reverbSubPill, -1, "REVERB_SUBTOGGLE");
    mVolumReverbSubModePill = reverbSubPill;

    float revSwX = mainCX - 242.f;
    pGraphics->AttachControl(new VoLumPowerSwitchControl(
                               IRECT(revSwX - 14.f, knobT - 4.f, revSwX + 14.f, knobT + knobDiam + 2.f), kReverbActive),
                             -1, "REVERB_POWER");

    // DELAY KNOBS (Centered) - 5 slots: TIME, FEEDBACK, MIX, TONE, AGE.
    // TIME (slot 1) swaps to a tempo DIVISION stepper when Sync is engaged, so it
    // lives in its own group like the Tremolo RATE knob.
    drawKnobCol(1, "TIME", kDelayTime, "ms", "DELAY_TIME", true, 5, 1, effectKnobOffset, effectColW,
                "Delay time in ms. With TEMPO SYNC on it becomes a musical DIVISION locked to the song tempo.");
    drawKnobCol(2, "FEEDBACK", kDelayFeedback, "%", "DELAY_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(3, "MIX", kDelayMix, "%", "DELAY_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(4, "TONE", kDelayTone, "", "DELAY_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    // AGE slot is built manually so we can capture pointers to the label, knob and value
    // controls. The slot's label and tooltip swap per delay mode (GRIT/WEAR/AGE/BLOOM)
    // because the underlying parameter does meaningfully different things in each mode
    // (Digital: bit-crush+noise, Analog: BBD wear, Reverse: fade-shape softness).
    {
      const int slot = 5;
      const float customColW = effectColW;
      const float cx =
        mainCX + effectKnobOffset - (5 * customColW) / 2.f + (slot - 1) * customColW + (customColW / 2.f);
      const float kL = cx - (knobDiam / 2.f);
      auto* ageLabel = new VoLumKnobLabelControl(IRECT(cx - 40.f, knobRowTop, cx + 40.f, knobRowTop + 20.f), "AGE");
      pGraphics->AttachControl(ageLabel, -1, "DELAY_KNOBS");
      auto* ageKnob = new VoLumDialKnobControl(
        IRECT(kL, knobT, kL + knobDiam, knobT + knobDiam), kDelayAge, "", volumKnobStyle, knobBackgroundBitmap);
      pGraphics->AttachControl(ageKnob, -1, "DELAY_KNOBS");
      ageKnob->SetSelectedForKeyboard(mVolumSelectedKnobParamIdx == kDelayAge);
      auto* ageValue = new VoLumParamValueControl(
        IRECT(cx - 30.f, knobT + knobDiam + 2.f, cx + 30.f, knobT + knobDiam + 2.f + valueH), kDelayAge, "");
      pGraphics->AttachControl(ageValue, -1, "DELAY_KNOBS");
      mVolumDelayAgeLabel = ageLabel;
      mVolumDelayAgeKnob = ageKnob;
      mVolumDelayAgeValue = ageValue;
    }
    IRECT delayPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    pGraphics->AttachControl(
      new VoLumModePickerControl(delayPickerRect, kDelayMode, {"DIGITAL", "ANALOG", "REVERSE"}), -1, "DELAY_KNOBS");

    // Delay PingPong toggle sits below the knob block.
    // Layout: [toggle][PING-PONG label]
    // The slide-switch needs the standard 34 px height to render the bitmap handle without
    // clipping, while the pill itself stays slim at 28 px. We therefore center the toggle
    // vertically on the pill's center so the two sit on a shared visual baseline despite the
    // height mismatch. Visibility is mode-dependent (PingPong hides on Reverse).
    const float pillRowY = knobT + knobDiam + valueH + 18.f;
    const float pillRowH = 28.f; // matches subPillH; slim pill height
    const float ppSwitchW = 60.f;
    const float ppSwitchH = 34.f; // standard slide-switch height; less than this clips bitmap
    const float ppSwitchY = pillRowY - (ppSwitchH - pillRowH) * 0.5f;
    const float ppSwitchX = mainCX - 220.f;
    const float ppLabelW = 90.f;
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(ppSwitchX, ppSwitchY, ppSwitchX + ppSwitchW, ppSwitchY + ppSwitchH), kDelayPingPong,
                           "", volumToggleStyle, switchHandleBitmap),
      -1, "DELAY_PINGPONG");
    pGraphics->AttachControl(
      new VoLumKnobLabelControl(
        IRECT(ppSwitchX + ppSwitchW + 4.f, ppSwitchY, ppSwitchX + ppSwitchW + 4.f + ppLabelW, ppSwitchY + ppSwitchH),
        "PING-PONG"),
      -1, "DELAY_PINGPONG");

    // DIVISION stepper occupies the TIME slot (slot 1) when Sync is engaged.
    {
      const float divCx =
        mainCX + effectKnobOffset - (5 * effectColW) / 2.f + (1 - 1) * effectColW + (effectColW / 2.f);
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(IRECT(divCx - 40.f, knobRowTop, divCx + 40.f, knobRowTop + 20.f), "DIVISION"), -1,
        "DELAY_DIV");
      const float divStepH = 28.f;
      const float divStepTop = knobT + (knobDiam - divStepH) / 2.f;
      auto* divStep = new VoLumChannelStepControl(
        IRECT(divCx - 34.f, divStepTop, divCx + 34.f, divStepTop + divStepH), [this](int newIdx) {
          GetParam(kDelayDivision)->Set(static_cast<double>(newIdx));
          SendParameterValueFromDelegate(kDelayDivision, GetParam(kDelayDivision)->GetNormalized(), true);
          OnParamChange(kDelayDivision);
          _VolumMarkPresetDirty();
        });
      divStep->SetChannels(
        {"1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16"}, GetParam(kDelayDivision)->Int());
      pGraphics->AttachControl(divStep, -1, "DELAY_DIV");
      mVolumDelayDivStep = divStep;
    }

    // TEMPO SYNC toggle on the pill row, right of the PING-PONG toggle (which can
    // hide on Reverse); sync stays available in every delay mode.
    {
      const float dSwitchX = mainCX + 40.f;
      pGraphics->AttachControl(
        new NAMSwitchControl(IRECT(dSwitchX, ppSwitchY, dSwitchX + ppSwitchW, ppSwitchY + ppSwitchH), kDelaySync, "",
                             volumToggleStyle, switchHandleBitmap),
        -1, "DELAY_SYNC");
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(
          IRECT(dSwitchX + ppSwitchW + 4.f, ppSwitchY, dSwitchX + ppSwitchW + 4.f + ppLabelW, ppSwitchY + ppSwitchH),
          "TEMPO SYNC"),
        -1, "DELAY_SYNC");
    }

    float dlySwX = mainCX - 242.f;
    pGraphics->AttachControl(new VoLumPowerSwitchControl(
                               IRECT(dlySwX - 14.f, knobT - 4.f, dlySwX + 14.f, knobT + knobDiam + 2.f), kDelayActive),
                             -1, "DELAY_POWER");

    // TREMOLO KNOBS (Centered) - RATE / DEPTH / SHAPE / MIX (+ X-OVER in Harmonic).
    // RATE (slot 1) swaps to a tempo DIVISION stepper when Sync is engaged.
    drawKnobCol(1, "RATE", kTremoloRate, "Hz", "TREMOLO_RATE", true, 5, 1, effectKnobOffset, effectColW,
                "Tremolo speed in Hz. With TEMPO SYNC on it becomes a musical DIVISION locked to the song tempo.");
    drawKnobCol(2, "DEPTH", kTremoloDepth, "%", "TREMOLO_KNOBS", true, 5, 1, effectKnobOffset, effectColW,
                "How far the volume dips each cycle. 100% = full silence at the trough.");
    drawKnobCol(3, "SHAPE", kTremoloShape, "%", "TREMOLO_KNOBS", true, 5, 1, effectKnobOffset, effectColW,
                "Morphs the LFO from a smooth sine (0%) toward a hard square-wave chop (100%).");
    drawKnobCol(4, "MIX", kTremoloMix, "%", "TREMOLO_KNOBS", true, 5, 1, effectKnobOffset, effectColW,
                "Blend between dry and the tremolo'd signal. 100% = fully modulated.");
    drawKnobCol(5, "X-OVER", kTremoloCrossover, "Hz", "TREMOLO_XOVER", true, 5, 1, effectKnobOffset, effectColW,
                "HARMONIC mode only: the split frequency where the low and high bands pulse out of phase.");
    IRECT tremoloPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    auto* tremoloModePicker =
      new VoLumModePickerControl(tremoloPickerRect, kTremoloMode, {"OPTICAL", "BIAS", "HARMONIC"});
    tremoloModePicker->SetTooltip(
      "OPTICAL = asymmetric photocell throb (sag down, snap up) | BIAS = smooth symmetric "
      "sine | HARMONIC = phasey out-of-phase band split (adds the X-OVER knob).");
    pGraphics->AttachControl(tremoloModePicker, -1, "TREMOLO_KNOBS");

    // DIVISION stepper occupies the RATE slot (slot 1) when Sync is engaged.
    {
      const float divCx =
        mainCX + effectKnobOffset - (5 * effectColW) / 2.f + (1 - 1) * effectColW + (effectColW / 2.f);
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(IRECT(divCx - 40.f, knobRowTop, divCx + 40.f, knobRowTop + 20.f), "DIVISION"), -1,
        "TREMOLO_DIV");
      const float divStepH = 28.f;
      const float divStepTop = knobT + (knobDiam - divStepH) / 2.f;
      auto* divStep = new VoLumChannelStepControl(
        IRECT(divCx - 34.f, divStepTop, divCx + 34.f, divStepTop + divStepH), [this](int newIdx) {
          GetParam(kTremoloDivision)->Set(static_cast<double>(newIdx));
          SendParameterValueFromDelegate(kTremoloDivision, GetParam(kTremoloDivision)->GetNormalized(), true);
          OnParamChange(kTremoloDivision);
          _VolumMarkPresetDirty();
        });
      divStep->SetChannels(
        {"1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16"}, GetParam(kTremoloDivision)->Int());
      pGraphics->AttachControl(divStep, -1, "TREMOLO_DIV");
      mVolumTremoloDivStep = divStep;
    }

    // TEMPO SYNC toggle below the knob block (mirrors the delay ping-pong row).
    {
      const float tSwitchX = mainCX - 220.f;
      const float tSwitchY = pillRowY - (ppSwitchH - pillRowH) * 0.5f;
      pGraphics->AttachControl(
        new NAMSwitchControl(IRECT(tSwitchX, tSwitchY, tSwitchX + ppSwitchW, tSwitchY + ppSwitchH), kTremoloSync, "",
                             volumToggleStyle, switchHandleBitmap),
        -1, "TREMOLO_SYNC");
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(
          IRECT(tSwitchX + ppSwitchW + 4.f, tSwitchY, tSwitchX + ppSwitchW + 4.f + ppLabelW, tSwitchY + ppSwitchH),
          "TEMPO SYNC"),
        -1, "TREMOLO_SYNC");
    }

    float tremSwX = mainCX - 242.f;
    pGraphics->AttachControl(
      new VoLumPowerSwitchControl(
        IRECT(tremSwX - 14.f, knobT - 4.f, tremSwX + 14.f, knobT + knobDiam + 2.f), kTremoloActive),
      -1, "TREMOLO_POWER");

    // PRE KNOBS
    drawKnobCol(1, "GAIN", kPreNam1Gain, "dB", "PRE_NAM1_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(2, "BASS", kPreNam1Bass, "", "PRE_NAM1_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(3, "MID", kPreNam1Mid, "", "PRE_NAM1_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(4, "MID Hz", kPreNam1MidFreq, "Hz", "PRE_NAM1_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(5, "TREBLE", kPreNam1Treble, "", "PRE_NAM1_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(6, "LEVEL", kPreNam1Level, "dB", "PRE_NAM1_KNOBS", true, 6, 1, 0.f, 66.f);

    drawKnobCol(1, "GAIN", kPreNam2Gain, "dB", "PRE_NAM2_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(2, "BASS", kPreNam2Bass, "", "PRE_NAM2_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(3, "MID", kPreNam2Mid, "", "PRE_NAM2_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(4, "MID Hz", kPreNam2MidFreq, "Hz", "PRE_NAM2_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(5, "TREBLE", kPreNam2Treble, "", "PRE_NAM2_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(6, "LEVEL", kPreNam2Level, "dB", "PRE_NAM2_KNOBS", true, 6, 1, 0.f, 66.f);

    // 1176-style FET compressor: Input drives, Attack/Release, Output. Ratio fixed at 4:1; Mix locked at 1.0
    // (kPreCompRatio and kPreCompMix retained as EParams for state compatibility but hidden from UI).
    drawKnobCol(2, "INPUT", kPreCompAmount, "", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(3, "ATTACK", kPreCompAttack, "ms", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(4, "RELEASE", kPreCompRelease, "ms", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(5, "OUTPUT", kPreCompLevel, "dB", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);

    const float preSwX = mainCX - 242.f;
    pGraphics->AttachControl(
      new VoLumPowerSwitchControl(
        IRECT(preSwX - 14.f, knobT - 4.f, preSwX + 14.f, knobT + knobDiam + 2.f), kPreCompActive),
      -1, "COMP_POWER");
    pGraphics->AttachControl(
      new VoLumPowerSwitchControl(
        IRECT(preSwX - 14.f, knobT - 4.f, preSwX + 14.f, knobT + knobDiam + 2.f), kPreNam1Active),
      -1, "PRE_NAM1_POWER");
    pGraphics->AttachControl(
      new VoLumPowerSwitchControl(
        IRECT(preSwX - 14.f, knobT - 4.f, preSwX + 14.f, knobT + knobDiam + 2.f), kPreNam2Active),
      -1, "PRE_NAM2_POWER");

    // PITCH KNOBS (Transpose + Octaver). Mirrors the centered REVERB effect layout: a
    // right-hand mode picker (TRANSPOSE / OCTAVER) swaps between two knob groups; the
    // Octaver group adds a VINTAGE / MODERN voicing pill. The granular engine runs at a
    // fixed ~21 ms latency (reported to the host for PDC), so there is no QUALITY knob.
    pGraphics->AttachControl(
      new VoLumPowerSwitchControl(
        IRECT(preSwX - 14.f, knobT - 4.f, preSwX + 14.f, knobT + knobDiam + 2.f), kPrePitchActive),
      -1, "PITCH_POWER");

    // Sparse pitch rows (3-4 knobs) sit in the centered zone left of the mode picker.
    const float pitchColW = 78.f;
    const float pitchKnobDiam = 60.f;
    const float pitchKnobOffset = -42.f;

    // Transpose: SEMI, MIX, LEVEL (3 knobs centered).
    drawKnobCol(1, "SEMI", kPrePitchSemitones, "st", "PITCH_TRANSPOSE_KNOBS", true, 3, 1, pitchKnobOffset, pitchColW,
                "Transpose the whole signal in half-steps (-12..+7). Tuned tightest for drop tuning; "
                "pick INSTANT (mono) or POLY (chords) below.",
                pitchKnobDiam);
    drawKnobCol(2, "MIX", kPrePitchMix, "%", "PITCH_TRANSPOSE_KNOBS", true, 3, 1, pitchKnobOffset, pitchColW,
                "Blend between dry and the shifted signal. 100% = fully retuned.", pitchKnobDiam);
    drawKnobCol(3, "LEVEL", kPrePitchLevel, "dB", "PITCH_TRANSPOSE_KNOBS", true, 3, 1, pitchKnobOffset, pitchColW,
                "Output level of the Pitch pedal (dB). 0 = unity gain.", pitchKnobDiam);

    // Octaver: OCT DN, OCT UP, DRY, LEVEL (4 knobs centered).
    drawKnobCol(1, "OCT DN", kPrePitchOctDown, "%", "PITCH_OCTAVER_KNOBS", true, 4, 1, pitchKnobOffset, pitchColW,
                "Level of the octave-down (sub) voice. Stacks independently with OCT UP and DRY.", pitchKnobDiam);
    drawKnobCol(2, "OCT UP", kPrePitchOctUp, "%", "PITCH_OCTAVER_KNOBS", true, 4, 1, pitchKnobOffset, pitchColW,
                "Level of the octave-up voice. Stacks independently with OCT DN and DRY.", pitchKnobDiam);
    drawKnobCol(3, "DRY", kPrePitchDry, "%", "PITCH_OCTAVER_KNOBS", true, 4, 1, pitchKnobOffset, pitchColW,
                "Level of the unshifted (dry) voice. The Octaver blends three independent voices (OCT DN / OCT UP / "
                "DRY), so there is no single wet/dry MIX like the other effects.",
                pitchKnobDiam);
    drawKnobCol(4, "LEVEL", kPrePitchLevel, "dB", "PITCH_OCTAVER_KNOBS", true, 4, 1, pitchKnobOffset, pitchColW,
                "Output level of the Pitch pedal (dB). 0 = unity gain.", pitchKnobDiam);

    IRECT pitchPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    auto* pitchModePicker = new VoLumModePickerControl(pitchPickerRect, kPrePitchMode, {"TRANSPOSE", "OCTAVER"});
    pitchModePicker->SetTooltip(
      "TRANSPOSE = shift the whole signal by semitones (drop tuning / capo) | OCTAVER = "
      "POG-style blend of independent octave-down / octave-up / dry voices.");
    pGraphics->AttachControl(pitchModePicker, -1, "PITCH_MODE_PICKER");

    const float pitchPillW = 200.f;
    const float pitchPillH = 28.f;
    const float pitchPillY = knobT + knobDiam + valueH + 18.f;
    IRECT pitchVoicingRect(mainCX - pitchPillW / 2.f, pitchPillY, mainCX + pitchPillW / 2.f, pitchPillY + pitchPillH);
    auto* pitchVoicingPill = new VoLumSubModePillControl(pitchVoicingRect, kPrePitchVoicing, {"VINTAGE", "MODERN"});
    pitchVoicingPill->SetTooltip(
      "Octaver voice colour. VINTAGE = warmer octave voices (tanh drive + low-pass) | "
      "MODERN = clean. Shapes the wet voices only, so raise OCT DN/UP to hear it.");
    pGraphics->AttachControl(pitchVoicingPill, -1, "PITCH_VOICING");
    // Transpose engine character (shares the voicing pill's slot; mode picks which).
    // Two modes only: INSTANT and POLY. The underlying enum is
    // {Drop=0, Instant=1, Poly=2} (serialization-frozen; Drop is retired from the
    // picker and any legacy Drop preset now plays as the improved INSTANT), so the
    // pill maps its two slots to param values {1, 2} via SetValueMap.
    auto* pitchTransCharPill = new VoLumSubModePillControl(pitchVoicingRect, kPrePitchTransChar, {"INSTANT", "POLY"});
    pitchTransCharPill->SetValueMap({volum::kVoLumPitchCharacterInstant, volum::kVoLumPitchCharacterPoly},
                                    volum::kVoLumPitchCharacterCount - 1);
    pitchTransCharPill->SetTooltip(
      "Transpose engine. INSTANT = lowest latency (~8.6 ms), tightest attack, single "
      "notes / lead lines - use for fast live tracking | POLY = tracks CHORDS, not just "
      "single notes (~14 ms) - use for riffs, dyads and full chords. Both hold pitch on "
      "low drop-tuned strings.");
    pGraphics->AttachControl(pitchTransCharPill, -1, "PITCH_TRANSCHAR");

    // I/O meters
    const float meterW = 8.f;
    const float meterH = knobDiam + 10.f;
    const float meterTop = knobT - 5.f;
    const float gapMeterToKnob = 18.f;
    const float gapLabelToMeter = 8.f;
    const float meterLabelStripW = 16.f;

    const float inMeterR = rowLeft - gapMeterToKnob;
    const float inMeterL = inMeterR - meterW;
    const float inLabelR = inMeterL - gapLabelToMeter;
    const float inLabelL = inLabelR - meterLabelStripW;

    pGraphics->AttachControl(
      new VoLumVerticalLabelControl(IRECT(inLabelL, meterTop, inLabelR, meterTop + meterH), "IN"));
    pGraphics->AttachControl(
      new NAMMeterControl(IRECT(inMeterL, meterTop, inMeterR, meterTop + meterH), meterBackgroundBitmap, volumStyle),
      kCtrlTagInputMeter);

    const float rowRight = knobX(6) + colW;
    const float outMeterL = rowRight + gapMeterToKnob;
    const float outMeterR = outMeterL + meterW;
    // Second (right-channel) OUT meter, shown when dual amp / stereo mode is active.
    // 5 px gap so the two bars read as L / R rather than one fat meter; visibility toggled
    // in _UpdateVoLumLayout.
    const float outMeter2L = outMeterR + 5.f;
    const float outMeter2R = outMeter2L + meterW;
    const float outLabelL = outMeter2R + gapLabelToMeter;
    const float outLabelR = outLabelL + meterLabelStripW;

    pGraphics->AttachControl(
      new NAMMeterControl(IRECT(outMeterL, meterTop, outMeterR, meterTop + meterH), meterBackgroundBitmap, volumStyle),
      kCtrlTagOutputMeter);
    pGraphics->AttachControl(new NAMMeterControl(IRECT(outMeter2L, meterTop, outMeter2R, meterTop + meterH),
                                                 meterBackgroundBitmap, volumStyle),
                             kCtrlTagOutputMeterR);
    pGraphics->AttachControl(
      new VoLumVerticalLabelControl(IRECT(outLabelL, meterTop, outLabelR, meterTop + meterH), "OUT"));

    // Toggles: slide switch + label side by side
    const float toggleY = knobT + knobDiam + valueH + 2.f + 10.f;
    const float switchW = 60.f;
    const float switchH = toggleH;

    // ---- Toggle row layout ----
    //   NOISE GATE | EQ
    //
    // DUAL AMP toggle now lives as a chip in the hero's top-right corner, and PAN is a per-lane
    // floor-strip rail at the bottom of each hero panel — see VoLumHeroImageControl.
    float ngX = mainCX - 136.f;
    float eqX = mainCX + 30.f;

    // MAIN lane toggles (NOISE GATE / EQ bound to MAIN params)
    pGraphics->AttachControl(new NAMSwitchControl(IRECT(ngX, toggleY, ngX + switchW, toggleY + switchH),
                                                  kNoiseGateActive, "", volumToggleStyle, switchHandleBitmap),
                             kCtrlTagVoLumNoiseGate, "MAIN_LANE_TOGGLES");
    pGraphics->AttachControl(
      new VoLumKnobLabelControl(
        IRECT(ngX + switchW + 4.f, toggleY, ngX + switchW + 90.f, toggleY + switchH), "NOISE GATE"),
      -1, "MAIN_LANE_TOGGLES");
    pGraphics->AttachControl(new NAMSwitchControl(IRECT(eqX, toggleY, eqX + switchW, toggleY + switchH), kEQActive, "",
                                                  volumToggleStyle, switchHandleBitmap),
                             kCtrlTagVoLumEQ, "MAIN_LANE_TOGGLES");
    pGraphics->AttachControl(
      new VoLumKnobLabelControl(IRECT(eqX + switchW + 4.f, toggleY, eqX + switchW + 46.f, toggleY + switchH), "EQ"), -1,
      "MAIN_LANE_TOGGLES");

    // SUPPORT lane toggles (identical positions, bound to SUPPORT params).
    pGraphics->AttachControl(new NAMSwitchControl(IRECT(ngX, toggleY, ngX + switchW, toggleY + switchH),
                                                  kSupportNoiseGateActive, "", volumToggleStyle, switchHandleBitmap),
                             -1, "SUPPORT_LANE_TOGGLES");
    pGraphics->AttachControl(
      new VoLumKnobLabelControl(
        IRECT(ngX + switchW + 4.f, toggleY, ngX + switchW + 90.f, toggleY + switchH), "NOISE GATE"),
      -1, "SUPPORT_LANE_TOGGLES");
    pGraphics->AttachControl(new NAMSwitchControl(IRECT(eqX, toggleY, eqX + switchW, toggleY + switchH),
                                                  kSupportEQActive, "", volumToggleStyle, switchHandleBitmap),
                             -1, "SUPPORT_LANE_TOGGLES");
    pGraphics->AttachControl(
      new VoLumKnobLabelControl(IRECT(eqX + switchW + 4.f, toggleY, eqX + switchW + 46.f, toggleY + switchH), "EQ"), -1,
      "SUPPORT_LANE_TOGGLES");

    const IRECT hintArea(mainCX - 270.f, toggleY + toggleH + 10.f, mainCX + 270.f, toggleY + toggleH + 10.f + 44.f);
    pGraphics->AttachControl(new VoLumKeyboardHintControl(hintArea), kCtrlTagVoLumKeyboardHint);

    // Footer
    const IRECT footerArea(mainL, hintArea.B + 6.f, mainR, hintArea.B + 6.f + 18.f);
    pGraphics->AttachControl(new VoLumFooterControl(footerArea), kCtrlTagVoLumFooter);
    if (!mVolumLastLoadedFile.empty())
      pGraphics->GetControlWithTag(kCtrlTagVoLumFooter)
        ->As<VoLumFooterControl>()
        ->SetText(mVolumLastLoadedFile.c_str());

    pGraphics->AttachControl(new VoLumExactEntryControl(b, kInputLevel, "INPUT"), kCtrlTagVoLumExactEntry)->Hide(true);
    pGraphics
      ->AttachControl(new VoLumPreCaptureMenuControl(IRECT(mainL, knobRowTop, mainL + 220.f, knobRowTop + 160.f)),
                      kCtrlTagVoLumPreCaptureMenu)
      ->Hide(true);
    // Dual-amp SUPPORT picker: scrollable list with "(none)" + factory amps.
    // Dual-amp SUPPORT picker: scrollable list with "(none)" + factory amps + a
    // "CUSTOM" group (shown only when custom amps exist). Picking a custom amp
    // makes it the support partner (display + session only). Custom rows are
    // offset by kVolumCustomSupportBase so the callback can tell them apart.
    {
      auto* supMenu = new VoLumListMenuControl(b);
      supMenu->SetCallback([this](int code) {
        if (code == VoLumListMenuControl::kNone)
          _VolumSetSupportAmp(-1);
        else if (code >= kVolumCustomSupportBase)
          _VolumSetSupportCustom(code - kVolumCustomSupportBase);
        else if (code >= 0 && code < volum::kAmpCount)
          _VolumSetSupportAmp(code);
      });
      pGraphics->AttachControl(supMenu, kCtrlTagVoLumSupportAmpMenu)->Hide(true);
    }

    // Lane belonging on the SUPPORT amp-row knobs is conveyed solely by the teal knob pointer
    // dot. Labels and value text stay bright/neutral so the row reads cleanly. Set once at attach
    // — SUPPORT_AMP_KNOBS is only ever visible while support is focused, so no retoggling.
    pGraphics->ForAllControlsFunc([](iplug::igraphics::IControl* c) {
      const char* g = c->GetGroup();
      if (!g || std::strcmp(g, "SUPPORT_AMP_KNOBS") != 0)
        return;
      if (auto* knob = dynamic_cast<NAMKnobControl*>(c))
        knob->SetStyle(volumKnobStyleSupport);
    });

    _UpdateVoLumLayout(pGraphics);

    // Toolbar buttons (top-right of main panel): Tuner | Metronome | Gear
    {
      const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
      const auto tunerSVG = pGraphics->LoadSVG(TUNER_FN);
      const auto metronomeSVG = pGraphics->LoadSVG(METRONOME_FN);
      const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
      const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
      const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);

      const IRECT gearArea(mainR - 44.f, b.T + 14.f, mainR - 18.f, b.T + 40.f);
      const IRECT metronomeArea(mainR - 80.f, b.T + 14.f, mainR - 54.f, b.T + 40.f);
      const IRECT tunerArea(mainR - 116.f, b.T + 14.f, mainR - 90.f, b.T + 40.f);

      // Tuner button
      auto* pPlugin = this;
      pGraphics->AttachControl(
        new NAMCircleButtonControl(tunerArea, [pPlugin](IControl*) { pPlugin->_ToggleVoLumTuner(); }, tunerSVG));

      // Metronome button
      pGraphics->AttachControl(
        new VoLumMetronomeButtonControl(
          metronomeArea, [pPlugin](IControl*) { pPlugin->_ToggleVoLumMetronomePanel(); }, metronomeSVG),
        kCtrlTagVoLumMetronomeButton);

      // Gear button
      pGraphics->AttachControl(new NAMCircleButtonControl(
        gearArea,
        [pGraphics](IControl* pCaller) {
          pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
        },
        gearSVG));

      pGraphics
        ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                   crossSVG, volumSettingsStyle, volumSettingsRadioStyle),
                        kCtrlTagSettingsBox)
        ->Hide(true);

      // Tuner overlay (on top of everything)
      {
        auto* tunerCtrl = new VoLumTunerControl(b);
        tunerCtrl->SetDismissAction([pPlugin]() { pPlugin->mTunerDSP.SetActive(false); });
        pGraphics->AttachControl(tunerCtrl, kCtrlTagVoLumTuner)->Hide(true);
      }

      // F5 preset bar — centred in the top header band, above the AMP/triptych
      // column. Clicking opens the anchored preset dropdown; < > cycle presets.
      {
        const float presetBarW = 240.f;
        const IRECT presetBarArea(mainCX - presetBarW * 0.5f, b.T + 12.f, mainCX + presetBarW * 0.5f, b.T + 40.f);
        pGraphics->AttachControl(
          new VoLumPresetBarControl(presetBarArea, [pPlugin]() { pPlugin->_VolumShowPresetMenu(); }),
          kCtrlTagVoLumPresetBar);
        if (auto* pb = pGraphics->GetControlWithTag(kCtrlTagVoLumPresetBar))
        {
          auto* bar = pb->As<VoLumPresetBarControl>();
          bar->SetRecallCallback([pPlugin](int index) { pPlugin->_VolumRecallPreset(index); });
          bar->SetSaveAsCallback([pPlugin](const std::string& name) { pPlugin->_VolumSavePresetAs(name); });
          pPlugin->_VolumRefreshPresetBar();
        }
      }

      // F5: preset dropdown (anchored under the preset bar). Picking recalls;
      // "Manage presets..." opens the shared Manage panel.
      {
        auto* presetMenu = new VoLumListMenuControl(b);
        presetMenu->SetCallback([pPlugin](int code) {
          auto* pGfx = pPlugin->GetUI();
          if (!pGfx)
            return;
          if (code == VoLumListMenuControl::kManage)
          {
            if (auto* ov = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
              ov->As<VoLumCustomOverlayControl>()->ShowManage(VoLumCustomOverlayControl::ManageKind::Presets,
                                                              pPlugin->mVolumAmpIdx,
                                                              pPlugin->_VolumMainAmpDisplayName());
            return;
          }
          if (code == VoLumListMenuControl::kDefault)
          {
            pPlugin->_VolumResetAmpToFactory();
            return;
          }
          if (code == VoLumListMenuControl::kOverwrite)
          {
            auto* bar = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar);
            if (!bar)
              return;
            auto* presetBar = bar->As<VoLumPresetBarControl>();
            const int idx = presetBar->ActiveIndex();
            if (idx < 0)
              return;
            const std::string name = presetBar->ActiveName();
            auto doOverwrite = [pPlugin, idx]() { pPlugin->_VolumOverwritePreset(idx); };
            if (auto* dlg = pGfx->GetControlWithTag(kCtrlTagVoLumConfirm))
              dlg->As<VoLumConfirmDialogControl>()->Show("Are you sure?",
                                                         "Overwrite preset \"" + name + "\" with the current settings?",
                                                         doOverwrite, "Overwrite");
            else
              doOverwrite();
            return;
          }
          if (code == VoLumListMenuControl::kSaveAsNew)
          {
            if (auto* bar = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
              bar->As<VoLumPresetBarControl>()->PromptSaveAs();
            return;
          }
          const auto presets = volum::custom::MockPresetsForAmp(pPlugin->mVolumAmpIdx);
          if (code >= 0 && code < (int)presets.size())
            pPlugin->_VolumRecallPreset(code); // apply settings + drive the bar
        });
        pGraphics->AttachControl(presetMenu, kCtrlTagVoLumPresetMenu)->Hide(true);
      }

      // F7: Custom IR cab dropdown (anchored under the speaker-row IR button).
      {
        auto* irMenu = new VoLumListMenuControl(b);
        irMenu->SetCallback([pPlugin](int code) {
          auto* pGfx = pPlugin->GetUI();
          if (!pGfx)
            return;
          if (code == VoLumListMenuControl::kManage)
          {
            if (auto* ov = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
              ov->As<VoLumCustomOverlayControl>()->ShowManage(VoLumCustomOverlayControl::ManageKind::IR);
            return;
          }
          const auto& irs = volum::custom::MockIRLibrary();
          const bool support = pPlugin->_VolumSupportFocused();
          if (code >= 0 && code < (int)irs.size())
            pPlugin->_VolumSelectIR(code, support); // stage IR into the focused lane + enable + persist
          else if (code == VoLumListMenuControl::kNone)
            pPlugin->_VolumClearIR(support); // back to baked cab
        });
        pGraphics->AttachControl(irMenu, kCtrlTagVoLumIrMenu)->Hide(true);
      }

      // Manage + Builder overlay (on top of everything; hidden until invoked).
      {
        auto* overlay = new VoLumCustomOverlayControl(b);
        overlay->SetCallbacks(
          // custom amp saved from the builder -> add to the live session list,
          // refresh the sidebar, and select the new amp (mock; no disk).
          [pPlugin](const volum::custom::CustomAmp& ampIn, int editIdx) -> std::string {
            // editIdx >= 0 -> the user edited an existing amp: mutate that entry
            // in place so we don't spawn a duplicate. Otherwise append a new one.
            auto& store = volum::content::GlobalContentStore();
            volum::custom::CustomAmp amp = ampIn;
            // F6 import: copy and parse every capture before mutating the registry.
            // A failed copy/parser check rolls back all files created by this Save
            // and returns an error to the still-open builder.
            std::string ampId = (editIdx >= 0) ? volum::custom::CustomAmpIdAt(editIdx) : std::string();
            if (ampId.empty())
            {
              amp.id = volum::content::MintId(store.reg(), "amp");
              ampId = amp.id;
            }
            auto prepared = volum::content::PrepareCustomNamImport(
              store, amp, ampId, [](const std::filesystem::path& path) -> std::string {
                try
                {
                  nam::dspData config;
                  auto model = nam::get_dsp(path, config);
                  return model ? std::string() : std::string("NAM parser returned no model");
                }
                catch (const std::exception& e)
                {
                  return e.what();
                }
              });
            if (!prepared)
              return "Save failed: " + prepared.error;
            amp = std::move(prepared.amp);

            const int idx =
              (editIdx >= 0) ? volum::custom::UpdateCustomAmp(editIdx, amp) : volum::custom::AddCustomAmp(amp);
            if (idx < 0)
            {
              for (const auto& rel : prepared.copiedPaths)
                store.RemoveStoredFile(rel);
              return "Save failed: the custom amp registry could not be updated";
            }
            auto* pGfx = pPlugin->GetUI();
            if (!pGfx)
              return {};
            const auto& amps = volum::custom::MockCustomAmps();
            if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
              al->As<VoLumAmpListControl>()->SetCustomAmps(amps, volum::custom::MockCustomAmpArts());
            // Full refresh (hero art/name + cabinet row + sidebar highlight) so a
            // renamed amp / re-mapped cabs show immediately - and an edit lands on
            // the same entry instead of a stray duplicate.
            if (idx >= 0 && idx < (int)amps.size())
              pPlugin->_VolumSelectCustomAmp(idx);
            return {};
          },
          // Manage panel mutated (preset/IR/pedal CRUD) -> recover the live cab if
          // the active custom IR was just deleted, then re-sync the header strip
          // for the currently focused amp (factory or custom).
          [pPlugin]() {
            // Auto-normalize any freshly-imported IR (uncalibrated) so it lands at
            // stock-cab level immediately, then re-push the active IR's shaping to
            // both lanes so a live trim/cut edit in the panel is heard at once.
            pPlugin->_VolumMigrateIrTrims();
            pPlugin->_VolumReconcileActiveIr();
            pPlugin->_VolumPushIrShaping(false);
            pPlugin->_VolumPushIrShaping(true);
            pPlugin->_VolumRefreshPresetBar();
          });
        // F5 preset capture: save-as / overwrite snapshot the live scene.
        overlay->SetPresetCallbacks([pPlugin](const std::string& name) { return pPlugin->_VolumSavePresetAs(name); },
                                    [pPlugin](int index) { pPlugin->_VolumOverwritePreset(index); });
        // Manage-panel destructive actions (delete / overwrite) go through the
        // shared confirm modal.
        overlay->SetConfirmCallback(
          [pPlugin](const std::string& msg, std::function<void()> onConfirm, const std::string& confirmLabel) {
            if (auto* pGfx = pPlugin->GetUI())
              if (auto* dlg = pGfx->GetControlWithTag(kCtrlTagVoLumConfirm))
                dlg->As<VoLumConfirmDialogControl>()->Show("Are you sure?", msg, std::move(onConfirm), confirmLabel);
          });
        // Double-clicking a Manage row performs its primary action (mock):
        //   preset -> recall onto the header bar; IR -> use on the focused cab;
        //   pedal  -> load into the originating PRE NAM slot (backend wires DSP).
        overlay->SetPrimaryActionCallback(
          [pPlugin](VoLumCustomOverlayControl::ManageKind kind, int ampIdx, int pedalSlot, int index) {
            auto* pGfx = pPlugin->GetUI();
            if (!pGfx)
              return;
            using MK = VoLumCustomOverlayControl::ManageKind;
            if (kind == MK::Presets)
            {
              const auto presets = volum::custom::MockPresetsForAmp(ampIdx);
              if (index >= 0 && index < (int)presets.size())
                pPlugin->_VolumRecallPreset(index); // apply settings + drive the bar
            }
            else if (kind == MK::IR)
            {
              const auto& irs = volum::custom::MockIRLibrary();
              if (index >= 0 && index < (int)irs.size())
                pPlugin->_VolumSelectIR(index, pPlugin->_VolumSupportFocused()); // DIRECT + convolve + persist + dirty
            }
            else // Pedals: load the imported capture into its originating PRE slot
            {
              const int legacy = volum::custom::PedalLegacyIndexAt(index);
              if (pedalSlot >= 0 && legacy >= volum::content::kCustomPedalIndexBase)
                pPlugin->_VolumSetPreNamCapture(pedalSlot, legacy); // stages the .nam + marks dirty
            }
          });
        pGraphics->AttachControl(overlay, kCtrlTagVoLumCustomOverlay)->Hide(true);

        // Shared "Are you sure?" modal, attached above the overlay.
        pGraphics->AttachControl(new VoLumConfirmDialogControl(b), kCtrlTagVoLumConfirm)->Hide(true);
      }

      // Metronome config overlay
      {
        auto* metCtrl = new VoLumMetronomeControl(b);
        metCtrl->mOnActiveChanged = [pPlugin](bool active) {
          pPlugin->mMetronomeDSP.SetActive(active);
          if (auto* btn = pPlugin->GetUI()->GetControlWithTag(kCtrlTagVoLumMetronomeButton))
            btn->As<VoLumMetronomeButtonControl>()->SetActive(active);
        };
        metCtrl->mOnBPMChanged = [pPlugin](float bpm) { pPlugin->mMetronomeDSP.SetBPM(bpm); };
        metCtrl->mOnVolumeChanged = [pPlugin](float vol) { pPlugin->mMetronomeDSP.SetVolume(vol); };
        metCtrl->mOnTimeSigChanged = [pPlugin](volum::MetronomeTimeSig sig) { pPlugin->mMetronomeDSP.SetTimeSig(sig); };
        pGraphics->AttachControl(metCtrl, kCtrlTagVoLumMetronome)->Hide(true);
      }
    }

#if defined(APP_API) && VOLUM_OPEN_SETTINGS_AT_LAUNCH
    if (auto* settings = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
      settings->As<NAMSettingsPageControl>()->HideAnimated(false);
#endif

    // Selection state (sidebar, cab row, IR chip, channel stepper, hero) is not
    // applied here. OnUIOpen calls _VolumSyncUiFromState once the whole editor
    // exists, which re-derives all of it from backend state through the shared
    // planner. Setting a partial selection here is what used to leave a lane with
    // an active custom IR reading as "No Cab".

    _SyncVoLumExactEntry();

    // Keyboard: keep the original arrows, add a shallow PRE/AMP/POST focus layer.
    pGraphics->SetKeyHandlerFunc([this](const IKeyPress& key, bool isUp) {
      if (isUp)
        return false;

      if (auto* pGfx = GetUI())
      {
        if (key.VK == kVK_ESCAPE)
        {
          if (auto* entry = pGfx->GetControlWithTag(kCtrlTagVoLumExactEntry))
          {
            auto* exact = entry->As<VoLumExactEntryControl>();
            if (!exact->IsHidden())
            {
              exact->CancelEntry();
              return true;
            }
          }
        }

        if (pGfx->GetControlInTextEntry())
          return false;

        // ESC closes the topmost open transient surface (overlay first, then any
        // anchored dropdown) for consistent dismissal across the UI.
        if (key.VK == kVK_ESCAPE)
        {
          const int kDismissTags[] = {kCtrlTagVoLumConfirm, kCtrlTagVoLumCustomOverlay,  kCtrlTagVoLumPresetMenu,
                                      kCtrlTagVoLumIrMenu,  kCtrlTagVoLumPreCaptureMenu, kCtrlTagVoLumSupportAmpMenu};
          for (int tag : kDismissTags)
          {
            if (auto* c = pGfx->GetControlWithTag(tag))
            {
              if (!c->IsHidden())
              {
                c->Hide(true);
                pGfx->SetAllControlsDirty();
                return true;
              }
            }
          }
        }

        if (auto* settings = pGfx->GetControlWithTag(kCtrlTagSettingsBox))
        {
          if (!settings->IsHidden())
          {
            if (key.VK == kVK_ESCAPE)
            {
              settings->As<NAMSettingsPageControl>()->HideAnimated(true);
              return true;
            }
            return false;
          }
        }

        // While a modal overlay or any anchored dropdown is open, the keyboard
        // belongs to it - not the main view behind it. Route arrows into the
        // builder art picker; otherwise swallow nav keys so the background amp
        // list / knobs don't move. Non-nav keys fall through to the focused
        // control (text entry etc.).
        {
          const int kModalTags[] = {kCtrlTagVoLumConfirm, kCtrlTagVoLumCustomOverlay,  kCtrlTagVoLumPresetMenu,
                                    kCtrlTagVoLumIrMenu,  kCtrlTagVoLumPreCaptureMenu, kCtrlTagVoLumSupportAmpMenu};
          for (int tag : kModalTags)
          {
            auto* c = pGfx->GetControlWithTag(tag);
            if (!c || c->IsHidden())
              continue;
            const bool isNav = key.VK == kVK_UP || key.VK == kVK_DOWN || key.VK == kVK_LEFT || key.VK == kVK_RIGHT;
            if (tag == kCtrlTagVoLumCustomOverlay && isNav)
            {
              if (c->As<VoLumCustomOverlayControl>()->OnArrowKey(key.VK))
                return true;
            }
            return isNav; // swallow background navigation; let other keys pass
          }
        }
      }

      if (_HandleVoLumKeyboardFocusKey(key))
        return true;

      if (_HandleVoLumSelectedKnobKey(key))
        return true;

      if (key.VK == kVK_UP || key.VK == kVK_DOWN)
      {
        // Navigate one combined list: factory amps [0..N-1] then custom amps
        // [N..N+C-1], wrapping across the whole thing so arrows reach CUSTOM.
        auto* pGfx = GetUI();
        VoLumAmpListControl* ampList = nullptr;
        if (pGfx)
          if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
            ampList = al->As<VoLumAmpListControl>();

        const int N = volum::kAmpCount;
        const int C = ampList ? ampList->GetCustomCount() : 0;
        const int total = N + C;
        int cur = mVolumAmpIdx;
        if (ampList && ampList->GetCustomSelected() >= 0)
          cur = N + ampList->GetCustomSelected();
        const int dir = (key.VK == kVK_UP) ? -1 : 1;
        const int newPos = ((cur + dir) % total + total) % total;

        _ClearVoLumKnobSelection();
        _VolumSaveCurrentToSettings();

        if (newPos >= N)
        {
          // Custom amp (mock: display-only; underlying factory DSP unchanged).
          _VolumSelectCustomAmp(newPos - N);
          return true;
        }

        const int newIdx = newPos;
        mVolumAmpIdx = newIdx;
        mVolumCustomMainIdx = -1; // keyboard-nav landed on a factory amp
        _VolumRestoreFromSettings(newIdx);
        _VolumRefreshChannels();
        mVolumNeedsLoad.store(true);
#ifdef APP_API
        // Coalesced; flushed by OnIdle() (see selection callback note above).
        mVolumSettingsDirty = true;
#endif
        if (pGfx)
        {
          if (ampList)
            ampList->SetSelected(newIdx); // also clears any custom selection
          if (auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
          {
            char ph[4] = {volum::kAmps[newIdx].displayName[0], (char)('0' + (newIdx % 10)), 0, 0};
            heroCtrl->As<VoLumHeroImageControl>()->SetPlaceholder(ph, newIdx);
            heroCtrl->As<VoLumHeroImageControl>()->SetName(volum::kAmps[newIdx].displayName);
          }
          if (auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
            if (mVolumExpandedSection == EVoLumSection::AMP)
              nameCtrl->As<VoLumSubRowTextControl>()->SetName(volum::kAmps[newIdx].displayName, true);
          // F5: refresh the header preset strip to this amp's bank.
          _VolumSyncPresetOwner();
          _VolumRefreshPresetBar();
          if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych))
          {
            auto* trip = tripCtrl->As<VoLumTriptychControl>();
            const bool preActive =
              GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
            trip->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), newIdx,
                           volum::kAmps[newIdx].displayName,
                           _VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), "NAM 1"),
                           _VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), "NAM 2"));
            mVolumPreLockUiDirty = mVolumPreLocked && _VolumIsPreDirty();
            mVolumPostLockUiDirty = mVolumPostLocked && _VolumIsPostDirty();
            trip->SetDirty(false);
          }
        }
        return true;
      }
      if (key.VK == kVK_LEFT || key.VK == kVK_RIGHT)
      {
        if (mVolumSelectedKnobParamIdx != kNoParameter)
          return false;

        if (mVolumExpandedSection == EVoLumSection::PRE || mVolumExpandedSection == EVoLumSection::POST)
          return _CycleVoLumKeyboardTarget(key.VK == kVK_LEFT ? -1 : 1);

        if (mVolumExpandedSection == EVoLumSection::AMP)
        {
          // Drive the focused lane's stepper through the SAME callback a click
          // uses (StepKeyboard). This keeps the keyboard and mouse paths in sync
          // for both factory and custom amps - previously the keyboard updated
          // only the index/label and never staged the new channel's .nam.
          const int delta = (key.VK == kVK_LEFT) ? -1 : 1;
          const bool supportFocus = GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport;
          const int tag = supportFocus ? kCtrlTagVoLumSupportChannelStep : kCtrlTagVoLumChannelStep;
          if (auto* pGfx = GetUI())
            if (auto* stepper = pGfx->GetControlWithTag(tag))
              stepper->As<VoLumChannelStepControl>()->StepKeyboard(delta);
          return true;
        }
        return false;
      }
      if (key.VK == kVK_ESCAPE)
      {
        _ClearVoLumKnobSelection();
        return false;
      }
      return false;
    });

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
}
