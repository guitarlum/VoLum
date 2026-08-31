#pragma once

// Settings overlay tab chrome.
//
// The overlay is three tabs, SIGNAL, MIDI and SYSTEM (see NAMSettingsPageControl
// in NeuralAmpModelerControls.h for what lives on each). This header owns the
// pieces that only exist because of that split:
//
// - VoLumSettingsTabStripControl: the segmented tab selector under the title.
// - VoLumMidiChannelControl: the per-instance channel row (Omni or 1-16).
// - VoLumMidiSoundMapControl: the Program Change 0-127 Sound assignment list.
//
// MIDI is a first-class tab, not a card on SIGNAL, and it carries the same Sound
// assignment list PLAY does. One store, two surfaces: both read
// content::Registry::midiSoundMap through volum::BuildPlaySlots and mutate it
// through the plugin's assign/clear path, so neither can hold a private copy.
//
// The SYSTEM tab's Content library row (VoLumSettingsPackRowControl) lives in
// VoLumSettingsOverlay.h, next to the Pack modal it opens.
//
// Split out of VoLumSettingsOverlay.h, which already owns the panel shell.

#include "VoLumColorHelpers.h"
#include "VoLumPlayModel.h"

#include <algorithm>
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
    g.DrawText(dim, "Assign the Sounds below, or in PLAY. MIDI port: File > Preferences.",
               IRECT(textL, mRECT.T + 26.f, mRECT.R, mRECT.T + 41.f));
