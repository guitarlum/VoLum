#pragma once

// Pure state machine behind VoLumNameDialogControl, the one field VoLum asks for a
// name with (Save preset, PLAY "Add this sound", Manage new / rename, the amp
// builder's name and cab labels). The control is a thin view: it forwards clicks
// and keys here and draws what this reports.
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
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

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

struct Snapshot
{
  std::string draft;
  std::size_t caret = 0;
  std::size_t anchor = 0;
};

// Which kind of edit last changed the draft. Consecutive edits of the same kind
// (a run of typed characters, held Backspace, held Delete) undo as one step, the
// way a Windows edit box does; a caret move or any other edit ends the run.
enum class EditKind
{
  None,
  Typing,
  Backspace,
  DeleteForward,
  Other,
};

// What a mouse drag extends by: characters after a single click, whole words after
// a double-click, nothing after a triple-click (all is already selected).
enum class DragUnit
{
  Char,
  Word,
  All,
};

inline constexpr std::size_t kMaxUndo = 100;

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
  // Byte cap of the name being asked for (presets and custom items differ).
  std::size_t maxLen = volum::custom::kMaxPresetNameLen;
  std::vector<Snapshot> undo;
  std::vector<Snapshot> redo;
  EditKind lastEdit = EditKind::None;
  DragUnit dragUnit = DragUnit::Char;
  std::size_t dragWordStart = 0;
  std::size_t dragWordEnd = 0;
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

