#pragma once

// VoLum settings overlay chrome controls.
//
// - VoLumSettingsBackdropControl: full-window dim that frames the panel.
// - VoLumSettingsGroupFrameControl: thin rounded frame around a group of
//   related settings rows.
// - VoLumSettingsFooterSepControl / VoLumSettingsVertRuleControl: hairlines.
// - VoLumSettingsCloseControl: the X button at the top-right of the panel.
//
// Extracted from VoLumCoreControls.h on the 1.0 hygiene split.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

/** Full-window dim + explicit panel rect (must match layout math in NAMSettingsPageControl). */
class VoLumSettingsBackdropControl : public IControl
{
public:
  VoLumSettingsBackdropControl(const IRECT& fullBounds, const IRECT& panelRect)
  : IControl(fullBounds)
  , mPanel(panelRect)
  {
    // Must receive hits: if ignored, dim/panel â€œemptyâ€ pixels fall through to main UI and can steal
    // mouse up/down when the cursor moves quickly (settings appears to close at random).
    mIgnoreMouse = false;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    // Consume clicks on overlay shell (dim + filler); interactive children sit above in z-order.
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(185, 8, 10, 14), mRECT);
    const IRECT& p = mPanel;
    // Slightly lifted panel so it reads clearly over the dim layer
    DrawPanelDepth(g, p);
    g.DrawRect(VoLumColors::FRAME, p);
    g.DrawRect(IColor(90, 200, 180, 100), p.GetPadded(2.f));
    const float cs = 18.f;
    const float m = 8.f;
    DrawCornerAccent(g, p.L + m, p.T + m, cs, false, false);
    DrawCornerAccent(g, p.R - m, p.T + m, cs, true, false);
    DrawCornerAccent(g, p.L + m, p.B - m, cs, false, true);
    DrawCornerAccent(g, p.R - m, p.B - m, cs, true, true);
  }

private:
  IRECT mPanel;
};

/** Subtle frame behind grouped settings controls (ignores mouse so widgets on top still hit-test). */
class VoLumSettingsGroupFrameControl : public IControl
{
public:
  explicit VoLumSettingsGroupFrameControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    // Group card: lifted panel depth + gold border + inner hairline.
    DrawPanelDepth(g, mRECT);
    g.DrawRect(IColor(89, 200, 162, 78), mRECT);
    g.DrawRect(IColor(31, 200, 162, 78), mRECT.GetPadded(3.f));
  }
};

struct VoLumMidiSettingsRow
{
  int slot = -1;
  std::string ampName;
  std::string presetName;
  bool valid = false;
};

struct VoLumMidiSoundChoice
{
  std::string ampId;
  std::string ampName;
  std::string presetId;
  std::string presetName;
};

/** Compact MIDI channel + interim Sound assignment list for the Settings card. */
class VoLumMidiSettingsControl : public IControl
{
public:
  using ChannelCallback = std::function<void(int)>;
  using AssignCallback = std::function<void(int, const std::string&, const std::string&)>;
  using ClearCallback = std::function<void(int)>;

  explicit VoLumMidiSettingsControl(const IRECT& bounds)
  : IControl(bounds)
  , mMenu("Assign MIDI Sound")
  {
    mIgnoreMouse = false;
  }

  void SetCallbacks(ChannelCallback channel, AssignCallback assign, ClearCallback clear)
  {
    mChannelCallback = std::move(channel);
    mAssignCallback = std::move(assign);
    mClearCallback = std::move(clear);
  }

