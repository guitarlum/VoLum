#pragma once

// Settings MIDI tab: the program number 0-127 Sound assignments, laid out like a
// floor controller.
//
// Sixteen banks of eight switches; switch s on bank b sends program b * 8 + s
// (volum::footswitch, VoLumMidiFootswitchModel.h). Each switch shows its program
// number on an LED readout, the Sound's preset name, its amp and the amp's mini
// art. An empty switch is a bare stomp cap with a "+". A switch whose Sound is
// gone keeps its number and reads red, exactly as MIDI treats it: the program
// still exists, the thing it pointed at does not.
//
// Click a switch to choose its Sound (VoLumSoundPickerPanel,
// VoLumMidiSoundPicker.h). Drag a switch onto another to swap them
// (onto an empty one it moves); hovering the bank arrows or a bank pip while
// dragging pages, so a Sound can travel to any program number. The cross clears.
//
// Same store and same rows as the PLAY rail (volum::BuildPlaySlots over
// content::Registry::midiSoundMap). Mutations go out through the callbacks the
// layout wires to the plugin's assign / clear / swap methods, the ones PLAY
// uses, so there is one writer per store and this view keeps no copy.

#include "VoLumColorHelpers.h"
#include "VoLumCustomModel.h"
#include "VoLumMidiFootswitchModel.h"
#include "VoLumMidiSoundPicker.h"
#include "VoLumPlayModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class VoLumMidiFootswitchControl : public IControl
{
public:
  using AssignCallback = std::function<void(int, const volum::SoundChoice&)>;
  using ClearCallback = std::function<void(int)>;
  using SwapCallback = std::function<void(int, int)>;

  explicit VoLumMidiFootswitchControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
    mSlotAt.fill(-1);
  }

  void SetCallbacks(AssignCallback assign, ClearCallback clear)
  {
    mAssign = std::move(assign);
    mClear = std::move(clear);
  }

  void SetSwapCallback(SwapCallback swap) { mSwap = std::move(swap); }
  void SetPickerGroups(volum::PickerGroupSession* session) { mPickerGroups = session; }

  // `liveProgram` is the last recalled program number (-1 for none); the active
  // pair decides whether that switch still lights LIVE, the same test PLAY uses.
  void SetData(const std::vector<volum::FactoryPreset>& factory, const volum::content::Registry& registry,
               int liveProgram = -1, const std::string& activeAmpId = {}, const std::string& activePresetId = {})
  {
    mSlots = volum::BuildPlaySlots(factory, registry);
    mChoices = volum::BuildSoundChoices(factory, registry);
    mSlotAt.fill(-1);
    std::vector<int> assigned;
    assigned.reserve(mSlots.size());
    for (int i = 0; i < static_cast<int>(mSlots.size()); ++i)
    {
      const int program = mSlots[static_cast<size_t>(i)].slot;
      if (volum::footswitch::IsProgram(program))
      {
        mSlotAt[static_cast<size_t>(program)] = i;
        assigned.push_back(program);
      }
    }
    mOccupancy = volum::footswitch::BankOccupancy(assigned);
    mLiveProgram = liveProgram;
    mActiveAmpId = activeAmpId;
    mActivePresetId = activePresetId;
    // Out of sight the view follows the live program, so Settings always opens
    // on the bank that is playing. On screen it stays where the player put it.
    if (IsHidden())
      mBank = volum::footswitch::OpeningBank(mLiveProgram, mOccupancy);
    // A switch can empty under the pointer (cleared in PLAY, edited by another
    // instance), so an in-flight drag and an open picker are re-validated.
    if (mScreen != kScreenBoard && mChoices.empty())
      mScreen = kScreenBoard;
    if (mDragging && SlotAt(mPressProgram) == nullptr)
      CancelPress();
    SetDirty(false);
  }

  int Bank() const { return mBank; }
  bool OnBoard() const { return mScreen == kScreenBoard; }

  // PageUp / PageDown, from the plugin key router. A dragged Sound travels with the
  // page, as when the pointer hovers an arrow; the picker screen ignores them.
  bool PageBankKey(int vk)
  {
    const int step = volum::footswitch::PageKeyBankStep(vk);
    if (step == 0 || mScreen != kScreenBoard)
      return false;
    SetBank(volum::footswitch::StepBank(mBank, step));
    if (mDragging)
      UpdateDragTarget(mDragX, mDragY);
    return true;
  }

  // Settings closes from the gear, the panel cross and Escape; a picker left
  // open there would otherwise still be up the next time the page opens.
  void ResetToBoard()
  {
    if (GetAnimationFunction())
      OnEndAnimation();
    mEmptyFlash = 0.f;
    mScreen = kScreenBoard;
    mEditProgram = -1;
    CancelPress();
    mHover = {};
    Picker().Reset();
    SetDirty(false);
  }

  // Escape backs out of a drag or the picker before it is allowed to close Settings.
  bool ConsumeEscape()
  {
    if (mScreen == kScreenBoard && !mDragging)
      return false;
    ResetToBoard();
    return true;
  }

  // Tab hide is not overlay close, but leaving the tab with the picker armed
  // would let the next Escape be eaten by a screen that is no longer visible.
  void Hide(bool hide) override
  {
    if (hide && (mScreen != kScreenBoard || mDragging))
      ResetToBoard();
    if (!hide && IsHidden())
      mBank = volum::footswitch::OpeningBank(mLiveProgram, mOccupancy);
    IControl::Hide(hide);
  }

  void Draw(IGraphics& g) override
  {
    if (mScreen == kScreenPicker)
    {
      DrawPicker(g);
      return;
    }
    const auto layout = BoardLayout();
    BuildArtLayers(g, layout);
    DrawHeader(g, layout);
    for (int i = 0; i < volum::footswitch::kSwitchesPerBank; ++i)
      DrawSwitch(g, layout, i);
    DrawDragGhost(g);
    DrawDropBadge(g, layout);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (mScreen == kScreenPicker)
    {
      OnPickerDown(x, y);
      return;
    }
    const auto hit = HitAt(x, y);
    switch (hit.kind)
    {
      case volum::footswitch::HitKind::Prev: SetBank(volum::footswitch::StepBank(mBank, -1)); return;
      case volum::footswitch::HitKind::Next: SetBank(volum::footswitch::StepBank(mBank, 1)); return;
      case volum::footswitch::HitKind::Pip: SetBank(hit.index); return;
      case volum::footswitch::HitKind::Tile:
      case volum::footswitch::HitKind::Clear:
        mPressProgram = volum::footswitch::ProgramFor(mBank, hit.index);
        mPressClear = hit.kind == volum::footswitch::HitKind::Clear;
        mPressX = x;
        mPressY = y;
        SetDirty(false);
        return;
      default: return;
    }
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mScreen == kScreenBoard && volum::footswitch::DoubleClickPages(HitAt(x, y).kind))
      OnMouseDown(x, y, mod);
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override
  {
    if (mScreen == kScreenPicker)
    {
      if (Picker().OnMouseDrag(y))
        SetDirty(false);
      return;
    }
    if (mScreen != kScreenBoard || mPressProgram < 0 || mPressClear || SlotAt(mPressProgram) == nullptr)
      return;
    if (!mDragging && (std::abs(x - mPressX) > 6.f || std::abs(y - mPressY) > 6.f))
      mDragging = true;
    if (!mDragging)
      return;
    mDragX = x;
    mDragY = y;
    UpdateDragTarget(x, y);
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod&) override
  {
    Picker().OnMouseUp();
    if (mScreen != kScreenBoard || mPressProgram < 0)
      return;
    const int from = mPressProgram;
    const bool pressClear = mPressClear;
    const bool wasDrag = mDragging;
    CancelPress();

    if (wasDrag)
    {
      const auto hit = HitAt(x, y);
      if (hit.kind != volum::footswitch::HitKind::Tile && hit.kind != volum::footswitch::HitKind::Clear)
        return;
      const int to = volum::footswitch::ProgramFor(mBank, hit.index);
      const auto action = volum::footswitch::DecideDrop(from, to, SlotAt(from) != nullptr, SlotAt(to) != nullptr);
      if (action != volum::footswitch::DropAction::None && mSwap)
        mSwap(from, to);
      return;
    }

    // A click lands only where it started, so sliding off a switch is a cancel.
    const auto hit = HitAt(x, y);
    const bool onClear = hit.kind == volum::footswitch::HitKind::Clear;
    if ((hit.kind != volum::footswitch::HitKind::Tile && !onClear)
        || volum::footswitch::ProgramFor(mBank, hit.index) != from || onClear != pressClear)
      return;
    if (onClear)
    {
      if (mClear && SlotAt(from) != nullptr)
        mClear(from);
      return;
    }
    if (mChoices.empty())
      FlashEmptyHint();
    else
      OpenPicker(from);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    if (mScreen == kScreenPicker)
    {
      if (Picker().OnMouseOver(x, y))
        SetDirty(false);
      return;
    }
    const auto hit = HitAt(x, y);
    if (hit.kind == mHover.kind && hit.index == mHover.index)
      return;
    mHover = hit;
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHover = {};
    Picker().OnMouseOut();
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (mScreen == kScreenPicker)
    {
      Picker().OnMouseWheel(x, y, d);
      SetDirty(false);
      return;
    }
    const int step = volum::footswitch::WheelBankStep(mWheelAccum, d);
    if (step != 0)
      SetBank(volum::footswitch::StepBank(mBank, step));
  }

