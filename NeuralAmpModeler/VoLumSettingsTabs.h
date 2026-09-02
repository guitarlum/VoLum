#pragma once

// Settings overlay tab chrome.
//
// The overlay is three tabs, SIGNAL, MIDI and SYSTEM (see NAMSettingsPageControl
// in NeuralAmpModelerControls.h for what lives on each). This header owns the
// pieces that only exist because of that split:
//
// - VoLumSettingsTabStripControl: the segmented tab selector under the title.
// - VoLumMidiChannelControl: the per-instance listen filter (all channels, or one).
// - VoLumMidiSoundMapControl: the program number 0-127 Sound assignment list.
//
// Two different MIDI numbers meet on this tab and a guitarist has no reason to
// know which is which, so the copy never lets them share a word: the listen
// filter is the *channel* (one cable, up to sixteen conversations), and the rows
// of the assignment list are *program numbers* (what a footswitch sends). "Omni"
// appears once, as a parenthetical for players who already know the term.
//
// MIDI is a first-class tab, not a card on SIGNAL, and it carries the same Sound
// assignment list PLAY does. One store, two surfaces: both read
// content::Registry::midiSoundMap through volum::BuildPlaySlots and mutate it
// through the plugin's assign/clear/swap path, so neither can hold a private copy.
//
// The SYSTEM tab's Content library row (VoLumSettingsPackRowControl) lives in
// VoLumSettingsOverlay.h, next to the Pack modal it opens.
//
// Split out of VoLumSettingsOverlay.h, which already owns the panel shell.

