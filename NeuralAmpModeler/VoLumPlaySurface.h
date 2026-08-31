#pragma once

// Opus D PLAY chrome: centre live art, Program Change thumb rail, and bypass-only
// PRE/POST board. Product/state decisions live in VoLumPlayModel.h; this file owns
// drawing and interaction so VoLumControls.h stays an umbrella.

#include "VoLumColorHelpers.h"
#include "VoLumFractalArt.h"
#include "VoLumPlayModel.h"

#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class VoLumModeToggleControl : public IControl
{
public:
  using Callback = std::function<void(volum::UiMode)>;

  VoLumModeToggleControl(const IRECT& bounds, Callback callback)
  : IControl(bounds)
  , mCallback(std::move(callback))
  {
  }

  void SetMode(volum::UiMode mode)
  {
    mMode = mode;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT play = mRECT.GetFromLeft(mRECT.W() * 0.5f);
    const IRECT build = mRECT.GetFromRight(mRECT.W() * 0.5f);
    g.FillRoundRect(IColor(210, 7, 9, 14), mRECT, 2.f);
    g.DrawRoundRect(VoLumColors::GOLD_DIM.WithOpacity(0.55f), mRECT, 2.f);
    DrawVoLumSelection(g, play, mMode == volum::UiMode::Play, mMouseIsOver && play.Contains(mMouseX, mMouseY),
                       VoLumSelectionStyle::AmberPicker, 1.f, 0.f);
    DrawVoLumSelection(g, build, mMode == volum::UiMode::Build, mMouseIsOver && build.Contains(mMouseX, mMouseY),
                       VoLumSelectionStyle::AmberPicker, 1.f, 0.f);
    g.DrawText(VoLumType::Value(8.f, SelectionInkColor(VoLumSelectionStyle::AmberPicker,
                                                       mMode == volum::UiMode::Play)),
               "PLAY", play);
    g.DrawText(VoLumType::Value(8.f, SelectionInkColor(VoLumSelectionStyle::AmberPicker,
                                                       mMode == volum::UiMode::Build)),
               "BUILD", build);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const volum::UiMode next = x < mRECT.MW() ? volum::UiMode::Play : volum::UiMode::Build;
    if (mCallback)
      mCallback(next);
  }
  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    mMouseIsOver = true;
    mMouseX = x;
    mMouseY = y;
    SetDirty(false);
  }
  void OnMouseOut() override
  {
    mMouseIsOver = false;
    SetDirty(false);
  }

private:
  volum::UiMode mMode = volum::UiMode::Build;
  bool mMouseIsOver = false;
  float mMouseX = 0.f;
  float mMouseY = 0.f;
  Callback mCallback;
};

class VoLumPlaySurfaceControl : public IControl
{
public:
  enum Fx : int
  {
    Pitch,
    Comp,
    Nam1,
    Nam2,
    Chorus,
    Delay,
    Reverb,
    Tremolo,
    FxCount
  };

  using RecallCallback = std::function<void(int, const volum::SoundChoice&)>;
  using AssignCallback = std::function<void(int, const volum::SoundChoice&)>;
  using ClearCallback = std::function<void(int)>;
  using BypassCallback = std::function<void(const char*)>;

  VoLumPlaySurfaceControl(const IRECT& bounds, RecallCallback recall, AssignCallback assign, ClearCallback clear,
                          BypassCallback bypass)
  : IControl(bounds)
  , mRecall(std::move(recall))
  , mAssign(std::move(assign))
  , mClear(std::move(clear))
  , mBypass(std::move(bypass))
  {
  }

