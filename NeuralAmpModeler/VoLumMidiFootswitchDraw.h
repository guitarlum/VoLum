#pragma once

// Drawing half of VoLumMidiFootswitchControl (VoLumMidiFootswitch.h): the board,
// its switches, the drag ghost and the picker page. Split out for file size; the
// class declares these members and this file, included at the end of that header,
// defines them.

#include "VoLumMidiFootswitch.h"
// A seven-segment readout: unlit "8"s behind the lit digits, the way an LED
// display shows its dark segments.
inline void VoLumMidiFootswitchControl::DrawLedDigits(IGraphics& g, const IRECT& r, const std::string& digits,
                                                      const IColor& ink, float size, bool lit)
{
  const std::string ghost(digits.size(), '8');
  g.DrawText(VoLumType::Value(size, ink.WithOpacity(0.10f)), ghost.c_str(), r);
  if (lit)
    DrawSoftGlowCircle(g, r.MW(), r.MH(), std::max(r.W(), r.H()) * 0.55f, ink.WithOpacity(0.10f));
  g.DrawText(VoLumType::Value(size, lit ? ink : ink.WithOpacity(0.45f)), digits.c_str(), r);
}

inline void VoLumMidiFootswitchControl::DrawArrow(IGraphics& g, const IRECT& r, bool next, bool enabled, bool hot)
{
  const float alpha = enabled ? 1.f : 0.35f;
  g.FillRoundRect(hot && enabled ? VoLumColors::SEL_BG_SOFT : VoLumColors::BTN_OFF_BG, r, 3.f);
  g.DrawRoundRect((hot && enabled ? VoLumColors::GOLD : VoLumColors::FRAME).WithOpacity(alpha), r, 3.f);
  // Byte escapes, not \u: the narrow-literal execution charset is not UTF-8,
  // so a \u2039 arrow comes out as one invalid byte and vanishes.
  g.DrawText(IText(16.f, (hot && enabled ? VoLumColors::GOLD : VoLumColors::GOLD_DIM).WithOpacity(alpha),
                   "Josefin-Bold", EAlign::Center, EVAlign::Middle),
             next ? "\xE2\x80\xBA" : "\xE2\x80\xB9", r.GetTranslated(0.f, -1.f));
}

