#include <algorithm> // std::clamp, std::min
#include <cassert> // RT capacity invariants
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
#include "VoLumLevelMute.h"
#include "VoLumMasterSafety.h"
#include "VoLumNanGuard.h"
#include "VoLumPaths.h"
#include "VoLumPrePedalCaptures.h"
#include "VoLumProcessIO.h"
#include "VoLumOutputMode.h"
#include "VoLumProcessingPlan.h"
// VoLum: chunk codec, settings I/O, and custom controls (upstream-equivalent file fence)
#include "VoLumChunkCodec.h"
#include "VoLumUserSettingsIO.h"
#include "VoLumControls.h"
#include "VoLumCustomUi.h"

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

void _SetMuteFloorDbDisplay(IParam* pParam)
{
  const double minimumDb = pParam->GetMin();
  pParam->SetDisplayFunc([minimumDb](double value, WDL_String& display) {
    if (volum::IsLevelMuteValue(value, minimumDb))
      display.Set("\xE2\x88\x92\xE2\x88\x9E");
    else
      display.SetFormatted(32, "%.1f", value);
  });
}

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
    SetTooltip(paramIdx == kSupportAmpPan ? "Pan the SUPPORT amp lane." : "Pan the MAIN amp lane.");
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

    if (mMouseIsOver)
      g.FillCircle(accent.WithOpacity(0.15f), cx, cy, widgetRadius + 3.f);

    // Subtle outer ring matches the lane accent without a heavy disc fill.
    g.DrawCircle(accent.WithOpacity(mMouseIsOver ? 0.85f : 0.35f), cx, cy, widgetRadius - 0.5f,
                 nullptr, mMouseIsOver ? 1.75f : 1.f);
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
  _SetMuteFloorDbDisplay(GetParam(kOutputLevel));
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", volum::kOutputModeDefault,
                                  {volum::kOutputModeLabels[0], volum::kOutputModeLabels[1], volum::kOutputModeLabels[2]});
#ifdef APP_API
  GetParam(kIRToggle)->InitBool("IRToggle", false);
#else
  GetParam(kIRToggle)->InitBool("IRToggle", true);
