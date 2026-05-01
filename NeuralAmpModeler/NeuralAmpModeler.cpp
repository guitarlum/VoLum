#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

#include "Colors.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#include "NeuralAmpModelerControls.h"
#include "VoLumAmpeteCatalog.h"
#include "VoLumPaths.h"
#include "VoLumProcessIO.h"
#if VOLUM_AMPETE_PRODUCT
#include "VoLumUserSettingsIO.h"
#include "VoLumControls.h"
#endif

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text5
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle titleStyle =
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

#if VOLUM_AMPETE_PRODUCT
const IColor kGold(255, 200, 162, 78);
const IColor kGoldDim(255, 138, 112, 48);
const IVColorSpec volumColorSpec{
  IColor(255, 17, 17, 24),        // Background
  kGold,                           // Foreground
  kGold.WithOpacity(0.3f),         // Pressed
  kGold.WithOpacity(0.25f),        // Frame
  kGold.WithOpacity(0.5f),         // Highlight (hover)
  DEFAULT_SHCOLOR,                 // Shadow
  kGold,                           // Extra 1
  COLOR_RED,                       // Extra 2 (clipping)
  kGold.WithContrast(0.1f),        // Extra 3
};
const IColor kGoldBright(255, 252, 235, 218);
const IVStyle volumStyle =
  IVStyle{true, true, volumColorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, kGoldBright},
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, kGoldBright},
          DEFAULT_HIDE_CURSOR, DEFAULT_DRAW_FRAME, false, DEFAULT_EMBOSS,
          0.2f, 2.f, DEFAULT_SHADOW_OFFSET, DEFAULT_WIDGET_FRAC, DEFAULT_WIDGET_ANGLE};
const IVStyle volumKnobStyle =
  volumStyle.WithShowLabel(false).WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(0.75f);
const IVStyle volumToggleStyle =
  volumStyle.WithShowLabel(false)
    .WithShowValue(false)
    .WithDrawFrame(false)
    .WithWidgetFrac(1.0f);
/** Settings overlay: flat controls on top of VoLumSettingsBackdropControl (no “patch” panels). */
const IVStyle volumSettingsStyle = volumStyle.WithDrawFrame(false)
                                    .WithDrawShadows(false)
                                    .WithColor(EVColor::kBG, COLOR_TRANSPARENT)
                                    .WithColor(EVColor::kFR, kGold.WithOpacity(0.22f))
                                    .WithColor(EVColor::kHL, kGold.WithOpacity(0.12f));
const IVStyle volumSettingsRadioStyle =
  volumSettingsStyle.WithShowLabel(false)
    .WithColor(EVColor::kON, kGold)
    .WithColor(EVColor::kOFF, kGold.WithOpacity(0.14f))
    .WithColor(EVColor::kX1, kGoldBright.WithOpacity(0.95f))
    // IVRadioButtonControl / IVTabSwitchControl draw option text with valueText (not labelText).
    .WithValueText(IText(14.f, kGoldBright, "Josefin-Bold", EAlign::Near, EVAlign::Middle))
    // Short stack rect in NAMSettingsPageControl; use full rect so three rows stay tight.
    .WithWidgetFrac(1.0f);
#endif

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = false;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;


NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  _InitToneStack();
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 1, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
#ifdef APP_API
  GetParam(kIRToggle)->InitBool("IRToggle", false);
#else
  GetParam(kIRToggle)->InitBool("IRToggle", true);
