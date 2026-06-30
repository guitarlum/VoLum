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
  // default voice: slow, deep Optical throb at a moderate wet blend.
  GetParam(kTremoloActive)->InitBool("TremoloActive", false);
  GetParam(kTremoloMode)->InitEnum("TremoloMode", volum::kVoLumTremoloModeOptical, {"Optical", "Bias", "Harmonic"});
  GetParam(kTremoloRate)->InitDouble("TremoloRate", 3.0, 0.1, 20.0, 0.1, "Hz");
  GetParam(kTremoloDepth)->InitDouble("TremoloDepth", 0.85, 0.0, 1.0, 0.01);
  GetParam(kTremoloShape)->InitDouble("TremoloShape", 0.0, 0.0, 1.0, 0.01);
  GetParam(kTremoloMix)->InitDouble("TremoloMix", 0.60, 0.0, 1.0, 0.01);
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
  GetParam(kPrePitchTransChar)
    ->InitEnum("PrePitchTransChar", volum::kVoLumPitchCharacterInstant, {"Drop", "Instant", "Poly"});
  // Delay tempo sync (reuses the Tremolo division table so both pedals snap to
  // the same musical grid). When off, the free-running Time knob (ms) is used.
  GetParam(kDelaySync)->InitBool("DelaySync", false);
  GetParam(kDelayDivision)
    ->InitEnum("DelayDivision", volum::kVoLumTremoloDivisionDefault,
               {"1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16"});
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

  mLayoutFunc = [this](IGraphics* pGraphics) { _BuildVoLumLayout(pGraphics); };
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
    for (int m = 0; m < volum::kVoLumPitchModeCount; ++m)
      p.modes[m] = s.prePitchModes[m];
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
    for (int m = 0; m < volum::kVoLumTremoloModeCount; ++m)
      t.modes[m] = s.postTremoloModes[m];
    return t;
  };
  auto delayTailFromSettings = [](const volum::VoLumAmpSettings& s) {
    volum::DelayTail d;
    d.present = true;
    d.sync = s.postDelaySync;
    d.division = s.postDelayDivision;
    return d;
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
    idTail.perAmpDelay[i] = delayTailFromSettings(mVolumAmpSettings[i]);
  }
  if (mVolumPreLocked)
    idTail.lockedPrePitch = pitchTailFromSettings(mVolumLiveLockedPre);
  if (mVolumPostLocked)
  {
    idTail.lockedPostTremolo = tremoloTailFromSettings(mVolumLiveLockedPost);
    idTail.lockedPostDelay = delayTailFromSettings(mVolumLiveLockedPost);
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
    case kPrePitchActive:
    case kPrePitchMode:
    case kPrePitchTransChar:
      // Toggling the pitch engine, switching Transpose/Octaver, or changing the
      // transpose CHARACTER (Drop ~17 ms / Instant ~8.6 ms / Poly ~49 ms) all
      // change reported (PDC) latency, so re-report it to the host.
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
      case kPrePitchMode:
      {
        // Switching Transpose<->Octaver saves the outgoing mode's shared knobs and
        // recalls the incoming mode's last knobs (mirrors the POST Tremolo mode
        // picker). Layout refresh follows for the Octaver-only knob swap.
        if (mVolumPreRestoreInProgress)
        {
          _UpdateVoLumLayout(pGraphics);
          break;
        }
        const int oldMode = std::clamp(mVolumPrePitchMode, 0, volum::kVoLumPitchModeCount - 1);
        _VolumSavePrePitchModeSnapshot(oldMode);
        const int newMode = std::clamp(GetParam(kPrePitchMode)->Int(), 0, volum::kVoLumPitchModeCount - 1);
        mVolumPrePitchMode = newMode;
        _VolumRestorePrePitchModeSnapshot(newMode);
        _UpdateVoLumLayout(pGraphics);
        break;
      }
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
      {
        // Switching tremolo mode saves the outgoing mode's knobs and recalls the
        // incoming mode's last knobs (mirrors Delay/Reverb). The mode picker also
        // toggles the Harmonic-only X-OVER knob, so a layout refresh follows.
        if (mVolumPostRestoreInProgress)
          break;
        const int oldMode = std::clamp(mVolumEffectSettings.tremoloMode, 0, volum::kVoLumTremoloModeCount - 1);
        _VolumSaveTremoloModeSnapshot(oldMode);
        const int newMode = std::clamp(GetParam(kTremoloMode)->Int(), 0, volum::kVoLumTremoloModeCount - 1);
        mVolumEffectSettings.tremoloMode = newMode;
        _VolumRestoreTremoloModeSnapshot(newMode);
        _UpdateVoLumLayout(pGraphics);
        break;
      }
      case kTremoloSync:
      case kDelaySync:
        // Sync swaps the free-running knob (Rate/Time) for the tempo DIVISION
        // stepper; layout refresh.
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

#include "VoLumLayoutBuild.inc.cpp"
