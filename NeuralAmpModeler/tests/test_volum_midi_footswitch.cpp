#include "third_party/doctest.h"

#include "../VoLumMidiFootswitchModel.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace volum::footswitch;

namespace
{
std::filesystem::path FootswitchRepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string FootswitchReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::array<bool, kSwitchesPerBank> NoneOccupied()
{
  return {};
}
} // namespace

TEST_CASE("Footswitch: sixteen banks of eight cover every program number exactly once")
{
  CHECK(kBankCount == 16);
  CHECK(kSwitchesPerBank == 8);
  std::set<int> seen;
  for (int bank = 0; bank < kBankCount; ++bank)
    for (int sw = 0; sw < kSwitchesPerBank; ++sw)
    {
      const int program = ProgramFor(bank, sw);
      CHECK(program == bank * 8 + sw);
      CHECK(BankOf(program) == bank);
      CHECK(SwitchOf(program) == sw);
      seen.insert(program);
    }
  CHECK(seen.size() == static_cast<size_t>(volum::kMidiSoundSlotCount));
  CHECK(*seen.begin() == 0);
  CHECK(*seen.rbegin() == 127);

  CHECK(ProgramFor(0, 0) == 0);
  CHECK(ProgramFor(1, 0) == 8);
  CHECK(ProgramFor(15, 7) == 127);
}

TEST_CASE("Footswitch: out-of-range banks, switches and programs never wrap onto a real number")
{
  CHECK(ProgramFor(-1, 0) == -1);
  CHECK(ProgramFor(16, 0) == -1);
  CHECK(ProgramFor(0, -1) == -1);
  CHECK(ProgramFor(0, 8) == -1);
  CHECK(BankOf(-1) == -1);
  CHECK(BankOf(128) == -1);
  CHECK(SwitchOf(-1) == -1);
  CHECK(SwitchOf(128) == -1);
  CHECK(IsProgram(0));
  CHECK(IsProgram(127));
  CHECK_FALSE(IsProgram(128));
}

TEST_CASE("Footswitch: the view opens on the bank holding the live program")
{
  CHECK(OpeningBank(-1) == 0); // nothing recalled yet
  CHECK(OpeningBank(0) == 0);
  CHECK(OpeningBank(7) == 0);
  CHECK(OpeningBank(8) == 1);
  CHECK(OpeningBank(9) == 1);
  CHECK(OpeningBank(42) == 5);
  CHECK(OpeningBank(127) == 15);
  CHECK(OpeningBank(500) == 0); // a stale out-of-range number is "nothing"
}

TEST_CASE("Footswitch: arrows and wheel page one bank and stop at the ends")
{
  CHECK(StepBank(0, 1) == 1);
  CHECK(StepBank(1, -1) == 0);
  CHECK(StepBank(0, -1) == 0);
  CHECK(StepBank(15, 1) == 15);
  CHECK(StepBank(4, 0) == 4);
  CHECK(StepBank(4, 5) == 5); // one bank per step, whatever the magnitude
  CHECK(ClampBank(-3) == 0);
  CHECK(ClampBank(99) == 15);

  float accum = 0.f;
  CHECK(WheelBankStep(accum, 1.f) == -1); // wheel up pages back
  CHECK(WheelBankStep(accum, -1.f) == 1); // wheel down pages forward
  // Trackpad fractions build up to one notch before paging.
  CHECK(WheelBankStep(accum, -0.4f) == 0);
  CHECK(WheelBankStep(accum, -0.4f) == 0);
  CHECK(WheelBankStep(accum, -0.4f) == 1);
  CHECK(accum == doctest::Approx(0.f));
  CHECK(WheelBankStep(accum, std::nanf("")) == 0);
  CHECK(accum == doctest::Approx(0.f));
}

TEST_CASE("Footswitch: bank pips count assigned programs per bank")
{
  const auto counts = BankOccupancy({0, 1, 4, 9, 127, 200, -1});
  CHECK(counts[0] == 3);
  CHECK(counts[1] == 1);
  CHECK(counts[15] == 1);
  CHECK(counts[2] == 0);
}

