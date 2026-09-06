#pragma once

// VoLum settings overlay chrome controls.
//
// - VoLumSettingsBackdropControl: full-window dim that frames the panel.
// - VoLumSettingsGroupFrameControl: thin rounded frame around a group of
//   related settings rows.
// - VoLumSettingsFooterSepControl / VoLumSettingsVertRuleControl: hairlines.
// - VoLumSettingsCloseControl: the X button at the top-right of the panel.
// - VoLumUpdateNoticeControl / VoLumSettingsCheckboxControl: update row bits.
// - VoLumSettingsShortcutInfoControl: the keyboard cheat-sheet columns.
//
// Extracted from VoLumCoreControls.h on the 1.0 hygiene split. The two-tab
// chrome (tab strip, MIDI channel row, content-library slot) lives next door in
// VoLumSettingsTabs.h.

#include "VoLumColorHelpers.h"
#include "VoLumPackLayout.h"

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

/** Right-aligned gold "update available" pill. Draws nothing when no update is
 * pending, so container Hide propagation cannot resurrect a stale notice. */
class VoLumUpdateNoticeControl : public IControl
{
public:
  VoLumUpdateNoticeControl(const IRECT& bounds, std::function<void()> onClick)
  : IControl(bounds)
  , mOnClick(std::move(onClick))
  {
  }