#endif

  // Delay (effect-staging order: Digital, Analog, Reverse)
  GetParam(kDelayActive)->InitBool("DelayActive", false);
  GetParam(kDelayTime)->InitDouble("DelayTime", 320.0, 10.0, 2000.0, 1.0, "ms");
  GetParam(kDelayFeedback)->InitDouble("DelayFeedback", 0.35, 0.0, 0.99, 0.01);
  GetParam(kDelayMix)->InitDouble("DelayMix", 0.28, 0.0, 1.0, 0.01);
  GetParam(kDelayMode)->InitEnum("DelayMode", volum::kVoLumDelayModeDigital,
                                  {"Digital", "Analog", "Reverse"});
  GetParam(kDelayTone)->InitDouble("DelayTone", 0.5, 0.0, 1.0, 0.01);
  GetParam(kDelayAge)->InitDouble("DelayAge", 0.0, 0.0, 1.0, 0.01);
  GetParam(kDelayPingPong)->InitBool("DelayPingPong", false);

  // Reverb (effect-staging order: Hall, Plate, Oktaverb)
  GetParam(kReverbActive)->InitBool("ReverbActive", false);
  GetParam(kReverbMix)->InitDouble("ReverbMix", 0.20, 0.0, 1.0, 0.01);
  GetParam(kReverbDecay)->InitDouble("ReverbDecay", 2.5, 0.1, 10.0, 0.1, "s");
  GetParam(kReverbTone)->InitDouble("ReverbTone", 5.0, 0.0, 10.0, 0.1);
  GetParam(kReverbPreDelay)->InitDouble("ReverbPreDelay", 30.0, 0.0, 200.0, 1.0, "ms");
  GetParam(kReverbShimmer)->InitDouble("ReverbShimmer", 0.0, 0.0, 1.0, 0.01);
  GetParam(kReverbMode)->InitEnum("ReverbMode", volum::kVoLumReverbModeHall,
                                  {"Hall", "Plate", "Oktaverb"});
  // Oktaverb-only sub-toggle.
  GetParam(kReverbSubMode)->InitEnum("ReverbSubMode", 1, {"Halo", "Shimmer", "Bloom"});

  // Reserved legacy boost params. Keep them initialized for old chunks even though PRE captures replaced this block.
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
  _SetMuteFloorDbDisplay(GetParam(kPreCompLevel));
  GetParam(kPreNam1Active)->InitBool("PreNam1Active", false);
  GetParam(kPreNam1Capture)->InitDouble("PreNam1Capture", 0.0, 0.0, 127.0, 1.0);
  GetParam(kPreNam1Gain)->InitDouble("PreNam1Gain", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kPreNam1Bass)->InitDouble("PreNam1Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam1Mid)->InitDouble("PreNam1Mid", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam1MidFreq)->InitDouble("PreNam1MidFreq", 650.0, 150.0, 2500.0, 10.0, "Hz");
  GetParam(kPreNam1Treble)->InitDouble("PreNam1Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam1Level)->InitDouble("PreNam1Level", 0.0, -20.0, 20.0, 0.1, "dB");
  _SetMuteFloorDbDisplay(GetParam(kPreNam1Level));
  GetParam(kPreNam2Active)->InitBool("PreNam2Active", false);
  GetParam(kPreNam2Capture)->InitDouble("PreNam2Capture", 0.0, 0.0, 127.0, 1.0);
  GetParam(kPreNam2Gain)->InitDouble("PreNam2Gain", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kPreNam2Bass)->InitDouble("PreNam2Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam2Mid)->InitDouble("PreNam2Mid", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam2MidFreq)->InitDouble("PreNam2MidFreq", 650.0, 150.0, 2500.0, 10.0, "Hz");
  GetParam(kPreNam2Treble)->InitDouble("PreNam2Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kPreNam2Level)->InitDouble("PreNam2Level", 0.0, -20.0, 20.0, 0.1, "dB");
  _SetMuteFloorDbDisplay(GetParam(kPreNam2Level));

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
  _SetMuteFloorDbDisplay(GetParam(kSupportOutputLevel));
  GetParam(kSupportNoiseGateActive)->InitBool("SupportNoiseGateActive", true);
  GetParam(kSupportEQActive)->InitBool("SupportToneStack", true);
  GetParam(kSupportAmpPan)->InitDouble("SupportAmpPan", 0.0, -1.0, 1.0, 0.01);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);
  mSupportNoiseGateTrigger.AddListener(&mSupportNoiseGateGain);

  {
    auto root = volum::FindRigsRootDirectory();
    if (!root.empty())
      mVolumRigsRoot = root.string();
    _VolumLoadSettingsFromFile();
    _VolumRestoreFromSettings(mVolumAmpIdx);
    _VolumApplyLiveLockSnapshots();
    _VolumRefreshPrePedalCaptures();
    _VolumRefreshChannels();
    _VolumRefreshSupportChannels();
    mVolumNeedsLoad.store(true);
    mVolumInitComplete = true;
    _VolumStartLoader();
  }

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

    // VoLum: Variant F UI layout (sidebar, triptych, hero, knob row)
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
#ifdef APP_API
          _VolumSaveSettingsToFile();
#endif

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

          // F5: refresh the header preset strip to this amp's preset bank (mock).
          if (auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
            pb->As<VoLumPresetBarControl>()->SetList(volum::custom::MockPresetsForAmp(ampIdx));
          
          if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych)) {
             auto* trip = tripCtrl->As<VoLumTriptychControl>();
             const bool preActive = GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
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
      ampList->SetCustomAmps(volum::custom::MockCustomAmps());
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
              ov->As<VoLumCustomOverlayControl>()->ShowBuilder(true, nm);
        },
        // bin: delete the custom amp from the live session list + refresh sidebar
        [this](int customIdx) {
          volum::custom::RemoveCustomAmp(customIdx);
          if (auto* pGfx = GetUI())
            if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
            {
              auto* list = al->As<VoLumAmpListControl>();
              list->SetCustomAmps(volum::custom::MockCustomAmps());
              list->SetCustomSelected(-1);
            }
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
        // Rows: "No custom IR" + each IR + a single "Manage custom IRs..." entry.
        std::vector<VoLumListMenuControl::Row> rows;
        rows.push_back({"No custom IR (use baked cab)", VoLumListMenuControl::kNone, false, false});
        for (int i = 0; i < (int)irs.size(); i++)
          rows.push_back({irs[(size_t)i], i, false, false});
        rows.push_back({"Manage custom IRs...", VoLumListMenuControl::kManage, true, false});

        const float w = 230.f;
        const float h = VoLumListMenuControl::MenuHeight(rows.size());
        const auto bounds = pGfx->GetBounds();
        float l = anchor.L;
        if (l + w > bounds.R - 4.f)
          l = bounds.R - 4.f - w;
        menu->SetTargetAndDrawRECTs(IRECT(l, anchor.B + 4.f, l + w, anchor.B + 4.f + h));
        // Reflect the speaker row's currently active IR (or "No custom IR").
        int selectedIr = VoLumListMenuControl::kNone;
        if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
        {
          auto* row = spk->As<VoLumSpeakerRowControl>();
          if (row->IsIrCabActive())
            for (int i = 0; i < (int)irs.size(); i++)
              if (irs[(size_t)i] == row->IrName())
                selectedIr = i;
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
    auto* hero = new VoLumHeroImageControl(heroArea,
      [this](bool supportFocused) {
        mVolumDualAmpFocusedSupport = supportFocused;
        mVolumFocusedEffect = EVoLumEffectFocus::AMP;
        _UpdateVoLumLayout();
        _UpdateVoLumKeyboardFocusHint();
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
        _UpdateVoLumKeyboardFocusHint();
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
    drawKnobCol(4, "PRE-DLY", kReverbPreDelay, "ms", "REVERB_PREDELAY", true, 5, 1, effectKnobOffset, effectColW);
    drawKnobCol(5, "INTENSITY", kReverbShimmer, "%", "REVERB_SHIMMER", true, 5, 1, effectKnobOffset, effectColW);
    IRECT reverbPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    pGraphics->AttachControl(new VoLumModePickerControl(reverbPickerRect, kReverbMode, {"HALL", "PLATE", "OKTAVERB"}), -1, "REVERB_KNOBS");

    // Reverb sub-mode pill is currently used by Oktaverb only. Keep the reusable pill UI,
    // including the slimmer row and hover feedback, but do not expose placeholder modes.
    const float subPillW = 256.f;
    // Slimmer than the AMP-row toggleH (34) — the row carries text-only pill labels and a
    // single slide-switch, so a tighter 28 px height keeps it from feeling visually heavy.
    const float subPillH = 28.f;
    const float subPillY = knobT + knobDiam + valueH + 18.f;
    IRECT reverbSubPillRect(mainCX - subPillW / 2.f, subPillY, mainCX + subPillW / 2.f, subPillY + subPillH);
    auto* reverbSubPill = new VoLumSubModePillControl(reverbSubPillRect, kReverbSubMode,
                                                     {"HALO", "SHIMMER", "BLOOM"});
    pGraphics->AttachControl(reverbSubPill, -1, "REVERB_SUBTOGGLE");
    mVolumReverbSubModePill = reverbSubPill;

    float revSwX = mainCX - 242.f;
    pGraphics->AttachControl(new VoLumPowerSwitchControl(IRECT(revSwX - 14.f, knobT - 4.f, revSwX + 14.f, knobT + knobDiam + 2.f), kReverbActive), -1, "REVERB_POWER");

    // DELAY KNOBS (Centered) - 5 slots: TIME, FEEDBACK, MIX, TONE, AGE
    drawKnobCol(1, "TIME", kDelayTime, "ms", "DELAY_KNOBS", true, 5, 1, effectKnobOffset, effectColW);
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
      const float cx = mainCX + effectKnobOffset - (5 * customColW) / 2.f + (slot - 1) * customColW + (customColW / 2.f);
      const float kL = cx - (knobDiam / 2.f);
      auto* ageLabel = new VoLumKnobLabelControl(IRECT(cx - 40.f, knobRowTop, cx + 40.f, knobRowTop + 20.f), "AGE");
      pGraphics->AttachControl(ageLabel, -1, "DELAY_KNOBS");
      auto* ageKnob = new NAMKnobControl(IRECT(kL, knobT, kL + knobDiam, knobT + knobDiam), kDelayAge, "", volumKnobStyle, knobBackgroundBitmap);
      pGraphics->AttachControl(ageKnob, -1, "DELAY_KNOBS");
      ageKnob->SetSelectedForKeyboard(mVolumSelectedKnobParamIdx == kDelayAge);
      auto* ageValue = new VoLumParamValueControl(IRECT(cx - 30.f, knobT + knobDiam + 2.f, cx + 30.f, knobT + knobDiam + 2.f + valueH), kDelayAge, "");
      pGraphics->AttachControl(ageValue, -1, "DELAY_KNOBS");
      mVolumDelayAgeLabel = ageLabel;
      mVolumDelayAgeKnob = ageKnob;
      mVolumDelayAgeValue = ageValue;
    }
    IRECT delayPickerRect(mainCX + 140.f, knobT + 2.f, mainCX + 230.f, knobT + knobDiam + valueH - 2.f);
    pGraphics->AttachControl(new VoLumModePickerControl(delayPickerRect, kDelayMode, {"DIGITAL", "ANALOG", "REVERSE"}), -1, "DELAY_KNOBS");

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
      new NAMSwitchControl(IRECT(ppSwitchX, ppSwitchY, ppSwitchX + ppSwitchW, ppSwitchY + ppSwitchH),
                           kDelayPingPong, "", volumToggleStyle, switchHandleBitmap),
      -1, "DELAY_PINGPONG");
    pGraphics->AttachControl(
      new VoLumKnobLabelControl(IRECT(ppSwitchX + ppSwitchW + 4.f, ppSwitchY,
                                       ppSwitchX + ppSwitchW + 4.f + ppLabelW, ppSwitchY + ppSwitchH),
                                "PING-PONG"),
      -1, "DELAY_PINGPONG");

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

    // 1176-style FET compressor: Input drives, Attack/Release, Output. Ratio fixed at 4:1; Mix locked at 1.0
    // (kPreCompRatio and kPreCompMix retained as EParams for state compatibility but hidden from UI).
    drawKnobCol(2, "INPUT", kPreCompAmount, "", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(3, "ATTACK", kPreCompAttack, "ms", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(4, "RELEASE", kPreCompRelease, "ms", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);
    drawKnobCol(5, "OUTPUT", kPreCompLevel, "dB", "COMP_KNOBS", true, 6, 1, 0.f, 66.f);

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

      // F5 preset bar — centred in the top header band, above the AMP/triptych
      // column. Clicking opens the anchored preset dropdown; < > cycle presets.
      {
        const float presetBarW = 240.f;
        const IRECT presetBarArea(mainCX - presetBarW * 0.5f, b.T + 12.f, mainCX + presetBarW * 0.5f, b.T + 40.f);
        pGraphics->AttachControl(
          new VoLumPresetBarControl(
            presetBarArea,
            [pPlugin]() { pPlugin->_VolumShowPresetMenu(); }),
          kCtrlTagVoLumPresetBar);
        if (auto* pb = pGraphics->GetControlWithTag(kCtrlTagVoLumPresetBar))
          pb->As<VoLumPresetBarControl>()->SetList(volum::custom::MockPresetsForAmp(mVolumAmpIdx));
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
              ov->As<VoLumCustomOverlayControl>()->ShowManage(
                VoLumCustomOverlayControl::ManageKind::Presets, pPlugin->mVolumAmpIdx,
                volum::kAmps[pPlugin->mVolumAmpIdx].displayName);
            return;
          }
          const auto presets = volum::custom::MockPresetsForAmp(pPlugin->mVolumAmpIdx);
          if (code >= 0 && code < (int)presets.size())
            if (auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
              pb->As<VoLumPresetBarControl>()->SelectName(presets[(size_t)code].c_str()); // mock recall
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
          auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
          if (!spk)
            return;
          auto* row = spk->As<VoLumSpeakerRowControl>();
          const auto& irs = volum::custom::MockIRLibrary();
          if (code >= 0 && code < (int)irs.size())
            row->SetIrCab(true, irs[(size_t)code].c_str()); // DIRECT capture + this IR (mock)
          else if (code == VoLumListMenuControl::kNone)
            row->SetIrCab(false, ""); // back to baked cab
        });
        pGraphics->AttachControl(irMenu, kCtrlTagVoLumIrMenu)->Hide(true);
      }

      // Manage + Builder overlay (on top of everything; hidden until invoked).
      {
        auto* overlay = new VoLumCustomOverlayControl(b);
        overlay->SetCallbacks(
          // custom amp saved from the builder -> add to the live session list,
          // refresh the sidebar, and select the new amp (mock; no disk).
          [pPlugin](const char* name) {
            const int idx = volum::custom::AddCustomAmp(name ? name : "");
            auto* pGfx = pPlugin->GetUI();
            if (!pGfx)
              return;
            const auto& amps = volum::custom::MockCustomAmps();
            if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
            {
              auto* list = al->As<VoLumAmpListControl>();
              list->SetCustomAmps(amps);
              list->SetCustomSelected(idx);
            }
            if (idx >= 0 && idx < (int)amps.size())
              if (auto* hero = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
                hero->As<VoLumHeroImageControl>()->SetName(amps[idx].c_str());
          },
          // preset bank mutated (save/rename/delete) -> re-sync the header strip
          // for the currently focused factory amp.
          [pPlugin]() {
            if (auto* pb = pPlugin->GetUI()->GetControlWithTag(kCtrlTagVoLumPresetBar))
              pb->As<VoLumPresetBarControl>()->SetList(volum::custom::MockPresetsForAmp(pPlugin->mVolumAmpIdx));
          });
        pGraphics->AttachControl(overlay, kCtrlTagVoLumCustomOverlay)->Hide(true);
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

    // Keyboard: keep the original arrows, add a shallow PRE/AMP/POST focus layer.
    pGraphics->SetKeyHandlerFunc([this](const IKeyPress& key, bool isUp) {
      if (isUp) return false;

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
        _VolumRestoreFromSettings(newIdx);
        _VolumRefreshChannels();
        mVolumNeedsLoad.store(true);
#ifdef APP_API
        _VolumSaveSettingsToFile();
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
          // F5: refresh the header preset strip to this amp's bank (mock).
          if (auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
            pb->As<VoLumPresetBarControl>()->SetList(volum::custom::MockPresetsForAmp(newIdx));
          if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych)) {
             auto* trip = tripCtrl->As<VoLumTriptychControl>();
             const bool preActive = GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
             trip->SetState(
               preActive, GetParam(kDelayActive)->Value() || GetParam(kReverbActive)->Value(), newIdx,
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
          if (GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport)
          {
            const int delta = key.VK == kVK_LEFT ? -1 : 1;
            const int channelCount = !mVolumSupportChannelLabels.empty()
              ? static_cast<int>(mVolumSupportChannelLabels.size())
              : 128;
            const int current = std::clamp(GetParam(kSupportChannelIdx)->Int(), 0, channelCount - 1);
            const int next = (current + delta + channelCount) % channelCount;
            GetParam(kSupportChannelIdx)->Set(next);
            SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
            if (auto* pGfx = GetUI())
              if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumSupportChannelStep))
                stepper->As<VoLumChannelStepControl>()->SetChannels(mVolumSupportChannelLabels, next);
            mVolumSupportNeedsLoad.store(true);
            mVolumSettingsDirty = true;
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

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  _VolumStopLoader();
  _VolumSaveCurrentToSettings();
#ifdef APP_API
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

  // Tuner reads from mono input (post-gain, pre-NAM)
  mTunerDSP.Process(mInputPointers[0], nFrames);
  _ApplyDSPStaging();
  sample** preAmpPointers = mInputPointers;
  const bool haveMainModel = (mModel != nullptr);
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();
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
  preAmpPointers =
    _VolumProcessPreChain(preAmpPointers, processingPlan, numChannelsInternal, nFrames, sampleRate);

  if (processingPlan.runDualAmp)
  {
    // Capacity invariant: OnReset() pre-allocates these scratch buffers to
    // maxBlockSize, so .resize() here must NEVER reallocate on the audio
    // thread. assert() is a no-op in NDEBUG release builds and fires in
    // debug + CI sanitizer builds if the invariant ever regresses.
    assert(mDualMainLaneBuffer.capacity() >= static_cast<size_t>(numFrames) && "Dual-amp main scratch not pre-reserved");
    mDualMainLaneBuffer.resize(numFrames);
    std::memcpy(mDualMainLaneBuffer.data(), preAmpPointers[0], numFrames * sizeof(sample));
  }

  sample** hpfPointers =
    _VolumProcessMainAmpChain(preAmpPointers, processingPlan, numChannelsInternal, nFrames, sampleRate);
  sample* supportLane = _VolumProcessDualAmpSupportLane(processingPlan, numChannelsInternal, nFrames, sampleRate);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
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
    assert(mDualMainAlignedBuffer.capacity() >= static_cast<size_t>(numFrames) && "Dual-amp main-aligned scratch not pre-reserved");
    assert(mDualSupportAlignedBuffer.capacity() >= static_cast<size_t>(numFrames) && "Dual-amp support-aligned scratch not pre-reserved");
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
  {
    mDualMainLatencyDelay.Reset();
    mDualSupportLatencyDelay.Reset();
    _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  }

  _VolumProcessPostChain(outputs, processingPlan, numChannelsExternalOut, nFrames, sampleRate);

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

  // Master safety stage: stateless soft clipper at the very end of the chain. Inert below
  // ~+2.9 dBFS knee so musical material is bit-identical; smooth tanh shoulder to ~+6 dBFS
  // ceiling above. Catches user-stacked runaway (hot model + Output gain + heavy POST mix)
  // without coloring normal use. See VoLumMasterSafety.h and design plan.
  bool safetyEngagedThisBlock = false;
  for (size_t c = 0; c < numChannelsExternalOut; ++c)
  {
    iplug::sample* ch = outputs[c];
    for (size_t s = 0; s < numFrames; ++s)
    {
      const double x = static_cast<double>(ch[s]);
      const double y = volum::SoftSafetyClip(x);
      if (!safetyEngagedThisBlock && std::fabs(x) >= 1.4)
        safetyEngagedThisBlock = true;
      ch[s] = static_cast<iplug::sample>(y);
    }
  }
  if (safetyEngagedThisBlock)
  {
    constexpr double kMasterSafetyUiHoldSeconds = 2.5;
    mMasterSafetyHoldSamples = static_cast<int>(std::max(1.0, sampleRate * kMasterSafetyUiHoldSeconds));
  }
  else if (mMasterSafetyHoldSamples > 0)
  {
    mMasterSafetyHoldSamples = std::max(0, mMasterSafetyHoldSamples - static_cast<int>(numFrames));
  }
  mMasterSafetyEngaged.store(mMasterSafetyHoldSamples > 0);

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
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, kDCBlockerFrequency);
  mHighPass.SetParams(highPassParams);
  mSupportHighPass.SetParams(highPassParams);
  mMasterSafetyHoldSamples = 0;
  mMasterSafetyEngaged.store(false);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mToneStack->Reset(sampleRate, maxBlockSize);
  if (mSupportToneStack)
    mSupportToneStack->Reset(sampleRate, maxBlockSize);
  mDualMainLatencyDelay.Reset();
  mDualSupportLatencyDelay.Reset();
  for (int i = 0; i < 2; ++i)
    mPreEq[i].Reset(sampleRate, maxBlockSize);
  mPreCompressor.Reset();
  const size_t postEffectChannels = std::max<size_t>(1, static_cast<size_t>(NOutChansConnected()));
  mDelay.Prepare(postEffectChannels, static_cast<size_t>(maxBlockSize), sampleRate);
  mReverb.Prepare(postEffectChannels, static_cast<size_t>(maxBlockSize), sampleRate);
  mDelay.Reset();
  mReverb.Reset();
  mPostDelayWasActive = false;
  mPostReverbWasActive = false;
  mPostEffectsClearedForMissingModel = false;
  // Pre-reserve dual-amp scratch buffers so ProcessBlock never has to grow them on
  // the audio thread when block size or dual-amp activation changes mid-session.
  const size_t maxBlockSizeT = static_cast<size_t>(std::max(0, maxBlockSize));
  mDualMainLaneBuffer.assign(maxBlockSizeT, 0.0);
  mDualSupportLaneBuffer.assign(maxBlockSizeT, 0.0);
  mDualMainAlignedBuffer.assign(maxBlockSizeT, 0.0);
  mDualSupportAlignedBuffer.assign(maxBlockSizeT, 0.0);
  _PrepareBuffers(kNumChannelsInternal, maxBlockSizeT);
  mTunerDSP.Reset(sampleRate);
  mMetronomeDSP.Reset(sampleRate);
  _UpdateLatency();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);
  mOutputSenderR.TransmitData(*this);

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
      if (fileToLoad == mNAMPaths.live.Get())
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

  if (mVolumInitComplete && (mVolumPreLocked || mVolumPostLocked))
  {
    const bool preDirty = mVolumPreLocked && _VolumIsPreDirty();
    const bool postDirty = mVolumPostLocked && _VolumIsPostDirty();
    if (preDirty != mVolumPreLockUiDirty || postDirty != mVolumPostLockUiDirty)
    {
      mVolumPreLockUiDirty = preDirty;
      mVolumPostLockUiDirty = postDirty;
      if (auto* pGfx = GetUI())
        if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych))
          tripCtrl->SetDirty(false);
    }
  }
  else
  {
    mVolumPreLockUiDirty = false;
    mVolumPostLockUiDirty = false;
  }

  // Write settings file when dirty (knob/speaker/channel changed)
#ifdef APP_API
  if (mVolumSettingsDirty)
  {
    mVolumSettingsDirty = false;
    _VolumSaveSettingsToFile();
  }

  if (auto* pGfx = GetUI())
  {
    const bool masterSafetyActive = mMasterSafetyEngaged.load();
    if (auto* meter = pGfx->GetControlWithTag(kCtrlTagOutputMeter))
      meter->As<NAMMeterControl>()->SetSafetyActive(masterSafetyActive);
    if (auto* meterR = pGfx->GetControlWithTag(kCtrlTagOutputMeterR))
      meterR->As<NAMMeterControl>()->SetSafetyActive(masterSafetyActive);

    if (auto* footer = pGfx->GetControlWithTag(kCtrlTagVoLumFooter))
    {
      const bool dualActive = GetParam(kDualAmpActive)->Bool();
      if (masterSafetyActive)
      {
        // Master safety took priority because it indicates the actual final-bus is being
        // shaped, which is the louder problem regardless of dual-amp state.
        footer->As<VoLumFooterControl>()->SetText("Output safety active - lower output or wet mix");
      }
      else if (dualActive && mVolumDualAmpOutputHot.load())
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
  chunk.PutStr(mNAMPaths.live.Get());
  chunk.PutStr(mIRPaths.live.Get());
  bool ok = SerializeParams(chunk);

  // VoLum: append per-amp settings after params (see Unserialization.cpp)
  volum::PutCurrentVoLumChunkState(
    chunk, {mVolumAmpIdx, mVolumSpeakerIdx, mVolumChannelIdx}, mVolumAmpSettings.data(), volum::kAmpCount);
  volum::PutPrePostLockFlags(chunk, mVolumPreLocked, mVolumPostLocked);
  // Live PRE/POST lock snapshots: present iff the corresponding lock is on.
  // The detector in Unserialization.cpp uses the lock flags to compute the
  // expected tail size, so older chunks (no snapshots) remain readable.
  volum::PutPrePostLockSnapshots(chunk, mVolumPreLocked, mVolumPostLocked, mVolumLiveLockedPre, mVolumLiveLockedPost);

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

  if (mModel != nullptr)
  {
    _UpdateControlsFromModel();
  }
  _UpdateLatency();
}

void NeuralAmpModeler::OnUIClose()
{
  // Save while params are still valid (destructor may run after teardown)
  _VolumSaveCurrentToSettings();
#ifdef APP_API
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
      _SetSupportOutputGain();
      break;
    case kSupportOutputLevel: _SetSupportOutputGain(); break;
    // Tone stack:
    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
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
          mSupportPolarityInvert.store(true);
          mVolumAmpSettings[mVolumAmpIdx].supportPolarityInvert = true;
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
    case kDelayTone:
    case kDelayAge:
    case kDelayPingPong:
      if (mVolumInitComplete && !mVolumPostRestoreInProgress)
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
      // Skip while restoring a snapshot: the setParam cascade would otherwise re-save
      // the partially-restored knob values back into the very snapshot we are loading.
      if (mVolumInitComplete && !mVolumReverbRestoreInProgress && !mVolumPostRestoreInProgress)
      {
        mVolumEffectSettings.reverbActive = GetParam(kReverbActive)->Bool();
        mVolumEffectSettings.reverbMode = GetParam(kReverbMode)->Int();
        _VolumSaveReverbModeSnapshot(std::clamp(GetParam(kReverbMode)->Int(), 0, volum::kVoLumReverbModeCount - 1));
      }
      break;
    case kReverbSubMode:
      // Do NOT write the new sub-mode to mVolumEffectSettings here. OnParamChangeUI runs after
      // this and needs the previously selected sub-mode (held in settings) to know which slot
      // to snapshot the current knob values into before swapping in the new slot's values.
      // Mirrors the kDelayMode / kReverbMode pattern, which are handled UI-only for the same
      // reason. Without this guard, switching sub-modes would overwrite the destination slot
      // with the source slot's knobs, defeating per-sub-mode persistence.
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
    default: break;
  }

  if (mVolumInitComplete)
    mVolumSettingsDirty = true;
}

namespace
{
bool IsPreBlockParam(int paramIdx)
{
  switch (paramIdx)
  {
    case kPreCompActive:
    case kPreCompAmount:
    case kPreCompRatio:
    case kPreCompAttack:
    case kPreCompRelease:
    case kPreCompMix:
    case kPreCompLevel:
    case kPreNam1Active:
    case kPreNam1Capture:
    case kPreNam1Gain:
    case kPreNam1Bass:
    case kPreNam1Mid:
    case kPreNam1MidFreq:
    case kPreNam1Treble:
    case kPreNam1Level:
    case kPreNam2Active:
    case kPreNam2Capture:
    case kPreNam2Gain:
    case kPreNam2Bass:
    case kPreNam2Mid:
    case kPreNam2MidFreq:
    case kPreNam2Treble:
    case kPreNam2Level:
      return true;
    default:
      return false;
  }
}

bool IsPostBlockParam(int paramIdx)
{
  switch (paramIdx)
  {
    case kDelayActive:
    case kDelayTime:
    case kDelayFeedback:
    case kDelayMix:
    case kDelayMode:
    case kDelayTone:
    case kDelayAge:
    case kDelayPingPong:
    case kReverbActive:
    case kReverbMix:
    case kReverbDecay:
    case kReverbTone:
    case kReverbPreDelay:
    case kReverbShimmer:
    case kReverbMode:
    case kReverbSubMode:
      return true;
    default:
      return false;
  }
}
} // namespace

void NeuralAmpModeler::_VolumRefreshPrePostLockChrome(int paramIdx)
{
  if (!mVolumInitComplete)
    return;

  const bool affectsPre = mVolumPreLocked && IsPreBlockParam(paramIdx);
  const bool affectsPost = mVolumPostLocked && IsPostBlockParam(paramIdx);
  if (!affectsPre && !affectsPost)
    return;

  if (affectsPre)
    mVolumPreLockUiDirty = _VolumIsPreDirty();
  if (affectsPost)
    mVolumPostLockUiDirty = _VolumIsPostDirty();

  if (auto* pGfx = GetUI())
    if (auto* tripCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumTriptych))
      tripCtrl->SetDirty(false);
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
        _UpdateVoLumLayout(pGraphics);
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
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneBass)) c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneMid)) c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneTreble)) c->SetDisabled(!active);
        break;
      case kDelayMode:
      {
        if (mVolumPostRestoreInProgress)
          break;
        const int oldMode = std::clamp(mVolumEffectSettings.delayMode, 0, volum::kVoLumDelayModeCount - 1);
        _VolumSaveDelayModeSnapshot(oldMode);
        const int newMode = std::clamp(GetParam(kDelayMode)->Int(), 0, volum::kVoLumDelayModeCount - 1);
        mVolumEffectSettings.delayMode = newMode;
        _VolumRestoreDelayModeSnapshot(newMode);
        _UpdateVoLumLayout(pGraphics);
        break;
      }
      case kReverbMode:
      {
        if (mVolumPostRestoreInProgress)
          break;
        const int oldMode = std::clamp(mVolumEffectSettings.reverbMode, 0, volum::kVoLumReverbModeCount - 1);
        _VolumSaveReverbModeSnapshot(oldMode);
        const int newMode = std::clamp(GetParam(kReverbMode)->Int(), 0, volum::kVoLumReverbModeCount - 1);
        mVolumEffectSettings.reverbMode = newMode;
        _VolumRestoreReverbModeSnapshot(newMode);
        _UpdateVoLumLayout(pGraphics);
        break;
      }
      case kReverbSubMode:
      {
        // Skip while a reverb mode / sub-mode restoration is in flight: the cascading
        // setParam handlers would otherwise overwrite the freshly-loaded sub-mode
        // snapshot with whatever knob values happened to be on screen mid-restore.
        // Also skip when the current reverb mode is not Oktaverb, since the sub-mode
        // pill is irrelevant outside Oktaverb and any apparent change there is just
        // the cascade from a Hall / Plate restoration.
        if (mVolumReverbRestoreInProgress || mVolumPostRestoreInProgress)
          break;
        if (GetParam(kReverbMode)->Int() != volum::kVoLumReverbModeOktaverb)
          break;
        const int oldSubMode = std::clamp(
          mVolumEffectSettings.reverbModes[volum::kVoLumReverbModeOktaverb].subMode, 0, 2);
        const int newSubMode = std::clamp(GetParam(kReverbSubMode)->Int(), 0, 2);
        // No-op if the user re-clicked the same sub-mode pill: avoids unnecessary
        // snapshot churn that has no observable effect anyway.
        if (newSubMode == oldSubMode)
          break;
        _VolumSaveOktaverbSubModeSnapshot(oldSubMode);
        mVolumEffectSettings.reverbModes[volum::kVoLumReverbModeOktaverb].subMode = newSubMode;
        _VolumRestoreOktaverbSubModeSnapshot(newSubMode);
        _UpdateVoLumLayout(pGraphics);
        break;
      }
      default: break;
    }

    _VolumRefreshPrePostLockChrome(paramIdx);
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
  _VolumDrainLoaderResults();

  bool removedMainModel = false;
  bool removedSupportModel = false;
  bool removedPreModel[2] = {false, false};
  bool appliedMainModel = false;
  bool appliedSupportModel = false;
  bool appliedPreModel[2] = {false, false};

  {
    std::lock_guard<std::mutex> lock(mStagingMutex);

    if (mShouldRemoveModel)
    {
      mModel = nullptr;
      mStagedModel = nullptr;
      volum::dsp_staging::ClearLiveAndStagedPath(mNAMPaths);
      mShouldRemoveModel = false;
      mModelCleared = true;
      removedMainModel = true;
    }
    if (mShouldRemoveSupportModel)
    {
      mSupportModel = nullptr;
      mShouldRemoveSupportModel = false;
      removedSupportModel = true;
    }
    if (mShouldRemoveIR)
    {
      mIR = nullptr;
      mStagedIR = nullptr;
      volum::dsp_staging::ClearLiveAndStagedPath(mIRPaths);
      mShouldRemoveIR = false;
    }
    for (int i = 0; i < 2; ++i)
    {
      if (mShouldRemovePreModel[i])
      {
        mPreModel[i] = nullptr;
        mShouldRemovePreModel[i] = false;
        removedPreModel[i] = true;
      }
    }

    if (mStagedModel != nullptr)
    {
      mModel = std::move(mStagedModel);
      mStagedModel = nullptr;
      volum::dsp_staging::CommitStagedPathOnApply(mNAMPaths);
      mNewModelLoadedInDSP = true;
      appliedMainModel = true;
    }
    if (mStagedSupportModel != nullptr)
    {
      mSupportModel = std::move(mStagedSupportModel);
      mStagedSupportModel = nullptr;
      appliedSupportModel = true;
    }
    for (int i = 0; i < 2; ++i)
    {
      if (mStagedPreModel[i] != nullptr)
      {
        mPreModel[i] = std::move(mStagedPreModel[i]);
        mStagedPreModel[i] = nullptr;
        appliedPreModel[i] = true;
      }
    }
    if (mStagedIR != nullptr)
    {
      mIR = std::move(mStagedIR);
      mStagedIR = nullptr;
      volum::dsp_staging::CommitStagedPathOnApply(mIRPaths);
    }
  }

  if (removedMainModel || appliedMainModel)
  {
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  if (removedSupportModel || appliedSupportModel)
  {
    _UpdateLatency();
    _SetSupportOutputGain();
  }
  for (int i = 0; i < 2; ++i)
  {
    if (removedPreModel[i] || appliedPreModel[i])
      _UpdateLatency();
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
  // Inspecting staged pointers needs the staging mutex because _StageModel /
  // _StageIR can write them from a non-audio thread (UnserializeState path).
  std::lock_guard<std::mutex> lock(mStagingMutex);

  // Model
  if (mStagedModel != nullptr)
  {
    mStagedModel->Reset(sampleRate, maxBlockSize);
  }
  else if (mModel != nullptr)
  {
    mModel->Reset(sampleRate, maxBlockSize);
  }

  if (mStagedSupportModel != nullptr)
  {
    mStagedSupportModel->Reset(sampleRate, maxBlockSize);
  }
  else if (mSupportModel != nullptr)
  {
    mSupportModel->Reset(sampleRate, maxBlockSize);
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
  auto* outputParam = GetParam(kOutputLevel);
  double gainDB = outputParam->Value();
  if (volum::IsLevelMuteValue(gainDB, outputParam->GetMin()))
  {
    mOutputGain = 0.0;
    return;
  }
  if (mModel != nullptr)
  {
    volum::OutputModeModelInfo modelInfo;
    modelInfo.hasLoudness = mModel->HasLoudness();
    if (modelInfo.hasLoudness)
      modelInfo.loudness = mModel->GetLoudness();
    modelInfo.hasOutputLevel = mModel->HasOutputLevel();
    if (modelInfo.hasOutputLevel)
      modelInfo.outputLevel = mModel->GetOutputLevel();
    gainDB = volum::ComputeOutputModeGainDb(gainDB, GetParam(kOutputMode)->Int(), modelInfo,
                                            GetParam(kInputCalibrationLevel)->Value());
  }
  mOutputGain = DBToAmp(gainDB);
}

void NeuralAmpModeler::_SetSupportOutputGain()
{
  auto* outputParam = GetParam(kSupportOutputLevel);
  double gainDB = outputParam->Value();
  if (volum::IsLevelMuteValue(gainDB, outputParam->GetMin()))
  {
    mSupportOutputGain = 0.0;
    return;
  }
  if (mSupportModel != nullptr)
  {
    volum::OutputModeModelInfo modelInfo;
    modelInfo.hasLoudness = mSupportModel->HasLoudness();
    if (modelInfo.hasLoudness)
      modelInfo.loudness = mSupportModel->GetLoudness();
    modelInfo.hasOutputLevel = mSupportModel->HasOutputLevel();
    if (modelInfo.hasOutputLevel)
      modelInfo.outputLevel = mSupportModel->GetOutputLevel();
    gainDB = volum::ComputeOutputModeGainDb(gainDB, GetParam(kOutputMode)->Int(), modelInfo,
                                            GetParam(kInputCalibrationLevel)->Value());
  }
  mSupportOutputGain = DBToAmp(gainDB);
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath)
{
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath);
    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), GetBlockSize());
    {
      // Serialize the staging assignment against the audio thread's read/move in
      // _ApplyDSPStaging. _StageModel is called from the host's UnserializeState
      // path and (in non-VoLum builds) from the file-browser completion handler,
      // both off the audio thread. mNAMPaths.live commits in _ApplyDSPStaging.
      std::lock_guard<std::mutex> lock(mStagingMutex);
      mStagedModel = std::move(temp);
      volum::dsp_staging::StagePathOnSuccess(mNAMPaths, modelPath);
    }
  }
  catch (std::runtime_error& e)
  {
    {
      std::lock_guard<std::mutex> lock(mStagingMutex);
      mStagedModel = nullptr;
      volum::dsp_staging::ClearStagedPath(mNAMPaths);
    }
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

namespace {
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

template <size_t N>
int RememberedOrFirst(const std::array<int, N>& params, int remembered)
{
  return volum::keyboard::Contains(params, remembered) ? remembered : params.front();
}

}

std::string NeuralAmpModeler::_GetVoLumKnobHintText(int paramIdx) const
{
  const IParam* pParam = GetParam(paramIdx);
  if (!pParam)
    return {};

  WDL_String line;
  line.SetFormatted(512, "%s  |  Up/Down adjust  |  Left/Right knob  |  Tab target  |  Enter exact  |  Del reset  |  Esc clear",
                    pParam->GetName());
  return line.Get();
}

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

  auto wrap = [](int value, int count) {
    return (value + count) % count;
  };

  switch (mVolumExpandedSection)
  {
    case EVoLumSection::PRE:
    {
      constexpr EVoLumEffectFocus targets[3] = {
        EVoLumEffectFocus::COMP, EVoLumEffectFocus::PRE_NAM1, EVoLumEffectFocus::PRE_NAM2,
      };
      int current = 0;
      for (int i = 0; i < 3; ++i)
        if (targets[i] == mVolumFocusedEffect)
          current = i;
      mVolumFocusedEffect = targets[wrap(current + direction, 3)];
      mVolumDualAmpFocusedSupport = false;
      break;
    }
    case EVoLumSection::AMP:
    {
      mVolumFocusedEffect = EVoLumEffectFocus::AMP;
      mVolumDualAmpFocusedSupport = GetParam(kDualAmpActive)->Bool()
        ? !mVolumDualAmpFocusedSupport
        : false;
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
    case EVoLumEffectFocus::COMP: paramIdx = kPreCompActive; break;
    case EVoLumEffectFocus::PRE_NAM1: paramIdx = kPreNam1Active; break;
    case EVoLumEffectFocus::PRE_NAM2: paramIdx = kPreNam2Active; break;
    case EVoLumEffectFocus::DELAY: paramIdx = kDelayActive; break;
    case EVoLumEffectFocus::REVERB: paramIdx = kReverbActive; break;
  }

  if (paramIdx == kNoParameter)
    return false;

  const bool next = !GetParam(paramIdx)->Bool();
  GetParam(paramIdx)->Set(next ? 1.0 : 0.0);
  SendParameterValueFromDelegate(paramIdx, GetParam(paramIdx)->GetNormalized(), true);
  OnParamChange(paramIdx);

  if (paramIdx == kDualAmpActive)
    mVolumDualAmpFocusedSupport = next;

  _UpdateVoLumLayout();
  _UpdateVoLumKeyboardFocusHint();
  return true;
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
    case EVoLumEffectFocus::COMP: target = "Compressor"; action = "Space on/off"; nav = "Left/Right or Tab target"; break;
    case EVoLumEffectFocus::PRE_NAM1: target = "NAM 1"; action = "Space on/off"; nav = "Left/Right or Tab target"; break;
    case EVoLumEffectFocus::PRE_NAM2: target = "NAM 2"; action = "Space on/off"; nav = "Left/Right or Tab target"; break;
    case EVoLumEffectFocus::DELAY: target = "Delay"; action = "Space on/off"; nav = "Left/Right or Tab target"; break;
    case EVoLumEffectFocus::REVERB: target = "Reverb"; action = "Space on/off"; nav = "Left/Right or Tab target"; break;
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
    case EVoLumEffectFocus::COMP: return kPreCompAmount;
    case EVoLumEffectFocus::PRE_NAM1: return kPreNam1Gain;
    case EVoLumEffectFocus::PRE_NAM2: return kPreNam2Gain;
    case EVoLumEffectFocus::DELAY: return kDelayTime;
    case EVoLumEffectFocus::REVERB: return kReverbMix;
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
        : (GetParam(kDualAmpActive)->Bool()
          ? RememberedOrFirst(kMainAmpDualParams, remembered)
          : RememberedOrFirst(kMainAmpMonoParams, remembered));
    case EVoLumEffectFocus::COMP: return RememberedOrFirst(kCompParams, remembered);
    case EVoLumEffectFocus::PRE_NAM1: return RememberedOrFirst(kPreNam1Params, remembered);
    case EVoLumEffectFocus::PRE_NAM2: return RememberedOrFirst(kPreNam2Params, remembered);
    case EVoLumEffectFocus::DELAY: return RememberedOrFirst(kDelayParams, remembered);
    case EVoLumEffectFocus::REVERB:
      return GetParam(kReverbMode)->Int() == volum::kVoLumReverbModeOktaverb
        ? RememberedOrFirst(kOktaverbParams, remembered)
        : RememberedOrFirst(kReverbParams, remembered);
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
    case EVoLumEffectFocus::DELAY:
      return SelectAdjacentFromList(this, kDelayParams, currentParamIdx, direction);
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
    case EVoLumEffectFocus::COMP:
      return SelectAdjacentFromList(this, kCompParams, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM1:
      return SelectAdjacentFromList(this, kPreNam1Params, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM2:
      return SelectAdjacentFromList(this, kPreNam2Params, currentParamIdx, direction);
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
    _HideControlGroup(pGfx, "REVERB_PREDELAY", true);
    _HideControlGroup(pGfx, "REVERB_SUBTOGGLE", true);
    _HideControlGroup(pGfx, "REVERB_POWER", true);
    _HideControlGroup(pGfx, "DELAY_KNOBS", true);
    _HideControlGroup(pGfx, "DELAY_PINGPONG", true);
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
        (void) isHall;
        (void) isPlate;
        disableGroup("REVERB_KNOBS", !GetParam(kReverbActive)->Bool());
        disableGroup("REVERB_PREDELAY", !GetParam(kReverbActive)->Bool());
        disableGroup("REVERB_SHIMMER", !GetParam(kReverbActive)->Bool());
        disableGroup("REVERB_SUBTOGGLE", !GetParam(kReverbActive)->Bool());
        break;
      }
      case EVoLumEffectFocus::DELAY:
      {
        const int delayMode = GetParam(kDelayMode)->Int();
        const bool isReverse = delayMode == volum::kVoLumDelayModeReverse;
        _HideControlGroup(pGfx, "DELAY_POWER", false);
        _HideControlGroup(pGfx, "DELAY_KNOBS", false);
        // Ping-pong has no meaning for reversed taps; hide that control row when Reverse.
        _HideControlGroup(pGfx, "DELAY_PINGPONG", isReverse);
        // The shared kDelayAge slot does meaningfully different things per mode. Swap the
        // visible label and the knob/value tooltip so the user can read what the knob does
        // without having to consult the design guide.
        const char* ageLabel = "AGE";
        const char* ageTip =
          "Adds character to the delay tail (effect varies by mode).";
        switch (delayMode)
        {
          case volum::kVoLumDelayModeDigital:
            ageLabel = "GRIT";
            ageTip = "Digital mode: adds bit-crush quantisation and a tape-machine noise "
                     "floor on top of the repeats. At 0 the wet signal is bit-perfect.";
            break;
          case volum::kVoLumDelayModeAnalog:
            ageLabel = "WEAR";
            ageTip = "Analog mode: increases BBD chorus depth, HF darkness and compander "
                     "softness. 0.5 is classic Memory Man, 1.0 is heavy chorused wear.";
            break;
          case volum::kVoLumDelayModeReverse:
            ageLabel = "BLOOM";
            ageTip = "Reverse mode: softens the old edge-faded reverse slice toward a "
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
        disableGroup("DELAY_KNOBS", !GetParam(kDelayActive)->Bool());
        disableGroup("DELAY_PINGPONG", !GetParam(kDelayActive)->Bool());
        break;
      }
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

  // F8: CUSTOM group lists imported pedals; a single "Manage custom pedals..."
  // row opens the shared Manage panel (CRUD + import via file dialog).
  // captureIdx -1 keeps these rows from ever matching a real selected capture
  // (which is always >= 0), so they never render with a false selection dot.
  items.push_back({"CUSTOM", -1, true, volum::PrePedalCaptureGroup::None});
  for (const auto& name : volum::custom::MockCustomPedals())
    items.push_back({name, -1, false, volum::PrePedalCaptureGroup::None, PreMenuAction::None, true});
  items.push_back({"Manage custom pedals...", -1, false, volum::PrePedalCaptureGroup::None, PreMenuAction::Manage, false});

  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  const int selected = std::clamp(GetParam(captureParam)->Int(), 0, captureCount - 1);
  const float menuW = std::max(anchorRect.W() * 0.88f, 180.f);
  const float menuH = VoLumPreCaptureMenuControl::MenuHeight(items);
  // Clamp vertically so the (now taller, with CUSTOM) menu never spills past the
  // window bottom; shift it up when it would, keeping it on-screen.
  const IRECT uiBounds = GetUI()->GetBounds();
  float menuT = anchorRect.B + 6.f;
  if (menuT + menuH > uiBounds.B - 6.f)
    menuT = std::max(uiBounds.T + 6.f, uiBounds.B - 6.f - menuH);
  const IRECT menuRect(anchorRect.L, menuT, anchorRect.L + menuW, menuT + menuH);

  menu->SetTargetAndDrawRECTs(menuRect);
  menu->SetItems(slot, items, selected);
  menu->Hide(false);
}

void NeuralAmpModeler::_VolumShowManageCustomPedals()
{
  if (auto* pGfx = GetUI())
    if (auto* ov = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
      ov->As<VoLumCustomOverlayControl>()->ShowManage(VoLumCustomOverlayControl::ManageKind::Pedals);
}

void NeuralAmpModeler::_VolumSelectCustomAmp(int customIdx)
{
  // Mock selection: custom amps are display-only in the UI shell, so this just
  // drives the hero art, AMP name, header preset strip, and the sidebar's custom
  // highlight (which also scrolls the row into view). No DSP/settings change.
  const auto& names = volum::custom::MockCustomAmps();
  if (customIdx < 0 || customIdx >= (int)names.size())
    return;
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  if (auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
    heroCtrl->As<VoLumHeroImageControl>()->SetName(names[(size_t)customIdx].c_str());
  if (auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
    if (mVolumExpandedSection == EVoLumSection::AMP)
      nameCtrl->As<VoLumSubRowTextControl>()->SetName(names[(size_t)customIdx].c_str(), true);
  if (auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
    pb->As<VoLumPresetBarControl>()->SetList({}); // custom-amp presets land with the backend
  if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
    al->As<VoLumAmpListControl>()->SetCustomSelected(customIdx);
}

void NeuralAmpModeler::_VolumShowPresetMenu()
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* raw = pGfx->GetControlWithTag(kCtrlTagVoLumPresetMenu);
  auto* bar = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar);
  if (!raw || !bar)
    return;
  if (!raw->IsHidden())
  {
    raw->Hide(true);
    return;
  }

  const auto presets = volum::custom::MockPresetsForAmp(mVolumAmpIdx);
  std::vector<VoLumListMenuControl::Row> rows;
  if (presets.empty())
    rows.push_back({"No presets yet", -99, false, true}); // dim hint
  for (int i = 0; i < (int)presets.size(); i++)
    rows.push_back({presets[(size_t)i], i, false, false});
  rows.push_back({"Manage presets...", VoLumListMenuControl::kManage, true, false});

  auto* menu = raw->As<VoLumListMenuControl>();
  const IRECT anchor = bar->GetRECT();
  const float w = std::max(anchor.W(), 220.f);
  const float h = VoLumListMenuControl::MenuHeight(rows.size());
  const auto bounds = pGfx->GetBounds();
  float l = anchor.L;
  if (l + w > bounds.R - 4.f)
    l = bounds.R - 4.f - w;
  menu->SetTargetAndDrawRECTs(IRECT(l, anchor.B + 4.f, l + w, anchor.B + 4.f + h));
  const int selected = bar->As<VoLumPresetBarControl>()->ActiveIndex();
  menu->SetRows(rows, selected);
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

// VoLum ProcessBlock helpers + async-loader + per-amp settings persistence.
// Tail-included for file-size hygiene; all are part of this TU.
#include "VoLumProcessBlock.inc.cpp"
#include "VoLumLoader.inc.cpp"

#include "VoLumSettings.inc.cpp"

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  std::unique_ptr<dsp::ImpulseResponse> stagedIR;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    stagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = stagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  {
    // Publish the staged IR (and its path) under the staging mutex so the audio thread
    // sees a fully-constructed object or none at all. mIRPaths.live commits in _ApplyDSPStaging.
    std::lock_guard<std::mutex> lock(mStagingMutex);
    if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
    {
      mStagedIR = std::move(stagedIR);
      volum::dsp_staging::StagePathOnSuccess(mIRPaths, irPath);
    }
    else
    {
      mStagedIR = nullptr;
      volum::dsp_staging::ClearStagedPath(mIRPaths);
    }
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
  mSupportToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
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
  const bool preNam1ShouldLoad =
    volum::ShouldLoadPrePedalCapture(GetParam(kPreNam1Active)->Bool(), GetParam(kPreNam1Capture)->Int());
  const bool preNam2ShouldLoad =
    volum::ShouldLoadPrePedalCapture(GetParam(kPreNam2Active)->Bool(), GetParam(kPreNam2Capture)->Int());
  if (preNam1ShouldLoad && mPreModel[0])
    preLatency += mPreModel[0]->GetLatency();
  if (preNam2ShouldLoad && mPreModel[1])
    preLatency += mPreModel[1]->GetLatency();

  int ampLatency = 0;
  if (mModel)
  {
    ampLatency = mModel->GetLatency();
  }
  if (GetParam(kDualAmpActive)->Bool() && mSupportModel)
  {
    ampLatency = std::max(ampLatency, mSupportModel->GetLatency());
  }
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

  // R (channel 1) goes to the second OUT meter only in dual-amp/stereo mode.
  if (nChansOut > 1 && GetParam(kDualAmpActive)->Bool())
  {
    sample* rightPtr = outputPointer[1];
    sample** rightBlock = &rightPtr;
    mOutputSenderR.ProcessBlock(rightBlock, (int)nFrames, kCtrlTagOutputMeterR, nChansHack);
  }
}

// Plugin-state unserialization (legacy + dual-amp + chunk-version helpers).
// Tail-included for file-size hygiene; part of this translation unit.
#include "Unserialization.cpp"