#endif

  // Delay
  GetParam(kDelayActive)->InitBool("DelayActive", false);
  GetParam(kDelayTime)->InitDouble("DelayTime", 380.0, 10.0, 2000.0, 1.0, "ms");
  GetParam(kDelayFeedback)->InitDouble("DelayFeedback", 0.35, 0.0, 0.99, 0.01);
  GetParam(kDelayMix)->InitDouble("DelayMix", 0.28, 0.0, 1.0, 0.01);
  GetParam(kDelayMode)->InitEnum("DelayMode", 1, {"Tape", "Digital", "Ping Pong"});

  // Reverb
  GetParam(kReverbActive)->InitBool("ReverbActive", false);
  GetParam(kReverbMix)->InitDouble("ReverbMix", 0.3, 0.0, 1.0, 0.01);
  GetParam(kReverbDecay)->InitDouble("ReverbDecay", 3.0, 0.1, 10.0, 0.1, "s");
  GetParam(kReverbTone)->InitDouble("ReverbTone", 4.5, 0.0, 10.0, 0.1);
  GetParam(kReverbPreDelay)->InitDouble("ReverbPreDelay", 20.0, 0.0, 80.0, 1.0, "ms");
  GetParam(kReverbShimmer)->InitDouble("ReverbShimmer", 0.5, 0.0, 1.0, 0.01);
  GetParam(kReverbMode)->InitEnum("ReverbMode", 0, {"Hall", "Plate", "Oktaverb"});

  // Boost (stub)
  GetParam(kBoostActive)->InitBool("BoostActive", false);
  GetParam(kBoostDrive)->InitDouble("BoostDrive", 4.5, 0.0, 10.0, 0.1);
  GetParam(kBoostTone)->InitDouble("BoostTone", 6.0, 0.0, 10.0, 0.1);
  GetParam(kBoostLevel)->InitDouble("BoostLevel", 0.0, -20.0, 20.0, 0.1, "dB");

  // PRE pedalboard
  GetParam(kPreCompActive)->InitBool("PreCompActive", false);
  GetParam(kPreCompAmount)->InitDouble("PreCompAmount", 3.0, 0.0, 10.0, 0.1);
  GetParam(kPreCompRatio)->InitDouble("PreCompRatio", 4.0, 1.0, 20.0, 0.1);
  GetParam(kPreCompAttack)->InitDouble("PreCompAttack", 4.0, 0.1, 30.0, 0.1, "ms");
  GetParam(kPreCompRelease)->InitDouble("PreCompRelease", 120.0, 20.0, 800.0, 1.0, "ms");
  GetParam(kPreCompMix)->InitDouble("PreCompMix", 1.0, 0.0, 1.0, 0.01);
  GetParam(kPreCompLevel)->InitDouble("PreCompLevel", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kPreNam1Active)->InitBool("PreNam1Active", false);
  GetParam(kPreNam1Capture)->InitDouble("PreNam1Capture", 0.0, 0.0, 127.0, 1.0);
  GetParam(kPreNam1Gain)->InitDouble("PreNam1Gain", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kPreNam1Bass)->InitDouble("PreNam1Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam1Mid)->InitDouble("PreNam1Mid", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam1MidFreq)->InitDouble("PreNam1MidFreq", 650.0, 150.0, 2500.0, 10.0, "Hz");
  GetParam(kPreNam1Treble)->InitDouble("PreNam1Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam1Level)->InitDouble("PreNam1Level", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kPreNam2Active)->InitBool("PreNam2Active", false);
  GetParam(kPreNam2Capture)->InitDouble("PreNam2Capture", 0.0, 0.0, 127.0, 1.0);
  GetParam(kPreNam2Gain)->InitDouble("PreNam2Gain", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kPreNam2Bass)->InitDouble("PreNam2Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam2Mid)->InitDouble("PreNam2Mid", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam2MidFreq)->InitDouble("PreNam2MidFreq", 650.0, 150.0, 2500.0, 10.0, "Hz");
  GetParam(kPreNam2Treble)->InitDouble("PreNam2Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam2Level)->InitDouble("PreNam2Level", 0.0, -20.0, 20.0, 0.1, "dB");

  GetParam(kVoLumAmpeteRig)
    ->InitEnum("RigFile", 0,
      {volum::kAmpeteFiles[0], volum::kAmpeteFiles[1], volum::kAmpeteFiles[2], volum::kAmpeteFiles[3],
       volum::kAmpeteFiles[4], volum::kAmpeteFiles[5], volum::kAmpeteFiles[6], volum::kAmpeteFiles[7],
       volum::kAmpeteFiles[8], volum::kAmpeteFiles[9], volum::kAmpeteFiles[10], volum::kAmpeteFiles[11],
       volum::kAmpeteFiles[12], volum::kAmpeteFiles[13], volum::kAmpeteFiles[14], volum::kAmpeteFiles[15]});
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

#if VOLUM_AMPETE_PRODUCT
  {
    auto root = volum::FindRigsRootDirectory();
    if (!root.empty())
      mVolumRigsRoot = root.string();
    _VolumLoadSettingsFromFile();
    _VolumRestoreFromSettings(mVolumAmpIdx);
    _VolumRefreshPrePedalCaptures();
    _VolumRefreshChannels();
    mVolumNeedsLoad.store(true);
    mVolumInitComplete = true;
    _VolumStartLoader();
  }
#endif

  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

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

#if VOLUM_AMPETE_PRODUCT
    // ========== VoLum Variant F Layout ==========
    const float sidebarW = 200.f;
    const float mainL = b.L + sidebarW;
    const float mainR = b.R;
    const float mainW = mainR - mainL;
    const float mainCX = mainL + mainW / 2.f;

    pGraphics->AttachControl(new VoLumBackgroundControl(b, sidebarW));
    pGraphics->AttachControl(new VoLumKnobSelectionClearControl(
      IRECT(mainL, b.T, mainR, b.B),
      [this]() {
        _ClearVoLumKnobSelection();
        _VolumHidePreCaptureMenu();
      }));

    // Sidebar: logo
    const IRECT logoArea(b.L, b.T + 8.f, b.L + sidebarW, b.T + 48.f);
    pGraphics->AttachControl(new VoLumLogoControl(logoArea));

    // Sidebar: amp list (names + abbreviations from catalog)
    static const char* ampNames[volum::kAmpCount];
    static const char* ampAbbrs[volum::kAmpCount] = {
      "A1", "BC", "BX", "DH", "FD", "HK", "LP", "M4", "MJ", "MV", "O1", "O2", "ST", "SL", "TC"
    };
    for (int i = 0; i < volum::kAmpCount; i++)
      ampNames[i] = volum::kAmps[i].displayName;

    const IRECT ampListArea(b.L + 6.f, logoArea.B + 4.f, b.L + sidebarW - 6.f, b.B - 8.f);
    pGraphics->AttachControl(
      new VoLumAmpListControl(
        ampListArea, volum::kAmpCount, ampNames, ampAbbrs,
        [this](int ampIdx) {
          _VolumSaveCurrentToSettings();
          mVolumAmpIdx = ampIdx;
          _VolumRestoreFromSettings(ampIdx);
          _VolumRefreshChannels();
          mVolumNeedsLoad.store(true);
          _VolumSaveSettingsToFile();

          auto* pGfx = GetUI();
          if (!pGfx) return;
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
          
          if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych)) {
             auto* trip = tripCtrl->As<VoLumTriptychControl>();
             const bool preActive = GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
             trip->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), ampIdx, volum::kAmps[ampIdx].displayName);
          }
        }),
      kCtrlTagVoLumAmpList);

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

    const float contentH = speakerH + 6.f + heroH + 4.f + nameH
                          + gapAfterAmpName + ampToKnobHairlineH + gapAfterHairline
                          + labelH + knobDiam + valueH + 2.f + 10.f + toggleH + hintGap + hintH + 6.f + footerH;
    const float contentTop = b.T + (b.H() - contentH) / 2.f;

    // Speaker mode row
    float yPos = contentTop;
    const IRECT speakerArea(mainL, yPos, mainR, yPos + speakerH);
    pGraphics->AttachControl(
      new VoLumSpeakerRowControl(speakerArea,
        [this](int speakerIdx) {
          mVolumSpeakerIdx = speakerIdx;
          mVolumAmpSettings[mVolumAmpIdx].speakerIdx = speakerIdx;
          mVolumSettingsDirty = true;
          _VolumRefreshChannels();
          mVolumNeedsLoad.store(true);
        }),
      kCtrlTagVoLumSpeakerRow);
    yPos += speakerH + 6.f;

    // Triptych (PRE | AMP | POST)
    const auto triptychBounds = volum::triptych_layout::BoundsForCenter(mainCX, yPos);
    const IRECT triptychArea = triptychBounds.As<IRECT>();
    
    auto* triptych = new VoLumTriptychControl(triptychArea, [this](EVoLumSection sec, EVoLumEffectFocus focus) {
        mVolumExpandedSection = sec;
        mVolumFocusedEffect = focus;
        _UpdateVoLumLayout();
    });
    pGraphics->AttachControl(triptych, kCtrlTagVoLumTriptych);

    // Re-attach VoLumHeroImageControl so the procedural art can actually draw
    // The triptych provides a space for it, but the hero control holds the fractal caching logic.
    // It should be centered within the AMP-expanded area of the triptych.
    // When AMP is expanded, the center of the expanded section is exactly at `mainCX`
    const float newHeroW = volum::triptych_layout::kAmpExpandedW;
    const IRECT heroArea(mainCX - newHeroW / 2.f, yPos, mainCX + newHeroW / 2.f, triptychArea.B);
    pGraphics->AttachControl(new VoLumHeroImageControl(heroArea), kCtrlTagVoLumHeroImage);

    const auto preCards = volum::triptych_layout::ComputePreCards(
      volum::triptych_layout::ComputeFrames(triptychBounds, EVoLumSection::PRE).pre);
    const auto postCards = volum::triptych_layout::ComputePostCards(
      volum::triptych_layout::ComputeFrames(triptychBounds, EVoLumSection::POST).post);

    auto onPedalClick = [this](VoLumPedalCardControl* card, bool isBypassClick) {
        (void) isBypassClick;
        mVolumFocusedEffect = card->GetEffect();
        _UpdateVoLumLayout();
    };

    auto* delayCard = new VoLumPedalCardControl(postCards.delay.As<IRECT>(), EVoLumEffectFocus::DELAY, onPedalClick);
    auto* reverbCard = new VoLumPedalCardControl(postCards.reverb.As<IRECT>(), EVoLumEffectFocus::REVERB, onPedalClick);
    auto* chainLink = new VoLumChainConnectorControl(postCards.connector.As<IRECT>());
    auto* compCard = new VoLumPedalCardControl(preCards.comp.As<IRECT>(), EVoLumEffectFocus::COMP, onPedalClick);
    auto* preNam1Card = new VoLumPedalCardControl(preCards.nam1.As<IRECT>(), EVoLumEffectFocus::PRE_NAM1, onPedalClick);
    auto* preNam2Card = new VoLumPedalCardControl(preCards.nam2.As<IRECT>(), EVoLumEffectFocus::PRE_NAM2, onPedalClick);
    auto* preChainLink1 = new VoLumChainConnectorControl(preCards.connector1.As<IRECT>());
    auto* preChainLink2 = new VoLumChainConnectorControl(preCards.connector2.As<IRECT>());
    
    pGraphics->AttachControl(compCard, kCtrlTagVoLumCompCard)->Hide(true);
    pGraphics->AttachControl(preChainLink1, kCtrlTagVoLumPreChainConnector1)->Hide(true);
    pGraphics->AttachControl(preNam1Card, kCtrlTagVoLumPreNam1Card)->Hide(true);
    pGraphics->AttachControl(preChainLink2, kCtrlTagVoLumPreChainConnector2)->Hide(true);
    pGraphics->AttachControl(preNam2Card, kCtrlTagVoLumPreNam2Card)->Hide(true);
    pGraphics->AttachControl(delayCard, kCtrlTagVoLumDelayCard)->Hide(true);
    pGraphics->AttachControl(chainLink, kCtrlTagVoLumChainConnector)->Hide(true);
    pGraphics->AttachControl(reverbCard, kCtrlTagVoLumReverbCard)->Hide(true);

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

    auto drawKnobCol = [&](int slot, const char* label, int paramId, const char* suffix, const char* group,
                           bool center_offset = false, int centerSlots = 3, int centerStart = 2,
                           float centerOffset = 0.f, float centerColW = 80.f) {
      float customColW = center_offset ? centerColW : colW;
      float cx = center_offset ? (mainCX + centerOffset - (centerSlots * customColW) / 2.f + (slot - centerStart) * customColW + (customColW / 2.f)) : knobX(slot) + (colW / 2.f);
      float kL = cx - (knobDiam / 2.f);

      // Use a wider label rect (-40.f to +40.f = 80px wide) to prevent "FEEDBACK" clipping
      pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(cx - 40.f, knobRowTop, cx + 40.f, knobRowTop + 20.f), label), -1, group);
      auto* knob = new NAMKnobControl(IRECT(kL, knobT, kL + knobDiam, knobT + knobDiam), paramId, "", volumKnobStyle, knobBackgroundBitmap);
      pGraphics->AttachControl(knob, -1, group);
      knob->SetSelectedForKeyboard(mVolumSelectedKnobParamIdx == paramId);
      pGraphics->AttachControl(new VoLumParamValueControl(IRECT(cx - 30.f, knobT + knobDiam + 2.f, cx + 30.f, knobT + knobDiam + 2.f + valueH), paramId, suffix), -1, group);
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
    const float effectKnobOffset = -38.f;
    const float effectColW = 70.f;
    drawKnobCol(1, "MIX", kReverbMix, "%", "REVERB_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(2, "DECAY", kReverbDecay, "s", "REVERB_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(3, "TONE", kReverbTone, "", "REVERB_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(4, "PRE-DLY", kReverbPreDelay, "ms", "REVERB_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(5, "SHIMMER", kReverbShimmer, "%", "REVERB_SHIMMER", true, 5, 1, effectKnobOffset, effectColW);
    IRECT reverbPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    pGraphics->AttachControl(new VoLumModePickerControl(reverbPickerRect, kReverbMode, {"HALL", "PLATE", "OKTAVERB"}), -1, "REVERB_KNOBS");
    
    float revSwX = mainCX - 242.f;
    pGraphics->AttachControl(new VoLumPowerSwitchControl(IRECT(revSwX - 14.f, knobT - 4.f, revSwX + 14.f, knobT + knobDiam + 2.f), kReverbActive), -1, "REVERB_POWER");

    // DELAY KNOBS (Centered)
    drawKnobCol(2, "TIME", kDelayTime, "ms", "DELAY_KNOBS", true, 3, 2, effectKnobOffset, effectColW);
    drawKnobCol(3, "FEEDBACK", kDelayFeedback, "%", "DELAY_KNOBS", true, 3, 2, effectKnobOffset, effectColW);
    drawKnobCol(4, "MIX", kDelayMix, "%", "DELAY_KNOBS", true, 3, 2, effectKnobOffset, effectColW);
    IRECT delayPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    pGraphics->AttachControl(new VoLumModePickerControl(delayPickerRect, kDelayMode, {"TAPE", "DIGITAL", "PING PONG"}), -1, "DELAY_KNOBS");

    float dlySwX = mainCX - 242.f;
    pGraphics->AttachControl(new VoLumPowerSwitchControl(IRECT(dlySwX - 14.f, knobT - 4.f, dlySwX + 14.f, knobT + knobDiam + 2.f), kDelayActive), -1, "DELAY_POWER");

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

    drawKnobCol(1, "AMOUNT", kPreCompAmount, "", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(2, "RATIO", kPreCompRatio, ":1", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(3, "ATTACK", kPreCompAttack, "ms", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(4, "RELEASE", kPreCompRelease, "ms", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(5, "MIX", kPreCompMix, "%", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(6, "LEVEL", kPreCompLevel, "dB", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);

    const float preSwX = mainCX - 242.f;
    pGraphics->AttachControl(new VoLumPowerSwitchControl(IRECT(preSwX - 14.f, knobT - 4.f, preSwX + 14.f, knobT + knobDiam + 2.f), kPreCompActive), -1, "COMP_POWER");
    pGraphics->AttachControl(new VoLumPowerSwitchControl(IRECT(preSwX - 14.f, knobT - 4.f, preSwX + 14.f, knobT + knobDiam + 2.f), kPreNam1Active), -1, "PRE_NAM1_POWER");
    pGraphics->AttachControl(new VoLumPowerSwitchControl(IRECT(preSwX - 14.f, knobT - 4.f, preSwX + 14.f, knobT + knobDiam + 2.f), kPreNam2Active), -1, "PRE_NAM2_POWER");

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
    const float toggleY = knobT + knobDiam + valueH + 2.f + 10.f;
    const float switchW = 60.f;
    const float switchH = toggleH;

    float ngX = mainCX - 136.f;
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(ngX, toggleY, ngX + switchW, toggleY + switchH), kNoiseGateActive, "", volumToggleStyle, switchHandleBitmap), kCtrlTagVoLumNoiseGate, "AMP_TOGGLES");
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(ngX + switchW + 4.f, toggleY, ngX + switchW + 90.f, toggleY + switchH), "NOISE GATE"), -1, "AMP_TOGGLES");

    float eqX = mainCX + 30.f;
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(eqX, toggleY, eqX + switchW, toggleY + switchH), kEQActive, "", volumToggleStyle, switchHandleBitmap), kCtrlTagVoLumEQ, "AMP_TOGGLES");
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(eqX + switchW + 4.f, toggleY, eqX + switchW + 46.f, toggleY + switchH), "EQ"), -1, "AMP_TOGGLES");

    const IRECT hintArea(mainCX - 270.f, toggleY + toggleH + 10.f, mainCX + 270.f, toggleY + toggleH + 10.f + 44.f);
    pGraphics->AttachControl(new VoLumKeyboardHintControl(hintArea), kCtrlTagVoLumKeyboardHint);

    // Footer
    const IRECT footerArea(mainL, hintArea.B + 6.f, mainR, hintArea.B + 6.f + 18.f);
    pGraphics->AttachControl(new VoLumFooterControl(footerArea), kCtrlTagVoLumFooter);
    if (!mVolumLastLoadedFile.empty())
      pGraphics->GetControlWithTag(kCtrlTagVoLumFooter)->As<VoLumFooterControl>()->SetText(mVolumLastLoadedFile.c_str());

    pGraphics->AttachControl(new VoLumExactEntryControl(b, kInputLevel, "INPUT"), kCtrlTagVoLumExactEntry)->Hide(true);
    pGraphics->AttachControl(new VoLumPreCaptureMenuControl(IRECT(mainL, knobRowTop, mainL + 220.f, knobRowTop + 160.f)),
                             kCtrlTagVoLumPreCaptureMenu)->Hide(true);

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
      pGraphics->AttachControl(new NAMCircleButtonControl(
        tunerArea,
        [pPlugin](IControl*) { pPlugin->_ToggleVoLumTuner(); },
        tunerSVG));

      // Metronome button
      pGraphics->AttachControl(new VoLumMetronomeButtonControl(
        metronomeArea,
        [pPlugin](IControl*) { pPlugin->_ToggleVoLumMetronomePanel(); },
        metronomeSVG), kCtrlTagVoLumMetronomeButton);

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

    // Apply loaded settings: select correct amp, speaker, hero image
    {
      auto* ampList = pGraphics->GetControlWithTag(kCtrlTagVoLumAmpList)->As<VoLumAmpListControl>();
      auto* spkRow = pGraphics->GetControlWithTag(kCtrlTagVoLumSpeakerRow)->As<VoLumSpeakerRowControl>();
      auto* heroCtrl = pGraphics->GetControlWithTag(kCtrlTagVoLumHeroImage)->As<VoLumHeroImageControl>();
      auto* nameCtrl = pGraphics->GetControlWithTag(kCtrlTagVoLumSubRowText)->As<VoLumSubRowTextControl>();

      if (ampList) ampList->SetSelected(mVolumAmpIdx);
      if (spkRow) spkRow->SetSelected(mVolumSpeakerIdx);
      if (nameCtrl && mVolumExpandedSection == EVoLumSection::AMP) nameCtrl->SetName(volum::kAmps[mVolumAmpIdx].displayName, true);
      if (heroCtrl)
      {
        char ph[4] = {volum::kAmps[mVolumAmpIdx].displayName[0], (char)('0' + (mVolumAmpIdx % 10)), 0, 0};
        heroCtrl->SetPlaceholder(ph, mVolumAmpIdx);
        heroCtrl->SetName(volum::kAmps[mVolumAmpIdx].displayName);
      }
    }

    _SyncVoLumExactEntry();

    // Keyboard: Up/Down = switch amps, Left/Right = switch channels
    pGraphics->SetKeyHandlerFunc([this](const IKeyPress& key, bool isUp) {
      if (isUp) return false;

      if (auto* pGfx = GetUI())
      {
        if (pGfx->GetControlInTextEntry())
          return false;

        if (auto* settings = pGfx->GetControlWithTag(kCtrlTagSettingsBox))
        {
          if (!settings->IsHidden())
            return false;
        }
      }

      if (_HandleVoLumSelectedKnobKey(key))
        return true;

      if (key.VK == kVK_UP || key.VK == kVK_DOWN)
      {
        int newIdx = (key.VK == kVK_UP)
          ? (mVolumAmpIdx - 1 + volum::kAmpCount) % volum::kAmpCount
          : (mVolumAmpIdx + 1) % volum::kAmpCount;
        // Simulate sidebar click via the same path
        _ClearVoLumKnobSelection();
        _VolumSaveCurrentToSettings();
        mVolumAmpIdx = newIdx;
        _VolumRestoreFromSettings(newIdx);
        _VolumRefreshChannels();
        mVolumNeedsLoad.store(true);
        _VolumSaveSettingsToFile();
        if (auto* pGfx = GetUI())
        {
          if (auto* ampList = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
            ampList->As<VoLumAmpListControl>()->SetSelected(newIdx);
          if (auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
          {
            char ph[4] = {volum::kAmps[newIdx].displayName[0], (char)('0' + (newIdx % 10)), 0, 0};
            heroCtrl->As<VoLumHeroImageControl>()->SetPlaceholder(ph, newIdx);
            heroCtrl->As<VoLumHeroImageControl>()->SetName(volum::kAmps[newIdx].displayName);
          }
          if (auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
            if (mVolumExpandedSection == EVoLumSection::AMP)
              nameCtrl->As<VoLumSubRowTextControl>()->SetName(volum::kAmps[newIdx].displayName, true);
          if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych)) {
             const bool preActive = GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
             tripCtrl->As<VoLumTriptychControl>()->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), newIdx, volum::kAmps[newIdx].displayName);
          }
        }
        return true;
      }
      if (key.VK == kVK_LEFT || key.VK == kVK_RIGHT)
      {
        if (mVolumSelectedKnobParamIdx != kNoParameter)
          return false;

        if (mVolumExpandedSection == EVoLumSection::AMP)
        {
          if (auto* pGfx = GetUI())
            if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumChannelStep))
            {
              auto* s = stepper->As<VoLumChannelStepControl>();
              int n = s->GetNumChannels();
              if (n > 0)
              {
                int newCh = (key.VK == kVK_LEFT)
                  ? (s->GetSelected() - 1 + n) % n
                  : (s->GetSelected() + 1) % n;
                s->SetChannels(mVolumChannelLabels, newCh);
                mVolumChannelIdx = newCh;
                mVolumNeedsLoad.store(true);
              }
            }
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

#else
    // ========== Original NAM Layout ==========
    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);

    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto titleHeight = 50.0f;
    const auto titleArea = contentArea.GetFromTop(titleHeight);

    const auto knobsPad = 20.0f;
    const auto knobsExtraSpaceBelowTitle = 25.0f;
    const auto singleKnobPad = -2.0f;
    const auto knobsArea = contentArea.GetFromTop(NAM_KNOB_HEIGHT)
                             .GetReducedFromLeft(knobsPad)
                             .GetReducedFromRight(knobsPad)
                             .GetVShifted(titleHeight + knobsExtraSpaceBelowTitle);
    const auto inputKnobArea = knobsArea.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto noiseGateArea = knobsArea.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto bassKnobArea = knobsArea.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto midKnobArea = knobsArea.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto trebleKnobArea = knobsArea.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto outputKnobArea = knobsArea.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const auto ngToggleArea =
      noiseGateArea.GetVShifted(noiseGateArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto eqToggleArea = midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    const auto fileWidth = 200.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);
    const auto settingsButtonArea = CornerButtonArea(b);

    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        const std::string msg = _StageModel(fileName);
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };

    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);
          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "NEURAL AMP MODELER", titleStyle));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";
    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(), "nam",
                                loadModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl),
      kCtrlTagModelFileBrowser);
    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl),
      kCtrlTagIRFileBrowser);

    pGraphics->AttachControl(
      new NAMSwitchControl(ngToggleArea, kNoiseGateActive, "Noise Gate", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));

    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(
      new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(midKnobArea, kToneMid, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));

    pGraphics->AttachControl(new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);
