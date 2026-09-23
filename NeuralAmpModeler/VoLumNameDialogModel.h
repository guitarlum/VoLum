#pragma once

// Pure state machine behind VoLumNameDialogControl (Save preset, PLAY "Add this
// sound"). The control is a thin view: it forwards clicks and keys here and draws
// what this reports.
//
// Phases: Closed -> Armed -> Committed | Cancelled. Only Commit (Enter or the
// confirm button) hands a name to the caller. Everything else that ends the
// dialog - Cancel, Esc, a click outside, an external Hide - is a Cancel and writes
// nothing. A text-entry completion only replaces the draft: iPlug fires one when
// the field loses focus, which is exactly what clicking Cancel does, and treating
// it as a commit is how Cancel used to create presets.
//
// The field is owned by the dialog, not iPlug's ITextEntryControl, because that
// control reports its text only on completion and routes every click while it is
// open to itself. Owning the draft is what lets the confirm label read "Update" or
// "Save" as each key lands, on every platform.

#include "VoLumCustomModel.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace volum::name_dialog
{

enum class Phase
{
  Closed,
  Armed,
  Committed,
  Cancelled,
};

enum class ConfirmLabel
{
  Save,
  Update,
};

struct State
{
  Phase phase = Phase::Closed;
  std::string draft;
  // Byte offsets into draft, always on UTF-8 boundaries. The selection is the
  // span between them; equal means a plain caret.
  std::size_t caret = 0;
  std::size_t anchor = 0;
  // Name of the active User preset this dialog may overwrite. Empty when the live
  // sound is Default, a Factory preset, or anything else that cannot be updated.
  std::string overwriteName;
};

inline bool IsArmed(const State& s)
{
  return s.phase == Phase::Armed;
}

// A real overwrite: the typed name, as it would be saved, is the active User
// preset's own name. Anything else creates a new preset.
inline bool Overwrites(const std::string& typed, const std::string& overwriteName)
{
  return !overwriteName.empty() && volum::custom::NormalizePresetName(typed.c_str()) == overwriteName;
}

inline ConfirmLabel LabelFor(const State& s)
{
  return Overwrites(s.draft, s.overwriteName) ? ConfirmLabel::Update : ConfirmLabel::Save;
}

inline const char* LabelText(ConfirmLabel label)
{
  return label == ConfirmLabel::Update ? "Update" : "Save";
}

inline void Open(State& s, const std::string& seed, const std::string& overwriteName)
{
  s.phase = Phase::Armed;
  s.draft = volum::custom::ClampName(seed, volum::custom::kMaxPresetNameLen);
  s.overwriteName = overwriteName;
  // Seed fully selected, so typing replaces it and Enter keeps it.
  s.anchor = 0;
  s.caret = s.draft.size();
}

// Enter or the confirm button. Returns true exactly once per Open, with the name
// to save in outName. An empty name keeps the dialog armed.
inline bool Commit(State& s, std::string& outName)
{
  if (!IsArmed(s))
    return false;
  const std::string name = volum::custom::NormalizePresetName(s.draft.c_str());
  if (name.empty())
    return false;
  s.phase = Phase::Committed;
  outName = name;
  return true;
}

// Cancel, Esc, click outside, or the dialog being hidden by anyone else.
inline bool Cancel(State& s)
{
  if (!IsArmed(s))
    return false;
  s.phase = Phase::Cancelled;
  return true;
}

// Any text-entry completion, including the one a focus loss produces. It carries
// the text the user typed but no intent, so it never commits.
inline void ApplyTextEntryCompletion(State& s, const char* str)
{
  if (!IsArmed(s))
    return;
  s.draft = volum::custom::ClampName(str ? str : "", volum::custom::kMaxPresetNameLen);
  s.anchor = s.caret = s.draft.size();
}

// ---- editing --------------------------------------------------------------

inline bool HasSelection(const State& s)
{
  return s.anchor != s.caret;
}

inline std::size_t SelectionStart(const State& s)
{
  return std::min(s.anchor, s.caret);
}

inline std::size_t SelectionEnd(const State& s)
{
  return std::max(s.anchor, s.caret);
}

inline std::string SelectedText(const State& s)
{
  return s.draft.substr(SelectionStart(s), SelectionEnd(s) - SelectionStart(s));
}

inline std::size_t PrevBoundary(const std::string& text, std::size_t pos)
{
  if (pos == 0)
    return 0;
  --pos;
  while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0u) == 0x80u)
    --pos;
  return pos;
}

inline std::size_t NextBoundary(const std::string& text, std::size_t pos)
{
  if (pos >= text.size())
    return text.size();
  ++pos;
  while (pos < text.size() && (static_cast<unsigned char>(text[pos]) & 0xC0u) == 0x80u)
    ++pos;
  return pos;
}

inline void DeleteSelection(State& s)
{
  if (!HasSelection(s))
    return;
  const std::size_t start = SelectionStart(s);
  s.draft.erase(start, SelectionEnd(s) - start);
  s.anchor = s.caret = start;
}

// Keep only printable text: control characters and newlines from a paste would
// otherwise end up in a preset name the list cannot draw on one line.
inline std::string PrintableText(const std::string& utf8)
{
  std::string out;
  std::size_t i = 0;
  while (i < utf8.size())
  {
    const std::size_t len = volum::custom::Utf8ValidSequenceLength(utf8, i);
    if (len == 0)
    {
      ++i;
      continue;
    }
    const unsigned char lead = static_cast<unsigned char>(utf8[i]);
    if (len > 1 || (lead >= 0x20u && lead != 0x7Fu))
      out.append(utf8, i, len);
    i += len;
  }
  return out;
}