  void SetData(const std::vector<volum::FactoryPreset>& factory, const volum::content::Registry& registry,
               const std::string& activeAmpId, const std::string& activePresetId, int lastSlot,
               const std::string& liveAmpName, int liveArt, bool customArt, bool dual, const std::string& supportName,
               int supportArt, bool supportCustom, const std::array<bool, FxCount>& fx, bool dirty)
  {
    mSlots = volum::BuildPlaySlots(factory, registry);
    mChoices = volum::BuildSoundChoices(factory, registry);
    mActiveAmpId = activeAmpId;
    mActivePresetId = activePresetId;
    mLastSlot = lastSlot;
    mLiveAmpName = liveAmpName;
    mLiveArt = liveArt;
    mCustomArt = customArt;
    mDual = dual;
    mSupportName = supportName;
    mSupportArt = supportArt;
    mSupportCustom = supportCustom;
    mFx = fx;
    mDirty = dirty;
    SetDirty(false);
  }

  void Tick()
  {
    mPhase += 0.035f;
    if (mPhase > 6.283185f)
      mPhase -= 6.283185f;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    FillVGradient(g, mRECT, IColor(255, 20, 26, 36), IColor(255, 8, 10, 15));
    g.DrawRect(VoLumColors::FRAME, mRECT);
    DrawHeader(g);
    if (mSlots.empty())
      DrawEmpty(g);
    else
      DrawStage(g);
    DrawBoard(g);
    if (mPickerOpen)
      DrawPicker(g);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (mPickerOpen)
    {
      const int choice = PickerChoiceAt(x, y);
      if (choice >= 0 && choice < static_cast<int>(mChoices.size()) && mAssign)
        mAssign(mEditSlot, mChoices[(size_t)choice]);
      if (choice >= 0 || !PickerRect().Contains(x, y))
        mPickerOpen = false;
      SetDirty(false);
      return;
    }

    if ((mSlots.empty() ? EmptyAddRect() : AddRect()).Contains(x, y))
    {
      mEditSlot = FirstFreeSlot();
      mPickerOpen = mEditSlot >= 0 && !mChoices.empty();
      SetDirty(false);
      return;
    }

    const int row = SlotAt(x, y);
    if (row >= 0)
    {
      if (ClearRectForRow(row).Contains(x, y))
      {
        if (mClear)
          mClear(mSlots[(size_t)row].slot);
      }
      else if (mSlots[(size_t)row].valid && mRecall)
        mRecall(mSlots[(size_t)row].slot, mSlots[(size_t)row].sound);
      return;
    }

    for (int i = 0; i < FxCount; ++i)
    {
      if (FxRect(i).Contains(x, y) && mBypass)
      {
        mBypass(volum::kPlayBypassParamNames[static_cast<size_t>(i)]);
        return;
      }
    }
  }

