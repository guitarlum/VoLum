#pragma once

// Pure model + geometry for the Settings MIDI footswitch view
// (VoLumMidiFootswitchControl in VoLumMidiFootswitch.h).
//
// The 128 program numbers are shown the way a floor controller pages them:
// sixteen banks of eight switches, program = bank * 8 + switch. Nothing here
// touches IGraphics or the content store, so the numbering, the bank a program
// lives on, what a drop does and where each hit zone sits are all unit-tested
// in test_volum_midi_footswitch.cpp.

#include "VoLumMidi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace volum
{
namespace footswitch
{

inline constexpr int kBankCount = 16;
inline constexpr int kSwitchesPerBank = 8;
inline constexpr int kColumns = 4;
inline constexpr int kRows = 2;
static_assert(kBankCount * kSwitchesPerBank == kMidiSoundSlotCount, "banks must cover every program number");
static_assert(kColumns * kRows == kSwitchesPerBank, "the grid must hold one bank");

inline bool IsProgram(int program)
{
  return program >= 0 && program < kMidiSoundSlotCount;
}

// -1 for a bank or switch that does not exist, never a wrapped number: a
// program that silently lands on another bank is a Sound in the wrong place.
inline int ProgramFor(int bank, int sw)
{
  if (bank < 0 || bank >= kBankCount || sw < 0 || sw >= kSwitchesPerBank)
    return -1;
  return bank * kSwitchesPerBank + sw;
}

inline int BankOf(int program)
{
  return IsProgram(program) ? program / kSwitchesPerBank : -1;
}
inline int SwitchOf(int program)
{
  return IsProgram(program) ? program % kSwitchesPerBank : -1;
}

inline int ClampBank(int bank)
{
  return std::clamp(bank, 0, kBankCount - 1);
}

// The page the view opens on: the one holding the live (last recalled) program,
// or the first bank when nothing has been recalled yet.
inline int OpeningBank(int liveProgram)
{
  const int bank = BankOf(liveProgram);
  return bank < 0 ? 0 : bank;
}

// Arrows and the wheel stop at the ends rather than wrapping: a wheel that
// jumps from bank 16 to bank 1 reads as the view resetting.
inline int StepBank(int bank, int dir)
{
  return ClampBank(bank + (dir > 0 ? 1 : (dir < 0 ? -1 : 0)));
}

// One bank per wheel notch. Trackpads deliver fractions of a notch, so they
// accumulate until a whole notch has passed. Wheel up (positive delta) pages
// back, like scrolling a list up.
inline int WheelBankStep(float& accum, float delta)
{
  if (!std::isfinite(delta))
    return 0;
  accum += delta;
  if (accum >= 1.f)
  {
    accum = 0.f;
    return -1;
  }
  if (accum <= -1.f)
  {
    accum = 0.f;
    return 1;
  }
  return 0;
}

// Assigned-program counts per bank, for the bank pips.
inline std::array<int, kBankCount> BankOccupancy(const std::vector<int>& assignedPrograms)
{
  std::array<int, kBankCount> out{};
  for (const int program : assignedPrograms)
  {
    const int bank = BankOf(program);
    if (bank >= 0)
      ++out[static_cast<size_t>(bank)];
  }
  return out;
}

// What releasing a dragged switch over another does. Both outcomes go out
// through the plugin's swap writer (SwapMidiSoundSlots moves onto a free
// program and exchanges with an occupied one), so no drop can drop a Sound.
enum class DropAction
{
  None,
  Move,
  Swap,
};

inline DropAction DecideDrop(int fromProgram, int toProgram, bool fromAssigned, bool toAssigned)
{
  if (!IsProgram(fromProgram) || !IsProgram(toProgram) || fromProgram == toProgram || !fromAssigned)
    return DropAction::None;
  return toAssigned ? DropAction::Swap : DropAction::Move;
}

// ---- Geometry ---------------------------------------------------------------

struct Box
{
  float L = 0.f, T = 0.f, R = 0.f, B = 0.f;
  float W() const { return R - L; }
  float H() const { return B - T; }
  float MW() const { return 0.5f * (L + R); }
  float MH() const { return 0.5f * (T + B); }
  bool Contains(float x, float y) const { return x >= L && x < R && y >= T && y < B; }
};

inline constexpr float kHeaderH = 22.f;
inline constexpr float kHeaderGap = 10.f;
inline constexpr float kTileGapX = 12.f;
inline constexpr float kTileGapY = 10.f;
inline constexpr float kArrowW = 24.f;
inline constexpr float kReadoutW = 86.f;
inline constexpr float kPipPitch = 13.f;
inline constexpr float kPipHitR = 6.f;
inline constexpr float kClearSize = 18.f;

struct Layout
{
  Box header;
  Box prev;
  Box readout;
  Box next;
  Box range;
  Box pipStrip;
  std::array<Box, kBankCount> pips{};
  Box hint;
  Box grid;
  std::array<Box, kSwitchesPerBank> tiles{};
};

inline Layout LayoutFootswitch(float l, float t, float r, float b)
{
  Layout out;
  out.header = {l, t, r, t + kHeaderH};
  out.prev = {l, t, l + kArrowW, t + kHeaderH};
  out.readout = {out.prev.R + 4.f, t, out.prev.R + 4.f + kReadoutW, t + kHeaderH};
  out.next = {out.readout.R + 4.f, t, out.readout.R + 4.f + kArrowW, t + kHeaderH};
  out.range = {out.next.R + 12.f, t, out.next.R + 12.f + 132.f, t + kHeaderH};

  const float stripW = kPipPitch * static_cast<float>(kBankCount);
  out.pipStrip = {out.range.R, t, out.range.R + stripW, t + kHeaderH};
  for (int i = 0; i < kBankCount; ++i)
  {
    const float cx = out.pipStrip.L + kPipPitch * (static_cast<float>(i) + 0.5f);
    const float cy = out.header.MH();
    out.pips[static_cast<size_t>(i)] = {cx - kPipHitR, cy - kPipHitR - 3.f, cx + kPipHitR, cy + kPipHitR + 3.f};
  }
  out.hint = {out.pipStrip.R + 14.f, t, r, t + kHeaderH};

  out.grid = {l, out.header.B + kHeaderGap, r, b};
  const float tileW = (out.grid.W() - kTileGapX * static_cast<float>(kColumns - 1)) / static_cast<float>(kColumns);
  const float tileH = (out.grid.H() - kTileGapY * static_cast<float>(kRows - 1)) / static_cast<float>(kRows);
  for (int i = 0; i < kSwitchesPerBank; ++i)
  {
    const int col = i % kColumns;
    const int row = i / kColumns;
    const float tl = out.grid.L + static_cast<float>(col) * (tileW + kTileGapX);
    const float tt = out.grid.T + static_cast<float>(row) * (tileH + kTileGapY);
    out.tiles[static_cast<size_t>(i)] = {tl, tt, tl + tileW, tt + tileH};
  }
  return out;
}

// Bottom-right, the same corner PLAY's rail uses for its clear button, so the
// LIVE lamp in the top-right is never covered by it.
inline Box ClearBox(const Box& tile)
{
  return {tile.R - kClearSize - 7.f, tile.B - kClearSize - 7.f, tile.R - 7.f, tile.B - 7.f};
}

enum class HitKind
{
  None,
  Prev,
  Next,
  Pip,
  Tile,
  Clear,
};

struct Hit
{
  HitKind kind = HitKind::None;
  int index = -1;
};

// `occupied[i]` says whether switch i on the shown bank holds a Sound: only
// those have a clear button to hit.
inline Hit HitTest(const Layout& layout, float x, float y, const std::array<bool, kSwitchesPerBank>& occupied)
{
  if (layout.prev.Contains(x, y))
    return {HitKind::Prev, -1};
  if (layout.next.Contains(x, y))
    return {HitKind::Next, -1};
  for (int i = 0; i < kBankCount; ++i)
    if (layout.pips[static_cast<size_t>(i)].Contains(x, y))
      return {HitKind::Pip, i};
  for (int i = 0; i < kSwitchesPerBank; ++i)
  {
    const Box& tile = layout.tiles[static_cast<size_t>(i)];
    if (!tile.Contains(x, y))
      continue;
    if (occupied[static_cast<size_t>(i)] && ClearBox(tile).Contains(x, y))
      return {HitKind::Clear, i};
    return {HitKind::Tile, i};
  }
  return {};
}

} // namespace footswitch
} // namespace volum
