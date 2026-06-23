#include <algorithm> // std::clamp, std::min
#include <cassert> // RT capacity invariants
#include <cmath> // pow
#include <cstdlib> // std::getenv (opt-in perf overlay)
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
#include "VoLumIrFileGuard.h"
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
#include "VoLumChunkIdTail.h"
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
  IColor(255, 17, 17, 24), // Background
  kGold, // Foreground
  kGold.WithOpacity(0.3f), // Pressed
  kGold.WithOpacity(0.25f), // Frame
  kGold.WithOpacity(0.5f), // Highlight (hover)
  DEFAULT_SHCOLOR, // Shadow
  kGold, // Extra 1
  COLOR_RED, // Extra 2 (clipping)
  kGold.WithContrast(0.1f), // Extra 3
};
const IColor kGoldBright(255, 252, 235, 218);
const IVStyle volumStyle = IVStyle{true,
                                   true,
                                   volumColorSpec,
                                   {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, kGoldBright},
                                   {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, kGoldBright},
                                   DEFAULT_HIDE_CURSOR,
                                   DEFAULT_DRAW_FRAME,
                                   false,
                                   DEFAULT_EMBOSS,
                                   0.2f,
                                   2.f,
                                   DEFAULT_SHADOW_OFFSET,
                                   DEFAULT_WIDGET_FRAC,
                                   DEFAULT_WIDGET_ANGLE};
// kBG is transparent so the IVKnobControl's square control-background isn't filled:
// the procedural dial (VoLumDialKnobControl) sits directly on the panel gradient
// instead of punching a lighter (17,17,24) square through it.
const IVStyle volumKnobStyle =
  volumStyle.WithShowLabel(false).WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(0.75f).WithColor(
    EVColor::kBG, COLOR_TRANSPARENT);
// Same vector-knob style, but with the X1/X3 colours retuned to teal so the rotating pointer
// dot on SUPPORT-lane knobs reads as "support" without changing any other knob geometry.
const IVStyle volumKnobStyleSupport =
  volumKnobStyle.WithColor(EVColor::kX1, kTeal).WithColor(EVColor::kX3, kTealBright);
// Pan knob style: transparent background and no frame so it sits flush on the hero art rather
// than punching a square dark patch through the lane fill.
const IVStyle volumPanKnobStyle = volumKnobStyle.WithColor(EVColor::kBG, COLOR_TRANSPARENT).WithDrawShadows(false);
const IVStyle volumPanKnobStyleSupport =
  volumKnobStyleSupport.WithColor(EVColor::kBG, COLOR_TRANSPARENT).WithDrawShadows(false);

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
    g.DrawCircle(accent.WithOpacity(mMouseIsOver ? 0.85f : 0.35f), cx, cy, widgetRadius - 0.5f, nullptr,
                 mMouseIsOver ? 1.75f : 1.f);
    DrawIndicatorTrack(g, angle, cx + 0.5f, cy, widgetRadius);

    float data[2][2];
    RadialPoints(angle, cx, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3.f);
    g.PathFill(IPattern::CreateRadialGradient(
                 data[1][0], data[1][1], 4.0f, {{accent, 0.f}, {accent, 0.8f}, {COLOR_TRANSPARENT, 1.0f}}),
               {}, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.4f), data[1][0], data[1][1], 3.f, &mBlend);

    if (IsSelectedForKeyboard())
      g.DrawCircle(accent.WithOpacity(0.8f), cx, cy, widgetRadius + 5.f, nullptr, 1.5f);
  }
};
// Layered "modern amp-sim" dial: drop shadow, metallic top-lit body, a hugging
// value-arc ring with an active-lane glow, tick nubs, and an accent pointer.
// Drop-in for NAMKnobControl - same interaction, keyboard nudge, exact entry and
// selection ring; only the widget rendering changes. Lane identity rides on the
// style's kX1/kX3 colours (gold = MAIN, teal = SUPPORT).
class VoLumDialKnobControl : public NAMKnobControl
{
public:
  VoLumDialKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : NAMKnobControl(bounds, paramIdx, label, style, bitmap)
  {
  }

  void DrawWidget(IGraphics& g) override
  {
    const auto knobRect = mWidgetBounds.GetCentredInside(mWidgetBounds.W(), mWidgetBounds.W());
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float R = knobRect.W() * 0.5f;
    const float bodyR = R - 5.f;
    const float arcR = R - 2.f;
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    const bool active = mMouseIsOver || IsSelectedForKeyboard();
    const IColor accent = GetColor(kX1);
    const IColor accentHi = GetColor(kX3);

    // (1) Tick nubs around the travel range (longer/brighter at the cardinal marks).
    const int kTicks = 11;
    for (int i = 0; i < kTicks; ++i)
    {
      const float t = mAngle1 + (mAngle2 - mAngle1) * (static_cast<float>(i) / (kTicks - 1));
      const bool major = (i % 5 == 0);
      const bool passed = t <= angle + 0.5f;
      float pts[2][2];
      RadialPoints(t, cx, cy, R - (major ? 1.5f : 0.5f), R + (major ? 2.0f : 1.0f), 2, pts);
      g.DrawLine(passed ? accent.WithOpacity(0.9f) : IColor(70, 200, 162, 78), pts[0][0], pts[0][1], pts[1][0],
                 pts[1][1], &mBlend, major ? 1.4f : 1.0f);
    }

    // (2) Value-arc track + filled arc (soft glow behind it when the dial is active).
    g.DrawArc(IColor(46, 200, 162, 78), cx, cy, arcR, mAngle1, mAngle2, &mBlend, 2.0f);
    if (active)
      g.DrawArc(accentHi.WithOpacity(0.28f), cx, cy, arcR, mAngle1, angle, &mBlend, 6.0f);
    g.DrawArc(active ? accentHi : accent, cx, cy, arcR, mAngle1, angle, &mBlend, 2.6f);

    // (3) Metallic body cap: drop shadow, top-lit radial gradient, accent rim + inner sheen.
    // (The control's square kBG is transparent so this cap sits directly on the panel.)
    g.FillCircle(IColor(150, 0, 0, 0), cx, cy + 1.5f, bodyR);
    g.PathCircle(cx, cy, bodyR);
    g.PathFill(IPattern::CreateRadialGradient(
      cx, cy - bodyR * 0.45f, bodyR * 1.55f,
      {{IColor(255, 60, 60, 73), 0.f}, {IColor(255, 31, 31, 41), 0.55f}, {IColor(255, 15, 15, 21), 1.f}}));
    g.DrawCircle(accent.WithOpacity(active ? 0.7f : 0.4f), cx, cy, bodyR, &mBlend, 1.2f);
    g.DrawCircle(IColor(34, 255, 255, 255), cx, cy, bodyR - 1.5f, &mBlend, 1.f);

    // (4) Pointer: accent line from the cap centre out to a bright dot.
    float pdot[2][2];
    RadialPoints(angle, cx, cy, bodyR * 0.26f, bodyR * 0.86f, 2, pdot);
    g.DrawLine(active ? accentHi : accent, pdot[0][0], pdot[0][1], pdot[1][0], pdot[1][1], &mBlend, 2.4f);
    g.FillCircle(active ? accentHi : accent, pdot[1][0], pdot[1][1], 2.6f);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.4f), pdot[1][0], pdot[1][1], 2.6f, &mBlend);

    // (5) Keyboard selection ring.
    if (IsSelectedForKeyboard())
      g.DrawCircle(accent.WithOpacity(0.85f), cx, cy, R + 3.f, nullptr, 1.5f);
  }
};

// kBG transparent (same reason as volumKnobStyle): the slide-switch pill sits on the
// panel gradient instead of on a lighter (17,17,24) control-background square.
const IVStyle volumToggleStyle =
  volumStyle.WithShowLabel(false).WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(1.0f).WithColor(
    EVColor::kBG, COLOR_TRANSPARENT);
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