#else
    g.DrawText(dim, "Assign the Sounds below, or in PLAY. MIDI arrives on this track's input.",
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

/** The Program Change 0-127 Sound assignment list, as Settings shows it.
 *
 * Same store and same rows as the PLAY rail (volum::BuildPlaySlots over
 * content::Registry::midiSoundMap), drawn as a table instead of a stage: a
 * numbered slot, the Sound's preset name, the amp it belongs to. A slot whose
 * Sound is gone keeps its number and reads red, exactly as MIDI treats it - the
 * program still exists, the thing it pointed at does not.
 *
 * Mutations go out through the two callbacks, which the layout wires to the same
 * plugin methods PLAY's Add/Clear use, so there is one writer per store. */
class VoLumMidiSoundMapControl : public IControl
{
public:
  using AssignCallback = std::function<void(int, const volum::SoundChoice&)>;
  using ClearCallback = std::function<void(int)>;

  explicit VoLumMidiSoundMapControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallbacks(AssignCallback assign, ClearCallback clear)
  {
    mAssign = std::move(assign);
    mClear = std::move(clear);
  }

  void SetData(const std::vector<volum::FactoryPreset>& factory, const volum::content::Registry& registry)
  {
    mSlots = volum::BuildPlaySlots(factory, registry);
    mChoices = volum::BuildSoundChoices(factory, registry);
    // A row can vanish under the pointer (cleared here or in PLAY), so the scroll
    // offset and the open picker both have to be re-validated, not trusted.
    mScroll = std::clamp(mScroll, 0.f, MaxScroll());
    if (mPickerOpen && mChoices.empty())
      mPickerOpen = false;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    if (mPickerOpen)
    {
      DrawPicker(g);
      return;
    }

    const IRECT head = HeadRect();
    const IText cap(9.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    g.DrawText(cap, "PC", IRECT(head.L + 6.f, head.T, head.L + 40.f, head.B));
    g.DrawText(cap, "SOUND", IRECT(head.L + 44.f, head.T, head.MW(), head.B));
    g.DrawText(IText(9.f, VoLumColors::TEAL_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "AMP",
               IRECT(head.MW() + 8.f, head.T, head.R - 30.f, head.B));

    const IRECT list = ListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);

    if (mSlots.empty())
    {
      g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                 "No Sounds assigned. Add one to recall it by Program Change.", list);
    }
    else
    {
      g.PathClipRegion(list);
      for (int i = 0; i < static_cast<int>(mSlots.size()); ++i)
      {
        const IRECT row = RowRect(i);
        if (row.B > list.T && row.T < list.B)
          DrawRow(g, row, i, list);
      }
      g.PathClipRegion();

      const float maxScroll = MaxScroll();
      if (maxScroll > 0.5f)
      {
        const IRECT track(list.R - 5.f, list.T + 1.f, list.R - 1.f, list.B - 1.f);
        const float thumbH = std::max(16.f, track.H() * list.H() / ContentH());
        const float top = track.T + (track.H() - thumbH) * (mScroll / maxScroll);
        DrawVoLumScrollbar(g, track, IRECT(track.L, top, track.R, top + thumbH));
      }
    }

    const IRECT add = AddRect();
    const bool hot = mHoverRow == kHoverAdd;
    g.FillRoundRect(hot ? IColor(70, 232, 168, 92) : VoLumColors::BTN_OFF_BG, add, 3.f);
    g.DrawRoundRect(hot ? VoLumColors::AMBER : VoLumColors::FRAME, add, 3.f, nullptr, hot ? 1.3f : 1.f);
    g.DrawText(IText(11.5f, hot ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               "+  Add Sound", add);
    g.DrawText(IText(10.5f, VoLumColors::TEXT_DIM.WithOpacity(0.75f), "Josefin-Sans", EAlign::Near, EVAlign::Middle),
               mChoices.empty()  ? "Save a preset first: a Sound is an amp plus a named preset."
               : mSlots.empty() ? "PLAY shows the same list."
                                : "Click a row to reassign it. PLAY shows the same list.",
               IRECT(add.R + 12.f, add.T, mRECT.R, add.B));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (mPickerOpen)
    {
      if (PickerCloseRect().Contains(x, y))
      {
        mPickerOpen = false;
        SetDirty(false);
        return;
      }
      const int choice = PickerChoiceAt(x, y);
      if (choice >= 0 && choice < static_cast<int>(mChoices.size()) && mAssign)
        mAssign(mEditSlot, mChoices[static_cast<size_t>(choice)]);
      if (choice >= 0)
        mPickerOpen = false;
      SetDirty(false);
      return;
    }

    if (AddRect().Contains(x, y))
    {
      OpenPicker(FirstFreeSlot());
      return;
    }

    const int row = RowAt(x, y);
    if (row < 0)
      return;
    if (ClearRect(RowRect(row)).Contains(x, y))
    {
      if (mClear)
        mClear(mSlots[static_cast<size_t>(row)].slot);
      return;
    }
    OpenPicker(mSlots[static_cast<size_t>(row)].slot);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int choice = mPickerOpen ? PickerChoiceAt(x, y) : -1;
    const int row = mPickerOpen ? -1 : (AddRect().Contains(x, y) ? kHoverAdd : RowAt(x, y));
    if (row == mHoverRow && choice == mHoverChoice)
      return;
    mHoverRow = row;
    mHoverChoice = choice;
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHoverRow = mHoverChoice = -1;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (mPickerOpen)
    {
      if (PickerListRect().Contains(x, y))
        mPickerScroll = std::clamp(mPickerScroll - d * kRowH * 2.f, 0.f, PickerMaxScroll());
    }
    else if (ListRect().Contains(x, y))
      mScroll = std::clamp(mScroll - d * kRowH * 2.f, 0.f, MaxScroll());
    SetDirty(false);
  }

private:
  static constexpr float kRowH = 22.f;
  static constexpr float kHeadH = 14.f;
  static constexpr float kFooterH = 24.f;
  static constexpr int kHoverAdd = -2;

  IRECT HeadRect() const { return IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + kHeadH); }
  IRECT ListRect() const { return IRECT(mRECT.L, mRECT.T + kHeadH, mRECT.R, mRECT.B - kFooterH - 6.f); }
  IRECT AddRect() const { return IRECT(mRECT.L, mRECT.B - kFooterH, mRECT.L + 118.f, mRECT.B); }
  float ContentH() const { return static_cast<float>(mSlots.size()) * kRowH; }
  float MaxScroll() const { return std::max(0.f, ContentH() - ListRect().H() + 2.f); }
  IRECT RowRect(int index) const
  {
    const IRECT list = ListRect();
    const float top = list.T + 1.f + static_cast<float>(index) * kRowH - mScroll;
    return IRECT(list.L + 1.f, top, list.R - (MaxScroll() > 0.5f ? 7.f : 1.f), top + kRowH);
  }
  static IRECT ClearRect(const IRECT& row) { return IRECT(row.R - 22.f, row.MH() - 8.f, row.R - 6.f, row.MH() + 8.f); }
  int RowAt(float x, float y) const
  {
    const IRECT list = ListRect();
    if (!list.Contains(x, y))
      return -1;
    const int row = static_cast<int>((y - list.T - 1.f + mScroll) / kRowH);
    if (row < 0 || row >= static_cast<int>(mSlots.size()))
      return -1;
    return RowRect(row).Contains(x, y) ? row : -1;
  }

  void OpenPicker(int slot)
  {
    if (slot < 0 || mChoices.empty())
      return;
    mEditSlot = slot;
    mPickerOpen = true;
    mPickerScroll = 0.f;
    mHoverChoice = -1;
    SetDirty(false);
  }

  int FirstFreeSlot() const
  {
    bool used[volum::kMidiSoundSlotCount] = {};
    for (const auto& slot : mSlots)
      if (slot.slot >= 0 && slot.slot < volum::kMidiSoundSlotCount)
        used[slot.slot] = true;
    for (int i = 0; i < volum::kMidiSoundSlotCount; ++i)
      if (!used[i])
        return i;
    return -1;
  }

  static std::string TwoDigits(int n)
  {
    if (n < 0)
      return "--";
    return n < 10 ? "0" + std::to_string(n) : std::to_string(n);
  }

  void DrawRow(IGraphics& g, const IRECT& row, int index, const IRECT& clip)
  {
    const auto& slot = mSlots[static_cast<size_t>(index)];
    const bool hovered = mHoverRow == index;
    DrawVoLumSelection(g, row, false, hovered, VoLumSelectionStyle::ListTeal, 2.f, 1.f);

    g.DrawText(IText(12.f, slot.valid ? VoLumColors::GOLD : VoLumColors::DANGER, "Josefin-Bold", EAlign::Near,
                     EVAlign::Middle),
               TwoDigits(slot.slot).c_str(), IRECT(row.L + 6.f, row.T, row.L + 40.f, row.B));

    // Clip each cell to its own column: a long preset or amp name must stop at the
    // next column, not run under it.
    auto cell = [&](const IRECT& r, const IText& t, const char* s) {
      const IRECT c = r.Intersect(clip);
      if (c.W() <= 0.f || c.H() <= 0.f)
        return;
      g.PathClipRegion(c);
      g.DrawText(t, s, r);
      g.PathClipRegion(clip);
    };

    if (slot.valid)
    {
      cell(IRECT(row.L + 44.f, row.T, row.MW(), row.B),
           IText(12.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
           slot.sound.presetName.c_str());
      cell(IRECT(row.MW() + 8.f, row.T, row.R - 26.f, row.B),
           IText(11.f, VoLumColors::TEAL_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
           slot.sound.ampName.c_str());
    }
    else
    {
      // Same words as the PLAY rail: one state, one name.
      cell(IRECT(row.L + 44.f, row.T, row.R - 26.f, row.B),
           IText(11.f, VoLumColors::DANGER, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "MISSING SOUND");
    }

    if (hovered)
    {
      const IRECT clear = ClearRect(row);
      g.FillRoundRect(VoLumColors::DANGER_FILL, clear, 2.f);
      g.DrawRoundRect(VoLumColors::DANGER, clear, 2.f);
      DrawCrossGlyph(g, clear, VoLumColors::TEXT_BRIGHT);
    }
  }

  // The picker takes the whole card body rather than floating a small panel: this
  // control is a child of the Settings container, so a popup that escaped the card
  // would be clipped by nothing and overlap the channel row above it.
  IRECT PickerListRect() const { return IRECT(mRECT.L, mRECT.T + 26.f, mRECT.R, mRECT.B); }
  IRECT PickerCloseRect() const { return IRECT(mRECT.R - 22.f, mRECT.T, mRECT.R, mRECT.T + 22.f); }
  float PickerContentH() const { return static_cast<float>(mChoices.size()) * kRowH; }
  float PickerMaxScroll() const { return std::max(0.f, PickerContentH() - PickerListRect().H() + 2.f); }
  IRECT PickerRowRect(int index) const
  {
    const IRECT list = PickerListRect();
    const float top = list.T + 1.f + static_cast<float>(index) * kRowH - mPickerScroll;
    return IRECT(list.L + 1.f, top, list.R - (PickerMaxScroll() > 0.5f ? 7.f : 1.f), top + kRowH);
  }
  int PickerChoiceAt(float x, float y) const
  {
    const IRECT list = PickerListRect();
    if (!list.Contains(x, y))
      return -1;
    const int row = static_cast<int>((y - list.T - 1.f + mPickerScroll) / kRowH);
    if (row < 0 || row >= static_cast<int>(mChoices.size()))
      return -1;
    return PickerRowRect(row).Contains(x, y) ? row : -1;
  }

  void DrawPicker(IGraphics& g)
  {
    g.DrawText(IText(12.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
               ("Sound for Program Change " + TwoDigits(mEditSlot)).c_str(),
               IRECT(mRECT.L + 6.f, mRECT.T, mRECT.R - 30.f, mRECT.T + 22.f));
    DrawCrossGlyph(g, PickerCloseRect(), VoLumColors::GOLD_DIM, 1.5f);

    const IRECT list = PickerListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);
    g.PathClipRegion(list);
    for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
    {
      const IRECT row = PickerRowRect(i);
      if (row.B <= list.T || row.T >= list.B)
        continue;
      DrawVoLumSelection(g, row, false, mHoverChoice == i, VoLumSelectionStyle::ListTeal, 2.f, 1.f);
      const auto& choice = mChoices[static_cast<size_t>(i)];
      g.DrawText(IText(12.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
                 choice.presetName.c_str(), IRECT(row.L + 8.f, row.T, row.MW(), row.B));
      g.DrawText(IText(11.f, choice.factory ? VoLumColors::GOLD_DIM : VoLumColors::TEAL_DIM, "Josefin-Sans",
                       EAlign::Near, EVAlign::Middle),
                 choice.ampName.c_str(), IRECT(row.MW() + 8.f, row.T, row.R - 8.f, row.B));
    }
    g.PathClipRegion();

    const float maxScroll = PickerMaxScroll();
    if (maxScroll > 0.5f)
    {
      const IRECT track(list.R - 5.f, list.T + 1.f, list.R - 1.f, list.B - 1.f);
      const float thumbH = std::max(16.f, track.H() * list.H() / PickerContentH());
      const float top = track.T + (track.H() - thumbH) * (mPickerScroll / maxScroll);
      DrawVoLumScrollbar(g, track, IRECT(track.L, top, track.R, top + thumbH));
    }
  }

  std::vector<volum::PlaySlot> mSlots;
  std::vector<volum::SoundChoice> mChoices;
  int mEditSlot = -1;
  int mHoverRow = -1;
  int mHoverChoice = -1;
  bool mPickerOpen = false;
  float mScroll = 0.f;
  float mPickerScroll = 0.f;
  AssignCallback mAssign;
  ClearCallback mClear;
};
