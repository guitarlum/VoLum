#pragma once

// The Sound picker the Settings MIDI footswitch view opens on a switch: a
// FACTORY / USER grouped list of every Sound, scrollable like every other VoLum
// list. It is a panel, not a control: VoLumMidiFootswitchControl owns one,
// binds it to its own bounds and forwards the mouse while it is up, and does
// the assignment itself when the panel reports a pick.
//
// Full-body rather than a floating panel: the footswitch view is a child of the
// Settings container, so a floating panel would overlap the listen filter card
// above it with nothing to clip it.

#include "VoLumColorHelpers.h"
#include "VoLumPlayModel.h"
#include "VoLumScroll.h"

#include <algorithm>
#include <string>
#include <vector>

class VoLumSoundPickerPanel
{
public:
  static constexpr float kRowH = 22.f;
  static constexpr int kNothing = -1;
  static constexpr int kClose = -2;

  // Every other call reads these, so the owner re-binds whenever its bounds or
  // its Sound list may have changed.
  void Bind(const IRECT& bounds, const std::vector<volum::SoundChoice>* choices, volum::PickerGroupSession* groups)
  {
    mRECT = bounds;
    mChoices = choices;
    mGroups = groups;
  }

  void Open()
  {
    mScroll = 0.f;
    Reset();
    if (!mGroups || !mChoices)
      return;
    bool hasFactory = false, hasUser = false;
    for (const auto& c : *mChoices)
      c.factory ? hasFactory = true : hasUser = true;
    volum::InitPickerGroups(*mGroups, hasFactory, hasUser);
  }

  void Reset()
  {
    mHoverChoice = -1;
    mHoverHeader = 0;
    mHoverClose = false;
    mBar.OnUp();
  }

  // kClose for the cross, a choice index for a picked Sound, kNothing otherwise
  // (scrollbar grab, group toggle, a miss).
  int OnMouseDown(float x, float y)
  {
    if (CloseRect().Contains(x, y))
      return kClose;
    const auto scroll = ScrollMetrics();
    const IRECT track = TrackRect();
    if (mBar.OnDown(x, y, track.L, track.R, scroll))
    {
      mScroll =
        volum::scroll::ThumbYToScroll(y - mBar.grabDY, scroll.trackTop, scroll.trackH, scroll.thumbH, scroll.maxScroll);
      return kNothing;
    }
    const int header = HeaderAt(x, y);
    if (header != 0 && mGroups)
    {
      volum::TogglePickerGroup(*mGroups, header > 0);
      return kNothing;
    }
    const int choice = ChoiceAt(x, y);
    return choice >= 0 && mChoices && choice < static_cast<int>(mChoices->size()) ? choice : kNothing;
  }

  // True while the scrollbar thumb is being dragged.
  bool OnMouseDrag(float y)
  {
    if (!mBar.dragging)
      return false;
    const float next = mBar.OnDrag(y, ScrollMetrics());
    if (next >= 0.f)
      mScroll = next;
    return true;
  }

  void OnMouseUp() { mBar.OnUp(); }

  // True when the hover changed and the owner should redraw.
  bool OnMouseOver(float x, float y)
  {
    const int choice = ChoiceAt(x, y);
    const int header = HeaderAt(x, y);
    const bool close = CloseRect().Contains(x, y);
    if (choice == mHoverChoice && header == mHoverHeader && close == mHoverClose)
      return false;
    mHoverChoice = choice;
    mHoverHeader = header;
    mHoverClose = close;
    return true;
  }

  void OnMouseOut()
  {
    mHoverChoice = -1;
    mHoverHeader = 0;
    mHoverClose = false;
  }

  void OnMouseWheel(float x, float y, float d)
  {
    if (ListRect().Contains(x, y))
      mScroll = volum::scroll::ClampScroll(mScroll + volum::scroll::ListWheelDelta(d, kRowH), MaxScroll());
  }

