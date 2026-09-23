#include "third_party/doctest.h"

#include "../VoLumAboutLayout.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumCustomModel.h"
#include "../VoLumFactoryPresets.h"
#include "../VoLumOverlayStack.h"
#include "../VoLumHeaderChrome.h"
#include "../VoLumKeyboardModel.h"
#include "../VoLumPackLayout.h"
#include "../VoLumPickerGroups.h"
#include "../VoLumPlayLight.h"
#include "../VoLumPlayModel.h"
#include "../VoLumScroll.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("Empty preset id is Save As, not overwrite")
{
  CHECK(volum::SaveActionForActivePreset("") == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset(volum::FactoryPresetId(0)) == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset("preset_user") == volum::PresetSaveAction::OverwriteUser);
}

TEST_CASE("Picker groups: one section starts open, two start collapsed, then memory")
{
  volum::PickerGroupSession s;
  volum::InitPickerGroups(s, true, false);
  CHECK(s.initialized);
  CHECK(s.factoryOpen);
  CHECK_FALSE(s.userOpen);

  volum::PickerGroupSession userOnly;
  volum::InitPickerGroups(userOnly, false, true);
  CHECK(userOnly.userOpen);
  CHECK_FALSE(userOnly.factoryOpen);

  volum::PickerGroupSession both;
  volum::InitPickerGroups(both, true, true);
  CHECK_FALSE(both.factoryOpen);
  CHECK_FALSE(both.userOpen);
  volum::TogglePickerGroup(both, true);
  CHECK(both.factoryOpen);
  volum::InitPickerGroups(both, true, true);
  CHECK(both.factoryOpen); // session memory

  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  const std::string menus = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpMenus.inc.cpp");
  CHECK(play.find("InitPickerGroups(") != std::string::npos);
  CHECK(tabs.find("InitPickerGroups(") != std::string::npos);
  CHECK(menus.find("InitPickerGroups(") != std::string::npos);
  CHECK(std::string(volum::PickerGroupGlyph(false)) == "+");
  CHECK(std::string(volum::PickerGroupGlyph(true)) == "-");
  CHECK(volum::PickerGroupMenuLabel(true, false) == "+  FACTORY");
  CHECK(volum::PickerGroupMenuLabel(false, true) == "-  USER");
  CHECK(play.find("FACTORY  Â·") == std::string::npos);
  CHECK(tabs.find("FACTORY  Â·") == std::string::npos);
  CHECK(menus.find("FACTORY  Â·") == std::string::npos);
  CHECK(play.find("PickerGroupGlyph(") != std::string::npos);
  CHECK(tabs.find("PickerGroupGlyph(") != std::string::npos);
  CHECK(menus.find("PickerGroupMenuLabel(") != std::string::npos);
}

TEST_CASE("About action row is pinned inside a 96 px leftover card")
{
  const auto l = volum::LayoutAboutCard(400.f, 96.f);
  CHECK(l.actionFits);
  CHECK(l.actionB == doctest::Approx(96.f));
  CHECK(l.actionT == doctest::Approx(96.f - volum::kAboutActionH));
  CHECK(l.noticeB <= l.actionT + 0.01f);
  CHECK(l.actionT >= 0.f);
  CHECK(l.url2B > l.url2T + 1.f);
  CHECK(l.noticeB - l.noticeT >= 16.f);
}

TEST_CASE("tier2e the shipped 76 px About body keeps the update pill above Check now")
{
  const auto l = volum::LayoutAboutCard(400.f, 76.f);
  CHECK(l.actionFits);
  CHECK(l.actionB == doctest::Approx(76.f));
  CHECK(l.noticeB <= l.actionT - volum::kAboutGap + 0.01f);
  CHECK(l.noticeB - l.noticeT >= 16.f);
  CHECK(l.url1B <= l.url1T + 0.01f);
}

TEST_CASE("tier2e both factory and user sections start collapsed together")
{
  volum::PickerGroupSession session;
  volum::InitPickerGroups(session, true, true);
  CHECK_FALSE(session.factoryOpen);
  CHECK_FALSE(session.userOpen);
  volum::PickerGroupSession onlyFactory;
  volum::InitPickerGroups(onlyFactory, true, false);
  CHECK(onlyFactory.factoryOpen);
  CHECK_FALSE(onlyFactory.userOpen);
}