  void SetData(int channel, std::vector<VoLumMidiSettingsRow> rows, std::vector<VoLumMidiSoundChoice> choices)
  {
    mChannel = std::clamp(channel, 0, 16);
    mRows = std::move(rows);
    mChoices = std::move(choices);
    ClampScroll();
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT channel = ChannelRect();
    g.FillRoundRect(VoLumColors::BTN_OFF_BG, channel, 3.f);
    g.DrawRoundRect(VoLumColors::GOLD_DIM, channel, 3.f);
    const std::string channelText = mChannel == 0 ? "‹  Omni  ›" : "‹  Ch " + std::to_string(mChannel) + "  ›";
    g.DrawText(IText(11.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               channelText.c_str(), channel);

    const IRECT list = ListRect();
    g.PathClipRegion(list);
    float y = list.T - mScroll;
    for (const auto& row : mRows)
    {
      const IRECT rr(list.L, y, list.R, y + kRowH);
      y += kRowH;
      if (rr.B < list.T || rr.T > list.B)
        continue;
      const IColor color = row.valid ? VoLumColors::TEXT_MED : IColor(255, 224, 88, 88);
      const std::string label =
        std::to_string(row.slot) + "  " + row.ampName + " / " + row.presetName;
      g.DrawText(IText(9.f, color, "Josefin-Sans", EAlign::Near, EVAlign::Middle), label.c_str(),
                 rr.GetReducedFromRight(16.f));
      g.DrawText(IText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "×",
                 rr.GetFromRight(14.f));
    }
    g.PathClipRegion();

    const float contentH = static_cast<float>(mRows.size()) * kRowH;
    if (contentH > list.H() + 0.5f)
    {
      const IRECT track(list.R - 4.f, list.T, list.R, list.B);
      const float thumbH = std::max(12.f, track.H() * list.H() / contentH);
      const float maxScroll = contentH - list.H();
      const float t = maxScroll > 0.f ? mScroll / maxScroll : 0.f;
      const IRECT thumb(track.L, track.T + (track.H() - thumbH) * t, track.R,
                        track.T + (track.H() - thumbH) * t + thumbH);
      DrawVoLumScrollbar(g, track, thumb);
    }

    const IRECT add = AddRect();
    g.FillRoundRect(VoLumColors::BTN_OFF_BG, add, 3.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, add, 3.f);
    g.DrawText(IText(10.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "+ Add Sound", add);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const IRECT channel = ChannelRect();
    if (channel.Contains(x, y))
    {
      const int delta = x < channel.MW() ? -1 : 1;
      mChannel = (mChannel + delta + 17) % 17;
      if (mChannelCallback)
        mChannelCallback(mChannel);
      SetDirty(false);
      return;
    }

    if (AddRect().Contains(x, y))
    {
      bool used[128]{};
      for (const auto& row : mRows)
        if (row.slot >= 0 && row.slot < 128)
          used[row.slot] = true;
      mTargetSlot = 0;
      while (mTargetSlot < 128 && used[mTargetSlot])
        ++mTargetSlot;
      if (mTargetSlot < 128)
        OpenChoiceMenu(AddRect());
      return;
    }

    const IRECT list = ListRect();
    if (!list.Contains(x, y))
      return;
    const int rowIdx = static_cast<int>((y - list.T + mScroll) / kRowH);
    if (rowIdx < 0 || rowIdx >= static_cast<int>(mRows.size()))
      return;
    const int slot = mRows[static_cast<size_t>(rowIdx)].slot;
    if (x >= list.R - 18.f)
    {
      if (mClearCallback)
        mClearCallback(slot);
    }
    else
    {
      mTargetSlot = slot;
      OpenChoiceMenu(IRECT(x, y, x + 1.f, y + 1.f));
    }
  }

  void OnMouseWheel(float, float, const IMouseMod&, float d) override
  {
    mScroll -= d * kRowH * 1.5f;
    ClampScroll();
    SetDirty(false);
  }

  void OnPopupMenuSelection(IPopupMenu* selected, int) override
  {
    if (!selected || !selected->GetChosenItem())
      return;
    const int choiceIdx = selected->GetChosenItem()->GetTag();
    if (choiceIdx < 0 || choiceIdx >= static_cast<int>(mMenuChoices.size()) || mTargetSlot < 0)
      return;
    const auto& choice = mMenuChoices[static_cast<size_t>(choiceIdx)];
    if (mAssignCallback)
      mAssignCallback(mTargetSlot, choice.ampId, choice.presetId);
  }

private:
  static constexpr float kRowH = 15.f;

  IRECT ChannelRect() const { return mRECT.GetFromTop(20.f); }
  IRECT AddRect() const { return mRECT.GetFromBottom(18.f); }
  IRECT ListRect() const { return IRECT(mRECT.L, ChannelRect().B + 2.f, mRECT.R, AddRect().T - 2.f); }

  void ClampScroll()
  {
    const float maxScroll = std::max(0.f, static_cast<float>(mRows.size()) * kRowH - ListRect().H());
    mScroll = std::clamp(mScroll, 0.f, maxScroll);
  }

  void OpenChoiceMenu(const IRECT& anchor)
  {
    mMenu.Clear();
    mMenuChoices = mChoices;
    std::string currentAmp;
    IPopupMenu* submenu = nullptr;
    for (int i = 0; i < static_cast<int>(mMenuChoices.size()); ++i)
    {
      const auto& choice = mMenuChoices[static_cast<size_t>(i)];
      if (!submenu || choice.ampId != currentAmp)
      {
        currentAmp = choice.ampId;
        submenu = new IPopupMenu(choice.ampName.c_str());
        mMenu.AddItem(choice.ampName.c_str(), submenu);
      }
      submenu->AddItem(new IPopupMenu::Item(choice.presetName.c_str(), IPopupMenu::Item::kNoFlags, i));
    }
    if (mMenuChoices.empty())
      mMenu.AddItem("No named presets", -1, IPopupMenu::Item::kDisabled);
    GetUI()->CreatePopupMenu(*this, mMenu, anchor);
  }

  int mChannel = 0;
  int mTargetSlot = -1;
  float mScroll = 0.f;
  std::vector<VoLumMidiSettingsRow> mRows;
  std::vector<VoLumMidiSoundChoice> mChoices;
  std::vector<VoLumMidiSoundChoice> mMenuChoices;
  IPopupMenu mMenu;
  ChannelCallback mChannelCallback;
  AssignCallback mAssignCallback;
  ClearCallback mClearCallback;
};

/** Thin horizontal rule above settings footer (mouse passes through). */
class VoLumSettingsFooterSepControl : public IControl
{
public:
  explicit VoLumSettingsFooterSepControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const IColor c(31, 200, 162, 78);
    g.FillRect(c, mRECT);
  }
};

/** Thin vertical rule between settings columns (mouse passes through). */
class VoLumSettingsVertRuleControl : public IControl
{
public:
  explicit VoLumSettingsVertRuleControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float t = mRECT.H() * 0.12f;
    const IColor mid(90, 200, 162, 78);
    const IColor end(35, 200, 162, 78);
    g.DrawLine(end, cx, mRECT.T, cx, mRECT.T + t, nullptr, 1.f);
    g.DrawLine(mid, cx, mRECT.T + t, cx, mRECT.B - t, nullptr, 1.f);
    g.DrawLine(end, cx, mRECT.B - t, cx, mRECT.B, nullptr, 1.f);
  }
};

