#pragma once

// In-app Save As name popup. Same family as VoLumConfirmDialogControl: scrim,
// brass panel, Cancel / Save. Replaces CreateTextEntry on the preset bar.

#include "VoLumColorHelpers.h"
#include "VoLumConfirmDialog.h"
#include "VoLumCustomContentApi.h"

#include <functional>
#include <string>

class VoLumNameDialogControl : public IControl
{
public:
  using SaveCallback = std::function<void(const std::string&)>;

  explicit VoLumNameDialogControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
  }

  void Show(const std::string& title, const std::string& message, const std::string& seed, SaveCallback onSave)
  {
    mTitle = title;
    mMessage = message;
    mName = volum::custom::ClampName(seed, volum::custom::kMaxPresetNameLen);
    mOnSave = std::move(onSave);
    Hide(false);
    SetDirty(false);
    StartEntry();
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
    g.PathClipRegion(field.GetPadded(-6.f, 0.f, -6.f, 0.f));
    g.DrawText(IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Near, EVAlign::Middle), mName.c_str(),
               field.GetPadded(-8.f, 0.f, -8.f, 0.f));
    g.PathClipRegion();

    DrawBtn(g, CancelRect(), "Cancel", false);
    DrawBtn(g, SaveRect(), "Save", true);
    g.DrawText(IText(9.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               "Enter to save  \u00B7  Esc to cancel", box.GetPadded(-12.f).GetFromBottom(11.f));
  }

  bool OnKeyDown(float, float, const IKeyPress& key) override
  {
    if (IsHidden())
      return false;
    if (key.VK == kVK_RETURN)
    {
      Commit();
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
    if (SaveRect().Contains(x, y))
    {
      Commit();
      return;
    }
    if (FieldRect().Contains(x, y))
    {
      StartEntry();
      return;
    }
    if (CancelRect().Contains(x, y) || !BoxRect().Contains(x, y))
    {
      Hide(true);
      return;
    }
  }

  void OnTextEntryCompletion(const char* str, int) override
  {
    mName = volum::custom::NormalizePresetName(str);
    Commit();
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

  void StartEntry()
  {
    if (auto* ui = GetUI())
    {
      SetTextEntryLength((int)volum::custom::kMaxPresetNameLen);
      ui->CreateTextEntry(*this, IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
                          FieldRect(), mName.c_str(), kNoValIdx);
    }
  }

  void Commit()
  {
    bool armed = static_cast<bool>(mOnSave);
    if (!volum::custom::NameDialogCommitOnce(armed, mName))
    {
      if (mName.empty() && mOnSave)
        StartEntry();
      return;
    }
    auto cb = std::move(mOnSave);
    mOnSave = nullptr;
    Hide(true);
    cb(mName);
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

  std::string mTitle, mMessage, mName;
  SaveCallback mOnSave;
};