TEST_CASE("SYSTEM mid-row body fits both Pack help lines")
{
  CHECK(volum::packui::SettingsCardBodyH(volum::packui::SystemMidRowH()) >= volum::packui::PackRowMinBodyH() - 0.01f);
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  CHECK(controls.find("SystemMidRowH()") != std::string::npos);
}

TEST_CASE("Pack status sits above the also-including band")
{
  const auto withAlso = volum::packui::LayoutPackChrome(0.f, 400.f, true);
  CHECK(withAlso.statusAboveAlso);
  CHECK(withAlso.statusT >= withAlso.alsoB - 0.01f);
  CHECK(withAlso.goT > withAlso.statusB);
  const auto noAlso = volum::packui::LayoutPackChrome(0.f, 400.f, false);
  CHECK(noAlso.statusAboveAlso);
}

TEST_CASE("Scroll thumb drag maps cursor y to a new offset")
{
  const float rectT = 0.f, rectB = 200.f, rectH = 200.f, contentH = 400.f;
  auto m = volum::scroll::ComputeScroll(rectT, rectB, rectH, contentH, 0.f);
  volum::scroll::Interaction bar;
  CHECK(bar.OnDown(93.f, m.thumbY + 2.f, 90.f, 96.f, m));
  CHECK(bar.dragging);
  const float mid = bar.OnDrag(m.trackTop + (m.trackH - m.thumbH) * 0.5f + bar.grabDY, m);
  CHECK(mid == doctest::Approx(m.maxScroll * 0.5f).epsilon(0.05));
  bar.OnUp();
  CHECK_FALSE(bar.dragging);
  CHECK(volum::scroll::WheelDelta(1.f, 20.f) == doctest::Approx(-30.f));
  CHECK(volum::scroll::ListWheelDelta(0.4f, 20.f) == doctest::Approx(-8.f));
  CHECK(volum::scroll::ListWheelDelta(1.f, 20.f) == doctest::Approx(-30.f));
  CHECK(volum::scroll::ClampScroll(-4.f, 10.f) == 0.f);
  CHECK(volum::scroll::ClampScroll(40.f, 10.f) == 10.f);
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  CHECK(play.find("VoLumScrollTrackRect(") != std::string::npos);
  CHECK(tabs.find("VoLumScrollTrackRect(") != std::string::npos);
  CHECK(play.find("amplist::RowRightX(") != std::string::npos);
}

TEST_CASE("PLAY + becomes Add this sound for dirty Factory or an unassigned User")
{
  CHECK(volum::PlayPlusAddsHeard(true, true, true));
  CHECK_FALSE(volum::PlayPlusAddsHeard(true, false, true));
  CHECK(volum::PlayPlusAddsHeard(false, false, false));
  CHECK_FALSE(volum::PlayPlusAddsHeard(false, false, true));
}

TEST_CASE("Add this sound Save As first for Default or dirty Factory")
{
  using A = volum::PresetSaveAction;
  CHECK(volum::AddHeardNeedsSaveAs(A::SaveUserCopy, false, true)); // clean Default
  CHECK(volum::AddHeardNeedsSaveAs(A::SaveUserCopy, true, true)); // dirty Default
  CHECK(volum::AddHeardNeedsSaveAs(A::SaveUserCopy, true, false)); // dirty Factory
  CHECK_FALSE(volum::AddHeardNeedsSaveAs(A::SaveUserCopy, false, false)); // clean Factory Ready
  CHECK(volum::AddHeardNeedsSaveAs(A::OverwriteUser, true, false));
  CHECK(volum::AddHeardNeedsSaveAs(A::OverwriteUser, false, true));
  CHECK(volum::AddHeardMarksLive(3, false));
  CHECK_FALSE(volum::AddHeardMarksLive(-1, false)); // map full
  CHECK_FALSE(volum::AddHeardMarksLive(0, true)); // Default has no id yet
}

TEST_CASE("PLAY illumination: quiet breathes, loud is brighter")
{
  const float dim = volum::PlayArtBrightness(0.f, 0.f);
  const float dimHi = volum::PlayArtBrightness(0.f, 1.f);
  const float loud = volum::PlayArtBrightness(0.85f, 0.5f);
  CHECK(loud > dimHi);
  CHECK(dimHi > dim);
  CHECK(volum::PlayCoronaOpacity(loud) > volum::PlayCoronaOpacity(dim));
  CHECK(volum::MeterNormFromLinear(0.25f) == doctest::Approx(0.83f).epsilon(0.03f));
  const float floorBright = volum::PlayArtBrightness(volum::kPlayPlayingFloorNorm, 1.f);
  CHECK(floorBright + 1e-4f >= dimHi);
  CHECK(volum::PlayLampFollow(0.2f, 0.8f) > 0.2f);
  CHECK(volum::PlayLampFollow(0.2f, 0.8f) < volum::PlayLampFollow(0.2f, 0.8f, 0.9f, 0.04f));
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  CHECK(play.find("PlayArtBrightness(mLampPeak, pulse)") != std::string::npos);
  CHECK(play.find("PlayArtBrightness(mInPeak, pulse)") == std::string::npos);
}

