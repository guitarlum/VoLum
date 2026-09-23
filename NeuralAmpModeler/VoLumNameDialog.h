#pragma once

// In-app Save As name popup. Same family as VoLumConfirmDialogControl: scrim,
// brass panel, Cancel / Save. Replaces CreateTextEntry on the preset bar.
//
// A thin view over volum::name_dialog (VoLumNameDialogModel.h). The name field is
// drawn and edited here rather than through iPlug's text entry, so the confirm
// button can read "Update" or "Save" as the user types and Cancel reaches this
// control instead of committing the edit.

#include "VoLumColorHelpers.h"
#include "VoLumConfirmDialog.h"
#include "VoLumCustomContentApi.h"
#include "VoLumNameDialogModel.h"

#include <cmath>
#include <functional>
#include <string>

class VoLumNameDialogControl : public IControl
{
public:
  using SaveCallback = std::function<void(const std::string&)>;
  using CancelCallback = std::function<void()>;

  explicit VoLumNameDialogControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
  }

  // overwriteName: the active User preset's name, or empty when there is nothing
  // this dialog could update. Typing exactly that name turns Save into Update.
  void Show(const std::string& title, const std::string& message, const std::string& seed,
            const std::string& overwriteName, SaveCallback onSave, CancelCallback onCancel = {})
  {
    Dismiss(); // a dialog still armed from an earlier prompt is cancelled, never committed
    DismissForeignTextEntry();
    mTitle = title;
    mMessage = message;
    volum::name_dialog::Open(mModel, seed, overwriteName);
    mOnSave = std::move(onSave);
    mOnCancel = std::move(onCancel);
    IControl::Hide(false);
    SetDirty(false);
  }

  // Cancel: disarm and hide. Nothing is written.
  void Dismiss()
  {
    if (!volum::name_dialog::Cancel(mModel))
      return;
    mOnSave = nullptr;
    auto cancelled = std::move(mOnCancel);
    mOnCancel = nullptr;
    IControl::Hide(true);
    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();
    if (cancelled)
      cancelled();
  }

  bool IsArmed() const { return volum::name_dialog::IsArmed(mModel); }

  // Anyone hiding the dialog (overlay close paths, a mode switch) cancels it, so a
  // stale save callback can never fire on a later click.
  void Hide(bool hide) override
  {
    if (hide && IsArmed())
    {
      Dismiss();
      return;
    }
    IControl::Hide(hide);
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(190, 8, 10, 14), mRECT);
    const IRECT box = BoxRect();
    g.FillRoundRect(VoLumColors::SEL_GLOW, box.GetPadded(3.5f), 9.f);
    DrawPanelDepth(g, box, 6.f);
    g.DrawRoundRect(VoLumColors::GOLD, box, 6.f, nullptr, 1.6f);

    g.DrawText(IText(15.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Top), mTitle.c_str(),
               box.GetPadded(-16.f).GetFromTop(22.f));
    g.DrawText(IText(11.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Center, EVAlign::Top), mMessage.c_str(),
               IRECT(box.L + 16.f, box.T + 36.f, box.R - 16.f, box.T + 56.f));

    const IRECT field = FieldRect();
    g.FillRect(VoLumColors::WELL_DARK, field);
    g.DrawRect(VoLumColors::GOLD_DIM, field);
    const IRECT textRect = TextRect();
    g.PathClipRegion(field.GetPadded(-6.f, 0.f, -6.f, 0.f));
    if (volum::name_dialog::HasSelection(mModel))
    {
      const float x0 = textRect.L + PrefixWidth(g, volum::name_dialog::SelectionStart(mModel));
      const float x1 = textRect.L + PrefixWidth(g, volum::name_dialog::SelectionEnd(mModel));
      g.FillRect(IColor(90, 232, 168, 92), IRECT(x0, field.T + 7.f, x1, field.B - 7.f));
    }
    g.DrawText(FieldText(), mModel.draft.c_str(), textRect);
    const float caretX = std::round(textRect.L + PrefixWidth(g, mModel.caret));
    g.FillRect(VoLumColors::TEXT_BRIGHT, IRECT(caretX, field.T + 7.f, caretX + 1.f, field.B - 7.f));
    g.PathClipRegion();

    const auto label = volum::name_dialog::LabelFor(mModel);
    DrawBtn(g, CancelRect(), "Cancel", false);
    DrawBtn(g, SaveRect(), volum::name_dialog::LabelText(label), true);
    g.DrawText(IText(9.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               label == volum::name_dialog::ConfirmLabel::Update ? "Enter to update  \u00B7  Esc to cancel"
                                                                 : "Enter to save  \u00B7  Esc to cancel",
               box.GetPadded(-12.f).GetFromBottom(11.f));
  }

  // Reached directly when the pointer is over the dialog, and through the plugin
  // key router (KeyConsumer::NameDialogKey) when it is not. Modal: every key is
  // consumed while the dialog is up.
  bool OnKeyDown(float, float, const IKeyPress& key) override
  {
    if (IsHidden())
      return false;
    using volum::name_dialog::KeyAction;
    switch (volum::name_dialog::ApplyKey(mModel, key.VK, key.utf8, key.S, key.C, key.A))
    {
      case KeyAction::Commit: Commit(); break;
      case KeyAction::Cancel: Dismiss(); break;
      case KeyAction::Copy: CopySelection(); break;
      case KeyAction::Cut:
        CopySelection();
        volum::name_dialog::DeleteSelection(mModel);
        break;
      case KeyAction::Paste: Paste(); break;
      case KeyAction::Edited:
      case KeyAction::Ignored: break;
    }
    SetDirty(false);
    return true;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ++VoLumConfirmClickEpoch();
    if (SaveRect().Contains(x, y))
    {
      Commit();
      return;
    }
    if (FieldRect().Contains(x, y))
    {
      volum::name_dialog::MoveCaretTo(mModel, CaretForX(x), mod.S);
      SetDirty(false);
      return;
    }
    if (CancelRect().Contains(x, y) || !BoxRect().Contains(x, y))
    {
      Dismiss();
      return;
    }
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (FieldRect().Contains(x, y))
    {
      volum::name_dialog::SelectAll(mModel);
      SetDirty(false);
      return;
    }
    OnMouseDown(x, y, mod);
  }

  // The dialog never opens a text entry of its own; this only matters if some
  // other path ever does. A completion carries text, not intent: draft only.
  void OnTextEntryCompletion(const char* str, int) override
  {
    volum::name_dialog::ApplyTextEntryCompletion(mModel, str);
    SetDirty(false);
  }

