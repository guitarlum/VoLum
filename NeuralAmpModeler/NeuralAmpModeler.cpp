#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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
#include "VoLumPrePedalCaptures.h"
#include "VoLumProcessIO.h"
#include "VoLumProcessingPlan.h"
#if VOLUM_AMPETE_PRODUCT
#include "VoLumChunkCodec.h"
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
// Teal counterpart for the support-amp lane: matches VoLumColors::TEAL so the IVKnob pointer
// dot reads at the same brightness as the teal SUPPORT panel border / value text.
const IColor kTeal(255, 91, 196, 196);
const IColor kTealBright(255, 156, 224, 224);
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
// Same vector-knob style, but with the X1/X3 colours retuned to teal so the rotating pointer
// dot on SUPPORT-lane knobs reads as "support" without changing any other knob geometry.
const IVStyle volumKnobStyleSupport =
  volumKnobStyle.WithColor(EVColor::kX1, kTeal).WithColor(EVColor::kX3, kTealBright);
// Pan knob style: transparent background and no frame so it sits flush on the hero art rather
// than punching a square dark patch through the lane fill.
const IVStyle volumPanKnobStyle = volumKnobStyle
  .WithColor(EVColor::kBG, COLOR_TRANSPARENT)
  .WithDrawShadows(false);
const IVStyle volumPanKnobStyleSupport = volumKnobStyleSupport
  .WithColor(EVColor::kBG, COLOR_TRANSPARENT)
  .WithDrawShadows(false);

// Hero-friendly knob: identical interaction to NAMKnobControl (keyboard nudge, value entry,
// selection ring) but skips the square knob bitmap. Lets the dial sit flush on the hero art.
class VoLumPanKnobControl : public NAMKnobControl
{
public:
  VoLumPanKnobControl(const IRECT& bounds, int paramIdx, const IVStyle& style)
  : NAMKnobControl(bounds, paramIdx, "", style, IBitmap())
  {
  }

  // Parent OnRescale would try to rescale a null bitmap — skip it for this transparent knob.
  void OnRescale() override {}

  void DrawWidget(IGraphics& g) override
  {
    const float widgetRadius = GetRadius() * 0.73f;
    const auto knobRect = mWidgetBounds.GetCentredInside(mWidgetBounds.W(), mWidgetBounds.W());
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));

    const IColor accent = GetColor(mMouseIsOver ? kX3 : kX1);

    // Subtle outer ring matches the lane accent without a heavy disc fill.
    g.DrawCircle(accent.WithOpacity(0.35f), cx, cy, widgetRadius - 0.5f);
    DrawIndicatorTrack(g, angle, cx + 0.5f, cy, widgetRadius);

    float data[2][2];
    RadialPoints(angle, cx, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3.f);
    g.PathFill(IPattern::CreateRadialGradient(data[1][0], data[1][1], 4.0f,
                                              {{accent, 0.f}, {accent, 0.8f}, {COLOR_TRANSPARENT, 1.0f}}),
               {}, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.4f), data[1][0], data[1][1], 3.f, &mBlend);

    if (IsSelectedForKeyboard())
      g.DrawCircle(accent.WithOpacity(0.8f), cx, cy, widgetRadius + 5.f, nullptr, 1.5f);
  }
};
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
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 10.0, 0.1);
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
  GetParam(kDelayMode)->InitEnum("DelayMode", 1, {"Tape", "Digital", "Ping Pong", "Reverse"});

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
  GetParam(kDualAmpActive)->InitBool("DualAmpActive", false);
  // Default to Custom (per-lane PAN) since the UI no longer surfaces STACK / L-R presets.
  // The enum + values are retained so existing chunks deserialise without migration.
  GetParam(kDualAmpRoute)->InitEnum("DualAmpRoute", 2, {"STACK", "L/R", "CUSTOM"});
  GetParam(kMainAmpPan)->InitDouble("MainAmpPan", 0.0, -1.0, 1.0, 0.01);
  GetParam(kSupportAmpIdx)->InitDouble("SupportAmp", -1.0, -1.0, static_cast<double>(volum::kAmpCount - 1), 1.0);
  GetParam(kSupportSpeakerIdx)->InitDouble("SupportSpeaker", 3.0, 0.0, 3.0, 1.0);
  GetParam(kSupportChannelIdx)->InitDouble("SupportChannel", 0.0, 0.0, 127.0, 1.0);
  GetParam(kSupportInputLevel)->InitGain("SupportInput", 0.0, -20.0, 20.0, 0.1);
  GetParam(kSupportNoiseGateThreshold)->InitGain("SupportThreshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kSupportToneBass)->InitDouble("SupportBass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kSupportToneMid)->InitDouble("SupportMiddle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kSupportToneTreble)->InitDouble("SupportTreble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kSupportOutputLevel)->InitGain("SupportOutput", volum::kDualAmpDefaultLaneLevelDb, -40.0, 10.0, 0.1);
  GetParam(kSupportNoiseGateActive)->InitBool("SupportNoiseGateActive", true);
  GetParam(kSupportEQActive)->InitBool("SupportToneStack", true);
  GetParam(kSupportAmpPan)->InitDouble("SupportAmpPan", 0.0, -1.0, 1.0, 0.01);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);
#if VOLUM_AMPETE_PRODUCT
  mSupportNoiseGateTrigger.AddListener(&mSupportNoiseGateGain);
#endif