#endif // VOLUM_AMPETE_PRODUCT

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
#if VOLUM_AMPETE_PRODUCT
  _VolumStopLoader();
  _VolumSaveCurrentToSettings();
  _VolumSaveSettingsToFile();
#endif
  _DeallocateIOPointers();
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);

#if VOLUM_AMPETE_PRODUCT
  // Tuner reads from mono input (post-gain, pre-NAM)
  mTunerDSP.Process(mInputPointers[0], nFrames);
#endif
  _ApplyDSPStaging();
  sample** preAmpPointers = mInputPointers;
#if VOLUM_AMPETE_PRODUCT
  if (GetParam(kPreCompActive)->Bool())
  {
    mPreCompressor.SetParams(GetParam(kPreCompAmount)->Value(), GetParam(kPreCompRatio)->Value(),
                             GetParam(kPreCompAttack)->Value(), GetParam(kPreCompRelease)->Value(),
                             GetParam(kPreCompMix)->Value(), GetParam(kPreCompLevel)->Value(), sampleRate);
    preAmpPointers = mPreCompressor.Process(preAmpPointers, numChannelsInternal, numFrames);
  }

  auto processPreSlot = [&](int slot, int activeParam, int gainParam, int bassParam, int midParam, int midFreqParam,
                            int trebleParam, int levelParam) {
    if (!GetParam(activeParam)->Bool() || mPreModel[slot] == nullptr)
      return;

    const double inGain = std::pow(10.0, GetParam(gainParam)->Value() / 20.0);
    mPreInputGain[slot].SetParams(recursive_linear_filter::LevelParams(inGain));
    preAmpPointers = mPreInputGain[slot].Process(preAmpPointers, numChannelsInternal, numFrames);

    mPreModel[slot]->process(preAmpPointers[0], mOutputPointers[0], nFrames);
    preAmpPointers = mOutputPointers;

    mPreEq[slot].SetParams(GetParam(bassParam)->Value(), GetParam(midParam)->Value(),
                           GetParam(midFreqParam)->Value(), GetParam(trebleParam)->Value());
    preAmpPointers = mPreEq[slot].Process(preAmpPointers, numChannelsInternal, numFrames);

    const double outGain = std::pow(10.0, GetParam(levelParam)->Value() / 20.0);
    mPreOutputGain[slot].SetParams(recursive_linear_filter::LevelParams(outGain));
    preAmpPointers = mPreOutputGain[slot].Process(preAmpPointers, numChannelsInternal, numFrames);
  };

  processPreSlot(0, kPreNam1Active, kPreNam1Gain, kPreNam1Bass, kPreNam1Mid, kPreNam1MidFreq, kPreNam1Treble,
                 kPreNam1Level);
  processPreSlot(1, kPreNam2Active, kPreNam2Gain, kPreNam2Bass, kPreNam2Mid, kPreNam2MidFreq, kPreNam2Treble,
                 kPreNam2Level);