// Support-amp dropdown row codes: factory amps use their 0-based index; custom
// amps are offset by this base so the select callback can tell them apart.
constexpr int kVolumCustomSupportBase = 10000;


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
  GetParam(kOutputMode)
    ->InitEnum("OutputMode", volum::kOutputModeDefault,
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
  GetParam(kDelayMode)->InitEnum("DelayMode", volum::kVoLumDelayModeDigital, {"Digital", "Analog", "Reverse"});
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
  GetParam(kReverbMode)->InitEnum("ReverbMode", volum::kVoLumReverbModeHall, {"Hall", "Plate", "Oktaverb"});
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
#ifdef APP_API
  GetParam(kSupportIRToggle)->InitBool("SupportIRToggle", false);
#else
  GetParam(kSupportIRToggle)->InitBool("SupportIRToggle", true);
#endif
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);
  mSupportNoiseGateTrigger.AddListener(&mSupportNoiseGateGain);

  {
    auto root = volum::FindRigsRootDirectory();
    if (!root.empty())
      mVolumRigsRoot = root.string();
    // 1.2.0: bind + load the all-format custom-content library (F5-F8). The base
    // dir is VoLum-owned and writable from standalone/VST3/AU; fall back to a
    // content/ folder beside the rigs tree if the OS path cannot be resolved.
    {
      auto contentDir = volum::VolumContentDir();
      if (contentDir.empty() && !mVolumRigsRoot.empty())
        contentDir = std::filesystem::path(mVolumRigsRoot) / "content";
      if (!contentDir.empty())
      {
        volum::content::GlobalContentStore().SetBaseDir(contentDir);
        volum::content::GlobalContentStore().Load();
      }
    }
    // F5: let the preset bridge capture/apply real live settings and act on the
    // focused amp's bank.
    _VolumInstallPresetHooks();
    _VolumSyncPresetOwner();
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
    const float sidebarW = 200.f;
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
            _VolumClearIR(supportFocus);
          if (customLane >= 0)
          {
            const int slot = (speakerIdx == 0) ? volum::custom::kDirectSlot : (speakerIdx - 1);
            // Switching cab retargets the captured .nam for this lane. Preserve
            // the current gain stage when the new slot also covers it (item:
            // cab/IR switch must not snap the channel back to 1); only fall back
            // to the slot's first channel when the current stage is unavailable.
            const auto amp = volum::custom::CustomAmpAt(customLane);
            const auto chs = volum::custom::AmpSlotChannels(amp, slot);
            const int curCh = supportFocus ? mVolumCustomSupportChannel : mVolumCustomMainChannel;
            const int ch = chs.empty() ? 1 : volum::custom::SnapChannel(chs, curCh);
            const int chanPos = volum::custom::ChannelStepIndex(chs, ch);
            _VolumSetCustomChannelStepper(customLane, slot, supportFocus, chanPos);
            if (supportFocus)
            {
              mVolumCustomSupportSlot = slot;
              mVolumCustomSupportChannel = ch;
              // Persist the custom SUPPORT cab/channel onto the active scene so it
              // survives preset save + session/DAW recall (mirrors the MAIN branch
              // below, which writes mVolumSpeakerIdx/mVolumChannelIdx).
              _VolumActiveScene().supportCustomSlot = slot;
              _VolumActiveScene().supportCustomChannel = ch;
              mVolumSettingsDirty = true;
              mVolumSupportNeedsLoad.store(true);
            }
            else
            {
              mVolumCustomMainSlot = slot;
              mVolumCustomMainChannel = ch;
              // Persist the pick into the focused custom scene (item: custom amps
              // must remember their last cab/channel). speakerIdx is the UI cab
              // index; channelIdx is the stepper position.
              mVolumSpeakerIdx = speakerIdx;
              mVolumChannelIdx = chanPos;
              mVolumSettingsDirty = true;
              mVolumNeedsLoad.store(true);
            }
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
                           float centerOffset = 0.f, float centerColW = 80.f) {
      float customColW = center_offset ? centerColW : colW;
      float cx = center_offset ? (mainCX + centerOffset - (centerSlots * customColW) / 2.f
                                  + (slot - centerStart) * customColW + (customColW / 2.f))
                               : knobX(slot) + (colW / 2.f);
      float kL = cx - (knobDiam / 2.f);

      // Use a wider label rect (-40.f to +40.f = 80px wide) to prevent "FEEDBACK" clipping
      pGraphics->AttachControl(
        new VoLumKnobLabelControl(IRECT(cx - 40.f, knobRowTop, cx + 40.f, knobRowTop + 20.f), label), -1, group);
      auto* knob = new VoLumDialKnobControl(
        IRECT(kL, knobT, kL + knobDiam, knobT + knobDiam), paramId, "", volumKnobStyle, knobBackgroundBitmap);
      pGraphics->AttachControl(knob, -1, group);
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
          // When a custom MAIN amp is focused, the stepper lists that amp's slot
          // channels; map the selected row to a channel number and stage its .nam.
          const bool supportFocus = GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport;
          if (mVolumCustomMainIdx >= 0 && !supportFocus)
          {
            const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
            const auto chs = volum::custom::AmpSlotChannels(amp, mVolumCustomMainSlot);
            if (newIdx >= 0 && newIdx < (int)chs.size())
              mVolumCustomMainChannel = chs[(size_t)newIdx];
            // Persist the channel pick (stepper position) into the focused scene.
            mVolumChannelIdx = newIdx;
            mVolumSettingsDirty = true;
            mVolumNeedsLoad.store(true);
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
          // Custom SUPPORT partner: the stepper lists that amp's slot channels;
          // map the selected row to a channel number and stage its .nam. The
          // custom support loader resolves from mVolumCustomSupportChannel (not
          // kSupportChannelIdx), so this must be set or the model never switches.
          if (mVolumCustomSupportIdx >= 0)
          {
            const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
            const auto chs = volum::custom::AmpSlotChannels(amp, mVolumCustomSupportSlot);
            if (newIdx >= 0 && newIdx < (int)chs.size())
              mVolumCustomSupportChannel = chs[(size_t)newIdx];
            // Persist the stepped gain stage so it survives preset save + recall.
            _VolumActiveScene().supportCustomSlot = mVolumCustomSupportSlot;
            _VolumActiveScene().supportCustomChannel = mVolumCustomSupportChannel;
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

    float dlySwX = mainCX - 242.f;
    pGraphics->AttachControl(new VoLumPowerSwitchControl(
                               IRECT(dlySwX - 14.f, knobT - 4.f, dlySwX + 14.f, knobT + knobDiam + 2.f), kDelayActive),
                             -1, "DELAY_POWER");

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
              dlg->As<VoLumConfirmDialogControl>()->Show(
                "Are you sure?", "Overwrite preset \"" + name + "\" with the current settings?", doOverwrite,
                "Overwrite");
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
          [pPlugin](const volum::custom::CustomAmp& ampIn, int editIdx) {
            // editIdx >= 0 -> the user edited an existing amp: mutate that entry
            // in place so we don't spawn a duplicate. Otherwise append a new one.
            auto& store = volum::content::GlobalContentStore();
            volum::custom::CustomAmp amp = ampIn;
            // F6 import: copy any newly-added captures (those carrying an absolute
            // sourcePath) into the VoLum-owned amps/ dir and record the resolvable
            // storedPath. Already-stored files (edit re-save) keep their storedPath.
            std::string idPrefix = (editIdx >= 0) ? volum::custom::CustomAmpIdAt(editIdx) : std::string();
            if (idPrefix.empty())
            {
              amp.id = volum::content::MintId(store.reg(), "amp");
              idPrefix = amp.id;
            }
            for (size_t i = 0; i < amp.files.size(); ++i)
            {
              auto& f = amp.files[i];
              if (f.sourcePath.empty())
                continue;
              const std::string rel =
                store.ImportFileCopy(std::filesystem::path(f.sourcePath), "amps", idPrefix + "_" + std::to_string(i));
              if (!rel.empty())
                f.storedPath = rel;
              f.sourcePath.clear();
            }
            const int idx =
              (editIdx >= 0) ? volum::custom::UpdateCustomAmp(editIdx, amp) : volum::custom::AddCustomAmp(amp);
            auto* pGfx = pPlugin->GetUI();
            if (!pGfx)
              return;
            const auto& amps = volum::custom::MockCustomAmps();
            if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
              al->As<VoLumAmpListControl>()->SetCustomAmps(amps, volum::custom::MockCustomAmpArts());
            // Full refresh (hero art/name + cabinet row + sidebar highlight) so a
            // renamed amp / re-mapped cabs show immediately - and an edit lands on
            // the same entry instead of a stray duplicate.
            if (idx >= 0 && idx < (int)amps.size())
              pPlugin->_VolumSelectCustomAmp(idx);
          },
          // Manage panel mutated (preset/IR/pedal CRUD) -> recover the live cab if
          // the active custom IR was just deleted, then re-sync the header strip
          // for the currently focused amp (factory or custom).
          [pPlugin]() {
            pPlugin->_VolumReconcileActiveIr();
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

    // Apply loaded settings: select correct amp, speaker, hero image
    {
      auto* ampList = pGraphics->GetControlWithTag(kCtrlTagVoLumAmpList)->As<VoLumAmpListControl>();
      auto* spkRow = pGraphics->GetControlWithTag(kCtrlTagVoLumSpeakerRow)->As<VoLumSpeakerRowControl>();
      auto* heroCtrl = pGraphics->GetControlWithTag(kCtrlTagVoLumHeroImage)->As<VoLumHeroImageControl>();
      auto* nameCtrl = pGraphics->GetControlWithTag(kCtrlTagVoLumSubRowText)->As<VoLumSubRowTextControl>();

      if (ampList)
        ampList->SetSelected(mVolumAmpIdx);
      if (spkRow)
        spkRow->SetSelected(mVolumSpeakerIdx);
      if (nameCtrl && mVolumExpandedSection == EVoLumSection::AMP)
        nameCtrl->SetName(volum::kAmps[mVolumAmpIdx].displayName, true);
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
  const auto processingPlan = volum::MakeProcessingPlan(
    haveMainModel, noiseGateActive, toneStackActive, GetParam(kIRToggle)->Value(), mIR != nullptr,
    GetParam(kPreCompActive)->Bool(), preNamActive, havePreNam, GetParam(kDelayActive)->Bool(),
    GetParam(kReverbActive)->Bool(), mTunerDSP.IsActive(), dualAmpActive, haveSupportModel, supportToneStackActive,
    GetParam(kSupportIRToggle)->Value(), mSupportIR != nullptr);
  preAmpPointers = _VolumProcessPreChain(preAmpPointers, processingPlan, numChannelsInternal, nFrames, sampleRate);

  if (processingPlan.runDualAmp)
  {
    // Capacity invariant: OnReset() pre-allocates these scratch buffers to
    // maxBlockSize, so .resize() here must NEVER reallocate on the audio
    // thread. assert() is a no-op in NDEBUG release builds and fires in
    // debug + CI sanitizer builds if the invariant ever regresses.
    assert(mDualMainLaneBuffer.capacity() >= static_cast<size_t>(numFrames)
           && "Dual-amp main scratch not pre-reserved");
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
    const auto panGains = volum::MakeDualAmpPanGains(
      volum::DualAmpRoute::Custom, GetParam(kMainAmpPan)->Value(), GetParam(kSupportAmpPan)->Value());
    int mainLatency = 0;
    int supportLatency = 0;
    if (mModel)
      mainLatency = mModel->GetLatency();
    if (mSupportModel)
      supportLatency = mSupportModel->GetLatency();
    const auto latencyComp = volum::MakeDualAmpLatencyCompensation(mainLatency, supportLatency);
    assert(mDualMainAlignedBuffer.capacity() >= static_cast<size_t>(numFrames)
           && "Dual-amp main-aligned scratch not pre-reserved");
    assert(mDualSupportAlignedBuffer.capacity() >= static_cast<size_t>(numFrames)
           && "Dual-amp support-aligned scratch not pre-reserved");
    mDualMainAlignedBuffer.resize(numFrames);
    mDualSupportAlignedBuffer.resize(numFrames);
    const sample* mainLane = mDualMainLatencyDelay.Process(
      hpfPointers[0], mDualMainAlignedBuffer.data(), numFrames, latencyComp.mainDelaySamples);
    const sample* compensatedSupportLane = mDualSupportLatencyDelay.Process(
      supportLane, mDualSupportAlignedBuffer.data(), numFrames, latencyComp.supportDelaySamples);
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
    bool customMainLoad = false;
    if (mVolumCustomMainIdx >= 0)
    {
      // F6: a custom MAIN amp is focused - resolve the manifest .nam for the
      // selected (slot, channel) and stage it from the content library.
      const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
      const std::string rel = volum::content::CaptureFileFor(amp, mVolumCustomMainSlot, mVolumCustomMainChannel);
      if (!rel.empty())
      {
        fileToLoad = volum::content::GlobalContentStore().ResolveStored(rel).string();
        customMainLoad = true;
      }
    }
    else if (mVolumChannelIdx >= 0 && mVolumChannelIdx < (int)mVolumChannelFiles.size())
    {
      namespace fs = std::filesystem;
      auto rigPath =
        fs::path(mVolumRigsRoot) / volum::kAmps[mVolumAmpIdx].folderName / mVolumChannelFiles[mVolumChannelIdx];
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

      // Custom amps live outside the factory rig tree, so disable the factory
      // sibling-prefetch by passing ampIdx = -1 (the loader skips its scan).
      const int ampIdx = customMainLoad ? -1 : mVolumAmpIdx;
      const std::string rigsRoot = customMainLoad ? std::string() : mVolumRigsRoot;
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

  // VoLum 1.2.0 id tail: project references into the shared content library
  // (custom MAIN/SUPPORT binding, active preset, per-amp custom IR / support
  // ids). Sentinel-guarded + appended last so older builds ignore it (see
  // VoLumChunkIdTail.h).
  volum::ChunkIdTail idTail;
  idTail.customMainId = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
  idTail.customSupportId = volum::custom::CustomAmpIdAt(mVolumCustomSupportIdx);
  idTail.activePresetId = mVolumActivePresetId;
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    idTail.perAmpIrId[i] = mVolumAmpSettings[i].activeIrId;
    idTail.perAmpSupportIrId[i] = mVolumAmpSettings[i].supportActiveIrId;
    idTail.perAmpSupportId[i] = mVolumAmpSettings[i].supportCustomId;
    idTail.perAmpSupportSlot[i] = mVolumAmpSettings[i].supportCustomSlot;
    idTail.perAmpSupportChannel[i] = mVolumAmpSettings[i].supportCustomChannel;
  }
  volum::PutChunkIdTail(chunk, idTail);

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
  _VolumRestoreSessionSelection();
}

// Standalone: re-focus the last custom MAIN amp and re-select the active preset
// once the UI exists (custom-amp focus needs the speaker row / cab controls). The
// dirty baseline must come from the PRESET BANK content, not the restored live
// scene -- the live scene may carry unsaved edits that were flushed to settings
// on the previous close, which would otherwise read as falsely clean.
void NeuralAmpModeler::_VolumRestoreSessionSelection()
{
  if (mVolumDidRestorePresetSelection)
    return;
  mVolumDidRestorePresetSelection = true;

  if (!mVolumRestoreCustomMainId.empty())
  {
    const int cmi = volum::custom::CustomAmpIndexById(mVolumRestoreCustomMainId);
    if (cmi >= 0)
      _VolumSelectCustomAmp(cmi); // applies the custom scene + cabs; clears the recalled preset
  }
  mVolumRestoreCustomMainId.clear();

  if (mVolumRestorePresetId.empty())
    return;
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  const auto& banks = volum::content::GlobalContentStore().reg().presetBanks;
  auto it = banks.find(_VolumActiveOwnerKey());
  if (it != banks.end())
    for (const auto& pr : it->second)
      if (pr.id == mVolumRestorePresetId)
      {
        mVolumActivePresetId = pr.id;
        // Baseline = the preset's stored content, so a reopen with unsaved edits
        // correctly reads dirty (see _VolumRefreshPresetBar -> _VolumRecomputePresetDirty).
        mVolumRecalledSnapshot = pr.settings;
        mVolumHasRecalledSnapshot = true;
        _VolumRememberActivePreset();
        break;
      }
  mVolumRestorePresetId.clear();
  _VolumRefreshPresetBar();
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
        // Uncustomised-pan heuristic: when both lanes are still centered, split them
        // hard L/R. This must fire even when a support amp is ALREADY selected -
        // otherwise a centered MAIN + polarity-inverted SUPPORT phase-cancel to near
        // silence (the "no sound in dual mode" case). A user who has panned either
        // lane keeps their layout.
        if (nowOn && std::abs(GetParam(kMainAmpPan)->Value()) < 1e-3
            && std::abs(GetParam(kSupportAmpPan)->Value()) < 1e-3)
        {
          mSupportPolarityInvert.store(true);
          mVolumAmpSettings[mVolumAmpIdx].supportPolarityInvert = true;
          GetParam(kMainAmpPan)->Set(-1.0);
          SendParameterValueFromDelegate(kMainAmpPan, GetParam(kMainAmpPan)->GetNormalized(), true);
          GetParam(kSupportAmpPan)->Set(1.0);
          SendParameterValueFromDelegate(kSupportAmpPan, GetParam(kSupportAmpPan)->GetNormalized(), true);

          // Mirror MAIN's currently selected cab onto SUPPORT only when no support
          // partner is chosen yet (don't clobber an explicitly-picked support cab).
          if (GetParam(kSupportAmpIdx)->Int() < 0 && mVolumCustomSupportIdx < 0)
          {
            const int mainSpk = std::clamp(mVolumSpeakerIdx, 0, 3);
            GetParam(kSupportSpeakerIdx)->Set(mainSpk);
            SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
          }
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
    case kPreNam2Level: return true;
    default: return false;
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
    case kReverbSubMode: return true;
    default: return false;
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
    // A user-driven param edit may diverge the live chain from the recalled
    // preset snapshot -> re-evaluate the "(unsaved)" flag with an equality test
    // (so nudging a knob back to the preset value clears it again). Programmatic
    // restores (amp switch, preset recall) arrive via kDelegate/kReset, not kUI.
    if (source == EParamSource::kUI)
      _VolumRecomputePresetDirty();

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
      case kSupportChannelIdx: _UpdateVoLumLayout(pGraphics); break;
      case kMainAmpPan:
      case kSupportAmpPan: break;
      case kSupportNoiseGateActive:
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportNoiseGateThreshold))
          c->SetDisabled(!active);
        break;
      case kSupportEQActive:
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportToneBass))
          c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportToneMid))
          c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kSupportToneTreble))
          c->SetDisabled(!active);
        break;
      case kEQActive:
        pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneBass))
          c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneMid))
          c->SetDisabled(!active);
        if (auto* c = pGraphics->GetControlWithParamIdx(kToneTreble))
          c->SetDisabled(!active);
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
        const int oldSubMode =
          std::clamp(mVolumEffectSettings.reverbModes[volum::kVoLumReverbModeOktaverb].subMode, 0, 2);
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
    if (mShouldRemoveSupportIR)
    {
      mSupportIR = nullptr;
      mStagedSupportIR = nullptr;
      volum::dsp_staging::ClearLiveAndStagedPath(mSupportIRPaths);
      mShouldRemoveSupportIR = false;
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
    if (mStagedSupportIR != nullptr)
    {
      mSupportIR = std::move(mStagedSupportIR);
      mStagedSupportIR = nullptr;
      volum::dsp_staging::CommitStagedPathOnApply(mSupportIRPaths);
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
  (void)inputs;
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
    gainDB = volum::ComputeOutputModeGainDb(
      gainDB, GetParam(kOutputMode)->Int(), modelInfo, GetParam(kInputCalibrationLevel)->Value());
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
    gainDB = volum::ComputeOutputModeGainDb(
      gainDB, GetParam(kOutputMode)->Int(), modelInfo, GetParam(kInputCalibrationLevel)->Value());
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

namespace
{
template <size_t N>
bool SelectAdjacentFromList(NeuralAmpModeler* plugin, const std::array<int, N>& params, int currentParamIdx,
                            int direction)
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

} // namespace

std::string NeuralAmpModeler::_GetVoLumKnobHintText(int paramIdx) const
{
  const IParam* pParam = GetParam(paramIdx);
  if (!pParam)
    return {};

  WDL_String line;
  line.SetFormatted(
    512, "%s  |  Up/Down adjust  |  Left/Right knob  |  Tab target  |  Enter exact  |  Del reset  |  Esc clear",
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

  auto wrap = [](int value, int count) { return (value + count) % count; };

  switch (mVolumExpandedSection)
  {
    case EVoLumSection::PRE:
    {
      constexpr EVoLumEffectFocus targets[3] = {
        EVoLumEffectFocus::COMP,
        EVoLumEffectFocus::PRE_NAM1,
        EVoLumEffectFocus::PRE_NAM2,
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
    case EVoLumEffectFocus::COMP: paramIdx = kPreCompActive; break;
    case EVoLumEffectFocus::PRE_NAM1: paramIdx = kPreNam1Active; break;
    case EVoLumEffectFocus::PRE_NAM2: paramIdx = kPreNam2Active; break;
    case EVoLumEffectFocus::DELAY: paramIdx = kDelayActive; break;
    case EVoLumEffectFocus::REVERB: paramIdx = kReverbActive; break;
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
               : (GetParam(kDualAmpActive)->Bool() ? RememberedOrFirst(kMainAmpDualParams, remembered)
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
    case EVoLumEffectFocus::COMP: return SelectAdjacentFromList(this, kCompParams, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM1: return SelectAdjacentFromList(this, kPreNam1Params, currentParamIdx, direction);
    case EVoLumEffectFocus::PRE_NAM2: return SelectAdjacentFromList(this, kPreNam2Params, currentParamIdx, direction);
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
        (void)isHall;
        (void)isPlate;
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
      const bool preActive =
        GetParam(kPreCompActive)->Bool() || GetParam(kPreNam1Active)->Bool() || GetParam(kPreNam2Active)->Bool();
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
        if (auto* linkCard = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector))
          linkCard->SetTargetAndDrawRECTs(cards.connector.As<IRECT>());
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
      if (auto* chain = pGfx->GetControlWithTag(kCtrlTagVoLumChainConnector))
      {
        chain->Hide(!postExpanded);
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
        mMetronomeDSP.IsActive(), mMetronomeDSP.GetBPM(), mMetronomeDSP.GetVolume(), mMetronomeDSP.GetTimeSig());
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

  auto channels = volum::DiscoverChannels(std::filesystem::path(mVolumRigsRoot), volum::kAmps[mVolumAmpIdx].folderName,
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
      footer->As<VoLumFooterControl>()->SetText(mVolumLastLoadedFile.empty() ? "(no rig loaded)"
                                                                             : mVolumLastLoadedFile.c_str());
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
  // Custom imported pedals (F8) live at stable indices >= kCustomPedalIndexBase,
  // outside the contiguous factory label vector. Resolve their name via a stable
  // scratch buffer so we can hand back a const char*.
  if (captureIdx >= volum::content::kCustomPedalIndexBase)
  {
    mVolumPreCaptureLabelScratch = volum::custom::PedalNameByLegacy(captureIdx);
    return mVolumPreCaptureLabelScratch.empty() ? "Click to change" : mVolumPreCaptureLabelScratch.c_str();
  }
  if (captureIdx <= 0 || captureIdx > static_cast<int>(mVolumPreCaptureLabels.size()))
    return "Click to change";
  return mVolumPreCaptureLabels[static_cast<size_t>(captureIdx - 1)].c_str();
}

const char* NeuralAmpModeler::_VolumGetPreCaptureShortLabel(int captureIdx, const char* fallback) const
{
  if (captureIdx >= volum::content::kCustomPedalIndexBase)
  {
    // Custom pedal captures carry the user's full import name; the Amp-view quiet
    // slot pill is tiny, so cap it like the factory short labels (curated 3-5
    // chars) instead of letting a long name overflow into the next pill.
    const std::string full = volum::custom::PedalNameByLegacy(captureIdx);
    if (full.empty())
      return fallback;
    mVolumPreCaptureLabelScratch = volum::custom::ShortCaptureLabel(full);
    return mVolumPreCaptureLabelScratch.c_str();
  }
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

std::string NeuralAmpModeler::_VolumGetPreCaptureLoadPath(int captureIdx) const
{
  if (captureIdx >= volum::content::kCustomPedalIndexBase)
  {
    const std::string rel = volum::custom::PedalStoredPathByLegacy(captureIdx);
    if (rel.empty())
      return {};
    return volum::content::GlobalContentStore().ResolveStored(rel).string();
  }
  const std::string fn = _VolumGetPreCaptureFilename(captureIdx);
  if (fn.empty() || mVolumRigsRoot.empty())
    return {};
  return (std::filesystem::path(mVolumRigsRoot) / "PrePedals" / fn).string();
}

void NeuralAmpModeler::_VolumSetPreNamCapture(int slot, int captureIdx)
{
  if (slot < 0 || slot >= 2)
    return;

  const int paramIdx = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  // Custom imported pedals use stable indices up to the param max (127), so clamp
  // to the full param range rather than the factory count.
  const int next = std::clamp(captureIdx, 0, volum::kPreCaptureMaxParamIndex);
  if (GetParam(paramIdx)->Int() == next)
    return;

  GetParam(paramIdx)->Set(next);
  SendParameterValueFromDelegate(paramIdx, GetParam(paramIdx)->GetNormalized(), true);
  mVolumPreNeedsLoad[slot].store(true);
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
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
  {
    const auto& peds = volum::content::GlobalContentStore().reg().pedals;
    for (const auto& p : peds)
      items.push_back({p.name, p.legacyIndex, false, volum::PrePedalCaptureGroup::None, PreMenuAction::None, true});
  }
  items.push_back(
    {"Manage custom pedals...", -1, false, volum::PrePedalCaptureGroup::None, PreMenuAction::Manage, false});

  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  // Unclamped so a custom-pedal selection (index >= kCustomPedalIndexBase) marks
  // the right row instead of a factory row.
  const int selected = GetParam(captureParam)->Int();
  const float menuW = std::max(anchorRect.W() * 0.88f, 180.f);
  const float fullH = VoLumPreCaptureMenuControl::MenuHeight(items);
  // Cap the menu to the window: drop below the anchor when it fits, otherwise
  // grow upward, and finally cap to the usable window height so a long imported
  // pedal list never spills off-screen - the menu scrolls internally instead.
  const IRECT uiBounds = GetUI()->GetBounds();
  const float top = anchorRect.B + 6.f;
  const float spaceBelow = uiBounds.B - 6.f - top;
  const float maxOnScreen = uiBounds.H() - 12.f;
  float menuT, menuH;
  if (fullH <= spaceBelow)
  {
    menuT = top;
    menuH = fullH;
  }
  else
  {
    menuH = std::min(fullH, maxOnScreen);
    menuT = std::max(uiBounds.T + 6.f, uiBounds.B - 6.f - menuH);
  }
  const IRECT menuRect(anchorRect.L, menuT, anchorRect.L + menuW, menuT + menuH);

  menu->SetTargetAndDrawRECTs(menuRect);
  menu->SetItems(slot, items, selected);
  menu->Hide(false);
}

void NeuralAmpModeler::_VolumShowManageCustomPedals(int preSlot)
{
  if (auto* pGfx = GetUI())
    if (auto* ov = pGfx->GetControlWithTag(kCtrlTagVoLumCustomOverlay))
      ov->As<VoLumCustomOverlayControl>()->ShowManage(
        VoLumCustomOverlayControl::ManageKind::Pedals, 0, nullptr, preSlot);
}

void NeuralAmpModeler::_VolumMarkPresetDirty()
{
  // Rig edits that bypass the kUI param hook (cab/channel/IR/polarity changes set
  // members or send kDelegate) funnel here. The "(unsaved)" flag is an equality
  // test against the recalled snapshot, so an A/B edit back to the preset clears
  // it; with no recalled preset there is nothing to diverge from.
  _VolumRecomputePresetDirty();
}

void NeuralAmpModeler::_VolumSelectCustomAmp(int customIdx)
{
  const auto& names = volum::custom::MockCustomAmps();
  if (customIdx < 0 || customIdx >= (int)names.size())
    return;

  // Scene isolation (F6): snapshot the outgoing lane (factory slot or the prior
  // custom scene, via the redirect in _VolumSaveCurrentToSettings), then make the
  // new custom amp active and apply its own scene (defaults on first focus).
  if (mVolumInitComplete)
    _VolumSaveCurrentToSettings();
  mVolumCustomMainIdx = customIdx;
  // Point the preset bank at this custom amp's owner key and drop any recalled
  // preset carried over from the previous amp.
  _VolumSyncPresetOwner();
  const std::string ampId = volum::custom::CustomAmpIdAt(customIdx);
  if (!ampId.empty())
    _VolumApplyAmpSettings(volum::content::GlobalContentStore().reg().customScenes[ampId]);

  auto* pGfx = GetUI();
  if (!pGfx)
  {
    _VolumApplyCustomMainCabs(customIdx);
    return;
  }
  if (auto* heroCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumHeroImage))
  {
    auto* hero = heroCtrl->As<VoLumHeroImageControl>();
    hero->SetCustomArt(true, volum::custom::CustomAmpArt(customIdx));
    hero->SetName(names[(size_t)customIdx].c_str());
  }
  if (auto* nameCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSubRowText))
    if (mVolumExpandedSection == EVoLumSection::AMP)
      nameCtrl->As<VoLumSubRowTextControl>()->SetName(names[(size_t)customIdx].c_str(), true);
  _VolumRefreshPresetBar(); // this custom amp's preset bank
  if (auto* al = pGfx->GetControlWithTag(kCtrlTagVoLumAmpList))
    al->As<VoLumAmpListControl>()->SetCustomSelected(customIdx);
  // Make the shared cabinet row + channel stepper reflect this custom amp.
  _VolumApplyCustomMainCabs(customIdx);
}

void NeuralAmpModeler::_VolumApplyCustomMainCabs(int customIdx, bool supportLane)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl)
    return;
  auto* row = spkCtrl->As<VoLumSpeakerRowControl>();
  const auto amp = volum::custom::CustomAmpAt(customIdx);
  // Blank the label of any cab slot with no coverage so the row disables it.
  std::string cab[volum::custom::kNumCabSlots];
  for (int s = 0; s < volum::custom::kNumCabSlots; s++)
    cab[s] = volum::custom::AmpSlotChannels(amp, s).empty() ? std::string() : amp.cabNames[(size_t)s];
  row->SetCabNames(cab[0], cab[1], cab[2]);
  // A custom IR only works when this amp has a DIRECT capture to convolve; grey
  // out the IR cab button otherwise (item: IR selectable without a DIRECT nam).
  row->SetIrEnabled(volum::custom::HasDirectCapture(amp), "Custom IR needs a DIRECT capture");
  // Reflect this lane's own custom IR (per-lane IR), not a blanket clear.
  _VolumReflectLaneIrChip(supportLane);
  // Default to the first populated slot (DIRECT -> No Cab index 0; cab slot s -> s+1).
  const auto slots = volum::custom::AmpSlots(amp);
  int sel = 0, selSlot = volum::custom::kDirectSlot;
  if (!slots.empty())
  {
    selSlot = slots.front();
    sel = (selSlot == volum::custom::kDirectSlot) ? 0 : selSlot + 1;
  }
  // Restore the cab/channel the focused lane last used (item: custom amps must
  // remember their speaker, and refocusing a lane must not reset the other
  // lane's channel). MAIN sources the slot from mVolumSpeakerIdx (UI cab index);
  // SUPPORT sources it from mVolumCustomSupportSlot. Both derive the stepper
  // position from the resolved gain stage so the row stays aligned.
  int chanPos = 0;
  {
    const int curSlot = supportLane
                          ? mVolumCustomSupportSlot
                          : ((mVolumSpeakerIdx == 0) ? volum::custom::kDirectSlot : (mVolumSpeakerIdx - 1));
    const int curCh = supportLane ? mVolumCustomSupportChannel : mVolumCustomMainChannel;
    const auto candChans = volum::custom::AmpSlotChannels(amp, curSlot);
    if (!candChans.empty())
    {
      selSlot = curSlot;
      sel = (curSlot == volum::custom::kDirectSlot) ? 0 : curSlot + 1;
      chanPos = volum::custom::ChannelStepIndex(candChans, volum::custom::SnapChannel(candChans, curCh));
    }
  }
  row->SetSelected(sel);
  _VolumSetCustomChannelStepper(customIdx, selSlot, supportLane, chanPos);

  // Record the resolved (slot, channel) for the focused lane and stage its .nam.
  const auto selChans = volum::custom::AmpSlotChannels(amp, selSlot);
  const int selChan = selChans.empty() ? 1 : selChans[(size_t)std::clamp(chanPos, 0, (int)selChans.size() - 1)];
  if (supportLane)
  {
    mVolumCustomSupportSlot = selSlot;
    mVolumCustomSupportChannel = selChan;
    mVolumSupportNeedsLoad.store(true);
  }
  else
  {
    mVolumCustomMainSlot = selSlot;
    mVolumCustomMainChannel = selChan;
    mVolumSpeakerIdx = sel;
    mVolumChannelIdx = chanPos;
    mVolumNeedsLoad.store(true);
  }
}

void NeuralAmpModeler::_VolumSetCustomChannelStepper(int customIdx, int slot, bool supportLane, int selected)
{
  const auto amp = volum::custom::CustomAmpAt(customIdx);
  std::vector<std::string> labels;
  for (int c : volum::custom::AmpSlotChannels(amp, slot))
    labels.push_back(std::to_string(c));
  if (labels.empty())
    labels.push_back("1"); // DIRECT amp-only fallback
  const int sel = std::clamp(selected, 0, (int)labels.size() - 1);
  // The MAIN and SUPPORT lanes have separate channel steppers; drive whichever
  // belongs to the focused lane.
  const int tag = supportLane ? kCtrlTagVoLumSupportChannelStep : kCtrlTagVoLumChannelStep;
  if (auto* pGfx = GetUI())
    if (auto* stepper = pGfx->GetControlWithTag(tag))
      stepper->As<VoLumChannelStepControl>()->SetChannels(labels, sel);
}

volum::VoLumAmpSettings& NeuralAmpModeler::_VolumActiveScene()
{
  if (mVolumCustomMainIdx >= 0)
  {
    const std::string id = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
    if (!id.empty())
      return volum::content::GlobalContentStore().reg().customScenes[id];
  }
  return mVolumAmpSettings[mVolumAmpIdx];
}

const char* NeuralAmpModeler::_VolumMainAmpDisplayName() const
{
  if (mVolumCustomMainIdx >= 0)
  {
    const auto& customAmps = volum::custom::MockCustomAmps();
    if (mVolumCustomMainIdx < static_cast<int>(customAmps.size()))
      return customAmps[(size_t)mVolumCustomMainIdx].c_str();
  }
  return volum::kAmps[mVolumAmpIdx].displayName;
}

void NeuralAmpModeler::_VolumSelectIR(int irIdx, bool support, bool interactive)
{
  const std::string id = volum::custom::IRIdAt(irIdx);
  const std::string rel = volum::custom::IRFileAt(irIdx);
  if (id.empty() || rel.empty())
    return;
  // A custom IR convolves the amp's DIRECT (raw) capture. A custom amp with no
  // DIRECT capture has nothing to feed the IR, so refuse the selection (the cab
  // row already greys the button out; this guards the menu/dialog/restore paths).
  const int customLane = support ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane >= 0 && !volum::custom::HasDirectCapture(volum::custom::CustomAmpAt(customLane)))
  {
    if (support)
      _VolumActiveScene().supportActiveIrId.clear();
    else
      _VolumActiveScene().activeIrId.clear();
    if (interactive)
      if (auto* pGfx = GetUI())
        _ShowMessageBox(pGfx,
                        "This custom amp has no DIRECT capture, so a custom IR has no raw signal to "
                        "convolve.\n\nAdd a DIRECT (AMP-/DI-) capture to use a custom IR.",
                        "Impulse Response", EMsgBoxType::kMB_OK);
    return;
  }
  const auto abs = volum::content::GlobalContentStore().ResolveStored(rel);
  // VoLum: reject unreasonably large IRs before decoding the whole file (the
  // convolver only uses the first ~8192 samples). Interactive guard with a clear
  // message; the restore path validated size at import time.
  std::string sizeWhy;
  if (!volum::IrFileSizeAcceptable(abs.string(), sizeWhy))
  {
    if (auto* pGfx = GetUI())
      _ShowMessageBox(pGfx, sizeWhy.c_str(), "Impulse Response", EMsgBoxType::kMB_OK);
    return;
  }
  WDL_String p(abs.string().c_str());
  const dsp::wav::LoadReturnCode loadRc = _StageIR(p, support);
  if (loadRc != dsp::wav::LoadReturnCode::SUCCESS)
  {
    // VoLum: surface why the IR did not activate instead of failing silently.
    if (auto* pGfx = GetUI())
    {
      const std::string msg = "VoLum could not load this impulse response.\n\n" + dsp::wav::GetMsgForLoadReturnCode(loadRc);
      _ShowMessageBox(pGfx, msg.c_str(), "Impulse Response", EMsgBoxType::kMB_OK);
    }
    return;
  }
  // A custom IR replaces the baked cab: force this lane's amp onto its DIRECT /
  // No-Cab capture so the IR convolves the raw amp (not amp + baked cab).
  _VolumForceDirectCapture(support);
  const int toggle = support ? kSupportIRToggle : kIRToggle;
  if (support)
    _VolumActiveScene().supportActiveIrId = id;
  else
    _VolumActiveScene().activeIrId = id;
  GetParam(toggle)->Set(1.0);
  SendParameterValueFromDelegate(toggle, GetParam(toggle)->GetNormalized(), true);
  // The shared speaker row shows the focused lane's IR; only update it when this
  // lane is the one on screen (a background restore of the other lane must not
  // overwrite the visible chip).
  if (support == _VolumSupportFocused())
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
      {
        const auto& names = volum::custom::MockIRLibrary();
        if (irIdx >= 0 && irIdx < (int)names.size())
          spk->As<VoLumSpeakerRowControl>()->SetIrCab(true, names[(size_t)irIdx].c_str());
      }
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
}

// Switch the given lane's amp onto its DIRECT (No-Cab) capture so a custom IR
// convolves the raw amp. Custom amps without a DIRECT capture keep their current
// cab (best effort - there is no raw signal to convolve).
void NeuralAmpModeler::_VolumForceDirectCapture(bool support)
{
  const bool laneFocused = (support == _VolumSupportFocused());
  auto selectRow0 = [this, laneFocused]() {
    if (!laneFocused)
      return; // the shared row shows the other lane; don't disturb it
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
        spk->As<VoLumSpeakerRowControl>()->SetSelected(0);
  };
  const int customLane = support ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane >= 0)
  {
    const auto amp = volum::custom::CustomAmpAt(customLane);
    const auto chs = volum::custom::AmpSlotChannels(amp, volum::custom::kDirectSlot);
    if (chs.empty())
      return; // no raw capture to convolve; leave the current cab in place
    // Keep the current gain stage if DIRECT also covers it (item: selecting a
    // custom IR while already on a DIRECT channel must not snap back to 1).
    const int curCh = support ? mVolumCustomSupportChannel : mVolumCustomMainChannel;
    const int snapped = volum::custom::SnapChannel(chs, curCh);
    const int chanPos = volum::custom::ChannelStepIndex(chs, snapped);
    if (support)
    {
      mVolumCustomSupportSlot = volum::custom::kDirectSlot;
      mVolumCustomSupportChannel = snapped;
      if (laneFocused)
        _VolumSetCustomChannelStepper(customLane, volum::custom::kDirectSlot, true, chanPos);
      mVolumSupportNeedsLoad.store(true);
    }
    else
    {
      mVolumCustomMainSlot = volum::custom::kDirectSlot;
      mVolumCustomMainChannel = snapped;
      mVolumSpeakerIdx = 0;
      mVolumChannelIdx = chanPos;
      _VolumSetCustomChannelStepper(customLane, volum::custom::kDirectSlot, false, chanPos);
      mVolumNeedsLoad.store(true);
    }
    selectRow0();
    return;
  }
  if (support)
  {
    if (GetParam(kSupportSpeakerIdx)->Int() != 0)
    {
      GetParam(kSupportSpeakerIdx)->Set(0.0);
      SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
      _VolumRefreshSupportChannels();
      selectRow0();
      mVolumSupportNeedsLoad.store(true);
    }
    return;
  }
  if (mVolumSpeakerIdx != 0)
  {
    mVolumSpeakerIdx = 0;
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = 0;
    _VolumRefreshChannels();
    selectRow0();
    mVolumNeedsLoad.store(true);
  }
}

void NeuralAmpModeler::_VolumClearIR(bool support)
{
  // audio thread drops the lane's convolver in _ApplyDSPStaging
  (support ? mShouldRemoveSupportIR : mShouldRemoveIR) = true;
  if (support)
    _VolumActiveScene().supportActiveIrId.clear();
  else
    _VolumActiveScene().activeIrId.clear();
  const int toggle = support ? kSupportIRToggle : kIRToggle;
  GetParam(toggle)->Set(0.0);
  SendParameterValueFromDelegate(toggle, GetParam(toggle)->GetNormalized(), true);
  if (support == _VolumSupportFocused())
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
        spk->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
}

void NeuralAmpModeler::_VolumApplyActiveIr(const std::string& irId, bool support)
{
  const int idx = volum::custom::IRIndexById(irId);
  if (idx < 0)
  {
    // Empty or orphaned id (the IR was deleted / is missing on this machine):
    // drop the convolver so the baked cab takes over. No UI when headless.
    (support ? mShouldRemoveSupportIR : mShouldRemoveIR) = true;
    if (support == _VolumSupportFocused())
      if (auto* pGfx = GetUI())
        if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
          spk->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
    return;
  }
  _VolumSelectIR(idx, support, /*interactive=*/false);
}

void NeuralAmpModeler::_VolumReconcileActiveIr()
{
  auto& scene = _VolumActiveScene();
  // SUPPORT lane: an orphaned id drops the support convolver. The IR had forced
  // the lane onto its DIRECT (cab-less) capture, so recover to a real cab too -
  // mirroring the MAIN _VolumFallbackToAvailableCab - instead of leaving the
  // support lane on the bare raw amp.
  if (!scene.supportActiveIrId.empty() && volum::custom::IRIndexById(scene.supportActiveIrId) < 0)
  {
    _VolumClearIR(true);
    if (mVolumCustomSupportIdx >= 0)
    {
      const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
      const auto slots = volum::custom::AmpSlots(amp);
      int chosen = volum::custom::kDirectSlot;
      for (int s : slots) // prefer a real cab over DIRECT
        if (s != volum::custom::kDirectSlot)
        {
          chosen = s;
          break;
        }
      if (chosen == volum::custom::kDirectSlot && !slots.empty())
        chosen = slots.front();
      const auto chs = volum::custom::AmpSlotChannels(amp, chosen);
      mVolumCustomSupportSlot = chosen;
      mVolumCustomSupportChannel = chs.empty() ? 1 : volum::custom::SnapChannel(chs, mVolumCustomSupportChannel);
      scene.supportCustomSlot = mVolumCustomSupportSlot;
      scene.supportCustomChannel = mVolumCustomSupportChannel;
      mVolumSupportNeedsLoad.store(true);
      if (_VolumSupportFocused())
        _VolumApplyFocusedLaneCabs();
    }
    else if (GetParam(kSupportAmpIdx)->Int() >= 0)
    {
      GetParam(kSupportSpeakerIdx)->Set(1.0); // first baked cab
      SendParameterValueFromDelegate(kSupportSpeakerIdx, GetParam(kSupportSpeakerIdx)->GetNormalized(), true);
      _VolumRefreshSupportChannels();
      mVolumSupportNeedsLoad.store(true);
    }
  }

  const std::string id = scene.activeIrId;
  // Only trust the visible IR chip as a signal for the MAIN lane when MAIN is
  // the focused lane (the shared row otherwise shows the support lane).
  bool uiActive = false;
  if (!_VolumSupportFocused())
    if (auto* pGfx = GetUI())
      if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
        uiActive = spk->As<VoLumSpeakerRowControl>()->IsIrCabActive();
  // Nothing was using a custom IR.
  if (id.empty() && !uiActive)
    return;
  // The active IR still resolves: leave the live state untouched.
  if (!id.empty() && volum::custom::IRIndexById(id) >= 0)
    return;
  // Orphaned (deleted on this machine): recover to a real cab.
  _VolumFallbackToAvailableCab();
}

void NeuralAmpModeler::_VolumFallbackToAvailableCab()
{
  mShouldRemoveIR = true; // audio thread drops mIR in _ApplyDSPStaging
  _VolumActiveScene().activeIrId.clear();
  GetParam(kIRToggle)->Set(0.0);
  SendParameterValueFromDelegate(kIRToggle, GetParam(kIRToggle)->GetNormalized(), true);
  auto* pGfx = GetUI();
  auto* row = pGfx ? pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow) : nullptr;
  if (mVolumCustomMainIdx >= 0)
  {
    const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
    const auto slots = volum::custom::AmpSlots(amp);
    int chosenSlot = volum::custom::kDirectSlot, sel = 0;
    for (int s : slots) // prefer a real cab over DIRECT
      if (s != volum::custom::kDirectSlot)
      {
        chosenSlot = s;
        sel = s + 1;
        break;
      }
    if (sel == 0 && !slots.empty())
    {
      chosenSlot = slots.front();
      sel = (chosenSlot == volum::custom::kDirectSlot) ? 0 : chosenSlot + 1;
    }
    const auto chs = volum::custom::AmpSlotChannels(amp, chosenSlot);
    mVolumCustomMainSlot = chosenSlot;
    mVolumCustomMainChannel = chs.empty() ? 1 : chs.front();
    mVolumSpeakerIdx = sel;
    mVolumChannelIdx = 0;
    _VolumSetCustomChannelStepper(mVolumCustomMainIdx, chosenSlot, false);
    if (row)
    {
      row->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
      row->As<VoLumSpeakerRowControl>()->SetSelected(sel);
    }
    mVolumNeedsLoad.store(true);
  }
  else
  {
    // Factory amps always ship No Cab + 3 baked cabs; prefer the first real cab.
    const int sel = 1;
    mVolumSpeakerIdx = sel;
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = sel;
    _VolumRefreshChannels();
    if (row)
    {
      row->As<VoLumSpeakerRowControl>()->SetIrCab(false, "");
      row->As<VoLumSpeakerRowControl>()->SetSelected(sel);
    }
    mVolumNeedsLoad.store(true);
  }
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
}

void NeuralAmpModeler::_VolumResetAmpToFactory()
{
  // Factory baseline == a default-constructed settings scene. Reset the FOCUSED
  // amp's scene (a custom amp keeps its own scene in the content library, so
  // resetting the factory slot would target the wrong owner), apply it to the
  // live params, rediscover channels, and clear any custom IR / recalled preset.
  const bool customMain = mVolumCustomMainIdx >= 0;
  auto& scene = _VolumActiveScene();
  scene = volum::VoLumAmpSettings{};
  _VolumApplyAmpSettings(scene);
  if (customMain)
    _VolumApplyCustomMainCabs(mVolumCustomMainIdx); // re-show this amp's named cabs
  else
    _VolumRefreshChannels();
  mVolumNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  // Drop any recalled preset: the bar reads "No Preset" and edits no longer diff.
  _VolumForgetActivePreset();
  if (auto* pGfx = GetUI())
  {
    if (auto* spk = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
    {
      auto* row = spk->As<VoLumSpeakerRowControl>();
      row->SetIrCab(false, "");
      if (!customMain)
        row->SetFactoryCabs(); // custom amps keep their named cabs (set above)
    }
    if (auto* pb = pGfx->GetControlWithTag(kCtrlTagVoLumPresetBar))
      pb->As<VoLumPresetBarControl>()->SelectName(""); // -> "No Preset", clean
  }
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
  auto* presetBar = bar->As<VoLumPresetBarControl>();
  const bool dirty = presetBar->IsEditDirty();
  const int activePresetIdx = presetBar->ActiveIndex();
  std::vector<VoLumListMenuControl::Row> rows;
  if (presets.empty())
  {
    // No presets to come back from, so the reset-to-factory row is pointless.
    rows.push_back({"No presets yet", -99, false, true}); // dim hint
  }
  else
  {
    // Pinned reset-to-factory row at the top, separated by a divider.
    rows.push_back({"Default (factory settings)", VoLumListMenuControl::kDefault, true, false, true});
    for (int i = 0; i < (int)presets.size(); i++)
      rows.push_back({presets[(size_t)i], i, false, false});
  }
  // When the rig is dirty, offer a one-click save path right in the dropdown:
  // overwrite the active named preset, or (no named preset / on Default) save a
  // new one. Saves opening the Manage panel just to commit a tweak.
  if (dirty)
  {
    if (activePresetIdx >= 0 && activePresetIdx < (int)presets.size())
      rows.push_back(
        {"Overwrite \"" + presets[(size_t)activePresetIdx] + "\"", VoLumListMenuControl::kOverwrite, true, false, true});
    else
      rows.push_back({"Save current as new...", VoLumListMenuControl::kSaveAsNew, true, false, true});
  }
  rows.push_back({"Manage presets...", VoLumListMenuControl::kManage, true, false});

  auto* menu = raw->As<VoLumListMenuControl>();
  const IRECT anchor = bar->GetRECT();
  const float w = std::max(anchor.W(), 220.f);
  const auto bounds = pGfx->GetBounds();
  const float top = anchor.B + 4.f;
  const float h = std::min(VoLumListMenuControl::MenuHeight(rows.size()), std::max(110.f, bounds.B - top - 8.f));
  float l = anchor.L;
  if (l + w > bounds.R - 4.f)
    l = bounds.R - 4.f - w;
  menu->SetMenuRect(IRECT(l, top, l + w, top + h));
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

  auto* menu = rawCtrl->As<VoLumListMenuControl>();
  if (!menu)
    return;

  if (!rawCtrl->IsHidden())
  {
    _VolumHideSupportAmpMenu();
    return;
  }

  // Row 0 = "(none)" so the user can clear the support amp without disabling Dual Amp mode.
  // Then the factory amps, then a "CUSTOM" group (header + custom amps) when any exist.
  std::vector<VoLumListMenuControl::Row> rows;
  rows.push_back({"(none)", VoLumListMenuControl::kNone, false, false});
  for (int i = 0; i < volum::kAmpCount; ++i)
    rows.push_back({volum::kAmps[i].displayName, i, false, false});
  const auto& customAmps = volum::custom::MockCustomAmps();
  if (!customAmps.empty())
  {
    rows.push_back({"CUSTOM", 0, false, false, false, false, /*header=*/true});
    for (int c = 0; c < static_cast<int>(customAmps.size()); ++c)
      rows.push_back({customAmps[(size_t)c], kVolumCustomSupportBase + c, false, false, false, /*group=*/true});
  }

  int selected = VoLumListMenuControl::kNone;
  if (mVolumCustomSupportIdx >= 0 && mVolumCustomSupportIdx < static_cast<int>(customAmps.size()))
    selected = kVolumCustomSupportBase + mVolumCustomSupportIdx;
  else
  {
    const int currentSupportIdx = GetParam(kSupportAmpIdx)->Int();
    if (currentSupportIdx >= 0 && currentSupportIdx < volum::kAmpCount)
      selected = currentSupportIdx;
  }

  const float menuW = std::max(anchorRect.W() * 0.96f, 200.f);
  const float menuH = VoLumListMenuControl::MenuHeight(rows.size());
  const float panelW = static_cast<float>(pGfx->Width());
  const float panelH = static_cast<float>(pGfx->Height());
  const float anchorL = std::min(anchorRect.L, panelW - menuW - 8.f);

  // Prefer to drop the menu BELOW the support hero so the support panel stays clickable to
  // dismiss the dropdown. The list scrolls internally, so cap the visible height to whichever
  // gap (below / above) is larger and let the user scroll the rest.
  const float spaceBelow = panelH - (anchorRect.B + 6.f) - 8.f;
  const float spaceAbove = anchorRect.T - 6.f - 8.f;
  float menuT;
  float visH;
  if (menuH <= spaceBelow)
  {
    menuT = anchorRect.B + 6.f;
    visH = menuH;
  }
  else if (menuH <= spaceAbove)
  {
    menuT = anchorRect.T - 6.f - menuH;
    visH = menuH;
  }
  else if (spaceBelow >= spaceAbove)
  {
    menuT = anchorRect.B + 6.f;
    visH = spaceBelow;
  }
  else
  {
    visH = spaceAbove;
    menuT = anchorRect.T - 6.f - visH;
  }

  const IRECT menuRect(anchorL, menuT, anchorL + menuW, menuT + visH);

  menu->SetMenuRect(menuRect);
  menu->SetRows(rows, selected);
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
  // Picking a factory support amp (or "(none)") always clears a custom support partner.
  const bool hadCustom = mVolumCustomSupportIdx >= 0;
  mVolumCustomSupportIdx = -1;
  if (!hadCustom && GetParam(kSupportAmpIdx)->Int() == clamped)
    return;

  GetParam(kSupportAmpIdx)->Set(clamped);
  SendParameterValueFromDelegate(kSupportAmpIdx, GetParam(kSupportAmpIdx)->GetNormalized(), true);
  _VolumActiveScene().supportCustomId.clear(); // factory partner: drop any custom ref
  mVolumSupportNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
  _VolumRefreshSupportChannels();
  _VolumApplyFocusedLaneCabs();
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumSetSupportCustom(int customIdx)
{
  const auto& names = volum::custom::MockCustomAmps();
  if (customIdx < 0 || customIdx >= static_cast<int>(names.size()))
    return;

  mVolumCustomSupportIdx = customIdx;
  // Clear the factory reference so the SUPPORT lane renders the custom amp's
  // art/cabs (not a factory amp).
  if (GetParam(kSupportAmpIdx)->Int() != -1)
  {
    GetParam(kSupportAmpIdx)->Set(-1.0);
    SendParameterValueFromDelegate(kSupportAmpIdx, GetParam(kSupportAmpIdx)->GetNormalized(), true);
  }
  // Persist the custom partner by stable id and resolve a default (slot, channel)
  // so the SUPPORT lane produces sound immediately (F6 dual amp DSP).
  _VolumActiveScene().supportCustomId = volum::custom::CustomAmpIdAt(customIdx);
  {
    const auto amp = volum::custom::CustomAmpAt(customIdx);
    int s = volum::custom::kDirectSlot, c = 1;
    if (volum::content::DefaultCaptureSelection(amp, s, c))
    {
      mVolumCustomSupportSlot = s;
      mVolumCustomSupportChannel = c;
    }
    // Persist the freshly resolved cab/channel so it round-trips like MAIN.
    _VolumActiveScene().supportCustomSlot = mVolumCustomSupportSlot;
    _VolumActiveScene().supportCustomChannel = mVolumCustomSupportChannel;
  }
  mVolumSupportNeedsLoad.store(true);
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
  _VolumApplyFocusedLaneCabs();
  _UpdateVoLumLayout();
}

void NeuralAmpModeler::_VolumApplyFocusedLaneCabs()
{
  // Reconcile the shared cabinet row + channel stepper with the focused lane.
  // Custom lanes (MAIN or the custom SUPPORT partner) show their own named cabs;
  // factory lanes restore the stock G12/G65/V30 labels and that lane's channels.
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl)
    return;

  const bool supportFocus = GetParam(kDualAmpActive)->Bool() && mVolumDualAmpFocusedSupport;
  const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
  if (customLane >= 0)
  {
    _VolumApplyCustomMainCabs(customLane, supportFocus);
    return;
  }

  auto* row = spkCtrl->As<VoLumSpeakerRowControl>();
  row->SetFactoryCabs();
  // Factory amps always expose a No-Cab (DIRECT) path, so a custom IR is allowed.
  row->SetIrEnabled(true);
  if (supportFocus)
  {
    row->SetSelected(std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3));
    _VolumRefreshSupportChannels();
  }
  else
  {
    row->SetSelected(mVolumSpeakerIdx);
    _VolumRefreshChannels();
  }
  // Factory amps can also carry a per-lane custom IR; reflect the focused lane's.
  _VolumReflectLaneIrChip(supportFocus);
}

void NeuralAmpModeler::_VolumReflectLaneIrChip(bool support)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl)
    return;
  auto* row = spkCtrl->As<VoLumSpeakerRowControl>();
  const std::string irId = support ? _VolumActiveScene().supportActiveIrId : _VolumActiveScene().activeIrId;
  const int irIdx = irId.empty() ? -1 : volum::custom::IRIndexById(irId);
  const auto& irNames = volum::custom::MockIRLibrary();
  if (irIdx >= 0 && irIdx < static_cast<int>(irNames.size()))
    row->SetIrCab(true, irNames[static_cast<size_t>(irIdx)].c_str());
  else
    row->SetIrCab(false, "");
}

