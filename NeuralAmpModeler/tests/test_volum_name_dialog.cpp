#include "third_party/doctest.h"

#include "VoLumFactoryPresets.h"
#include "VoLumNameDialogModel.h"

#include <string>
#include <utility>

namespace nd = volum::name_dialog;

namespace
{
nd::State Opened(const std::string& seed, const std::string& overwriteName = "")
{
  nd::State s;
  nd::Open(s, seed, overwriteName);
  return s;
}

void Type(nd::State& s, const char* text)
{
  for (const char* p = text; *p; ++p)
  {
    const char ch[2] = {*p, 0};
    const int vk = (*p >= 'a' && *p <= 'z') ? *p - 'a' + 'A' : *p;
    nd::ApplyKey(s, vk, ch, *p >= 'A' && *p <= 'Z', false, false);
  }
}
} // namespace

TEST_CASE("Name dialog: the completion a Cancel click causes never commits")
{
  // Clicking Cancel takes focus from the text field; iPlug reports that as a
  // completion carrying the field's text, before the click itself is handled.
  // The 1.3.0 dialog committed on it, so Cancel saved "New Preset", "New Preset 2"...
  auto s = Opened("New Preset");
  nd::ApplyTextEntryCompletion(s, "New Preset");
  CHECK(s.phase == nd::Phase::Armed);
  CHECK(s.draft == "New Preset");

  CHECK(nd::Cancel(s));
  CHECK(s.phase == nd::Phase::Cancelled);
  std::string name;
  CHECK_FALSE(nd::Commit(s, name));
  CHECK(name.empty());

  // A late completion after the cancel changes nothing either.
  nd::ApplyTextEntryCompletion(s, "Late");
  CHECK(s.phase == nd::Phase::Cancelled);
  CHECK_FALSE(nd::Commit(s, name));
}

TEST_CASE("Name dialog: five cancelled rounds write nothing, the sixth saves once")
{
  int saves = 0;
  nd::State s;
  for (int round = 0; round < 5; ++round)
  {
    nd::Open(s, volum::kSaveDialogNewPresetSeed, "");
    nd::ApplyTextEntryCompletion(s, s.draft.c_str()); // focus loss
    CHECK(nd::Cancel(s));
    std::string name;
    saves += nd::Commit(s, name) ? 1 : 0;
  }
  CHECK(saves == 0);

  nd::Open(s, volum::kSaveDialogNewPresetSeed, "");
  std::string name;
  CHECK(nd::ApplyKey(s, nd::kVkReturn, "\r", false, false, false) == nd::KeyAction::Commit);
  CHECK(nd::Commit(s, name));
  CHECK(name == "New Preset");
  CHECK_FALSE(nd::Commit(s, name)); // Enter and the Save button cannot both fire
  CHECK_FALSE(nd::Cancel(s)); // nor can a hide after the commit cancel it
}

TEST_CASE("Name dialog: an empty or blank name keeps the dialog armed")
{
  auto s = Opened("   ");
  std::string name;
  CHECK_FALSE(nd::Commit(s, name));
  CHECK(nd::IsArmed(s));
  Type(s, "  Lead  ");
  CHECK(nd::Commit(s, name));
  CHECK(name == "Lead");
}

TEST_CASE("Name dialog: Closed is not Armed; Cancel and Commit need an Open")
{
  nd::State s;
  std::string name;
  CHECK_FALSE(nd::IsArmed(s));
  CHECK_FALSE(nd::Cancel(s));
  CHECK_FALSE(nd::Commit(s, name));
  CHECK(nd::ApplyKey(s, 'A', "a", false, false, false) == nd::KeyAction::Ignored);
  CHECK(s.draft.empty());
}

