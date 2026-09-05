#include "third_party/doctest.h"

#include "../VoLumAboutLayout.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumCustomModel.h"
#include "../VoLumFactoryPresets.h"
#include "../VoLumOverlayStack.h"
#include "../VoLumHeaderChrome.h"
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
  CHECK(play.find("FACTORY  ·") == std::string::npos);
  CHECK(tabs.find("FACTORY  ·") == std::string::npos);
  CHECK(menus.find("FACTORY  ·") == std::string::npos);
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
}

TEST_CASE("SYSTEM mid-row body fits both Pack help lines")
{
  CHECK(volum::packui::SettingsCardBodyH(volum::packui::SystemMidRowH())
        >= volum::packui::PackRowMinBodyH() - 0.01f);
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
  CHECK_FALSE(volum::AddHeardNeedsSaveAs(A::OverwriteUser, true, false));
  CHECK_FALSE(volum::AddHeardNeedsSaveAs(A::OverwriteUser, false, true));
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
  CHECK(volum::PlayBranchConsumes(false, false, true));
  CHECK_FALSE(volum::PlayBranchConsumes(true, true, false)); // Ctrl+S
  CHECK_FALSE(volum::PlayBranchConsumes(false, false, false)); // T/M/H/plain S
  CHECK(volum::SectionForEffectFocus(EVoLumEffectFocus::PRE_NAM1) == EVoLumSection::PRE);
  CHECK(volum::SectionForEffectFocus(EVoLumEffectFocus::CHORUS) == EVoLumSection::POST);
  CHECK(volum::SectionForEffectFocus(EVoLumEffectFocus::AMP) == EVoLumSection::AMP);
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  REQUIRE(layout.find("PlayBranchConsumes(key.C, railStep, stomp >= 0)") != std::string::npos);
  const auto consumes = layout.find("PlayBranchConsumes(key.C, railStep, stomp >= 0)");
  const auto swallow = layout.find("return true;", consumes);
  REQUIRE(layout.find("AnyOverlayOpen(", consumes) < swallow);
  const auto playBranch = layout.find("if (mVolumUiMode == volum::UiMode::Play)");
  const auto fallthrough = layout.find("T / M / H / Ctrl+S fall through to the shared handler.");
  const auto shared = layout.find("if (_HandleVoLumKeyboardFocusKey(key))");
  REQUIRE(playBranch != std::string::npos);
  REQUIRE(fallthrough != std::string::npos);
  REQUIRE(shared != std::string::npos);
  CHECK(playBranch < fallthrough);
  CHECK(fallthrough < shared);
}

TEST_CASE("Ctrl+S and Default dirty use the live-vs-default comparison")
{
  const std::string presets = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsPresets.inc.cpp");
  REQUIRE(presets.find("LivePresetDirty(mVolumHasRecalledSnapshot, _VolumActiveScene(), mVolumRecalledSnapshot)")
          != std::string::npos);
  REQUIRE(presets.find("bool NeuralAmpModeler::_VolumHandleSaveShortcut()") != std::string::npos);
  REQUIRE(presets.find("_VolumPromptSaveAs") != std::string::npos);
  REQUIRE(presets.find("kCtrlTagVoLumNameDialog") != std::string::npos);
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
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const auto hGate = layout.find("if (key.VK == 'h' || key.VK == 'H')");
  const auto settingsH = layout.find("if (key.VK == kVK_ESCAPE || key.VK == 'h' || key.VK == 'H')");
  REQUIRE(hGate != std::string::npos);
  REQUIRE(settingsH != std::string::npos);
  CHECK(hGate < settingsH);
  const auto packHide = layout.find("kCtrlTagVoLumPackOverlay", hGate);
  REQUIRE(packHide != std::string::npos);
  CHECK(packHide < settingsH);
}

TEST_CASE("Name dialog Enter in the field saves")
{
  CHECK(volum::custom::NormalizePresetName("  Lead  ") == "Lead");
  CHECK(volum::custom::NameDialogCommitAfterTextEntry("Lead"));
  CHECK_FALSE(volum::custom::NameDialogCommitAfterTextEntry(""));
  bool armed = true;
  CHECK(volum::custom::NameDialogCommitOnce(armed, "Lead"));
  CHECK_FALSE(armed);
  CHECK_FALSE(volum::custom::NameDialogCommitOnce(armed, "Lead"));
  armed = true;
  CHECK_FALSE(volum::custom::NameDialogCommitOnce(armed, ""));
  CHECK(armed);
  const std::string dialog = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumNameDialog.h");
  const auto complete = dialog.find("void OnTextEntryCompletion");
  REQUIRE(complete != std::string::npos);
  CHECK(dialog.find("Commit();", complete) != std::string::npos);
  CHECK(dialog.find("std::move(mOnSave)") != std::string::npos);
  CHECK(dialog.find("mOnSave = nullptr") != std::string::npos);
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
  const auto guard = scene.rfind("#if defined(APP_API)", apply);
  REQUIRE(guard != std::string::npos);
  CHECK(apply - guard < 80);
  CHECK(midi - guard < 200);
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

  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  CHECK(tabs.find("bool ConsumeEscape()") != std::string::npos);
  CHECK(tabs.find("void ResetToList()") != std::string::npos);
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
