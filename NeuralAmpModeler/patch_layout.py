import re

with open("NeuralAmpModeler.cpp", "r", encoding="utf-8") as f:
    text = f.read()

# We want to replace the sections starting from:
# // Amp hero image
# until the end of _AttachVoLumGraphics layout, which is around:
# // Settings gear button (top-right of main panel)

start_marker = "    // Amp hero image"
end_marker = "    // Settings gear button (top-right of main panel)"

start_idx = text.find(start_marker)
end_idx = text.find(end_marker)

if start_idx == -1 or end_idx == -1:
    print("Markers not found")
    exit(1)

print(f"Found from {start_idx} to {end_idx}")

new_code = """    // Triptych (PRE | AMP | POST)
    const float triptychW = 620.f;
    const float triptychH = 196.f;
    const IRECT triptychArea(mainCX - triptychW / 2.f, yPos, mainCX + triptychW / 2.f, yPos + triptychH);
    
    auto* triptych = new VoLumTriptychControl(triptychArea, [this](EVoLumSection sec, EVoLumEffectFocus focus) {
        mVolumExpandedSection = sec;
        mVolumFocusedEffect = focus;
        _UpdateVoLumLayout();
    });
    pGraphics->AttachControl(triptych, kCtrlTagVoLumTriptych);

    // POST Expanded Pedal Cards
    const float pedalW = 210.f;
    const float pedalH = 158.f;
    const float postCx = triptychArea.MW(); // Center of Triptych
    // When POST is expanded, pedals are centered.
    const IRECT delayCardRect(postCx - pedalW - 10.f, yPos + 20.f, postCx - 10.f, yPos + 20.f + pedalH);
    const IRECT reverbCardRect(postCx + 10.f, yPos + 20.f, postCx + 10.f + pedalW, yPos + 20.f + pedalH);
    const IRECT chainLinkRect(postCx - 10.f, yPos + 20.f + pedalH/2.f - 6.f, postCx + 10.f, yPos + 20.f + pedalH/2.f + 6.f);

    auto onPedalClick = [this](VoLumPedalCardControl* card, bool isBypassClick) {
        if (isBypassClick) {
            if (card->GetEffect() == EVoLumEffectFocus::DELAY) {
                GetParam(kDelayActive)->Set(!GetParam(kDelayActive)->Value());
            } else if (card->GetEffect() == EVoLumEffectFocus::REVERB) {
                GetParam(kReverbActive)->Set(!GetParam(kReverbActive)->Value());
            }
        } else {
            mVolumFocusedEffect = card->GetEffect();
        }
        _UpdateVoLumLayout();
    };

    pGraphics->AttachControl(new VoLumPedalCardControl(delayCardRect, EVoLumEffectFocus::DELAY, "DELAY", 16, kDelayActive, onPedalClick), kCtrlTagVoLumDelayCard)->Hide(true);
    pGraphics->AttachControl(new VoLumChainConnectorControl(chainLinkRect), -1, "POST_CARDS")->Hide(true);
    pGraphics->AttachControl(new VoLumPedalCardControl(reverbCardRect, EVoLumEffectFocus::REVERB, "REVERB", 15, kReverbActive, onPedalClick), kCtrlTagVoLumReverbCard)->Hide(true);

    yPos += triptychH + 4.f;

    // Sub-row text (Replaces Amp Name / Focus Header)
    const IRECT subRowArea(mainL, yPos, mainR, yPos + 54.f);
    pGraphics->AttachControl(new VoLumSubRowTextControl(subRowArea), kCtrlTagVoLumSubRowText);
    yPos += 54.f + 12.f; // Name + rule + gap

    // ---- Knobs: [Channel] | [Input, Gate] | [Bass, Mid, Treble] | [Output] ----
    const float colW = 64.f;
    const float divW = 12.f;
    const float knobRowTop = yPos;
    const float knobT = knobRowTop + 20.f; // labelH
    const float knobDiam = 58.f;
    const float valueH = 18.f;
    const float totalW = 7 * colW + 3 * divW + 20.f;
    const float rowLeft = mainCX - totalW / 2.f;

    auto knobX = [&](int slot) -> float {
      float x = rowLeft;
      int dividers = 0;
      if (slot > 0) dividers++;
      if (slot > 2) dividers++;
      if (slot > 5) dividers++;
      return x + slot * colW + dividers * divW;
    };

    auto drawDivider = [&](float afterSlotRight, const char* group) {
      float dx = afterSlotRight + divW / 2.f - 1.f;
      auto ctrl = new VoLumDividerControl(IRECT(dx, knobT + 4.f, dx + 2.f, knobT + knobDiam - 4.f));
      pGraphics->AttachControl(ctrl, -1, group);
    };

    auto drawKnobCol = [&](int slot, const char* label, int paramId, const char* suffix, const char* group, bool center_offset = false) {
      float cx = center_offset ? (mainCX - (3 * colW + 1 * divW) / 2.f + slot * colW) : knobX(slot);
      float kL = cx + (colW - knobDiam) / 2.f;

      pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(cx, knobRowTop, cx + colW, knobRowTop + 20.f), label), -1, group);
      auto* knob = new NAMKnobControl(IRECT(kL, knobT, kL + knobDiam, knobT + knobDiam), paramId, "", volumKnobStyle, knobBackgroundBitmap);
      pGraphics->AttachControl(knob, -1, group);
      knob->SetSelectedForKeyboard(mVolumSelectedKnobParamIdx == paramId);
      pGraphics->AttachControl(new VoLumParamValueControl(IRECT(cx, knobT + knobDiam + 2.f, cx + colW, knobT + knobDiam + 2.f + valueH), paramId, suffix), -1, group);
    };

    // AMP KNOBS
    {
      float cx = knobX(0);
      pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(cx, knobRowTop, cx + colW, knobRowTop + 20.f), "CHANNEL", true), -1, "AMP_KNOBS");
      float stepH = 28.f;
      float stepTop = knobT + (knobDiam - stepH) / 2.f;
      auto* channelStep = new VoLumChannelStepControl(IRECT(cx, stepTop, cx + colW, stepTop + stepH), [this](int newIdx) {
          mVolumChannelIdx = newIdx;
          mVolumAmpSettings[mVolumAmpIdx].channelIdx = newIdx;
          mVolumSettingsDirty = true;
          mVolumNeedsLoad.store(true);
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

    // REVERB KNOBS (Centered)
    drawKnobCol(0, "MIX", kReverbMix, "%", "REVERB_KNOBS", true);
    drawKnobCol(1, "DECAY", kReverbDecay, "s", "REVERB_KNOBS", true);
    drawKnobCol(2, "TONE", kReverbTone, "", "REVERB_KNOBS", true);
    pGraphics->HideControlGroup("REVERB_KNOBS", true);

    // DELAY KNOBS (Centered)
    drawKnobCol(0, "TIME", kDelayTime, "ms", "DELAY_KNOBS", true);
    drawKnobCol(1, "FEEDBACK", kDelayFeedback, "%", "DELAY_KNOBS", true);
    drawKnobCol(2, "MIX", kDelayMix, "%", "DELAY_KNOBS", true);
    pGraphics->HideControlGroup("DELAY_KNOBS", true);

    // BOOST KNOBS (Centered)
    drawKnobCol(0, "DRIVE", kBoostDrive, "", "BOOST_KNOBS", true);
    drawKnobCol(1, "TONE", kBoostTone, "", "BOOST_KNOBS", true);
    drawKnobCol(2, "LEVEL", kBoostLevel, "dB", "BOOST_KNOBS", true);
    pGraphics->HideControlGroup("BOOST_KNOBS", true);

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

    pGraphics->AttachControl(new VoLumVerticalLabelControl(IRECT(inLabelL, meterTop, inLabelR, meterTop + meterH), "IN"));
    pGraphics->AttachControl(new NAMMeterControl(IRECT(inMeterL, meterTop, inMeterR, meterTop + meterH), meterBackgroundBitmap, volumStyle), kCtrlTagInputMeter);

    const float rowRight = knobX(6) + colW;
    const float outMeterL = rowRight + gapMeterToKnob;
    const float outMeterR = outMeterL + meterW;
    const float outLabelL = outMeterR + gapLabelToMeter;
    const float outLabelR = outLabelL + meterLabelStripW;

    pGraphics->AttachControl(new NAMMeterControl(IRECT(outMeterL, meterTop, outMeterR, meterTop + meterH), meterBackgroundBitmap, volumStyle), kCtrlTagOutputMeter);
    pGraphics->AttachControl(new VoLumVerticalLabelControl(IRECT(outLabelL, meterTop, outLabelR, meterTop + meterH), "OUT"));

    // Toggles: slide switch + label side by side
    const float toggleH = 34.f;
    const float toggleY = knobT + knobDiam + valueH + 2.f + 10.f;
    const float switchW = 60.f;
    const float switchH = toggleH;

    float ngX = mainCX - 136.f;
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(ngX, toggleY, ngX + switchW, toggleY + switchH), kNoiseGateActive, "", volumToggleStyle, switchHandleBitmap), kCtrlTagVoLumNoiseGate);
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(ngX + switchW + 4.f, toggleY, ngX + switchW + 90.f, toggleY + switchH), "NOISE GATE"), kCtrlTagVoLumNoiseGate);

    float eqX = mainCX + 30.f;
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(eqX, toggleY, eqX + switchW, toggleY + switchH), kEQActive, "", volumToggleStyle, switchHandleBitmap), kCtrlTagVoLumEQ);
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(eqX + switchW + 4.f, toggleY, eqX + switchW + 46.f, toggleY + switchH), "EQ"), kCtrlTagVoLumEQ);

    const IRECT hintArea(mainCX - 270.f, toggleY + toggleH + 10.f, mainCX + 270.f, toggleY + toggleH + 10.f + 44.f);
    pGraphics->AttachControl(new VoLumKeyboardHintControl(hintArea), kCtrlTagVoLumKeyboardHint);

    // Footer
    const IRECT footerArea(mainL, hintArea.B + 6.f, mainR, hintArea.B + 6.f + 18.f);
    pGraphics->AttachControl(new VoLumFooterControl(footerArea), kCtrlTagVoLumFooter);
    if (!mVolumLastLoadedFile.empty())
      pGraphics->GetControlWithTag(kCtrlTagVoLumFooter)->As<VoLumFooterControl>()->SetText(mVolumLastLoadedFile.c_str());

    pGraphics->AttachControl(new VoLumExactEntryControl(b, kInputLevel, "INPUT"), kCtrlTagVoLumExactEntry)->Hide(true);

"""

new_text = text[:start_idx] + new_code + text[end_idx:]

with open("NeuralAmpModeler.cpp", "w", encoding="utf-8") as f:
    f.write(new_text)

print("Patched successfully")