TEST_CASE("Name dialog label reads Update only while the typed name is the active User preset")
{
  auto s = Opened("Lead", "Lead");
  CHECK(nd::LabelFor(s) == nd::ConfirmLabel::Update);
  CHECK(std::string(nd::LabelText(nd::LabelFor(s))) == "Update");

  // Live, key by key: the seed is selected, so the first key replaces it.
  Type(s, "L");
  CHECK(s.draft == "L");
  CHECK(nd::LabelFor(s) == nd::ConfirmLabel::Save);
  Type(s, "ead");
  CHECK(nd::LabelFor(s) == nd::ConfirmLabel::Update);
  Type(s, "2");
  CHECK(std::string(nd::LabelText(nd::LabelFor(s))) == "Save");
  nd::ApplyKey(s, nd::kVkBack, "\b", false, false, false);
  CHECK(nd::LabelFor(s) == nd::ConfirmLabel::Update);

  // Trailing space is trimmed on save, so it is still the same preset.
  Type(s, " ");
  CHECK(nd::LabelFor(s) == nd::ConfirmLabel::Update);

  // Names are exact: a different case is a different (new) preset.
  auto lower = Opened("lead", "Lead");
  CHECK(nd::LabelFor(lower) == nd::ConfirmLabel::Save);

  // Default / Factory: nothing to update, whatever is typed.
  auto fresh = Opened("New Preset", "");
  CHECK(nd::LabelFor(fresh) == nd::ConfirmLabel::Save);
}

TEST_CASE("Name dialog overwrite rule agrees with the preset save rule")
{
  for (const char* typed : {"Lead", "Lead2", "lead", "", "New Preset"})
    for (const char* current : {"Lead", "", "New Preset"})
    {
      INFO(typed << " vs " << current);
      CHECK(nd::Overwrites(typed, current) == volum::SaveDialogOverwritesCurrent(typed, current));
    }
}

TEST_CASE("Name dialog keys: Enter commits, Esc cancels, hotkey letters are text")
{
  auto s = Opened("");
  CHECK(nd::ApplyKey(s, nd::kVkEscape, "\x1b", false, false, false) == nd::KeyAction::Cancel);
  CHECK(nd::ApplyKey(s, nd::kVkReturn, "\r", false, false, false) == nd::KeyAction::Commit);
  // H / T / M open Settings, the tuner and the metronome elsewhere; here they spell.
  Type(s, "htm 12");
  CHECK(s.draft == "htm 12");
  // Tab and arrows at the edges do not insert anything.
  CHECK(nd::ApplyKey(s, 0x09, "\t", false, false, false) == nd::KeyAction::Ignored);
  CHECK(s.draft == "htm 12");
}

TEST_CASE("Name dialog shortcuts: Ctrl+A selects, C/X/V go to the clipboard, AltGr is text")
{
  auto s = Opened("Crunch");
  nd::MoveCaretTo(s, 0, false);
  CHECK(nd::ApplyKey(s, 'A', "\x01", false, true, false) == nd::KeyAction::Edited);
  CHECK(nd::SelectedText(s) == "Crunch");
  CHECK(nd::ApplyKey(s, 'C', "\x03", false, true, false) == nd::KeyAction::Copy);
  CHECK(nd::ApplyKey(s, 'X', "\x18", false, true, false) == nd::KeyAction::Cut);
  CHECK(nd::ApplyKey(s, 'V', "\x16", false, true, false) == nd::KeyAction::Paste);
  // macOS sends Cmd+letter with no VK, only the character.
  CHECK(nd::ApplyKey(s, 0, "v", false, true, false) == nd::KeyAction::Paste);
  // Ctrl+S (the save shortcut) must not type an 's' into the name.
  CHECK(nd::ApplyKey(s, 'S', "\x13", false, true, false) == nd::KeyAction::Ignored);
  CHECK(s.draft == "Crunch");

  // AltGr arrives as Ctrl+Alt on Windows; '@' is text, not a shortcut.
  nd::MoveCaretTo(s, s.draft.size(), false);
  CHECK(nd::ApplyKey(s, 'Q', "@", false, true, true) == nd::KeyAction::Edited);
  CHECK(s.draft == "Crunch@");
}