  void SetUpdate(bool available, const std::string& version, const std::string& notes = {})
  {
    mAvailable = available;
    mNotes = notes;
    mLabel = version.empty() ? "Update available  ·  What's new" : "Update available: " + version + "  ·  What's new";
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    if (!mAvailable)
      return;
    const IRECT pill = PillRect(g);
    g.FillRoundRect(VoLumColors::GOLD.WithOpacity(mMouseIsOver ? 0.34f : 0.24f), pill, pill.H() * 0.5f);
    g.DrawRoundRect(VoLumColors::GOLD, pill, pill.H() * 0.5f);
    g.DrawText(IText(10.f, mMouseIsOver ? VoLumColors::TEXT_BRIGHT : VoLumColors::GOLD, "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               mLabel.c_str(), pill);
    if (!mNotes.empty())
    {
      const IRECT notes(mRECT.L, pill.B + 4.f, mRECT.R, mRECT.B);
      g.PathClipRegion(notes);
      g.DrawText(IText(10.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Top), mNotes.c_str(), notes);
      g.PathClipRegion();
    }
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    if (mAvailable && mOnClick)
      mOnClick();
  }
  void OnMouseOver(float, float, const IMouseMod&) override
  {
    mMouseIsOver = true;
    SetDirty(false);
  }
  void OnMouseOut() override
  {
    mMouseIsOver = false;
    SetDirty(false);
  }
  bool IsHit(float x, float y) const override { return mAvailable && IControl::IsHit(x, y); }

private:
  IRECT PillRect(IGraphics& g) const
  {
    IRECT measured;
    g.MeasureText(IText(10.f, "Josefin-Bold"), mLabel.c_str(), measured);
    const float w = std::min(mRECT.W(), measured.W() + 22.f);
    const float h = 16.f;
    return IRECT(mRECT.L, mRECT.T, mRECT.L + w, mRECT.T + h);
  }

  bool mAvailable = false;
  std::string mLabel = "Update available";
  std::string mNotes;
  std::function<void()> mOnClick;
};

/** Labelled checkbox for the settings footer (visible on/off state, no param). */
class VoLumSettingsCheckboxControl : public IControl
{
public:
  using Callback = std::function<void(bool)>;

  VoLumSettingsCheckboxControl(const IRECT& bounds, const char* label, Callback callback)
  : IControl(bounds)
  , mLabel(label ? label : "")
  , mCallback(std::move(callback))
  {
  }

  void SetChecked(bool checked)
  {
    mChecked = checked;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT box = BoxRect();
    g.FillRoundRect(mChecked ? VoLumColors::GOLD.WithOpacity(0.28f) : IColor(255, 9, 9, 14), box, 2.f);
    g.DrawRoundRect(mMouseIsOver ? VoLumColors::GOLD : VoLumColors::FRAME, box, 2.f);
    if (mChecked)
    {
      g.DrawLine(VoLumColors::GOLD, box.L + 3.f, box.MH(), box.MW() - 1.f, box.B - 3.5f, nullptr, 1.6f);
      g.DrawLine(VoLumColors::GOLD, box.MW() - 1.f, box.B - 3.5f, box.R - 2.5f, box.T + 3.f, nullptr, 1.6f);
    }
    g.DrawText(IText(11.f, mMouseIsOver ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED, "Josefin-Sans",
                     EAlign::Near, EVAlign::Middle),
               mLabel.c_str(), IRECT(box.R + 7.f, mRECT.T, mRECT.R, mRECT.B));
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    mChecked = !mChecked;
    if (mCallback)
      mCallback(mChecked);
    SetDirty(false);
  }
  void OnMouseOver(float, float, const IMouseMod&) override
  {
    mMouseIsOver = true;
    SetDirty(false);
  }
  void OnMouseOut() override
  {
    mMouseIsOver = false;
    SetDirty(false);
  }

private:
  IRECT BoxRect() const
  {
    const float s = 13.f;
    return IRECT(mRECT.L, mRECT.MH() - s * 0.5f, mRECT.L + s, mRECT.MH() + s * 0.5f);
  }

  std::string mLabel;
  bool mChecked = false;
  Callback mCallback;
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

/** Content-library row: the Export Pack / Import Pack pair on the SYSTEM tab.
 *
 * One control for both buttons so the Settings page grows by a single named child
 * and one pair of callbacks. The Pack chrome itself is a separate modal
 * (VoLumPackOverlayControl); this row only opens it.
 *
 * No caption of its own: the SYSTEM card that hosts this control already caps it
 * with "Back up your library". */
class VoLumSettingsPackRowControl : public IControl
{
public:
  VoLumSettingsPackRowControl(const IRECT& bounds, std::function<void()> onExport, std::function<void()> onImport)
  : IControl(bounds)
  , mOnExport(std::move(onExport))
  , mOnImport(std::move(onImport))
  {
    mIgnoreMouse = false;
  }

  void Draw(IGraphics& g) override
  {
    _DrawBtn(g, _ExportRect(), "Export Pack...", mHover == 1);
    _DrawBtn(g, _ImportRect(), "Import Pack...", mHover == 2);

    const IText help(11.f, VoLumColors::TEXT_DIM.WithOpacity(0.75f), "Josefin-Sans", EAlign::Near, EVAlign::Top);
    const float helpT = mRECT.T + volum::packui::kPackRowBtnH + volum::packui::kPackRowBtnToHelp;
    g.DrawText(help, "Move your custom amps, presets, IRs and pedals to",
               IRECT(mRECT.L, helpT, mRECT.R, helpT + volum::packui::kPackRowHelpLineH));
    g.DrawText(help, "another computer, or share part of your library.",
               IRECT(mRECT.L, helpT + volum::packui::kPackRowHelpLineGap, mRECT.R,
                     helpT + volum::packui::kPackRowHelpLineGap + volum::packui::kPackRowHelpLineH));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (_ExportRect().Contains(x, y) && mOnExport)
      mOnExport();
    else if (_ImportRect().Contains(x, y) && mOnImport)
      mOnImport();
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int was = mHover;
    mHover = _ExportRect().Contains(x, y) ? 1 : (_ImportRect().Contains(x, y) ? 2 : 0);
    if (was != mHover)
      SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHover = 0;
    SetDirty(false);
  }

private:
  // The two buttons share the card's width so they read as one pair, with the
  // help lines underneath. The card body is ~64 px tall, so the row cannot also
  // hold a caption; the card's own cap supplies it.
  static constexpr float kBtnGap = 10.f;
  IRECT _ExportRect() const
  {
    return IRECT(mRECT.L, mRECT.T, mRECT.L + (mRECT.W() - kBtnGap) * 0.5f, mRECT.T + volum::packui::kPackRowBtnH);
  }
  IRECT _ImportRect() const
  {
    return IRECT(mRECT.R - (mRECT.W() - kBtnGap) * 0.5f, mRECT.T, mRECT.R, mRECT.T + volum::packui::kPackRowBtnH);
  }

  static void _DrawBtn(IGraphics& g, const IRECT& r, const char* label, bool hover)
  {
    g.FillRoundRect(hover ? IColor(70, 232, 168, 92) : VoLumColors::BTN_OFF_BG, r, 3.f);
    g.DrawRoundRect(hover ? VoLumColors::AMBER : VoLumColors::FRAME, r, 3.f, nullptr, hover ? 1.3f : 1.f);
    g.DrawText(IText(11.5f, hover ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               label, r);
  }

  std::function<void()> mOnExport;
  std::function<void()> mOnImport;
  int mHover = 0;
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
    // No frame or title of its own: the Settings card that hosts this control
    // draws the panel depth and caps it with "Keyboard shortcuts".
    const IText keyText(11.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Top);
    const IText descText(10.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Top);
    const IText capText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top);

    // A band, not the full card: on the SYSTEM tab this card is ~720 px wide but
    // the three columns only need ~330 px of text, so spreading them edge to edge
    // left each key list floating alone in its third. Left-aligned so "Navigate"
    // starts under the card's caption.
    const float gap = 18.f;
    const IRECT body = mRECT.GetFromLeft(std::min(mRECT.W(), 560.f));
    // Weighted, not equal thirds: "PRE / AMP / POST" is ~95 px of the ~142 px
    // Navigate needs, and at an equal third it overflowed the divider and
    // collided with the Edit column's keys. Edit and Tools have slack to spare.
    const float colBand = body.W() - 2.f * gap;
    const float navW = colBand * 0.47f;
    const float editW = colBand * 0.27f;
    const IRECT navCol(body.L, body.T, body.L + navW, body.B);
    const IRECT editCol(navCol.R + gap, body.T, navCol.R + gap + editW, body.B);
    const IRECT toolCol(editCol.R + gap, body.T, body.R, body.B);

    g.DrawLine(
      VoLumColors::FRAME.WithOpacity(0.55f), navCol.R + gap * 0.5f, body.T + 1.f, navCol.R + gap * 0.5f, body.B - 1.f);
    g.DrawLine(VoLumColors::FRAME.WithOpacity(0.55f), editCol.R + gap * 0.5f, body.T + 1.f, editCol.R + gap * 0.5f,
               body.B - 1.f);

    const float rowH = 12.f;
    auto drawPair = [&](const IRECT& col, int row, float keyW, const char* key, const char* desc) {
      const float y = col.T + 14.f + row * rowH;
      g.DrawText(keyText, key, IRECT(col.L, y, col.L + keyW, y + rowH));
      const IRECT descR(col.L + keyW + 5.f, y, col.R, y + rowH);
      g.PathClipRegion(descR);
      g.DrawText(descText, desc, descR);
      g.PathClipRegion();
    };

    // One key-column width per section (widest key in that column) so every
    // description in a column starts at the same x. Previously each row used its
    // own keyW, so Navigate/Edit descriptions stair-stepped while Tools (all 16)
    // happened to line up.
    const float navKeyW = 42.f; // "Arrows"
    const float editKeyW = 48.f; // "Ctrl+S"
    const float toolKeyW = 16.f; // single letters

    g.DrawText(capText, "Navigate", IRECT(navCol.L, navCol.T, navCol.R, navCol.T + 12.f));
    drawPair(navCol, 0, navKeyW, "1/2/3", "PRE / AMP / POST");
    drawPair(navCol, 1, navKeyW, "Tab", "target focus");
    drawPair(navCol, 2, navKeyW, "Arrows", "amp / channel");
    // PLAY has one shortcut and it is the whole point of the mode, so it gets its
    // own line rather than a parenthesis on the BUILD arrows row. Spelled out, not
    // as arrow glyphs: Josefin has no U+2191/2193, so those drew an empty key.
    drawPair(navCol, 3, navKeyW, "Up/Dn", "Sound in PLAY");
    drawPair(navCol, 4, navKeyW, "1-8", "stomps in PLAY");

    g.DrawText(capText, "Edit", IRECT(editCol.L, editCol.T, editCol.R, editCol.T + 12.f));
    drawPair(editCol, 0, editKeyW, "Enter", "edit");
#ifdef APP_API
    drawPair(editCol, 1, editKeyW, "Space", "toggle");
#else
    drawPair(editCol, 1, editKeyW, "B", "toggle");
#endif
    drawPair(editCol, 2, editKeyW, "S", "cab");
    drawPair(editCol, 3, editKeyW, "Ctrl+S", "save Sound");
    drawPair(editCol, 4, editKeyW, "Esc", "close");

    g.DrawText(capText, "Tools", IRECT(toolCol.L, toolCol.T, toolCol.R, toolCol.T + 12.f));
    drawPair(toolCol, 0, toolKeyW, "T", "tuner");
    drawPair(toolCol, 1, toolKeyW, "M", "metronome");
    drawPair(toolCol, 2, toolKeyW, "H", "settings");
  }
};