#endif
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = preAmpPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(preAmpPointers, numChannelsInternal, numFrames);
  }

  const bool haveMainModel = (mModel != nullptr);
  if (haveMainModel)
  {
    mModel->process(triggerOutput[0], mOutputPointers[0], nFrames);
  }
  else
  {
    _FallbackDSP(triggerOutput, mOutputPointers, numChannelsInternal, numFrames);
#if VOLUM_AMPETE_PRODUCT
    if (!mPostEffectsClearedForMissingModel)
    {
      mDelay.Reset();
      mReverb.Reset();
      mPostEffectsClearedForMissingModel = true;
    }
#endif
  }
  if (haveMainModel)
    mPostEffectsClearedForMissingModel = false;

  sample** hpfPointers = mOutputPointers;
  if (haveMainModel)
  {
    // Apply the noise gate after the NAM
    sample** gateGainOutput =
      noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

    sample** toneStackOutPointers = (toneStackActive && mToneStack != nullptr)
                                      ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                      : gateGainOutput;

    sample** irPointers = toneStackOutPointers;
    if (mIR != nullptr && GetParam(kIRToggle)->Value())
      irPointers = mIR->Process(toneStackOutPointers, numChannelsInternal, numFrames);

    // And the HPF for DC offset (Issue 271)
    const double highPassCutoffFreq = kDCBlockerFrequency;
    // const double lowPassCutoffFreq = 20000.0;
    const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
    // const recursive_linear_filter::LowPassParams lowPassParams(sampleRate, lowPassCutoffFreq);
    mHighPass.SetParams(highPassParams);
    // mLowPass.SetParams(lowPassParams);
    hpfPointers = mHighPass.Process(irPointers, numChannelsInternal, numFrames);
    // sample** lpfPointers = mLowPass.Process(hpfPointers, numChannelsInternal, numFrames);
  }

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);

  // Apply POST effects (Delay -> Reverb) in stereo
  iplug::sample** postPointers = outputs;

  if (haveMainModel && GetParam(kDelayActive)->Value())
  {
    mDelay.SetParams(GetParam(kDelayTime)->Value(), GetParam(kDelayFeedback)->Value(),
                     GetParam(kDelayMix)->Value(), GetParam(kDelayMode)->Int(), sampleRate);
    postPointers = mDelay.Process(postPointers, numChannelsExternalOut, numFrames);
  }

  if (haveMainModel && GetParam(kReverbActive)->Value())
  {
    mReverb.SetParams(GetParam(kReverbMix)->Value(), GetParam(kReverbDecay)->Value(),
                      GetParam(kReverbTone)->Value(), GetParam(kReverbPreDelay)->Value(),
                      GetParam(kReverbShimmer)->Value(), GetParam(kReverbMode)->Int(), sampleRate);
    postPointers = mReverb.Process(postPointers, numChannelsExternalOut, numFrames);
  }

  if (postPointers != outputs)
  {
    for (size_t c = 0; c < numChannelsExternalOut; c++)
      std::memcpy(outputs[c], postPointers[c], numFrames * sizeof(iplug::sample));
  }

#if VOLUM_AMPETE_PRODUCT
  // Metronome: sum click into output
  mMetronomeDSP.Process(outputs, nFrames, static_cast<int>(numChannelsExternalOut));

  // Tuner active: silence output so player can tune without hearing amp
  if (mTunerDSP.IsActive())
  {
    for (size_t c = 0; c < numChannelsExternalOut; c++)
      std::memset(outputs[c], 0, numFrames * sizeof(iplug::sample));
  }
#endif

  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mToneStack->Reset(sampleRate, maxBlockSize);
  for (int i = 0; i < 2; ++i)
    mPreEq[i].Reset(sampleRate, maxBlockSize);
  mPreCompressor.Reset();
  const size_t postEffectChannels = std::max<size_t>(1, static_cast<size_t>(NOutChansConnected()));
  mDelay.Prepare(postEffectChannels, static_cast<size_t>(maxBlockSize), sampleRate);
  mReverb.Prepare(postEffectChannels, static_cast<size_t>(maxBlockSize), sampleRate);
  mDelay.Reset();
  mReverb.Reset();
#if VOLUM_AMPETE_PRODUCT
  mTunerDSP.Reset(sampleRate);
  mMetronomeDSP.Reset(sampleRate);
#endif
  _UpdateLatency();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

#if VOLUM_AMPETE_PRODUCT
  // Push tuner result to UI
  if (mTunerDSP.IsActive())
  {
    if (auto* pGfx = GetUI())
    {
      if (auto* tuner = pGfx->GetControlWithTag(kCtrlTagVoLumTuner))
        tuner->As<VoLumTunerControl>()->SetResult(mTunerDSP.GetResult());
    }
  }

  if (mVolumNeedsLoad.load() && !mVolumIsLoading.load())
  {
    mVolumNeedsLoad.store(false);

    // Capture path on main thread to avoid races with _VolumRefreshChannels
    std::string fileToLoad;
    if (mVolumChannelIdx >= 0 && mVolumChannelIdx < (int)mVolumChannelFiles.size())
    {
      namespace fs = std::filesystem;
      auto rigPath = fs::path(mVolumRigsRoot)
        / volum::kAmps[mVolumAmpIdx].folderName
        / mVolumChannelFiles[mVolumChannelIdx];
      fileToLoad = fs::weakly_canonical(rigPath).string();
    }

    if (!fileToLoad.empty())
    {
      mVolumLastLoadedFile = std::filesystem::path(fileToLoad).filename().string();
      if (auto* pGfx = GetUI())
      {
        if (auto* footer = pGfx->GetControlWithTag(kCtrlTagVoLumFooter))
          footer->As<VoLumFooterControl>()->SetText(mVolumLastLoadedFile.c_str());
      }

      const int ampIdx = mVolumAmpIdx;
      const std::string rigsRoot = mVolumRigsRoot;
      mVolumIsLoading.store(true);
      _VolumQueueMainModelLoad(fileToLoad, ampIdx, rigsRoot);
    }
    else
    {
      mVolumIsLoading.store(false);
    }
  }

  for (int slot = 0; slot < 2; ++slot)
  {
    if (mVolumPreNeedsLoad[slot].load() && !mVolumPreIsLoading[slot].load())
    {
      mVolumPreNeedsLoad[slot].store(false);
      _VolumRequestPreNamLoad(slot);
    }
  }

  // Always keep in-memory settings current (OnIdle runs on main thread, params valid)
  if (mVolumInitComplete)
    _VolumSaveCurrentToSettings();

  // Write settings file when dirty (knob/speaker/channel changed)
  if (mVolumSettingsDirty)
  {
    mVolumSettingsDirty = false;
    _VolumSaveSettingsToFile();
  }
#endif

  if (mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewModelLoadedInDSP = false;
    }
  }
  if (mModelCleared)
  {
    if (auto* pGraphics = GetUI())
    {
      static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->ClearModelInfo();
      mModelCleared = false;
    }
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  WDL_String header("###NeuralAmpModeler###"); // Don't change this!
  chunk.PutStr(header.Get());
  WDL_String version(PLUG_VERSION_STR);
  chunk.PutStr(version.Get());
  chunk.PutStr(mNAMPath.Get());
  chunk.PutStr(mIRPath.Get());
  bool ok = SerializeParams(chunk);

#if VOLUM_AMPETE_PRODUCT
  // VoLum: append per-amp settings after params (see Unserialization.cpp)
  chunk.Put(&mVolumAmpIdx);
  chunk.Put(&mVolumSpeakerIdx);
  chunk.Put(&mVolumChannelIdx);
  for (int i = 0; i < volum::kAmpCount; i++)
  {
    const auto& s = mVolumAmpSettings[i];
    chunk.Put(&s.speakerIdx);
    chunk.Put(&s.channelIdx);
    chunk.Put(&s.inputLevel);
    chunk.Put(&s.gateThreshold);
    chunk.Put(&s.toneBass);
    chunk.Put(&s.toneMid);
    chunk.Put(&s.toneTreble);
    chunk.Put(&s.outputLevel);
    int ng = s.noiseGateActive ? 1 : 0;
    int eq = s.eqActive ? 1 : 0;
    chunk.Put(&ng);
    chunk.Put(&eq);
    int pc = s.preCompActive ? 1 : 0;
    int p1 = s.preNam1Active ? 1 : 0;
    int p2 = s.preNam2Active ? 1 : 0;
    chunk.Put(&pc);
    chunk.Put(&s.preCompAmount);
    chunk.Put(&s.preCompRatio);
    chunk.Put(&s.preCompAttack);
    chunk.Put(&s.preCompRelease);
    chunk.Put(&s.preCompMix);
    chunk.Put(&s.preCompLevel);
    chunk.Put(&p1);
    chunk.Put(&s.preNam1Capture);
    chunk.Put(&s.preNam1Gain);
    chunk.Put(&s.preNam1Bass);
    chunk.Put(&s.preNam1Mid);
    chunk.Put(&s.preNam1MidFreq);
    chunk.Put(&s.preNam1Treble);
    chunk.Put(&s.preNam1Level);
    chunk.Put(&p2);
    chunk.Put(&s.preNam2Capture);
    chunk.Put(&s.preNam2Gain);
    chunk.Put(&s.preNam2Bass);
    chunk.Put(&s.preNam2Mid);
    chunk.Put(&s.preNam2MidFreq);
    chunk.Put(&s.preNam2Treble);
    chunk.Put(&s.preNam2Level);
  }
#endif

  return ok;
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  // Look for the expected header. If it's there, then we'll know what to do.
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  const char* kExpectedHeader = "###NeuralAmpModeler###";
  if (strcmp(header.Get(), kExpectedHeader) == 0)
  {
    return _UnserializeStateWithKnownVersion(chunk, pos);
  }
  else
  {
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();

#if !VOLUM_AMPETE_PRODUCT
  if (mNAMPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
    if (mModel == nullptr && mStagedModel == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
  }
#endif

#if !VOLUM_AMPETE_PRODUCT
#ifndef APP_API
  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }
#endif
#endif

  if (mModel != nullptr)
  {
    _UpdateControlsFromModel();
  }
}

void NeuralAmpModeler::OnUIClose()
{
#if VOLUM_AMPETE_PRODUCT
  // Save while params are still valid (destructor may run after teardown)
  _VolumSaveCurrentToSettings();
  _VolumSaveSettingsToFile();
#endif
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    // Changes to the input gain
    case kCalibrateInput:
    case kInputCalibrationLevel:
    case kInputLevel: _SetInputGain(); break;
    // Changes to the output gain
    case kOutputLevel:
    case kOutputMode: _SetOutputGain(); break;
    // Tone stack:
    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
#if VOLUM_AMPETE_PRODUCT
    case kDelayActive:
    case kDelayTime:
    case kDelayFeedback:
    case kDelayMix:
      if (mVolumInitComplete)
      {
        mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
        mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
        _VolumSaveDelayModeSnapshot(std::clamp(GetParam(kDelayMode)->Int(), 0, 2));
      }
      break;
    case kReverbActive:
    case kReverbMix:
    case kReverbDecay:
    case kReverbTone:
    case kReverbPreDelay:
    case kReverbShimmer:
      if (mVolumInitComplete)
      {
        mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
        mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
        _VolumSaveReverbModeSnapshot(std::clamp(GetParam(kReverbMode)->Int(), 0, 2));
      }
      break;
    case kPreNam1Capture:
    case kPreNam1Active:
      if (mVolumInitComplete)
        mVolumPreNeedsLoad[0].store(true);
      break;
    case kPreNam2Capture:
    case kPreNam2Active:
      if (mVolumInitComplete)
        mVolumPreNeedsLoad[1].store(true);
      break;
    case kVoLumAmpeteRig: break; // handled by callback-based channel stepper
#endif
    default: break;
  }

#if VOLUM_AMPETE_PRODUCT
  if (mVolumInitComplete)
    mVolumSettingsDirty = true;
#endif
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kDelayActive:
      case kReverbActive:
      case kPreCompActive:
      case kPreNam1Active:
      case kPreNam2Active:
      case kPreNam1Capture:
      case kPreNam2Capture:
#if VOLUM_AMPETE_PRODUCT
        _UpdateVoLumLayout(pGraphics);
#endif
        break;
      case kEQActive:
        pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
#if VOLUM_AMPETE_PRODUCT
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneBass)) c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneMid)) c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneTreble)) c->SetDisabled(!active);
#endif
        break;