TEST_CASE("Name dialog editing: selection, caret moves, Delete and Backspace")
{
  auto s = Opened("Clean");
  CHECK(nd::HasSelection(s)); // the seed is selected
  nd::ApplyKey(s, nd::kVkLeft, "", false, false, false);
  CHECK_FALSE(nd::HasSelection(s));
  CHECK(s.caret == 0);
  nd::ApplyKey(s, nd::kVkRight, "", true, false, false);
  nd::ApplyKey(s, nd::kVkRight, "", true, false, false);
  CHECK(nd::SelectedText(s) == "Cl");
  Type(s, "B");
  CHECK(s.draft == "Bean");
  nd::ApplyKey(s, nd::kVkEnd, "", false, false, false);
  nd::ApplyKey(s, nd::kVkBack, "\b", false, false, false);
  CHECK(s.draft == "Bea");
  nd::ApplyKey(s, nd::kVkHome, "", false, false, false);
  nd::ApplyKey(s, nd::kVkDelete, "", false, false, false);
  CHECK(s.draft == "ea");
  nd::ApplyKey(s, nd::kVkHome, "", false, false, false);
  nd::ApplyKey(s, nd::kVkBack, "\b", false, false, false); // nothing before the caret
  CHECK(s.draft == "ea");
}

TEST_CASE("Name dialog text is UTF-8 safe, printable, and capped at the preset name length")
{
  auto s = Opened("");
  // Windows hands a Latin-1 byte for keys like the German umlauts.
  const char latin1[2] = {static_cast<char>(0xE4), 0};
  CHECK(nd::KeyTextToUtf8(latin1) == "\xC3\xA4");
  nd::ApplyKey(s, 0xDE, latin1, false, false, false);
  CHECK(s.draft == "\xC3\xA4");
  nd::ApplyKey(s, nd::kVkBack, "\b", false, false, false);
  CHECK(s.draft.empty()); // both bytes of the glyph, never half of it

  nd::InsertText(s, "Line\none\x7f");
  CHECK(s.draft == "Lineone");

  nd::SelectAll(s);
  nd::InsertText(s, std::string(40, 'x'));
  CHECK(s.draft.size() == volum::custom::kMaxPresetNameLen);

  // A multibyte glyph that does not fit is dropped whole.
  nd::SelectAll(s);
  nd::InsertText(s, std::string(volum::custom::kMaxPresetNameLen - 1, 'y') + "\xC3\xA4");
  CHECK(s.draft == std::string(volum::custom::kMaxPresetNameLen - 1, 'y'));

  // Seeds are clamped the same way the registry clamps names.
  auto longSeed = Opened(std::string(40, 'z'));
  CHECK(longSeed.draft.size() == volum::custom::kMaxPresetNameLen);
}

namespace
{
nd::KeyAction Ctrl(nd::State& s, int vk, bool shift = false)
{
  // Windows turns Ctrl+Backspace into DEL (0x7F) and Ctrl+letters into control
  // characters; arrows and Delete carry no text.
  const char* text = vk == nd::kVkBack ? "\x7f" : "";
  return nd::ApplyKey(s, vk, text, shift, true, false);
}

nd::State AtEnd(const std::string& text)
{
  auto s = Opened(text);
  nd::MoveCaretTo(s, s.draft.size(), false);
  return s;
}
} // namespace

TEST_CASE("Name dialog word boundaries: spaces and punctuation split words, UTF-8 stays whole")
{
  const std::string t = "Lead-Tone  v2";
  CHECK(nd::PrevWordStart(t, t.size()) == 11); // "v2"
  CHECK(nd::PrevWordStart(t, 11) == 5); // over the spaces, then "Tone"
  CHECK(nd::PrevWordStart(t, 5) == 4); // the dash is its own run
  CHECK(nd::PrevWordStart(t, 4) == 0);
  CHECK(nd::PrevWordStart(t, 0) == 0);
  CHECK(nd::NextWordStart(t, 0) == 4);
  CHECK(nd::NextWordStart(t, 4) == 5);
  CHECK(nd::NextWordStart(t, 5) == 11); // "Tone" plus the spaces after it
  CHECK(nd::NextWordStart(t, 11) == t.size());

  // A multibyte glyph is part of its word; no boundary lands inside it.
  const std::string umlaut = "Gr\xC3\xBCn Amp";
  CHECK(nd::PrevWordStart(umlaut, 5) == 0);
  CHECK(nd::NextWordStart(umlaut, 0) == 6);
  CHECK(nd::WordRangeAt(umlaut, 3) == std::pair<std::size_t, std::size_t>(0, 5));
}