TEST_CASE("AnyOverlayOpen is true when any listed tag is showing")
{
  CHECK_FALSE(volum::ui::AnyOverlayOpen({1, 2, 3}, [](int) { return false; }));
  CHECK(volum::ui::AnyOverlayOpen({1, 2, 3}, [](int tag) { return tag == 2; }));
}

TEST_CASE("Invalid PLAY slots share one label")
{
  const auto factory = volum::DefaultFactoryPresets();
  volum::content::Registry registry;
  volum::content::AssignMidiSound(registry, 4, "gone", "gone");
  const auto slots = volum::BuildPlaySlots(factory, registry);
  REQUIRE(slots.size() == 1);
  CHECK(slots[0].sound.presetName == std::string(volum::kPlayInvalidSlotLabel));
  CHECK(volum::OccupiedSlotLabel(true, "Lead") == "Lead");
  CHECK(volum::OccupiedSlotLabel(false, "Lead") == std::string(volum::kPlayInvalidSlotLabel));
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  CHECK(play.find("MISSING SOUND") == std::string::npos);
  CHECK(tabs.find("MISSING SOUND") == std::string::npos);
  CHECK(play.find("Missing Sound") == std::string::npos);
  CHECK(play.find("a missing Sound") == std::string::npos);
  CHECK(tabs.find("gone missing") == std::string::npos);
  CHECK(play.find("OccupiedSlotLabel(") != std::string::npos);
  CHECK(tabs.find("OccupiedSlotLabel(") != std::string::npos);
}

TEST_CASE("Overlay attach needles exist in the layout and chrome is attached first")
{
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  size_t lastChrome = 0;
  for (const char* needle : volum::ui::kChromeUnderOverlayNeedles)
  {
    const auto at = layout.find(needle);
    REQUIRE(at != std::string::npos);
    lastChrome = std::max(lastChrome, at);
  }
  size_t firstOverlay = std::string::npos;
  size_t prev = 0;
  for (const char* needle : volum::ui::kOverlayAttachNeedles)
  {
    const auto at = layout.find(needle);
    REQUIRE(at != std::string::npos);
    CHECK(at > prev);
    prev = at;
    firstOverlay = (firstOverlay == std::string::npos) ? at : std::min(firstOverlay, at);
  }
  CHECK(lastChrome < firstOverlay);
}

TEST_CASE("Entering PLAY drops PRE/POST lock without restoring the scene")
{
  CHECK(volum::EnteringPlayDropsLocks(volum::UiMode::Build, volum::UiMode::Play));
  CHECK_FALSE(volum::EnteringPlayDropsLocks(volum::UiMode::Play, volum::UiMode::Build));
  CHECK_FALSE(volum::EnteringPlayDropsLocks(volum::UiMode::Play, volum::UiMode::Play));
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  REQUIRE(runtime.find("EnteringPlayDropsLocks(mVolumUiMode, mode)") != std::string::npos);
  REQUIRE(runtime.find("SectionForEffectFocus(f)") != std::string::npos);
  REQUIRE(runtime.find("_VolumStorePreToCurrentAmp()") != std::string::npos);
  REQUIRE(runtime.find("mVolumPreLocked = false") != std::string::npos);
  REQUIRE(runtime.find("_VolumSetPreLocked(false)") == std::string::npos);
  REQUIRE(runtime.find("_VolumClampSupportFocus()") != std::string::npos);
}

