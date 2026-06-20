#pragma once

// VoLum focused-pedal "card" control.
//
// One card per effect (COMP / PRE_NAM1 / PRE_NAM2 / DELAY / REVERB) shown in
// the PRE and POST expanded layouts. Routes clicks to the parent plugin (focus,
// bypass toggle, PRE-capture menu) and renders a cached fractal-motif layer
// behind a preset-name footer and an LED dot. Extracted from VoLumTriptych.h on
// the 1.0-bugs-hygiene branch (file-size hygiene split).
//
// Source-string locks for the layer-cache idiom and bypass-state tracking live
// in test_volum_ui_regressions.cpp and now read this file rather than
// VoLumTriptych.h.

#include "VoLumColorHelpers.h"
#include "VoLumTriptychMotifs.h"
#include "VoLumTriptychState.h"
#include "NeuralAmpModeler.h"

#include <functional>
#include <string>
#include <utility>

using namespace iplug;
using namespace igraphics;

class VoLumPedalCardControl : public IControl
{
public:
  using ClickCallback = std::function<void(VoLumPedalCardControl*, bool isBypassClick)>;

  VoLumPedalCardControl(const IRECT& bounds, EVoLumEffectFocus effect, ClickCallback cb)
  : IControl(bounds)
  , mEffect(effect)
  , mCallback(std::move(cb))
  {
  }

  void Draw(IGraphics& g) override
  {
    bool focused = mIsFocused;
    bool bypassed = (GetValue() < 0.5);

    if (focused)
      g.FillRoundRect(IColor(44, 232, 168, 92), mRECT.GetPadded(2.5f), 6.f);
    DrawPanelDepth(g, mRECT, 4.f);
    if (mHovered)
    {
      g.FillRect(IColor(18, 80, 140, 160), mRECT);
      g.DrawRoundRect(VoLumColors::TEAL_DIM.WithOpacity(0.48f),
                      mRECT.GetPadded(-1.f, -1.f, -1.f, -1.f), 4.f);
    }
    
    IColor borderCol = focused ? VoLumColors::AMBER : (bypassed ? VoLumColors::FRAME : VoLumColors::TEAL_DIM);
    if (mHovered && !focused)
      borderCol = bypassed ? VoLumColors::GOLD_DIM : VoLumColors::TEAL;
    g.DrawRoundRect(borderCol, mRECT, 4.f);
    const float cs = 8.f;
    DrawCornerAccent(g, mRECT.L + 4.f, mRECT.T + 4.f, cs, false, false, borderCol);
    DrawCornerAccent(g, mRECT.R - 4.f, mRECT.T + 4.f, cs, true, false, borderCol);
    DrawCornerAccent(g, mRECT.L + 4.f, mRECT.B - 4.f, cs, false, true, borderCol);
    DrawCornerAccent(g, mRECT.R - 4.f, mRECT.B - 4.f, cs, true, true, borderCol);

    IRECT artRect = mRECT.GetPadded(-2.f, -2.f, -2.f, -22.f);
    if (!g.CheckLayer(mArtLayer) || mCachedBypassed != bypassed) {
      g.StartLayer(this, artRect);
      _DrawFractalArt(g, artRect, bypassed);
      mArtLayer = g.EndLayer();
      mCachedBypassed = bypassed;
    }
    g.DrawLayer(mArtLayer);
    if (bypassed)
      g.FillRect(VoLumColors::HERO_BG.WithOpacity(0.68f), artRect);

    IText presetTxt(11.5f, bypassed ? VoLumColors::CREAM_DIM : VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const std::string presetName = _GetPresetName();
    const IRECT presetRect(mRECT.L + 10.f, mRECT.B - 22.f, mRECT.R - 22.f, mRECT.B - 4.f);
    g.DrawText(presetTxt, presetName.c_str(), presetRect);

    const IRECT ledRect(mRECT.R - 20.f, mRECT.B - 20.f, mRECT.R - 8.f, mRECT.B - 8.f);
    g.FillCircle(bypassed ? IColor(255, 42, 48, 52) : VoLumColors::TEAL, ledRect.MW(), ledRect.MH(), 4.5f);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate());
    if (mIsFocused)
    {
      if (plugin && mEffect == EVoLumEffectFocus::PRE_NAM1)
      {
        plugin->_VolumShowPreCaptureMenu(0, mRECT);
        return;
      }
      if (plugin && mEffect == EVoLumEffectFocus::PRE_NAM2)
      {
        plugin->_VolumShowPreCaptureMenu(1, mRECT);
        return;
      }
    }
    if (mCallback)
      mCallback(this, false);
  }
  
  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;
    if (!mHovered)
    {
      mHovered = true;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mHovered)
    {
      mHovered = false;
      SetDirty(false);
    }
  }

  void SetFocused(bool focused)
  {
    mIsFocused = focused;
    SetDirty(false);
  }

  void SetActiveState(bool active)
  {
    const double value = active ? 1.0 : 0.0;
    const bool wasBypassed = GetValue() < 0.5;
    const bool willBypass = !active;
    if (GetValue() != value)
      SetValue(value);
    if (wasBypassed != willBypass || mCachedBypassed != willBypass)
      mArtLayer = nullptr;
    SetDirty(false);
  }

  EVoLumEffectFocus GetEffect() const { return mEffect; }

private:
  void _DrawFractalArt(IGraphics& g, const IRECT& r, bool dimmed)
  {
    DrawEffectMotif(g, r, mEffect, dimmed);
  }


  std::string _GetPresetName()
  {
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate());
    if (!plugin)
      return (mEffect == EVoLumEffectFocus::DELAY) ? "DIGITAL . 380 ms" : "HALL . 50%";

    WDL_String modeText;
    WDL_String summary;
    switch (mEffect)
    {
      case EVoLumEffectFocus::DELAY:
        plugin->GetParam(kDelayMode)->GetDisplay(modeText);
        summary.SetFormatted(64, "%s . %.0f ms", modeText.Get(), plugin->GetParam(kDelayTime)->Value());
        return summary.Get();
      case EVoLumEffectFocus::REVERB:
        plugin->GetParam(kReverbMode)->GetDisplay(modeText);
        summary.SetFormatted(64, "%s . %.0f %%", modeText.Get(), plugin->GetParam(kReverbMix)->Value() * 100.0);
        return summary.Get();
      case EVoLumEffectFocus::COMP:
        return "Compressor";
      case EVoLumEffectFocus::PRE_NAM1:
        return plugin->_VolumGetPreCaptureLabel(plugin->GetParam(kPreNam1Capture)->Int());
      case EVoLumEffectFocus::PRE_NAM2:
        return plugin->_VolumGetPreCaptureLabel(plugin->GetParam(kPreNam2Capture)->Int());
      default:
        return "BYPASS";
    }
  }

  EVoLumEffectFocus mEffect;
  bool mIsFocused = false;
  bool mHovered = false;
  ILayerPtr mArtLayer;
  bool mCachedBypassed = false;
  ClickCallback mCallback;
};