inline void InsertText(State& s, const std::string& utf8)
{
  if (!IsArmed(s))
    return;
  const std::string text = PrintableText(utf8);
  if (text.empty())
    return;
  DeleteSelection(s);
  const std::size_t room =
    volum::custom::kMaxPresetNameLen > s.draft.size() ? volum::custom::kMaxPresetNameLen - s.draft.size() : 0;
  const std::string fits = volum::custom::Utf8Prefix(text, room);
  s.draft.insert(s.caret, fits);
  s.caret += fits.size();
  s.anchor = s.caret;
}

inline void Backspace(State& s)
{
  if (HasSelection(s))
    return DeleteSelection(s);
  const std::size_t from = PrevBoundary(s.draft, s.caret);
  s.draft.erase(from, s.caret - from);
  s.anchor = s.caret = from;
}

inline void DeleteForward(State& s)
{
  if (HasSelection(s))
    return DeleteSelection(s);
  const std::size_t to = NextBoundary(s.draft, s.caret);
  s.draft.erase(s.caret, to - s.caret);
  s.anchor = s.caret;
}

inline void MoveCaretTo(State& s, std::size_t pos, bool extend)
{
  s.caret = std::min(pos, s.draft.size());
  if (!extend)
    s.anchor = s.caret;
}

inline void MoveLeft(State& s, bool extend)
{
  if (!extend && HasSelection(s))
    return MoveCaretTo(s, SelectionStart(s), false);
  MoveCaretTo(s, PrevBoundary(s.draft, s.caret), extend);
}

inline void MoveRight(State& s, bool extend)
{
  if (!extend && HasSelection(s))
    return MoveCaretTo(s, SelectionEnd(s), false);
  MoveCaretTo(s, NextBoundary(s.draft, s.caret), extend);
}

inline void SelectAll(State& s)
{
  s.anchor = 0;
  s.caret = s.draft.size();
}

// ---- keys -----------------------------------------------------------------

// iPlug kVK_* values (Windows VK codes on every platform). Numeric so this header
// stays free of IGraphics.
inline constexpr int kVkBack = 0x08;
inline constexpr int kVkReturn = 0x0D;
inline constexpr int kVkEscape = 0x1B;
inline constexpr int kVkEnd = 0x23;
inline constexpr int kVkHome = 0x24;
inline constexpr int kVkLeft = 0x25;
inline constexpr int kVkRight = 0x27;
inline constexpr int kVkDelete = 0x2E;

enum class KeyAction
{
  Ignored,
  Edited,
  Commit,
  Cancel,
  Copy,
  Cut,
  Paste,
};

// Windows hands over one ANSI byte per key; a lone byte >= 0xA0 is the Latin-1
// character it names (what iPlug's own field assumed too). macOS already sends
// UTF-8, which passes through untouched.
inline std::string KeyTextToUtf8(const char* utf8)
{
  const std::string raw = utf8 ? utf8 : "";
  if (raw.size() == 1)
  {
    const unsigned char c = static_cast<unsigned char>(raw[0]);
    if (c >= 0xA0u)
      return std::string{static_cast<char>(0xC0u | (c >> 6)), static_cast<char>(0x80u | (c & 0x3Fu))};
  }
  return raw;
}

// What one key press means to the dialog. Clipboard work is left to the view
// (Copy/Cut/Paste), everything else is applied to s here. Ctrl is Cmd on macOS.
// Ctrl+Alt is AltGr on Windows keyboards ('@', '{' on German layouts): that is
// text, not a shortcut.
inline KeyAction ApplyKey(State& s, int vk, const char* utf8, bool shift, bool ctrl, bool alt)
{
  if (!IsArmed(s))
    return KeyAction::Ignored;
  if (vk == kVkReturn)
    return KeyAction::Commit;
  if (vk == kVkEscape)
    return KeyAction::Cancel;

  if (ctrl && !alt)
  {
    int letter = (vk >= 'A' && vk <= 'Z') ? vk : 0;
    if (!letter && utf8 && utf8[0] && !utf8[1])
      letter = std::toupper(static_cast<unsigned char>(utf8[0]));
    switch (letter)
    {
      case 'A': SelectAll(s); return KeyAction::Edited;
      case 'C': return KeyAction::Copy;
      case 'X': return KeyAction::Cut;
      case 'V': return KeyAction::Paste;
      default: return KeyAction::Ignored;
    }
  }

  switch (vk)
  {
    case kVkBack: Backspace(s); return KeyAction::Edited;
    case kVkDelete: DeleteForward(s); return KeyAction::Edited;
    case kVkLeft: MoveLeft(s, shift); return KeyAction::Edited;
    case kVkRight: MoveRight(s, shift); return KeyAction::Edited;
    case kVkHome: MoveCaretTo(s, 0, shift); return KeyAction::Edited;
    case kVkEnd: MoveCaretTo(s, s.draft.size(), shift); return KeyAction::Edited;
    default: break;
  }

  const std::string text = PrintableText(KeyTextToUtf8(utf8));
  if (text.empty())
    return KeyAction::Ignored;
  InsertText(s, text);
  return KeyAction::Edited;
}

} // namespace volum::name_dialog