inline void VoLumMidiFootswitchControl::DrawHeader(IGraphics& g, const volum::footswitch::Layout& layout)
{
  const bool dragging = mDragging;
  const auto hoverKind = mHover.kind;
  DrawArrow(g, ToRect(layout.prev), false, mBank > 0,
            hoverKind == volum::footswitch::HitKind::Prev || mDragPageKind == volum::footswitch::HitKind::Prev);
  DrawArrow(g, ToRect(layout.next), true, mBank < volum::footswitch::kBankCount - 1,
            hoverKind == volum::footswitch::HitKind::Next || mDragPageKind == volum::footswitch::HitKind::Next);

  const IRECT readout = ToRect(layout.readout);
  DrawInsetWell(g, readout, 3.f);
  g.DrawRoundRect(VoLumColors::FRAME, readout, 3.f);
  g.DrawText(
    VoLumType::Label(8.5f, VoLumColors::GOLD_DIM, EAlign::Near), "BANK", readout.GetPadded(-9.f, 0.f, 0.f, 0.f));
  DrawLedDigits(
    g, readout.GetFromRight(40.f).GetTranslated(-6.f, 0.f), BankLabel(mBank), VoLumColors::AMBER, 12.f, true);

  const int first = volum::footswitch::ProgramFor(mBank, 0);
  const int last = volum::footswitch::ProgramFor(mBank, volum::footswitch::kSwitchesPerBank - 1);
  const std::string range = "Program numbers " + std::to_string(first) + "-" + std::to_string(last);
  g.DrawText(VoLumType::Body(11.f, VoLumColors::CREAM_DIM, EAlign::Near), range.c_str(), ToRect(layout.range));

  const int liveBank =
    SlotAt(mLiveProgram) != nullptr && IsLive(*SlotAt(mLiveProgram)) ? volum::footswitch::BankOf(mLiveProgram) : -1;
  for (int i = 0; i < volum::footswitch::kBankCount; ++i)
  {
    const auto& pip = layout.pips[static_cast<size_t>(i)];
    const float cx = pip.MW();
    const float cy = pip.MH();
    const bool current = i == mBank;
    const bool used = mOccupancy[static_cast<size_t>(i)] > 0;
    const bool hot = hoverKind == volum::footswitch::HitKind::Pip && mHover.index == i;
    if (current)
    {
      DrawSoftGlowCircle(g, cx, cy, 9.f, VoLumColors::GOLD.WithOpacity(0.30f));
      g.FillCircle(VoLumColors::GOLD, cx, cy, 3.6f);
    }
    else if (used)
      g.FillCircle(VoLumColors::AMBER.WithOpacity(hot ? 0.95f : 0.55f), cx, cy, 2.8f);
    else
      g.DrawCircle(VoLumColors::GOLD_DIM.WithOpacity(hot ? 0.9f : 0.35f), cx, cy, 2.6f, nullptr, 1.f);
    if (i == liveBank && !current)
      g.DrawCircle(VoLumColors::GOLD, cx, cy, 5.2f, nullptr, 1.f);
  }

  const IRECT hint = ToRect(layout.hint);
  const IText dim = VoLumType::Body(10.f, VoLumColors::TEXT_DIM.WithOpacity(0.72f), EAlign::Far);
  const char* line1 = "Click a switch to choose its Sound.";
  const char* line2 = "Drag onto another to swap. The cross clears.";
  IText ink = dim;
  if (mChoices.empty())
  {
    line1 = "Save a preset first:";
    line2 = "a Sound is an amp plus a named preset.";
    if (mEmptyFlash > 0.f)
      ink = VoLumType::Label(10.f, VoLumColors::GOLD.WithOpacity(0.55f + 0.45f * mEmptyFlash), EAlign::Far);
  }
  else if (dragging)
  {
    line1 = "Drop on a switch: taken swaps, empty moves.";
    line2 = "Hover an arrow or a bank dot to page.";
    ink = VoLumType::Body(10.f, VoLumColors::GOLD, EAlign::Far);
  }
  else if (mSlots.empty())
  {
    line1 = "Nothing assigned yet.";
    line2 = "Click a switch to give its program number a Sound.";
  }
  g.DrawText(ink, FitTextToWidth(g, ink, line1, hint.W()).c_str(), hint.GetFromTop(hint.H() * 0.5f));
  g.DrawText(ink, FitTextToWidth(g, ink, line2, hint.W()).c_str(), hint.GetFromBottom(hint.H() * 0.5f));
}

inline IRECT VoLumMidiFootswitchControl::ThumbRect(const IRECT& tile)
{
  const float s = tile.H() - 20.f;
  return IRECT(tile.L + 10.f, tile.T + 10.f, tile.L + 10.f + s, tile.T + 10.f + s);
}

// An unassigned switch is a bare stomp cap: brushed ring, dark centre, "+".
inline void VoLumMidiFootswitchControl::DrawStompCap(IGraphics& g, const IRECT& thumb, bool hot)
{
  const float cx = thumb.MW();
  const float cy = thumb.MH();
  const float rad = thumb.W() * 0.42f;
  g.FillCircle(IColor(110, 0, 0, 0), cx, cy + 2.f, rad + 1.5f);
  g.PathCircle(cx, cy, rad);
  g.PathFill(IPattern::CreateLinearGradient(
    cx, cy - rad, cx, cy + rad, {{IColor(255, 74, 68, 58), 0.f}, {IColor(255, 30, 29, 33), 1.f}}));
  g.DrawCircle(VoLumColors::GOLD_DIM.WithOpacity(hot ? 0.85f : 0.40f), cx, cy, rad, nullptr, 1.2f);
  const float inner = rad * 0.70f;
  g.PathCircle(cx, cy, inner);
  g.PathFill(IPattern::CreateRadialGradient(
    cx, cy - inner * 0.4f, inner * 1.3f, {{IColor(255, 34, 33, 40), 0.f}, {IColor(255, 12, 12, 17), 1.f}}));
  const float arm = inner * 0.45f;
  const IColor plus = hot ? VoLumColors::GOLD : VoLumColors::GOLD_DIM.WithOpacity(0.75f);
  g.DrawLine(plus, cx - arm, cy, cx + arm, cy, nullptr, 2.f);
  g.DrawLine(plus, cx, cy - arm, cx, cy + arm, nullptr, 2.f);
}