  // `current` is the Sound the program already plays (null for none); its row
  // reads as the selected one.
  void Draw(IGraphics& g, const std::string& title, const std::string& where, const volum::SoundChoice* current) const
  {
    const IRECT head(mRECT.L + 6.f, mRECT.T, mRECT.R - 30.f, mRECT.T + 22.f);
    g.DrawText(IText(12.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle), title.c_str(), head);
    g.DrawText(VoLumType::Body(11.f, VoLumColors::CREAM_DIM, EAlign::Far), where.c_str(), head);
    DrawCrossGlyph(g, CloseRect(), mHoverClose ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, 1.5f);

    const IRECT list = ListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);
    if (!mChoices)
      return;
    g.PathClipRegion(list);
    WalkRows([&](int kind, int choice, const IRECT& row) {
      if (row.B <= list.T || row.T >= list.B)
        return;
      if (kind != 0)
      {
        const bool factory = kind > 0;
        const bool open = GroupOpen(factory);
        DrawVoLumSelection(g, row, false, mHoverHeader == kind, VoLumSelectionStyle::ListTeal, 2.f, 1.f);
        g.DrawText(IText(12.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
                   volum::PickerGroupGlyph(open),
                   IRECT(row.L + 6.f, row.T, row.L + 6.f + volum::kPickerGroupMarkW, row.B));
        g.DrawText(IText(10.f, factory ? VoLumColors::GOLD_DIM : VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near,
                         EVAlign::Middle),
                   volum::PickerGroupTitle(factory),
                   IRECT(row.L + 6.f + volum::kPickerGroupMarkW, row.T, row.R, row.B));
        return;
      }
      const auto& c = (*mChoices)[static_cast<size_t>(choice)];
      const bool active = current && current->ampId == c.ampId && current->presetId == c.presetId;
      DrawVoLumSelection(g, row, active, mHoverChoice == choice, VoLumSelectionStyle::ListTeal, 2.f, 1.f);
      g.DrawText(IText(12.f, SelectionInkColor(VoLumSelectionStyle::ListTeal, active).WithOpacity(active ? 1.f : 0.93f),
                       "Josefin-Sans", EAlign::Near, EVAlign::Middle),
                 c.presetName.c_str(), IRECT(row.L + 8.f, row.T, row.MW(), row.B));
      g.DrawText(IText(11.f, c.factory ? VoLumColors::GOLD_DIM : VoLumColors::TEAL_DIM, "Josefin-Sans", EAlign::Near,
                       EVAlign::Middle),
                 c.ampName.c_str(), IRECT(row.MW() + 8.f, row.T, row.R - 8.f, row.B));
    });
    g.PathClipRegion();

    const auto scroll = ScrollMetrics();
    if (scroll.maxScroll > 0.5f)
    {
      const IRECT track = TrackRect();
      DrawVoLumScrollbar(
        g, track, IRECT(track.L, scroll.thumbY, track.R, scroll.thumbY + scroll.thumbH), mBar.dragging);
    }
  }

private:
  IRECT ListRect() const { return IRECT(mRECT.L, mRECT.T + 26.f, mRECT.R, mRECT.B); }
  IRECT CloseRect() const { return IRECT(mRECT.R - 22.f, mRECT.T, mRECT.R, mRECT.T + 22.f); }
  IRECT TrackRect() const { return VoLumScrollTrackRect(ListRect()); }
  volum::scroll::ScrollMetrics ScrollMetrics() const
  {
    const IRECT list = ListRect();
    return volum::scroll::ComputeScroll(list.T, list.B, list.H(), ContentH(), mScroll);
  }

  bool GroupOpen(bool factory) const
  {
    if (!mGroups)
      return true;
    return factory ? mGroups->factoryOpen : mGroups->userOpen;
  }
  bool HasGroup(bool factory) const
  {
    if (!mChoices)
      return false;
    for (const auto& c : *mChoices)
      if (c.factory == factory)
        return true;
    return false;
  }

  // kind 1 / -1 is the FACTORY / USER heading, 0 a Sound row with its choice index.
  template <typename Fn>
  void WalkRows(Fn&& fn) const
  {
    if (!mChoices)
      return;
    const IRECT list = ListRect();
    float y = list.T + 1.f - mScroll;
    for (const bool factory : {true, false})
    {
      if (!HasGroup(factory))
        continue;
      fn(factory ? 1 : -1, -1, IRECT(list.L + 1.f, y, list.R - 7.f, y + kRowH));
      y += kRowH;
      if (!GroupOpen(factory))
        continue;
      for (int i = 0; i < static_cast<int>(mChoices->size()); ++i)
      {
        if ((*mChoices)[static_cast<size_t>(i)].factory != factory)
          continue;
        fn(0, i, IRECT(list.L + 1.f, y, list.R - 7.f, y + kRowH));
        y += kRowH;
      }
    }
  }

  float ContentH() const
  {
    float h = 0.f;
    WalkRows([&](int, int, const IRECT&) { h += kRowH; });
    return h;
  }
  float MaxScroll() const { return std::max(0.f, ContentH() - ListRect().H() + 2.f); }

  int HeaderAt(float x, float y) const
  {
    if (!ListRect().Contains(x, y))
      return 0;
    int found = 0;
    WalkRows([&](int kind, int, const IRECT& row) {
      if (found == 0 && kind != 0 && row.Contains(x, y))
        found = kind;
    });
    return found;
  }
  int ChoiceAt(float x, float y) const
  {
    if (!ListRect().Contains(x, y))
      return -1;
    int found = -1;
    WalkRows([&](int kind, int choice, const IRECT& row) {
      if (found < 0 && kind == 0 && row.Contains(x, y))
        found = choice;
    });
    return found;
  }

  IRECT mRECT;
  const std::vector<volum::SoundChoice>* mChoices = nullptr;
  volum::PickerGroupSession* mGroups = nullptr;
  float mScroll = 0.f;
  int mHoverChoice = -1;
  int mHoverHeader = 0;
  bool mHoverClose = false;
  volum::scroll::Interaction mBar;
};