#if VOLUM_AMPETE_PRODUCT
  {
    auto root = volum::FindRigsRootDirectory();
    if (!root.empty())
      mVolumRigsRoot = root.string();
    _VolumLoadSettingsFromFile();
    _VolumRestoreFromSettings(mVolumAmpIdx);
    _VolumRefreshPrePedalCaptures();
    _VolumRefreshChannels();
    _VolumRefreshSupportChannels();
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
        _VolumHideSupportAmpMenu();
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
            trip->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), ampIdx,
                           volum::kAmps[ampIdx].displayName,
                           _VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), "NAM 1"),
                           _VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), "NAM 2"));
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
    auto* hero = new VoLumHeroImageControl(heroArea,
      [this](bool supportFocused) {
        mVolumDualAmpFocusedSupport = supportFocused;
        mVolumFocusedEffect = EVoLumEffectFocus::AMP;
        _UpdateVoLumLayout();
      },
      [this](const IRECT& anchor) {
        // Only allow the picker when Dual Amp is on; mono mode shouldn't surface a Support amp menu.
        if (GetParam(kDualAmpActive)->Bool())
          _VolumShowSupportAmpMenu(anchor);
      },
      // DUAL chip — toggle the global Dual Amp parameter.
      [this]() {
        const bool current = GetParam(kDualAmpActive)->Bool();
        GetParam(kDualAmpActive)->Set(current ? 0.0 : 1.0);
        SendParameterValueFromDelegate(kDualAmpActive, GetParam(kDualAmpActive)->GetNormalized(), true);
        OnParamChange(kDualAmpActive);
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
      new VoLumPanKnobControl(hero->GetMainPanKnobSlot(), kMainAmpPan, volumPanKnobStyle),
      -1, "MAIN_PAN_KNOB");
    pGraphics->AttachControl(
      new VoLumPanKnobControl(hero->GetSupportPanKnobSlot(), kSupportAmpPan, volumPanKnobStyleSupport),
      -1, "SUPPORT_PAN_KNOB");
    pGraphics->AttachControl(
      new VoLumSupportPolarityControl(hero->GetSupportPolarityToggleSlot(),
        [this]() { return mSupportPolarityInvert.load(); },
        [this]() {
          const bool next = !mSupportPolarityInvert.load();
          mSupportPolarityInvert.store(next);
          mVolumAmpSettings[mVolumAmpIdx].supportPolarityInvert = next;
          mVolumSettingsDirty = true;
          if (auto* pGfx = GetUI())
            pGfx->SetAllControlsDirty();
        }),
      -1, "SUPPORT_POLARITY_TOGGLE");

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

    // SUPPORT AMP KNOBS — identical layout to AMP_KNOBS, just bound to support params.
    // Visibility is toggled on lane focus so the user sees one row at a time in the same slots.
    {
      float cx = knobX(0);
      pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(cx, knobRowTop, cx + colW, knobRowTop + 20.f), "CHANNEL", true), -1, "SUPPORT_AMP_KNOBS");
      float stepH = 28.f;
      float stepTop = knobT + (knobDiam - stepH) / 2.f;
      auto* supChannelStep = new VoLumChannelStepControl(IRECT(cx, stepTop, cx + colW, stepTop + stepH), [this](int newIdx) {
          GetParam(kSupportChannelIdx)->Set(newIdx);
          SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
          mVolumSupportNeedsLoad.store(true);
          mVolumSettingsDirty = true;
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
    pGraphics->AttachControl(new VoLumModePickerControl(delayPickerRect, kDelayMode, {"TAPE", "DIGITAL", "PING PONG", "REVERSE"}), -1, "DELAY_KNOBS");

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
    // Second (right-channel) OUT meter, shown when dual amp / stereo mode is active.
    // 5 px gap so the two bars read as L / R rather than one fat meter; visibility toggled
    // in _UpdateVoLumLayout.
    const float outMeter2L = outMeterR + 5.f;
    const float outMeter2R = outMeter2L + meterW;
    const float outLabelL = outMeter2R + gapLabelToMeter;
    const float outLabelR = outLabelL + meterLabelStripW;

    pGraphics->AttachControl(new NAMMeterControl(IRECT(outMeterL, meterTop, outMeterR, meterTop + meterH), meterBackgroundBitmap, volumStyle), kCtrlTagOutputMeter);
    pGraphics->AttachControl(new NAMMeterControl(IRECT(outMeter2L, meterTop, outMeter2R, meterTop + meterH), meterBackgroundBitmap, volumStyle), kCtrlTagOutputMeterR);
    pGraphics->AttachControl(new VoLumVerticalLabelControl(IRECT(outLabelL, meterTop, outLabelR, meterTop + meterH), "OUT"));

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
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(ngX, toggleY, ngX + switchW, toggleY + switchH), kNoiseGateActive, "", volumToggleStyle, switchHandleBitmap), kCtrlTagVoLumNoiseGate, "MAIN_LANE_TOGGLES");
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(ngX + switchW + 4.f, toggleY, ngX + switchW + 90.f, toggleY + switchH), "NOISE GATE"), -1, "MAIN_LANE_TOGGLES");
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(eqX, toggleY, eqX + switchW, toggleY + switchH), kEQActive, "", volumToggleStyle, switchHandleBitmap), kCtrlTagVoLumEQ, "MAIN_LANE_TOGGLES");
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(eqX + switchW + 4.f, toggleY, eqX + switchW + 46.f, toggleY + switchH), "EQ"), -1, "MAIN_LANE_TOGGLES");

    // SUPPORT lane toggles (identical positions, bound to SUPPORT params).
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(ngX, toggleY, ngX + switchW, toggleY + switchH), kSupportNoiseGateActive, "", volumToggleStyle, switchHandleBitmap), -1, "SUPPORT_LANE_TOGGLES");
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(ngX + switchW + 4.f, toggleY, ngX + switchW + 90.f, toggleY + switchH), "NOISE GATE"), -1, "SUPPORT_LANE_TOGGLES");
    pGraphics->AttachControl(
      new NAMSwitchControl(IRECT(eqX, toggleY, eqX + switchW, toggleY + switchH), kSupportEQActive, "", volumToggleStyle, switchHandleBitmap), -1, "SUPPORT_LANE_TOGGLES");
    pGraphics->AttachControl(new VoLumKnobLabelControl(IRECT(eqX + switchW + 4.f, toggleY, eqX + switchW + 46.f, toggleY + switchH), "EQ"), -1, "SUPPORT_LANE_TOGGLES");

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
    pGraphics->AttachControl(new VoLumSupportAmpMenuControl(IRECT(mainL, knobRowTop, mainL + 220.f, knobRowTop + 160.f)),
                             kCtrlTagVoLumSupportAmpMenu)->Hide(true);

    // Lane belonging on the SUPPORT amp-row knobs is conveyed solely by the teal knob pointer
    // dot. Labels and value text stay bright/neutral so the row reads cleanly. Set once at attach
    // — SUPPORT_AMP_KNOBS is only ever visible while support is focused, so no retoggling.
    pGraphics->ForAllControlsFunc([](iplug::igraphics::IControl* c) {
      const char* g = c->GetGroup();
      if (!g || std::strcmp(g, "SUPPORT_AMP_KNOBS") != 0) return;
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
             tripCtrl->As<VoLumTriptychControl>()->SetState(
               preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), newIdx,
               volum::kAmps[newIdx].displayName,
               _VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), "NAM 1"),
               _VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), "NAM 2"));
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
          if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
          {
            const int delta = key.VK == kVK_LEFT ? -1 : 1;
            const int next = std::clamp(GetParam(kSupportChannelIdx)->Int() + delta, 0, 127);
            GetParam(kSupportChannelIdx)->Set(next);
            SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
            mVolumSupportNeedsLoad.store(true);
            return true;
          }
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
  const bool haveMainModel = (mModel != nullptr);
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();
#if VOLUM_AMPETE_PRODUCT
  const bool dualAmpActive = GetParam(kDualAmpActive)->Bool();
  const bool supportAmpSelected = GetParam(kSupportAmpIdx)->Int() >= 0;
  const bool haveSupportModel = supportAmpSelected && (mSupportModel != nullptr);
  const bool supportToneStackActive = GetParam(kSupportEQActive)->Bool();
  const bool preNamActive[2] = {GetParam(kPreNam1Active)->Bool(), GetParam(kPreNam2Active)->Bool()};
  const bool havePreNam[2] = {mPreModel[0] != nullptr, mPreModel[1] != nullptr};
  const auto processingPlan =
    volum::MakeProcessingPlan(haveMainModel, noiseGateActive, toneStackActive, GetParam(kIRToggle)->Value(),
                              mIR != nullptr, GetParam(kPreCompActive)->Bool(), preNamActive, havePreNam,
                              GetParam(kDelayActive)->Bool(), GetParam(kReverbActive)->Bool(), mTunerDSP.IsActive(),
                              dualAmpActive, haveSupportModel, supportToneStackActive);
  if (processingPlan.runPreComp)
  {
    mPreCompressor.SetParams(GetParam(kPreCompAmount)->Value(), GetParam(kPreCompRatio)->Value(),
                             GetParam(kPreCompAttack)->Value(), GetParam(kPreCompRelease)->Value(),
                             GetParam(kPreCompMix)->Value(), GetParam(kPreCompLevel)->Value(), sampleRate);
    preAmpPointers = mPreCompressor.Process(preAmpPointers, numChannelsInternal, numFrames);
  }

  auto processPreSlot = [&](int slot, int activeParam, int gainParam, int bassParam, int midParam, int midFreqParam,
                            int trebleParam, int levelParam) {
    (void)activeParam;
    if (!processingPlan.runPreNam[slot])
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
#else
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};
  const auto processingPlan =
    volum::MakeProcessingPlan(haveMainModel, noiseGateActive, toneStackActive, GetParam(kIRToggle)->Value(),
                              mIR != nullptr, false, preNamActive, havePreNam, GetParam(kDelayActive)->Bool(),
                              GetParam(kReverbActive)->Bool(), false);