inline void VoLumMidiFootswitchControl::FillTileFace(IGraphics& g, const IRECT& tile, const IColor& top,
                                                     const IColor& bot)
{
  g.PathRoundRect(tile, kTileRadius);
  g.PathFill(IPattern::CreateLinearGradient(tile.L, tile.T, tile.L, tile.B, {{top, 0.f}, {bot, 1.f}}));
  g.DrawLine(VoLumColors::RIM_LIGHT, tile.L + kTileRadius, tile.T + 1.f, tile.R - kTileRadius, tile.T + 1.f);
}

inline void VoLumMidiFootswitchControl::DrawSwitch(IGraphics& g, const volum::footswitch::Layout& layout, int index)
{
  const IRECT tile = ToRect(layout.tiles[static_cast<size_t>(index)]);
  const int program = volum::footswitch::ProgramFor(mBank, index);
  const volum::PlaySlot* slot = SlotAt(program);
  const bool assigned = slot != nullptr;
  const bool valid = assigned && slot->valid;
  const bool live = valid && IsLive(*slot);
  const bool tileHover =
    !mDragging && (mHover.kind == volum::footswitch::HitKind::Tile || mHover.kind == volum::footswitch::HitKind::Clear)
    && mHover.index == index;
  const bool clearHot = tileHover && mHover.kind == volum::footswitch::HitKind::Clear;
  const bool source = mDragging && program == mPressProgram;
  const bool target = mDragging && program == mDropProgram && program != mPressProgram;

  // Face: lifted panel for a Sound, a darker recess for an empty switch, the
  // PLAY rail's red for a Sound that is gone.
  if (!assigned)
    FillTileFace(g, tile, IColor(255, 17, 17, 23), IColor(255, 11, 11, 16));
  else if (!valid)
    FillTileFace(g, tile, IColor(255, 40, 17, 19), IColor(255, 24, 10, 12));
  else
    FillTileFace(g, tile, VoLumColors::PANEL_TOP, VoLumColors::PANEL_BOT);
  DrawVoLumSelection(g, tile, live, tileHover && !live, VoLumSelectionStyle::Brass, kTileRadius, 0.f);
  if (!live)
  {
    const IColor edge = !assigned ? VoLumColors::FRAME.WithOpacity(0.55f)
                                  : (valid ? VoLumColors::FRAME : VoLumColors::DANGER.WithOpacity(0.6f));
    g.DrawRoundRect(tileHover ? VoLumColors::GOLD_DIM.WithOpacity(0.8f) : edge, tile, kTileRadius);
  }

  const IRECT thumb = ThumbRect(tile);
  if (valid)
  {
    g.FillRoundRect(VoLumColors::HERO_BG, thumb, 3.f);
    const ILayerPtr& art = ArtLayer(slot->sound);
    if (art && g.CheckLayer(art))
      g.DrawFittedLayer(art, thumb, nullptr);
    g.DrawRoundRect(live ? VoLumColors::SEL_BORDER : VoLumColors::FRAME, thumb, 3.f);
  }
  else if (assigned)
  {
    DrawInsetWell(g, thumb, 3.f);
    g.DrawRoundRect(VoLumColors::DANGER.WithOpacity(0.7f), thumb, 3.f);
    g.DrawText(VoLumType::Label(24.f, VoLumColors::DANGER), "!", thumb);
  }
  else
  {
    DrawStompCap(g, thumb, tileHover || target);
  }

  // Right column: LED program readout and LIVE lamp on top, then the Sound.
  const float textL = thumb.R + 11.f;
  const IRECT led(textL, tile.T + 9.f, textL + 46.f, tile.T + 28.f);
  DrawInsetWell(g, led, 2.5f);
  g.DrawRoundRect(!assigned || valid ? VoLumColors::FRAME : VoLumColors::DANGER.WithOpacity(0.55f), led, 2.5f);
  DrawLedDigits(g, led, ProgramLabel(program),
                !assigned ? VoLumColors::CREAM_DIM : (valid ? VoLumColors::AMBER : VoLumColors::DANGER), 11.f,
                assigned);

  const float lampX = tile.R - 14.f;
  const float lampY = led.MH();
  if (live)
  {
    DrawSoftGlowCircle(g, lampX, lampY, 11.f, VoLumColors::GOLD.WithOpacity(0.45f));
    g.FillCircle(VoLumColors::GOLD, lampX, lampY, 3.6f);
    g.DrawText(
      VoLumType::Label(8.f, VoLumColors::GOLD, EAlign::Far), "LIVE", IRECT(led.R + 6.f, led.T, lampX - 7.f, led.B));
  }
  else
  {
    g.FillCircle(IColor(255, 34, 28, 22), lampX, lampY, 3.4f);
    g.DrawCircle(VoLumColors::FRAME, lampX, lampY, 3.4f, nullptr, 1.f);
  }

  const float textR = tile.R - 10.f;
  const IRECT nameR(textL, tile.T + 31.f, textR, tile.T + 50.f);
  const float ampRight =
    tileHover && assigned ? ToRect(volum::footswitch::ClearBox(layout.tiles[(size_t)index])).L - 4.f : textR;
  const IRECT ampR(textL, tile.T + 50.f, ampRight, tile.T + 64.f);
  if (valid)
  {
    const IText name = VoLumType::Display(16.f, live ? VoLumColors::SEL_TEXT : VoLumColors::CREAM, EAlign::Near);
    g.DrawText(name, FitTextToWidth(g, name, slot->sound.presetName.c_str(), nameR.W()).c_str(), nameR);
    const IText amp = VoLumType::Label(9.f, VoLumColors::TEAL_DIM, EAlign::Near);
    g.DrawText(amp, FitTextToWidth(g, amp, slot->sound.ampName.c_str(), ampR.W()).c_str(), ampR);
  }
  else if (assigned)
  {
    // Same words as the PLAY rail: one state, one name.
    g.DrawText(VoLumType::Label(11.f, VoLumColors::DANGER, EAlign::Near),
               volum::OccupiedSlotLabel(false, slot->sound.presetName).c_str(), nameR);
    const IText why = VoLumType::Label(9.f, VoLumColors::DANGER.WithOpacity(0.7f), EAlign::Near);
    g.DrawText(
      why, FitTextToWidth(g, why, tileHover ? "Click to reassign" : "Its Sound is gone", ampR.W()).c_str(), ampR);
  }
  else
  {
    g.DrawText(VoLumType::Body(12.f, VoLumColors::CREAM_DIM, EAlign::Near), "Empty", nameR);
    const IText cue = VoLumType::Label(9.f, VoLumColors::GOLD_DIM, EAlign::Near);
    if (tileHover)
      g.DrawText(cue, FitTextToWidth(g, cue, "Click to choose a Sound", ampR.W()).c_str(), ampR);
  }

  if (tileHover && assigned)
  {
    const IRECT clear = ToRect(volum::footswitch::ClearBox(layout.tiles[static_cast<size_t>(index)]));
    g.FillRoundRect(clearHot ? VoLumColors::DANGER.WithOpacity(0.55f) : VoLumColors::DANGER_FILL, clear, 3.f);
    g.DrawRoundRect(VoLumColors::DANGER, clear, 3.f);
    DrawCrossGlyph(g, clear, VoLumColors::TEXT_BRIGHT);
  }

  if (source)
  {
    g.FillRoundRect(IColor(160, 9, 9, 14), tile, kTileRadius);
    g.DrawDottedRect(VoLumColors::GOLD_DIM, tile.GetPadded(-2.f), nullptr, 1.f, 3.f);
  }
  if (target)
    g.DrawRoundRect(VoLumColors::GOLD, tile.GetPadded(-1.f), kTileRadius, nullptr, 2.f);
}

