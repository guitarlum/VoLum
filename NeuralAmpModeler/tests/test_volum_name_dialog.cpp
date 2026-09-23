#include "third_party/doctest.h"

#include "VoLumFactoryPresets.h"
#include "VoLumNameDialogModel.h"

#include <string>

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