#endif

#if VOLUM_AMPETE_PRODUCT
  if (processingPlan.runDualAmp)
  {
    mDualMainLaneBuffer.resize(numFrames);
    std::memcpy(mDualMainLaneBuffer.data(), preAmpPointers[0], numFrames * sizeof(sample));
  }
#endif

  // Noise gate trigger
  sample** triggerOutput = preAmpPointers;
  if (processingPlan.runNoiseGate)
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

  if (processingPlan.runMainModel)
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
  if (processingPlan.runMainModel)
    mPostEffectsClearedForMissingModel = false;

  sample** hpfPointers = mOutputPointers;
  if (processingPlan.runMainModel)
  {
    // Apply the noise gate after the NAM
    sample** gateGainOutput =
      processingPlan.runNoiseGate ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

    sample** toneStackOutPointers = (processingPlan.runToneStack && mToneStack != nullptr)
                                      ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                      : gateGainOutput;

    sample** irPointers = toneStackOutPointers;
    if (processingPlan.runIR)
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

#if VOLUM_AMPETE_PRODUCT
  sample* supportLane = nullptr;
  if (processingPlan.runDualAmp)
  {
    mDualSupportLaneBuffer.resize(numFrames);

    const double supportInputGain = DBToAmp(GetParam(kSupportInputLevel)->Value());
    for (size_t i = 0; i < numFrames; ++i)
      mDualMainLaneBuffer[i] = static_cast<sample>(static_cast<double>(mDualMainLaneBuffer[i]) * supportInputGain);

    sample* supportInputPtr = mDualMainLaneBuffer.data();
    sample* supportOutputPtr = mDualSupportLaneBuffer.data();
    sample* supportInputPointers[1] = {supportInputPtr};
    sample** supportTriggerOutput = supportInputPointers;

    if (GetParam(kSupportNoiseGateActive)->Bool())
    {
      const double time = 0.01;
      const double threshold = GetParam(kSupportNoiseGateThreshold)->Value();
      const double ratio = 0.1;
      const double openTime = 0.005;
      const double holdTime = 0.01;
      const double closeTime = 0.05;
      const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
      mSupportNoiseGateTrigger.SetParams(triggerParams);
      mSupportNoiseGateTrigger.SetSampleRate(sampleRate);
      supportTriggerOutput = mSupportNoiseGateTrigger.Process(supportInputPointers, numChannelsInternal, numFrames);
    }

    mSupportModel->process(supportTriggerOutput[0], supportOutputPtr, nFrames);
    sample* supportModelPointers[1] = {supportOutputPtr};
    sample** supportPostPointers = supportModelPointers;
    if (GetParam(kSupportNoiseGateActive)->Bool())
      supportPostPointers = mSupportNoiseGateGain.Process(supportPostPointers, numChannelsInternal, numFrames);

    if (processingPlan.runSupportToneStack && mSupportToneStack != nullptr)
      supportPostPointers = mSupportToneStack->Process(supportPostPointers, numChannelsInternal, nFrames);

    const double highPassCutoffFreq = kDCBlockerFrequency;
    const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
    mSupportHighPass.SetParams(highPassParams);
    supportPostPointers = mSupportHighPass.Process(supportPostPointers, numChannelsInternal, numFrames);
    supportLane = supportPostPointers[0];
  }
#endif

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
#if VOLUM_AMPETE_PRODUCT
  if (processingPlan.runDualAmp && supportLane != nullptr)
  {
#if defined(APP_API)
    constexpr bool kAppApi = true;
#else
    constexpr bool kAppApi = false;
#endif
    // The route picker UI was removed in favour of per-lane PAN knobs; always use CUSTOM
    // routing regardless of `kDualAmpRoute`. Older user settings/presets shipped with
    // route=STACK (0) as the historical default, which would force constant-power-center for
    // both lanes and ignore PAN — hard-coding CUSTOM here makes the panning behave as the
    // visible knobs imply for every existing state file.
    const auto panGains = volum::MakeDualAmpPanGains(volum::DualAmpRoute::Custom,
                                                     GetParam(kMainAmpPan)->Value(),
                                                     GetParam(kSupportAmpPan)->Value());
    int mainLatency = 0;
    int supportLatency = 0;
    if (mModel)
      mainLatency = mModel->GetLatency();
    if (mSupportModel)
      supportLatency = mSupportModel->GetLatency();
    const auto latencyComp = volum::MakeDualAmpLatencyCompensation(mainLatency, supportLatency);
    mDualMainAlignedBuffer.resize(numFrames);
    mDualSupportAlignedBuffer.resize(numFrames);
    const sample* mainLane =
      mDualMainLatencyDelay.Process(hpfPointers[0], mDualMainAlignedBuffer.data(), numFrames,
                                    latencyComp.mainDelaySamples);
    const sample* compensatedSupportLane =
      mDualSupportLatencyDelay.Process(supportLane, mDualSupportAlignedBuffer.data(), numFrames,
                                       latencyComp.supportDelaySamples);
    const double supportOutputGain = mSupportPolarityInvert.load() ? -mSupportOutputGain : mSupportOutputGain;
    volum::MergeDualAmpToStereo(mainLane, compensatedSupportLane, outputs, numFrames, numChannelsExternalOut,
                                mOutputGain, supportOutputGain, panGains, kAppApi);
  }
  else
#endif
  {
#if VOLUM_AMPETE_PRODUCT
    mDualMainLatencyDelay.Reset();
    mDualSupportLatencyDelay.Reset();
#endif
    _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  }

  // Apply POST effects (Delay -> Reverb) in stereo
  iplug::sample** postPointers = outputs;

  if (processingPlan.runDelay)
  {
    mDelay.SetParams(GetParam(kDelayTime)->Value(), GetParam(kDelayFeedback)->Value(),
                     GetParam(kDelayMix)->Value(), GetParam(kDelayMode)->Int(), sampleRate);
    postPointers = mDelay.Process(postPointers, numChannelsExternalOut, numFrames);
  }

  if (processingPlan.runReverb)
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
  if (processingPlan.silenceForTuner)
  {
    for (size_t c = 0; c < numChannelsExternalOut; c++)
      std::memset(outputs[c], 0, numFrames * sizeof(iplug::sample));
  }

  if (processingPlan.runDualAmp)
  {
    double peak = 0.0;
    for (size_t c = 0; c < numChannelsExternalOut; ++c)
      for (size_t s = 0; s < numFrames; ++s)
        peak = std::max(peak, std::abs(static_cast<double>(outputs[c][s])));
    mVolumDualAmpOutputHot.store(peak >= 0.95);
  }
  else
  {
    mVolumDualAmpOutputHot.store(false);
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
  mOutputSenderR.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mToneStack->Reset(sampleRate, maxBlockSize);
#if VOLUM_AMPETE_PRODUCT
  if (mSupportToneStack)
    mSupportToneStack->Reset(sampleRate, maxBlockSize);
  mDualMainLatencyDelay.Reset();
  mDualSupportLatencyDelay.Reset();
#endif
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
  mOutputSenderR.TransmitData(*this);

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
      if (fileToLoad == mNAMPath.Get())
      {
        mVolumIsLoading.store(false);
      }
      else
      {
        mVolumIsLoading.store(true);
        _VolumQueueMainModelLoad(fileToLoad, ampIdx, rigsRoot);
      }
    }
    else
    {
      mVolumIsLoading.store(false);
    }
  }

  if (mVolumSupportNeedsLoad.load() && !mVolumSupportIsLoading.load())
  {
    mVolumSupportNeedsLoad.store(false);
    _VolumRequestSupportModelLoad();
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

  if (auto* pGfx = GetUI())
  {
    if (auto* footer = pGfx->GetControlWithTag(kCtrlTagVoLumFooter))
    {
      const bool dualActive = GetParam(kDualAmpActive)->Bool();
      if (dualActive && mVolumDualAmpOutputHot.load())
      {
        footer->As<VoLumFooterControl>()->SetText("Dual Amp output hot - lower lane or output levels");
      }
      else if (dualActive && !mVolumLastLoadedFile.empty() && !mVolumLastLoadedSupportFile.empty())
      {
        // Show both NAM filenames side-by-side so the user can see exactly which two captures
        // are stacked. Bullet separator keeps it scannable in the narrow footer strip.
        std::string both = mVolumLastLoadedFile + "  |  " + mVolumLastLoadedSupportFile;
        footer->As<VoLumFooterControl>()->SetText(both.c_str());
      }
      else if (!mVolumLastLoadedFile.empty())
      {
        footer->As<VoLumFooterControl>()->SetText(mVolumLastLoadedFile.c_str());
      }
      else
      {
        footer->As<VoLumFooterControl>()->SetText("(no rig loaded)");
      }
    }
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
  volum::PutCurrentVoLumChunkState(
    chunk, {mVolumAmpIdx, mVolumSpeakerIdx, mVolumChannelIdx}, mVolumAmpSettings.data(), volum::kAmpCount);
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
  _UpdateLatency();
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
    case kOutputLevel: _SetOutputGain(); break;
    case kOutputMode:
      _SetOutputGain();
#if VOLUM_AMPETE_PRODUCT
      _SetSupportOutputGain();
#endif
      break;
#if VOLUM_AMPETE_PRODUCT
    case kSupportOutputLevel: _SetSupportOutputGain(); break;
#endif
    // Tone stack:
    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
#if VOLUM_AMPETE_PRODUCT
    case kSupportToneBass:
      if (mSupportToneStack)
        mSupportToneStack->SetParam("bass", GetParam(paramIdx)->Value());
      break;
    case kSupportToneMid:
      if (mSupportToneStack)
        mSupportToneStack->SetParam("middle", GetParam(paramIdx)->Value());
      break;
    case kSupportToneTreble:
      if (mSupportToneStack)
        mSupportToneStack->SetParam("treble", GetParam(paramIdx)->Value());
      break;
    case kDualAmpActive:
      if (mVolumInitComplete)
      {
        const bool nowOn = GetParam(kDualAmpActive)->Bool();
        // First-time-on heuristic: support not yet picked AND both pans still at 0
        // (i.e. user hasn't customised this dual rig). Apply hard L/R + mirror MAIN cab.
        // If the user already configured a Dual setup, those values are restored from per-amp memory.
        if (nowOn && GetParam(kSupportAmpIdx)->Int() < 0
            && std::abs(GetParam(kMainAmpPan)->Value()) < 1e-3
            && std::abs(GetParam(kSupportAmpPan)->Value()) < 1e-3)
        {
          GetParam(kMainAmpPan)->Set(-1.0);
          SendParameterValueFromDelegate(kMainAmpPan, GetParam(kMainAmpPan)->GetNormalized(), true);
          GetParam(kSupportAmpPan)->Set(1.0);
          SendParameterValueFromDelegate(kSupportAmpPan, GetParam(kSupportAmpPan)->GetNormalized(), true);

          // Mirror MAIN's currently selected cab onto SUPPORT.
          const int mainSpk = std::clamp(mVolumSpeakerIdx, 0, 3);
          GetParam(kSupportSpeakerIdx)->Set(mainSpk);
          SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
          mVolumSettingsDirty = true;
        }

        mVolumSupportNeedsLoad.store(true);
        // Force focus back to MAIN when dual amp is disabled.
        if (!nowOn)
          mVolumDualAmpFocusedSupport = false;
        _UpdateVoLumLayout();
      }
      break;
    case kMainAmpPan:
    case kSupportAmpPan:
      if (mVolumInitComplete)
        mVolumSettingsDirty = true;
      break;
    case kSupportAmpIdx:
    case kSupportSpeakerIdx:
      if (mVolumInitComplete)
      {
        mVolumSupportNeedsLoad.store(true);
        _VolumRefreshSupportChannels();
        _UpdateVoLumLayout();
      }
      break;
    case kSupportChannelIdx:
      if (mVolumInitComplete)
      {
        mVolumSupportNeedsLoad.store(true);
        if (auto* pGfx = GetUI())
          if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumSupportChannelStep))
            stepper->As<VoLumChannelStepControl>()->SetChannels(
              mVolumSupportChannelLabels, GetParam(kSupportChannelIdx)->Int());
      }
      break;
    case kDelayActive:
    case kDelayTime:
    case kDelayFeedback:
    case kDelayMix:
      if (mVolumInitComplete)
      {
        mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
        mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
        _VolumSaveDelayModeSnapshot(std::clamp(GetParam(kDelayMode)->Int(), 0, volum::kVoLumDelayModeCount - 1));
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
      {
        mVolumPreNeedsLoad[0].store(true);
        _UpdateLatency();
      }
      break;
    case kPreNam2Capture:
    case kPreNam2Active:
      if (mVolumInitComplete)
      {
        mVolumPreNeedsLoad[1].store(true);
        _UpdateLatency();
      }
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
      case kDualAmpActive:
      case kSupportAmpIdx:
      case kSupportSpeakerIdx:
      case kSupportChannelIdx:
#if VOLUM_AMPETE_PRODUCT
        _UpdateVoLumLayout(pGraphics);
#endif
        break;
      case kMainAmpPan:
      case kSupportAmpPan:
        break;
      case kSupportNoiseGateActive:
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportNoiseGateThreshold)) c->SetDisabled(!active);
        break;
      case kSupportEQActive:
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportToneBass)) c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportToneMid)) c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportToneTreble)) c->SetDisabled(!active);
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
        const int oldMode = std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1);
        _VolumSaveDelayModeSnapshot(oldMode);
        const int newMode = std::clamp(GetParam(kDelayMode)->Int(), 0, volum::kVoLumDelayModeCount - 1);
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
#if VOLUM_AMPETE_PRODUCT
  if (mShouldRemoveSupportModel)
  {
    mSupportModel = nullptr;
    mShouldRemoveSupportModel = false;
    _UpdateLatency();
    _SetSupportOutputGain();
  }