TEST_CASE("Footswitch: a drop swaps onto a taken switch, moves onto an empty one, else does nothing")
{
  CHECK(DecideDrop(3, 5, true, true) == DropAction::Swap);
  CHECK(DecideDrop(3, 5, true, false) == DropAction::Move);
  CHECK(DecideDrop(3, 3, true, true) == DropAction::None); // dropped back where it came from
  CHECK(DecideDrop(3, 5, false, true) == DropAction::None); // nothing was picked up
  CHECK(DecideDrop(3, -1, true, false) == DropAction::None); // dropped off the board
  CHECK(DecideDrop(3, 128, true, false) == DropAction::None);
  CHECK(DecideDrop(-1, 5, true, true) == DropAction::None);
  // Across banks: switch 1 of bank 0 onto switch 6 of bank 3.
  CHECK(DecideDrop(ProgramFor(0, 1), ProgramFor(3, 6), true, false) == DropAction::Move);
}

TEST_CASE("Footswitch: the default MIDI card lays out two rows of four inside its body")
{
  // The Settings MIDI card body at the 900x600 canvas.
  const float l = 72.f, t = 342.f, r = 828.f, b = 534.f;
  const auto layout = LayoutFootswitch(l, t, r, b);

  for (int i = 0; i < kSwitchesPerBank; ++i)
  {
    const Box& tile = layout.tiles[static_cast<size_t>(i)];
    INFO("switch " << i);
    CHECK(tile.L >= l - 0.01f);
    CHECK(tile.R <= r + 0.01f);
    CHECK(tile.T >= layout.header.B);
    CHECK(tile.B <= b + 0.01f);
    // Readable at the default size: room for mini art, the LED and a name.
    CHECK(tile.W() >= 170.f);
    CHECK(tile.H() >= 70.f);
    // Same size everywhere, aligned in rows and columns.
    CHECK(tile.W() == doctest::Approx(layout.tiles[0].W()));
    CHECK(tile.H() == doctest::Approx(layout.tiles[0].H()));
    CHECK(tile.L == doctest::Approx(layout.tiles[static_cast<size_t>(i % kColumns)].L));
    CHECK(tile.T == doctest::Approx(layout.tiles[static_cast<size_t>((i / kColumns) * kColumns)].T));
  }
  // Switch order reads left to right, then the second row.
  CHECK(layout.tiles[1].L > layout.tiles[0].R);
  CHECK(layout.tiles[4].T > layout.tiles[0].B);
  CHECK(layout.tiles[4].L == doctest::Approx(layout.tiles[0].L));
  CHECK(layout.tiles[3].R == doctest::Approx(r));
  CHECK(layout.tiles[7].B == doctest::Approx(b));

  // Header pieces sit side by side, left to right, without overlapping.
  CHECK(layout.prev.R <= layout.readout.L);
  CHECK(layout.readout.R <= layout.next.L);
  CHECK(layout.next.R <= layout.range.L);
  CHECK(layout.range.R <= layout.pips[0].L + 0.01f);
  for (int i = 1; i < kBankCount; ++i)
    CHECK(layout.pips[static_cast<size_t>(i - 1)].R <= layout.pips[static_cast<size_t>(i)].L + 0.01f);
  CHECK(layout.pips[kBankCount - 1].R <= layout.hint.L);
  CHECK(layout.hint.R == doctest::Approx(r));
  CHECK(layout.hint.W() >= 200.f);
}