// Painted after the ghost: the ghost sits on the pointer, which is over the
// target, and the badge is the one thing on that switch the player must read.
inline void VoLumMidiFootswitchControl::DrawDropBadge(IGraphics& g, const volum::footswitch::Layout& layout)
{
  if (!mDragging || mDropProgram < 0 || mDropProgram == mPressProgram
      || volum::footswitch::BankOf(mDropProgram) != mBank)
    return;
  const IRECT tile = ToRect(layout.tiles[static_cast<size_t>(volum::footswitch::SwitchOf(mDropProgram))]);
  const bool swap = SlotAt(mDropProgram) != nullptr;
  const IRECT pill(tile.R - (swap ? 56.f : 76.f), tile.T - 7.f, tile.R - 10.f, tile.T + 9.f);
  g.FillRoundRect(IColor(140, 0, 0, 0), pill.GetTranslated(0.f, 1.5f), 8.f);
  g.FillRoundRect(VoLumColors::GOLD, pill, 8.f);
  g.DrawText(VoLumType::Label(8.5f, IColor(255, 26, 18, 8)), swap ? "SWAP" : "MOVE HERE", pill);
}

inline void VoLumMidiFootswitchControl::DrawDragGhost(IGraphics& g)
{
  const volum::PlaySlot* slot = mDragging ? SlotAt(mPressProgram) : nullptr;
  if (!slot)
    return;
  const float w = 168.f;
  const float h = 42.f;
  const float l = std::clamp(mDragX - w * 0.5f, mRECT.L, mRECT.R - w);
  const float t = std::clamp(mDragY - h * 0.5f, mRECT.T, mRECT.B - h);
  const IRECT ghost(l, t, l + w, t + h);
  g.FillRoundRect(IColor(120, 0, 0, 0), ghost.GetTranslated(2.f, 3.f), 6.f);
  g.FillRoundRect(VoLumColors::WELL_DARK.WithOpacity(0.94f), ghost, 6.f);
  g.DrawRoundRect(VoLumColors::GOLD, ghost, 6.f, nullptr, 1.4f);
  const IRECT art(ghost.L + 6.f, ghost.T + 6.f, ghost.L + 36.f, ghost.T + 36.f);
  if (slot->valid)
  {
    g.FillRect(VoLumColors::HERO_BG, art);
    const ILayerPtr& layer = ArtLayer(slot->sound);
    if (layer && g.CheckLayer(layer))
      g.DrawFittedLayer(layer, art, nullptr);
  }
  const IRECT text(art.R + 8.f, ghost.T + 4.f, ghost.R - 8.f, ghost.B - 4.f);
  const IText label = VoLumType::Value(9.f, VoLumColors::AMBER, EAlign::Near);
  g.DrawText(label, ProgramLabel(mPressProgram).c_str(), text.GetFromTop(14.f));
  const IText name = VoLumType::Display(14.f, slot->valid ? VoLumColors::CREAM : VoLumColors::DANGER, EAlign::Near);
  const std::string shown = volum::OccupiedSlotLabel(slot->valid, slot->sound.presetName);
  g.DrawText(name, FitTextToWidth(g, name, shown.c_str(), text.W()).c_str(), text.GetFromBottom(20.f));
}