TEST_CASE("PLAY T/M/H and Ctrl+S fall through the PLAY key branch")
{
  CHECK(volum::PlayBranchConsumes(false, true, false));
  CHECK(volum::PlayBranchConsumes(true, true, false)); // Ctrl+arrow still owns the rail
  CHECK(volum::PlayBranchConsumes(false, false, true));
  CHECK(volum::PlayBranchConsumes(true, false, true));
  CHECK_FALSE(volum::PlayBranchConsumes(true, false, false)); // Ctrl+S is not a rail key
  CHECK_FALSE(volum::PlayBranchConsumes(false, false, false)); // T/M/H/plain S
  CHECK(volum::PlaySwallowsHiddenBuildEdit(false, 's'));
  CHECK(volum::PlaySwallowsHiddenBuildEdit(false, ' '));
  CHECK(volum::PlaySwallowsHiddenBuildEdit(false, 'b'));
  CHECK(volum::PlaySwallowsHiddenBuildEdit(false, '\t'));
  CHECK(volum::PlaySwallowsHiddenBuildEdit(false, 8)); // kVK_BACK
  CHECK(volum::PlaySwallowsHiddenBuildEdit(false, 13)); // kVK_RETURN
  CHECK(volum::PlaySwallowsHiddenBuildEdit(false, 0x2E)); // kVK_DELETE
  CHECK_FALSE(volum::PlaySwallowsHiddenBuildEdit(true, 's')); // Ctrl+S saves
  CHECK_FALSE(volum::PlaySwallowsHiddenBuildEdit(false, 't'));
  CHECK_FALSE(volum::PlaySwallowsHiddenBuildEdit(false, 'm'));
  CHECK_FALSE(volum::PlaySwallowsHiddenBuildEdit(false, 'h'));
  CHECK(volum::SectionForEffectFocus(EVoLumEffectFocus::PRE_NAM1) == EVoLumSection::PRE);
  CHECK(volum::SectionForEffectFocus(EVoLumEffectFocus::CHORUS) == EVoLumSection::POST);
  CHECK(volum::SectionForEffectFocus(EVoLumEffectFocus::AMP) == EVoLumSection::AMP);
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  REQUIRE(layout.find("PlayBranchConsumes(key.C, railStep, stomp >= 0)") != std::string::npos);
  REQUIRE(layout.find("PlaySwallowsHiddenBuildEdit(key.C, key.VK)") != std::string::npos);
  const auto playBranch = layout.find("if (mVolumUiMode == volum::UiMode::Play)");
  const auto consumePlay = layout.find("ConsumePlayKey(key)", playBranch);
  REQUIRE(playBranch != std::string::npos);
  REQUIRE(consumePlay != std::string::npos);
  REQUIRE(layout.find("AnyOverlayOpen(", playBranch) < consumePlay);
  const auto fallthrough = layout.find("T / M / H / Ctrl+S fall through to the shared handler.");
  const auto hideBuild = layout.find("PlaySwallowsHiddenBuildEdit(key.C, key.VK)");
  const auto shared = layout.find("if (_HandleVoLumKeyboardFocusKey(key))");
  REQUIRE(playBranch != std::string::npos);
  REQUIRE(fallthrough != std::string::npos);
  REQUIRE(hideBuild != std::string::npos);
  REQUIRE(shared != std::string::npos);
  CHECK(playBranch < fallthrough);
  CHECK(fallthrough < hideBuild);
  CHECK(hideBuild < shared);
}

TEST_CASE("Ctrl+S and Default dirty use the live-vs-default comparison")
{
  const std::string presets = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsPresets.inc.cpp");
  REQUIRE(presets.find("LivePresetDirty(mVolumHasRecalledSnapshot, sounding, mVolumRecalledSnapshot)")
          != std::string::npos);
  REQUIRE(presets.find("bool NeuralAmpModeler::_VolumHandleSaveShortcut()") != std::string::npos);
  REQUIRE(presets.find("_VolumPromptSaveAs") != std::string::npos);
  REQUIRE(presets.find("kCtrlTagVoLumNameDialog") != std::string::npos);
}

TEST_CASE("Ctrl+S always prompts and may reassign the LIVE slot only")
{
  const std::string presets = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsPresets.inc.cpp");
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  REQUIRE(presets.find("bool NeuralAmpModeler::_VolumHandleSaveShortcut()") != std::string::npos);
  REQUIRE(presets.find("_VolumPromptSaveAs()") != std::string::npos);
  REQUIRE(presets.find("SaveDialogSeedName") != std::string::npos);
  CHECK(presets.find("AssignMidiSound") == std::string::npos);
  REQUIRE(runtime.find("_VolumReassignLivePlaySlotAfterSave") != std::string::npos);
  REQUIRE(runtime.find("FirstFreeMidiSoundSlot") != std::string::npos);
}