#endif
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
      _UpdateLatency();
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
#if VOLUM_AMPETE_PRODUCT
  if (mStagedSupportModel != nullptr)
  {
    mSupportModel = std::move(mStagedSupportModel);
    mStagedSupportModel = nullptr;
    _UpdateLatency();
    _SetSupportOutputGain();
  }
#endif
  for (int i = 0; i < 2; ++i)
  {
    if (mStagedPreModel[i] != nullptr)
    {
      mPreModel[i] = std::move(mStagedPreModel[i]);
      mStagedPreModel[i] = nullptr;
      _UpdateLatency();
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

#if VOLUM_AMPETE_PRODUCT
  if (mStagedSupportModel != nullptr)
  {
    mStagedSupportModel->Reset(sampleRate, maxBlockSize);
  }
  else if (mSupportModel != nullptr)
  {
    mSupportModel->Reset(sampleRate, maxBlockSize);
  }
#endif

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

void NeuralAmpModeler::_SetSupportOutputGain()
{
  double gainDB = GetParam(kSupportOutputLevel)->Value();
#if VOLUM_AMPETE_PRODUCT
  if (mSupportModel != nullptr)
  {
    // Use the same OutputMode as main so the two lanes share the loudness-target / calibration
    // model. Without this, main lane gets the +N dB normalization boost while support stays at
    // raw knob value, making support consistently quieter at identical knob settings.
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mSupportModel->HasLoudness())
        {
          const double loudness = mSupportModel->GetLoudness();
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mSupportModel->HasOutputLevel())
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mSupportModel->GetOutputLevel();
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 0: // Raw
      default: break;
    }
  }
#endif
  mSupportOutputGain = DBToAmp(gainDB);
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
constexpr std::array<int, 10> kVoLumSupportAmpKeyboardKnobParams = {
  kSupportAmpIdx,
  kSupportSpeakerIdx,
  kSupportChannelIdx,
  kSupportInputLevel,
  kSupportNoiseGateThreshold,
  kSupportToneBass,
  kSupportToneMid,
  kSupportToneTreble,
  kSupportOutputLevel,
  kSupportAmpPan,
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
    case kSupportToneBass:
    case kSupportToneMid:
    case kSupportToneTreble:
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
      if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
        return SelectAdjacentFromList(this, kVoLumSupportAmpKeyboardKnobParams, currentParamIdx, direction);
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
    _HideControlGroup(pGfx, "SUPPORT_AMP_KNOBS", true);
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
    _HideControlGroup(pGfx, "MAIN_LANE_TOGGLES", true);
    _HideControlGroup(pGfx, "SUPPORT_LANE_TOGGLES", true);
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumPreCaptureMenu))
      menu->Hide(true);
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumSupportAmpMenu))
      menu->Hide(true);
    
    // Hide/show the correct group based on focused effect
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
    const bool dualActiveNow = GetParam(kDualAmpActive)->Bool();
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
      heroImage->SetDualAmpState(GetParam(kDualAmpActive)->Bool(), mVolumDualAmpFocusedSupport,
                                 GetParam(kSupportAmpIdx)->Int());
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
          const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
          subText->SetName(supportAmpIdx >= 0 && supportAmpIdx < volum::kAmpCount
                            ? volum::kAmps[supportAmpIdx].displayName
                            : "Choose support amp",
                           true);
        }
        else
          subText->SetName(volum::kAmps[mVolumAmpIdx].displayName, true);
      }
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
      trip->SetState(preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), mVolumAmpIdx,
                     volum::kAmps[mVolumAmpIdx].displayName,
                     _VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), "NAM 1"),
                     _VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), "NAM 2"));
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
  mVolumPreCaptureShortLabels.clear();
  mVolumPreCaptureGroups.clear();

  auto addMockCaptures = [&]() {
    struct MockCapture
    {
      const char* label;
      const char* shortLabel;
      volum::PrePedalCaptureGroup group;
    };
    const MockCapture captures[] = {
      {"Klon - Gold Horse", "Klon", volum::PrePedalCaptureGroup::Klon},
      {"TS - Green Drive", "TS", volum::PrePedalCaptureGroup::TsBoost},
      {"Fuzz - Velvet Doom", "Fuzz", volum::PrePedalCaptureGroup::Fuzz},
      {"Nuke - Petty Push", "Nuke", volum::PrePedalCaptureGroup::Fuzz},
      {"Boost - Clean Lift", "Boost", volum::PrePedalCaptureGroup::TsBoost},
    };
    for (const auto& capture : captures)
    {
      mVolumPreCaptureFiles.emplace_back();
      mVolumPreCaptureLabels.emplace_back(capture.label);
      mVolumPreCaptureShortLabels.emplace_back(capture.shortLabel);
      mVolumPreCaptureGroups.emplace_back(capture.group);
    }
  };

  if (mVolumRigsRoot.empty())
  {
    addMockCaptures();
    return;
  }

  const auto captures = volum::DiscoverPrePedalCaptures(std::filesystem::path(mVolumRigsRoot));
  for (const auto& capture : captures)
  {
    mVolumPreCaptureFiles.push_back(capture.filename);
    mVolumPreCaptureLabels.push_back(capture.label);
    mVolumPreCaptureShortLabels.push_back(capture.shortLabel);
    mVolumPreCaptureGroups.push_back(capture.group);
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

const char* NeuralAmpModeler::_VolumGetPreCaptureShortLabel(int captureIdx, const char* fallback) const
{
  if (captureIdx <= 0 || captureIdx > static_cast<int>(mVolumPreCaptureShortLabels.size()))
    return fallback;
  return mVolumPreCaptureShortLabels[static_cast<size_t>(captureIdx - 1)].c_str();
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
  std::vector<VoLumPreCaptureMenuItem> items;
  items.reserve(static_cast<size_t>(captureCount + 4));
  items.push_back({_VolumGetPreCaptureLabel(0), 0, false, volum::PrePedalCaptureGroup::None});
  volum::PrePedalCaptureGroup lastGroup = volum::PrePedalCaptureGroup::None;
  for (int i = 1; i < captureCount; ++i)
  {
    const auto group = (i - 1 < static_cast<int>(mVolumPreCaptureGroups.size()))
                         ? mVolumPreCaptureGroups[static_cast<size_t>(i - 1)]
                         : volum::PrePedalCaptureGroup::None;
    if (group != lastGroup)
    {
      items.push_back({volum::PrePedalCaptureGroupLabel(group), 0, true, group});
      lastGroup = group;
    }
    items.push_back({_VolumGetPreCaptureLabel(i), i, false, group});
  }

  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  const int selected = std::clamp(GetParam(captureParam)->Int(), 0, captureCount - 1);
  const float menuW = std::max(anchorRect.W() * 0.88f, 180.f);
  const float menuH = VoLumPreCaptureMenuControl::MenuHeight(items);
  const IRECT menuRect(anchorRect.L, anchorRect.B + 6.f, anchorRect.L + menuW, anchorRect.B + 6.f + menuH);

  menu->SetTargetAndDrawRECTs(menuRect);
  menu->SetItems(slot, items, selected);
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

void NeuralAmpModeler::_VolumShowSupportAmpMenu(const IRECT& anchorRect)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  auto* rawCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSupportAmpMenu);
  if (!rawCtrl)
    return;

  auto* menu = rawCtrl->As<VoLumSupportAmpMenuControl>();
  if (!menu)
    return;

  if (!rawCtrl->IsHidden())
  {
    _VolumHideSupportAmpMenu();
    return;
  }

  // Item 0 = "(none)" so the user can clear the support amp without disabling Dual Amp mode.
  std::vector<std::string> labels;
  labels.reserve(static_cast<size_t>(volum::kAmpCount + 1));
  labels.emplace_back("(none)");
  for (int i = 0; i < volum::kAmpCount; ++i)
    labels.emplace_back(volum::kAmps[i].displayName);

  const int currentSupportIdx = GetParam(kSupportAmpIdx)->Int();
  const int selected = std::clamp(currentSupportIdx + 1, 0, static_cast<int>(labels.size() - 1));

  const float menuW = std::max(anchorRect.W() * 0.96f, 200.f);
  const float menuH = VoLumSupportAmpMenuControl::ItemHeight() * static_cast<float>(labels.size()) + 12.f;
  const float panelW = static_cast<float>(pGfx->Width());
  const float panelH = static_cast<float>(pGfx->Height());
  const float anchorL = std::min(anchorRect.L, panelW - menuW - 8.f);

  // Prefer to drop the menu BELOW the support hero so the support panel stays clickable to
  // dismiss the dropdown. If it doesn't fit (host shrinks the canvas, smaller standalone window,
  // etc.), pop it ABOVE the hero instead — never clip the rect, otherwise the bottom amps
  // become unreachable.
  const float spaceBelow = panelH - (anchorRect.B + 6.f) - 8.f;
  const float spaceAbove = anchorRect.T - 6.f - 8.f;
  float menuT;
  if (menuH <= spaceBelow)
    menuT = anchorRect.B + 6.f;
  else if (menuH <= spaceAbove)
    menuT = anchorRect.T - 6.f - menuH;
  else
    // Last-resort: pin to the panel top with full natural height. Some items may extend past
    // the visible canvas, but at this point the host window is just too short for the list.
    menuT = std::max(8.f, panelH - menuH - 8.f);

  const IRECT menuRect(anchorL, menuT, anchorL + menuW, menuT + menuH);

  menu->SetTargetAndDrawRECTs(menuRect);
  menu->SetItems(labels, selected);
  menu->Hide(false);
}

