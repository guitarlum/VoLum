#pragma once

// Settings overlay tab chrome.
//
// The overlay is two tabs, SIGNAL and SYSTEM (see NAMSettingsPageControl in
// NeuralAmpModelerControls.h for what lives on each). This header owns the
// pieces that only exist because of that split:
//
// - VoLumSettingsTabStripControl: the segmented tab selector under the title.
// - VoLumMidiChannelControl: channel-only MIDI row. The Sound assignment list
//   lives in PLAY (.scratch/midi-control/spec.md); Settings keeps the channel.
//
// The SYSTEM tab's Content library row (VoLumSettingsPackRowControl) lives in
// VoLumSettingsOverlay.h, next to the Pack modal it opens.
//
// Split out of VoLumSettingsOverlay.h, which already owns the panel shell.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <functional>
#include <string>
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
    const int tab = TabAt(x, y);
    if (tab < 0 || tab == mActive)
      return;
    mActive = tab;
    if (mCallback)
      mCallback(tab);
    SetDirty(false);
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

  // Sized to content, centred: two tabs stretched across 712 px read as a
  // toolbar, not as a choice between two pages.
  IRECT StripRect() const
  {
    const float w = std::min(mRECT.W(), 150.f * static_cast<float>(std::max(1, TabCount())));
    return IRECT(mRECT.MW() - w * 0.5f, mRECT.T, mRECT.MW() + w * 0.5f, mRECT.B);
  }

  IRECT TabRect(int index) const
  {
    const IRECT strip = StripRect();
    const float w = strip.W() / static_cast<float>(std::max(1, TabCount()));
    return IRECT(strip.L + static_cast<float>(index) * w, strip.T, strip.L + static_cast<float>(index + 1) * w,
                 strip.B);
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
};

/** Per-instance MIDI channel: Omni or 1-16, plus the two lines that explain
 * where the Sound list and the port picker actually live. */
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
    mChannel = std::clamp(channel, 0, 16);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.DrawText(IText(9.f, VoLumColors::TEXT_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "CHANNEL",
               IRECT(mRECT.L, mRECT.T, mRECT.L + kStepperW, mRECT.T + 12.f));

    const IRECT channel = ChannelRect();
    g.FillRoundRect(VoLumColors::BTN_OFF_BG, channel, 3.f);
    g.DrawRoundRect(mHover != kHoverNone ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, channel, 3.f);
    const std::string channelText = mChannel == 0 ? "Omni" : "Ch " + std::to_string(mChannel);
    g.DrawText(IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               channelText.c_str(), channel);
    // Byte escapes, not \u: the narrow-literal execution charset here is not
    // UTF-8, so a \u2039 single-quote glyph came out as one invalid byte and the
    // arrows vanished from the stepper.
    g.DrawText(IText(13.f, mHover == kHoverDown ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                     EAlign::Center, EVAlign::Middle),
               "\xE2\x80\xB9", channel.GetFromLeft(18.f));
    g.DrawText(IText(13.f, mHover == kHoverUp ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                     EAlign::Center, EVAlign::Middle),
               "\xE2\x80\xBA", channel.GetFromRight(18.f));

    const IText body(11.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    const IText dim(11.f, VoLumColors::TEXT_DIM.WithOpacity(0.72f), "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    const float textL = mRECT.L + kStepperW + 24.f;
    g.DrawText(body, "Program Change recalls a Sound on this channel. Omni listens to all 16.",
               IRECT(textL, mRECT.T + 11.f, mRECT.R, mRECT.T + 26.f));
#if defined(APP_API)
    g.DrawText(dim, "Assign Sounds to program numbers in PLAY. MIDI port: File > Preferences.",
               IRECT(textL, mRECT.T + 26.f, mRECT.R, mRECT.T + 41.f));
#else
    g.DrawText(dim, "Assign Sounds to program numbers in PLAY. MIDI arrives on this track's input.",
               IRECT(textL, mRECT.T + 26.f, mRECT.R, mRECT.T + 41.f));
#endif
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const IRECT channel = ChannelRect();
    if (!channel.Contains(x, y))
      return;
    mChannel = (mChannel + (x < channel.MW() ? -1 : 1) + 17) % 17;
    if (mCallback)
      mCallback(mChannel);
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const IRECT channel = ChannelRect();
    const int hover = !channel.Contains(x, y) ? kHoverNone : (x < channel.MW() ? kHoverDown : kHoverUp);
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
  static constexpr float kStepperW = 116.f;
  static constexpr int kHoverNone = 0;
  static constexpr int kHoverDown = 1;
  static constexpr int kHoverUp = 2;

  // Anchored to the top, not the middle: the caption sits above it and the two
  // help lines beside it, so the whole row has to read as one band.
  IRECT ChannelRect() const { return IRECT(mRECT.L, mRECT.T + 14.f, mRECT.L + kStepperW, mRECT.T + 40.f); }

  int mChannel = 0;
  int mHover = kHoverNone;
  ChannelCallback mCallback;
};
