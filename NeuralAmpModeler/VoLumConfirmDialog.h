#pragma once

// VoLumConfirmDialogControl: shared destructive-delete confirmation modal.
// Extracted from VoLumCustomUi.h for file-size hygiene.

#include "VoLumColorHelpers.h"
#include "VoLumCustomContentApi.h"
#include "VoLumFractalArt.h"
#include "VoLumIrFileGuard.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// Bumped every time the modal consumes a mouse-down. The modal hides on the down,
// so the second click of a double-click on its own button is hit-tested afresh and
// lands on whatever surface was underneath. That surface cannot tell such a click
// from one of its own by looking at its own events - it saw a down and an up before
// the modal opened either way - so it compares this counter instead.
inline unsigned& VoLumConfirmClickEpoch()
{
  static unsigned epoch = 0;
  return epoch;
}

// ---------------------------------------------------------------------------
// Shared "Are you sure?" confirmation modal, used for every destructive delete
// (Manage panel + sidebar trash). Attached full-window above other surfaces;
// click-outside or Cancel dismisses, Delete runs the stored callback.
// ---------------------------------------------------------------------------
class VoLumConfirmDialogControl : public IControl
{
public:
  explicit VoLumConfirmDialogControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
  }

  void Show(const std::string& title, const std::string& message, std::function<void()> onConfirm,
            const std::string& confirmLabel = "Delete")
  {
    mTitle = title;
    mMessage = message;
    mConfirmLabel = confirmLabel;
    mOnConfirm = std::move(onConfirm);
    Hide(false);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(190, 8, 10, 14), mRECT); // dimming scrim
    const IRECT box = BoxRect();
    const bool destructive = _IsDestructive();
    const IColor accent = destructive ? VoLumColors::DANGER : VoLumColors::AMBER;

    g.FillRoundRect(destructive ? VoLumColors::DANGER_GLOW : VoLumColors::SEL_GLOW, box.GetPadded(3.5f), 9.f);
    DrawPanelDepth(g, box, 6.f);
    g.DrawRoundRect(accent, box, 6.f, nullptr, 1.6f);

    // Caution glyph (triangle + bang) left of the title.
    _DrawWarnGlyph(g, IRECT(box.L + 18.f, box.T + 12.f, box.L + 40.f, box.T + 34.f), accent);

    g.DrawText(IText(15.f, accent, "Josefin-Bold", EAlign::Center, EVAlign::Top), mTitle.c_str(),
               box.GetPadded(-16.f).GetFromTop(22.f));
    const IRECT msgR = box.GetPadded(-16.f, -38.f, -16.f, -64.f);
    g.PathClipRegion(msgR);
    g.DrawText(
      IText(11.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Center, EVAlign::Middle), mMessage.c_str(), msgR);
    g.PathClipRegion();

    DrawBtn(g, CancelRect(), "Cancel", false, accent);
    DrawBtn(g, DeleteRect(), mConfirmLabel.c_str(), true, accent);

    g.DrawText(IText(9.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               "Enter to confirm  \u00B7  Esc to cancel", box.GetPadded(-12.f).GetFromBottom(11.f));
  }

  bool OnKeyDown(float, float, const IKeyPress& key) override
  {
    if (IsHidden())
      return false;
    if (key.VK == kVK_RETURN)
    {
      auto cb = mOnConfirm;
      Hide(true);
      if (cb)
        cb();
      return true;
    }
    if (key.VK == kVK_ESCAPE)
    {
      Hide(true);
      return true;
    }
    return false;
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    ++VoLumConfirmClickEpoch();
    if (DeleteRect().Contains(x, y))
    {
      auto cb = mOnConfirm;
      Hide(true);
      if (cb)
        cb();
      return;
    }
    // Cancel button or any click outside the box dismisses without acting.
    if (CancelRect().Contains(x, y) || !BoxRect().Contains(x, y))
    {
      Hide(true);
      return;
    }
  }

private:
  IRECT BoxRect() const
  {
    const float w = 444.f, h = 168.f;
    return IRECT(mRECT.MW() - w / 2.f, mRECT.MH() - h / 2.f, mRECT.MW() + w / 2.f, mRECT.MH() + h / 2.f);
  }
  IRECT CancelRect() const
  {
    const IRECT box = BoxRect();
    return IRECT(box.L + 18.f, box.B - 56.f, box.MW() - 6.f, box.B - 30.f);
  }
  IRECT DeleteRect() const
  {
    const IRECT box = BoxRect();
    return IRECT(box.MW() + 6.f, box.B - 56.f, box.R - 18.f, box.B - 30.f);
  }
  bool _IsDestructive() const
  {
    // Delete/Remove read as destructive (red); Save/Overwrite/Replace are caution (amber).
    return mConfirmLabel.find("Delete") != std::string::npos || mConfirmLabel.find("Remove") != std::string::npos;
  }

  // Caution triangle with an exclamation, tinted to the modal accent.
  static void _DrawWarnGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW();
    const float top = r.T + 1.f;
    const float bot = r.B - 1.f;
    IStrokeOptions so;
    so.mJoinOption = ELineJoin::Round;
    g.PathClear();
    g.PathMoveTo(cx, top);
    g.PathLineTo(r.R - 1.f, bot);
    g.PathLineTo(r.L + 1.f, bot);
    g.PathClose();
    g.PathStroke(col, 1.6f, so, nullptr);
    g.DrawLine(col, cx, top + 6.f, cx, bot - 5.f, nullptr, 1.6f);
    g.FillCircle(col, cx, bot - 3.f, 1.0f);
  }

  void DrawBtn(IGraphics& g, const IRECT& r, const char* label, bool primary, const IColor& accent)
  {
    if (primary)
    {
      g.FillRoundRect(_IsDestructive() ? VoLumColors::DANGER_FILL : IColor(70, 232, 168, 92), r, 3.f);
      g.DrawRoundRect(accent, r, 3.f, nullptr, 1.2f);
    }
    else
    {
      g.FillRoundRect(VoLumColors::BTN_OFF_BG, r, 3.f);
      g.DrawRoundRect(VoLumColors::FRAME, r, 3.f, nullptr, 1.f);
    }
    g.DrawText(IText(12.f, primary ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               label, r);
  }

  std::string mTitle, mMessage, mConfirmLabel = "Delete";
  std::function<void()> mOnConfirm;
};