void NeuralAmpModeler::_VolumRefreshSupportChannels()
{
  mVolumSupportChannelFiles.clear();
  mVolumSupportChannelLabels.clear();

  // Custom support amp: channels come from the custom amp's cab slots (mock),
  // not the factory rig folders. Drive the support stepper from its first
  // populated slot and skip the factory discovery below.
  if (mVolumCustomSupportIdx >= 0)
  {
    const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
    const auto slots = volum::custom::AmpSlots(amp);
    // Honor the lane's saved/last cab slot + gain stage instead of snapping back
    // to the first populated slot (item: custom support amp forgets its cab /
    // channel on recall). Fall back to the first slot only when the saved one no
    // longer exists.
    int slot = mVolumCustomSupportSlot;
    if (slots.empty())
      slot = volum::custom::kDirectSlot;
    else if (std::find(slots.begin(), slots.end(), slot) == slots.end())
      slot = slots.front();
    const auto chs = volum::custom::AmpSlotChannels(amp, slot);
    const int sel = volum::custom::ChannelStepIndex(chs, volum::custom::SnapChannel(chs, mVolumCustomSupportChannel));
    mVolumCustomSupportSlot = slot;
    if (!chs.empty())
      mVolumCustomSupportChannel = chs[(size_t)std::clamp(sel, 0, (int)chs.size() - 1)];
    _VolumSetCustomChannelStepper(mVolumCustomSupportIdx, slot, /*supportLane=*/true, sel);
    return;
  }

  const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
  if (supportAmpIdx >= 0 && supportAmpIdx < volum::kAmpCount && !mVolumRigsRoot.empty())
  {
    const int speakerIdx = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3);
    auto channels =
      volum::DiscoverChannels(std::filesystem::path(mVolumRigsRoot), volum::kAmps[supportAmpIdx].folderName,
                              volum::kSpeakerPrefixes[speakerIdx]);
    for (auto& ch : channels)
    {
      mVolumSupportChannelFiles.push_back(std::move(ch.filename));
      mVolumSupportChannelLabels.push_back(std::move(ch.label));
    }

    int channelIdx = std::clamp(
      GetParam(kSupportChannelIdx)->Int(), 0, std::max(0, static_cast<int>(mVolumSupportChannelFiles.size()) - 1));
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
  // Polarity belongs to the SUPPORT lane whenever it has an amp - a factory amp
  // or a custom support partner.
  const bool hasSupportAmp =
    (GetParam(kSupportAmpIdx)->Int() >= 0 && GetParam(kSupportAmpIdx)->Int() < volum::kAmpCount)
    || mVolumCustomSupportIdx >= 0;
  const bool showSupportPolarity = showPanKnobs && hasSupportAmp;

  if (auto* spkRow = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow))
  {
    // A focused custom lane (MAIN or custom SUPPORT) manages its own cab
    // selection in _VolumApplyFocusedLaneCabs; don't fight it here. Factory
    // lanes track their per-amp speaker index.
    const int customLane = supportFocus ? mVolumCustomSupportIdx : mVolumCustomMainIdx;
    if (customLane < 0)
    {
      auto* row = spkRow->As<VoLumSpeakerRowControl>();
      const int focusedSpeakerIdx =
        supportFocus ? std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3) : mVolumSpeakerIdx;
      row->SetSelected(focusedSpeakerIdx);
    }
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

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath, bool support)
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
    // sees a fully-constructed object or none at all. Live path commits in _ApplyDSPStaging.
    // The MAIN amp and the dual-amp SUPPORT lane stage into separate convolvers.
    std::lock_guard<std::mutex> lock(mStagingMutex);
    auto& stagedSlot = support ? mStagedSupportIR : mStagedIR;
    auto& pathPair = support ? mSupportIRPaths : mIRPaths;
    if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
    {
      stagedSlot = std::move(stagedIR);
      volum::dsp_staging::StagePathOnSuccess(pathPair, irPath);
    }
    else
    {
      stagedSlot = nullptr;
      volum::dsp_staging::ClearStagedPath(pathPair);
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