void NeuralAmpModeler::_VolumHideSupportAmpMenu()
{
  if (auto* pGfx = GetUI())
  {
    if (auto* menu = pGfx->GetControlWithTag(kCtrlTagVoLumSupportAmpMenu))
      menu->Hide(true);
  }
}

void NeuralAmpModeler::_VolumSetSupportAmp(int ampIdx)
{
  const int clamped = std::clamp(ampIdx, -1, volum::kAmpCount - 1);
  if (GetParam(kSupportAmpIdx)->Int() == clamped)
    return;

  GetParam(kSupportAmpIdx)->Set(clamped);
  SendParameterValueFromDelegate(kSupportAmpIdx, GetParam(kSupportAmpIdx)->GetNormalized(), true);
  mVolumSupportNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  _VolumRefreshSupportChannels();
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumRefreshSupportChannels()
{
  mVolumSupportChannelFiles.clear();
  mVolumSupportChannelLabels.clear();

  const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
  if (supportAmpIdx >= 0 && supportAmpIdx < volum::kAmpCount && !mVolumRigsRoot.empty())
  {
    const int speakerIdx = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3);
    auto channels = volum::DiscoverChannels(
      std::filesystem::path(mVolumRigsRoot),
      volum::kAmps[supportAmpIdx].folderName,
      volum::kSpeakerPrefixes[speakerIdx]);
    for (auto& ch : channels)
    {
      mVolumSupportChannelFiles.push_back(std::move(ch.filename));
      mVolumSupportChannelLabels.push_back(std::move(ch.label));
    }

    int channelIdx = std::clamp(GetParam(kSupportChannelIdx)->Int(), 0,
                                std::max(0, static_cast<int>(mVolumSupportChannelFiles.size()) - 1));
    if (channelIdx != GetParam(kSupportChannelIdx)->Int())
    {
      GetParam(kSupportChannelIdx)->Set(channelIdx);
      SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
    }
  }

  if (auto* pGfx = GetUI())
  {
    if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumSupportChannelStep))
      stepper->As<VoLumChannelStepControl>()->SetChannels(
        mVolumSupportChannelLabels, GetParam(kSupportChannelIdx)->Int());
  }
}