private:
  static constexpr int kScreenBoard = 0;
  static constexpr int kScreenPicker = 1;

  static IRECT ToRect(const volum::footswitch::Box& b) { return IRECT(b.L, b.T, b.R, b.B); }

  volum::footswitch::Layout BoardLayout() const
  {
    return volum::footswitch::LayoutFootswitch(mRECT.L, mRECT.T, mRECT.R, mRECT.B);
  }

  const volum::PlaySlot* SlotAt(int program) const
  {
    if (!volum::footswitch::IsProgram(program))
      return nullptr;
    const int index = mSlotAt[static_cast<size_t>(program)];
    return index < 0 ? nullptr : &mSlots[static_cast<size_t>(index)];
  }

  std::array<bool, volum::footswitch::kSwitchesPerBank> OccupiedOnBank() const
  {
    std::array<bool, volum::footswitch::kSwitchesPerBank> out{};
    for (int i = 0; i < volum::footswitch::kSwitchesPerBank; ++i)
      out[static_cast<size_t>(i)] = SlotAt(volum::footswitch::ProgramFor(mBank, i)) != nullptr;
    return out;
  }

  volum::footswitch::Hit HitAt(float x, float y) const
  {
    return volum::footswitch::HitTest(BoardLayout(), x, y, OccupiedOnBank());
  }

  bool IsLive(const volum::PlaySlot& slot) const
  {
    return volum::IsLastRecalledSlot(slot, mLiveProgram, mActiveAmpId, mActivePresetId);
  }

  void SetBank(int bank)
  {
    const int next = volum::footswitch::ClampBank(bank);
    if (next == mBank)
      return;
    mBank = next;
    mHover = {};
    SetDirty(false);
  }

  void CancelPress()
  {
    mPressProgram = -1;
    mPressClear = false;
    mDragging = false;
    mDropProgram = -1;
    mDragPageKind = volum::footswitch::HitKind::None;
  }

  // Hovering an arrow pages once per entry, so holding still on it does not race
  // through all sixteen banks; a pip jumps straight to its bank.
  void UpdateDragTarget(float x, float y)
  {
    const auto hit = HitAt(x, y);
    mDropProgram = -1;
    if (hit.kind == volum::footswitch::HitKind::Prev || hit.kind == volum::footswitch::HitKind::Next)
    {
      if (mDragPageKind != hit.kind)
        SetBank(volum::footswitch::StepBank(mBank, hit.kind == volum::footswitch::HitKind::Next ? 1 : -1));
      mDragPageKind = hit.kind;
      return;
    }
    mDragPageKind = volum::footswitch::HitKind::None;
    if (hit.kind == volum::footswitch::HitKind::Pip)
      SetBank(hit.index);
    else if (hit.kind == volum::footswitch::HitKind::Tile || hit.kind == volum::footswitch::HitKind::Clear)
      mDropProgram = volum::footswitch::ProgramFor(mBank, hit.index);
  }

  // The answer to "why did nothing happen" is already in the header, so the click
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

  static std::string ProgramLabel(int program)
  {
    if (!volum::footswitch::IsProgram(program))
      return "---";
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%03d", program);
    return buf;
  }

  static std::string BankLabel(int bank)
  {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d", bank + 1);
    return buf;
  }

  // ---- Board --------------------------------------------------------------

  static void DrawLedDigits(IGraphics& g, const IRECT& r, const std::string& digits, const IColor& ink, float size,
                            bool lit);

  void DrawArrow(IGraphics& g, const IRECT& r, bool next, bool enabled, bool hot);

  void DrawHeader(IGraphics& g, const volum::footswitch::Layout& layout);

  static IRECT ThumbRect(const IRECT& tile);

  static void DrawStompCap(IGraphics& g, const IRECT& thumb, bool hot);

  static void FillTileFace(IGraphics& g, const IRECT& tile, const IColor& top, const IColor& bot);

  void DrawSwitch(IGraphics& g, const volum::footswitch::Layout& layout, int index);

  void DrawDropBadge(IGraphics& g, const volum::footswitch::Layout& layout);

  void DrawDragGhost(IGraphics& g);

  // ---- Art ----------------------------------------------------------------

  static int CustomArtIndex(int art)
  {
    const int n = volum::custom::kNumCustomArts;
    return ((art % n) + n) % n;
  }
  static int FactoryArtIndex(int art) { return std::clamp(art, 0, volum::kAmpCount - 1); }

  const ILayerPtr& ArtLayer(const volum::SoundChoice& sound) const;

  void BuildArtLayers(IGraphics& g, const volum::footswitch::Layout& layout);

  // ---- Picker: which Sound a switch plays ---------------------------------

  VoLumSoundPickerPanel& Picker()
  {
    mPicker.Bind(mRECT, &mChoices, mPickerGroups);
    return mPicker;
  }

  void OpenPicker(int program)
  {
    if (!volum::footswitch::IsProgram(program) || mChoices.empty())
      return;
    mEditProgram = program;
    mScreen = kScreenPicker;
    Picker().Open();
    SetDirty(false);
  }

  void OnPickerDown(float x, float y)
  {
    const int choice = Picker().OnMouseDown(x, y);
    if (choice == VoLumSoundPickerPanel::kClose)
      mScreen = kScreenBoard;
    else if (choice >= 0)
    {
      if (mAssign)
        mAssign(mEditProgram, mChoices[static_cast<size_t>(choice)]);
      mScreen = kScreenBoard;
    }
    SetDirty(false);
  }

  void DrawPicker(IGraphics& g);

  static constexpr float kTileRadius = 7.f;

  std::vector<volum::PlaySlot> mSlots;
  std::vector<volum::SoundChoice> mChoices;
  std::array<int, volum::kMidiSoundSlotCount> mSlotAt{};
  std::array<int, volum::footswitch::kBankCount> mOccupancy{};
  int mLiveProgram = -1;
  std::string mActiveAmpId;
  std::string mActivePresetId;
  int mBank = 0;
  float mWheelAccum = 0.f;
  int mScreen = kScreenBoard;
  int mEditProgram = -1;
  volum::footswitch::Hit mHover;
  volum::footswitch::HitKind mDragPageKind = volum::footswitch::HitKind::None;
  int mPressProgram = -1;
  bool mPressClear = false;
  bool mDragging = false;
  int mDropProgram = -1;
  float mPressX = 0.f;
  float mPressY = 0.f;
  float mDragX = 0.f;
  float mDragY = 0.f;
  float mEmptyFlash = 0.f;
  VoLumSoundPickerPanel mPicker;
  volum::PickerGroupSession* mPickerGroups = nullptr;
  std::vector<ILayerPtr> mFactoryArt;
  std::array<ILayerPtr, volum::custom::kNumCustomArts> mCustomArt{};
  AssignCallback mAssign;
  ClearCallback mClear;
  SwapCallback mSwap;
};

#include "VoLumMidiFootswitchDraw.h"