TEST_CASE("Add this sound does not retarget the last Factory PLAY slot")
{
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  REQUIRE(runtime.find("FirstFreeMidiSoundSlot") != std::string::npos);
  REQUIRE(runtime.find("void NeuralAmpModeler::_VolumAddHeardPlaySound()") != std::string::npos);
  REQUIRE(runtime.find("AddHeardNeedsSaveAs") != std::string::npos);
  REQUIRE(runtime.find("AddHeardMarksLive") != std::string::npos);
  REQUIRE(runtime.find("_VolumPromptSaveAs(finish)") != std::string::npos);
  const auto finish = runtime.find("auto finish = [this]()");
  REQUIRE(finish != std::string::npos);
  const auto finishEnd = runtime.find("_VolumPromptSaveAs(finish)", finish);
  REQUIRE(finishEnd != std::string::npos);
  CHECK(runtime.substr(finish, finishEnd - finish).find("mVolumLastRecalledPlaySlot = slot") != std::string::npos);
}

TEST_CASE("H peels Pack before it closes Settings")
{
  // This used to be pinned as "the Pack branch appears before the Settings
  // branch in the H handler". Hotkey routing is now one decision function
  // (volum::keyboard::RouteKey), so the invariant is asked directly instead of
  // inferred from the order two strings happen to appear in a file.
  using volum::keyboard::KeyConsumer;
  using volum::keyboard::KeyKind;
  using volum::keyboard::OverlayStack;
  using volum::keyboard::RouteKey;

  OverlayStack packOverSettings;
  packOverSettings.settings = true;
  packOverSettings.pack = true;
  CHECK(RouteKey(packOverSettings, KeyKind::HotkeyH) == KeyConsumer::CloseOverlay);
  CHECK(volum::keyboard::TopOverlay(packOverSettings) == volum::keyboard::OverlayId::Pack);

  OverlayStack settingsOnly;
  settingsOnly.settings = true;
  CHECK(RouteKey(settingsOnly, KeyKind::HotkeyH) == KeyConsumer::CloseOverlay);
  CHECK(volum::keyboard::TopOverlay(settingsOnly) == volum::keyboard::OverlayId::Settings);
}

TEST_CASE("Name dialog is a view over the model: only Enter and Save commit")
{
  // The decisions live in VoLumNameDialogModel.h (test_volum_name_dialog.cpp).
  // These pins keep the control from growing its own again.
  CHECK(volum::custom::NormalizePresetName("  Lead  ") == "Lead");
  const std::string dialog = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumNameDialog.h");
  const auto complete = dialog.find("void OnTextEntryCompletion");
  REQUIRE(complete != std::string::npos);
  const auto completeEnd = dialog.find("\n  }", complete);
  REQUIRE(completeEnd != std::string::npos);
  const std::string completion = dialog.substr(complete, completeEnd - complete);
  CHECK(completion.find("ApplyTextEntryCompletion") != std::string::npos);
  CHECK(completion.find("Commit") == std::string::npos);
  // No iPlug text entry: it reports only on completion and eats the Cancel click.
  CHECK(dialog.find("->CreateTextEntry(") == std::string::npos);
  CHECK(dialog.find("volum::name_dialog::LabelText(label)") != std::string::npos);
  CHECK(dialog.find("std::move(mOnSave)") != std::string::npos);

  // The overwrite target is resolved by id inside the commit callback, never an
  // index captured when the dialog opened.
  const std::string presets = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsPresets.inc.cpp");
  const auto prompt = presets.find("void NeuralAmpModeler::_VolumPromptSaveAs");
  REQUIRE(prompt != std::string::npos);
  const auto commit = presets.find("[this, after, currentName, currentId](const std::string& name)", prompt);
  REQUIRE(commit != std::string::npos);
  CHECK(presets.find("PresetIndexByIdForOwner(_VolumActiveOwnerKey(), currentId)", commit) != std::string::npos);
  CHECK(presets.find("currentUserIdx", prompt) == std::string::npos);

  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const auto route = layout.find("case KeyConsumer::NameDialogKey:");
  REQUIRE(route != std::string::npos);
  CHECK(layout.substr(route, 200).find("dlg->OnKeyDown(0.f, 0.f, key);") != std::string::npos);
  CHECK(layout.find("mVolumUiMode == volum::UiMode::Play && !nameDialogOpen") != std::string::npos);
}

