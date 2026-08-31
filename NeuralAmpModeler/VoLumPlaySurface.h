#pragma once

// Opus D PLAY chrome: centre live art, Program Change thumb rail, and bypass-only
// PRE/POST board. Product/state decisions live in VoLumPlayModel.h; this file owns
// drawing and interaction so VoLumControls.h stays an umbrella.

#include "VoLumColorHelpers.h"
#include "VoLumFractalArt.h"
#include "VoLumPlayModel.h"
#include "VoLumTriptychMotifs.h"

#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/** The PLAY / BUILD mode pair in the header cluster.
 *
 * A word pair, not a switch. The pre-1.3.0 version drew its own bevelled well and
 * filled the active half solid amber, which made the loudest object in the header
 * a control the player touches twice a session - and because it is attached above
 * the legacy full-canvas overlays, that amber slab floated on top of Settings,
 * Pack and Manage.
 *
 * So: no well, no outer border, brass instead of amber (the quiet dialect the cab
 * row and the PLAY rail already speak), and the inactive word recedes rather than
 * competing. It now reads as a sibling of the tuner / metronome / gear glyphs to
 * its right, which is what it is. Z-order is fixed at the attach site in
 * VoLumLayoutBuild.inc.cpp: this goes on before the overlays, so they cover it. */
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
    const IRECT play = CellRect(volum::UiMode::Play);
    const IRECT build = CellRect(volum::UiMode::Build);
    DrawCell(g, play, "PLAY", volum::UiMode::Play);
    DrawCell(g, build, "BUILD", volum::UiMode::Build);
    // Hairline between the two words, inset so it reads as a separator rather
    // than as the wall of a box.
    g.DrawLine(VoLumColors::FRAME.WithOpacity(0.8f), mRECT.MW(), mRECT.T + 7.f, mRECT.MW(), mRECT.B - 7.f);
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
  IRECT CellRect(volum::UiMode mode) const
  {
    const IRECT half = mode == volum::UiMode::Play ? mRECT.GetFromLeft(mRECT.W() * 0.5f)
                                                   : mRECT.GetFromRight(mRECT.W() * 0.5f);
    return half.GetPadded(0.f, -4.f, 0.f, -4.f);
  }

  void DrawCell(IGraphics& g, const IRECT& cell, const char* label, volum::UiMode mode)
  {
    const bool active = mMode == mode;
    const bool hovered = !active && mMouseIsOver && cell.Contains(mMouseX, mMouseY);
    DrawVoLumSelection(g, cell, active, hovered, VoLumSelectionStyle::Brass, 2.f, 0.f);
    // Selection ink for the active word; the same ink at 0.55 for the idle one, so
    // the pair stays one family and only the live mode carries weight.
    const IColor ink = active ? SelectionInkColor(VoLumSelectionStyle::Brass, true)
                              : SelectionInkColor(VoLumSelectionStyle::Brass, false)
                                  .WithOpacity(hovered ? 0.85f : 0.55f);
    g.DrawText(VoLumType::Label(10.f, ink), label, cell);
  }

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
               int supportArt, bool supportCustom, const std::array<bool, FxCount>& fx,
               const std::array<bool, FxCount>& fxAvailable, int midiChannel, bool dirty)
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
    mFxAvailable = fxAvailable;
    mMidiChannel = midiChannel;
    mDirty = dirty;
    ClampRailScroll();
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
      if (PickerCloseRect().Contains(x, y))
      {
        mPickerOpen = false;
        SetDirty(false);
        return;
      }
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
      mPickerScroll = 0.f;
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
      else if (!mSlots[(size_t)row].valid && !mChoices.empty())
      {
        // A numbered hole stays in the rail; clicking it is the way to fill it.
        mEditSlot = mSlots[(size_t)row].slot;
        mPickerOpen = true;
        mPickerScroll = 0.f;
        SetDirty(false);
      }
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
    mPickerScroll = 0.f;
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int row = mPickerOpen ? -1 : SlotAt(x, y);
    int fx = -1;
    if (!mPickerOpen)
      for (int i = 0; i < FxCount; ++i)
        if (FxRect(i).Contains(x, y))
          fx = i;
    const int choice = mPickerOpen ? PickerChoiceAt(x, y) : -1;
    if (row != mHoverRow || fx != mHoverFx || choice != mHoverChoice)
    {
      mHoverRow = row;
      mHoverFx = fx;
      mHoverChoice = choice;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mHoverRow = mHoverFx = mHoverChoice = -1;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (mPickerOpen && PickerRect().Contains(x, y))
      mPickerScroll = std::clamp(mPickerScroll - d * 30.f, 0.f, PickerMaxScroll());
    else if (RailListRect().Contains(x, y))
      mRailScroll = std::clamp(mRailScroll - d * kRowPitch, 0.f, RailMaxScroll());
    SetDirty(false);
  }