  void OnMouseDblClick(float x, float y, const IMouseMod&) override
  {
    const int row = SlotAt(x, y);
    if (row < 0 || mChoices.empty())
      return;
    mEditSlot = mSlots[(size_t)row].slot;
    mPickerOpen = true;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (mPickerOpen && PickerRect().Contains(x, y))
      mPickerScroll = std::clamp(mPickerScroll - d * 34.f, 0.f, PickerMaxScroll());
    else if (RailListRect().Contains(x, y))
      mRailScroll = std::clamp(mRailScroll - d * 59.f, 0.f, RailMaxScroll());
    SetDirty(false);
  }

private:
  IRECT HeaderRect() const { return IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + 46.f); }
  IRECT UpperRect() const { return IRECT(mRECT.L + 12.f, mRECT.T + 56.f, mRECT.R - 12.f, mRECT.B - 142.f); }
  IRECT RailRect() const { return UpperRect().GetFromRight(172.f); }
  IRECT RailListRect() const
  {
    const auto r = RailRect();
    return IRECT(r.L, r.T + 22.f, r.R, r.B - 42.f);
  }
  IRECT AddRect() const
  {
    const auto r = RailRect();
    return IRECT(r.L, r.B - 36.f, r.R, r.B);
  }
  IRECT EmptyAddRect() const
  {
    const IRECT empty(mRECT.L, 46.f, mRECT.R, mRECT.B - 142.f);
    return IRECT(empty.MW() - 78.f, empty.MH() + 62.f, empty.MW() + 78.f, empty.MH() + 98.f);
  }
  IRECT ArtRect() const { return UpperRect().GetReducedFromRight(182.f); }
  IRECT BoardRect() const { return IRECT(mRECT.L + 12.f, mRECT.B - 133.f, mRECT.R - 12.f, mRECT.B - 9.f); }
  IRECT PickerRect() const { return IRECT(mRECT.MW() - 230.f, 76.f, mRECT.MW() + 230.f, 532.f); }

  int ActiveRow() const
  {
    for (int i = 0; i < static_cast<int>(mSlots.size()); ++i)
      if (volum::IsLastRecalledSlot(mSlots[(size_t)i], mLastSlot, mActiveAmpId, mActivePresetId))
        return i;
    return -1;
  }

  void DrawHeader(IGraphics& g)
  {
    FillVGradient(g, HeaderRect(), IColor(255, 18, 24, 33), IColor(240, 11, 15, 22));
    g.DrawLine(VoLumColors::TEAL_DIM.WithOpacity(0.3f), mRECT.L, 46.f, mRECT.R, 46.f);
    g.DrawText(VoLumType::Display(25.f, VoLumColors::GOLD, EAlign::Near), "VoLum",
               IRECT(16.f, 4.f, 150.f, 31.f));
    g.DrawText(VoLumType::Value(6.5f, VoLumColors::GOLD_DIM, EAlign::Near), "NAM PLAYER",
               IRECT(18.f, 28.f, 150.f, 42.f));
    g.FillCircle(VoLumColors::TEAL, 413.f, 23.f, 2.5f);
    g.DrawText(VoLumType::Value(7.f, VoLumColors::CREAM_DIM), "SOUNDS  ·  PROGRAM CHANGE",
               IRECT(426.f, 13.f, 610.f, 33.f));
  }

  void DrawEmpty(IGraphics& g)
  {
    const IRECT empty(mRECT.L, 46.f, mRECT.R, mRECT.B - 142.f);
    const float glow = 0.08f + 0.06f * (0.5f + 0.5f * std::sin(mPhase));
    DrawSoftGlowCircle(g, empty.MW(), empty.MH() - 12.f, 138.f, VoLumColors::GOLD.WithOpacity(glow));
    DrawHeroFractalArt(g, IRECT(empty.MW() - 110.f, empty.MH() - 100.f, empty.MW() + 110.f, empty.MH() + 60.f), 14);
    g.FillRect(IColor(205, 8, 10, 15), empty);
    g.DrawText(VoLumType::Display(38.f, VoLumColors::CREAM), "No Sounds assigned",
               IRECT(empty.L, empty.MH() - 46.f, empty.R, empty.MH() + 4.f));
    DrawDiamond(g, empty.MW(), empty.MH() + 12.f, 3.f, VoLumColors::GOLD_DIM);
    g.DrawText(VoLumType::Body(12.f, VoLumColors::CREAM_DIM), "Add a Sound to light this stage.",
               IRECT(empty.L, empty.MH() + 25.f, empty.R, empty.MH() + 49.f));
    const IRECT add = EmptyAddRect();
    g.DrawRoundRect(VoLumColors::GOLD_DIM, add, 2.f);
    g.DrawText(VoLumType::Value(9.f, VoLumColors::GOLD), "+  ADD SOUND", add);
  }

  void DrawAmpPanel(IGraphics& g, const IRECT& rect, const std::string& name, int art, bool custom, bool support)
  {
    const float pulse = 0.5f + 0.5f * std::sin(mPhase + (support ? 0.7f : 0.f));
    g.FillRect(VoLumColors::HERO_BG, rect);
    DrawSoftGlowCircle(g, rect.MW(), rect.MH(), rect.W() * 0.48f,
                       (support ? VoLumColors::TEAL : VoLumColors::GOLD).WithOpacity(0.06f + pulse * 0.10f));
    const IRECT artRect = rect.GetPadded(-18.f);
    if (custom)
      DrawCustomAmpArt(g, artRect, art, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
    else
      DrawHeroFractalArt(g, artRect, art);
    g.FillRect(IColor(85, 4, 6, 10), IRECT(rect.L, rect.B - 44.f, rect.R, rect.B));
    g.DrawRect((support ? VoLumColors::TEAL_DIM : VoLumColors::GOLD_DIM).WithOpacity(0.7f), rect);
    const std::string role = support ? "SUPPORT · " + name : (mDual ? "MAIN · " + name : name);
    g.DrawText(VoLumType::Value(6.5f, support ? VoLumColors::TEAL : VoLumColors::GOLD, EAlign::Near), role.c_str(),
               IRECT(rect.L + 8.f, rect.T + 5.f, rect.R - 8.f, rect.T + 19.f));
  }

  void DrawStage(IGraphics& g)
  {
    const IRECT art = ArtRect();
    if (mDual)
    {
      const IRECT main = IRECT(art.L, art.T, art.MW() - 4.f, art.B);
      const IRECT support = IRECT(art.MW() + 4.f, art.T, art.R, art.B);
      DrawAmpPanel(g, main, mLiveAmpName, mLiveArt, mCustomArt, false);
      DrawAmpPanel(g, support, mSupportName, mSupportArt, mSupportCustom, true);
    }
    else
      DrawAmpPanel(g, art, mLiveAmpName, mLiveArt, mCustomArt, false);

    const int active = ActiveRow();
    std::string title = mLiveAmpName;
    int pc = -1;
    if (active >= 0)
    {
      title = mSlots[(size_t)active].sound.presetName;
      pc = mSlots[(size_t)active].slot;
    }
    const IRECT overlay(art.L, art.B - 58.f, art.R, art.B);
    g.FillRect(IColor(190, 5, 7, 11), overlay);
    const std::string pcText = pc >= 0 ? "PC " + TwoDigits(pc) : "LIVE";
    g.DrawText(VoLumType::Value(17.f, VoLumColors::GOLD, EAlign::Near), pcText.c_str(),
               IRECT(overlay.L + 14.f, overlay.T + 14.f, overlay.L + 85.f, overlay.B - 5.f));
    g.DrawText(VoLumType::Display(32.f, VoLumColors::TEXT_BRIGHT, EAlign::Near), title.c_str(),
               IRECT(overlay.L + 90.f, overlay.T + 5.f, overlay.R - 120.f, overlay.B - 3.f));
    if (mDirty)
      g.DrawText(VoLumType::Value(7.f, VoLumColors::AMBER), "UNSAVED",
                 IRECT(overlay.R - 112.f, overlay.T + 15.f, overlay.R - 12.f, overlay.B - 8.f));
    DrawRail(g);
  }

  void DrawRail(IGraphics& g)
  {
    const auto rail = RailRect();
    g.DrawText(VoLumType::Value(7.f, VoLumColors::TEAL_DIM, EAlign::Near), "SOUNDS",
               IRECT(rail.L, rail.T, rail.R - 28.f, rail.T + 18.f));
    g.DrawText(VoLumType::Value(7.f, VoLumColors::GOLD_DIM, EAlign::Far), "PC",
               IRECT(rail.L, rail.T, rail.R, rail.T + 18.f));
    const auto list = RailListRect();
    g.PathClipRegion(list);
    int sticky = -1;
    for (int i = 0; i < static_cast<int>(mSlots.size()); ++i)
    {
      const IRECT row = RailRowRect(i, false);
      if (i == ActiveRow() && row.T < list.T)
      {
        sticky = i;
        continue;
      }
      if (row.B >= list.T && row.T <= list.B)
        DrawSlot(g, row, i);
    }
    if (sticky >= 0)
      DrawSlot(g, RailRowRect(sticky, true), sticky);
    g.PathClipRegion();
    g.FillRect(IColor(20, 252, 222, 145), AddRect());
    g.DrawRect(VoLumColors::GOLD_DIM.WithOpacity(0.65f), AddRect());
    g.DrawText(VoLumType::Value(8.f, VoLumColors::GOLD), "+  ADD", AddRect());
  }

  void DrawSlot(IGraphics& g, const IRECT& row, int index)
  {
    const auto& slot = mSlots[(size_t)index];
    const bool active = index == ActiveRow();
    g.FillRect(slot.valid ? IColor(230, 8, 12, 18) : IColor(230, 20, 10, 13), row);
    DrawVoLumSelection(g, row, active, false, VoLumSelectionStyle::Brass, 1.f, 0.f);
    g.DrawRect(slot.valid ? VoLumColors::TEAL_DIM.WithOpacity(0.45f) : VoLumColors::DANGER.WithOpacity(0.55f), row);
    if (slot.valid)
    {
      const IRECT art(row.L + 2.f, row.T + 2.f, row.L + 47.f, row.B - 2.f);
      if (slot.sound.customArt)
        DrawCustomAmpArt(g, art, slot.sound.art, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
      else
        DrawStripMiniFractal(g, art, slot.sound.art);
      g.DrawText(VoLumType::Value(10.f, VoLumColors::GOLD, EAlign::Near), TwoDigits(slot.slot).c_str(),
                 IRECT(row.L + 53.f, row.T + 4.f, row.R - 15.f, row.T + 19.f));
      g.DrawText(VoLumType::Display(14.f, VoLumColors::CREAM, EAlign::Near), slot.sound.presetName.c_str(),
                 IRECT(row.L + 53.f, row.T + 17.f, row.R - 15.f, row.T + 37.f));
      g.DrawText(VoLumType::Label(7.f, VoLumColors::TEAL_DIM, EAlign::Near), slot.sound.ampName.c_str(),
                 IRECT(row.L + 53.f, row.T + 36.f, row.R - 15.f, row.B - 2.f));
      if (active)
        g.DrawText(VoLumType::Value(5.5f, VoLumColors::GOLD, EAlign::Far), "LIVE",
                   IRECT(row.L, row.T + 4.f, row.R - 5.f, row.T + 17.f));
    }
    else
    {
      g.DrawText(VoLumType::Value(10.f, VoLumColors::CREAM_DIM, EAlign::Near), TwoDigits(slot.slot).c_str(),
                 IRECT(row.L + 10.f, row.T + 5.f, row.R, row.T + 22.f));
      g.DrawText(VoLumType::Label(9.f, VoLumColors::DANGER, EAlign::Near), "INVALID SLOT",
                 IRECT(row.L + 10.f, row.T + 23.f, row.R - 15.f, row.B - 5.f));
    }
    g.DrawText(VoLumType::Label(8.f, VoLumColors::CREAM_DIM), "×", ClearRectForRow(index, row));
  }

  void DrawBoard(IGraphics& g)
  {
    const auto board = BoardRect();
    DrawPanelDepth(g, board);
    g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(0.38f), board);
    g.DrawText(VoLumType::Value(6.5f, VoLumColors::TEAL_DIM, EAlign::Near), "PRE  ·  STOMP TO BYPASS",
               IRECT(board.L + 20.f, board.T + 4.f, board.MW() - 8.f, board.T + 19.f));
    g.DrawText(VoLumType::Value(6.5f, VoLumColors::GOLD_DIM, EAlign::Near), "POST",
               IRECT(board.MW() + 8.f, board.T + 4.f, board.R - 20.f, board.T + 19.f));
    static const char* kNames[FxCount] = {"PITCH", "COMP", "NAM 1", "NAM 2", "CHORUS", "DELAY", "REVERB", "TREM"};
    for (int i = 0; i < FxCount; ++i)
    {
      const IRECT r = FxRect(i);
      DrawInsetWell(g, r, 2.f);
      if (mFx[(size_t)i])
      {
        g.FillRect(IColor(62, 252, 222, 145), r);
        g.DrawRect(VoLumColors::SEL_BORDER, r);
      }
      else
        g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(0.35f), r);
      g.DrawCircle(mFx[(size_t)i] ? VoLumColors::GOLD : VoLumColors::TEAL_DIM.WithOpacity(0.35f), r.MW(), r.T + 29.f,
                   9.f);
      g.FillCircle(mFx[(size_t)i] ? VoLumColors::GOLD : IColor(255, 28, 36, 45), r.MW(), r.T + 29.f, 3.f);
      g.DrawText(VoLumType::Value(7.f, mFx[(size_t)i] ? VoLumColors::GOLD : VoLumColors::CREAM_DIM), kNames[i],
                 IRECT(r.L, r.B - 27.f, r.R, r.B - 6.f));
    }
    DrawMeter(g, IRECT(board.L + 4.f, board.T + 24.f, board.L + 10.f, board.B - 12.f), false);
    DrawMeter(g, IRECT(board.R - 10.f, board.T + 24.f, board.R - 4.f, board.B - 12.f), true);
  }

  void DrawMeter(IGraphics& g, const IRECT& r, bool out)
  {
    g.FillRect(IColor(255, 4, 6, 9), r);
    const float level = 0.3f + 0.42f * (0.5f + 0.5f * std::sin(mPhase + (out ? 0.45f : 0.f)));
    const IRECT fill(r.L + 1.f, r.B - 1.f - (r.H() - 2.f) * level, r.R - 1.f, r.B - 1.f);
    g.FillRect(out ? VoLumColors::TEAL : VoLumColors::GOLD, fill);
  }

  void DrawPicker(IGraphics& g)
  {
    const IRECT panel = PickerRect();
    g.FillRoundRect(IColor(250, 8, 11, 16), panel, 4.f);
    g.DrawRoundRect(VoLumColors::GOLD_DIM, panel, 4.f);
    g.DrawText(VoLumType::Display(24.f, VoLumColors::CREAM), "Choose Sound",
               IRECT(panel.L, panel.T + 10.f, panel.R, panel.T + 48.f));
    g.DrawText(VoLumType::Value(7.f, VoLumColors::CREAM_DIM), ("ASSIGN TO PC " + TwoDigits(mEditSlot)).c_str(),
               IRECT(panel.L, panel.T + 44.f, panel.R, panel.T + 68.f));
    const IRECT list(panel.L + 12.f, panel.T + 76.f, panel.R - 12.f, panel.B - 12.f);
    g.PathClipRegion(list);
    float y = list.T - mPickerScroll;
    bool factoryHeader = false, userHeader = false;
    for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
    {
      const bool isFactory = mChoices[(size_t)i].factory;
      if ((isFactory && !factoryHeader) || (!isFactory && !userHeader))
      {
        const char* label = isFactory ? "FACTORY" : "USER";
        g.DrawText(VoLumType::Value(7.f, isFactory ? VoLumColors::GOLD_DIM : VoLumColors::TEAL_DIM, EAlign::Near),
                   label, IRECT(list.L + 4.f, y, list.R, y + 24.f));
        y += 24.f;
        factoryHeader |= isFactory;
        userHeader |= !isFactory;
      }
      const IRECT row(list.L, y, list.R, y + 34.f);
      if (row.B >= list.T && row.T <= list.B)
      {
        g.FillRect(IColor(180, 16, 20, 28), IRECT(row.L, row.T + 1.f, row.R, row.B - 1.f));
        g.DrawText(VoLumType::Label(12.f, VoLumColors::CREAM, EAlign::Near), mChoices[(size_t)i].presetName.c_str(),
                   IRECT(row.L + 10.f, row.T, row.MW() + 60.f, row.B));
        g.DrawText(VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Far), mChoices[(size_t)i].ampName.c_str(),
                   IRECT(row.MW(), row.T, row.R - 10.f, row.B));
      }
      y += 34.f;
    }
    g.PathClipRegion();
  }

  int SlotAt(float x, float y) const
  {
    const auto list = RailListRect();
    if (!list.Contains(x, y))
      return -1;
    const int active = ActiveRow();
    if (active >= 0)
    {
      const auto natural = RailRowRect(active, false);
      if (natural.T < list.T && RailRowRect(active, true).Contains(x, y))
        return active;
    }
    const int row = static_cast<int>((y - list.T + mRailScroll) / 59.f);
    return row >= 0 && row < static_cast<int>(mSlots.size()) ? row : -1;
  }
  IRECT ClearRectForRow(int index) const
  {
    const auto natural = RailRowRect(index, false);
    const bool sticky = index == ActiveRow() && natural.T < RailListRect().T;
    return ClearRectForRow(index, sticky ? RailRowRect(index, true) : natural);
  }
  IRECT ClearRectForRow(int, const IRECT& row) const { return IRECT(row.R - 16.f, row.B - 18.f, row.R, row.B); }
  IRECT FxRect(int i) const
  {
    const auto board = BoardRect();
    const float left = board.L + 20.f;
    const float usable = board.W() - 40.f;
    const float gap = 7.f;
    const float divider = 11.f;
    const float width = (usable - gap * 6.f - divider) / 8.f;
    const float x = left + i * (width + gap) + (i >= 4 ? divider - gap : 0.f);
    return IRECT(x, board.T + 23.f, x + width, board.B - 8.f);
  }
  float RailMaxScroll() const
  {
    return std::max(0.f, static_cast<float>(mSlots.size()) * 59.f - RailListRect().H());
  }
  IRECT RailRowRect(int index, bool sticky) const
  {
    const auto list = RailListRect();
    const float top = sticky ? list.T : list.T + index * 59.f - mRailScroll;
    return IRECT(list.L, top, list.R - 2.f, top + 54.f);
  }
  float PickerContentHeight() const
  {
    if (mChoices.empty())
      return 0.f;
    bool hasFactory = false, hasUser = false;
    for (const auto& c : mChoices)
      c.factory ? hasFactory = true : hasUser = true;
    return static_cast<float>(mChoices.size()) * 34.f + (hasFactory ? 24.f : 0.f) + (hasUser ? 24.f : 0.f);
  }
  float PickerMaxScroll() const { return std::max(0.f, PickerContentHeight() - (PickerRect().H() - 88.f)); }
  int PickerChoiceAt(float x, float y) const
  {
    const IRECT list(PickerRect().L + 12.f, PickerRect().T + 76.f, PickerRect().R - 12.f, PickerRect().B - 12.f);
    if (!list.Contains(x, y))
      return -1;
    float rowY = list.T - mPickerScroll;
    bool factoryHeader = false, userHeader = false;
    for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
    {
      const bool factory = mChoices[(size_t)i].factory;
      if ((factory && !factoryHeader) || (!factory && !userHeader))
      {
        rowY += 24.f;
        factoryHeader |= factory;
        userHeader |= !factory;
      }
      if (IRECT(list.L, rowY, list.R, rowY + 34.f).Contains(x, y))
        return i;
      rowY += 34.f;
    }
    return -1;
  }
  int FirstFreeSlot() const
  {
    bool used[128] = {};
    for (const auto& slot : mSlots)
      if (slot.slot >= 0 && slot.slot < 128)
        used[slot.slot] = true;
    for (int i = 0; i < 128; ++i)
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
  static int FactoryAmpForOwner(const std::string& owner)
  {
    try
    {
      return std::clamp(std::stoi(owner.substr(8)), 0, volum::kAmpCount - 1);
    }
    catch (...)
    {
      return 0;
    }
  }

  std::vector<volum::PlaySlot> mSlots;
  std::vector<volum::SoundChoice> mChoices;
  std::string mActiveAmpId, mActivePresetId, mLiveAmpName, mSupportName;
  int mLastSlot = -1, mLiveArt = 0, mSupportArt = 0, mEditSlot = -1;
  bool mCustomArt = false, mDual = false, mSupportCustom = false, mDirty = false, mPickerOpen = false;
  std::array<bool, FxCount> mFx{};
  float mRailScroll = 0.f, mPickerScroll = 0.f, mPhase = 0.f;
  RecallCallback mRecall;
  AssignCallback mAssign;
  ClearCallback mClear;
  BypassCallback mBypass;
};