#include "VoLumColorHelpers.h"
#include "VoLumNumericEntry.h"
#include "VoLumPlayModel.h"
#include "VoLumScroll.h"

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

    const IRECT allR = AllRect();
    DrawVoLumSelection(g, allR, all, mHover == kHoverAll && !all, VoLumSelectionStyle::AmberPicker, 3.f, 0.f);
    g.DrawText(IText(13.f, SelectionInkColor(VoLumSelectionStyle::AmberPicker, all), "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               "All MIDI channels", allR);

    const IRECT oneR = OneRect();
    DrawVoLumSelection(g, oneR, !all, all && mHover == kHoverOne, VoLumSelectionStyle::AmberPicker, 3.f, 0.f);
    g.DrawText(IText(13.f, SelectionInkColor(VoLumSelectionStyle::AmberPicker, !all), "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               all ? "Just one channel" : ("Channel " + std::to_string(mChannel)).c_str(), oneR);
    if (!all)
    {
      // Byte escapes, not \u: the narrow-literal execution charset here is not
      // UTF-8, so a \u2039 single-quote glyph came out as one invalid byte and the
      // arrows vanished from the stepper.
      const IColor dark(255, 26, 18, 8);
      g.DrawText(IText(15.f, mHover == kHoverDown ? dark : dark.WithOpacity(0.55f), "Josefin-Bold", EAlign::Center,
                       EVAlign::Middle),
                 "\xE2\x80\xB9", oneR.GetFromLeft(20.f));
      g.DrawText(IText(15.f, mHover == kHoverUp ? dark : dark.WithOpacity(0.55f), "Josefin-Bold", EAlign::Center,
                       EVAlign::Middle),
                 "\xE2\x80\xBA", oneR.GetFromRight(20.f));
    }

    const IText body(11.5f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    const IText dim(10.5f, VoLumColors::TEXT_DIM.WithOpacity(0.75f), "Josefin-Sans", EAlign::Near, EVAlign::Middle);

    g.DrawText(dim, all ? "MIDI calls this Omni." : "Anything on the other 15 channels is ignored.",
               IRECT(mRECT.L, mRECT.T + 30.f, mRECT.L + kColumnW, mRECT.T + 44.f));

    const float textL = mRECT.L + kColumnW + 22.f;
    g.DrawText(
      body, "One guitarist, one pedalboard: leave this on All.", IRECT(textL, mRECT.T + 1.f, mRECT.R, mRECT.T + 15.f));
    g.DrawText(body, "Two VoLums on the same MIDI cable: give each one its own channel,",
               IRECT(textL, mRECT.T + 15.f, mRECT.R, mRECT.T + 29.f));
    g.DrawText(body, "so a footswitch can change one amp without touching the other.",
               IRECT(textL, mRECT.T + 29.f, mRECT.R, mRECT.T + 43.f));
#if defined(APP_API)
    g.DrawText(
      dim, "Pick the MIDI port under File > Preferences.", IRECT(textL, mRECT.T + 44.f, mRECT.R, mRECT.T + 58.f));
#else
    g.DrawText(dim, "MIDI arrives on this track's input.", IRECT(textL, mRECT.T + 44.f, mRECT.R, mRECT.T + 58.f));
#endif
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (AllRect().Contains(x, y))
    {
      Commit(0);
      return;
    }
    const IRECT one = OneRect();
    if (!one.Contains(x, y))
      return;
    if (mChannel == 0)
    {
      // Entering "just one channel" must not silently reset to 1 for a player who
      // had already picked 7 and toggled back to All to compare.
      Commit(mLastOne);
      return;
    }
    if (x < one.L + 20.f)
      Commit(mChannel == 1 ? volum::kMidiChannelCount : mChannel - 1);
    else if (x > one.R - 20.f)
      Commit(mChannel == volum::kMidiChannelCount ? 1 : mChannel + 1);
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
  static constexpr float kAllW = 150.f;
  static constexpr float kOneW = 138.f;
  static constexpr float kColumnW = kAllW + 8.f + kOneW;
  static constexpr float kRowH = 27.f;
  static constexpr int kHoverNone = 0;
  static constexpr int kHoverAll = 1;
  static constexpr int kHoverOne = 2;
  static constexpr int kHoverDown = 3;
  static constexpr int kHoverUp = 4;

  // Anchored to the top, not the middle: the Omni hint sits under the pair and
  // the three explanation lines beside it, so the card reads as one band.
  IRECT AllRect() const { return IRECT(mRECT.L, mRECT.T, mRECT.L + kAllW, mRECT.T + kRowH); }
  IRECT OneRect() const { return IRECT(mRECT.L + kAllW + 8.f, mRECT.T, mRECT.L + kColumnW, mRECT.T + kRowH); }

  int HoverAt(float x, float y) const
  {
    if (AllRect().Contains(x, y))
      return kHoverAll;
    const IRECT one = OneRect();
    if (!one.Contains(x, y))
      return kHoverNone;
    if (mChannel == 0)
      return kHoverOne;
    if (x < one.L + 20.f)
      return kHoverDown;
    if (x > one.R - 20.f)
      return kHoverUp;
    return kHoverOne;
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
};

/** The program number 0-127 Sound assignment list, as Settings shows it.
 *
 * Same store and same rows as the PLAY rail (volum::BuildPlaySlots over
 * content::Registry::midiSoundMap), drawn as a table instead of a stage: the
 * program number, the Sound's preset name, the amp it belongs to. A row whose
 * Sound is gone keeps its number and reads red, exactly as MIDI treats it - the
 * program still exists, the thing it pointed at does not.
 *
 * Settings is not the performance surface, so the number is an editable field
 * rather than a caption: clicking it types a new one. Assignment is Add /
 * reassign / clear (clicks). Row-swap drag is disabled so Program Change
 * numbers stay put unless the player asks. Clicking a number still goes out as
 * a swap, which is safe - SwapMidiSoundSlots onto an empty number is a move
 * and onto an occupied one an exchange, so neither edit can silently drop a Sound.
 *
 * Adding asks for the number first (prefilled with the first free one) and the
 * Sound second, because the number is the thing the player's footswitch sends
 * and the thing they opened this tab to control.
 *
 * Mutations go out through the three callbacks, which the layout wires to the
 * same plugin methods PLAY's Add/Clear use, so there is one writer per
 * store. */
class VoLumMidiSoundMapControl : public IControl
{
public:
  using AssignCallback = std::function<void(int, const volum::SoundChoice&)>;
  using ClearCallback = std::function<void(int)>;
  using SwapCallback = std::function<void(int, int)>;

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

  void SetSwapCallback(SwapCallback swap) { mSwap = std::move(swap); }
  void SetPickerGroups(volum::PickerGroupSession* session) { mPickerGroups = session; }

  void SetData(const std::vector<volum::FactoryPreset>& factory, const volum::content::Registry& registry)
  {
    mSlots = volum::BuildPlaySlots(factory, registry);
    mChoices = volum::BuildSoundChoices(factory, registry);
    // A row can vanish under the pointer (cleared here, dragged in PLAY, edited by
    // another instance), so the scroll offset, the open screen and any in-flight
    // drag all get re-validated rather than trusted.
    mScroll = std::clamp(mScroll, 0.f, MaxScroll());
    if (mScreen != kScreenList && mChoices.empty())
      mScreen = kScreenList;
    mPressRow = -1;
    mPressSlot = -1;
    mPressCell = kCellNone;
    SetDirty(false);
  }

  // Settings closes from the gear, the panel × and Escape; a sub-screen left open
  // there would otherwise still be up the next time the page opens.
  void ResetToList()
  {
    if (GetAnimationFunction())
      OnEndAnimation();
    mEmptyFlash = 0.f;
    mScreen = kScreenList;
    mEditSlot = -1;
    mTextTarget = kTextNone;
    mTextSlot = -1;
    mPressRow = -1;
    mPressSlot = -1;
    mPressCell = kCellNone;
    mHoverRow = -1;
    mHoverChoice = -1;
    mHoverHeader = 0;
    mHoverCell = kCellNone;
    mNumberHover = kNumberHoverNone;
    mListBar.OnUp();
    mPickerBar.OnUp();
    SetDirty(false);
  }

  // Escape backs out of a sub-screen before it is allowed to close Settings.
  bool ConsumeEscape()
  {
    if (mScreen == kScreenList)
      return false;
    ResetToList();
    return true;
  }

  void Draw(IGraphics& g) override
  {
    if (mScreen == kScreenPicker)
    {
      DrawPicker(g);
      return;
    }
    if (mScreen == kScreenNumber)
    {
      DrawNumberStep(g);
      return;
    }

    const IRECT head = HeadRect();
    const IText cap(9.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    g.DrawText(cap, "PROGRAM", IRECT(head.L + 6.f, head.T, head.L + kNumberW + 8.f, head.B));
    // +9, not +8: the rows start one pixel inside the list frame, so the caption
    // has to shift with them or it sits a hair left of the column it names.
    g.DrawText(cap, "SOUND", IRECT(head.L + kNumberW + 9.f, head.T, head.MW(), head.B));
    g.DrawText(IText(9.f, VoLumColors::TEAL_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "AMP",
               IRECT(head.MW() + 8.f, head.T, head.R - 30.f, head.B));

    const IRECT list = ListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);

    if (mSlots.empty())
    {
      g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                 "No Sounds yet. Add one, and your footswitch calls it up by its program number.", list);
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

      const auto scroll = ListScrollMetrics();
      if (scroll.maxScroll > 0.5f)
      {
        const IRECT track = ListTrackRect();
        DrawVoLumScrollbar(
          g, track, IRECT(track.L, scroll.thumbY, track.R, scroll.thumbY + scroll.thumbH), mListBar.dragging);
      }
    }

    // Nothing to assign means Add cannot lead anywhere, so it reads inert rather
    // than swallowing the click silently.
    const IRECT add = AddRect();
    const bool addLive = !mChoices.empty();
    const bool hot = addLive && mHoverRow == kHoverAdd;
    g.FillRoundRect(hot ? IColor(70, 232, 168, 92) : VoLumColors::BTN_OFF_BG.WithOpacity(addLive ? 1.f : 0.45f), add,
                    3.f);
    g.DrawRoundRect(hot ? VoLumColors::AMBER : VoLumColors::FRAME.WithOpacity(addLive ? 1.f : 0.5f), add, 3.f, nullptr,
                    hot ? 1.3f : 1.f);
    g.DrawText(IText(11.5f, hot ? VoLumColors::TEXT_BRIGHT : (addLive ? VoLumColors::CREAM : VoLumColors::TEXT_DIM),
                     "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               "+  Add Sound", add);

    const IText foot(10.5f, VoLumColors::TEXT_DIM.WithOpacity(0.75f), "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    if (mChoices.empty())
    {
      const IText why = mEmptyFlash > 0.f
                          ? IText(10.5f, VoLumColors::GOLD.WithOpacity(0.55f + 0.45f * mEmptyFlash), "Josefin-Bold",
                                  EAlign::Near, EVAlign::Middle)
                          : foot;
      g.DrawText(why, "Save a preset first: a Sound is an amp plus a named preset.",
                 IRECT(add.R + 12.f, add.T, mRECT.R, add.B));
    }
    else
    {
      g.DrawText(foot, "Click a number to change it. Click a row to pick another Sound.",
                 IRECT(add.R + 12.f, add.T, mRECT.R, add.MH()));
      g.DrawText(foot, "Clear a row with ×. PLAY shows the same list.", IRECT(add.R + 12.f, add.MH(), mRECT.R, add.B));
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (mScreen == kScreenPicker)
    {
      OnPickerDown(x, y);
      return;
    }
    if (mScreen == kScreenNumber)
    {
      OnNumberDown(x, y);
      return;
    }

    if (AddRect().Contains(x, y))
    {
      if (mChoices.empty())
        FlashEmptyHint();
      else
        OpenNumberStep(FirstFreeSlot());
      return;
    }

    const auto scroll = ListScrollMetrics();
    const IRECT track = ListTrackRect();
    if (mListBar.OnDown(x, y, track.L, track.R, scroll))
    {
      mScroll = volum::scroll::ThumbYToScroll(
        y - mListBar.grabDY, scroll.trackTop, scroll.trackH, scroll.thumbH, scroll.maxScroll);
      SetDirty(false);
      return;
    }

    const int row = RowAt(x, y);
    if (row < 0)
      return;
    mPressRow = row;
    mPressSlot = mSlots[static_cast<size_t>(row)].slot;
    mPressCell = CellAt(RowRect(row), x);
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override
  {
    if (mListBar.dragging)
    {
      const auto m = ListScrollMetrics();
      const float next = mListBar.OnDrag(y, m);
      if (next >= 0.f)
        mScroll = next;
      SetDirty(false);
    }
    else if (mPickerBar.dragging)
    {
      const auto m = PickerScrollMetrics();
      const float next = mPickerBar.OnDrag(y, m);
      if (next >= 0.f)
        mPickerScroll = next;
      SetDirty(false);
    }
    (void)x;
  }

  void OnMouseUp(float x, float y, const IMouseMod&) override
  {
    mListBar.OnUp();
    mPickerBar.OnUp();
    if (mScreen != kScreenList || mPressRow < 0)
      return;

    const int pressRow = mPressRow;
    const int pressSlot = mPressSlot;
    const int pressCell = mPressCell;
    mPressRow = -1;
    mPressSlot = -1;
    mPressCell = kCellNone;

    if (RowAt(x, y) != pressRow)
      return;

    if (pressCell == kCellClear)
    {
      if (mClear)
        mClear(pressSlot);
      return;
    }
    if (pressCell == kCellNumber)
    {
      BeginNumberEntry(kTextRenumber, pressSlot, NumberCellRect(RowRect(pressRow)));
      return;
    }
    OpenPicker(pressSlot);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const bool list = mScreen == kScreenList;
    const int choice = mScreen == kScreenPicker ? PickerChoiceAt(x, y) : -1;
    const int header = mScreen == kScreenPicker ? PickerHeaderAt(x, y) : 0;
    const int row = list ? (AddRect().Contains(x, y) ? kHoverAdd : RowAt(x, y)) : -1;
    const int cell = list && row >= 0 ? CellAt(RowRect(row), x) : kCellNone;
    const int button = mScreen == kScreenNumber ? NumberHoverAt(x, y) : kNumberHoverNone;
    if (row == mHoverRow && choice == mHoverChoice && cell == mHoverCell && button == mNumberHover
        && header == mHoverHeader)
      return;
    mHoverRow = row;
    mHoverChoice = choice;
    mHoverCell = cell;
    mNumberHover = button;
    mHoverHeader = header;
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHoverRow = mHoverChoice = -1;
    mHoverCell = kCellNone;
    mNumberHover = kNumberHoverNone;
    mHoverHeader = 0;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (mScreen == kScreenPicker)
    {
      if (PickerListRect().Contains(x, y))
        mPickerScroll =
          volum::scroll::ClampScroll(mPickerScroll + volum::scroll::ListWheelDelta(d, kRowH), PickerMaxScroll());
    }
    else if (mScreen == kScreenList && ListRect().Contains(x, y))
      mScroll = volum::scroll::ClampScroll(mScroll + volum::scroll::ListWheelDelta(d, kRowH), MaxScroll());
    SetDirty(false);
  }

  // Opened with kNoValIdx, so iPlug2 hands back the raw string instead of the
  // atof() it would otherwise apply: "" and "abc" have to leave the number where
  // it was, not move the Sound to program 0. See VoLumNumericEntry.h.
  void OnTextEntryCompletion(const char* str, int) override
  {
    const int target = mTextTarget;
    const int slot = mTextSlot;
    mTextTarget = kTextNone;
    mTextSlot = -1;

    double parsed = 0.0;
    if (!str || !volum::ParseNumericEntry(str, parsed))
    {
      SetDirty(false);
      return;
    }
    const int number = std::clamp(static_cast<int>(std::lround(parsed)), 0, volum::kMidiSoundSlotCount - 1);

    if (target == kTextAddStep)
      mNumberDraft = number;
    else if (target == kTextRenumber && slot >= 0 && number != slot && mSwap)
      mSwap(slot, number);
    SetDirty(false);
  }

private:
  static constexpr float kRowH = 22.f;
  static constexpr float kHeadH = 14.f;
  static constexpr float kFooterH = 24.f;
  static constexpr float kNumberW = 48.f;
  static constexpr int kHoverAdd = -2;

  static constexpr int kScreenList = 0;
  static constexpr int kScreenNumber = 1;
  static constexpr int kScreenPicker = 2;

  static constexpr int kCellNone = 0;
  static constexpr int kCellNumber = 1;
  static constexpr int kCellBody = 2;
  static constexpr int kCellClear = 3;

  static constexpr int kNumberHoverNone = 0;
  static constexpr int kNumberHoverDown = 1;
  static constexpr int kNumberHoverUp = 2;
  static constexpr int kNumberHoverField = 3;
  static constexpr int kNumberHoverGo = 4;
  static constexpr int kNumberHoverClose = 5;

  static constexpr int kTextNone = 0;
  static constexpr int kTextAddStep = 1;
  static constexpr int kTextRenumber = 2;

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
  static IRECT NumberCellRect(const IRECT& row)
  {
    return IRECT(row.L + 4.f, row.T + 2.f, row.L + kNumberW, row.B - 2.f);
  }
  static int CellAt(const IRECT& row, float x)
  {
    if (ClearRect(row).Contains(x, row.MH()))
      return kCellClear;
    return x < row.L + kNumberW ? kCellNumber : kCellBody;
  }
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
    mScreen = kScreenPicker;
    mPickerScroll = 0.f;
    mHoverChoice = -1;
    InitPickerSession();
    SetDirty(false);
  }

  // The answer to "why did nothing happen" is already on screen, so the click
  // pulses that line instead of opening anything.
  void FlashEmptyHint()
  {
    mEmptyFlash = 1.f;
    SetAnimation(
      [this](IControl* pCaller) {
        const float progress = static_cast<float>(pCaller->GetAnimationProgress());
        if (progress > 1.f)
        {
          mEmptyFlash = 0.f;
          pCaller->OnEndAnimation();
          return;
        }
        mEmptyFlash = 1.f - progress;
        SetDirty(false);
      },
      700);
    SetDirty(false);
  }

  void OpenNumberStep(int slot)
  {
    if (mChoices.empty())
      return;
    // -1 means all 128 numbers are taken; land on 0 so the step still opens and
    // the "already plays" line explains what choosing a Sound will do.
    mNumberDraft = slot < 0 ? 0 : slot;
    mScreen = kScreenNumber;
    mNumberHover = kNumberHoverNone;
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

  const volum::PlaySlot* SlotAtNumber(int number) const
  {
    for (const auto& slot : mSlots)
      if (slot.slot == number)
        return &slot;
    return nullptr;
  }

  static std::string NumberLabel(int n)
  {
    if (n < 0)
      return "--";
    return n < 10 ? "0" + std::to_string(n) : std::to_string(n);
  }

  void BeginNumberEntry(int target, int slot, const IRECT& bounds)
  {
    auto* ui = GetUI();
    if (!ui)
      return;
    mTextTarget = target;
    mTextSlot = slot;
    SetTextEntryLength(3);
    ui->CreateTextEntry(*this, IText(12.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
                        bounds, NumberLabel(slot).c_str(), kNoValIdx);
    SetDirty(false);
  }

  void DrawRow(IGraphics& g, const IRECT& row, int index, const IRECT& clip)
  {
    const auto& slot = mSlots[static_cast<size_t>(index)];
    const bool hovered = mHoverRow == index;
    DrawVoLumSelection(g, row, false, hovered, VoLumSelectionStyle::ListTeal, 2.f, 1.f);

    // The number is a field, not a caption: it carries a frame so it reads as
    // something you can click and retype. That affordance is what lets the rest
    // of the row keep click-to-reassign without the two gestures colliding.
    const IRECT numberCell = NumberCellRect(row);
    const bool numberHot = hovered && mHoverCell == kCellNumber;
    if (hovered)
      g.DrawRoundRect(numberHot ? VoLumColors::GOLD : VoLumColors::FRAME, numberCell, 2.f);
    const IColor numberInk = slot.valid ? VoLumColors::GOLD : VoLumColors::DANGER;
    g.DrawText(IText(12.f, numberInk, "Josefin-Bold", EAlign::Center, EVAlign::Middle), NumberLabel(slot.slot).c_str(),
               numberCell);

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

    const float textL = row.L + kNumberW + 8.f;
    if (slot.valid)
    {
      cell(IRECT(textL, row.T, row.MW(), row.B),
           IText(12.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
           slot.sound.presetName.c_str());
      cell(IRECT(row.MW() + 8.f, row.T, row.R - 26.f, row.B),
           IText(11.f, VoLumColors::TEAL_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
           slot.sound.ampName.c_str());
    }
    else
    {
      // Same words as the PLAY rail: one state, one name.
      cell(IRECT(textL, row.T, row.R - 26.f, row.B),
           IText(11.f, VoLumColors::DANGER, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
           volum::kPlayInvalidSlotLabel);
    }

    if (hovered)
    {
      const IRECT clear = ClearRect(row);
      g.FillRoundRect(VoLumColors::DANGER_FILL, clear, 2.f);
      g.DrawRoundRect(VoLumColors::DANGER, clear, 2.f);
      DrawCrossGlyph(g, clear, VoLumColors::TEXT_BRIGHT);
    }
  }

  // ---- Add, step one: which program number --------------------------------
  //
  // Full-body like the picker, and for the same reason: this control is a child
  // of the Settings container, so a floating panel would overlap the listen
  // filter card above it with nothing to clip it.
  IRECT NumberBodyRect() const { return IRECT(mRECT.L, mRECT.T + 30.f, mRECT.R, mRECT.B); }
  IRECT NumberFieldRect() const
  {
    const IRECT body = NumberBodyRect();
    return IRECT(body.L + 6.f, body.T + 16.f, body.L + 162.f, body.T + 48.f);
  }
  IRECT NumberGoRect() const
  {
    const IRECT field = NumberFieldRect();
    return IRECT(field.R + 16.f, field.T, field.R + 176.f, field.B);
  }
  IRECT NumberCloseRect() const { return IRECT(mRECT.R - 22.f, mRECT.T, mRECT.R, mRECT.T + 22.f); }

  int NumberHoverAt(float x, float y) const
  {
    if (NumberCloseRect().Contains(x, y))
      return kNumberHoverClose;
    if (NumberGoRect().Contains(x, y))
      return kNumberHoverGo;
    const IRECT field = NumberFieldRect();
    if (!field.Contains(x, y))
      return kNumberHoverNone;
    if (x < field.L + 30.f)
      return kNumberHoverDown;
    if (x > field.R - 30.f)
      return kNumberHoverUp;
    return kNumberHoverField;
  }

  void OnNumberDown(float x, float y)
  {
    switch (NumberHoverAt(x, y))
    {
      case kNumberHoverClose: mScreen = kScreenList; break;
      case kNumberHoverGo: OpenPicker(mNumberDraft); return;
      case kNumberHoverDown:
        mNumberDraft = (mNumberDraft + volum::kMidiSoundSlotCount - 1) % volum::kMidiSoundSlotCount;
        break;
      case kNumberHoverUp: mNumberDraft = (mNumberDraft + 1) % volum::kMidiSoundSlotCount; break;
      case kNumberHoverField:
        BeginNumberEntry(kTextAddStep, mNumberDraft, NumberFieldRect().GetPadded(-30.f, 0.f, -30.f, 0.f));
        return;
      default: return;
    }
    SetDirty(false);
  }

  void DrawNumberStep(IGraphics& g)
  {
    g.DrawText(IText(12.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "Add a Sound",
               IRECT(mRECT.L + 6.f, mRECT.T, mRECT.R - 30.f, mRECT.T + 22.f));
    DrawCrossGlyph(
      g, NumberCloseRect(), mNumberHover == kNumberHoverClose ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, 1.5f);

    const IRECT body = NumberBodyRect();
    g.DrawText(IText(9.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "PROGRAM NUMBER",
               IRECT(body.L + 6.f, body.T, body.R, body.T + 14.f));

    const IRECT field = NumberFieldRect();
    g.FillRoundRect(VoLumColors::BTN_OFF_BG, field, 3.f);
    g.DrawRoundRect(mNumberHover == kNumberHoverField ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, field, 3.f);
    g.DrawText(IText(18.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               NumberLabel(mNumberDraft).c_str(), field);
    // Byte escapes, not \u: see VoLumMidiChannelControl above.
    g.DrawText(IText(16.f, mNumberHover == kNumberHoverDown ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                     EAlign::Center, EVAlign::Middle),
               "\xE2\x80\xB9", field.GetFromLeft(28.f));
    g.DrawText(IText(16.f, mNumberHover == kNumberHoverUp ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold",
                     EAlign::Center, EVAlign::Middle),
               "\xE2\x80\xBA", field.GetFromRight(28.f));

    const IRECT go = NumberGoRect();
    const bool goHot = mNumberHover == kNumberHoverGo;
    g.FillRoundRect(goHot ? IColor(70, 232, 168, 92) : VoLumColors::BTN_OFF_BG, go, 3.f);
    g.DrawRoundRect(goHot ? VoLumColors::AMBER : VoLumColors::FRAME, go, 3.f, nullptr, goHot ? 1.3f : 1.f);
    g.DrawText(IText(12.5f, goHot ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               "Choose Sound", go);

    const IText line(11.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    const IText dim(10.5f, VoLumColors::TEXT_DIM.WithOpacity(0.75f), "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    const volum::PlaySlot* taken = SlotAtNumber(mNumberDraft);
    const std::string status =
      taken == nullptr ? NumberLabel(mNumberDraft) + " is free."
                       : NumberLabel(mNumberDraft) + " already plays "
                           + volum::OccupiedSlotLabel(taken->valid, taken->sound.presetName)
                           + ". Choosing a Sound replaces it.";
    g.DrawText(taken == nullptr ? line : line.WithFGColor(VoLumColors::GOLD), status.c_str(),
               IRECT(body.L + 6.f, field.B + 8.f, body.R, field.B + 24.f));
    g.DrawText(dim, "This is the number your footswitch or floorboard sends. Anything from 0 to 127.",
               IRECT(body.L + 6.f, field.B + 26.f, body.R, field.B + 42.f));
  }

  // ---- Add, step two, and click-to-reassign: which Sound -------------------
  IRECT PickerListRect() const { return IRECT(mRECT.L, mRECT.T + 26.f, mRECT.R, mRECT.B); }
  IRECT PickerCloseRect() const { return IRECT(mRECT.R - 22.f, mRECT.T, mRECT.R, mRECT.T + 22.f); }
  IRECT ListTrackRect() const
  {
    const IRECT list = ListRect();
    return IRECT(list.R - 5.f, list.T + 1.f, list.R - 1.f, list.B - 1.f);
  }
  IRECT PickerTrackRect() const
  {
    const IRECT list = PickerListRect();
    return IRECT(list.R - 5.f, list.T + 1.f, list.R - 1.f, list.B - 1.f);
  }
  volum::scroll::ScrollMetrics ListScrollMetrics() const
  {
    const IRECT list = ListRect();
    return volum::scroll::ComputeScroll(list.T, list.B, list.H(), ContentH(), mScroll);
  }
  volum::scroll::ScrollMetrics PickerScrollMetrics() const
  {
    const IRECT list = PickerListRect();
    return volum::scroll::ComputeScroll(list.T, list.B, list.H(), PickerContentH(), mPickerScroll);
  }

  void InitPickerSession()
  {
    if (!mPickerGroups)
      return;
    bool hasFactory = false, hasUser = false;
    for (const auto& c : mChoices)
      c.factory ? hasFactory = true : hasUser = true;
    volum::InitPickerGroups(*mPickerGroups, hasFactory, hasUser);
  }
  bool PickerGroupOpen(bool factory) const
  {
    if (!mPickerGroups)
      return true;
    return factory ? mPickerGroups->factoryOpen : mPickerGroups->userOpen;
  }
  bool HasPickerFactory() const
  {
    for (const auto& c : mChoices)
      if (c.factory)
        return true;
    return false;
  }
  bool HasPickerUser() const
  {
    for (const auto& c : mChoices)
      if (!c.factory)
        return true;
    return false;
  }

  template <typename Fn>
  void WalkPickerRows(Fn&& fn) const
  {
    const IRECT list = PickerListRect();
    float y = list.T + 1.f - mPickerScroll;
    if (HasPickerFactory())
    {
      fn(1, -1, IRECT(list.L + 1.f, y, list.R - 7.f, y + kRowH));
      y += kRowH;
      if (PickerGroupOpen(true))
      {
        for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
        {
          if (!mChoices[(size_t)i].factory)
            continue;
          fn(0, i, IRECT(list.L + 1.f, y, list.R - 7.f, y + kRowH));
          y += kRowH;
        }
      }
    }
    if (HasPickerUser())
    {
      fn(-1, -1, IRECT(list.L + 1.f, y, list.R - 7.f, y + kRowH));
      y += kRowH;
      if (PickerGroupOpen(false))
      {
        for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
        {
          if (mChoices[(size_t)i].factory)
            continue;
          fn(0, i, IRECT(list.L + 1.f, y, list.R - 7.f, y + kRowH));
          y += kRowH;
        }
      }
    }
  }

  float PickerContentH() const
  {
    float h = 0.f;
    if (HasPickerFactory())
    {
      h += kRowH;
      if (PickerGroupOpen(true))
        for (const auto& c : mChoices)
          if (c.factory)
            h += kRowH;
    }
    if (HasPickerUser())
    {
      h += kRowH;
      if (PickerGroupOpen(false))
        for (const auto& c : mChoices)
          if (!c.factory)
            h += kRowH;
    }
    return h;
  }
  float PickerMaxScroll() const { return std::max(0.f, PickerContentH() - PickerListRect().H() + 2.f); }
  int PickerHeaderAt(float x, float y) const
  {
    if (!PickerListRect().Contains(x, y))
      return 0;
    int found = 0;
    WalkPickerRows([&](int kind, int, const IRECT& row) {
      if (found == 0 && kind != 0 && row.Contains(x, y))
        found = kind;
    });
    return found;
  }
  int PickerChoiceAt(float x, float y) const
  {
    if (!PickerListRect().Contains(x, y))
      return -1;
    int found = -1;
    WalkPickerRows([&](int kind, int choice, const IRECT& row) {
      if (found < 0 && kind == 0 && row.Contains(x, y))
        found = choice;
    });
    return found;
  }

  void OnPickerDown(float x, float y)
  {
    if (PickerCloseRect().Contains(x, y))
    {
      mScreen = kScreenList;
      SetDirty(false);
      return;
    }
    const auto scroll = PickerScrollMetrics();
    const IRECT track = PickerTrackRect();
    if (mPickerBar.OnDown(x, y, track.L, track.R, scroll))
    {
      mPickerScroll = volum::scroll::ThumbYToScroll(
        y - mPickerBar.grabDY, scroll.trackTop, scroll.trackH, scroll.thumbH, scroll.maxScroll);
      SetDirty(false);
      return;
    }
    const int header = PickerHeaderAt(x, y);
    if (header != 0 && mPickerGroups)
    {
      volum::TogglePickerGroup(*mPickerGroups, header > 0);
      SetDirty(false);
      return;
    }
    const int choice = PickerChoiceAt(x, y);
    if (choice >= 0 && choice < static_cast<int>(mChoices.size()) && mAssign)
      mAssign(mEditSlot, mChoices[static_cast<size_t>(choice)]);
    if (choice >= 0)
      mScreen = kScreenList;
    SetDirty(false);
  }

  void DrawPicker(IGraphics& g)
  {
    g.DrawText(IText(12.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
               ("Sound for program number " + NumberLabel(mEditSlot)).c_str(),
               IRECT(mRECT.L + 6.f, mRECT.T, mRECT.R - 30.f, mRECT.T + 22.f));
    DrawCrossGlyph(g, PickerCloseRect(), VoLumColors::GOLD_DIM, 1.5f);

    const IRECT list = PickerListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);
    g.PathClipRegion(list);
    WalkPickerRows([&](int kind, int choice, const IRECT& row) {
      if (row.B <= list.T || row.T >= list.B)
        return;
      if (kind != 0)
      {
        const bool factory = kind > 0;
        const bool open = PickerGroupOpen(factory);
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
      DrawVoLumSelection(g, row, false, mHoverChoice == choice, VoLumSelectionStyle::ListTeal, 2.f, 1.f);
      const auto& c = mChoices[static_cast<size_t>(choice)];
      g.DrawText(IText(12.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Middle), c.presetName.c_str(),
                 IRECT(row.L + 8.f, row.T, row.MW(), row.B));
      g.DrawText(IText(11.f, c.factory ? VoLumColors::GOLD_DIM : VoLumColors::TEAL_DIM, "Josefin-Sans", EAlign::Near,
                       EVAlign::Middle),
                 c.ampName.c_str(), IRECT(row.MW() + 8.f, row.T, row.R - 8.f, row.B));
    });
    g.PathClipRegion();

    const auto scroll = PickerScrollMetrics();
    if (scroll.maxScroll > 0.5f)
    {
      const IRECT track = PickerTrackRect();
      DrawVoLumScrollbar(
        g, track, IRECT(track.L, scroll.thumbY, track.R, scroll.thumbY + scroll.thumbH), mPickerBar.dragging);
    }
  }

  std::vector<volum::PlaySlot> mSlots;
  std::vector<volum::SoundChoice> mChoices;
  int mScreen = kScreenList;
  int mEditSlot = -1;
  int mNumberDraft = 0;
  int mNumberHover = kNumberHoverNone;
  int mHoverRow = -1;
  int mHoverChoice = -1;
  int mHoverHeader = 0;
  int mHoverCell = kCellNone;
  int mPressRow = -1;
  int mPressSlot = -1;
  int mPressCell = kCellNone;
  int mTextTarget = kTextNone;
  int mTextSlot = -1;
  float mScroll = 0.f;
  float mPickerScroll = 0.f;
  float mEmptyFlash = 0.f;
  volum::scroll::Interaction mListBar;
  volum::scroll::Interaction mPickerBar;
  volum::PickerGroupSession* mPickerGroups = nullptr;
  AssignCallback mAssign;
  ClearCallback mClear;
  SwapCallback mSwap;
};