TEST_CASE("Footswitch: hit-testing finds arrows, pips, switches and only an occupied switch's cross")
{
  const auto layout = LayoutFootswitch(72.f, 342.f, 828.f, 534.f);

  CHECK(HitTest(layout, layout.prev.MW(), layout.prev.MH(), NoneOccupied()).kind == HitKind::Prev);
  CHECK(HitTest(layout, layout.next.MW(), layout.next.MH(), NoneOccupied()).kind == HitKind::Next);
  // The readout and the range caption are labels, not buttons.
  CHECK(HitTest(layout, layout.readout.MW(), layout.readout.MH(), NoneOccupied()).kind == HitKind::None);
  CHECK(HitTest(layout, layout.range.MW(), layout.range.MH(), NoneOccupied()).kind == HitKind::None);

  for (int i = 0; i < kBankCount; ++i)
  {
    const Box& pip = layout.pips[static_cast<size_t>(i)];
    const auto hit = HitTest(layout, pip.MW(), pip.MH(), NoneOccupied());
    CHECK(hit.kind == HitKind::Pip);
    CHECK(hit.index == i);
  }

  std::array<bool, kSwitchesPerBank> occupied{};
  occupied[5] = true;
  for (int i = 0; i < kSwitchesPerBank; ++i)
  {
    const Box& tile = layout.tiles[static_cast<size_t>(i)];
    const auto body = HitTest(layout, tile.L + 20.f, tile.MH(), occupied);
    CHECK(body.kind == HitKind::Tile);
    CHECK(body.index == i);
    const Box clear = ClearBox(tile);
    CHECK(clear.L > tile.L);
    CHECK(clear.R <= tile.R);
    CHECK(clear.B <= tile.B);
    const auto corner = HitTest(layout, clear.MW(), clear.MH(), occupied);
    CHECK(corner.index == i);
    // Only a switch with a Sound has something to clear.
    CHECK(corner.kind == (i == 5 ? HitKind::Clear : HitKind::Tile));
  }

  // The gutters between switches belong to no switch.
  const float gapX = 0.5f * (layout.tiles[0].R + layout.tiles[1].L);
  CHECK(HitTest(layout, gapX, layout.tiles[0].MH(), occupied).kind == HitKind::None);
  const float gapY = 0.5f * (layout.tiles[0].B + layout.tiles[4].T);
  CHECK(HitTest(layout, layout.tiles[0].MW(), gapY, occupied).kind == HitKind::None);
  CHECK(HitTest(layout, 10.f, 10.f, occupied).kind == HitKind::None);
}

TEST_CASE("Footswitch: the Settings MIDI tab is the footswitch view, not the old table")
{
  const auto root = FootswitchRepoRoot() / "NeuralAmpModeler";
  const std::string view = FootswitchReadText(root / "VoLumMidiFootswitch.h");
  const std::string tabs = FootswitchReadText(root / "VoLumSettingsTabs.h");
  const std::string controls = FootswitchReadText(root / "NeuralAmpModelerControls.h");
  const std::string layout = FootswitchReadText(root / "VoLumLayoutBuild.inc.cpp");
  const std::string core = FootswitchReadText(root / "VoLumCoreControls.h");

  CHECK(view.find("class VoLumMidiFootswitchControl") != std::string::npos);
  CHECK(core.find("#include \"VoLumMidiFootswitch.h\"") != std::string::npos);
  CHECK(controls.find("new VoLumMidiFootswitchControl(cardBody)") != std::string::npos);
  // One view, not two: the PROGRAM / SOUND / AMP table and its number step are gone.
  CHECK(tabs.find("class VoLumMidiSoundMapControl") == std::string::npos);
  CHECK(controls.find("VoLumMidiSoundMapControl") == std::string::npos);
  CHECK(view.find("\"PROGRAM NUMBER\"") == std::string::npos);
  CHECK(view.find("OpenNumberStep(") == std::string::npos);
  CHECK(view.find("+  Add Sound") == std::string::npos);

  // Numbering, paging, drop decisions and hit zones come from the tested model.
  CHECK(view.find("volum::footswitch::ProgramFor(mBank, hit.index)") != std::string::npos);
  CHECK(view.find("volum::footswitch::OpeningBank(mLiveProgram)") != std::string::npos);
  CHECK(view.find("volum::footswitch::WheelBankStep(mWheelAccum, d)") != std::string::npos);
  CHECK(view.find("volum::footswitch::DecideDrop(from, to,") != std::string::npos);
  CHECK(view.find("volum::footswitch::HitTest(BoardLayout(), x, y, OccupiedOnBank())") != std::string::npos);

  // Same data and the same writers PLAY uses; a drop is always a swap (move onto empty).
  CHECK(view.find("volum::BuildPlaySlots(factory, registry)") != std::string::npos);
  CHECK(view.find("volum::BuildSoundChoices(factory, registry)") != std::string::npos);
  CHECK(view.find("midiSoundMap =") == std::string::npos);
  CHECK(view.find("mSwap(from, to)") != std::string::npos);
  CHECK(view.find("mClear(from)") != std::string::npos);
  CHECK(view.find("mAssign(mEditProgram, mChoices[static_cast<size_t>(choice)])") != std::string::npos);
  CHECK(layout.find("settingsPage->SetMidiSoundMapSwap([pPlugin](int a, int b) { pPlugin->_VolumSwapPlaySounds(a, b); "
                    "});")
        != std::string::npos);
  CHECK(layout.find("settingsPage->SetMidiSoundMapInsert(") == std::string::npos);

  // LIVE is PLAY's test, and the page hands the live program down.
  CHECK(view.find("volum::IsLastRecalledSlot(slot, mLiveProgram, mActiveAmpId, mActivePresetId)") != std::string::npos);
  CHECK(controls.find("SetData(factory, registry, liveProgram, activeAmpId, activePresetId)") != std::string::npos);

  // Selection SSOT: the switch highlight and the picker rows go through DrawVoLumSelection.
  CHECK(view.find("DrawVoLumSelection(g, tile, live, tileHover && !live, VoLumSelectionStyle::Brass")
        != std::string::npos);
  const std::string picker = FootswitchReadText(root / "VoLumMidiSoundPicker.h");
  CHECK(picker.find("DrawVoLumSelection(g, row, active, mHoverChoice == choice, VoLumSelectionStyle::ListTeal")
        != std::string::npos);
  // The picker is one shared panel, not a second copy inside the view.
  CHECK(view.find("#include \"VoLumMidiSoundPicker.h\"") != std::string::npos);
  CHECK(view.find("void WalkRows(") == std::string::npos);
  CHECK(view.find("current && current->valid ? &current->sound : nullptr") != std::string::npos);
  // Invalid switches use the PLAY rail's words.
  CHECK(view.find("volum::OccupiedSlotLabel(false, slot->sound.presetName)") != std::string::npos);
}