void NeuralAmpModeler::_VolumApplyDualAmpFocus()
{
  // Sync the speaker row to the focused lane (cab selection is per-amp). Lane belonging on the
  // SUPPORT amp-row knobs is conveyed by their teal pointer dot + teal value text — set once at
  // attach time, no per-frame retoggling needed here.
  auto* pGfx = GetUI();
  if (!pGfx)
    return;

  const bool dualActive = GetParam(kDualAmpActive)->Bool();
  const bool supportFocus = dualActive && mVolumDualAmpFocusedSupport;
  const bool showPanKnobs = dualActive && mVolumExpandedSection == EVoLumSection::AMP;
  const bool showSupportPolarity =
    showPanKnobs && GetParam(kSupportAmpIdx)->Int() >= 0 && GetParam(kSupportAmpIdx)->Int() < volum::kAmpCount;

  if (auto* spkRow = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
  {
    auto* row = spkRow->As<VoLumSpeakerRowControl>();
    const int focusedSpeakerIdx = supportFocus
      ? std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3)
      : mVolumSpeakerIdx;
    row->SetSelected(focusedSpeakerIdx);
  }

  // PAN knobs follow Dual Amp: shown only in dual mode, and their slot tracks the hero geometry.
  if (auto* hero = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
  {
    auto* heroCtrl = hero->As<VoLumHeroImageControl>();
    if (auto* mainPanGrp = pGfx)
    {
      mainPanGrp->ForControlInGroup("MAIN_PAN_KNOB", [&](IControl* c) {
        c->SetTargetAndDrawRECTs(heroCtrl->GetMainPanKnobSlot());
        c->Hide(!showPanKnobs);
      });
      mainPanGrp->ForControlInGroup("SUPPORT_PAN_KNOB", [&](IControl* c) {
        c->SetTargetAndDrawRECTs(heroCtrl->GetSupportPanKnobSlot());
        c->Hide(!showPanKnobs);
      });
      mainPanGrp->ForControlInGroup("SUPPORT_POLARITY_TOGGLE", [&](IControl* c) {
        c->SetTargetAndDrawRECTs(heroCtrl->GetSupportPolarityToggleSlot());
        c->Hide(!showSupportPolarity);
      });
    }
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
    mVolumLoadingMainPath.clear();
    mVolumLoadingSupportPath.clear();
    mVolumLoadingPrePath[0].clear();
    mVolumLoadingPrePath[1].clear();
  }
  mVolumLoaderCv.notify_one();

  if (mVolumLoaderThread.joinable())
    mVolumLoaderThread.join();
}

void NeuralAmpModeler::_VolumQueueMainModelLoad(std::string fileToLoad, int ampIdx, std::string rigsRoot)
{
  if (fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Main;
  request.ampIdx = ampIdx;
  request.fileToLoad = fileToLoad;
  request.rigsRoot = std::move(rigsRoot);
  request.sampleRate = GetSampleRate();
  request.blockSize = GetBlockSize();

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumLoadingMainPath == fileToLoad)
      return;

    mVolumLoadingMainPath = fileToLoad;
    mVolumLoadRequests.erase(
      std::remove_if(mVolumLoadRequests.begin(), mVolumLoadRequests.end(), [](const VoLumLoadRequest& queued) {
        return queued.kind == VoLumLoadKind::Main || queued.kind == VoLumLoadKind::MainPrefetch;
      }),
      mVolumLoadRequests.end());
    mVolumLoadRequests.push_front(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumQueueMainPrefetch(std::string fileToLoad)
{
  if (fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::MainPrefetch;
  request.fileToLoad = fileToLoad;

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumDspCache.find(fileToLoad) != mVolumDspCache.end())
      return;
    const auto alreadyQueued =
      std::any_of(mVolumLoadRequests.begin(), mVolumLoadRequests.end(), [&](const VoLumLoadRequest& queued) {
        return queued.kind == VoLumLoadKind::MainPrefetch && queued.fileToLoad == fileToLoad;
      });
    if (alreadyQueued)
      return;
    mVolumLoadRequests.push_back(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumQueueSupportModelLoad(std::string fileToLoad, int ampIdx)
{
  if (fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Support;
  request.ampIdx = ampIdx;
  request.fileToLoad = fileToLoad;
  request.sampleRate = GetSampleRate();
  request.blockSize = GetBlockSize();

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumLoadingSupportPath == fileToLoad)
      return;
    mVolumLoadingSupportPath = fileToLoad;
    mVolumLoadRequests.push_back(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumQueuePreNamLoad(int slot, std::string fileToLoad)
{
  if (slot < 0 || slot >= 2 || fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Pre;
  request.slot = slot;
  request.fileToLoad = fileToLoad;
  request.sampleRate = GetSampleRate();
  request.blockSize = GetBlockSize();

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumLoadingPrePath[slot] == fileToLoad)
      return;
    mVolumLoadingPrePath[slot] = fileToLoad;
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
      {
        std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
        if (mVolumLoadingMainPath == result.path)
          mVolumLoadingMainPath.clear();
      }
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

    if (result.kind == VoLumLoadKind::Support)
    {
      {
        std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
        if (mVolumLoadingSupportPath == result.path)
          mVolumLoadingSupportPath.clear();
      }
      mVolumSupportIsLoading.store(false);
      if (mVolumSupportNeedsLoad.load())
        continue;

      if (!result.error.empty())
      {
        mShouldRemoveSupportModel.store(true);
        continue;
      }

      if (result.model != nullptr)
        mStagedSupportModel = std::move(result.model);
      continue;
    }

    const int slot = result.slot;
    if (slot < 0 || slot >= 2)
      continue;

    mVolumPreIsLoading[slot].store(false);
    {
      std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
      if (mVolumLoadingPrePath[slot] == result.path)
        mVolumLoadingPrePath[slot].clear();
    }
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

  auto touchCache = [&](const std::string& key) {
    mVolumDspCacheOrder.erase(std::remove(mVolumDspCacheOrder.begin(), mVolumDspCacheOrder.end(), key),
                              mVolumDspCacheOrder.end());
    mVolumDspCacheOrder.push_front(key);
  };

  auto storeCache = [&](const std::string& key, nam::dspData&& config) {
    mVolumDspCache[key] = std::move(config);
    touchCache(key);
    while (mVolumDspCacheOrder.size() > kVolumDspCacheMaxEntries)
    {
      mVolumDspCache.erase(mVolumDspCacheOrder.back());
      mVolumDspCacheOrder.pop_back();
    }
  };

  auto makeModel = [&](const std::string& path) {
    auto cacheIt = mVolumDspCache.find(path);
    if (cacheIt != mVolumDspCache.end())
    {
      touchCache(path);
      // Core consumes dspData::weights during construction, so keep the cached copy immutable.
      nam::dspData cachedConfig = cacheIt->second;
      return nam::get_dsp(cachedConfig);
    }

    nam::dspData conf;
    auto model = nam::get_dsp(fs::u8path(path), conf);
    storeCache(path, std::move(conf));
    return model;
  };

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
        auto model = makeModel(request.fileToLoad);
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

              if (entry.path().extension() != ".nam")
                continue;

              std::error_code pathEc;
              const std::string prefetchPath = fs::weakly_canonical(entry.path(), pathEc).string();
              if (pathEc || prefetchPath.empty() || prefetchPath == request.fileToLoad)
                continue;
              if (mVolumDspCache.find(prefetchPath) == mVolumDspCache.end())
              {
                _VolumQueueMainPrefetch(prefetchPath);
              }
            }
          }
        }
      }
      else if (request.kind == VoLumLoadKind::MainPrefetch)
      {
        if (!mVolumNeedsLoad.load() && !mVolumLoaderStop.load() && mVolumDspCache.find(request.fileToLoad) == mVolumDspCache.end())
        {
          nam::dspData conf;
          nam::get_dsp(fs::u8path(request.fileToLoad), conf);
          storeCache(request.fileToLoad, std::move(conf));
        }
      }
      else
      {
        auto model = makeModel(request.fileToLoad);
        result.model = std::make_unique<ResamplingNAM>(std::move(model), request.sampleRate);
        result.model->Reset(request.sampleRate, request.blockSize);
      }
    }
    catch (const std::runtime_error& e)
    {
      result.error = e.what();
      if (request.kind == VoLumLoadKind::Main)
        std::cerr << "VoLum load failed: " << result.error << std::endl;
      else if (request.kind == VoLumLoadKind::Support)
        std::cerr << "VoLum support load failed: " << result.error << std::endl;
      else if (request.kind == VoLumLoadKind::MainPrefetch)
        std::cerr << "VoLum prefetch failed: " << result.error << std::endl;
      else
        std::cerr << "VoLum PRE load failed: " << result.error << std::endl;
    }

    if (request.kind == VoLumLoadKind::MainPrefetch)
      continue;

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

  const int activeParam = slot == 0 ? kPreNam1Active : kPreNam2Active;
  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  if (!volum::ShouldLoadPrePedalCapture(GetParam(activeParam)->Bool(), GetParam(captureParam)->Int()))
  {
    mShouldRemovePreModel[slot].store(true);
    mVolumPreIsLoading[slot].store(false);
    return;
  }

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

void NeuralAmpModeler::_VolumRequestSupportModelLoad()
{
  const bool dualActive = GetParam(kDualAmpActive)->Bool();
  const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
  if (!dualActive || supportAmpIdx < 0 || supportAmpIdx >= volum::kAmpCount || mVolumRigsRoot.empty())
  {
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  namespace fs = std::filesystem;
  const int speakerIdx = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3);
  auto channels = volum::DiscoverChannels(
    fs::path(mVolumRigsRoot), volum::kAmps[supportAmpIdx].folderName, volum::kSpeakerPrefixes[speakerIdx]);
  if (channels.empty())
  {
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  int channelIdx = std::clamp(GetParam(kSupportChannelIdx)->Int(), 0, static_cast<int>(channels.size()) - 1);
  if (channelIdx != GetParam(kSupportChannelIdx)->Int())
  {
    GetParam(kSupportChannelIdx)->Set(channelIdx);
    SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
  }

  const auto rigPath = fs::path(mVolumRigsRoot) / volum::kAmps[supportAmpIdx].folderName / channels[channelIdx].filename;
  std::error_code ec;
  const std::string fileToLoad = fs::weakly_canonical(rigPath, ec).string();
  if (fileToLoad.empty())
  {
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  mVolumSupportIsLoading.store(true);
  mVolumLastLoadedSupportFile = std::filesystem::path(fileToLoad).filename().string();
  _VolumQueueSupportModelLoad(fileToLoad, supportAmpIdx);
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
}

void NeuralAmpModeler::_VolumSaveEffectSettings()
{
  mVolumEffectSettings.delayActive = GetParam(kDelayActive)->Bool();
  mVolumEffectSettings.delayMode = GetParam(kDelayMode)->Int();
  mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
  mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
  _VolumSaveDelayModeSnapshot(std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1));
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
  _VolumRestoreDelayModeSnapshot(std::clamp(fx.delayMode, 0, volum::kVoLumDelayModeCount - 1));
  setParam(kReverbActive, fx.reverbActive ? 1.0 : 0.0);
  setParam(kReverbMode, fx.reverbMode);
  _VolumRestoreReverbModeSnapshot(std::clamp(fx.reverbMode, 0, 2));
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumSaveDelayModeSnapshot(int mode)
{
  auto& s = mVolumEffectSettings.delayModes[std::clamp(mode, 0, volum::kVoLumDelayModeCount - 1)];
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
  const auto& s = mVolumEffectSettings.delayModes[std::clamp(mode, 0, volum::kVoLumDelayModeCount - 1)];
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
#if VOLUM_AMPETE_PRODUCT
  mSupportToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
#endif
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
  int preLatency = 0;
#if VOLUM_AMPETE_PRODUCT
  const bool preNam1ShouldLoad =
    volum::ShouldLoadPrePedalCapture(GetParam(kPreNam1Active)->Bool(), GetParam(kPreNam1Capture)->Int());
  const bool preNam2ShouldLoad =
    volum::ShouldLoadPrePedalCapture(GetParam(kPreNam2Active)->Bool(), GetParam(kPreNam2Capture)->Int());
  if (preNam1ShouldLoad && mPreModel[0])
    preLatency += mPreModel[0]->GetLatency();
  if (preNam2ShouldLoad && mPreModel[1])
    preLatency += mPreModel[1]->GetLatency();
#endif

  int ampLatency = 0;
  if (mModel)
  {
    ampLatency = mModel->GetLatency();
  }
#if VOLUM_AMPETE_PRODUCT
  if (GetParam(kDualAmpActive)->Bool() && mSupportModel)
  {
    ampLatency = std::max(ampLatency, mSupportModel->GetLatency());
  }
#endif
  // Other things that add latency here...
  const int latency = preLatency + ampLatency;

  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }

  if (auto* pGraphics = GetUI())
  {
    if (auto* settings = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
      settings->As<NAMSettingsPageControl>()->SetCurrentLatency(GetLatency(), GetSampleRate());
  }
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  // L (channel 0) goes to the primary OUT meter.
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);

#if VOLUM_AMPETE_PRODUCT
  // R (channel 1) goes to the second OUT meter only in dual-amp/stereo mode.
  if (nChansOut > 1 && GetParam(kDualAmpActive)->Bool())
  {
    sample* rightPtr = outputPointer[1];
    sample** rightBlock = &rightPtr;
    mOutputSenderR.ProcessBlock(rightBlock, (int)nFrames, kCtrlTagOutputMeterR, nChansHack);
  }
#endif
}

// HACK
#include "Unserialization.cpp"