/** Gold â€œÃ—â€ close control (no grey SVG) for the settings overlay. */
class VoLumSettingsCloseControl : public IControl
{
public:
  VoLumSettingsCloseControl(const IRECT& bounds, IActionFunction actionFunc)
  : IControl(bounds, kNoParameter, nullptr)
  , mCloseAction(std::move(actionFunc))
  {
  }

  void Draw(IGraphics& g) override
  {
    const IColor c = mMouseIsOver ? VoLumColors::GOLD : VoLumColors::GOLD_DIM;
    const float inset = 8.f;
    const float t = mMouseIsOver ? 2.25f : 1.75f;
    const float x0 = mRECT.L + inset;
    const float y0 = mRECT.T + inset;
    const float x1 = mRECT.R - inset;
    const float y1 = mRECT.B - inset;
    g.DrawLine(c, x0, y0, x1, y1, nullptr, t);
    g.DrawLine(c, x1, y0, x0, y1, nullptr, t);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    if (mCloseAction)
      mCloseAction(this);
  }

private:
  IActionFunction mCloseAction;
};

class VoLumSettingsShortcutInfoControl : public IControl
{
public:
  explicit VoLumSettingsShortcutInfoControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    // Match the three settings cards: lifted panel depth + gold border + inner hairline.
    DrawPanelDepth(g, mRECT);
    g.DrawRect(IColor(89, 200, 162, 78), mRECT);
    g.DrawRect(IColor(31, 200, 162, 78), mRECT.GetPadded(3.f));