inline void Open(State& s, const std::string& seed, const std::string& overwriteName,
                 std::size_t maxLen = volum::custom::kMaxPresetNameLen)
{
  s.phase = Phase::Armed;
  s.maxLen = std::min(maxLen, volum::custom::kMaxPresetNameLen);
  s.draft = volum::custom::ClampName(seed, s.maxLen);
  s.overwriteName = overwriteName;
  // Seed fully selected, so typing replaces it and Enter keeps it.
  s.anchor = 0;
  s.caret = s.draft.size();
  s.undo.clear();
  s.redo.clear();
  s.lastEdit = EditKind::None;
  s.dragUnit = DragUnit::Char;
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
  s.draft = volum::custom::ClampName(str ? str : "", s.maxLen);
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

// ---- words ----------------------------------------------------------------

enum class CharClass
{
  Space,
  Punct,
  Word,
};

// Spaces and ASCII punctuation separate words. Every byte of a multi-byte UTF-8
// character is a word byte, so a class boundary never falls inside a character.
inline CharClass ClassAt(const std::string& text, std::size_t pos)
{
  const unsigned char c = static_cast<unsigned char>(text[pos]);
  if (c >= 0x80u || std::isalnum(c) || c == '_')
    return CharClass::Word;
  if (std::isspace(c))
    return CharClass::Space;
  return CharClass::Punct;
}

// Ctrl+Left / Ctrl+Backspace: back over spaces, then over one run of word or
// punctuation characters.
inline std::size_t PrevWordStart(const std::string& text, std::size_t pos)
{
  pos = std::min(pos, text.size());
  while (pos > 0 && ClassAt(text, pos - 1) == CharClass::Space)
    --pos;
  if (pos == 0)
    return 0;
  const CharClass run = ClassAt(text, pos - 1);
  while (pos > 0 && ClassAt(text, pos - 1) == run)
    --pos;
  return pos;
}

// Ctrl+Right / Ctrl+Delete, as in a Windows edit box: over the current run, then
// over the spaces after it, landing on the start of the next word.
inline std::size_t NextWordStart(const std::string& text, std::size_t pos)
{
  const std::size_t n = text.size();
  pos = std::min(pos, n);
  if (pos < n && ClassAt(text, pos) != CharClass::Space)
  {
    const CharClass run = ClassAt(text, pos);
    while (pos < n && ClassAt(text, pos) == run)
      ++pos;
  }
  while (pos < n && ClassAt(text, pos) == CharClass::Space)
    ++pos;
  return pos;
}

// The run a double-click at pos selects: the word, the punctuation or the spaces
// under the pointer. At the very end it is the run before the caret.
inline std::pair<std::size_t, std::size_t> WordRangeAt(const std::string& text, std::size_t pos)
{
  if (text.empty())
    return {0, 0};
  pos = std::min(pos, text.size());
  const std::size_t probe = pos == text.size() ? pos - 1 : pos;
  const CharClass run = ClassAt(text, probe);
  std::size_t start = probe, end = probe;
  while (start > 0 && ClassAt(text, start - 1) == run)
    --start;
  while (end < text.size() && ClassAt(text, end) == run)
    ++end;
  return {start, end};
}

// ---- undo -----------------------------------------------------------------

inline Snapshot Snap(const State& s)
{
  return {s.draft, s.caret, s.anchor};
}

inline void Restore(State& s, const Snapshot& snap)
{
  s.draft = snap.draft;
  s.caret = snap.caret;
  s.anchor = snap.anchor;
}

// Runs apply, then records the state before it as one undo step unless nothing
// changed or it continues a run of the same kind that started from a plain caret.
template <typename Apply>
inline void Edit(State& s, EditKind kind, Apply&& apply)
{
  const Snapshot before = Snap(s);
  apply();
  if (s.draft == before.draft)
    return;
  const bool continuesRun =
    kind != EditKind::Other && kind == s.lastEdit && before.caret == before.anchor && !s.undo.empty();
  if (!continuesRun)
  {
    s.undo.push_back(before);
    if (s.undo.size() > kMaxUndo)
      s.undo.erase(s.undo.begin());
  }
  s.redo.clear();
  s.lastEdit = kind;
}

inline bool Undo(State& s)
{
  if (s.undo.empty())
    return false;
  s.redo.push_back(Snap(s));
  Restore(s, s.undo.back());
  s.undo.pop_back();
  s.lastEdit = EditKind::None;
  return true;
}

inline bool Redo(State& s)
{
  if (s.redo.empty())
    return false;
  s.undo.push_back(Snap(s));
  Restore(s, s.redo.back());
  s.redo.pop_back();
  s.lastEdit = EditKind::None;
  return true;
}

// ---- edits ----------------------------------------------------------------

inline void EraseRange(State& s, std::size_t from, std::size_t to)
{
  s.draft.erase(from, to - from);
  s.anchor = s.caret = from;
}

inline void InsertTextAs(State& s, const std::string& utf8, EditKind kind)
{
  if (!IsArmed(s))
    return;
  const std::string text = PrintableText(utf8);
  if (text.empty())
    return;
  Edit(s, kind, [&] {
    DeleteSelection(s);
    const std::size_t room = s.maxLen > s.draft.size() ? s.maxLen - s.draft.size() : 0;
    const std::string fits = volum::custom::Utf8Prefix(text, room);
    s.draft.insert(s.caret, fits);
    s.caret += fits.size();
    s.anchor = s.caret;
  });
}

inline void InsertText(State& s, const std::string& utf8)
{
  InsertTextAs(s, utf8, EditKind::Typing);
}

// A paste is its own undo step, never merged into the typing around it.
inline void PasteText(State& s, const std::string& utf8)
{
  InsertTextAs(s, utf8, EditKind::Other);
}

inline void CutSelection(State& s)
{
  Edit(s, EditKind::Other, [&] { DeleteSelection(s); });
}

inline void Backspace(State& s)
{
  if (HasSelection(s))
    return Edit(s, EditKind::Other, [&] { DeleteSelection(s); });
  Edit(s, EditKind::Backspace, [&] { EraseRange(s, PrevBoundary(s.draft, s.caret), s.caret); });
}

inline void DeleteForward(State& s)
{
  if (HasSelection(s))
    return Edit(s, EditKind::Other, [&] { DeleteSelection(s); });
  Edit(s, EditKind::DeleteForward, [&] {
    const std::size_t at = s.caret;
    EraseRange(s, at, NextBoundary(s.draft, at));
  });
}

// Ctrl+Backspace. A selection goes first, as a plain Backspace would take it.
inline void DeleteWordBack(State& s)
{
  if (HasSelection(s))
    return Edit(s, EditKind::Other, [&] { DeleteSelection(s); });
  Edit(s, EditKind::Other, [&] { EraseRange(s, PrevWordStart(s.draft, s.caret), s.caret); });
}

// Ctrl+Delete.
inline void DeleteWordForward(State& s)
{
  if (HasSelection(s))
    return Edit(s, EditKind::Other, [&] { DeleteSelection(s); });
  Edit(s, EditKind::Other, [&] {
    const std::size_t at = s.caret;
    EraseRange(s, at, NextWordStart(s.draft, at));
  });
}

// ---- caret and selection --------------------------------------------------

inline void MoveCaretTo(State& s, std::size_t pos, bool extend)
{
  s.caret = std::min(pos, s.draft.size());
  if (!extend)
    s.anchor = s.caret;
  s.lastEdit = EditKind::None;
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

inline void MoveWordLeft(State& s, bool extend)
{
  MoveCaretTo(s, PrevWordStart(s.draft, s.caret), extend);
}

inline void MoveWordRight(State& s, bool extend)
{
  MoveCaretTo(s, NextWordStart(s.draft, s.caret), extend);
}

inline void SelectAll(State& s)
{
  s.anchor = 0;
  s.caret = s.draft.size();
  s.lastEdit = EditKind::None;
}

// ---- mouse ----------------------------------------------------------------

inline constexpr double kMultiClickMs = 500.0; // Windows' default double-click time
inline constexpr float kMultiClickSlop = 4.f;

struct ClickTracker
{
  double lastMs = -1.0e9;
  float x = 0.f;
  float y = 0.f;
  int count = 0;
};

// 1, 2 or 3 for a single, double or triple click. osDoubleClick is the platform's
// own double-click event, which is a second click whatever the timing says.
inline int RegisterClick(ClickTracker& t, double nowMs, float x, float y, bool osDoubleClick)
{
  const bool chained =
    nowMs - t.lastMs <= kMultiClickMs && std::fabs(x - t.x) <= kMultiClickSlop && std::fabs(y - t.y) <= kMultiClickSlop;
  int count = chained ? t.count + 1 : 1;
  if (osDoubleClick)
    count = std::max(count, 2);
  count = std::min(count, 3);
  t.lastMs = nowMs;
  t.x = x;
  t.y = y;
  t.count = count;
  return count;
}

// Press inside the field at text offset pos: place the caret (Shift extends), or
// select the word under it (double) or everything (triple).
inline void PointerDown(State& s, std::size_t pos, int clickCount, bool shift)
{
  if (clickCount >= 3)
  {
    SelectAll(s);
    s.dragUnit = DragUnit::All;
    return;
  }
  if (clickCount == 2)
  {
    const auto word = WordRangeAt(s.draft, pos);
    s.dragWordStart = word.first;
    s.dragWordEnd = word.second;
    s.anchor = word.first;
    MoveCaretTo(s, word.second, true);
    s.dragUnit = DragUnit::Word;
    return;
  }
  MoveCaretTo(s, pos, shift);
  s.dragUnit = DragUnit::Char;
}

// Drag with the button held after PointerDown.
inline void PointerDrag(State& s, std::size_t pos)
{
  switch (s.dragUnit)
  {
    case DragUnit::All: return;
    case DragUnit::Char: MoveCaretTo(s, pos, true); return;
    case DragUnit::Word:
    {
      const auto word = WordRangeAt(s.draft, pos);
      if (pos < s.dragWordStart)
      {
        s.anchor = s.dragWordEnd;
        MoveCaretTo(s, word.first, true);
      }
      else
      {
        s.anchor = s.dragWordStart;
        MoveCaretTo(s, std::max(word.second, s.dragWordEnd), true);
      }
      return;
    }
  }
}

// ---- caret blink ----------------------------------------------------------

inline constexpr double kCaretBlinkMs = 530.0; // Windows' default caret blink time

// The caret shows for the first half of each blink period after the last key or
// click, so it is always visible while typing.
inline int CaretBlinkPhase(double msSinceInput)
{
  return msSinceInput <= 0.0 ? 0 : static_cast<int>(msSinceInput / kCaretBlinkMs);
}

inline bool CaretVisible(double msSinceInput)
{
  return CaretBlinkPhase(msSinceInput) % 2 == 0;
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
    switch (vk)
    {
      case kVkBack: DeleteWordBack(s); return KeyAction::Edited;
      case kVkDelete: DeleteWordForward(s); return KeyAction::Edited;
      case kVkLeft: MoveWordLeft(s, shift); return KeyAction::Edited;
      case kVkRight: MoveWordRight(s, shift); return KeyAction::Edited;
      case kVkHome: MoveCaretTo(s, 0, shift); return KeyAction::Edited;
      case kVkEnd: MoveCaretTo(s, s.draft.size(), shift); return KeyAction::Edited;
      default: break;
    }
    int letter = (vk >= 'A' && vk <= 'Z') ? vk : 0;
    if (!letter && utf8 && utf8[0] && !utf8[1])
      letter = std::toupper(static_cast<unsigned char>(utf8[0]));
    switch (letter)
    {
      case 'A': SelectAll(s); return KeyAction::Edited;
      case 'C': return KeyAction::Copy;
      case 'X': return KeyAction::Cut;
      case 'V': return KeyAction::Paste;
      case 'Z': return (shift ? Redo(s) : Undo(s)) ? KeyAction::Edited : KeyAction::Ignored;
      case 'Y': return Redo(s) ? KeyAction::Edited : KeyAction::Ignored;
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
