#pragma once

// Settings overlay tab chrome.
//
// The overlay is three tabs, SIGNAL, MIDI and SYSTEM (see NAMSettingsPageControl
// in NeuralAmpModelerControls.h for what lives on each). This header owns the
// pieces that only exist because of that split:
//
// - VoLumSettingsTabStripControl: the segmented tab selector under the title.
// - VoLumMidiChannelControl: the per-instance listen filter (all channels, or one).
// - VoLumMidiRecallCcControl: the per-instance Sound-recall CC (value = program).
// - VoLumAnimateArtSwitchControl: SIGNAL's "Animate art in PLAY" switch.
//
// The tab's third piece, the program number 0-127 Sound assignments drawn as a
// footswitch bank view, is VoLumMidiFootswitchControl in VoLumMidiFootswitch.h.
//
// Two different MIDI numbers meet on this tab and a guitarist has no reason to
// know which is which, so the copy never lets them share a word: the listen
// filter is the *channel* (one cable, up to sixteen conversations), and the
// switches of the assignment view are *program numbers* (what a footswitch
// sends). "Omni" appears once, as a parenthetical for players who already know
// the term.
//
// MIDI is a first-class tab, not a card on SIGNAL, and it carries the same Sound
// assignments PLAY does. One store, two surfaces: both read
// content::Registry::midiSoundMap through volum::BuildPlaySlots and mutate it
// through the plugin's assign/clear/swap path, so neither can hold a private copy.
//
// The SYSTEM tab's library backup row (VoLumSettingsPackRowControl) lives in
// VoLumSettingsOverlay.h, next to the Pack modal it opens.
//
// Split out of VoLumSettingsOverlay.h, which already owns the panel shell.

#include "VoLumColorHelpers.h"
#include "VoLumMidi.h"
#include "VoLumSecondPress.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/** Segmented tab selector. Selection goes through the shared SSOT so the strip
 * reads like every other VoLum picker (solid amber, dark ink). */
class VoLumSettingsTabStripControl : public IControl
{
public:
  using Callback = std::function<void(int)>;

  VoLumSettingsTabStripControl(const IRECT& bounds, std::vector<std::string> labels, Callback callback)
  : IControl(bounds)
  , mLabels(std::move(labels))
  , mCallback(std::move(callback))
  {
    mIgnoreMouse = false;
  }

  void SetActive(int index)
  {
    mActive = index;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT strip = StripRect();
    g.FillRoundRect(IColor(255, 9, 12, 17), strip, 3.f);
    for (int i = 0; i < TabCount(); ++i)
    {
      const IRECT tab = TabRect(i);
      const bool active = i == mActive;
      DrawVoLumSelection(g, tab, active, i == mHover && !active, VoLumSelectionStyle::AmberPicker, 2.f, 1.f);
      g.DrawText(IText(14.f, SelectionInkColor(VoLumSelectionStyle::AmberPicker, active), "Josefin-Bold",
                       EAlign::Center, EVAlign::Middle),
                 mLabels[static_cast<size_t>(i)].c_str(), tab);
      if (i > 0)
        g.DrawLine(VoLumColors::FRAME, tab.L, strip.T + 5.f, tab.L, strip.B - 5.f);
    }
    g.DrawRoundRect(VoLumColors::GOLD_DIM, strip, 3.f);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const auto pressed = mSecondPress.Press();
    const int tab = TabAt(x, y);
    if (tab < 0 || tab == mActive)
      return;
    mActive = tab;
    if (mCallback)
      mCallback(tab);
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mSecondPress.Take())
      OnMouseDown(x, y, mod);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int tab = TabAt(x, y);
    if (tab == mHover)
      return;
    mHover = tab;
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHover = -1;
    SetDirty(false);
  }

private:
  int TabCount() const { return static_cast<int>(mLabels.size()); }

  // Sized to content, centred: tabs stretched across the full 712 px read as a
  // toolbar, not as a choice between pages.
  IRECT StripRect() const
  {
    const float w = std::min(mRECT.W(), 150.f * static_cast<float>(std::max(1, TabCount())));
    return IRECT(mRECT.MW() - w * 0.5f, mRECT.T, mRECT.MW() + w * 0.5f, mRECT.B);
  }

  IRECT TabRect(int index) const
  {
    const IRECT strip = StripRect();
    const float w = strip.W() / static_cast<float>(std::max(1, TabCount()));
    return IRECT(
      strip.L + static_cast<float>(index) * w, strip.T, strip.L + static_cast<float>(index + 1) * w, strip.B);
  }

  int TabAt(float x, float y) const
  {
    for (int i = 0; i < TabCount(); ++i)
      if (TabRect(i).Contains(x, y))
        return i;
    return -1;
  }

  std::vector<std::string> mLabels;
  int mActive = 0;
  int mHover = -1;
  Callback mCallback;
  volum::ui::SecondPressGate mSecondPress;
};