    const IRECT inner = mRECT.GetPadded(-16.f, -8.f, -16.f, -8.f);
    const IText titleText(13.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Top);
    const IText keyText(11.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Top);
    const IText descText(10.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Top);
    const IText capText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top);

    g.DrawText(titleText, "Shortcut info", inner.GetFromTop(16.f));
    const IRECT body = inner.GetReducedFromTop(20.f);
    const float gap = 18.f;
    const float colW = (body.W() - 2.f * gap) / 3.f;
    const IRECT navCol(body.L, body.T, body.L + colW, body.B);
    const IRECT editCol(navCol.R + gap, body.T, navCol.R + gap + colW, body.B);
    const IRECT toolCol(editCol.R + gap, body.T, body.R, body.B);

    g.DrawLine(
      VoLumColors::FRAME.WithOpacity(0.55f), navCol.R + gap * 0.5f, body.T + 1.f, navCol.R + gap * 0.5f, body.B - 1.f);
    g.DrawLine(VoLumColors::FRAME.WithOpacity(0.55f), editCol.R + gap * 0.5f, body.T + 1.f, editCol.R + gap * 0.5f,
               body.B - 1.f);

    const float rowH = 12.f;
    auto drawPair = [&](const IRECT& col, int row, float keyW, const char* key, const char* desc) {
      const float y = col.T + 14.f + row * rowH;
      g.DrawText(keyText, key, IRECT(col.L, y, col.L + keyW, y + rowH));
      g.DrawText(descText, desc, IRECT(col.L + keyW + 5.f, y, col.R, y + rowH));
    };

    // One key-column width per section (widest key in that column) so every
    // description in a column starts at the same x. Previously each row used its
    // own keyW, so Navigate/Edit descriptions stair-stepped while Tools (all 16)
    // happened to line up.
    const float navKeyW = 42.f; // "Arrows"
    const float editKeyW = 34.f; // "Enter" / standalone "Space"
    const float toolKeyW = 16.f; // single letters

    g.DrawText(capText, "Navigate", IRECT(navCol.L, navCol.T, navCol.R, navCol.T + 12.f));
    drawPair(navCol, 0, navKeyW, "1/2/3", "PRE / AMP / POST");
    drawPair(navCol, 1, navKeyW, "Tab", "target focus");
    drawPair(navCol, 2, navKeyW, "Arrows", "amp / channel");

    g.DrawText(capText, "Edit", IRECT(editCol.L, editCol.T, editCol.R, editCol.T + 12.f));
    drawPair(editCol, 0, editKeyW, "Enter", "edit");
#ifdef APP_API
    drawPair(editCol, 1, editKeyW, "Space", "toggle");
#else
    drawPair(editCol, 1, editKeyW, "B", "toggle");
#endif
    drawPair(editCol, 2, editKeyW, "S", "cab");
    drawPair(editCol, 3, editKeyW, "Esc", "close");

    g.DrawText(capText, "Tools", IRECT(toolCol.L, toolCol.T, toolCol.R, toolCol.T + 12.f));
    drawPair(toolCol, 0, toolKeyW, "T", "tuner");
    drawPair(toolCol, 1, toolKeyW, "M", "metronome");
    drawPair(toolCol, 2, toolKeyW, "H", "settings");
  }
};