#if !VOLUM_AMPETE_PRODUCT
#ifndef APP_API
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active); break;
#endif
#else
      case kDelayMode:
      {
        const int oldMode = std::clamp(mVolumEffectSettings.delayMode, 0, 2);
        _VolumSaveDelayModeSnapshot(oldMode);
        const int newMode = std::clamp(GetParam(kDelayMode)->Int(), 0, 2);
        mVolumEffectSettings.delayMode = newMode;
        _VolumRestoreDelayModeSnapshot(newMode);
        break;
      }
      case kReverbMode:
      {
        const int oldMode = std::clamp(mVolumEffectSettings.reverbMode, 0, 2);
        _VolumSaveReverbModeSnapshot(oldMode);
        const int newMode = std::clamp(GetParam(kReverbMode)->Int(), 0, 2);
        mVolumEffectSettings.reverbMode = newMode;
        _VolumRestoreReverbModeSnapshot(newMode);
        _UpdateVoLumLayout(pGraphics);
        break;
      }
#endif
      default: break;
    }
  }
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel: mShouldRemoveModel = true; return true;
    case kMsgTagClearIR: mShouldRemoveIR = true; return true;
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);

      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());

            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }

      return true;
    }
    default: return false;
  }
}

// Private methods ============================================================

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_ApplyDSPStaging()
{
#if VOLUM_AMPETE_PRODUCT
  _VolumDrainLoaderResults();
#endif

  // Remove marked modules
  if (mShouldRemoveModel)
  {
    mModel = nullptr;
    mNAMPath.Set("");
    mShouldRemoveModel = false;
    mModelCleared = true;
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  if (mShouldRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  for (int i = 0; i < 2; ++i)
  {
    if (mShouldRemovePreModel[i])
    {
      mPreModel[i] = nullptr;
      mShouldRemovePreModel[i] = false;
    }
  }
  // Move things from staged to live
  if (mStagedModel != nullptr)
  {
    mModel = std::move(mStagedModel);
    mStagedModel = nullptr;
    mNewModelLoadedInDSP = true;
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  for (int i = 0; i < 2; ++i)
  {
    if (mStagedPreModel[i] != nullptr)
    {
      mPreModel[i] = std::move(mStagedPreModel[i]);
      mStagedPreModel[i] = nullptr;
    }
  }
  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }
}

void NeuralAmpModeler::_DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  (void) inputs;
  volum::process_io::ClearBuffers(outputs, numFrames, numChannels);
}

void NeuralAmpModeler::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  // Model
  if (mStagedModel != nullptr)
  {
    mStagedModel->Reset(sampleRate, maxBlockSize);
  }
  else if (mModel != nullptr)
  {
    mModel->Reset(sampleRate, maxBlockSize);
  }

  for (int i = 0; i < 2; ++i)
  {
    if (mStagedPreModel[i] != nullptr)
      mStagedPreModel[i]->Reset(sampleRate, maxBlockSize);
    else if (mPreModel[i] != nullptr)
      mPreModel[i]->Reset(sampleRate, maxBlockSize);
  }

  // IR
  if (mStagedIR != nullptr)
  {
    const double irSampleRate = mStagedIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
}

void NeuralAmpModeler::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  // Input calibration
  if ((mModel != nullptr) && (mModel->HasInputLevel()) && GetParam(kCalibrateInput)->Bool())
  {
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - mModel->GetInputLevel();
  }
  mInputGain = DBToAmp(inputGainDB);
}

void NeuralAmpModeler::_SetOutputGain()
{
  double gainDB = GetParam(kOutputLevel)->Value();
  if (mModel != nullptr)
  {
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mModel->HasLoudness())
        {
          const double loudness = mModel->GetLoudness();
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mModel->HasOutputLevel())
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mModel->GetOutputLevel();
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 0: // Raw
      default: break;
    }
  }
  mOutputGain = DBToAmp(gainDB);
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = mNAMPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath);
    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), GetBlockSize());
    mStagedModel = std::move(temp);
    mNAMPath = modelPath;