TEST_CASE("Plugins ignore standalone volumUiMode in the machine file")
{
  const std::string scene = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsScene.inc.cpp");
  const auto load = scene.find("void NeuralAmpModeler::_VolumLoadSettingsFromFile()");
  REQUIRE(load != std::string::npos);
  const auto apply = scene.find("UiModeFromMachineSettings(true, j, mVolumUiMode)", load);
  REQUIRE(apply != std::string::npos);
  const auto midi = scene.find("MidiChannelFromMachineSettings(true, j,", load);
  REQUIRE(midi != std::string::npos);
  const auto recallCc = scene.find("MidiRecallCcFromMachineSettings(true, j,", load);
  REQUIRE(recallCc != std::string::npos);
  const auto guard = scene.rfind("#if defined(APP_API)", apply);
  REQUIRE(guard != std::string::npos);
  CHECK(scene.find("#endif", guard) > apply);
  CHECK(scene.find("#endif", guard) > midi);
  CHECK(scene.find("#endif", guard) > recallCc);
  CHECK(scene.find("j.contains(\"midiCh\")", load) == std::string::npos);
  const auto setLite = scene.find("void NeuralAmpModeler::_VolumSetLiteMode(bool lite)");
  const auto owner = scene.find("std::string NeuralAmpModeler::_VolumActiveOwnerKey()");
  REQUIRE(setLite != std::string::npos);
  REQUIRE(owner != std::string::npos);
  CHECK(setLite < owner);
  CHECK(scene.substr(setLite, owner - setLite).find("_VolumSaveLiteMode();") != std::string::npos);
  CHECK(scene.substr(setLite, owner - setLite).find("_VolumSaveSettingsToFile") == std::string::npos);
}

TEST_CASE("PLAY picker, Settings MIDI, and Pack share ListWheelDelta")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  const std::string pack = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPackOverlay.h");
  CHECK(play.find("ListWheelDelta(d, kPickerRowH)") != std::string::npos);
  CHECK(tabs.find("ListWheelDelta(d, kRowH)") != std::string::npos);
  CHECK(pack.find("ListWheelDelta(d, kRowH)") != std::string::npos);
}

TEST_CASE("Degenerate dual heal only rewrites centered inverted pans")
{
  volum::VoLumAmpSettings s;
  s.dualAmpActive = true;
  s.supportPolarityInvert = true;
  s.mainAmpPan = 0.0;
  s.supportAmpPan = 0.0;
  CHECK(volum::DegenerateDualNeedsPanHeal(s));
  s.mainAmpPan = -1.0;
  s.supportAmpPan = 1.0;
  CHECK_FALSE(volum::DegenerateDualNeedsPanHeal(s));
  s.mainAmpPan = 0.0;
  s.supportAmpPan = 0.0;
  s.supportPolarityInvert = false;
  CHECK_FALSE(volum::DegenerateDualNeedsPanHeal(s));
}

TEST_CASE("BUILD preset menu hides an empty User section")
{
  const std::string menus = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpMenus.inc.cpp");
  REQUIRE(menus.find("if (!presets.empty())") != std::string::npos);
  REQUIRE(menus.find("No user presets yet") == std::string::npos);
}

TEST_CASE("PLAY art sits above the name banner")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  REQUIRE(play.find("static constexpr float kBannerH = 58.f;") != std::string::npos);
  REQUIRE(play.find("stage.B - kBannerH") != std::string::npos);
  REQUIRE(play.find("IRECT BannerRect()") != std::string::npos);
  REQUIRE(play.find("DrawCachedStageArt") != std::string::npos);
}

TEST_CASE("Header chrome: name center, PLAY/BUILD on the tool rail")
{
  const auto h = volum::LayoutHeaderChrome(178.f, 900.f, 0.f);
  CHECK(h.plateB == doctest::Approx(46.f));
  CHECK(h.inkT == doctest::Approx(10.f));
  CHECK(h.inkB == doctest::Approx(36.f));
  CHECK(h.inkT == doctest::Approx((h.plateB - 26.f) * 0.5f));
  CHECK(h.toggleL == doctest::Approx(721.f));
  CHECK(h.toggleR == doctest::Approx(765.f));
  CHECK(h.presetL == doctest::Approx(419.f));
  CHECK(h.presetR == doctest::Approx(659.f));
  CHECK(h.tunerL == doctest::Approx(778.f));
  CHECK(h.gearR == doctest::Approx(882.f));
  CHECK(h.toggleR + volum::kHeaderToolGap == doctest::Approx(h.tunerL));
  CHECK(h.cabBandT == doctest::Approx(56.f));
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  CHECK(layout.find("LayoutHeaderChrome(mainL, mainR, b.T)") != std::string::npos);
  CHECK(layout.find("VoLumBuildHeaderPlateControl") != std::string::npos);
  CHECK(layout.find("VoLumSettingsVertRuleControl(IRECT(mainR - 125.f") == std::string::npos);
}

