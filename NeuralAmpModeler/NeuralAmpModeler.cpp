#include <algorithm> // std::clamp, std::min
#include <cassert> // RT capacity invariants
#include <cmath> // pow
#include <chrono> // debug-only custom-amp seeding sandbox naming
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

  // Tremolo (POST) - third POST pedal, runs last after Reverb. Defaults voice the
  // classic "Bang Bang (My Baby Shot Me Down)" tone: smooth Bias sine, deep, slow.
  GetParam(kTremoloActive)->InitBool("TremoloActive", false);
  GetParam(kTremoloMode)->InitEnum("TremoloMode", volum::kVoLumTremoloModeBias, {"Optical", "Bias", "Harmonic"});
  GetParam(kTremoloRate)->InitDouble("TremoloRate", 5.0, 0.1, 20.0, 0.1, "Hz");
  GetParam(kTremoloDepth)->InitDouble("TremoloDepth", 0.85, 0.0, 1.0, 0.01);
  GetParam(kTremoloShape)->InitDouble("TremoloShape", 0.0, 0.0, 1.0, 0.01);
  GetParam(kTremoloMix)->InitDouble("TremoloMix", 1.0, 0.0, 1.0, 0.01);
  GetParam(kTremoloCrossover)->InitDouble("TremoloCrossover", 800.0, 200.0, 2000.0, 10.0, "Hz");
  GetParam(kTremoloSync)->InitBool("TremoloSync", false);
  GetParam(kTremoloDivision)
    ->InitEnum("TremoloDivision", volum::kVoLumTremoloDivisionDefault,
               {"1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16"});

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
  // PRE Pitch pedal (Transpose / Octaver), inserted at the very front of the PRE chain.
  GetParam(kPrePitchActive)->InitBool("PrePitchActive", false);
  GetParam(kPrePitchMode)->InitEnum("PrePitchMode", volum::kVoLumPitchModeTranspose, {"Transpose", "Octaver"});
  GetParam(kPrePitchSemitones)->InitDouble("PrePitchSemitones", 0.0, -12.0, 7.0, 1.0, "st");
  GetParam(kPrePitchMix)->InitDouble("PrePitchMix", 1.0, 0.0, 1.0, 0.01);
  GetParam(kPrePitchOctDown)->InitDouble("PrePitchOctDown", 0.8, 0.0, 1.0, 0.01);
  GetParam(kPrePitchOctUp)->InitDouble("PrePitchOctUp", 0.0, 0.0, 1.0, 0.01);
  GetParam(kPrePitchDry)->InitDouble("PrePitchDry", 1.0, 0.0, 1.0, 0.01);
  GetParam(kPrePitchVoicing)->InitEnum("PrePitchVoicing", volum::kVoLumPitchVoicingModern, {"Vintage", "Modern"});
  GetParam(kPrePitchLevel)->InitDouble("PrePitchLevel", 0.0, -20.0, 20.0, 0.1, "dB");
  _SetMuteFloorDbDisplay(GetParam(kPrePitchLevel));
  GetParam(kPrePitchTransChar)->InitEnum("PrePitchTransChar", volum::kVoLumPitchCharacterInstant, {"Drop", "Instant"});
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
#ifndef NDEBUG
      // Debug-only, opt-in screenshot/test seeding: VOLUM_SEED_CUSTOM_AMPS=N
      // sandboxes the content store in a fresh temp dir and seeds N custom amps
      // so the local screenshot harness (win-screenshot.ps1) can reach states
      // that only appear once the amp library overflows (custom rows + sidebar
      // scrollbar). It NEVER touches the user's real content store and is
      // compiled out of release builds. See backlog/Q4-screenshot-harness-*.
      int volumSeedCustomAmps = 0;
      if (const char* seed = std::getenv("VOLUM_SEED_CUSTOM_AMPS"); seed && seed[0])
        volumSeedCustomAmps = std::atoi(seed);
      if (volumSeedCustomAmps > 0)
      {
        std::error_code seedEc;
        const auto sandbox = std::filesystem::temp_directory_path(seedEc)
                             / ("volum-seed-" + std::to_string(static_cast<unsigned long long>(
                                                  std::chrono::steady_clock::now().time_since_epoch().count())));
        if (!seedEc)
        {
          std::filesystem::create_directories(sandbox, seedEc);
          contentDir = sandbox; // redirect the whole session to the sandbox
        }
      }