#if !VOLUM_AMPETE_PRODUCT
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
#endif
  }
  catch (std::runtime_error& e)
  {
#if !VOLUM_AMPETE_PRODUCT
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
#endif

    if (mStagedModel != nullptr)
    {
      mStagedModel = nullptr;
    }
    mNAMPath = previousNAMPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

#if VOLUM_AMPETE_PRODUCT
namespace {
constexpr std::array<int, 6> kVoLumAmpKeyboardKnobParams = {
  kInputLevel,
  kNoiseGateThreshold,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
};
constexpr std::array<int, 3> kVoLumDelayKeyboardKnobParams = {
  kDelayTime,
  kDelayFeedback,
  kDelayMix,
};
constexpr std::array<int, 4> kVoLumReverbKeyboardKnobParams = {
  kReverbMix,
  kReverbDecay,
  kReverbTone,
  kReverbPreDelay,
};
constexpr std::array<int, 5> kVoLumOktaverbKeyboardKnobParams = {
  kReverbMix,
  kReverbDecay,
  kReverbTone,
  kReverbPreDelay,
  kReverbShimmer,
};
constexpr std::array<int, 6> kVoLumPreNam1KeyboardKnobParams = {
  kPreNam1Gain, kPreNam1Bass, kPreNam1Mid, kPreNam1MidFreq, kPreNam1Treble, kPreNam1Level,
};
constexpr std::array<int, 6> kVoLumPreNam2KeyboardKnobParams = {
  kPreNam2Gain, kPreNam2Bass, kPreNam2Mid, kPreNam2MidFreq, kPreNam2Treble, kPreNam2Level,
};
constexpr std::array<int, 6> kVoLumCompKeyboardKnobParams = {
  kPreCompAmount, kPreCompRatio, kPreCompAttack, kPreCompRelease, kPreCompMix, kPreCompLevel,
};

double GetVoLumKeyboardStepForParam(int paramIdx, bool fine)
{
  switch (paramIdx)
  {
    case kToneBass:
    case kToneMid:
    case kToneTreble:
    case kReverbTone:
    case kBoostTone:
    case kBoostDrive:
    case kPreNam1Bass:
    case kPreNam1Mid:
    case kPreNam1Treble:
    case kPreNam2Bass:
    case kPreNam2Mid:
    case kPreNam2Treble:
      return fine ? 0.1 : 0.5;
    case kDelayTime:
    case kReverbPreDelay:
    case kPreNam1MidFreq:
    case kPreNam2MidFreq:
    case kPreCompAttack:
    case kPreCompRelease:
      return fine ? 1.0 : 5.0;
    case kDelayFeedback:
    case kDelayMix:
    case kReverbMix:
    case kReverbDecay:
    case kReverbShimmer:
    case kPreCompMix:
      return fine ? 0.01 : 0.05;
    default:
      return fine ? 0.1 : 1.0;
  }
}

template <size_t N>
bool SelectAdjacentFromList(NeuralAmpModeler* plugin, const std::array<int, N>& params, int currentParamIdx, int direction)
{
  const auto it = std::find(params.begin(), params.end(), currentParamIdx);
  if (it == params.end())
    return false;

  const int idx = static_cast<int>(std::distance(params.begin(), it));
  const int count = static_cast<int>(params.size());
  const int nextIdx = (idx + direction + count) % count;
  plugin->_SelectVoLumKnob(params[nextIdx]);
  return true;
}

}

std::string NeuralAmpModeler::_GetVoLumKnobHintText(int paramIdx) const
{
  const IParam* pParam = GetParam(paramIdx);
  if (!pParam)
    return {};

  WDL_String line;
  line.SetFormatted(512, "%s  |  Up/Down adjust  |  Left/Right select  |  Enter exact  |  Delete reset  |  Shift fine",
                    pParam->GetName());
  return line.Get();
}

bool NeuralAmpModeler::_SelectAdjacentVoLumKnob(int currentParamIdx, int direction)
{
  switch (mVolumFocusedEffect)
  {
    case EVoLumEffectFocus::DELAY:
      return SelectAdjacentFromList(this, kVoLumDelayKeyboardKnobParams, currentParamIdx, direction);
    case EVoLumEffectFocus::REVERB:
      if (GetParam(kReverbMode)->Int() == 2)
        return SelectAdjacentFromList(this, kVoLumOktaverbKeyboardKnobParams, currentParamIdx, direction);
      return SelectAdjacentFromList(this, kVoLumReverbKeyboardKnobParams, currentParamIdx, direction);
    case EVoLumEffectFocus::AMP:
      return SelectAdjacentFromList(this, kVoLumAmpKeyboardKnobParams, currentParamIdx, direction);
    case EVoLumEffectFocus::COMP:
      return SelectAdjacentFromList(this, kVoLumCompKeyboardKnobParams, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM1:
      return SelectAdjacentFromList(this, kVoLumPreNam1KeyboardKnobParams, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM2:
      return SelectAdjacentFromList(this, kVoLumPreNam2KeyboardKnobParams, currentParamIdx, direction);
  }
  return false;
}

void NeuralAmpModeler::_SelectVoLumKnob(int paramIdx)
{
  mVolumSelectedKnobParamIdx = paramIdx;
  mVolumSelectedKnobHintText.clear();

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
            mVolumSelectedKnobParamIdx = kNoParameter;

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

void NeuralAmpModeler::_HideControlGroup(iplug::igraphics::IGraphics* pGfx, const char* group, bool hide)
{
  if (pGfx) {
    pGfx->ForAllControlsFunc([group, hide](iplug::igraphics::IControl* c) {
      if (c->GetGroup() && std::strcmp(c->GetGroup(), group) == 0) {
        c->Hide(hide);
      }
    });
  }
}

void NeuralAmpModeler::_UpdateVoLumLayout(iplug::igraphics::IGraphics* pGfx)
{
  if (!pGfx) pGfx = GetUI();
  if (pGfx)
  {
    auto disableGroup = [pGfx](const char* group, bool disable) {
      pGfx->ForAllControlsFunc([group, disable](iplug::igraphics::IControl* c) {
        if (c->GetGroup() && std::strcmp(c->GetGroup(), group) == 0)
          c->SetDisabled(disable);
      });
    };

    _HideControlGroup(pGfx, "AMP_KNOBS", true);
    _HideControlGroup(pGfx, "REVERB_KNOBS", true);
    _HideControlGroup(pGfx, "REVERB_SHIMMER", true);
    _HideControlGroup(pGfx, "REVERB_POWER", true);
    _HideControlGroup(pGfx, "DELAY_KNOBS", true);
    _HideControlGroup(pGfx, "DELAY_POWER", true);
    _HideControlGroup(pGfx, "COMP_KNOBS", true);
    _HideControlGroup(pGfx, "COMP_POWER", true);
    _HideControlGroup(pGfx, "PRE_NAM1_KNOBS", true);
    _HideControlGroup(pGfx, "PRE_NAM1_POWER", true);
    _HideControlGroup(pGfx, "PRE_NAM2_KNOBS", true);
    _HideControlGroup(pGfx, "PRE_NAM2_POWER", true);
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumPreCaptureMenu))
      menu->Hide(true);
    
    // Hide/show the correct group based on focused effect
    switch (mVolumFocusedEffect)
    {
      case EVoLumEffectFocus::AMP: _HideControlGroup(pGfx, "AMP_KNOBS", false); break;
      case EVoLumEffectFocus::REVERB:
        _HideControlGroup(pGfx, "REVERB_POWER", false);
        _HideControlGroup(pGfx, "REVERB_KNOBS", false);
        _HideControlGroup(pGfx, "REVERB_SHIMMER", GetParam(kReverbMode)->Int() != 2);
        disableGroup("REVERB_KNOBS", !GetParam(kReverbActive)->Bool());
        disableGroup("REVERB_SHIMMER", !GetParam(kReverbActive)->Bool());
        break;
      case EVoLumEffectFocus::DELAY:
        _HideControlGroup(pGfx, "DELAY_POWER", false);
        _HideControlGroup(pGfx, "DELAY_KNOBS", false);
        disableGroup("DELAY_KNOBS", !GetParam(kDelayActive)->Bool());
        break;
      case EVoLumEffectFocus::COMP:
        _HideControlGroup(pGfx, "COMP_POWER", false);
        _HideControlGroup(pGfx, "COMP_KNOBS", false);
        disableGroup("COMP_KNOBS", !GetParam(kPreCompActive)->Bool());
        break;
      case EVoLumEffectFocus::PRE_NAM1:
        _HideControlGroup(pGfx, "PRE_NAM1_POWER", false);
        _HideControlGroup(pGfx, "PRE_NAM1_KNOBS", false);
        disableGroup("PRE_NAM1_KNOBS", !GetParam(kPreNam1Active)->Bool());
        break;
      case EVoLumEffectFocus::PRE_NAM2:
        _HideControlGroup(pGfx, "PRE_NAM2_POWER", false);
        _HideControlGroup(pGfx, "PRE_NAM2_KNOBS", false);
        disableGroup("PRE_NAM2_KNOBS", !GetParam(kPreNam2Active)->Bool());
        break;
    }

    bool ampExpanded = (mVolumExpandedSection == EVoLumSection::AMP);
    _HideControlGroup(pGfx, "AMP_TOGGLES", !ampExpanded);
    
    if (auto* hero = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage)) hero->Hide(!ampExpanded);

    // Update Sub-row text
    if (auto* subTextCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
    {
      auto* subText = subTextCtrl->As<VoLumSubRowTextControl>();
      if (mVolumExpandedSection == EVoLumSection::AMP)
        subText->SetName(volum::kAmps[mVolumAmpIdx].displayName, true);
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
    }
    
    // Inform the Triptych of the current states
    if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych))
    {
      auto* trip = tripCtrl->As<VoLumTriptychControl>();
      const bool preActive = GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
      trip->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), mVolumAmpIdx, volum::kAmps[mVolumAmpIdx].displayName);
      trip->SetExpandedSection(mVolumExpandedSection);
      
      // Update Pedal Cards visibility, layout, and state based on whether POST is expanded
      bool preExpanded = (mVolumExpandedSection == EVoLumSection::PRE);
      bool postExpanded = (mVolumExpandedSection == EVoLumSection::POST);

      if (preExpanded) {
        const IRECT tripBounds = trip->GetRECT();
        const auto frames = volum::triptych_layout::ComputeFrames(volum::triptych_layout::FromRect(tripBounds), EVoLumSection::PRE);
        const auto cards = volum::triptych_layout::ComputePreCards(frames.pre);

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
      }
      
      if (postExpanded) {
        const IRECT tripBounds = trip->GetRECT();
        const auto frames = volum::triptych_layout::ComputeFrames(volum::triptych_layout::FromRect(tripBounds), EVoLumSection::POST);
        const auto cards = volum::triptych_layout::ComputePostCards(frames.post);

        if (auto* delayCard = pGfx->GetControlWithTag(kCtrlTagVoLumDelayCard))
          delayCard->SetTargetAndDrawRECTs(cards.delay.As<IRECT>());
        if (auto* reverbCard = pGfx->GetControlWithTag(kCtrlTagVoLumReverbCard))
          reverbCard->SetTargetAndDrawRECTs(cards.reverb.As<IRECT>());
        if (auto* linkCard = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector))
          linkCard->SetTargetAndDrawRECTs(cards.connector.As<IRECT>());
      }

      if (auto* delayCard = pGfx->GetControlWithTag(kCtrlTagVoLumDelayCard)) {
        delayCard->Hide(!postExpanded);
        if (postExpanded) {
          auto* card = delayCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::DELAY);
          card->SetActiveState(GetParam(kDelayActive)->Bool());
        }
      }
      if (auto* reverbCard = pGfx->GetControlWithTag(kCtrlTagVoLumReverbCard)) {
        reverbCard->Hide(!postExpanded);
        if (postExpanded) {
          auto* card = reverbCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::REVERB);
          card->SetActiveState(GetParam(kReverbActive)->Bool());
        }
      }
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector)) {
        chain->Hide(!postExpanded);
      }
      if (auto* compCard = pGfx->GetControlWithTag(kCtrlTagVoLumCompCard)) {
        compCard->Hide(!preExpanded);
        if (preExpanded) {
          auto* card = compCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::COMP);
          card->SetActiveState(GetParam(kPreCompActive)->Bool());
        }
      }
      if (auto* preCard = pGfx->GetControlWithTag(kCtrlTagVoLumPreNam1Card)) {
        preCard->Hide(!preExpanded);
        if (preExpanded) {
          auto* card = preCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::PRE_NAM1);
          card->SetActiveState(GetParam(kPreNam1Active)->Bool());
        }
      }
      if (auto* preCard = pGfx->GetControlWithTag(kCtrlTagVoLumPreNam2Card)) {
        preCard->Hide(!preExpanded);
        if (preExpanded) {
          auto* card = preCard->As<VoLumPedalCardControl>();
          card->SetFocused(mVolumFocusedEffect == EVoLumEffectFocus::PRE_NAM2);
          card->SetActiveState(GetParam(kPreNam2Active)->Bool());
        }
      }
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector1))
        chain->Hide(!preExpanded);
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumPreChainConnector2))
        chain->Hide(!preExpanded);
    }
  }
}

void NeuralAmpModeler::_ToggleVoLumTuner()
{
  if (auto* pGfx = GetUI())
  {
    auto* tuner = pGfx->GetControlWithTag(kCtrlTagVoLumTuner)->As<VoLumTunerControl>();
    if (tuner->IsHidden())
    {
      mTunerDSP.SetActive(true);
      tuner->Show();
    }
    else
    {
      mTunerDSP.SetActive(false);
      tuner->Hide(true);
    }
  }
}