TEST_CASE("PLAY header is wordmark-only; rail Add matches empty-state copy")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const auto hdr = play.find("void DrawHeader(IGraphics& g)");
  const auto empty = play.find("void DrawEmpty(IGraphics& g)");
  REQUIRE(hdr != std::string::npos);
  REQUIRE(empty != std::string::npos);
  REQUIRE(hdr < empty);
  const std::string header = play.substr(hdr, empty - hdr);
  CHECK(header.find("FillVGradient") != std::string::npos);
  CHECK(header.find("g.DrawLine(VoLumColors::FRAME, h.L, h.B, h.R, h.B)") != std::string::npos);
  CHECK(header.find("MIDI IN") == std::string::npos);
  CHECK(header.find("kSidebarW") != std::string::npos);
  CHECK(header.find("volum::kHeaderRail") != std::string::npos);
  CHECK(header.find("LayoutHeaderChrome") != std::string::npos);
  CHECK(play.find("mPlusAddsHeard ? \"+   Add this sound\" : \"+   Add Sound\"") != std::string::npos);
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const auto hidePreset = layout.find("preset->Hide(mVolumUiMode == volum::UiMode::Play)");
  CHECK(hidePreset != std::string::npos);
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  CHECK(layout.find("kCtrlTagVoLumHeaderPlate") != std::string::npos);
  CHECK(layout.find("plate->Hide(mVolumUiMode == volum::UiMode::Play)") != std::string::npos);
  CHECK(runtime.find("plate->Hide(mode == volum::UiMode::Play)") != std::string::npos);
}

TEST_CASE("PLAY rail follows the LIVE slot instead of pinning it")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  CHECK(play.find("void EnsureActiveRowVisible()") != std::string::npos);
  CHECK(play.find("if (lastSlot != prevSlot)") != std::string::npos);
  CHECK(play.find("volum::scroll::ScrollToReveal") != std::string::npos);
  CHECK(play.find("bool sticky") == std::string::npos);
  CHECK(play.find("RailRowRect(int index, bool") == std::string::npos);
  CHECK(play.find("if (active && !hovered)") == std::string::npos);
  CHECK(play.find("row.B - 23.f") != std::string::npos);
  CHECK(play.find("FitTextToWidth") != std::string::npos);
  CHECK(play.find("mCurTip") != std::string::npos);
}

TEST_CASE("BUILD status row is padded; hint sits under it")
{
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const std::string footer = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCoreControls.h");
  const std::string hint = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboardNav.h");
  const auto footerClass = footer.find("class VoLumFooterControl");
  const auto hintClass = hint.find("class VoLumKeyboardHintControl");
  REQUIRE(footerClass != std::string::npos);
  REQUIRE(hintClass != std::string::npos);
  const std::string footerBody = footer.substr(footerClass, 900);
  const std::string hintBody = hint.substr(hintClass, 800);
  CHECK(layout.find("hintH + 6.f + footerH") == std::string::npos);
  CHECK(layout.find("footerGap + footerH + hintH") != std::string::npos);
  CHECK(layout.find("const float footerH = 24.f;") != std::string::npos);
  CHECK(layout.find("const float hintH = 16.f;") != std::string::npos);
  CHECK(layout.find("footerArea.T - 6.f - hintH") == std::string::npos);
  CHECK(layout.find("IRECT hintArea(mainL, footerArea.B, mainR, footerArea.B + hintH)") != std::string::npos);
  CHECK(footerBody.find("FitTextToWidth") != std::string::npos);
  CHECK(footerBody.find("void SetStatus(const char* text, bool alert)") != std::string::npos);
  CHECK(hintBody.find("FillRoundRect") == std::string::npos);
  CHECK(hintBody.find("FitTextToWidth") != std::string::npos);
  const std::string plugin = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  CHECK(plugin.find("SetStatus(\"Output safety active - lower output or wet mix\", true)") != std::string::npos);
}