TEST_CASE("Name dialog Ctrl+Backspace deletes the previous word")
{
  auto s = AtEnd("Crunch Rhythm 2");
  CHECK(Ctrl(s, nd::kVkBack) == nd::KeyAction::Edited);
  CHECK(s.draft == "Crunch Rhythm ");
  Ctrl(s, nd::kVkBack);
  CHECK(s.draft == "Crunch ");
  Ctrl(s, nd::kVkBack);
  CHECK(s.draft.empty());
  CHECK(s.caret == 0);

  // A selection goes first, like a plain Backspace.
  auto sel = Opened("Clean Verb");
  Ctrl(sel, nd::kVkBack);
  CHECK(sel.draft.empty());
}

TEST_CASE("Name dialog Ctrl+Delete deletes the next word and the spaces after it")
{
  auto s = Opened("Crunch Rhythm 2");
  nd::MoveCaretTo(s, 0, false);
  CHECK(Ctrl(s, nd::kVkDelete) == nd::KeyAction::Edited);
  CHECK(s.draft == "Rhythm 2");
  CHECK(s.caret == 0);
  nd::MoveCaretTo(s, 3, false); // inside a word: to its end and the spaces
  Ctrl(s, nd::kVkDelete);
  CHECK(s.draft == "Rhy2");
}

TEST_CASE("Name dialog Ctrl+Left / Ctrl+Right jump by word, Shift extends")
{
  auto s = AtEnd("Lead Boost Hot");
  Ctrl(s, nd::kVkLeft);
  CHECK(s.caret == 11);
  CHECK_FALSE(nd::HasSelection(s));
  Ctrl(s, nd::kVkLeft, true);
  CHECK(nd::SelectedText(s) == "Boost ");
  Ctrl(s, nd::kVkRight, true);
  CHECK_FALSE(nd::HasSelection(s)); // back where the selection started
  Ctrl(s, nd::kVkHome);
  CHECK(s.caret == 0);
  Ctrl(s, nd::kVkRight);
  CHECK(s.caret == 5);
  Ctrl(s, nd::kVkRight, true);
  Ctrl(s, nd::kVkRight, true);
  CHECK(nd::SelectedText(s) == "Boost Hot");
}

TEST_CASE("Name dialog Ctrl+Z undoes, typing runs undo as one step, Ctrl+Y and Ctrl+Shift+Z redo")
{
  auto s = Opened("Clean");
  Type(s, "Lead"); // replaces the selected seed: one step
  Type(s, " two"); // continues the run
  CHECK(s.draft == "Lead two");
  CHECK(Ctrl(s, 'Z') == nd::KeyAction::Edited);
  CHECK(s.draft == "Clean");
  CHECK(nd::SelectedText(s) == "Clean"); // selection restored with it
  CHECK(Ctrl(s, 'Z') == nd::KeyAction::Ignored); // nothing older
  CHECK(Ctrl(s, 'Y') == nd::KeyAction::Edited);
  CHECK(s.draft == "Lead two");

  // A caret move ends the run; held Backspace is one step of its own.
  nd::ApplyKey(s, nd::kVkLeft, "", false, false, false);
  nd::ApplyKey(s, nd::kVkRight, "", false, false, false);
  Type(s, "!");
  nd::ApplyKey(s, nd::kVkBack, "\b", false, false, false);
  nd::ApplyKey(s, nd::kVkBack, "\b", false, false, false);
  nd::ApplyKey(s, nd::kVkBack, "\b", false, false, false);
  CHECK(s.draft == "Lead t");
  Ctrl(s, 'Z');
  CHECK(s.draft == "Lead two!");
  Ctrl(s, 'Z');
  CHECK(s.draft == "Lead two");
  CHECK(Ctrl(s, 'Z', true) == nd::KeyAction::Edited); // Ctrl+Shift+Z
  CHECK(s.draft == "Lead two!");

  // A new edit clears the redo branch; paste and word delete are steps of their own.
  Type(s, "?");
  CHECK(Ctrl(s, 'Y') == nd::KeyAction::Ignored);
  nd::PasteText(s, "xx");
  Ctrl(s, nd::kVkBack); // "xx" is the last word; the "?" before it is punctuation
  CHECK(s.draft == "Lead two!?");
  Ctrl(s, 'Z');
  CHECK(s.draft == "Lead two!?xx");
  Ctrl(s, 'Z');
  CHECK(s.draft == "Lead two!?");

  // Reopening starts a fresh history.
  nd::Open(s, "Other", "");
  CHECK(Ctrl(s, 'Z') == nd::KeyAction::Ignored);
}