private:
  IRECT BoxRect() const
  {
    const float w = 420.f, h = 188.f;
    return IRECT(mRECT.MW() - w / 2.f, mRECT.MH() - h / 2.f, mRECT.MW() + w / 2.f, mRECT.MH() + h / 2.f);
  }
  IRECT FieldRect() const
  {
    const IRECT box = BoxRect();
    return IRECT(box.L + 18.f, box.T + 68.f, box.R - 18.f, box.T + 100.f);
  }
  IRECT TextRect() const { return FieldRect().GetPadded(-8.f, 0.f, -8.f, 0.f); }
  IRECT CancelRect() const
  {
    const IRECT box = BoxRect();
    return IRECT(box.L + 18.f, box.B - 56.f, box.MW() - 6.f, box.B - 30.f);
  }
  IRECT SaveRect() const
  {
    const IRECT box = BoxRect();
    return IRECT(box.MW() + 6.f, box.B - 56.f, box.R - 18.f, box.B - 30.f);
  }

  static IText FieldText()
  {
    return IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
  }

  float PrefixWidth(IGraphics& g, std::size_t bytes) const
  {
    if (bytes == 0)
      return 0.f;
    IRECT bounds;
    return g.MeasureText(FieldText(), mModel.draft.substr(0, bytes).c_str(), bounds);
  }

  std::size_t CaretForX(float x)
  {
    auto* ui = GetUI();
    if (!ui)
      return mModel.draft.size();
    const float local = x - TextRect().L;
    std::size_t best = 0;
    float bestDist = std::fabs(local);
    for (std::size_t pos = volum::name_dialog::NextBoundary(mModel.draft, 0); pos <= mModel.draft.size();
         pos = volum::name_dialog::NextBoundary(mModel.draft, pos))
    {
      const float dist = std::fabs(local - PrefixWidth(*ui, pos));
      if (dist < bestDist)
      {
        bestDist = dist;
        best = pos;
      }
      if (pos == mModel.draft.size())
        break;
    }
    return best;
  }

  // An exact-value box left open underneath would keep receiving keys and clicks
  // before this dialog does.
  void DismissForeignTextEntry()
  {
    if (auto* ui = GetUI())
      if (ui->GetControlInTextEntry())
        if (auto* textEntry = ui->GetTextEntryControl())
          textEntry->DismissEdit();
  }

  void CopySelection()
  {
    if (auto* ui = GetUI(); ui && volum::name_dialog::HasSelection(mModel))
      ui->SetTextInClipboard(volum::name_dialog::SelectedText(mModel).c_str());
  }

  void Paste()
  {
    WDL_String clip;
    if (auto* ui = GetUI(); ui && ui->GetTextFromClipboard(clip))
      volum::name_dialog::InsertText(mModel, clip.Get());
  }

  void Commit()
  {
    std::string name;
    if (!volum::name_dialog::Commit(mModel, name))
      return;
    auto cb = std::move(mOnSave);
    mOnSave = nullptr;
    mOnCancel = nullptr;
    IControl::Hide(true);
    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();
    if (cb)
      cb(name);
  }

  void DrawBtn(IGraphics& g, const IRECT& r, const char* label, bool primary)
  {
    if (primary)
    {
      g.FillRoundRect(IColor(70, 232, 168, 92), r, 3.f);
      g.DrawRoundRect(VoLumColors::GOLD, r, 3.f, nullptr, 1.2f);
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

  std::string mTitle, mMessage;
  volum::name_dialog::State mModel;
  SaveCallback mOnSave;
  CancelCallback mOnCancel;
};