TEST_CASE("Settings MIDI hide resets to the list and Escape pops first")
{
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const auto hideFn = controls.find("void HideAnimated(bool hide)");
  const auto reset = controls.find("ResetToList()", hideFn);
  const auto hideKids = controls.find("ForAllChildrenFunc([hide]", hideFn);
  REQUIRE(hideFn != std::string::npos);
  REQUIRE(reset != std::string::npos);
  REQUIRE(hideKids != std::string::npos);
  CHECK(reset < hideKids);

  const auto esc = controls.find("if (key.VK == kVK_ESCAPE)");
  const auto consume = controls.find("ConsumeEscape()", esc);
  const auto close = controls.find("HideAnimated(true);", esc);
  REQUIRE(esc != std::string::npos);
  REQUIRE(consume != std::string::npos);
  REQUIRE(close != std::string::npos);
  CHECK(consume < close);

  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  REQUIRE(layout.find("page->ConsumeEscape()") != std::string::npos);
  REQUIRE(layout.find("ConsumePlayKey(key)") != std::string::npos);
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  CHECK(play.find("bool ConsumePlayKey(const IKeyPress& key)") != std::string::npos);

  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  CHECK(tabs.find("bool ConsumeEscape()") != std::string::npos);
  CHECK(tabs.find("void ResetToList()") != std::string::npos);
  const auto hideOverride = tabs.find("void Hide(bool hide) override");
  REQUIRE(hideOverride != std::string::npos);
  const auto hideDraw = tabs.find("void Draw(IGraphics& g) override", hideOverride);
  REQUIRE(hideDraw != std::string::npos);
  const auto hideReset = tabs.find("ResetToList()", hideOverride);
  REQUIRE(hideReset != std::string::npos);
  CHECK(hideReset < hideDraw);
  CHECK(tabs.find("if (hide && mScreen != kScreenList)") != std::string::npos);
  CHECK(tabs.find("FlashEmptyHint()") != std::string::npos);
  const auto addClick = tabs.find("if (AddRect().Contains(x, y))");
  const auto flash = tabs.find("FlashEmptyHint();", addClick);
  const auto open = tabs.find("OpenNumberStep(FirstFreeSlot());", addClick);
  REQUIRE(addClick != std::string::npos);
  REQUIRE(flash != std::string::npos);
  REQUIRE(open != std::string::npos);
  CHECK(flash < open);
  CHECK(tabs.find("if (mChoices.empty())") != std::string::npos);
}

TEST_CASE("Settings MIDI callbacks are wired after the page attaches")
{
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const auto attach = layout.find("AttachControl(settingsPage, kCtrlTagSettingsBox)");
  const auto setMidi = layout.find("settingsPage->SetMidiCallbacks([pPlugin](int channel)");
  REQUIRE(attach != std::string::npos);
  REQUIRE(setMidi != std::string::npos);
  CHECK(attach < setMidi);
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  CHECK(controls.find("_ApplyMidiWiring()") != std::string::npos);
}

TEST_CASE("Add picker lands on 0 when every program number is taken")
{
  CHECK(volum::AddPickerStartSlot(-1) == 0);
  CHECK(volum::AddPickerStartSlot(7) == 7);
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  CHECK(play.find("OpenPicker(volum::AddPickerStartSlot(FirstFreeSlot()), true);") != std::string::npos);
}

TEST_CASE("Empty PLAY NAM stomps do not take a bypass click")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const auto guard = play.find("if (!mFxAvailable[(size_t)i])");
  const auto bypass = play.find("mBypass(volum::kPlayBypassParamNames");
  REQUIRE(guard != std::string::npos);
  REQUIRE(bypass != std::string::npos);
  CHECK(guard < bypass);
  REQUIRE(play.find("if (mod.R)") != std::string::npos);
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  REQUIRE(runtime.find("PlayNamCaptureAssigned(GetParam(kPreNam1Capture)->Int())") != std::string::npos);
  CHECK(runtime.find("fxAvailable[VoLumPlaySurfaceControl::Nam1] = mPreModel[0] != nullptr") == std::string::npos);
  REQUIRE(runtime.find("_ClearVoLumKnobSelection()") != std::string::npos);
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  REQUIRE(layout.find("PlayStompCanBypass(stomp,") != std::string::npos);
}

TEST_CASE("PLAY OUT meter follows the output peak")
{
  const std::string plugin = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  CHECK(plugin.find("mVolumPlayOutPeak.store") != std::string::npos);
  CHECK(runtime.find("SetOutPeak(") != std::string::npos);
  CHECK(play.find("out ? mOutPeak : mInPeak") != std::string::npos);
}

TEST_CASE("Settings MIDI and PLAY copy stay in Josefin's glyph set")
{
  auto noHigh = [](const std::filesystem::path& path) {
    const std::string text = ReadText(path);
    for (unsigned char c : text)
      CHECK(c < 0x80);
  };
  noHigh(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  noHigh(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
}