void NeuralAmpModeler::_ToggleVoLumMetronomePanel()
{
  if (auto* pGfx = GetUI())
  {
    auto* panel = pGfx->GetControlWithTag(kCtrlTagVoLumMetronome)->As<VoLumMetronomeControl>();
    if (panel->IsHidden())
    {
      panel->Show(
        mMetronomeDSP.IsActive(),
        mMetronomeDSP.GetBPM(),
        mMetronomeDSP.GetVolume(),
        mMetronomeDSP.GetTimeSig());
    }
    else
    {
      panel->Hide(true);
    }
  }
}

void NeuralAmpModeler::_VolumRefreshChannels()
{
  if (mVolumRigsRoot.empty())
    return;

  if (mVolumSpeakerIdx < 0 || mVolumSpeakerIdx >= 4)
  {
    mVolumSpeakerIdx = std::clamp(mVolumSpeakerIdx, 0, 3);
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = mVolumSpeakerIdx;
    mVolumSettingsDirty = true;
  }

  auto channels = volum::DiscoverChannels(
    std::filesystem::path(mVolumRigsRoot),
    volum::kAmps[mVolumAmpIdx].folderName,
    volum::kSpeakerPrefixes[mVolumSpeakerIdx]);

  mVolumChannelFiles.clear();
  mVolumChannelLabels.clear();
  for (auto& ch : channels)
  {
    mVolumChannelFiles.push_back(std::move(ch.filename));
    mVolumChannelLabels.push_back(std::move(ch.label));
  }

  if (mVolumChannelIdx < 0 || mVolumChannelIdx >= (int)mVolumChannelFiles.size())
  {
    mVolumChannelIdx = 0;
    mVolumAmpSettings[mVolumAmpIdx].channelIdx = mVolumChannelIdx;
    mVolumSettingsDirty = true;
  }

  if (!mVolumChannelFiles.empty() && mVolumChannelIdx >= 0 && mVolumChannelIdx < (int)mVolumChannelFiles.size())
    mVolumLastLoadedFile = std::filesystem::path(mVolumChannelFiles[mVolumChannelIdx]).filename().string();
  else
    mVolumLastLoadedFile.clear();

  if (auto* pGfx = GetUI())
  {
    if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumChannelStep))
      stepper->As<VoLumChannelStepControl>()->SetChannels(mVolumChannelLabels, mVolumChannelIdx);
    if (auto* footer = pGfx->GetControlWithTag(kCtrlTagVoLumFooter))
      footer->As<VoLumFooterControl>()->SetText(mVolumLastLoadedFile.empty() ? "(no rig loaded)" : mVolumLastLoadedFile.c_str());
  }
}

void NeuralAmpModeler::_VolumRefreshPrePedalCaptures()
{
  mVolumPreCaptureFiles.clear();
  mVolumPreCaptureLabels.clear();

  auto addMockCaptures = [&]() {
    const char* labels[] = {
      "Klon - Gold Horse",
      "TS - Green Drive",
      "Fuzz - Velvet Doom",
      "Nuke - Petty Push",
      "Boost - Clean Lift",
    };
    for (const char* label : labels)
    {
      mVolumPreCaptureFiles.emplace_back();
      mVolumPreCaptureLabels.emplace_back(label);
    }
  };

  if (mVolumRigsRoot.empty())
  {
    addMockCaptures();
    return;
  }

  namespace fs = std::filesystem;
  const fs::path preDir = fs::path(mVolumRigsRoot) / "PrePedals";
  std::error_code ec;
  if (!fs::is_directory(preDir, ec))
  {
    addMockCaptures();
    return;
  }

  std::vector<std::pair<std::string, std::string>> captures;
  for (const auto& entry : fs::directory_iterator(preDir, ec))
  {
    if (!entry.is_regular_file(ec))
      continue;

    std::string name = entry.path().filename().string();
    if (name.size() <= 4 || name.compare(name.size() - 4, 4, ".nam") != 0)
      continue;

    std::string label = entry.path().stem().string();
    const auto dash = label.find('-');
    if (dash != std::string::npos && dash > 0)
      label = label.substr(0, dash);
    captures.push_back({name, label});
  }

  std::sort(captures.begin(), captures.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

  for (const auto& capture : captures)
  {
    mVolumPreCaptureFiles.push_back(capture.first);
    mVolumPreCaptureLabels.push_back(capture.second);
  }

  if (mVolumPreCaptureLabels.empty())
    addMockCaptures();
}

const char* NeuralAmpModeler::_VolumGetPreCaptureLabel(int captureIdx) const
{
  if (captureIdx <= 0 || captureIdx > static_cast<int>(mVolumPreCaptureLabels.size()))
    return "Click to change";
  return mVolumPreCaptureLabels[static_cast<size_t>(captureIdx - 1)].c_str();
}

int NeuralAmpModeler::_VolumGetPreCaptureCount() const
{
  return static_cast<int>(mVolumPreCaptureLabels.size()) + 1; // zero is EMPTY
}

std::string NeuralAmpModeler::_VolumGetPreCaptureFilename(int captureIdx) const
{
  if (captureIdx <= 0 || captureIdx > static_cast<int>(mVolumPreCaptureFiles.size()))
    return {};
  return mVolumPreCaptureFiles[static_cast<size_t>(captureIdx - 1)];
}

void NeuralAmpModeler::_VolumCyclePreNamCapture(int slot, int direction)
{
  if (slot < 0 || slot >= 2)
    return;

  const int paramIdx = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  const int total = static_cast<int>(mVolumPreCaptureFiles.size()) + 1; // zero is EMPTY
  if (total <= 0)
    return;

  int next = GetParam(paramIdx)->Int() + direction;
  while (next < 0)
    next += total;
  next %= total;

  GetParam(paramIdx)->Set(next);
  SendParameterValueFromDelegate(paramIdx, GetParam(paramIdx)->GetNormalized(), true);
  mVolumPreNeedsLoad[slot].store(true);
  mVolumSettingsDirty = true;
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumSetPreNamCapture(int slot, int captureIdx)
{
  if (slot < 0 || slot >= 2)
    return;

  const int paramIdx = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  const int maxIdx = std::max(0, _VolumGetPreCaptureCount() - 1);
  const int next = std::clamp(captureIdx, 0, maxIdx);
  if (GetParam(paramIdx)->Int() == next)
    return;

  GetParam(paramIdx)->Set(next);
  SendParameterValueFromDelegate(paramIdx, GetParam(paramIdx)->GetNormalized(), true);
  mVolumPreNeedsLoad[slot].store(true);
  mVolumSettingsDirty = true;
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumShowPreCaptureMenu(int slot, const IRECT& anchorRect)
{
  if (slot < 0 || slot >= 2)
    return;

  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  auto* rawCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumPreCaptureMenu);
  if (!rawCtrl)
    return;

  auto* menu = rawCtrl->As<VoLumPreCaptureMenuControl>();
  if (!rawCtrl->IsHidden() && menu && menu->GetSlot() == slot)
  {
    _VolumHidePreCaptureMenu();
    return;
  }

  const int captureCount = std::max(1, _VolumGetPreCaptureCount());
  std::vector<std::string> labels;
  labels.reserve(static_cast<size_t>(captureCount));
  for (int i = 0; i < captureCount; ++i)
    labels.emplace_back(_VolumGetPreCaptureLabel(i));

  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  const int selected = std::clamp(GetParam(captureParam)->Int(), 0, captureCount - 1);
  const float menuW = std::max(anchorRect.W() * 0.88f, 180.f);
  const float menuH = VoLumPreCaptureMenuControl::ItemHeight() * captureCount + 12.f;
  const IRECT menuRect(anchorRect.L, anchorRect.B + 6.f, anchorRect.L + menuW, anchorRect.B + 6.f + menuH);

  menu->SetTargetAndDrawRECTs(menuRect);
  menu->SetItems(slot, labels, selected);
  menu->Hide(false);
}

void NeuralAmpModeler::_VolumHidePreCaptureMenu()
{
  if (auto* pGfx = GetUI())
  {
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumPreCaptureMenu))
      menu->Hide(true);
  }
}

void NeuralAmpModeler::_VolumStartLoader()
{
  if (mVolumLoaderThread.joinable())
    return;

  mVolumLoaderStop.store(false);
  mVolumLoaderThread = std::thread([this]() { _VolumLoaderThreadMain(); });
}

void NeuralAmpModeler::_VolumStopLoader()
{
  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    mVolumLoaderStop.store(true);
    mVolumLoadRequests.clear();
  }
  mVolumLoaderCv.notify_one();

  if (mVolumLoaderThread.joinable())
    mVolumLoaderThread.join();
}

