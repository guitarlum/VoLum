#pragma once

// Opus D PLAY chrome: centre live art, Program Change thumb rail, and bypass-only
// PRE/POST board. Product/state decisions live in VoLumPlayModel.h; this file owns
// drawing and interaction so VoLumControls.h stays an umbrella.

#include "VoLumColorHelpers.h"
#include "VoLumFractalArt.h"
#include "VoLumNumericEntry.h"
#include "VoLumPlayLight.h"
#include "VoLumPlayModel.h"
#include "VoLumScroll.h"
#include "VoLumTriptychMotifs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/** Destination toggle in the header cluster.
 *
 * One control. Idle shows the other mode (where a click will go): BUILD's
 * faders while you are in PLAY, PLAY's stomp ring while you are in BUILD.
 * Larger than the 26 px tuner / metronome / gear circles; same brass family.
 * Z-order is fixed at the attach site in VoLumLayoutBuild.inc.cpp: this goes
 * on before the overlays, so they cover it. */
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
    const volum::UiMode dest = Destination();
    DrawVoLumSelection(g, mRECT, false, mMouseIsOver, VoLumSelectionStyle::Brass, 3.f, 1.5f);
    g.DrawRoundRect(VoLumColors::GOLD_DIM.WithOpacity(mMouseIsOver ? 0.95f : 0.72f), mRECT, 4.f, nullptr, 1.5f);
    const IColor ink =
      SelectionInkColor(VoLumSelectionStyle::Brass, mMouseIsOver).WithOpacity(mMouseIsOver ? 1.f : 0.9f);
    const IRECT glyph = mRECT.GetPadded(-8.f, -4.f, -8.f, -4.f);
    if (dest == volum::UiMode::Play)
      DrawPlayGlyph(g, glyph, ink);
    else
      DrawBuildGlyph(g, glyph, ink);
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    if (mCallback)
      mCallback(Destination());
  }
  void OnMouseOver(float, float, const IMouseMod&) override
  {
    mMouseIsOver = true;
    const char* tip = Destination() == volum::UiMode::Play ? "PLAY" : "BUILD";
    if (mTip != tip)
    {
      mTip = tip;
      SetTooltip(tip);
    }
    SetDirty(false);
  }
  void OnMouseOut() override
  {
    mMouseIsOver = false;
    if (!mTip.empty())
    {
      mTip.clear();
      SetTooltip("");
    }
    SetDirty(false);
  }