inline const ILayerPtr& VoLumMidiFootswitchControl::ArtLayer(const volum::SoundChoice& sound) const
{
  static const ILayerPtr kNone;
  if (sound.customArt)
    return mCustomArt[static_cast<size_t>(CustomArtIndex(sound.art))];
  const size_t idx = static_cast<size_t>(FactoryArtIndex(sound.art));
  return idx < mFactoryArt.size() ? mFactoryArt[idx] : kNone;
}

// The PLAY rail's mini art and inks, one cached layer per amp, built at the
// thumb's logical size. Must run with no clip set: StartLayer/EndLayer mutates
// the clip region.
inline void VoLumMidiFootswitchControl::BuildArtLayers(IGraphics& g, const volum::footswitch::Layout& layout)
{
  if (mFactoryArt.size() != static_cast<size_t>(volum::kAmpCount))
    mFactoryArt.resize(static_cast<size_t>(volum::kAmpCount));
  const IRECT thumb = ThumbRect(ToRect(layout.tiles[0]));
  const IRECT build(mRECT.L, mRECT.T, mRECT.L + thumb.W(), mRECT.T + thumb.H());
  for (const auto& slot : mSlots)
  {
    if (!slot.valid)
      continue;
    if (slot.sound.customArt)
    {
      const int art = CustomArtIndex(slot.sound.art);
      if (!g.CheckLayer(mCustomArt[static_cast<size_t>(art)]))
      {
        g.StartLayer(this, build);
        DrawCustomAmpArt(g, build, art, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
        mCustomArt[static_cast<size_t>(art)] = g.EndLayer();
      }
      continue;
    }
    const int amp = FactoryArtIndex(slot.sound.art);
    if (!g.CheckLayer(mFactoryArt[static_cast<size_t>(amp)]))
    {
      g.StartLayer(this, build);
      DrawSidebarMiniFractal(g, build, FractalCaseForAmp(amp), IColor(200, 120, 210, 220), IColor(100, 100, 180, 200));
      mFactoryArt[static_cast<size_t>(amp)] = g.EndLayer();
    }
  }
}

inline void VoLumMidiFootswitchControl::DrawPicker(IGraphics& g)
{
  const std::string where = "Bank " + BankLabel(volum::footswitch::BankOf(mEditProgram)) + ", switch "
                            + std::to_string(volum::footswitch::SwitchOf(mEditProgram) + 1);
  const volum::PlaySlot* current = SlotAt(mEditProgram);
  Picker().Draw(g, "Sound for program number " + ProgramLabel(mEditProgram), where,
                current && current->valid ? &current->sound : nullptr);
}