void NeuralAmpModeler::_VolumQueueMainModelLoad(std::string fileToLoad, int ampIdx, std::string rigsRoot)
{
  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Main;
  request.ampIdx = ampIdx;
  request.fileToLoad = std::move(fileToLoad);
  request.rigsRoot = std::move(rigsRoot);
  request.sampleRate = GetSampleRate();
  request.blockSize = GetBlockSize();

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    mVolumLoadRequests.push_back(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumQueuePreNamLoad(int slot, std::string fileToLoad)
{
  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Pre;
  request.slot = slot;
  request.fileToLoad = std::move(fileToLoad);
  request.sampleRate = GetSampleRate();
  request.blockSize = GetBlockSize();

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    mVolumLoadRequests.push_back(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumDrainLoaderResults()
{
  std::deque<VoLumLoadResult> results;
  {
    std::unique_lock<std::mutex> lock(mVolumLoaderMutex, std::try_to_lock);
    if (!lock.owns_lock())
      return;
    results.swap(mVolumLoadResults);
  }

  for (auto& result : results)
  {
    if (result.kind == VoLumLoadKind::Main)
    {
      mVolumIsLoading.store(false);
      if (mVolumNeedsLoad.load())
        continue;

      if (!result.error.empty())
        continue;

      if (result.model != nullptr)
      {
        mStagedModel = std::move(result.model);
        mNAMPath.Set(result.path.c_str());
      }
      continue;
    }

    const int slot = result.slot;
    if (slot < 0 || slot >= 2)
      continue;

    mVolumPreIsLoading[slot].store(false);
    if (mVolumPreNeedsLoad[slot].load())
      continue;

    if (!result.error.empty())
    {
      mShouldRemovePreModel[slot].store(true);
      continue;
    }

    if (result.model != nullptr)
      mStagedPreModel[slot] = std::move(result.model);
  }
}

void NeuralAmpModeler::_VolumLoaderThreadMain()
{
  namespace fs = std::filesystem;

  for (;;)
  {
    VoLumLoadRequest request;
    {
      std::unique_lock<std::mutex> lock(mVolumLoaderMutex);
      mVolumLoaderCv.wait(lock, [&]() { return mVolumLoaderStop.load() || !mVolumLoadRequests.empty(); });

      if (mVolumLoaderStop.load() && mVolumLoadRequests.empty())
        break;

      request = std::move(mVolumLoadRequests.front());
      mVolumLoadRequests.pop_front();
    }

    VoLumLoadResult result;
    result.kind = request.kind;
    result.slot = request.slot;
    result.path = request.fileToLoad;

    try
    {
      if (request.kind == VoLumLoadKind::Main)
      {
        const std::string filename = fs::path(request.fileToLoad).filename().string();

        if (mVolumCachedAmpIdx != request.ampIdx)
        {
          mVolumDspCache.clear();
          mVolumCachedAmpIdx = request.ampIdx;
        }

        auto cacheIt = mVolumDspCache.find(filename);
        std::unique_ptr<nam::DSP> model;
        if (cacheIt != mVolumDspCache.end())
        {
          // Core consumes dspData::weights during construction, so keep the cached copy immutable.
          nam::dspData cachedConfig = cacheIt->second;
          model = nam::get_dsp(cachedConfig);
        }
        else
        {
          nam::dspData conf;
          model = nam::get_dsp(fs::u8path(request.fileToLoad), conf);
          mVolumDspCache[filename] = std::move(conf);
        }

        result.model = std::make_unique<ResamplingNAM>(std::move(model), request.sampleRate);
        result.model->Reset(request.sampleRate, request.blockSize);

        if (!mVolumNeedsLoad.load())
        {
          const fs::path ampDir = fs::path(request.rigsRoot) / volum::kAmps[request.ampIdx].folderName;
          std::error_code ec;
          if (fs::is_directory(ampDir, ec))
          {
            for (const auto& entry : fs::directory_iterator(ampDir, ec))
            {
              if (mVolumNeedsLoad.load() || mVolumLoaderStop.load())
                break;
              if (!entry.is_regular_file(ec))
                continue;

              const std::string name = entry.path().filename().string();
              if (name.size() > 4 && name.compare(name.size() - 4, 4, ".nam") == 0
                  && mVolumDspCache.find(name) == mVolumDspCache.end())
              {
                try
                {
                  nam::dspData conf;
                  nam::get_dsp(entry.path(), conf);
                  mVolumDspCache[name] = std::move(conf);
                }
                catch (...)
                {
                }
              }
            }
          }
        }
      }
      else
      {
        nam::dspData conf;
        std::unique_ptr<nam::DSP> model = nam::get_dsp(fs::u8path(request.fileToLoad), conf);
        result.model = std::make_unique<ResamplingNAM>(std::move(model), request.sampleRate);
        result.model->Reset(request.sampleRate, request.blockSize);
      }
    }
    catch (const std::runtime_error& e)
    {
      result.error = e.what();
      if (request.kind == VoLumLoadKind::Main)
        std::cerr << "VoLum load failed: " << result.error << std::endl;
      else
        std::cerr << "VoLum PRE load failed: " << result.error << std::endl;
    }

    {
      std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
      mVolumLoadResults.push_back(std::move(result));
    }
  }
}

void NeuralAmpModeler::_VolumRequestPreNamLoad(int slot)
{
  if (slot < 0 || slot >= 2)
    return;

  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  const std::string filename = _VolumGetPreCaptureFilename(GetParam(captureParam)->Int());
  if (filename.empty() || mVolumRigsRoot.empty())
  {
    mShouldRemovePreModel[slot].store(true);
    mVolumPreIsLoading[slot].store(false);
    return;
  }

  mVolumPreIsLoading[slot].store(true);
  const std::string fileToLoad = (std::filesystem::path(mVolumRigsRoot) / "PrePedals" / filename).string();
  _VolumQueuePreNamLoad(slot, fileToLoad);
}

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
}

void NeuralAmpModeler::_VolumSaveEffectSettings()
{
  mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
  mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
  mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
  mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, 2));
  _VolumSaveReverbModeSnapshot(std::clamp(mVolumEffectSettings.reverbMode, 0, 2));
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
  _VolumRestoreDelayModeSnapshot(std::clamp(fx.delayMode, 0, 2));
  setParam(kReverbActive, fx.reverbActive ? 1.0 : 0.0);
  setParam(kReverbMode, fx.reverbMode);
  _VolumRestoreReverbModeSnapshot(std::clamp(fx.reverbMode, 0, 2));
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumSaveDelayModeSnapshot(int mode)
{
  auto& s = mVolumEffectSettings.delayModes[std::clamp(mode, 0, 2)];
  s.time = GetParam(kDelayTime)->Value();
  s.feedback = GetParam(kDelayFeedback)->Value();
  s.mix = GetParam(kDelayMix)->Value();
}

void NeuralAmpModeler::_VolumRestoreDelayModeSnapshot(int mode)
{
  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  const auto& s = mVolumEffectSettings.delayModes[std::clamp(mode, 0, 2)];
  setParam(kDelayTime, s.time);
  setParam(kDelayFeedback, s.feedback);
  setParam(kDelayMix, s.mix);
}

void NeuralAmpModeler::_VolumSaveReverbModeSnapshot(int mode)
{
  auto& s = mVolumEffectSettings.reverbModes[std::clamp(mode, 0, 2)];
  s.mix = GetParam(kReverbMix)->Value();
  s.decay = GetParam(kReverbDecay)->Value();
  s.tone = GetParam(kReverbTone)->Value();
  s.preDelay = GetParam(kReverbPreDelay)->Value();
  s.shimmer = GetParam(kReverbShimmer)->Value();
}

void NeuralAmpModeler::_VolumRestoreReverbModeSnapshot(int mode)
{
  auto setParam = [this](int idx, double val) {
    GetParam(idx)->Set(val);
    SendParameterValueFromDelegate(idx, GetParam(idx)->GetNormalized(), true);
  };
  const auto& s = mVolumEffectSettings.reverbModes[std::clamp(mode, 0, 2)];
  setParam(kReverbMix, s.mix);
  setParam(kReverbDecay, s.decay);
  setParam(kReverbTone, s.tone);
  setParam(kReverbPreDelay, s.preDelay);
  setParam(kReverbShimmer, s.shimmer);
}

void NeuralAmpModeler::_VolumRestoreFromSettings(int ampIdx)
{
  const auto& s = mVolumAmpSettings[ampIdx];
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
  mVolumPreNeedsLoad[0].store(true);
  mVolumPreNeedsLoad[1].store(true);

  // Update speaker row UI if available
  if (auto* pGfx = GetUI())
  {
    if (auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      spkCtrl->As<VoLumSpeakerRowControl>()->SetSelected(mVolumSpeakerIdx);
  }
}

void NeuralAmpModeler::_VolumSaveSettingsToFile()
{
  _VolumSaveEffectSettings();
  nlohmann::json j = volum::VolumUserSettingsToJson(mVolumAmpSettings.data(), volum::kAmpCount, mVolumAmpIdx, &mVolumEffectSettings);

  namespace fs = std::filesystem;
  fs::path settingsPath = volum::VolumUserSettingsFilePath();
  if (settingsPath.empty())
  {
    if (mVolumRigsRoot.empty())
      return;
    settingsPath = fs::path(mVolumRigsRoot) / "volum-settings.json";
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
}

void NeuralAmpModeler::_VolumLoadSettingsFromFile()
{
  namespace fs = std::filesystem;
  const fs::path userPath = volum::VolumUserSettingsFilePath();
  fs::path legacyPath;
  if (!mVolumRigsRoot.empty())
    legacyPath = fs::path(mVolumRigsRoot) / "volum-settings.json";

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
    if (settingsHealed)
      mVolumSettingsDirty = true;
    _VolumRestoreEffectSettings();
  }
  catch (...)
  {
    std::cerr << "Failed to read volum-settings.json" << std::endl;
  }
}
#endif

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mStagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mIRPath = irPath;
#if !VOLUM_AMPETE_PRODUCT
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
#endif
  }
  else
  {
    if (mStagedIR != nullptr)
    {
      mStagedIR = nullptr;
    }
    mIRPath = previousIRPath;
#if !VOLUM_AMPETE_PRODUCT
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
#endif
  }

  return wavState;
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::_InitToneStack()
{
  // If you want to customize the tone stack, then put it here!
  mToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
}
void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

#if defined(APP_API)
  constexpr bool kAppApi = true;
#else
  constexpr bool kAppApi = false;
#endif
  volum::process_io::MixExternalInputsToMono(inputs, nFrames, nChansIn, mInputGain, kAppApi, mInputArray[0].data());
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  // Assume _PrepareBuffers() was already called
  if (nChansIn != 1)
    throw std::runtime_error("Plugin is supposed to process in mono.");
  const size_t cin = 0;
#if defined(APP_API)
  constexpr bool kAppApi = true;
#else
  constexpr bool kAppApi = false;
#endif
  volum::process_io::ApplyOutputGainBroadcast(inputs[cin], outputs, nFrames, nChansOut, gain, kAppApi);
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  if (mModel == nullptr)
    return;

  if (auto* pGraphics = GetUI())
  {
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = mModel->GetEncapsulatedSampleRate();
    modelInfo.inputCalibrationLevel.known = mModel->HasInputLevel();
    modelInfo.inputCalibrationLevel.value = mModel->HasInputLevel() ? mModel->GetInputLevel() : 0.0;
    modelInfo.outputCalibrationLevel.known = mModel->HasOutputLevel();
    modelInfo.outputCalibrationLevel.value = mModel->HasOutputLevel() ? mModel->GetOutputLevel() : 0.0;

    static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->SetModelInfo(modelInfo);

    const bool disableInputCalibrationControls = !mModel->HasInputLevel();
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
    {
      auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode));
      c->SetNormalizedDisable(!mModel->HasLoudness());
      c->SetCalibratedDisable(!mModel->HasOutputLevel());
    }
  }
}

void NeuralAmpModeler::_UpdateLatency()
{
  int latency = 0;
  if (mModel)
  {
    latency += mModel->GetLatency();
  }
  // Other things that add latency here...

  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

// HACK
#include "Unserialization.cpp"