#endif
      if (!contentDir.empty())
      {
        volum::content::GlobalContentStore().SetBaseDir(contentDir);
        volum::content::GlobalContentStore().Load();
      }
#ifndef NDEBUG
      if (volumSeedCustomAmps > 0)
        for (int i = 0; i < volumSeedCustomAmps; ++i)
          volum::custom::AddCustomAmp("Seed Amp " + std::to_string(i + 1), i);
#endif
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
            _VolumClearIR(supportFocus);
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
              _VolumClearIR(false);
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
                _VolumClearIR(true);
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
                "Transpose the whole signal in half-steps (-12..+7). Polyphonic; tuned tightest for drop tuning.",
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
    auto* pitchTransCharPill = new VoLumSubModePillControl(pitchVoicingRect, kPrePitchTransChar, {"DROP", "INSTANT"});
    pitchTransCharPill->SetTooltip(
      "Transpose engine. INSTANT = lowest latency (~8.6 ms), default | DROP = WSOLA, "
      "cleanest on big shifts (~17 ms). Same pitch accuracy; pick by feel vs latency.");
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
    GetParam(kSupportIRToggle)->Value(), mSupportIR != nullptr, GetParam(kPrePitchActive)->Bool(),
    GetParam(kTremoloActive)->Bool());
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
  {
    std::lock_guard<std::mutex> lock(mPrePitchMutex);
    mPitch.Configure(sampleRate, maxBlockSize);
    mPitch.Reset();
  }
  mPrePitchConfigureRequested.store(false);
  const size_t postEffectChannels = std::max<size_t>(1, static_cast<size_t>(NOutChansConnected()));
  mDelay.Prepare(postEffectChannels, static_cast<size_t>(maxBlockSize), sampleRate);
  mReverb.Prepare(postEffectChannels, static_cast<size_t>(maxBlockSize), sampleRate);
  mTremolo.Prepare(sampleRate, maxBlockSize, static_cast<int>(postEffectChannels));
  mDelay.Reset();
  mReverb.Reset();
  mTremolo.Reset();
  mPostDelayWasActive = false;
  mPostReverbWasActive = false;
  mPostTremoloWasActive = false;
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

  // PRE Pitch reconfigure off the audio thread (only needed if the grain/sample
  // rate ever changes outside OnReset), then refresh reported latency.
  if (mPrePitchConfigureRequested.exchange(false))
  {
    {
      std::lock_guard<std::mutex> lock(mPrePitchMutex);
      mPitch.Configure(GetSampleRate(), GetBlockSize());
      mPitch.Reset();
    }
    _UpdateLatency();
  }

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
    // Consume any pending lite-mode force-reload request: a same-path reload is
    // normally skipped, but a Lite/Full switch must re-stage the main model.
    const bool forceMainReload = mVolumForceMainReload.exchange(false);

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
      if (fileToLoad == mNAMPaths.live.Get() && !forceMainReload)
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
  // Flush current live params into the active per-amp scene before serializing.
  // The scene is otherwise only synced from OnIdle (line ~2253), which needs an
  // editor / idle pump. A host that sets parameters and immediately saves state
  // without idling - e.g. pluginval's "Plugin state restoration" test - would
  // otherwise persist a stale scene; on reload _VolumRestoreFromSettings then
  // clobbers the freshly-restored live params with that stale scene (seen as
  // "PrePitchDry/Semitones not restored"). const_cast mirrors the existing
  // pattern in this file's const dirty-checks (_VolumIsPreDirty/_VolumIsPostDirty).
  if (mVolumInitComplete)
    const_cast<NeuralAmpModeler*>(this)->_VolumSaveCurrentToSettings();

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
  auto pitchTailFromSettings = [](const volum::VoLumAmpSettings& s) {
    volum::PitchTail p;
    p.present = true;
    p.active = s.prePitchActive;
    p.mode = s.prePitchMode;
    p.semitones = s.prePitchSemitones;
    p.mix = s.prePitchMix;
    p.octDown = s.prePitchOctDown;
    p.octUp = s.prePitchOctUp;
    p.dry = s.prePitchDry;
    p.voicing = s.prePitchVoicing;
    p.level = s.prePitchLevel;
    p.transChar = s.prePitchTransChar;
    return p;
  };
  auto tremoloTailFromSettings = [](const volum::VoLumAmpSettings& s) {
    volum::TremoloTail t;
    t.present = true;
    t.active = s.postTremoloActive;
    t.mode = s.postTremoloMode;
    t.rate = s.postTremoloRate;
    t.depth = s.postTremoloDepth;
    t.shape = s.postTremoloShape;
    t.mix = s.postTremoloMix;
    t.crossover = s.postTremoloCrossover;
    t.sync = s.postTremoloSync;
    t.division = s.postTremoloDivision;
    return t;
  };
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    idTail.perAmpIrId[i] = mVolumAmpSettings[i].activeIrId;
    idTail.perAmpSupportIrId[i] = mVolumAmpSettings[i].supportActiveIrId;
    idTail.perAmpSupportId[i] = mVolumAmpSettings[i].supportCustomId;
    idTail.perAmpSupportSlot[i] = mVolumAmpSettings[i].supportCustomSlot;
    idTail.perAmpSupportChannel[i] = mVolumAmpSettings[i].supportCustomChannel;
    idTail.perAmpPitch[i] = pitchTailFromSettings(mVolumAmpSettings[i]);
    idTail.perAmpTremolo[i] = tremoloTailFromSettings(mVolumAmpSettings[i]);
  }
  if (mVolumPreLocked)
    idTail.lockedPrePitch = pitchTailFromSettings(mVolumLiveLockedPre);
  if (mVolumPostLocked)
    idTail.lockedPostTremolo = tremoloTailFromSettings(mVolumLiveLockedPost);
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
    case kPrePitchActive:
    case kPrePitchMode:
    case kPrePitchTransChar:
      // Toggling the pitch engine, switching Transpose/Octaver, or changing the
      // transpose CHARACTER (Drop ~17 ms / Instant ~8.6 ms) all change reported
      // (PDC) latency, so re-report it to the host.
      if (mVolumInitComplete)
        _UpdateLatency();
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
    case kPrePitchActive:
    case kPrePitchMode:
    case kPrePitchSemitones:
    case kPrePitchMix:
    case kPrePitchOctDown:
    case kPrePitchOctUp:
    case kPrePitchDry:
    case kPrePitchVoicing:
    case kPrePitchLevel:
    case kPrePitchTransChar: return true;
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
      case kPrePitchActive:
      case kPrePitchMode:
      case kPrePitchVoicing:
      case kPrePitchTransChar:
      // kTremoloActive belongs here too: toggling it must re-run the layout pass so
      // the collapsed POST strip motif + focused card art refresh their active/dim
      // state immediately (previously they only updated on the next focus/card flip).
      case kTremoloActive:
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
      case kTremoloMode:
      case kTremoloSync:
        // Mode picker toggles the Harmonic-only X-OVER knob; Sync swaps the RATE
        // knob for the tempo DIVISION stepper. Both need a layout refresh.
        _UpdateVoLumLayout(pGraphics);
        break;
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
    // VoLum: honor the machine-global A2 Lite/Full choice on the legacy sync
    // load path too (no-op on non-slimmable models). Selected before Reset so
    // only the chosen slice is prewarmed.
    temp->SetSlimmableSize(mVolumLiteMode.load() ? 0.0 : 1.0);
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

#include "VoLumKeyboard.inc.cpp"
#include "VoLumLayoutRuntime.inc.cpp"
#include "VoLumSceneRig.inc.cpp"
#include "VoLumAmpMenus.inc.cpp"

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
  // PRE Pitch pedal reports the granular engine latency (dry is delayed to match)
  // so the host can compensate via PDC. Compute from the CURRENT params (mode +
  // character) rather than mPitch.Latency(): the live member is only refreshed on
  // the audio thread inside SetParams, so reading it here (main thread, right after
  // a param change) would lag by one change and could report the previous
  // character's latency (e.g. show FAST's value while INSTANT is selected).
  if (GetParam(kPrePitchActive)->Bool())
  {
    const auto pitchMode = static_cast<dsp::effect::VoLumPitch::Mode>(
      std::clamp(GetParam(kPrePitchMode)->Int(), 0, volum::kVoLumPitchModeCount - 1));
    const auto pitchChar = static_cast<dsp::effect::VoLumPitch::Character>(
      std::clamp(GetParam(kPrePitchTransChar)->Int(), 0, volum::kVoLumPitchCharacterCount - 1));
    preLatency += dsp::effect::VoLumPitch::LatencyFor(pitchMode, pitchChar, GetSampleRate());
  }

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