TEST_CASE("Name dialog cut is undoable")
{
  auto s = Opened("Crunch");
  nd::CutSelection(s);
  CHECK(s.draft.empty());
  Ctrl(s, 'Z');
  CHECK(s.draft == "Crunch");
}

TEST_CASE("Name dialog clicks: one places the caret, two select a word, three select all")
{
  nd::ClickTracker t;
  CHECK(nd::RegisterClick(t, 1000.0, 10.f, 10.f, false) == 1);
  CHECK(nd::RegisterClick(t, 1200.0, 11.f, 10.f, true) == 2);
  CHECK(nd::RegisterClick(t, 1400.0, 11.f, 11.f, false) == 3);
  CHECK(nd::RegisterClick(t, 1500.0, 11.f, 11.f, false) == 3);
  CHECK(nd::RegisterClick(t, 2600.0, 11.f, 11.f, false) == 1); // too late
  CHECK(nd::RegisterClick(t, 2700.0, 40.f, 11.f, false) == 1); // moved away
  CHECK(nd::RegisterClick(t, 2750.0, 40.f, 11.f, true) == 2);

  auto s = Opened("Crunch Rhythm");
  nd::PointerDown(s, 9, 1, false);
  CHECK(s.caret == 9);
  CHECK_FALSE(nd::HasSelection(s));
  nd::PointerDown(s, 9, 2, false);
  CHECK(nd::SelectedText(s) == "Rhythm");
  nd::PointerDown(s, 2, 2, false);
  CHECK(nd::SelectedText(s) == "Crunch");
  nd::PointerDown(s, 9, 3, false);
  CHECK(nd::SelectedText(s) == "Crunch Rhythm");
  nd::PointerDown(s, 3, 1, false);
  nd::PointerDown(s, 9, 1, true); // Shift+click extends
  CHECK(nd::SelectedText(s) == "nch Rh");
}

TEST_CASE("Name dialog drag selects by character, or by word after a double-click")
{
  auto s = Opened("Lead Boost Hot");
  nd::PointerDown(s, 2, 1, false);
  nd::PointerDrag(s, 7);
  CHECK(nd::SelectedText(s) == "ad Bo");
  nd::PointerDrag(s, 0); // back past the anchor
  CHECK(nd::SelectedText(s) == "Le");

  nd::PointerDown(s, 6, 2, false); // "Boost"
  nd::PointerDrag(s, 12);
  CHECK(nd::SelectedText(s) == "Boost Hot");
  nd::PointerDrag(s, 1);
  CHECK(nd::SelectedText(s) == "Lead Boost");

  nd::PointerDown(s, 6, 3, false);
  nd::PointerDrag(s, 1);
  CHECK(nd::SelectedText(s) == "Lead Boost Hot");
}

TEST_CASE("Name dialog caret blinks at the Windows rate and shows on every key")
{
  CHECK(nd::CaretVisible(0.0));
  CHECK(nd::CaretVisible(nd::kCaretBlinkMs - 1.0));
  CHECK_FALSE(nd::CaretVisible(nd::kCaretBlinkMs + 1.0));
  CHECK(nd::CaretVisible(2.0 * nd::kCaretBlinkMs + 1.0));
  CHECK(nd::CaretBlinkPhase(nd::kCaretBlinkMs * 3.5) == 3);
  CHECK(nd::kCaretBlinkMs == doctest::Approx(530.0));
}

TEST_CASE("Name dialog cap follows the item being named")
{
  nd::State s;
  nd::Open(s, "V30 Cab", "", 3);
  CHECK(s.draft == "V30");
  nd::MoveCaretTo(s, s.draft.size(), false);
  Type(s, "x");
  CHECK(s.draft == "V30");
  nd::Open(s, std::string(40, 'a'), "", volum::custom::kMaxCustomNameLen);
  CHECK(s.draft.size() == volum::custom::kMaxCustomNameLen);
}