private:
  volum::UiMode Destination() const
  {
    return mMode == volum::UiMode::Play ? volum::UiMode::Build : volum::UiMode::Play;
  }

  static void DrawPlayGlyph(IGraphics& g, const IRECT& r, const IColor& ink)
  {
    g.DrawCircle(ink, r.MW(), r.MH(), 10.2f, nullptr, 1.8f);
    g.FillCircle(ink, r.MW(), r.MH(), 3.1f);
  }

  static void DrawBuildGlyph(IGraphics& g, const IRECT& r, const IColor& ink)
  {
    static const float kX[3] = {-7.4f, 0.f, 7.4f};
    static const float kCapY[3] = {-3.0f, 4.2f, -5.6f};
    for (int i = 0; i < 3; ++i)
    {
      const float x = r.MW() + kX[i];
      g.DrawLine(ink, x, r.MH() - 9.4f, x, r.MH() + 9.4f, nullptr, 1.6f);
      g.DrawLine(ink, x - 3.0f, r.MH() + kCapY[i], x + 3.0f, r.MH() + kCapY[i], nullptr, 2.6f);
    }
  }

  volum::UiMode mMode = volum::UiMode::Build;
  bool mMouseIsOver = false;
  std::string mTip;
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
  using EditInBuildCallback = std::function<void(int)>;
  using AddHeardCallback = std::function<void()>;

  VoLumPlaySurfaceControl(const IRECT& bounds, RecallCallback recall, AssignCallback assign, ClearCallback clear,
                          BypassCallback bypass, EditInBuildCallback edit, AddHeardCallback addHeard)
  : IControl(bounds)
  , mRecall(std::move(recall))
  , mAssign(std::move(assign))
  , mClear(std::move(clear))
  , mBypass(std::move(bypass))
  , mEditInBuild(std::move(edit))
  , mAddHeard(std::move(addHeard))
  {
  }

  void SetPlusAddsHeard(bool addsHeard)
  {
    if (mPlusAddsHeard == addsHeard)
      return;
    mPlusAddsHeard = addsHeard;
    SetDirty(false);
  }

  void SetInPeak(float peak) { mInPeak = std::clamp(peak, 0.f, 1.f); }

  void SetPickerGroups(volum::PickerGroupSession* session) { mPickerGroups = session; }

  void SetData(const std::vector<volum::FactoryPreset>& factory, const volum::content::Registry& registry,
               const std::string& activeAmpId, const std::string& activePresetId, int lastSlot,
               const std::string& liveAmpName, int liveArt, bool customArt, bool dual, const std::string& supportName,
               int supportArt, bool supportCustom, const std::array<bool, FxCount>& fx,
               const std::array<bool, FxCount>& fxAvailable, int midiChannel, bool dirty,
               const char* nam1Label = nullptr, const char* nam2Label = nullptr)
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
    mNam1Label = (nam1Label && nam1Label[0]) ? nam1Label : "NAM 1";
    mNam2Label = (nam2Label && nam2Label[0]) ? nam2Label : "NAM 2";
    if (mCachedMainArt != mLiveArt || mCachedMainCustom != mCustomArt)
    {
      mStageMainLayer = nullptr;
      mCachedMainArt = mLiveArt;
      mCachedMainCustom = mCustomArt;
    }
    if (mCachedSupportArt != mSupportArt || mCachedSupportCustom != mSupportCustom)
    {
      mStageSupportLayer = nullptr;
      mCachedSupportArt = mSupportArt;
      mCachedSupportCustom = mSupportCustom;
    }
    ClampRailScroll();
    SetDirty(false);
  }

  void OnRescale() override
  {
    // The backing pixel scale changed (window resize / DPI). Drop the cached art
    // layers so they re-render crisp instead of being blitted at the old
    // resolution, exactly as the sidebar amp list does.
    for (auto& layer : mFactoryArtLayers)
      layer = nullptr;
    for (auto& layer : mCustomArtLayers)
      layer = nullptr;
    mStageMainLayer = nullptr;
    mStageSupportLayer = nullptr;
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
    // The same top-lit gradient + vignette + brass frame BUILD's canvas draws
    // (VoLumBackgroundControl). PLAY used to open on a blue-green (20, 26, 36)
    // wash, so switching modes changed the colour of the instrument.
    FillVGradient(g, mRECT, IColor(255, 21, 21, 29), IColor(255, 12, 12, 18));
    DrawVignette(g, mRECT, 72);
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

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mPickerOpen)
    {
      if (PickerCloseRect().Contains(x, y))
      {
        mPickerOpen = false;
        SetDirty(false);
        return;
      }
      if (mSlotEditable)
      {
        if (PickerSlotStepRect(-1).Contains(x, y))
        {
          SetEditSlot(mEditSlot - 1);
          return;
        }
        if (PickerSlotStepRect(1).Contains(x, y))
        {
          SetEditSlot(mEditSlot + 1);
          return;
        }
        if (PickerSlotValueRect().Contains(x, y))
        {
          StartSlotEntry();
          return;
        }
      }
      const auto pickerM = PickerScrollMetrics();
      const IRECT pickerTrack = PickerTrackRect();
      if (mPickerBar.OnDown(x, y, pickerTrack.L, pickerTrack.R, pickerM))
      {
        mPickerScroll = volum::scroll::ThumbYToScroll(
          y - mPickerBar.grabDY, pickerM.trackTop, pickerM.trackH, pickerM.thumbH, pickerM.maxScroll);
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
      {
        mAssign(mEditSlot, mChoices[(size_t)choice]);
        mPickerOpen = false;
        SetDirty(false);
        return;
      }
      if (!PickerRect().Contains(x, y))
        mPickerOpen = false;
      SetDirty(false);
      return;
    }

    const auto railM = RailScrollMetrics();
    const IRECT railTrack = RailTrackRect();
    if (mRailBar.OnDown(x, y, railTrack.L, railTrack.R, railM))
    {
      mRailScroll = mRailScrollTarget =
        volum::scroll::ThumbYToScroll(y - mRailBar.grabDY, railM.trackTop, railM.trackH, railM.thumbH, railM.maxScroll);
      SetDirty(false);
      return;
    }

    if ((mSlots.empty() ? EmptyAddRect() : AddRect()).Contains(x, y))
    {
      if (mPlusAddsHeard && mAddHeard)
      {
        mAddHeard();
        return;
      }
      OpenPicker(FirstFreeSlot(), true);
      return;
    }

    const int row = SlotAt(x, y);
    if (row >= 0)
    {
      if (ClearRectForRow(row).Contains(x, y))
      {
        if (mClear)
          mClear(mSlots[(size_t)row].slot);
        return;
      }
      if (AssignRectForRow(row).Contains(x, y))
      {
        OpenPicker(mSlots[(size_t)row].slot, false);
        return;
      }
      const auto& slot = mSlots[(size_t)row];
      if (slot.valid)
      {
        if (mRecall)
          mRecall(slot.slot, slot.sound);
      }
      else if (!mChoices.empty())
        OpenPicker(slot.slot, false);
      return;
    }

    for (int i = 0; i < FxCount; ++i)
    {
      if (!FxRect(i).Contains(x, y))
        continue;
      if (mod.R)
      {
        if (mEditInBuild)
          mEditInBuild(static_cast<int>(kStompFocus[static_cast<size_t>(i)]));
        return;
      }
      if (mBypass)
        mBypass(volum::kPlayBypassParamNames[static_cast<size_t>(i)]);
      return;
    }
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override
  {
    if (mRailBar.dragging)
    {
      const auto m = RailScrollMetrics();
      const float next = mRailBar.OnDrag(y, m);
      if (next >= 0.f)
        mRailScroll = mRailScrollTarget = next;
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

  void OnMouseUp(float, float, const IMouseMod&) override
  {
    mRailBar.OnUp();
    mPickerBar.OnUp();
  }

  void OnMouseDblClick(float x, float y, const IMouseMod&) override
  {
    const int row = SlotAt(x, y);
    if (row < 0 || row >= static_cast<int>(mSlots.size()) || mChoices.empty())
      return;
    OpenPicker(mSlots[(size_t)row].slot, false);
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
    int step = 0;
    if (mPickerOpen && mSlotEditable)
      step = PickerSlotStepRect(-1).Contains(x, y) ? -1 : (PickerSlotStepRect(1).Contains(x, y) ? 1 : 0);
    if (row != mHoverRow || fx != mHoverFx || choice != mHoverChoice || step != mHoverStep)
    {
      mHoverRow = row;
      mHoverFx = fx;
      mHoverChoice = choice;
      mHoverStep = step;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mHoverRow = mHoverFx = mHoverChoice = -1;
    mHoverStep = 0;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    if (mPickerOpen && PickerRect().Contains(x, y))
    {
      mPickerScroll = std::clamp(mPickerScroll - d * 30.f, 0.f, PickerMaxScroll());
      SetDirty(false);
      return;
    }
    if (!RailListRect().Contains(x, y))
      return;

    // Same two input shapes VoLumAmpList.h separates, because the rail is the same
    // kind of list and jumping a whole kRowPitch per event with no animation made
    // a trackpad flick teleport:
    //  - Precision touchpads stream many fractional events (|d| < 1). Map those
    //    1:1 to pixels with NO easing, or the list lags behind the finger.
    //  - A wheel notch / coarse detent sends |d| >= 1. Glide toward a target so
    //    one notch travels ~2 rows smoothly.
    const float maxScroll = RailMaxScroll();
    if (std::abs(d) < 1.f)
    {
      mRailScrollTarget = std::clamp(mRailScrollTarget - d * kRowPitch, 0.f, maxScroll);
      mRailScroll = mRailScrollTarget;
      SetDirty(false);
    }
    else
    {
      mRailScrollTarget = std::clamp(mRailScrollTarget - d * kRowPitch * 2.f, 0.f, maxScroll);
      StartRailScrollAnim();
    }
    OnMouseOver(x, y, mod);
  }

  void OnTextEntryCompletion(const char* str, int) override
  {
    // The same parser the exact-value box and the Settings Sound map use, not
    // atof: "" and "abc" have to leave the number alone rather than move the new
    // assignment to program 0.
    double parsed = 0.0;
    if (str && volum::ParseNumericEntry(str, parsed))
      mEditSlot = static_cast<int>(std::clamp(std::round(parsed), 0.0, 127.0));
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
  static constexpr float kThumbSize = 42.f; // square row art, inset from the row
  static constexpr float kTextInset = 57.f; // thumb inset + thumb + gutter
  static constexpr float kBannerH = 58.f;

  static constexpr EVoLumEffectFocus kStompFocus[FxCount] = {
    EVoLumEffectFocus::PITCH,  EVoLumEffectFocus::COMP,  EVoLumEffectFocus::PRE_NAM1, EVoLumEffectFocus::PRE_NAM2,
    EVoLumEffectFocus::CHORUS, EVoLumEffectFocus::DELAY, EVoLumEffectFocus::REVERB,   EVoLumEffectFocus::TREMOLO};

  void OpenPicker(int slot, bool numberEditable)
  {
    if (slot < 0 || mChoices.empty())
      return;
    mEditSlot = slot;
    mSlotEditable = numberEditable;
    mPickerOpen = true;
    mPickerScroll = 0.f;
    InitPickerSession();
    SetDirty(false);
  }

  void SetEditSlot(int slot)
  {
    mEditSlot = std::clamp(slot, 0, 127);
    SetDirty(false);
  }

  void StartSlotEntry()
  {
    auto* ui = GetUI();
    if (!ui)
      return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", std::clamp(mEditSlot, 0, 127));
    SetTextEntryLength(3);
    // kNoValIdx so completion comes back as raw text: this control owns no param,
    // and a valIdx would route the string through parameter conversion.
    ui->CreateTextEntry(*this, VoLumType::Value(14.f, VoLumColors::GOLD), PickerSlotValueRect(), buf, kNoValIdx);
  }

  // Ease the rendered offset toward the target each frame, same catch-up rate the
  // sidebar list uses so both lists glide identically.
  void StartRailScrollAnim()
  {
    SetAnimation(
      [this](IControl* pCaller) {
        mRailScroll += (mRailScrollTarget - mRailScroll) * 0.45f;
        if (std::abs(mRailScrollTarget - mRailScroll) < 0.4f)
        {
          mRailScroll = mRailScrollTarget;
          pCaller->OnEndAnimation();
        }
        SetDirty(false);
      },
      350);
  }

  IRECT HeaderRect() const { return IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + kHeaderH); }
  IRECT StageRect() const
  {
    return IRECT(mRECT.L + 10.f, mRECT.T + kHeaderH + 8.f, mRECT.R - 12.f, mRECT.B - kBoardBandH);
  }
  IRECT RailRect() const { return StageRect().GetFromRight(kRailW); }
  IRECT ArtRect() const
  {
    const auto stage = StageRect();
    return IRECT(stage.L, stage.T, RailRect().L - kRailGap, stage.B - kBannerH);
  }
  IRECT BannerRect() const
  {
    const auto stage = StageRect();
    return IRECT(stage.L, stage.B - kBannerH, RailRect().L - kRailGap, stage.B);
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
    FillVGradient(g, h, VoLumColors::PANEL_TOP, VoLumColors::PANEL_BOT);
    g.DrawLine(VoLumColors::FRAME, h.L, h.B, h.R, h.B);
    g.DrawText(VoLumType::Display(26.f, VoLumColors::GOLD, EAlign::Near), "VoLum",
               IRECT(h.L + 16.f, h.T + 3.f, h.L + 170.f, h.T + 29.f));
    g.DrawText(VoLumType::Label(10.f, VoLumColors::GOLD_DIM, EAlign::Near), "NAM PLAYER",
               IRECT(h.L + 18.f, h.T + 27.f, h.L + 170.f, h.T + 41.f));

    // Same 26 px band as tuner / metronome / gear (T+14 .. T+40). Horizontally
    // centred on the PLAY header, where the listen reminder belongs.
    const IRECT chip(h.MW() - 66.f, h.T + 14.f, h.MW() + 66.f, h.T + 40.f);
    g.FillRoundRect(VoLumColors::WELL_DARK, chip, 3.f);
    g.DrawRoundRect(VoLumColors::FRAME, chip, 3.f);
    g.FillCircle(VoLumColors::GOLD_DIM, chip.L + 13.f, chip.MH(), 3.f);
    g.DrawText(VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Near), "MIDI IN",
               IRECT(chip.L + 23.f, chip.T, chip.L + 78.f, chip.B));
    const std::string chan = mMidiChannel <= 0 ? "ALL" : "CH " + std::to_string(mMidiChannel);
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
    g.DrawText(VoLumType::Label(12.f, VoLumColors::GOLD), mPlusAddsHeard ? "+   Add this sound" : "+   Add Sound", add);
  }

  void DrawCachedStageArt(IGraphics& g, const IRECT& artRect, int art, bool custom, ILayerPtr& layer)
  {
    const IRECT paint = artRect.GetPadded(-18.f);
    if (!g.CheckLayer(layer))
    {
      g.StartLayer(this, paint);
      if (custom)
        DrawCustomAmpArt(g, paint, art, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
      else
        DrawHeroFractalArt(g, paint, FractalCaseForAmp(art));
      layer = g.EndLayer();
    }
    g.FillRect(VoLumColors::HERO_BG, artRect);
    if (layer && g.CheckLayer(layer))
      g.DrawFittedLayer(layer, paint, nullptr);
  }

  void DrawAmpPanel(IGraphics& g, const IRECT& rect, const std::string& name, int art, bool custom, bool support)
  {
    DrawCachedStageArt(g, rect, art, custom, support ? mStageSupportLayer : mStageMainLayer);
    const float pulse = volum::PlayIdlePulse(mPhase);
    const float bright = volum::PlayArtBrightness(mInPeak, pulse);
    const float veil = std::clamp(1.f - bright, 0.f, 0.64f);
    g.FillRect(IColor(static_cast<int>(veil * 140.f), 6, 8, 12), rect);
    const float corona = volum::PlayCoronaOpacity(bright);
    const IColor frame = (support ? VoLumColors::TEAL : VoLumColors::GOLD).WithOpacity(corona);
    g.DrawRect(frame, rect, nullptr, 2.2f);
    g.DrawRect(frame.WithOpacity(corona * 0.45f), rect.GetPadded(2.f), nullptr, 3.f);
    const float acc = 14.f;
    const IColor corner = (support ? VoLumColors::TEAL : VoLumColors::CORNER).WithOpacity(0.8f);
    DrawCornerAccent(g, rect.L + 6.f, rect.T + 6.f, acc, false, false, corner);
    DrawCornerAccent(g, rect.R - 6.f, rect.T + 6.f, acc, true, false, corner);
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

    const IRECT banner = BannerRect();
    g.FillRect(IColor(205, 5, 7, 11), banner);
    g.DrawLine(VoLumColors::GOLD_DIM.WithOpacity(0.5f), banner.L, banner.T, banner.R, banner.T);
    const float titleL = pc >= 0 ? banner.L + 66.f : banner.L + 22.f;
    if (pc >= 0)
      g.DrawText(VoLumType::Value(19.f, VoLumColors::GOLD, EAlign::Near), TwoDigits(pc).c_str(),
                 IRECT(banner.L + 18.f, banner.T + 12.f, banner.L + 60.f, banner.B - 8.f));

    const IText titleText = VoLumType::Display(32.f, VoLumColors::TEXT_BRIGHT, EAlign::Near);
    const IRECT titleRect(titleL, banner.T + 4.f, banner.R - 120.f, banner.B - 4.f);
    g.PathClipRegion(titleRect);
    g.DrawText(titleText, title.c_str(), titleRect);
    g.PathClipRegion();
    if (!secondary.empty())
    {
      IRECT measured = titleRect;
      g.MeasureText(titleText, title.c_str(), measured);
      const float sx = std::min(titleRect.L + measured.W() + 14.f, titleRect.R - 90.f);
      g.DrawText(VoLumType::Label(10.f, VoLumColors::TEAL_DIM, EAlign::Near), secondary.c_str(),
                 IRECT(sx, banner.T + 20.f, titleRect.R, banner.B - 12.f));
    }
    if (mDirty)
      g.DrawText(VoLumType::Label(10.f, VoLumColors::AMBER, EAlign::Far), "(unsaved)",
                 IRECT(banner.R - 112.f, banner.T + 20.f, banner.R - 14.f, banner.B - 12.f));
    DrawRail(g);
  }

  void DrawRail(IGraphics& g)
  {
    const auto rail = RailRect();
    // One caption. A second header on the far right used to label a column that
    // is not there: the program number sits at the left of each row next to the
    // art, so that header pointed at the clear affordance instead.
    g.DrawText(VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Near), "SOUNDS",
               IRECT(rail.L + 2.f, rail.T, rail.R - 2.f, rail.T + 15.f));

    // Before any clip is set: StartLayer/EndLayer mutates the clip region.
    BuildRowArtLayers(g);

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

    const auto scroll = RailScrollMetrics();
    if (scroll.maxScroll > 0.5f)
    {
      const IRECT track = RailTrackRect();
      DrawVoLumScrollbar(
        g, track, IRECT(track.L, scroll.thumbY, track.R, scroll.thumbY + scroll.thumbH), mRailBar.dragging);
    }

    // Dashed slot, not a solid button: the mock's rail Add reads as "an empty
    // place a Sound could go", which is also what distinguishes it from a row.
    const IRECT add = AddRect();
    const bool hot = mHoverRow == kHoverAdd;
    g.FillRect(VoLumColors::GOLD.WithOpacity(hot ? 0.12f : 0.03f), add);
    g.DrawDottedRect(VoLumColors::GOLD_DIM.WithOpacity(hot ? 0.95f : 0.6f), add, nullptr, 1.f, 4.f);
    g.DrawText(VoLumType::Label(mPlusAddsHeard ? 11.f : 15.f, VoLumColors::GOLD.WithOpacity(hot ? 1.f : 0.8f)),
               mPlusAddsHeard ? "Add this sound" : "+", add);
  }

  // clip is the rail list rect: IGraphics has no clip stack, so every inner
  // PathClipRegion must restore the caller's clip instead of clearing it, or the
  // last row escapes the list and paints over the pinned Add button.
  void DrawSlot(IGraphics& g, const IRECT& row, int index, const IRECT& clip)
  {
    const auto& slot = mSlots[(size_t)index];
    const bool active = index == ActiveRow();
    const bool hovered = mHoverRow == index;
    g.FillRect(slot.valid ? VoLumColors::WELL_DARK : IColor(235, 26, 12, 15), row);
    DrawVoLumSelection(g, row, active, hovered && !active, VoLumSelectionStyle::Brass, 2.f, 0.f);
    if (!active)
      g.DrawRect(slot.valid ? VoLumColors::FRAME : VoLumColors::DANGER.WithOpacity(0.55f), row);

    const float textL = row.L + kTextInset;
    if (slot.valid)
    {
      // Square, inset and layer-cached, like the sidebar amp list. This used to be
      // a full-height 48x64 tile calling DrawHeroFractalArt per row per frame: the
      // hero renderer cropped to a portrait strip reads as a smear rather than as
      // art, and eight rows of it is a lot of path work for a 48 px tile.
      const IRECT thumb(
        row.L + 7.f, row.MH() - kThumbSize * 0.5f, row.L + 7.f + kThumbSize, row.MH() + kThumbSize * 0.5f);
      g.FillRect(VoLumColors::HERO_BG, thumb);
      // DrawFittedLayer, not DrawFittedBitmap: it translates to the thumb's current
      // position so the art tracks the scroll, and it scales by the layer's LOGICAL
      // bounds so the thumbnail still scales when the window is resized.
      const ILayerPtr& art = RowArtLayer(slot.sound);
      if (art && g.CheckLayer(art))
        g.DrawFittedLayer(art, thumb, nullptr);
      g.DrawRect(VoLumColors::FRAME, thumb);
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
      g.DrawText(VoLumType::Label(10.f, VoLumColors::DANGER, EAlign::Near), volum::kPlayInvalidSlotLabel,
                 IRECT(textL, row.T + 34.f, row.R - 8.f, row.T + 50.f));
    }
    // Reassign and clear are hover affordances, the same pen-and-bin pair the
    // sidebar's custom rows use. Reassign has to be *visible*: shipping the picker
    // behind a double-click only meant nothing advertised that a row could be
    // pointed at a different Sound without deleting it first.
    if (hovered)
    {
      const IRECT assign = AssignRectForRow(index, row);
      g.FillRoundRect(VoLumColors::SEL_BG_SOFT, assign, 2.f);
      g.DrawRoundRect(VoLumColors::GOLD_DIM, assign, 2.f);
      DrawPenGlyph(g, assign, VoLumColors::TEXT_BRIGHT);

      const IRECT clear = ClearRectForRow(index, row);
      g.FillRoundRect(VoLumColors::DANGER_FILL, clear, 2.f);
      g.DrawRoundRect(VoLumColors::DANGER, clear, 2.f);
      DrawCrossGlyph(g, clear, VoLumColors::TEXT_BRIGHT);
    }
  }

  // A diagonal pencil, same construction as the sidebar's custom-amp rows: two
  // parallel body lines, an end cap, and a converging nib.
  static void DrawPenGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH();
    const float t = 1.4f;
    g.DrawLine(col, cx - 4.f, cy + 3.f, cx + 2.f, cy - 3.f, nullptr, t);
    g.DrawLine(col, cx - 2.f, cy + 5.f, cx + 4.f, cy - 1.f, nullptr, t);
    g.DrawLine(col, cx + 2.f, cy - 3.f, cx + 4.f, cy - 1.f, nullptr, t);
    g.DrawLine(col, cx - 4.f, cy + 3.f, cx - 5.f, cy + 6.f, nullptr, t);
    g.DrawLine(col, cx - 2.f, cy + 5.f, cx - 5.f, cy + 6.f, nullptr, t);
  }

  void DrawBoard(IGraphics& g)
  {
    const auto board = BoardRect();
    DrawPanelDepth(g, board);
    g.DrawRect(VoLumColors::FRAME, board);
    // Two quiet band labels and nothing else. A sentence under Pre used to explain
    // a footswitch to someone already standing on one, and it was the widest piece
    // of text on the board.
    g.DrawText(VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Near), "Pre",
               IRECT(board.L + 8.f, board.T + 3.f, FxRect(3).R, board.T + 18.f));
    g.DrawText(VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Near), "Post",
               IRECT(FxRect(4).L, board.T + 3.f, board.R - 8.f, board.T + 18.f));
    for (int i = 0; i < FxCount; ++i)
      DrawStomp(g, i);
    DrawMeter(g, false);
    DrawMeter(g, true);
  }

  void DrawStomp(IGraphics& g, int i)
  {
    static const char* kFixed[FxCount] = {"PITCH", "COMP", nullptr, nullptr, "CHORUS", "DELAY", "REVERB", "TREM"};
    const char* name = kFixed[i];
    if (i == Nam1)
      name = mNam1Label.c_str();
    else if (i == Nam2)
      name = mNam2Label.c_str();
    const IRECT r = FxRect(i);
    const bool on = mFx[(size_t)i];
    const bool live = mFxAvailable[(size_t)i];
    const bool hovered = mHoverFx == i;
    DrawInsetWell(g, r, 2.f);
    // Same language as BUILD's PRE/POST tiles: the motif lights when the pedal is
    // on, and the well stays a quiet frame. A brass selection chip plus a gold LED
    // plus gold captions turned every engaged stomp into a yellow brick.
    if (hovered && !on)
      g.FillRect(VoLumColors::SEL_BG_SOFT, r);
    g.DrawRect(VoLumColors::FRAME.WithOpacity(live ? (on ? 0.9f : (hovered ? 1.f : 0.55f)) : 0.3f), r);

    // The well carries the pedal's own motif, same as the Quiet strip and the
    // pedal cards. Eight identical footswitch rings gave the board no identity
    // beyond the caption, and the Opus D board is motif-first.
    const IRECT motif(r.L + 6.f, r.T + 5.f, r.R - 6.f, r.B - 26.f);
    // Clip: the motifs bloom and glow past their rect (a lit NAM 1 threw gold
    // nodes into the NAM 2 well), and wells here sit 4 px apart.
    g.PathClipRegion(motif);
    // Variant stays 0: the PLAY caption is the fixed "PITCH" from the spec, so a
    // motif that switched to the Octaver chevrons would contradict its own label.
    DrawEffectMotif(g, motif, kStompFocus[i], !on);
    g.PathClipRegion();
    // An effect the current rig cannot reach (NAM 2 in mono) reads as furniture,
    // not as a stomp you failed to hit: veil the motif rather than hide it.
    if (!live)
      g.FillRect(IColor(150, 9, 12, 17), motif);

    const IColor ink =
      on ? VoLumColors::CREAM : (live ? VoLumColors::CREAM_DIM : VoLumColors::CREAM_DIM.WithOpacity(0.35f));
    g.DrawText(VoLumType::Label(9.f, ink), name, IRECT(r.L, r.B - 24.f, r.R, r.B - 8.f));
  }

  void DrawMeter(IGraphics& g, bool out)
  {
    const IRECT r = MeterRect(out);
    DrawInsetWell(g, r, 1.f);
    const float level = std::clamp(out ? mInPeak * 0.85f : mInPeak, 0.f, 1.f);
    // Segmented ladder: a solid bar in a 10 px well reads as a stray rectangle.
    const int segments = 18;
    const float segH = (r.H() - 4.f) / segments;
    // Both ladders are brass. Teal on the OUT meter made "output" a lane identity,
    // which in VoLum is the SUPPORT amp's colour and nothing else; IN vs OUT is
    // already carried by the side of the board they sit on and by their captions.
    const IColor lit = out ? VoLumColors::GOLD : VoLumColors::GOLD_DIM;
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

  // The program number is part of the decision, not a consequence of it: on the Add
  // path it is a stepper you can also type into, defaulted to the first free slot,
  // and it says out loud when the number you landed on already holds a Sound. On
  // the reassign path the number is fixed - it is the row you came from.
  void DrawPickerSlotRow(IGraphics& g)
  {
    const IRECT row = PickerSlotRowRect();
    if (!mSlotEditable)
    {
      g.DrawText(
        VoLumType::Label(10.f, VoLumColors::CREAM_DIM), ("Assign to program " + TwoDigits(mEditSlot)).c_str(), row);
      return;
    }
    g.DrawText(
      VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Near), "PROGRAM", IRECT(row.L, row.T, row.L + 60.f, row.B));
    const IRECT value = PickerSlotValueRect();
    g.FillRect(VoLumColors::WELL_DARK, value);
    g.DrawRect(VoLumColors::FRAME, value);
    g.DrawText(VoLumType::Value(15.f, VoLumColors::GOLD), TwoDigits(mEditSlot).c_str(), value);
    for (int dir = -1; dir <= 1; dir += 2)
      g.DrawText(VoLumType::Label(14.f, mHoverStep == dir ? VoLumColors::GOLD : VoLumColors::GOLD_DIM),
                 dir < 0 ? "<" : ">", PickerSlotStepRect(dir));

    const IRECT note(value.R + 26.f, row.T, row.R, row.B);
    if (const volum::PlaySlot* taken = SlotHolder(mEditSlot))
      g.DrawText(VoLumType::Label(9.f, VoLumColors::AMBER, EAlign::Near),
                 ("replaces " + (taken->valid ? taken->sound.presetName : std::string("a missing Sound"))).c_str(),
                 note);
    else
      g.DrawText(VoLumType::Label(9.f, VoLumColors::CREAM_DIM, EAlign::Near), "free", note);
  }

  void DrawPicker(IGraphics& g)
  {
    const IRECT panel = PickerRect();
    g.FillRoundRect(IColor(250, 8, 11, 16), panel, 4.f);
    g.DrawRoundRect(VoLumColors::GOLD_DIM, panel, 4.f);
    g.DrawText(VoLumType::Display(24.f, VoLumColors::CREAM), mSlotEditable ? "Add Sound" : "Choose Sound",
               IRECT(panel.L, panel.T + 10.f, panel.R, panel.T + 42.f));
    DrawPickerSlotRow(g);
    DrawCrossGlyph(g, PickerCloseRect(), VoLumColors::GOLD_DIM, 1.5f);

    const IRECT list = PickerListRect();
    g.PathClipRegion(list);
    float y = list.T - mPickerScroll;
    WalkPickerRows([&](int kind, int choice, const IRECT& row) {
      if (row.B < list.T || row.T > list.B)
        return;
      if (kind != 0)
      {
        const bool factory = kind > 0;
        const bool open = PickerGroupOpen(factory);
        g.DrawText(VoLumType::Label(9.f, factory ? VoLumColors::GOLD_DIM : VoLumColors::CREAM_DIM, EAlign::Near),
                   factory ? (open ? "FACTORY" : "FACTORY  ·") : (open ? "USER" : "USER  ·"),
                   IRECT(list.L + 6.f, row.T, list.R, row.B));
        g.DrawLine(VoLumColors::FRAME.WithOpacity(0.5f), list.L + 58.f, row.MH(), list.R - 6.f, row.MH());
        return;
      }
      DrawVoLumSelection(g, row, false, mHoverChoice == choice, VoLumSelectionStyle::ListTeal, 2.f, 1.f);
      g.DrawText(VoLumType::Body(13.f, VoLumColors::CREAM, EAlign::Near), mChoices[(size_t)choice].presetName.c_str(),
                 IRECT(row.L + 10.f, row.T, row.MW(), row.B));
      g.DrawText(VoLumType::Label(9.f, VoLumColors::TEAL_DIM, EAlign::Far), mChoices[(size_t)choice].ampName.c_str(),
                 IRECT(row.MW(), row.T, row.R - 10.f, row.B));
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
  IRECT ClearRectForRow(int, const IRECT& row) const
  {
    return IRECT(row.R - 26.f, row.T + 5.f, row.R - 6.f, row.T + 23.f);
  }
  IRECT AssignRectForRow(int index) const
  {
    if (index < 0 || index >= static_cast<int>(mSlots.size()))
      return IRECT();
    const auto natural = RailRowRect(index, false);
    const bool sticky = index == ActiveRow() && natural.T < RailListRect().T;
    return AssignRectForRow(index, sticky ? RailRowRect(index, true) : natural);
  }
  IRECT AssignRectForRow(int, const IRECT& row) const
  {
    return IRECT(row.R - 48.f, row.T + 5.f, row.R - 28.f, row.T + 23.f);
  }
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
  void ClampRailScroll()
  {
    const float maxScroll = RailMaxScroll();
    mRailScroll = std::clamp(mRailScroll, 0.f, maxScroll);
    mRailScrollTarget = std::clamp(mRailScrollTarget, 0.f, maxScroll);
  }

  static int CustomArtIndex(int art)
  {
    const int n = volum::custom::kNumCustomArts;
    return ((art % n) + n) % n;
  }
  static int FactoryArtIndex(int art) { return std::clamp(art, 0, volum::kAmpCount - 1); }

  const ILayerPtr& RowArtLayer(const volum::SoundChoice& sound) const
  {
    static const ILayerPtr kNone;
    if (sound.customArt)
      return mCustomArtLayers[(size_t)CustomArtIndex(sound.art)];
    const size_t idx = (size_t)FactoryArtIndex(sound.art);
    return idx < mFactoryArtLayers.size() ? mFactoryArtLayers[idx] : kNone;
  }

  // Build every art tile the visible rail needs, once, at the thumb's logical size.
  // Must run with no clip set: StartLayer/EndLayer mutates the clip region.
  void BuildRowArtLayers(IGraphics& g)
  {
    if (mFactoryArtLayers.size() != static_cast<size_t>(volum::kAmpCount))
      mFactoryArtLayers.resize(static_cast<size_t>(volum::kAmpCount));
    const IRECT build(mRECT.L, mRECT.T, mRECT.L + kThumbSize, mRECT.T + kThumbSize);
    for (const auto& slot : mSlots)
    {
      if (!slot.valid)
        continue;
      if (slot.sound.customArt)
      {
        const int art = CustomArtIndex(slot.sound.art);
        if (!g.CheckLayer(mCustomArtLayers[(size_t)art]))
        {
          g.StartLayer(this, build);
          DrawCustomAmpArt(g, build, art, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
          mCustomArtLayers[(size_t)art] = g.EndLayer();
        }
        continue;
      }
      const int amp = FactoryArtIndex(slot.sound.art);
      if (!g.CheckLayer(mFactoryArtLayers[(size_t)amp]))
      {
        g.StartLayer(this, build);
        // The same mini fractal and the same two inks the sidebar amp list draws, so
        // a row and its sidebar entry are recognisably one amp across both modes.
        DrawSidebarMiniFractal(
          g, build, FractalCaseForAmp(amp), IColor(200, 120, 210, 220), IColor(100, 100, 180, 200));
        mFactoryArtLayers[(size_t)amp] = g.EndLayer();
      }
    }
  }

  const volum::PlaySlot* SlotHolder(int slot) const
  {
    for (const auto& candidate : mSlots)
      if (candidate.slot == slot)
        return &candidate;
    return nullptr;
  }
  IRECT RailRowRect(int index, bool sticky) const
  {
    const auto list = RailListRect();
    const float top = sticky ? list.T : list.T + index * kRowPitch - mRailScroll;
    return IRECT(list.L, top, list.R - (RailMaxScroll() > 0.5f ? 8.f : 0.f), top + kRowH);
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

  // kind: +1 factory header, -1 user header, 0 choice row.
  template <typename Fn>
  void WalkPickerRows(Fn&& fn) const
  {
    const IRECT list = PickerListRect();
    float y = list.T - mPickerScroll;
    if (HasPickerFactory())
    {
      fn(1, -1, IRECT(list.L, y, list.R, y + kPickerCapH));
      y += kPickerCapH;
      if (PickerGroupOpen(true))
      {
        for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
        {
          if (!mChoices[(size_t)i].factory)
            continue;
          fn(0, i, IRECT(list.L, y, list.R, y + kPickerRowH));
          y += kPickerRowH;
        }
      }
    }
    if (HasPickerUser())
    {
      fn(-1, -1, IRECT(list.L, y, list.R, y + kPickerCapH));
      y += kPickerCapH;
      if (PickerGroupOpen(false))
      {
        for (int i = 0; i < static_cast<int>(mChoices.size()); ++i)
        {
          if (mChoices[(size_t)i].factory)
            continue;
          fn(0, i, IRECT(list.L, y, list.R, y + kPickerRowH));
          y += kPickerRowH;
        }
      }
    }
  }

  float PickerContentHeight() const
  {
    float h = 0.f;
    if (HasPickerFactory())
    {
      h += kPickerCapH;
      if (PickerGroupOpen(true))
        for (const auto& c : mChoices)
          if (c.factory)
            h += kPickerRowH;
    }
    if (HasPickerUser())
    {
      h += kPickerCapH;
      if (PickerGroupOpen(false))
        for (const auto& c : mChoices)
          if (!c.factory)
            h += kPickerRowH;
    }
    return h;
  }
  IRECT PickerRect() const
  {
    const auto empty = EmptyRect();
    const float h = std::clamp(PickerContentHeight() + 92.f, 190.f, empty.H() - 24.f);
    const float cy = empty.MH();
    return IRECT(mRECT.MW() - 205.f, cy - h * 0.5f, mRECT.MW() + 205.f, cy + h * 0.5f);
  }
  // One band whichever path opened the panel, so the list below never shifts.
  IRECT PickerSlotRowRect() const
  {
    const auto panel = PickerRect();
    return IRECT(panel.L + 16.f, panel.T + 42.f, panel.R - 16.f, panel.T + 66.f);
  }
  IRECT PickerSlotValueRect() const
  {
    const auto row = PickerSlotRowRect();
    return IRECT(row.L + 82.f, row.T + 1.f, row.L + 124.f, row.B - 1.f);
  }
  IRECT PickerSlotStepRect(int dir) const
  {
    const auto value = PickerSlotValueRect();
    return dir < 0 ? IRECT(value.L - 24.f, value.T, value.L - 4.f, value.B)
                   : IRECT(value.R + 4.f, value.T, value.R + 24.f, value.B);
  }
  IRECT PickerListRect() const
  {
    const auto panel = PickerRect();
    return IRECT(panel.L + 14.f, panel.T + 74.f, panel.R - 20.f, panel.B - 12.f);
  }
  IRECT PickerCloseRect() const
  {
    const auto panel = PickerRect();
    return IRECT(panel.R - 30.f, panel.T + 6.f, panel.R - 6.f, panel.T + 30.f);
  }
  float PickerMaxScroll() const { return std::max(0.f, PickerContentHeight() - PickerListRect().H()); }
  volum::scroll::ScrollMetrics RailScrollMetrics() const
  {
    const auto list = RailListRect();
    return volum::scroll::ComputeScroll(list.T, list.B, list.H(), RailContentH(), mRailScroll);
  }
  volum::scroll::ScrollMetrics PickerScrollMetrics() const
  {
    const auto list = PickerListRect();
    return volum::scroll::ComputeScroll(list.T, list.B, list.H(), PickerContentHeight(), mPickerScroll);
  }
  IRECT RailTrackRect() const
  {
    const auto list = RailListRect();
    return IRECT(list.R - 4.f, list.T, list.R, list.B);
  }
  IRECT PickerTrackRect() const
  {
    const auto list = PickerListRect();
    return IRECT(list.R + 2.f, list.T, list.R + 6.f, list.B);
  }
  int PickerHeaderAt(float x, float y) const
  {
    const IRECT list = PickerListRect();
    if (!list.Contains(x, y))
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
    const IRECT list = PickerListRect();
    if (!list.Contains(x, y))
      return -1;
    int found = -1;
    WalkPickerRows([&](int kind, int choice, const IRECT& row) {
      if (found < 0 && kind == 0 && row.Contains(x, y))
        found = choice;
    });
    return found;
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
  int mHoverRow = -1, mHoverFx = -1, mHoverChoice = -1, mHoverStep = 0;
  bool mCustomArt = false, mDual = false, mSupportCustom = false, mDirty = false, mPickerOpen = false;
  bool mSlotEditable = false; // the picker was opened by Add, so it owns the number
  bool mPlusAddsHeard = false;
  std::string mNam1Label = "NAM 1";
  std::string mNam2Label = "NAM 2";
  std::array<bool, FxCount> mFx{};
  std::array<bool, FxCount> mFxAvailable{};
  float mRailScroll = 0.f, mRailScrollTarget = 0.f, mPickerScroll = 0.f, mPhase = 0.f, mInPeak = 0.f;
  int mCachedMainArt = -1, mCachedSupportArt = -1;
  bool mCachedMainCustom = false, mCachedSupportCustom = false;

  // Cached row art, keyed by art id rather than by row, so a rail of eight presets
  // on one amp costs one tile.
  std::vector<ILayerPtr> mFactoryArtLayers;
  std::array<ILayerPtr, volum::custom::kNumCustomArts> mCustomArtLayers{};
  ILayerPtr mStageMainLayer;
  ILayerPtr mStageSupportLayer;
  volum::scroll::Interaction mRailBar;
  volum::scroll::Interaction mPickerBar;
  volum::PickerGroupSession* mPickerGroups = nullptr;
  RecallCallback mRecall;
  AssignCallback mAssign;
  ClearCallback mClear;
  BypassCallback mBypass;
  EditInBuildCallback mEditInBuild;
  AddHeardCallback mAddHeard;
};