private:
  // Opus D metrics. The rail rows, board wells and art panel are sized from the
  // locked 900x600 mock (.scratch/release-1.3.0/play-proto/opus2, variant D), so
  // every band below is expressed relative to mRECT rather than hard-coded.
  static constexpr float kHeaderH = 46.f;
  static constexpr float kRailW = 170.f;
  static constexpr float kRailGap = 10.f;
  static constexpr float kRailCapH = 18.f;
  static constexpr float kRowH = 66.f;
  static constexpr float kRowPitch = 70.f;
  static constexpr float kAddH = 30.f;
  static constexpr float kAddGap = 8.f;
  static constexpr float kBoardBandH = 140.f;
  static constexpr float kPickerRowH = 30.f;
  static constexpr float kPickerCapH = 22.f;

  IRECT HeaderRect() const { return IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + kHeaderH); }
  IRECT StageRect() const
  {
    return IRECT(mRECT.L + 10.f, mRECT.T + kHeaderH + 8.f, mRECT.R - 12.f, mRECT.B - kBoardBandH);
  }
  IRECT RailRect() const { return StageRect().GetFromRight(kRailW); }
  IRECT ArtRect() const
  {
    const auto stage = StageRect();
    return IRECT(stage.L, stage.T, RailRect().L - kRailGap, stage.B);
  }
  float RailContentH() const
  {
    return mSlots.empty() ? 0.f : static_cast<float>(mSlots.size()) * kRowPitch - (kRowPitch - kRowH);
  }
  // The Add row follows the last thumb until the list is long enough to scroll;
  // from then on it stays pinned to the bottom of the rail so it is always
  // reachable. Both readings of "Add pinned under the list" hold, and a rail
  // with one Sound no longer leaves a 300 px hole above the button.
  IRECT RailListRect() const
  {
    const auto rail = RailRect();
    const float top = rail.T + kRailCapH;
    const float maxBottom = rail.B - kAddH - kAddGap;
    return IRECT(rail.L, top, rail.R, std::min(maxBottom, top + std::max(RailContentH(), kRowH)));
  }
  IRECT AddRect() const
  {
    const auto list = RailListRect();
    return IRECT(list.L, list.B + kAddGap, list.R, list.B + kAddGap + kAddH);
  }
  IRECT EmptyRect() const { return IRECT(mRECT.L, HeaderRect().B, mRECT.R, mRECT.B - kBoardBandH); }
  float EmptyAnchorY() const
  {
    const auto empty = EmptyRect();
    return empty.T + empty.H() * 0.36f;
  }
  IRECT EmptyAddRect() const
  {
    const float cx = EmptyRect().MW();
    const float top = EmptyAnchorY() + 108.f;
    return IRECT(cx - 75.f, top, cx + 75.f, top + 35.f);
  }
  IRECT BoardRect() const { return IRECT(mRECT.L + 34.f, mRECT.B - 130.f, mRECT.R - 34.f, mRECT.B - 8.f); }
  IRECT MeterRect(bool out) const
  {
    const auto board = BoardRect();
    return out ? IRECT(mRECT.R - 24.f, board.T + 8.f, mRECT.R - 14.f, board.B - 28.f)
               : IRECT(mRECT.L + 14.f, board.T + 8.f, mRECT.L + 24.f, board.B - 28.f);
  }

  int ActiveRow() const
  {
    for (int i = 0; i < static_cast<int>(mSlots.size()); ++i)
      if (volum::IsLastRecalledSlot(mSlots[(size_t)i], mLastSlot, mActiveAmpId, mActivePresetId))
        return i;
    return -1;
  }

  void DrawHeader(IGraphics& g)
  {
    const auto h = HeaderRect();
    FillVGradient(g, h, IColor(255, 18, 24, 33), IColor(240, 11, 15, 22));
    g.DrawLine(VoLumColors::TEAL_DIM.WithOpacity(0.3f), h.L, h.B, h.R, h.B);
    g.DrawText(VoLumType::Display(26.f, VoLumColors::GOLD, EAlign::Near), "VoLum",
               IRECT(h.L + 16.f, h.T + 3.f, h.L + 170.f, h.T + 29.f));
    g.DrawText(VoLumType::Label(10.f, VoLumColors::GOLD_DIM, EAlign::Near), "NAM PLAYER",
               IRECT(h.L + 18.f, h.T + 27.f, h.L + 170.f, h.T + 41.f));

    // Read-only reminder that PLAY is driven by Program Change. The channel
    // itself is edited in Settings (product law), never here.
    const IRECT chip(h.MW() - 66.f, h.T + 13.f, h.MW() + 66.f, h.T + 33.f);
    g.FillRoundRect(IColor(200, 10, 20, 26), chip, 3.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM.WithOpacity(0.55f), chip, 3.f);
    g.FillCircle(VoLumColors::TEAL, chip.L + 13.f, chip.MH(), 3.f);
    g.DrawText(VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Near), "MIDI IN",
               IRECT(chip.L + 23.f, chip.T, chip.L + 78.f, chip.B));
    const std::string chan = mMidiChannel <= 0 ? "OMNI" : "CH " + std::to_string(mMidiChannel);
    g.DrawText(VoLumType::Label(9.f, VoLumColors::GOLD, EAlign::Near), chan.c_str(),
               IRECT(chip.L + 78.f, chip.T, chip.R - 8.f, chip.B));
  }

  void DrawEmpty(IGraphics& g)
  {
    const auto empty = EmptyRect();
    const float cx = empty.MW();
    const float cy = EmptyAnchorY();
    // A breathing brass halo, not amp art: art behind the headline reads as
    // dirt on the panel rather than an unlit stage.
    // Stacked soft fills, not a stroked ring: a 1 px circle behind the headline
    // read as a stray artifact instead of an unlit stage waiting for a rig.
    const float breathe = 0.5f + 0.5f * std::sin(mPhase);
    DrawSoftGlowCircle(g, cx, cy - 6.f, 176.f, VoLumColors::GOLD.WithOpacity(0.040f + 0.030f * breathe));
    DrawSoftGlowCircle(g, cx, cy - 6.f, 108.f, VoLumColors::GOLD.WithOpacity(0.045f + 0.035f * breathe));
    DrawSoftGlowCircle(g, cx, cy - 6.f, 58.f, VoLumColors::GOLD.WithOpacity(0.055f + 0.040f * breathe));

    g.DrawText(VoLumType::Display(38.f, VoLumColors::CREAM), "No Sounds assigned",
               IRECT(empty.L, cy - 26.f, empty.R, cy + 14.f));
    DrawDiamond(g, cx, cy + 24.f, 3.5f, VoLumColors::GOLD_DIM);
    g.DrawText(VoLumType::Body(12.f, VoLumColors::CREAM_DIM), "PLAY recalls your rigs by MIDI Program Change.",
               IRECT(empty.L, cy + 36.f, empty.R, cy + 54.f));
    g.DrawText(VoLumType::Body(12.f, VoLumColors::CREAM_DIM), "Add a Sound to give this stage something to light up.",
               IRECT(empty.L, cy + 56.f, empty.R, cy + 74.f));

    const IRECT add = EmptyAddRect();
    const bool hot = mHoverRow == kHoverAdd;
    g.FillRoundRect(VoLumColors::GOLD.WithOpacity(hot ? 0.16f : 0.07f), add, 2.f);
    g.DrawRoundRect(VoLumColors::GOLD_DIM, add, 2.f);
    g.DrawText(VoLumType::Label(12.f, VoLumColors::GOLD), "+   Add Sound", add);
  }

  void DrawAmpPanel(IGraphics& g, const IRECT& rect, const std::string& name, int art, bool custom, bool support)
  {
    // No glow behind the art: BUILD's hero (VoLumHero.h) is background + fractal,
    // and a glow scaled to this much larger panel washed the whole stage brown.
    g.FillRect(VoLumColors::HERO_BG, rect);
    const IRECT artRect = rect.GetPadded(-18.f);
    if (custom)
      DrawCustomAmpArt(g, artRect, art, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
    else
      // FractalCaseForAmp, not the raw amp index: BUILD's hero (VoLumHero.h) maps
      // through it, and without the mapping the same amp showed a different
      // fractal in PLAY than in BUILD.
      DrawHeroFractalArt(g, artRect, FractalCaseForAmp(art));
    g.FillRect(IColor(85, 4, 6, 10), IRECT(rect.L, rect.B - 44.f, rect.R, rect.B));
    g.DrawRect((support ? VoLumColors::TEAL_DIM : VoLumColors::GOLD_DIM).WithOpacity(0.7f), rect);
    const float acc = 14.f;
    const IColor corner = (support ? VoLumColors::TEAL : VoLumColors::CORNER).WithOpacity(0.8f);
    DrawCornerAccent(g, rect.L + 6.f, rect.T + 6.f, acc, false, false, corner);
    DrawCornerAccent(g, rect.R - 6.f, rect.T + 6.f, acc, true, false, corner);
    // Only tag the panel in dual, where MAIN vs SUPPORT is the whole point. In
    // mono the amp name is already the overlay's secondary line, and printing it
    // twice on one panel just crowds the corner accents.
    if (mDual)
    {
      const std::string role = (support ? "SUPPORT · " : "MAIN · ") + name;
      g.DrawText(VoLumType::Label(9.f, support ? VoLumColors::TEAL : VoLumColors::GOLD_DIM, EAlign::Near), role.c_str(),
                 IRECT(rect.L + 24.f, rect.T + 4.f, rect.R - 8.f, rect.T + 20.f));
    }
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
    std::string secondary;
    int pc = -1;
    if (active >= 0)
    {
      title = mSlots[(size_t)active].sound.presetName;
      secondary = mSlots[(size_t)active].sound.ampName;
      pc = mSlots[(size_t)active].slot;
    }

    const IRECT overlay(art.L, art.B - 58.f, art.R, art.B);
    g.FillRect(IColor(205, 5, 7, 11), overlay);
    // 0.5, not 0.22: several amp arts draw their own bright horizon rule, and at
    // 0.22 the band's top edge disappeared into the artwork.
    g.DrawLine(VoLumColors::GOLD_DIM.WithOpacity(0.5f), overlay.L, overlay.T, overlay.R, overlay.T);
    // No PC slug when nothing has been recalled yet: a "LIVE" chip with no
    // program number claimed a recall that never happened.
    const float titleL = pc >= 0 ? overlay.L + 66.f : overlay.L + 22.f;
    if (pc >= 0)
      g.DrawText(VoLumType::Value(19.f, VoLumColors::GOLD, EAlign::Near), TwoDigits(pc).c_str(),
                 IRECT(overlay.L + 18.f, overlay.T + 12.f, overlay.L + 60.f, overlay.B - 8.f));

    const IText titleText = VoLumType::Display(26.f, VoLumColors::TEXT_BRIGHT, EAlign::Near);
    const IRECT titleRect(titleL, overlay.T + 4.f, overlay.R - 120.f, overlay.B - 4.f);
    g.PathClipRegion(titleRect);
    g.DrawText(titleText, title.c_str(), titleRect);
    g.PathClipRegion();
    if (!secondary.empty())
    {
      IRECT measured = titleRect;
      g.MeasureText(titleText, title.c_str(), measured);
      const float sx = std::min(titleRect.L + measured.W() + 14.f, titleRect.R - 90.f);
      g.DrawText(VoLumType::Label(10.f, VoLumColors::TEAL_DIM, EAlign::Near), secondary.c_str(),
                 IRECT(sx, overlay.T + 20.f, titleRect.R, overlay.B - 12.f));
    }
    if (mDirty)
      g.DrawText(VoLumType::Label(10.f, VoLumColors::AMBER, EAlign::Far), "(unsaved)",
                 IRECT(overlay.R - 112.f, overlay.T + 20.f, overlay.R - 14.f, overlay.B - 12.f));
    DrawRail(g);
  }

  void DrawRail(IGraphics& g)
  {
    const auto rail = RailRect();
    g.DrawText(VoLumType::Label(9.f, VoLumColors::TEAL_DIM, EAlign::Near), "SOUNDS",
               IRECT(rail.L + 2.f, rail.T, rail.R - 30.f, rail.T + 15.f));
    g.DrawText(VoLumType::Label(9.f, VoLumColors::GOLD_DIM, EAlign::Far), "PC",
               IRECT(rail.L, rail.T, rail.R - 2.f, rail.T + 15.f));

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
      if (row.B > list.T + 0.5f && row.T < list.B - 0.5f)
        DrawSlot(g, row, i, list);
    }
    if (sticky >= 0)
      DrawSlot(g, RailRowRect(sticky, true), sticky, list);
    g.PathClipRegion();

    const float maxScroll = RailMaxScroll();
    if (maxScroll > 0.5f)
    {
      const IRECT track(list.R - 4.f, list.T, list.R, list.B);
      const float contentH = RailContentH();
      const float thumbH = std::max(16.f, track.H() * list.H() / contentH);
      const float t = mRailScroll / maxScroll;
      const float top = track.T + (track.H() - thumbH) * t;
      DrawVoLumScrollbar(g, track, IRECT(track.L, top, track.R, top + thumbH));
    }

    // Dashed slot, not a solid button: the mock's rail Add reads as "an empty
    // place a Sound could go", which is also what distinguishes it from a row.
    const IRECT add = AddRect();
    const bool hot = mHoverRow == kHoverAdd;
    g.FillRect(VoLumColors::GOLD.WithOpacity(hot ? 0.12f : 0.03f), add);
    g.DrawDottedRect(VoLumColors::GOLD_DIM.WithOpacity(hot ? 0.95f : 0.6f), add, nullptr, 1.f, 4.f);
    g.DrawText(VoLumType::Label(15.f, VoLumColors::GOLD.WithOpacity(hot ? 1.f : 0.8f)), "+", add);
  }

  // clip is the rail list rect: IGraphics has no clip stack, so every inner
  // PathClipRegion must restore the caller's clip instead of clearing it, or the
  // last row escapes the list and paints over the pinned Add button.
  void DrawSlot(IGraphics& g, const IRECT& row, int index, const IRECT& clip)
  {
    const auto& slot = mSlots[(size_t)index];
    const bool active = index == ActiveRow();
    const bool hovered = mHoverRow == index;
    g.FillRect(slot.valid ? IColor(235, 10, 14, 20) : IColor(235, 26, 12, 15), row);
    DrawVoLumSelection(g, row, active, hovered && !active, VoLumSelectionStyle::Brass, 2.f, 0.f);
    if (!active)
      g.DrawRect(slot.valid ? VoLumColors::TEAL_DIM.WithOpacity(0.42f) : VoLumColors::DANGER.WithOpacity(0.55f), row);

    const float textL = row.L + 56.f;
    if (slot.valid)
    {
      // A full-height tile of the amp's own hero art, flush to the row's left
      // edge. The 22 px sidebar mini fractal does not scale to 46x60: it drew a
      // small figure in the corner of an inset well and read as an icon slot.
      const IRECT thumb(row.L + 1.f, row.T + 1.f, row.L + 49.f, row.B - 1.f);
      g.FillRect(VoLumColors::HERO_BG, thumb);
      g.PathClipRegion(thumb);
      if (slot.sound.customArt)
        DrawCustomAmpArt(g, thumb, slot.sound.art, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
      else
        DrawHeroFractalArt(g, thumb, FractalCaseForAmp(slot.sound.art));
      g.PathClipRegion(clip);
      g.DrawLine(VoLumColors::TEAL_DIM.WithOpacity(0.45f), thumb.R, thumb.T, thumb.R, thumb.B);
      g.DrawText(VoLumType::Value(17.f, VoLumColors::GOLD, EAlign::Near), TwoDigits(slot.slot).c_str(),
                 IRECT(textL, row.T + 4.f, row.R - 40.f, row.T + 24.f));
      // IGraphics has no clip stack, so restore the caller's clip rather than
      // clearing it, and skip the draw when the intersection is degenerate: an
      // inverted scissor rect is treated as "no clip" and let rows escape the
      // list to paint over the pinned Add button.
      auto clipped = [&](const IRECT& r, const IText& t, const char* s) {
        const IRECT c = r.Intersect(clip);
        if (c.W() <= 0.f || c.H() <= 0.f)
          return;
        g.PathClipRegion(c);
        g.DrawText(t, s, r);
        g.PathClipRegion(clip);
      };
      clipped(IRECT(textL, row.T + 23.f, row.R - 8.f, row.T + 43.f),
              VoLumType::Display(17.f, active ? VoLumColors::SEL_TEXT : VoLumColors::CREAM, EAlign::Near),
              slot.sound.presetName.c_str());
      clipped(IRECT(textL, row.T + 43.f, row.R - 8.f, row.T + 57.f),
              VoLumType::Label(9.f, VoLumColors::TEAL_DIM, EAlign::Near), slot.sound.ampName.c_str());
      // LIVE and the clear affordance share the row's top-right corner: while the
      // pointer is on the row you get the action, otherwise the state. The brass
      // selection border already says "live", so nothing is lost.
      if (active && !hovered)
      {
        g.FillCircle(VoLumColors::GOLD, row.R - 40.f, row.T + 14.f, 2.5f);
        g.DrawText(VoLumType::Label(8.f, VoLumColors::GOLD, EAlign::Far), "LIVE",
                   IRECT(row.L, row.T + 7.f, row.R - 8.f, row.T + 21.f));
      }
    }
    else
    {
      // Same text column as a valid row: the number is the one thing a numbered
      // hole still has, and indenting it to the empty thumb well broke the rail's
      // vertical rhythm.
      g.DrawText(VoLumType::Value(17.f, VoLumColors::CREAM_DIM, EAlign::Near), TwoDigits(slot.slot).c_str(),
                 IRECT(textL, row.T + 13.f, row.R - 8.f, row.T + 33.f));
      // Same words as the Settings Sound map ("missing sound"): the slot is fine,
      // the Sound it pointed at is gone, and two names for one state is a bug.
      g.DrawText(VoLumType::Label(10.f, VoLumColors::DANGER, EAlign::Near), "MISSING SOUND",
                 IRECT(textL, row.T + 34.f, row.R - 8.f, row.T + 50.f));
    }
    // Clear is a hover affordance: a permanent glyph on every row read as noise
    // at 8 px and was too small to hit.
    if (hovered)
    {
      const IRECT clear = ClearRectForRow(index, row);
      g.FillRoundRect(VoLumColors::DANGER_FILL, clear, 2.f);
      g.DrawRoundRect(VoLumColors::DANGER, clear, 2.f);
      DrawCrossGlyph(g, clear, VoLumColors::TEXT_BRIGHT);
    }
  }

  void DrawBoard(IGraphics& g)
  {
    const auto board = BoardRect();
    DrawPanelDepth(g, board);
    g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(0.38f), board);
    g.DrawText(VoLumType::Label(9.f, VoLumColors::TEAL_DIM, EAlign::Near), "Pre  ·  stomp to bypass the live rig",
               IRECT(board.L + 8.f, board.T + 3.f, FxRect(3).R, board.T + 18.f));
    g.DrawText(VoLumType::Label(9.f, VoLumColors::GOLD_DIM, EAlign::Near), "Post",
               IRECT(FxRect(4).L, board.T + 3.f, board.R - 8.f, board.T + 18.f));
    for (int i = 0; i < FxCount; ++i)
      DrawStomp(g, i);
    DrawMeter(g, false);
    DrawMeter(g, true);
  }

  void DrawStomp(IGraphics& g, int i)
  {
    static const char* kNames[FxCount] = {"PITCH", "COMP", "NAM 1", "NAM 2", "CHORUS", "DELAY", "REVERB", "TREM"};
    const IRECT r = FxRect(i);
    const bool on = mFx[(size_t)i];
    const bool live = mFxAvailable[(size_t)i];
    const bool hovered = mHoverFx == i;
    DrawInsetWell(g, r, 2.f);
    if (on)
    {
      g.FillRect(IColor(58, 252, 222, 145), r);
      g.DrawRect(VoLumColors::SEL_BORDER, r);
    }
    else
    {
      if (hovered)
        g.FillRect(VoLumColors::SEL_BG_SOFT, r);
      g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(live ? (hovered ? 0.5f : 0.32f) : 0.13f), r);
    }

    // The well carries the pedal's own motif, same as the Quiet strip and the
    // pedal cards. Eight identical footswitch rings gave the board no identity
    // beyond the caption, and the Opus D board is motif-first.
    static const EVoLumEffectFocus kFocus[FxCount] = {
      EVoLumEffectFocus::PITCH,  EVoLumEffectFocus::COMP,   EVoLumEffectFocus::PRE_NAM1,
      EVoLumEffectFocus::PRE_NAM2, EVoLumEffectFocus::CHORUS, EVoLumEffectFocus::DELAY,
      EVoLumEffectFocus::REVERB, EVoLumEffectFocus::TREMOLO};
    const IRECT motif(r.L + 6.f, r.T + 5.f, r.R - 6.f, r.B - 26.f);
    // Clip: the motifs bloom and glow past their rect (a lit NAM 1 threw gold
    // nodes into the NAM 2 well), and wells here sit 4 px apart.
    g.PathClipRegion(motif);
    // Variant stays 0: the PLAY caption is the fixed "PITCH" from the spec, so a
    // motif that switched to the Octaver chevrons would contradict its own label.
    DrawEffectMotif(g, motif, kFocus[i], !on);
    g.PathClipRegion();
    // An effect the current rig cannot reach (NAM 2 in mono) reads as furniture,
    // not as a stomp you failed to hit: veil the motif rather than hide it.
    if (!live)
      g.FillRect(IColor(150, 9, 12, 17), motif);
    if (on)
      g.FillCircle(VoLumColors::GOLD, r.R - 12.f, r.T + 11.f, 2.5f);

    const IColor ink =
      on ? VoLumColors::GOLD : (live ? VoLumColors::CREAM_DIM : VoLumColors::CREAM_DIM.WithOpacity(0.35f));
    g.DrawText(VoLumType::Label(9.f, ink), kNames[i], IRECT(r.L, r.B - 24.f, r.R, r.B - 8.f));
  }

  void DrawMeter(IGraphics& g, bool out)
  {
    const IRECT r = MeterRect(out);
    DrawInsetWell(g, r, 1.f);
    const float level = 0.3f + 0.42f * (0.5f + 0.5f * std::sin(mPhase + (out ? 0.45f : 0.f)));
    // Segmented ladder: a solid bar in a 10 px well reads as a stray rectangle.
    const int segments = 18;
    const float segH = (r.H() - 4.f) / segments;
    const IColor lit = out ? VoLumColors::TEAL : VoLumColors::GOLD;
    for (int i = 0; i < segments; ++i)
    {
      const float frac = static_cast<float>(segments - i) / segments;
      const float top = r.T + 2.f + i * segH;
      const IRECT seg(r.L + 2.f, top, r.R - 2.f, top + segH - 1.f);
      g.FillRect(frac <= level ? lit : lit.WithOpacity(0.10f), seg);
    }
    g.DrawText(VoLumType::Label(8.f, VoLumColors::CREAM_DIM), out ? "OUT" : "IN",
               IRECT(r.L - 6.f, r.B + 4.f, r.R + 6.f, r.B + 20.f));
  }

  void DrawPicker(IGraphics& g)
  {
    const IRECT panel = PickerRect();
    g.FillRoundRect(IColor(250, 8, 11, 16), panel, 4.f);
    g.DrawRoundRect(VoLumColors::GOLD_DIM, panel, 4.f);
    g.DrawText(VoLumType::Display(24.f, VoLumColors::CREAM), "Choose Sound",
               IRECT(panel.L, panel.T + 10.f, panel.R, panel.T + 42.f));
    g.DrawText(VoLumType::Label(10.f, VoLumColors::CREAM_DIM), ("Assign to PC " + TwoDigits(mEditSlot)).c_str(),
               IRECT(panel.L, panel.T + 40.f, panel.R, panel.T + 58.f));
    DrawCrossGlyph(g, PickerCloseRect(), VoLumColors::GOLD_DIM, 1.5f);

    const IRECT list = PickerListRect();
    g.PathClipRegion(list);
    float y = list.T - mPickerScroll;
    bool factoryHeader = false, userHeader = false;
    for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
    {
      const bool isFactory = mChoices[(size_t)i].factory;
      if ((isFactory && !factoryHeader) || (!isFactory && !userHeader))
      {
        const char* label = isFactory ? "FACTORY" : "USER";
        g.DrawText(VoLumType::Label(9.f, isFactory ? VoLumColors::GOLD_DIM : VoLumColors::TEAL_DIM, EAlign::Near),
                   label, IRECT(list.L + 6.f, y, list.R, y + kPickerCapH));
        g.DrawLine(VoLumColors::FRAME.WithOpacity(0.5f), list.L + 58.f, y + kPickerCapH * 0.5f, list.R - 6.f,
                   y + kPickerCapH * 0.5f);
        y += kPickerCapH;
        factoryHeader |= isFactory;
        userHeader |= !isFactory;
      }
      const IRECT row(list.L, y, list.R, y + kPickerRowH);
      if (row.B >= list.T && row.T <= list.B)
      {
        DrawVoLumSelection(g, row, false, mHoverChoice == i, VoLumSelectionStyle::ListTeal, 2.f, 1.f);
        g.DrawText(VoLumType::Body(13.f, VoLumColors::CREAM, EAlign::Near), mChoices[(size_t)i].presetName.c_str(),
                   IRECT(row.L + 10.f, row.T, row.MW(), row.B));
        g.DrawText(VoLumType::Label(9.f, VoLumColors::TEAL_DIM, EAlign::Far), mChoices[(size_t)i].ampName.c_str(),
                   IRECT(row.MW(), row.T, row.R - 10.f, row.B));
      }
      y += kPickerRowH;
    }
    g.PathClipRegion();

    const float maxScroll = PickerMaxScroll();
    if (maxScroll > 0.5f)
    {
      const IRECT track(list.R + 2.f, list.T, list.R + 6.f, list.B);
      const float thumbH = std::max(18.f, track.H() * list.H() / PickerContentHeight());
      const float top = track.T + (track.H() - thumbH) * (mPickerScroll / maxScroll);
      DrawVoLumScrollbar(g, track, IRECT(track.L, top, track.R, top + thumbH));
    }
  }

  int SlotAt(float x, float y) const
  {
    if ((mSlots.empty() ? EmptyAddRect() : AddRect()).Contains(x, y))
      return kHoverAdd;
    const auto list = RailListRect();
    if (mSlots.empty() || !list.Contains(x, y))
      return -1;
    const int active = ActiveRow();
    if (active >= 0)
    {
      const auto natural = RailRowRect(active, false);
      if (natural.T < list.T && RailRowRect(active, true).Contains(x, y))
        return active;
    }
    const int row = static_cast<int>((y - list.T + mRailScroll) / kRowPitch);
    if (row < 0 || row >= static_cast<int>(mSlots.size()))
      return -1;
    return RailRowRect(row, false).Contains(x, y) ? row : -1;
  }
  IRECT ClearRectForRow(int index) const
  {
    if (index < 0 || index >= static_cast<int>(mSlots.size()))
      return IRECT();
    const auto natural = RailRowRect(index, false);
    const bool sticky = index == ActiveRow() && natural.T < RailListRect().T;
    return ClearRectForRow(index, sticky ? RailRowRect(index, true) : natural);
  }
  // Top-right, not bottom-right: at the bottom this box sat on top of the row's
  // amp-name line.
  IRECT ClearRectForRow(int, const IRECT& row) const { return IRECT(row.R - 26.f, row.T + 5.f, row.R - 6.f, row.T + 23.f); }
  IRECT FxRect(int i) const
  {
    const auto board = BoardRect();
    const float left = board.L + 6.f;
    const float right = board.R - 6.f;
    const float gap = 4.f;
    const float divider = 16.f;
    const float width = (right - left - gap * 7.f - divider) / 8.f;
    const float x = left + i * (width + gap) + (i >= 4 ? divider : 0.f);
    return IRECT(x, board.T + 21.f, x + width, board.B - 14.f);
  }
  float RailMaxScroll() const { return std::max(0.f, RailContentH() - RailListRect().H()); }
  void ClampRailScroll() { mRailScroll = std::clamp(mRailScroll, 0.f, RailMaxScroll()); }
  IRECT RailRowRect(int index, bool sticky) const
  {
    const auto list = RailListRect();
    const float top = sticky ? list.T : list.T + index * kRowPitch - mRailScroll;
    return IRECT(list.L, top, list.R - (RailMaxScroll() > 0.5f ? 8.f : 0.f), top + kRowH);
  }
  float PickerContentHeight() const
  {
    if (mChoices.empty())
      return 0.f;
    bool hasFactory = false, hasUser = false;
    for (const auto& c : mChoices)
      c.factory ? hasFactory = true : hasUser = true;
    return static_cast<float>(mChoices.size()) * kPickerRowH + (hasFactory ? kPickerCapH : 0.f)
           + (hasUser ? kPickerCapH : 0.f);
  }
  IRECT PickerRect() const
  {
    const auto empty = EmptyRect();
    const float h = std::clamp(PickerContentHeight() + 82.f, 180.f, empty.H() - 24.f);
    const float cy = empty.MH();
    return IRECT(mRECT.MW() - 205.f, cy - h * 0.5f, mRECT.MW() + 205.f, cy + h * 0.5f);
  }
  IRECT PickerListRect() const
  {
    const auto panel = PickerRect();
    return IRECT(panel.L + 14.f, panel.T + 64.f, panel.R - 20.f, panel.B - 12.f);
  }
  IRECT PickerCloseRect() const
  {
    const auto panel = PickerRect();
    return IRECT(panel.R - 30.f, panel.T + 6.f, panel.R - 6.f, panel.T + 30.f);
  }
  float PickerMaxScroll() const { return std::max(0.f, PickerContentHeight() - PickerListRect().H()); }
  int PickerChoiceAt(float x, float y) const
  {
    const IRECT list = PickerListRect();
    if (!list.Contains(x, y))
      return -1;
    float rowY = list.T - mPickerScroll;
    bool factoryHeader = false, userHeader = false;
    for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
    {
      const bool factory = mChoices[(size_t)i].factory;
      if ((factory && !factoryHeader) || (!factory && !userHeader))
      {
        rowY += kPickerCapH;
        factoryHeader |= factory;
        userHeader |= !factory;
      }
      if (IRECT(list.L, rowY, list.R, rowY + kPickerRowH).Contains(x, y))
        return i;
      rowY += kPickerRowH;
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

  static constexpr int kHoverAdd = -2;

  std::vector<volum::PlaySlot> mSlots;
  std::vector<volum::SoundChoice> mChoices;
  std::string mActiveAmpId, mActivePresetId, mLiveAmpName, mSupportName;
  int mLastSlot = -1, mLiveArt = 0, mSupportArt = 0, mEditSlot = -1, mMidiChannel = 0;
  int mHoverRow = -1, mHoverFx = -1, mHoverChoice = -1;
  bool mCustomArt = false, mDual = false, mSupportCustom = false, mDirty = false, mPickerOpen = false;
  std::array<bool, FxCount> mFx{};
  std::array<bool, FxCount> mFxAvailable{};
  float mRailScroll = 0.f, mPickerScroll = 0.f, mPhase = 0.f;
  RecallCallback mRecall;
  AssignCallback mAssign;
  ClearCallback mClear;
  BypassCallback mBypass;
};