TEST_CASE("Footswitch: the view reopens on the live bank and Escape backs out of a pick or drag first")
{
  const auto root = FootswitchRepoRoot() / "NeuralAmpModeler";
  const std::string view = FootswitchReadText(root / "VoLumMidiFootswitch.h");
  const std::string presets = FootswitchReadText(root / "VoLumSettingsPresets.inc.cpp");
  const std::string plugin = FootswitchReadText(root / "NeuralAmpModeler.cpp");

  // Hidden (Settings closed, another tab showing) the view follows the live
  // program; visible, it stays on the bank the player chose.
  const auto setData = view.find("void SetData(");
  REQUIRE(setData != std::string::npos);
  const auto follow = view.find("if (IsHidden())", setData);
  REQUIRE(follow != std::string::npos);
  CHECK(view.find("mBank = volum::footswitch::OpeningBank(mLiveProgram);", follow) - follow < 120);
  const auto hide = view.find("void Hide(bool hide) override");
  REQUIRE(hide != std::string::npos);
  CHECK(view.find("if (!hide && IsHidden())", hide) != std::string::npos);
  CHECK(view.find("if (hide && (mScreen != kScreenBoard || mDragging))", hide) != std::string::npos);

  const auto esc = view.find("bool ConsumeEscape()");
  REQUIRE(esc != std::string::npos);
  CHECK(view.find("if (mScreen == kScreenBoard && !mDragging)", esc) != std::string::npos);

  CHECK(presets.find("page->SetMidiSoundMap(mVolumFactoryPresets, volum::content::GlobalContentStore().reg(), "
                     "mVolumLastRecalledPlaySlot,")
        != std::string::npos);
  // A Program Change that lands while Settings is open moves the LIVE lamp.
  const auto drain = plugin.find("mVolumMidiQueue.Drain()");
  REQUIRE(drain != std::string::npos);
  CHECK(plugin.substr(drain, 520).find("_VolumRefreshMidiSettingsChrome();") != std::string::npos);
}
