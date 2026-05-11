#pragma once

// VoLum tuner + metronome overlay controls.
//
// - VoLumTunerControl: NDSP-style chromatic tuner that consumes a
//   volum::TunerEngine snapshot and renders the strobe arc + cents readout.
// - VoLumMetronomeButtonControl: SVG play/stop transport button used by the
//   metronome overlay header.
// - VoLumMetronomeControl: full metronome panel - BPM stepper, time-signature
//   chips, volume slider, beat grid.
//
// Extracted from VoLumCoreControls.h on the 1.0-bugs-hygiene branch (file-size
// hygiene split). The metronome and tuner DSP live in VoLumMetronomeDSP.h and
// VoLumTunerDSP.h respectively; this file only renders / handles input.

#include "VoLumColorHelpers.h"
#include "VoLumTunerDSP.h"
#include "VoLumMetronomeDSP.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

// =========================================================================
// Tuner overlay (NDSP-style chromatic tuner)
// =========================================================================

class VoLumTunerControl : public IControl
{
public:
  VoLumTunerControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
  }

  void Draw(IGraphics& g) override
  {
    if (mHide)
      return;

    g.FillRect(IColor(200, 8, 10, 14), mRECT);

    const IRECT panel = _PanelRect();
    const IRECT frame = panel.GetPadded(10.f);

    g.FillRoundRect(IColor(255, 22, 22, 30), panel, 10.f);
    g.DrawRoundRect(VoLumColors::FRAME, panel, 10.f, nullptr, 1.2f);
    g.DrawRoundRect(IColor(90, 200, 180, 100), frame, 8.f, nullptr, 1.f);

    DrawCornerAccent(g, panel.L + 11.f, panel.T + 11.f, 16.f, false, false);
    DrawCornerAccent(g, panel.R - 11.f, panel.T + 11.f, 16.f, true, false);
    DrawCornerAccent(g, panel.L + 11.f, panel.B - 11.f, 16.f, false, true);
    DrawCornerAccent(g, panel.R - 11.f, panel.B - 11.f, 16.f, true, true);

    const IRECT titleRect(panel.L, panel.T + 14.f, panel.R, panel.T + 34.f);
    g.DrawText(IText(14.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               "TUNER", titleRect);

    if (!mResult.valid)
    {
      g.DrawText(IText(16.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                 "Play a note...", IRECT(panel.L, panel.T + 70.f, panel.R, panel.T + 100.f));
      return;
    }

    bool inTune = std::fabs(mResult.cents) < 5.f;
    IColor noteColor = inTune ? IColor(255, 80, 220, 120) : VoLumColors::TEXT_BRIGHT;

    const IRECT noteRect(panel.L, panel.T + 42.f, panel.R, panel.T + 95.f);
    char noteStr[8];
    snprintf(noteStr, sizeof(noteStr), "%s%d", volum::TunerDSP::NoteName(mResult.noteIndex), mResult.octave);
    g.DrawText(IText(42.f, noteColor, "Josefin-Bold", EAlign::Center, EVAlign::Middle), noteStr, noteRect);

    const IRECT freqRect(panel.L, panel.T + 96.f, panel.R, panel.T + 114.f);
    char freqStr[32];
    snprintf(freqStr, sizeof(freqStr), "%.1f Hz", mResult.frequency);
    g.DrawText(IText(12.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Center, EVAlign::Middle), freqStr, freqRect);

    const float barW = 240.f;
    const float barH = 12.f;
    const float barY = panel.T + 122.f;
    const float barL = panel.MW() - barW / 2.f;
    const IRECT barRect(barL, barY, barL + barW, barY + barH);

    g.FillRoundRect(IColor(180, 14, 16, 22), barRect, 4.f);
    g.DrawRoundRect(IColor(60, 200, 162, 78), barRect, 4.f, nullptr, 1.f);

    g.DrawLine(VoLumColors::TEXT_DIM, barRect.MW(), barRect.T + 1.f, barRect.MW(), barRect.B - 1.f, nullptr, 1.f);

    float clampedCents = std::clamp(mResult.cents, -50.f, 50.f);
    float needleX = barRect.MW() + (clampedCents / 50.f) * (barW / 2.f - 4.f);
    float needleW = 4.f;
    g.FillRect(noteColor, IRECT(needleX - needleW / 2.f, barRect.T + 1.f, needleX + needleW / 2.f, barRect.B - 1.f));

    const IRECT centsRect(panel.L, barRect.B + 4.f, panel.R, barRect.B + 20.f);
    char centsStr[16];
    snprintf(centsStr, sizeof(centsStr), "%+.0f cents", mResult.cents);
    g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle), centsStr, centsRect);

    g.DrawText(IText(10.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               "FLAT", IRECT(barRect.L - 30.f, barRect.T, barRect.L - 2.f, barRect.B));
    g.DrawText(IText(10.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               "SHARP", IRECT(barRect.R + 2.f, barRect.T, barRect.R + 34.f, barRect.B));
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
    if (mHide)
      return;
    if (!_PanelRect().Contains(x, y))
      _Dismiss();
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    (void)x;
    (void)y;
    if (!mHide && key.VK == kVK_ESCAPE)
    {
      _Dismiss();
      return true;
    }
    return false;
  }

  void SetResult(const volum::TunerResult& r)
  {
    if (mHide)
      return;
    mResult = r;
    SetDirty(false);
  }

  void Show()
  {
    mResult = {};
    Hide(false);
    SetDirty(false);
  }

  void SetDismissAction(std::function<void()> fn) { mDismissAction = std::move(fn); }

private:
  IRECT _PanelRect() const { return mRECT.GetCentredInside(340.f, 180.f); }

  void _Dismiss()
  {
    Hide(true);
    if (mDismissAction)
      mDismissAction();
  }

  volum::TunerResult mResult;
  std::function<void()> mDismissAction;
};

// =========================================================================
// Metronome toolbar button (with active-state glow)
// =========================================================================

class VoLumMetronomeButtonControl : public ISVGButtonControl
{
public:
  VoLumMetronomeButtonControl(const IRECT& bounds, IActionFunction actionFunc, const ISVG& svg)
  : ISVGButtonControl(bounds, actionFunc, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
    {
      g.FillEllipse(IColor(44, 200, 200, 200), mRECT.MW(), mRECT.MH(),
                    mRECT.W() * 0.5f, mRECT.H() * 0.5f);
    }

    float alpha = mActive ? 1.f : 0.35f;
    IBlend blend(EBlend::Default, alpha);
    g.DrawSVG(mOffSVG, mRECT, &blend);
  }

  void SetActive(bool active)
  {
    mActive = active;
    SetDirty(false);
  }

  bool IsActive() const { return mActive; }

private:
  bool mActive = false;
};

// =========================================================================
// Metronome config overlay
// =========================================================================

class VoLumMetronomeControl : public IControl
{
public:
  VoLumMetronomeControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
    mBpmTextEntry = IText(18.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle, 0.f,
                          IColor(245, 14, 16, 22), VoLumColors::TEXT_BRIGHT);
    SetTextEntryLength(6);
  }

  void Draw(IGraphics& g) override
  {
    if (mHide)
      return;

    g.FillRect(IColor(200, 8, 10, 14), mRECT);

    const IRECT panel = _PanelRect();
    const IRECT frame = panel.GetPadded(10.f);

    g.FillRoundRect(IColor(255, 22, 22, 30), panel, 10.f);
    g.DrawRoundRect(VoLumColors::FRAME, panel, 10.f, nullptr, 1.2f);
    g.DrawRoundRect(IColor(90, 200, 180, 100), frame, 8.f, nullptr, 1.f);

    DrawCornerAccent(g, panel.L + 11.f, panel.T + 11.f, 16.f, false, false);
    DrawCornerAccent(g, panel.R - 11.f, panel.T + 11.f, 16.f, true, false);
    DrawCornerAccent(g, panel.L + 11.f, panel.B - 11.f, 16.f, false, true);
    DrawCornerAccent(g, panel.R - 11.f, panel.B - 11.f, 16.f, true, true);

    const IRECT titleRect(panel.L, panel.T + 14.f, panel.R, panel.T + 34.f);
    g.DrawText(IText(14.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               "METRONOME", titleRect);

    // On/Off toggle
    const float toggleY = panel.T + 48.f;
    const float toggleW = 52.f;
    const float toggleH = 24.f;
    const IRECT toggleRect(panel.MW() - toggleW / 2.f, toggleY, panel.MW() + toggleW / 2.f, toggleY + toggleH);

    IColor trackColor = mActive ? IColor(210, 180, 130, 74) : IColor(120, 50, 45, 42);
    g.FillRoundRect(trackColor, toggleRect, toggleH / 2.f);
    g.DrawRoundRect(mActive ? VoLumColors::GOLD_DIM : VoLumColors::FRAME, toggleRect, toggleH / 2.f, nullptr, 1.f);

    float knobX = mActive ? (toggleRect.R - toggleH / 2.f) : (toggleRect.L + toggleH / 2.f);
    g.FillCircle(VoLumColors::TEXT_BRIGHT, knobX, toggleRect.MH(), toggleH / 2.f - 3.f);

    const IRECT onOffLabel(toggleRect.R + 8.f, toggleRect.T, toggleRect.R + 50.f, toggleRect.B);
    g.DrawText(IText(12.f, mActive ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
               mActive ? "ON" : "OFF", onOffLabel);

    mToggleRect = toggleRect;

    // BPM row
    const float bpmY = toggleY + 40.f;
    const IRECT bpmLabel(panel.L + 30.f, bpmY, panel.L + 80.f, bpmY + 22.f);
    g.DrawText(IText(12.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle), "BPM", bpmLabel);

    const IRECT bpmMinusRect(panel.L + 85.f, bpmY, panel.L + 107.f, bpmY + 22.f);
    const IRECT bpmValueRect(panel.L + 107.f, bpmY, panel.R - 107.f, bpmY + 22.f);
    const IRECT bpmPlusRect(panel.R - 107.f, bpmY, panel.R - 85.f, bpmY + 22.f);

    IColor btnColor = VoLumColors::GOLD_DIM;
    g.FillRoundRect(IColor(180, 30, 30, 40), bpmMinusRect, 4.f);
    g.DrawText(IText(16.f, btnColor, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "-", bpmMinusRect);
    g.FillRoundRect(IColor(180, 30, 30, 40), bpmPlusRect, 4.f);
    g.DrawText(IText(16.f, btnColor, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "+", bpmPlusRect);

    char bpmStr[8];
    snprintf(bpmStr, sizeof(bpmStr), "%.0f", mBPM);
    g.FillRoundRect(IColor(220, 14, 16, 22), bpmValueRect, 4.f);
    g.DrawRoundRect(IColor(50, 200, 162, 78), bpmValueRect, 4.f, nullptr, 1.f);
    g.DrawText(IText(18.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle), bpmStr, bpmValueRect);

    mBpmMinusRect = bpmMinusRect;
    mBpmValueRect = bpmValueRect;
    mBpmPlusRect = bpmPlusRect;

    // Volume row
    const float volY = bpmY + 32.f;
    const IRECT volLabel(panel.L + 30.f, volY, panel.L + 80.f, volY + 22.f);
    g.DrawText(IText(12.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle), "VOL", volLabel);

    const float barL = panel.L + 85.f;
    const float barR = panel.R - 40.f;
    const float barMid = volY + 11.f;
    const IRECT volBar(barL, barMid - 3.f, barR, barMid + 3.f);

    g.FillRoundRect(IColor(180, 30, 30, 40), volBar, 3.f);
    float fillW = mVolume * (barR - barL);
    if (fillW > 1.f)
      g.FillRoundRect(VoLumColors::GOLD_DIM, IRECT(barL, volBar.T, barL + fillW, volBar.B), 3.f);

    float handleX = barL + mVolume * (barR - barL);
    g.FillCircle(VoLumColors::TEXT_BRIGHT, handleX, barMid, 6.f);

    char volStr[8];
    snprintf(volStr, sizeof(volStr), "%d%%", static_cast<int>(mVolume * 100.f));
    g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
               volStr, IRECT(barR + 4.f, volY, panel.R - 8.f, volY + 22.f));

    mVolBarRect = IRECT(barL, volY, barR, volY + 22.f);

    // Time signature buttons
    const float tsY = volY + 34.f;
    const IRECT tsLabel(panel.L + 30.f, tsY, panel.L + 80.f, tsY + 22.f);
    g.DrawText(IText(12.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle), "TIME", tsLabel);

    const int numSigs = static_cast<int>(volum::MetronomeTimeSig::kCount);
    float tsX = panel.L + 85.f;
    float tsBtnW = 38.f;
    float tsBtnGap = 4.f;

    for (int i = 0; i < numSigs; ++i)
    {
      IRECT btn(tsX, tsY, tsX + tsBtnW, tsY + 22.f);
      bool selected = (i == static_cast<int>(mTimeSig));
      IColor bg = selected ? IColor(150, 190, 150, 82) : IColor(180, 30, 30, 40);
      IColor txt = selected ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_DIM;

      g.FillRoundRect(bg, btn, 4.f);
      if (selected)
        g.DrawRoundRect(VoLumColors::GOLD_DIM, btn, 4.f, nullptr, 1.f);
      g.DrawText(IText(11.f, txt, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
                 volum::MetronomeTimeSigName(static_cast<volum::MetronomeTimeSig>(i)), btn);

      mTimeSigRects[i] = btn;
      tsX += tsBtnW + tsBtnGap;
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
    if (mHide)
      return;

    const IRECT panel = _PanelRect();
    if (!panel.Contains(x, y))
    {
      Hide(true);
      if (auto* ui = GetUI())
      {
        if (auto* textEntry = ui->GetTextEntryControl())
          textEntry->DismissEdit();
      }
      return;
    }

    if (mToggleRect.Contains(x, y))
    {
      mActive = !mActive;
      if (mOnActiveChanged)
        mOnActiveChanged(mActive);
      SetDirty(false);
      return;
    }

    if (mBpmMinusRect.Contains(x, y))
    {
      mBPM = std::max(volum::MetronomeDSP::kMinBPM, mBPM - 1.f);
      if (mOnBPMChanged)
        mOnBPMChanged(mBPM);
      SetDirty(false);
      return;
    }
    if (mBpmPlusRect.Contains(x, y))
    {
      mBPM = std::min(volum::MetronomeDSP::kMaxBPM, mBPM + 1.f);
      if (mOnBPMChanged)
        mOnBPMChanged(mBPM);
      SetDirty(false);
      return;
    }
    if (mBpmValueRect.Contains(x, y))
    {
      _StartBPMEntry();
      return;
    }

    if (mVolBarRect.Contains(x, y))
    {
      _UpdateVolumeFromX(x);
      mDraggingVolume = true;
      return;
    }

    for (int i = 0; i < static_cast<int>(volum::MetronomeTimeSig::kCount); ++i)
    {
      if (mTimeSigRects[i].Contains(x, y))
      {
        mTimeSig = static_cast<volum::MetronomeTimeSig>(i);
        if (mOnTimeSigChanged)
          mOnTimeSigChanged(mTimeSig);
        SetDirty(false);
        return;
      }
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    (void)y;
    (void)dX;
    (void)dY;
    (void)mod;
    if (mDraggingVolume)
      _UpdateVolumeFromX(x);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    mDraggingVolume = false;
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    (void)x;
    (void)y;
    if (!mHide && key.VK == kVK_ESCAPE)
    {
      Hide(true);
      return true;
    }
    return false;
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    (void)valIdx;
    mEditingBPM = false;

    if (str && str[0])
    {
      char* end = nullptr;
      const float bpm = std::strtof(str, &end);
      if (end != str)
      {
        mBPM = std::clamp(bpm, volum::MetronomeDSP::kMinBPM, volum::MetronomeDSP::kMaxBPM);
        if (mOnBPMChanged)
          mOnBPMChanged(mBPM);
      }
    }

    SetDirty(false);
  }

  void Hide(bool hide) override
  {
    IControl::Hide(hide);
    if (hide)
      mEditingBPM = false;
  }

  void Show(bool active, float bpm, float vol, volum::MetronomeTimeSig sig)
  {
    mActive = active;
    mBPM = bpm;
    mVolume = vol;
    mTimeSig = sig;
    Hide(false);
    SetDirty(false);
  }

  std::function<void(bool)> mOnActiveChanged;
  std::function<void(float)> mOnBPMChanged;
  std::function<void(float)> mOnVolumeChanged;
  std::function<void(volum::MetronomeTimeSig)> mOnTimeSigChanged;

private:
  IRECT _PanelRect() const { return mRECT.GetCentredInside(320.f, 230.f); }

  void _StartBPMEntry()
  {
    if (mHide)
      return;

    auto* ui = GetUI();
    if (!ui)
      return;

    char bpmStr[8];
    snprintf(bpmStr, sizeof(bpmStr), "%.0f", mBPM);
    mEditingBPM = true;
    ui->CreateTextEntry(*this, mBpmTextEntry, mBpmValueRect, bpmStr);
    SetDirty(false);
  }

  void _UpdateVolumeFromX(float x)
  {
    float norm = (x - mVolBarRect.L) / mVolBarRect.W();
    mVolume = std::clamp(norm, 0.f, 1.f);
    if (mOnVolumeChanged)
      mOnVolumeChanged(mVolume);
    SetDirty(false);
  }

  bool mActive = false;
  float mBPM = volum::MetronomeDSP::kDefaultBPM;
  float mVolume = 0.5f;
  volum::MetronomeTimeSig mTimeSig = volum::MetronomeTimeSig::Quarter_4_4;

  IRECT mToggleRect;
  IRECT mBpmMinusRect;
  IRECT mBpmValueRect;
  IRECT mBpmPlusRect;
  IRECT mVolBarRect;
  IRECT mTimeSigRects[static_cast<int>(volum::MetronomeTimeSig::kCount)];
  bool mDraggingVolume = false;
  bool mEditingBPM = false;
  IText mBpmTextEntry;
};