/** SIGNAL > Performance: "Animate art in PLAY" as OFF | ON, under Lite.
 *
 * Machine-global, like Lite; the owner hands in the getter and setter so this
 * header needs no plugin type. Off keeps every PLAY art still (lowest CPU). */
class VoLumAnimateArtSwitchControl : public IControl
{
public:
  using Getter = std::function<bool()>;
  using Setter = std::function<void(bool)>;

  VoLumAnimateArtSwitchControl(const IRECT& bounds, Getter get, Setter set)
  : IControl(bounds)
  , mGet(std::move(get))
  , mSet(std::move(set))
  {
    SetTooltip("Moves the amp art while you play. Off = still art, lowest CPU.");
  }

  void Draw(IGraphics& g) override
  {
    const bool on = IsOn();
    g.DrawText(IText(12.f, VoLumColors::TEXT_BRIGHT, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               "Animate art in PLAY", mRECT.GetFromTop(kLabelH));
    const IRECT seg = SegmentTrack();
    DrawVoLumSegmentSwitch(g, seg, on);
    const IText onText(12.f, VoLumColors::SEL_TEXT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const IText offText(
      12.f, VoLumColors::TEXT_DIM.WithOpacity(0.55f), "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(on ? offText : onText, "OFF", seg.GetFromLeft(seg.W() * 0.5f));
    g.DrawText(on ? onText : offText, "ON", seg.GetFromRight(seg.W() * 0.5f));
    if (mMouseIsOver)
      g.FillRoundRect(COLOR_WHITE.WithOpacity(0.06f), seg, seg.H() * 0.5f);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const auto pressed = mSecondPress.Press();
    const IRECT seg = SegmentTrack();
    if (!seg.Contains(x, y))
      return;
    if (mSet)
      mSet(x >= seg.MW());
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mSecondPress.Take())
      OnMouseDown(x, y, mod);
  }

private:
  static constexpr float kLabelH = 18.f;
  static constexpr float kSegH = 26.f;

  bool IsOn() const { return mGet ? mGet() : true; }

  IRECT SegmentTrack() const
  {
    const float segW = std::min(176.f, mRECT.W());
    return IRECT(
      mRECT.MW() - segW * 0.5f, mRECT.T + kLabelH + 2.f, mRECT.MW() + segW * 0.5f, mRECT.T + kLabelH + 2.f + kSegH);
  }

  Getter mGet;
  Setter mSet;
  volum::ui::SecondPressGate mSecondPress;
};

/** The per-instance listen filter: all MIDI channels, or exactly one of 1-16.
 *
 * Stored as the same 0 = all, 1-16 = one integer the id tail has always carried;
 * only the wording changed. The old row was captioned CHANNEL and showed "Omni",
 * which asks the player to know a MIDI term before they can tell whether the
 * setting concerns them at all. It is now a two-button choice whose default
 * states what it does, with the term kept as a parenthetical for the players who
 * do know it, and the two situations that actually decide the answer spelled out
 * beside it. */
class VoLumMidiChannelControl : public IControl
{
public:
  using ChannelCallback = std::function<void(int)>;

  explicit VoLumMidiChannelControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallback(ChannelCallback callback) { mCallback = std::move(callback); }

  void SetChannel(int channel)
  {
    mChannel = std::clamp(channel, 0, volum::kMidiChannelCount);
    if (mChannel > 0)
      mLastOne = mChannel;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const bool all = mChannel == 0;
    const IRECT pill = PillRect();
    DrawVoLumSegmentSwitch(g, pill, !all);
    const IText onText(12.f, VoLumColors::SEL_TEXT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const IText offText(
      12.f, VoLumColors::TEXT_DIM.WithOpacity(0.55f), "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(all ? onText : offText, "All channels", AllRect());
    g.DrawText(all ? offText : onText, "One channel", OneRect());

    if (!all)
    {
      const IRECT step = StepperRect();
      DrawInsetWell(g, step, step.H() * 0.5f);
      // Byte escapes, not \u: the narrow-literal execution charset here is not
      // UTF-8, so a \u2039 single-quote glyph came out as one invalid byte and the
      // arrows vanished from the stepper.
      g.DrawText(IText(15.f, mHover == kHoverDown ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                       EAlign::Center, EVAlign::Middle),
                 "\xE2\x80\xB9", step.GetFromLeft(22.f));
      g.DrawText(VoLumType::Value(13.f, VoLumColors::GOLD), ("CH " + std::to_string(mChannel)).c_str(), step);
      g.DrawText(IText(15.f, mHover == kHoverUp ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                       EAlign::Center, EVAlign::Middle),
                 "\xE2\x80\xBA", step.GetFromRight(22.f));
    }

    const IText body(11.5f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    const IText dim(10.5f, VoLumColors::TEXT_DIM.WithOpacity(0.75f), "Josefin-Sans", EAlign::Near, EVAlign::Middle);

    g.DrawText(dim, all ? "MIDI calls this Omni." : "Other channels are ignored.",
               IRECT(mRECT.L, mRECT.T + 30.f, mRECT.L + 200.f, mRECT.T + 44.f));

    g.DrawText(body, "One guitarist, one board: leave this on All channels.",
               IRECT(mRECT.L, mRECT.T + 44.f, mRECT.R, mRECT.T + 58.f));
    g.DrawText(body, "Two VoLums on one MIDI cable: give each its own channel.",
               IRECT(mRECT.L, mRECT.T + 58.f, mRECT.R, mRECT.T + 72.f));
#if defined(APP_API)
    g.DrawText(
      dim, "Pick the MIDI port under File > Preferences.", IRECT(mRECT.L, mRECT.T + 72.f, mRECT.R, mRECT.T + 86.f));
#else
    g.DrawText(dim, "MIDI arrives on this track's input.", IRECT(mRECT.L, mRECT.T + 72.f, mRECT.R, mRECT.T + 86.f));
#endif
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const auto pressed = mSecondPress.Press();
    if (AllRect().Contains(x, y))
    {
      Commit(0);
      return;
    }
    if (OneRect().Contains(x, y))
    {
      if (mChannel == 0)
        Commit(mLastOne);
      return;
    }
    if (mChannel == 0)
      return;
    const IRECT step = StepperRect();
    if (!step.Contains(x, y))
      return;
    if (x < step.L + 22.f)
      Commit(mChannel == 1 ? volum::kMidiChannelCount : mChannel - 1);
    else if (x > step.R - 22.f)
      Commit(mChannel == volum::kMidiChannelCount ? 1 : mChannel + 1);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mSecondPress.Take())
      OnMouseDown(x, y, mod);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int hover = HoverAt(x, y);
    if (hover == mHover)
      return;
    mHover = hover;
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHover = kHoverNone;
    SetDirty(false);
  }

private:
  static constexpr float kPillW = 200.f;
  static constexpr float kPillH = 26.f;
  static constexpr int kHoverNone = 0;
  static constexpr int kHoverAll = 1;
  static constexpr int kHoverOne = 2;
  static constexpr int kHoverDown = 3;
  static constexpr int kHoverUp = 4;

  IRECT PillRect() const { return IRECT(mRECT.L, mRECT.T, mRECT.L + kPillW, mRECT.T + kPillH); }
  IRECT AllRect() const { return PillRect().GetFromLeft(kPillW * 0.5f); }
  IRECT OneRect() const { return PillRect().GetFromRight(kPillW * 0.5f); }
  IRECT StepperRect() const
  {
    const IRECT pill = PillRect();
    return IRECT(pill.R + 12.f, pill.T, pill.R + 108.f, pill.B);
  }

  int HoverAt(float x, float y) const
  {
    if (AllRect().Contains(x, y))
      return kHoverAll;
    if (OneRect().Contains(x, y))
      return kHoverOne;
    if (mChannel == 0)
      return kHoverNone;
    const IRECT step = StepperRect();
    if (!step.Contains(x, y))
      return kHoverNone;
    if (x < step.L + 22.f)
      return kHoverDown;
    if (x > step.R - 22.f)
      return kHoverUp;
    return kHoverNone;
  }

  void Commit(int channel)
  {
    mChannel = std::clamp(channel, 0, volum::kMidiChannelCount);
    if (mChannel > 0)
      mLastOne = mChannel;
    if (mCallback)
      mCallback(mChannel);
    SetDirty(false);
  }

  int mChannel = 0;
  int mLastOne = 1;
  int mHover = kHoverNone;
  ChannelCallback mCallback;
  volum::ui::SecondPressGate mSecondPress;
};

/** The per-instance Sound-recall CC: value 0-127 is the program number.
 *
 * Default 102 (MIDI-undefined). 120-127 are channel-mode messages and cannot be
 * chosen here: hosts eat them, so they never reach the plugin. Same stepper and
 * commit-callback shape as the listen filter beside it. */
class VoLumMidiRecallCcControl : public IControl
{
public:
  using CcCallback = std::function<void(int)>;

  explicit VoLumMidiRecallCcControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallback(CcCallback callback) { mCallback = std::move(callback); }

  void SetCc(int cc)
  {
    mCc = volum::ClampMidiRecallCc(cc);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.DrawText(IText(12.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "Recall CC", LabelRect());

    const IRECT step = StepperRect();
    DrawInsetWell(g, step, step.H() * 0.5f);
    g.DrawText(IText(15.f, mHover == kHoverDown ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                     EAlign::Center, EVAlign::Middle),
               "\xE2\x80\xB9", step.GetFromLeft(22.f));
    g.DrawText(VoLumType::Value(13.f, VoLumColors::GOLD), std::to_string(mCc).c_str(), step);
    g.DrawText(IText(15.f, mHover == kHoverUp ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                     EAlign::Center, EVAlign::Middle),
               "\xE2\x80\xBA", step.GetFromRight(22.f));

    const IText body(11.5f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    g.DrawText(body, "Value is the program number.", IRECT(mRECT.L, mRECT.T + 30.f, mRECT.R, mRECT.T + 44.f));
    g.DrawText(
      body, "Use this when Program Change never arrives.", IRECT(mRECT.L, mRECT.T + 44.f, mRECT.R, mRECT.T + 58.f));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const auto pressed = mSecondPress.Press();
    const IRECT step = StepperRect();
    if (!step.Contains(x, y))
      return;
    if (x < step.L + 22.f)
      Commit(mCc == volum::kMidiRecallCcMin ? volum::kMidiRecallCcMax : mCc - 1);
    else if (x > step.R - 22.f)
      Commit(mCc == volum::kMidiRecallCcMax ? volum::kMidiRecallCcMin : mCc + 1);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mSecondPress.Take())
      OnMouseDown(x, y, mod);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (std::abs(d) < 0.01f || !StepperRect().Contains(x, y))
      return;
    const int step = d > 0.f ? 1 : -1;
    int next = mCc + step;
    if (next < volum::kMidiRecallCcMin)
      next = volum::kMidiRecallCcMax;
    else if (next > volum::kMidiRecallCcMax)
      next = volum::kMidiRecallCcMin;
    Commit(next);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int hover = HoverAt(x, y);
    if (hover == mHover)
      return;
    mHover = hover;
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHover = kHoverNone;
    SetDirty(false);
  }

private:
  static constexpr float kStepperW = 96.f;
  static constexpr float kStepperH = 26.f;
  static constexpr int kHoverNone = 0;
  static constexpr int kHoverDown = 1;
  static constexpr int kHoverUp = 2;

  IRECT LabelRect() const { return IRECT(mRECT.L, mRECT.T, mRECT.L + 88.f, mRECT.T + kStepperH); }
  IRECT StepperRect() const
  {
    const float l = LabelRect().R + 8.f;
    return IRECT(l, mRECT.T, l + kStepperW, mRECT.T + kStepperH);
  }

  int HoverAt(float x, float y) const
  {
    const IRECT step = StepperRect();
    if (!step.Contains(x, y))
      return kHoverNone;
    if (x < step.L + 22.f)
      return kHoverDown;
    if (x > step.R - 22.f)
      return kHoverUp;
    return kHoverNone;
  }

  void Commit(int cc)
  {
    mCc = volum::ClampMidiRecallCc(cc);
    if (mCallback)
      mCallback(mCc);
    SetDirty(false);
  }

  int mCc = volum::kMidiRecallCcDefault;
  int mHover = kHoverNone;
  CcCallback mCallback;
  volum::ui::SecondPressGate mSecondPress;
};
