#include "third_party/doctest.h"
#include "../config.h"
#include "../VoLumTriptychLayout.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Read NeuralAmpModeler.cpp plus its tail-included siblings. They are
// all part of the same plugin translation unit; we treat them as one
// logical source blob for source-string regression locks so a hygiene
// extract of a function into a *.inc.cpp file does not require updating
// every test that pinned a string in that function.
std::string ReadPluginSource()
{
  const auto root = RepoRoot() / "NeuralAmpModeler";
  std::string blob;
  blob += ReadText(root / "NeuralAmpModeler.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumLayoutBuild.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumKeyboard.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumLayoutRuntime.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumSceneRig.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumRigRepair.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumAmpMenus.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumProcessBlock.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumLoader.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumSettings.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumSettingsLocks.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumSettingsScene.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumSettingsPresets.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "Unserialization.cpp");
  return blob;
}

void RequireContains(const std::string& haystack, const char* needle)
{
  INFO(std::string(needle));
  REQUIRE(haystack.find(needle) != std::string::npos);
}

void RequireDoesNotContain(const std::string& haystack, const char* needle)
{
  INFO(std::string(needle));
  REQUIRE(haystack.find(needle) == std::string::npos);
}

std::string MemberFnUntilNext(const std::string& src, const char* signature)
{
  const auto start = src.find(signature);
  REQUIRE(start != std::string::npos);
  const auto sigEnd = src.find(')', start);
  REQUIRE(sigEnd != std::string::npos);
  const auto end = src.find(" NeuralAmpModeler::", sigEnd);
  REQUIRE(end != std::string::npos);
  return src.substr(start, end - start);
}
} // namespace

TEST_CASE("POST pedal cards refresh active art state from delay and reverb params")
{
  const std::string source = ReadPluginSource(); // _UpdateVoLumLayout now in VoLumLayoutRuntime.inc.cpp
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(source, "card->SetActiveState(GetParam(kDelayActive)->Bool());");
  RequireContains(source, "card->SetActiveState(GetParam(kReverbActive)->Bool());");
  RequireContains(triptych, "{EVoLumEffectFocus::REVERB, \"REVRB\", kReverbActive}");
}

TEST_CASE("POST carries a fourth Chorus card wired to the Throat motif")
{
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");
  const std::string motifs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptychMotifs.h");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptychLayout.h");
  const std::string build = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutRuntime.inc.cpp");
  const std::string card = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPedalCardControl.h");

  // Quiet strip slot + expanded card + connector, all in bus order (chorus first).
  RequireContains(triptych, "{EVoLumEffectFocus::CHORUS, \"CHORUS\", kChorusActive}");
  RequireContains(layout, "Rect chorus;");
  RequireContains(layout, "Rect connector3;");
  RequireContains(build, "EVoLumEffectFocus::CHORUS, onPedalClick");
  RequireContains(build, "kCtrlTagVoLumChainConnector3");
  RequireContains(runtime, "card->SetActiveState(GetParam(kChorusActive)->Bool());");
  // The card footer must read the chorus mode, not fall through to "BYPASS".
  RequireContains(card, "case EVoLumEffectFocus::CHORUS:");
  // Throat motif: wormhole mouths + straight generators, not another LFO curve.
  RequireContains(motifs, "DrawChorusThroatMotif");
  RequireContains(motifs, "effect == EVoLumEffectFocus::CHORUS");
}

TEST_CASE("Clear and close affordances stroke a cross instead of drawing U+00D7")
{
  // Josefin ships no U+00D7, so "Ã—" renders as a tofu box. Every clear/close
  // affordance must go through DrawCrossGlyph.
  const std::string helpers = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumColorHelpers.h");
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string settings = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");

  RequireContains(helpers, "inline void DrawCrossGlyph(");
  RequireContains(play, "DrawCrossGlyph(g, clear,");
  RequireContains(play, "DrawCrossGlyph(g, PickerCloseRect()");
  // The glyph itself must never come back as text in these two surfaces.
  RequireDoesNotContain(play, "\xC3\x97\"");
  RequireDoesNotContain(settings, "\xC3\x97\"");
}

TEST_CASE("PLAY rail rows restore the list clip so none escape over the pinned Add")
{
  // IGraphics has no clip stack: an inner PathClipRegion() with no argument
  // clears the rail-list scissor entirely, and an inverted intersection is
  // treated as "no clip". Either one let the last row paint over Add.
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");

  RequireContains(play, "void DrawSlot(IGraphics& g, const IRECT& row, int index, const IRECT& clip)");
  RequireContains(play, "const IRECT c = r.Intersect(clip);");
  RequireContains(play, "if (c.W() <= 0.f || c.H() <= 0.f)");
  RequireContains(play, "g.PathClipRegion(clip);");
  RequireContains(play, "if (row.B > list.T + 0.5f && row.T < list.B - 0.5f)");
}

TEST_CASE("PLAY hero art maps through FractalCaseForAmp like the BUILD hero")
{
  // Passing the raw amp index drew a different fractal in PLAY than in BUILD for
  // the same amp.
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string hero = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumHero.h");

  RequireContains(hero, "DrawHeroFractalArt(g, artRect, FractalCaseForAmp(");
  RequireContains(play, "DrawHeroFractalArt(g, paint, FractalCaseForAmp(art));");
}

TEST_CASE("Settings update notice self-gates so opening Settings cannot resurrect it")
{
  // NAMSettingsPageControl::HideAnimated calls IContainerBase::Hide(false), which
  // un-hides every descendant. A Hide()-based update button therefore reappeared
  // with no update pending; the notice must decide inside Draw instead.
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");

  RequireContains(overlay, "class VoLumUpdateNoticeControl");
  RequireContains(overlay, "if (!mAvailable)");
  RequireContains(overlay, "return mAvailable && IControl::IsHit(x, y);");
  // An empty version must not render "Update available:  - What's new".
  RequireContains(overlay, "version.empty() ? \"Update available");
  RequireContains(controls, "mUpdateNotice->SetUpdate(available, version, notes, checkError);");
  RequireDoesNotContain(controls, "mUpdateButton->Hide(!available);");
  // Auto-check state must be visible; IVToggleControl drew neither frame nor value here.
  RequireContains(overlay, "class VoLumSettingsCheckboxControl");
  RequireContains(controls, "mAutoCheck->SetChecked(autoCheck);");
  RequireDoesNotContain(controls, "new IVToggleControl");
  // One left-aligned stack: a right-floating, vertically-centred pill drew the
  // release notes through the checkbox and off the card.
  RequireContains(controls, "volum::LayoutAboutCard(body.W(), body.H())");
  RequireContains(
    controls, "new VoLumUpdateNoticeControl(IRECT(body.L, body.T + l.noticeT, body.R, body.T + l.noticeB)");
  RequireDoesNotContain(controls, "left.ReduceFromRight((GetRECT().W() - colGap) * 0.44f)");
  RequireContains(overlay, "return IRECT(mRECT.L, mRECT.T, mRECT.L + w, mRECT.T + h);");
  RequireDoesNotContain(overlay, "return IRECT(mRECT.R - w, mRECT.MH() - h * 0.5f, mRECT.R, mRECT.MH() + h * 0.5f);");
  RequireContains(overlay, "g.PathClipRegion(notes);");
}

TEST_CASE("Shortcut info columns are weighted and clipped so Navigate cannot bleed")
{
  // "PRE / AMP / POST" is ~95 px of the ~142 px Navigate needs; at an equal third
  // it overflowed the divider onto the Edit column's keys.
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");

  RequireContains(overlay, "const float navW = colBand * 0.47f;");
  RequireContains(overlay, "g.PathClipRegion(descR);");
  RequireDoesNotContain(overlay, "const float colW = (body.W() - 2.f * gap) / 3.f;");
}

TEST_CASE("Settings is exactly three tabs and every locked capability still has a home")
{
  // The one-page overlay could not hold calibration, output mode, performance,
  // MIDI, shortcuts, model info, about and update at 900x600 without clipping its
  // own footer onto the panel's corner accents. MIDI is a tab of its own since
  // 1.3.0: it carries the Program Change assignment list, which does not fit on a
  // card beside three other cards.
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");

  RequireContains(tabs, "class VoLumSettingsTabStripControl");
  RequireContains(controls, "kTabNames[kTabCount] = {\"SIGNAL\", \"MIDI\", \"SYSTEM\"}");
  RequireContains(controls, "static constexpr int kTabCount = 3;");
  RequireContains(controls, "void _BuildSignalTab(const IRECT& body)");
  RequireContains(controls, "void _BuildMidiTab(const IRECT& body)");
  RequireContains(controls, "void _BuildSystemTab(const IRECT& body)");

  // SIGNAL owns the audio path.
  RequireContains(controls, "\"Input calibration\", mControlNames.inputGroupFrame");
  RequireContains(controls, "\"Output mode\", mControlNames.outputGroupFrame");
  RequireContains(controls, "\"Performance\", mControlNames.perfGroupFrame");
  RequireContains(controls, "audioHintStr");
  // MIDI owns the channel and the assignments.
  RequireContains(controls, "\"What this VoLum listens to\", mControlNames.midiGroupFrame");
  RequireContains(controls, "\"What each program number plays\"");
  RequireContains(controls, "mControlNames.midiMapGroupFrame");
  // SYSTEM owns this install.
  RequireContains(controls, "\"Keyboard shortcuts\"");
  RequireContains(controls, "\"Model information\"");
  RequireContains(controls, "\"Back up your library\"");
  RequireContains(controls, "mControlNames.aboutGroupFrame");

  // Both tabs live in one container, so every show path that un-hides all
  // descendants has to re-assert which tab owns the body.
  RequireContains(controls, "void _ApplyTabVisibility()");
  // Twice: once for HideAnimated's immediate un-hide, once for the fade's final
  // IContainerBase::Hide(false).
  size_t applied = 0;
  for (size_t pos = controls.find("_ApplyTabVisibility();"); pos != std::string::npos;
       pos = controls.find("_ApplyTabVisibility();", pos + 1))
    ++applied;
  CHECK(applied >= 3); // OnAttached + HideAnimated + animation end
  RequireContains(controls, "if (!mWillHide)");
}

TEST_CASE("The Settings MIDI tab and PLAY are two views of one Sound map")
{
  // Owner override to .scratch/midi-control/spec.md: the assignment list lives on
  // both surfaces. What must never happen is a second store - both have to read
  // registry.midiSoundMap through BuildPlaySlots and write it through the plugin's
  // assign/clear pair, or the two lists drift apart the moment one is edited.
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  const std::string view = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumMidiFootswitch.h");
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const std::string presets = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsPresets.inc.cpp");

  RequireContains(tabs, "class VoLumMidiChannelControl");
  RequireContains(tabs, "class VoLumMidiRecallCcControl");
  RequireContains(tabs, "\"Recall CC\"");
  RequireContains(tabs, "Value is the program number.");
  RequireContains(tabs, "\"All channels\"");
  RequireContains(tabs, "MIDI calls this Omni.");
  RequireContains(view, "class VoLumMidiFootswitchControl");
  RequireContains(controls, "void SetMidiChannel(int channel)");
  RequireContains(controls, "void SetMidiRecallCc(int cc)");
  RequireContains(controls, "void SetMidiSoundMap(");
  RequireContains(layout, "settingsPage->SetMidiCallbacks([pPlugin](int channel)");
  RequireContains(layout, "pPlugin->_VolumSetMidiRecallCc(cc)");

  // Both surfaces derive their rows from the same pure model helper.
  RequireContains(view, "volum::BuildPlaySlots(factory, registry)");
  RequireContains(play, "volum::BuildPlaySlots(factory, registry)");
  RequireContains(view, "volum::BuildSoundChoices(factory, registry)");
  RequireContains(play, "volum::BuildSoundChoices(factory, registry)");

  // Both write through the same plugin methods; the Settings tab keeps no copy
  // of its own, and the panel is refilled from the live registry. The
  // footswitch view has no insert: a switch's position is its program number.
  RequireContains(layout, "settingsPage->SetMidiSoundMapCallbacks(");
  RequireContains(layout, "settingsPage->SetMidiSoundMapSwap(");
  RequireDoesNotContain(layout, "settingsPage->SetMidiSoundMapInsert(");
  RequireContains(layout, "pPlugin->_VolumSwapPlaySounds(a, b)");
  RequireContains(layout, "pPlugin->_VolumAssignPlaySound(slot, sound)");
  RequireContains(layout, "pPlugin->_VolumClearPlaySound(slot)");
  RequireContains(presets, "page->SetMidiSoundMap(mVolumFactoryPresets, volum::content::GlobalContentStore().reg(),");
  RequireDoesNotContain(tabs, "midiSoundMap =");
  RequireDoesNotContain(view, "midiSoundMap =");

  // The pre-1.3.0 duplicate-list control is still gone; this is a new one.
  RequireDoesNotContain(overlay, "VoLumMidiSettingsControl");
}

TEST_CASE("The Settings MIDI tab says program numbers, and never calls a Sound row a channel")
{
  // Two unrelated MIDI numbers meet on this tab. Before 1.3.0 the copy let them
  // share vocabulary - a card captioned CHANNEL over a column headed PC - so a
  // guitarist reasonably read the rows as sixteen more channels. The listen
  // filter now leads with what it does, and the rows are program numbers
  // everywhere they are named.
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");

  RequireContains(controls, "\"What this VoLum listens to\", mControlNames.midiGroupFrame");
  RequireContains(tabs, "\"All channels\"");
  RequireContains(tabs, "\"One channel\"");
  // Omni survives exactly once, as the parenthetical for players who know it.
  RequireContains(tabs, "MIDI calls this Omni.");
  // The two situations that actually decide the answer, in the player's terms.
  RequireContains(tabs, "One guitarist, one board: leave this on All channels.");
  RequireContains(tabs, "Two VoLums on one MIDI cable");
  // The old stepper legend is gone: a default install only ever showed "Omni",
  // and "Ch 7" made the filter look like one of the numbered rows below it.
  RequireDoesNotContain(tabs, "\"CHANNEL\"");
  RequireDoesNotContain(tabs, "\"Ch \"");

  const auto midiClass = tabs.find("class VoLumMidiChannelControl");
  const auto midiEnd = tabs.find("class VoLumMidiRecallCcControl");
  REQUIRE(midiClass != std::string::npos);
  REQUIRE(midiEnd != std::string::npos);
  const std::string midiBody = tabs.substr(midiClass, midiEnd - midiClass);
  RequireContains(midiBody, "DrawVoLumSegmentSwitch(");
  RequireDoesNotContain(midiBody, "AmberPicker");
  RequireContains(controls, "DrawVoLumSegmentSwitch(");
  RequireContains(controls, "ReduceFromTop(134.f)");

  // The footswitch view names its numbers the same way: program numbers on
  // banks of switches, never "PC" and never a channel.
  const std::string view = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumMidiFootswitch.h");
  RequireContains(controls, "\"What each program number plays\"");
  RequireContains(view, "\"Program numbers \"");
  RequireContains(view, "\"BANK\"");
  RequireContains(view, "Sound for program number ");
  RequireContains(view, "Click a switch to give its program number a Sound.");
  RequireDoesNotContain(view, "\"PC\"");
  RequireDoesNotContain(view, "\"PC ");
  RequireDoesNotContain(view, "hannel");
  RequireDoesNotContain(view, "Sound for Program Change ");
}

TEST_CASE("Settings moves a Sound between program numbers only through swap, so no edit can drop a Sound")
{
  // The footswitch view replaced the retype-the-number field: a switch's
  // position is its program number, so moving a Sound is dragging its switch.
  // Every drop routes through SwapMidiSoundSlots: onto a free number it is a
  // move, onto an occupied one an exchange. Clicking a switch opens the Sound
  // picker for that number directly; there is no separate number step.
  const std::string view = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumMidiFootswitch.h");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");

  RequireContains(view, "OpenPicker(from);");
  RequireContains(view, "void SetSwapCallback(SwapCallback swap)");
  RequireContains(view, "volum::footswitch::DecideDrop(from, to, SlotAt(from) != nullptr, SlotAt(to) != nullptr)");
  RequireContains(view, "if (action != volum::footswitch::DropAction::None && mSwap)");
  RequireContains(view, "mSwap(from, to);");
  RequireContains(view, "void DrawDragGhost(IGraphics& g)");
  RequireContains(layout, "pPlugin->_VolumSwapPlaySounds(a, b)");
  RequireDoesNotContain(view, "InsertCallback");
  RequireDoesNotContain(view, "ParseNumericEntry");

  // A drag can page: arrows once per entry, pips straight to their bank.
  RequireContains(view, "if (mDragPageKind != hit.kind)");
  const auto track = view.find("void UpdateDragTarget(float x, float y)");
  REQUIRE(track != std::string::npos);
  const auto pip = view.find("if (hit.kind == volum::footswitch::HitKind::Pip)", track);
  REQUIRE(pip != std::string::npos);
  CHECK(view.find("SetBank(hit.index);", pip) - pip < 80);
  // The picker a switch opens is the shared, scrollable Sound list.
  const std::string picker = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumMidiSoundPicker.h");
  RequireContains(view, "VoLumSoundPickerPanel mPicker;");
  RequireContains(picker, "volum::scroll::Interaction");
}

TEST_CASE("The SYSTEM tab's Content library row opens the live Pack modal")
{
  // .scratch/pack/spec.md puts Export/Import Pack in Gear -> Settings. Pack IO is
  // merged now, so the row must be the wired one: the inert placeholder that once
  // reserved the slot in VoLumSettingsTabs.h has to be gone, or the two classes
  // shadow each other and the dead one can win the overload.
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");

  RequireContains(overlay, "class VoLumSettingsPackRowControl");
  RequireContains(overlay, "\"Export Pack...\"");
  RequireContains(overlay, "\"Import Pack...\"");
  RequireDoesNotContain(tabs, "class VoLumSettingsPackRowControl");
  RequireDoesNotContain(overlay, "Not enabled in this build.");

  // Wired end to end: the row takes both callbacks, the page exposes the setter,
  // and the layout hands the plugin's Pack entry points down.
  RequireContains(controls, "new VoLumSettingsPackRowControl(");
  RequireContains(controls, "mOnExportPack();");
  RequireContains(controls, "mOnImportPack();");
  RequireContains(controls, "void SetPackCallbacks(");
  RequireContains(layout, "SetPackCallbacks(");
}

TEST_CASE("Every full-canvas overlay is attached above the PLAY/BUILD mode pair")
{
  // Attach order is z-order, and the 1.3.0 bug was attaching the mode pair last:
  // it then painted and hit-tested on top of Settings, Pack, Manage, the confirm
  // modal, the tuner and the metronome. It only has to outrank the PLAY surface,
  // which owns the whole main panel while visible; every real overlay comes after.
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");

  const auto toggle = layout.find("AttachControl(modeToggle, kCtrlTagVoLumModeToggle)");
  const auto playSurface = layout.find("kCtrlTagVoLumPlaySurface);");
  REQUIRE(toggle != std::string::npos);
  REQUIRE(playSurface != std::string::npos);
  INFO("the mode pair must stay clickable over the PLAY surface");
  CHECK(playSurface < toggle);

  for (const char* overlay :
       {"AttachControl(settingsPage, kCtrlTagSettingsBox)", "AttachControl(pack, kCtrlTagVoLumPackOverlay)",
        "AttachControl(overlay, kCtrlTagVoLumCustomOverlay)",
        "AttachControl(new VoLumConfirmDialogControl(b), kCtrlTagVoLumConfirm)",
        "AttachControl(tunerCtrl, kCtrlTagVoLumTuner)", "AttachControl(metCtrl, kCtrlTagVoLumMetronome)"})
  {
    INFO(overlay);
    const auto at = layout.find(overlay);
    REQUIRE(at != std::string::npos);
    CHECK(toggle < at);
  }

  // Destination glyph, brass family, hover names the other mode.
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const auto ctrl = play.find("class VoLumModeToggleControl");
  const auto ctrlEnd = play.find("class VoLumPlaySurfaceControl");
  REQUIRE(ctrl != std::string::npos);
  REQUIRE(ctrlEnd != std::string::npos);
  const std::string body = play.substr(ctrl, ctrlEnd - ctrl);
  RequireContains(body, "DrawVoLumSelection(g, mRECT, false, mMouseIsOver, VoLumSelectionStyle::Brass, 3.f, 1.5f);");
  RequireContains(body, "SelectionInkColor(VoLumSelectionStyle::Brass");
  RequireDoesNotContain(body, "AmberPicker");
  RequireDoesNotContain(body, "static constexpr float kCellW = 30.f;");
  RequireDoesNotContain(body, "mSplitX");
  RequireContains(body, "volum::UiMode Destination() const");
  RequireContains(body, "mCallback(Destination());");
  RequireContains(body, "static void DrawPlayGlyph(IGraphics& g, const IRECT& r, const IColor& ink)");
  RequireContains(body, "static void DrawBuildGlyph(IGraphics& g, const IRECT& r, const IColor& ink)");
  RequireContains(body, "SetTooltip(tip);");
  RequireContains(body, "Destination() == volum::UiMode::Play ? \"Switch to PLAY\" : \"Switch to BUILD\"");
  RequireContains(layout, "LayoutHeaderChrome(mainL, mainR, b.T)");
  RequireContains(layout, "IRECT(header.toggleL, header.inkT, header.toggleR, header.inkB)");
  RequireContains(layout, "presetBarArea(header.presetL, header.inkT, header.presetR, header.inkB)");
  RequireDoesNotContain(layout, "IRECT(mainR - 218.f, b.T + 12.f, mainR - 128.f, b.T + 42.f)");
  RequireDoesNotContain(layout, "IRECT(mainR - 202.f, b.T + 14.f, mainR - 134.f, b.T + 40.f)");
  RequireDoesNotContain(play, "const IRECT chip(h.MW() - 66.f, h.T + 14.f, h.MW() + 66.f, h.T + 40.f);");
  RequireDoesNotContain(play, "\"MIDI IN\"");
  RequireDoesNotContain(play, "h.R - 344.f");
}

TEST_CASE("PLAY assignment drag swaps or inserts among existing program numbers")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");

  RequireContains(play, "using SwapCallback = std::function<void(int, int)>;");
  RequireContains(play, "CommitRailDrop(");
  RequireContains(play, "DrawRailDrop(");
  RequireContains(layout, "_VolumSwapPlaySounds(a, b)");
  RequireContains(layout, "_VolumInsertPlaySound(from, before)");
  RequireContains(runtime, "volum::content::SwapMidiSoundSlots(store.reg(), slotA, slotB)");
  RequireContains(runtime, "InsertMidiSoundAmongAssigned");
  RequireContains(runtime, "FollowLiveSlotAfterReorder");
  RequireContains(runtime, "_VolumSyncLivePlaySlotFromActivePair");
  RequireContains(play, "mRecall(slot.slot, slot.sound);");
  RequireContains(play, "mAddHeard()");
  RequireContains(play, "mod.R");
  RequireContains(play, "mEditInBuild(static_cast<int>(kStompFocus[static_cast<size_t>(i)]))");

  RequireContains(play, "IRECT AssignRectForRow(int index) const");
  RequireContains(play, "DrawPenGlyph(g, assign, VoLumColors::TEXT_BRIGHT);");
  RequireContains(play, "if (AssignRectForRow(row).Contains(x, y))");

  RequireContains(play, "OpenPicker(volum::AddPickerStartSlot(FirstFreeSlot()), true);");
  RequireContains(play, "SetEditSlot(mEditSlot - 1);");
  RequireContains(play, "volum::ParseNumericEntry(str, parsed)");
  RequireContains(play, "if (number >= 0 && number < volum::kMidiSoundSlotCount)");
  RequireContains(play, "if (mod.R)");
  RequireContains(play, "kNoValIdx");
  RequireContains(play, "\"replaces \"");
}

TEST_CASE("PLAY row art is a layer-cached mini fractal, and the rail scrolls like the sidebar")
{
  // The rail used to call DrawHeroFractalArt per row per frame into a 48x64
  // portrait strip: that is the hero renderer, and a hero composition cropped to a
  // strip reads as a smear rather than as art. Row art is now the same mini
  // fractal the sidebar draws, cached per art id and blitted with DrawFittedLayer
  // so it survives a window resize (regression B3).
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");

  RequireContains(play, "if (!g.CheckLayer(mFactoryArtLayers[(size_t)amp]))");
  RequireContains(play, "DrawSidebarMiniFractal(");
  RequireContains(play, "FractalCaseForAmp(amp)");
  RequireContains(play, "g.DrawFittedLayer(art, thumb, nullptr);");
  RequireContains(play, "void OnRescale() override");
  RequireContains(play, "for (auto& layer : mFactoryArtLayers)");
  RequireDoesNotContain(play, "|| g.CheckLayer(");
  RequireDoesNotContain(play, "DrawHeroFractalArt(g, thumb");
  // Layers are built before the rail list clip: StartLayer mutates the clip region.
  const auto build = play.find("BuildRowArtLayers(g);");
  const auto clipList = play.find("g.PathClipRegion(list);");
  REQUIRE(build != std::string::npos);
  REQUIRE(clipList != std::string::npos);
  CHECK(build < clipList);

  // Precision vs detent, and an eased target - the sidebar's exact split. Jumping a
  // whole kRowPitch per event with no animation made a trackpad flick teleport.
  RequireContains(play, "if (std::abs(d) < 1.f)");
  RequireContains(play, "mRailScroll = mRailScrollTarget;");
  RequireContains(play, "void StartRailScrollAnim()");
  RequireContains(play, "mRailScroll += (mRailScrollTarget - mRailScroll) * 0.45f;");
}

TEST_CASE("PLAY speaks BUILD's palette: brass chrome, teal only for SUPPORT and amp names")
{
  // Switching modes must not change the colour of the instrument. PLAY shipped on a
  // blue-green canvas with teal frames on the board, the rail rows, the header chip
  // and the OUT meter, so BUILD and PLAY read as two different products. Teal is
  // VoLum's SUPPORT-lane identity; on this surface only the secondary amp name and
  // the SUPPORT panel may use it.
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string core = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCoreControls.h");

  // The same canvas BUILD paints.
  RequireContains(core, "FillVGradient(g, mRECT, IColor(255, 21, 21, 29), IColor(255, 12, 12, 18));");
  RequireContains(play, "FillVGradient(g, mRECT, IColor(255, 21, 21, 29), IColor(255, 12, 12, 18));");
  RequireContains(play, "DrawVignette(g, mRECT, 72);");
  RequireDoesNotContain(play, "IColor(255, 20, 26, 36)");

  // A lit stomp is the motif at full brightness, same as BUILD's PRE/POST tiles.
  // Brass selection, a gold LED, and gold captions were the yellow brick that
  // replaced the art the moment more than two pedals were on.
  RequireContains(play, "DrawEffectMotif(g, motif, kStompFocus[i], !on);");
  RequireDoesNotContain(play, "DrawVoLumSelection(g, r, true, false, VoLumSelectionStyle::Brass, 2.f, 0.f);");
  RequireDoesNotContain(play, "g.FillCircle(VoLumColors::GOLD, r.R - 12.f, r.T + 11.f, 2.5f);");
  RequireDoesNotContain(play, "IColor(58, 252, 222, 145)");

  // Chrome that explained itself, or labelled a column that is not there.
  RequireDoesNotContain(play, "stomp to bypass the live rig");
  RequireDoesNotContain(play, "\"PC\"");

  // Teal survives in exactly two places: the SUPPORT amp panel and the secondary
  // amp name under the live title.
  size_t tealUses = 0;
  for (size_t at = play.find("VoLumColors::TEAL"); at != std::string::npos; at = play.find("VoLumColors::TEAL", at + 1))
    ++tealUses;
  INFO("teal uses in VoLumPlaySurface.h: " << tealUses);
  CHECK(tealUses <= 6);
  RequireContains(play, "g.DrawText(VoLumType::Label(10.f, VoLumColors::TEAL_DIM, EAlign::Near), secondary.c_str()");
}

TEST_CASE("PLAY consumes up/down to step Sounds and never falls through to BUILD")
{
  // Before 1.3.0 the PLAY branch returned false for every key, so arrows did
  // nothing at all; returning them unhandled instead would be worse, because the
  // BUILD amp list behind PLAY would silently switch amps.
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  const std::string model = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayModel.h");

  RequireContains(layout, "const int stomp = (key.VK >= '1' && key.VK <= '8')");
  RequireContains(layout, "_VolumTogglePlayBypass(volum::kPlayBypassParamNames");
  RequireContains(layout, "key.VK == kVK_LEFT || key.VK == kVK_RIGHT");
  RequireContains(layout, "_VolumStepPlaySlot((key.VK == kVK_UP || key.VK == kVK_LEFT) ? -1 : 1);");
  RequireContains(layout, "T / M / H / Ctrl+S fall through to the shared handler.");
  RequireDoesNotContain(layout, "_VolumStepPlaySlot(key.VK == kVK_UP ? -1 : 1);");
  // A modal over PLAY owns the keyboard.
  RequireContains(layout, "kCtrlTagVoLumPackOverlay, kCtrlTagVoLumCustomOverlay");

  // The step goes through the same recall a rail click uses, and the decision of
  // which slot is next is a pure helper with its own doctest.
  RequireContains(runtime, "volum::StepAssignedSlot(slots, mVolumLastRecalledPlaySlot, dir)");
  RequireContains(runtime, "VolumRecallSound(slot.sound.ampId, slot.sound.presetId)");
  RequireContains(model, "inline int StepAssignedSlot(");

  // Documented where the keyboard is documented, with the key spelled out: the
  // first attempt used U+2191/2193 arrow glyphs, which Josefin does not have, so
  // the cheat sheet shipped a labelled row with an empty key column.
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");
  RequireContains(overlay, "\"Up/Dn\", \"Sound in PLAY\"");
  RequireContains(overlay, "\"1-8\", \"stomps in PLAY\"");
}

TEST_CASE("Pack export offers Sound, whole-amp and Everything, and the closure is not a footnote")
{
  // The tick list shipped as one undifferentiated column of amps and presets with
  // the closure as a grey line under it. A player could not tell "this Sound" from
  // "this whole amp", and the one genuinely surprising fact about a Pack - what
  // gets dragged along - was the quietest thing on the sheet.
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPackOverlay.h");

  RequireContains(overlay, "enum class Scope");
  RequireContains(overlay, "static constexpr int kScopeCount = 3;");
  RequireContains(overlay, "\"Everything\"");
  RequireContains(overlay, "\"Sounds\"");
  RequireContains(overlay, "\"A whole amp\"");

  // Scopes map onto the existing selection contract; no new Pack fields.
  RequireContains(overlay, "sel.everything = mScope == Scope::Everything;");
  RequireContains(overlay, "(r.isAmp ? sel.ampIds : sel.presetIds).push_back(r.id)");

  // The closure has its own band, drawn on every scope, and locked rows stay
  // locked: a requirement is not a choice.
  RequireContains(overlay, "IRECT _AlsoRect() const");
  RequireContains(overlay, "void _DrawAlso(IGraphics& g)");
  RequireContains(overlay, "ITEMS COME ALONG");
  RequireContains(overlay, "r.required = !r.picked");
  RequireDoesNotContain(overlay, "\"Also including: \"");
}

TEST_CASE("All pedal controls remain editable while their block is bypassed")
{
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutRuntime.inc.cpp");

  // Pin every bypassable PRE/POST block before asserting the shared policy.
  for (const char* group : {"PITCH_TRANSPOSE_KNOBS", "PITCH_OCTAVER_KNOBS", "COMP_KNOBS", "PRE_NAM1_KNOBS",
                            "PRE_NAM2_KNOBS", "DELAY_KNOBS", "REVERB_KNOBS", "TREMOLO_KNOBS", "CHORUS_KNOBS"})
    RequireContains(runtime, group);

  INFO("Bypass must affect DSP only; visible controls must keep accepting mouse-wheel and pointer edits");
  CHECK(runtime.find("disableGroup(") == std::string::npos);
  CHECK(runtime.find("SetDisabled(") == std::string::npos);
}

TEST_CASE("Windows binary version resource matches config.h")
{
  const std::string rc = ReadText(RepoRoot() / "NeuralAmpModeler" / "resources" / "main.rc");
  const std::string version = PLUG_VERSION_STR;
  std::string numeric = version;
  for (char& c : numeric)
    if (c == '.')
      c = ',';
  numeric += ",0";

  RequireContains(rc, ("FILEVERSION " + numeric).c_str());
  RequireContains(rc, ("PRODUCTVERSION " + numeric).c_str());
  RequireContains(rc, ("VALUE \"FileVersion\", \"" + version + "\"").c_str());
  RequireContains(rc, ("VALUE \"ProductVersion\", \"" + version + "\"").c_str());
}

TEST_CASE("Collapsed PRE slots show selected pedal short labels")
{
  const std::string source = ReadPluginSource(); // layout now in VoLumLayoutBuild.inc.cpp
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "mPreNam1Label = (preNam1Label && preNam1Label[0] != '\\0') ? preNam1Label : \"NAM 1\";");
  RequireContains(triptych, "preSlots[2].label = mPreNam1Label.c_str();");
  RequireContains(source, "_VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), \"NAM 1\")");
  RequireContains(source, "_VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), \"NAM 2\")");
  RequireContains(triptych, "std::toupper(c)");
  RequireContains(triptych, "IText labelText(");
}

TEST_CASE("Long custom prepedal names are truncated and clipped in the quiet slot")
{
  const std::string source = ReadPluginSource(); // _UpdateVoLumLayout now in VoLumLayoutRuntime.inc.cpp
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  // Custom prepedal short label is capped (item: long names overflowed the pill).
  RequireContains(source, "volum::custom::ShortCaptureLabel(full);");
  // Belt-and-suspenders: the quiet-slot label draw is clipped to its own rect.
  RequireContains(triptych, "g.PathClipRegion(labelR);");
}

TEST_CASE("Dual amp pan knobs only show in AMP view")
{
  const std::string source = ReadPluginSource(); // _VolumApplyDualAmpFocus now in VoLumAmpMenus.inc.cpp

  RequireContains(source, "const bool showPanKnobs = dualActive && mVolumExpandedSection == EVoLumSection::AMP;");
  RequireContains(source, "c->Hide(!showPanKnobs);");
  RequireContains(source, "Pan the SUPPORT amp lane.");
  RequireContains(source, "Pan the MAIN amp lane.");

  // The SUPPORT lane's own overlays additionally need an amp to belong to. An
  // ungated PAN knob covered the empty lane's title strip - the exact 24x24 the
  // "Choose support amp" call to action is drawn in - and swallowed the clicks
  // meant to fill the lane, while dragging pan for an amp that did not exist.
  RequireContains(source, "const bool showSupportLaneControls = showPanKnobs && _VolumHasSupportAmp();");
  const auto supportKnob = source.find("ForControlInGroup(\"SUPPORT_PAN_KNOB\"");
  REQUIRE(supportKnob != std::string::npos);
  const auto supportKnobEnd = source.find("});", supportKnob);
  REQUIRE(supportKnobEnd != std::string::npos);
  RequireContains(source.substr(supportKnob, supportKnobEnd - supportKnob), "c->Hide(!showSupportLaneControls);");
}

TEST_CASE("The PRE NAM card routes its click through the shared capture-card protocol")
{
  // The decision itself is covered in test_volum_pre_pedal_captures.cpp. This
  // only pins that the control actually asks - a correct protocol nobody calls
  // is exactly how the empty SUPPORT lane shipped.
  const std::string card = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPedalCardControl.h");
  RequireContains(card, "volum::DecideCaptureCardClick(mIsFocused, captureIdx > volum::kPreCaptureEmptyIndex)");
  // Focus before open, or the layout rebuild the focus callback triggers hides
  // the menu that was just opened.
  const auto focusCall = card.find("action == volum::CaptureCardClick::FocusThenOpenPicker && mCallback");
  const auto openCall = card.find("plugin->_VolumShowPreCaptureMenu(captureSlot, mRECT);");
  REQUIRE(focusCall != std::string::npos);
  REQUIRE(openCall != std::string::npos);
  CHECK(focusCall < openCall);
}

TEST_CASE("Keyboard channel navigation routes through the focused lane's stepper callback")
{
  const std::string source = ReadPluginSource(); // layout now in VoLumLayoutBuild.inc.cpp
  const std::string keyboardNav = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboardNav.h");

  // Left/Right pick the focused lane's stepper and drive StepKeyboard, which
  // fires the SAME callback a click would (so keyboard + mouse cannot diverge,
  // and both stage the new channel's .nam). Regression: the old keyboard path
  // only relabelled the stepper and never loaded the custom channel.
  RequireContains(source, "supportFocus ? kCtrlTagVoLumSupportChannelStep : kCtrlTagVoLumChannelStep;");
  RequireContains(source, "stepper->As<VoLumChannelStepControl>()->StepKeyboard(delta);");
  RequireContains(keyboardNav, "void StepKeyboard(int delta)");
  RequireContains(keyboardNav, "if (mCallback)");
  // The custom SUPPORT stepper callback must update the custom support channel
  // (the loader resolves the .nam from mVolumCustomSupportChannel, not the param).
  // Channel-first: the picked row maps to an amp-wide gain stage.
  RequireContains(source, "mVolumCustomSupportChannel = chosen;");
}

TEST_CASE("Keyboard accessibility layer keeps section and target shortcuts")
{
  const std::string source = ReadPluginSource(); // keyboard handlers now in VoLumKeyboard.inc.cpp
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboardModel.h");
  const std::string exactEntry = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumExactEntry.h");
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string settings = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");

  RequireContains(source, "_HandleVoLumKeyboardFocusKey(key)");
  RequireContains(source, "key.VK == '1'");
  RequireContains(source, "key.VK == '2'");
  RequireContains(source, "key.VK == '3'");
  RequireContains(source, "key.VK == 's' || key.VK == 'S'");
  RequireContains(source, "_CycleVoLumKeyboardSpeaker(key.S ? -1 : 1)");
  RequireContains(source, "key.VK == 't' || key.VK == 'T'");
  RequireContains(source, "key.VK == 'm' || key.VK == 'M'");
  RequireContains(source, "key.VK == 'h' || key.VK == 'H'");
  RequireContains(source, "settings->As<NAMSettingsPageControl>()->HideAnimated(false);");
  RequireContains(source, "key.VK == kTabKey");
  // Standalone retains Space; plug-ins use B and leave Space for DAW transport.
  RequireContains(source, "#ifdef APP_API");
  RequireContains(source, "if (key.VK == ' ')");
  RequireContains(source, "if (key.VK == 'b' || key.VK == 'B')");
  RequireContains(source, "constexpr const char* kToggleOnOffHint = \"Space on/off\";");
  RequireContains(source, "constexpr const char* kToggleOnOffHint = \"B on/off\";");
  RequireContains(source, "return _CycleVoLumKeyboardTarget(key.VK == kVK_LEFT ? -1 : 1)");
  // POST Tab/arrow cycling must reach all four cards (Chorus/Delay/Reverb/
  // Tremolo), not just toggle Delay<->Reverb (the "can't arrow to Tremolo in
  // POST" bug). PRE has four too, so both sections use the same wrap count.
  RequireContains(source, "mVolumFocusedEffect = targets[wrap(current + direction, 4)];");
  RequireContains(source, "EVoLumEffectFocus::CHORUS: paramIdx = kChorusActive; break;");
  RequireContains(source, "Left/Right channel  |  S cab  |  Tab target");
  // S runs the cab row's own step, which fires the callback a click fires. Covered
  // properly in test_volum_cab_step.cpp; pinned here so the shortcut layer keeps
  // routing through it rather than growing a second copy of the cab logic again.
  RequireContains(source, "StepKeyboard(direction)");
  RequireContains(source, "Left/Right or Tab target");
  // The cheat-sheet's own title moved to the SYSTEM tab's card cap; the control
  // only draws the columns now.
  RequireContains(controls, "\"Keyboard shortcuts\"");
  RequireContains(settings, "class VoLumSettingsShortcutInfoControl");
  RequireContains(settings, "\"1/2/3\", \"PRE / AMP / POST\"");
  RequireContains(settings, "\"Space\", \"toggle\"");
  RequireContains(settings, "\"B\", \"toggle\"");
  RequireContains(settings, "\"S\", \"cab\"");
  RequireContains(settings, "\"Ctrl+S\", \"save Sound\"");
  RequireContains(settings, "\"T\", \"tuner\"");
  RequireContains(settings, "\"M\", \"metronome\"");
  RequireContains(settings, "\"H\", \"settings\"");
  RequireContains(settings, "\"Esc\", \"close\"");
  RequireContains(header, "constexpr std::array<int, 6> kMainAmpMonoParams");
  RequireContains(header, "constexpr std::array<int, 7> kMainAmpDualParams");
  RequireContains(header, "constexpr std::array<int, 7> kSupportAmpParams");
  RequireContains(header, "kSupportInputLevel, kSupportNoiseGateThreshold, kSupportToneBass");
  RequireDoesNotContain(header, "kSupportAmpIdx, kSupportSpeakerIdx, kSupportChannelIdx");
  RequireContains(source, "SelectAdjacentFromList(this, kMainAmpMonoParams");
  RequireContains(source, "SelectAdjacentFromList(this, kMainAmpDualParams");
  RequireContains(source, "SelectAdjacentFromList(this, kSupportAmpParams");
  RequireContains(source, "RememberedOrFirst(kDelaySyncedParams, remembered)");
  RequireContains(source, "RememberedOrFirst(kTremoloSyncedParams, remembered)");
  RequireContains(source, "volum::keyboard::RouteKey(stack, kind)");
  RequireContains(source, "SelectedKnobConsumesKind(");
  RequireContains(source, "As<VoLumTunerControl>()->Dismiss()");
  RequireContains(source, "As<VoLumMetronomeControl>()->Dismiss()");
  RequireContains(source, "SetActive(mMetronomeDSP.IsActive())");
  RequireContains(source, "DisabledPointerPolicy::kMouseOverWhenDisabled");
  RequireContains(source, "DisabledPointerPolicy::kMouseEventsWhenDisabled");
  RequireDoesNotContain(source, "SetMouseEventsWhenDisabled(true)");
  RequireContains(header, "kMainAmpPan");
  RequireContains(header, "kSupportAmpPan");
  RequireContains(header, "kDelaySyncedParams");
  RequireContains(header, "kTremoloSyncedParams");
  RequireContains(header, "kTremoloHarmonicSyncedParams");
  RequireContains(header, "enum class KeyConsumer");
  RequireContains(header, "inline KeyConsumer RouteKey(");
  RequireContains(controls, "volum::keyboard::StepForParam(GetParamIdx(), fine)");
  RequireContains(controls, "if (!mKeyboardSelected)");
  RequireContains(controls, "return Nudge(false, key.S);");
  RequireContains(controls, "PromptExactValueEntry();");
  RequireContains(source, "_UpdateVoLumKeyboardFocusHint();");
  RequireContains(source, "exact->CancelEntry();");
  RequireContains(exactEntry, "void CancelEntry()");
  RequireContains(exactEntry, "textEntry->DismissEdit();");
}

TEST_CASE("Percent value labels render natural percent text")
{
  const std::string exactEntry = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumExactEntry.h");
  const std::string source = ReadPluginSource(); // layout now in VoLumLayoutBuild.inc.cpp

  RequireContains(exactEntry, "std::strcmp(mSuffix, \"%\") == 0");
  RequireContains(exactEntry, "const double percent = pParam->Value() * 100.0;");
  RequireContains(exactEntry, "str.SetFormatted(16, \"%.0f%%\", percent);");
  RequireContains(source, "\"FEEDBACK\", kDelayFeedback, \"%\"");
  RequireContains(source, "\"MIX\", kDelayMix, \"%\"");
  RequireContains(source, "\"MIX\", kReverbMix, \"%\"");
  RequireContains(source, "\"INTENSITY\", kReverbShimmer, \"%\"");
}

TEST_CASE("Support hero label remains centered with polarity glyph")
{
  // VoLumHeroImageControl + VoLumSupportPolarityControl moved to VoLumHero.h
  // on the 1.0 hygiene split.
  const std::string hero = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumHero.h");

  RequireContains(hero, "Filled DAW-style polarity glyph");
  RequireContains(hero, "const float right = lane.R - 8.f;");
  RequireContains(hero, "const float top = lane.T + 8.f;");
  RequireContains(hero, "Flip polarity");
  RequireContains(hero, "Switch to Single Amp");
  RequireContains(hero, "Switch to Dual Amp");
  // The lane title is ellipsized to the strip width before drawing (long custom
  // amp names must not bleed past their lane).
  RequireContains(hero, "titleStrip.R - kPanKnobSize - 4.f");
  RequireContains(hero, "FitTextToWidth(g, nameText, name, nameR.W() - 6.f)");
  RequireContains(hero, "g.DrawText(nameText, fitted.c_str(), nameR);");
  RequireDoesNotContain(hero, "titleStrip.R - 34.f");
}

TEST_CASE("Amp settings restore refreshes support channel list")
{
  // _VolumRestoreFromSettings now lives in VoLumSettings.inc.cpp (still part of
  // the NeuralAmpModeler.cpp translation unit).
  const std::string source = ReadPluginSource();
  const auto restorePos = source.find("void NeuralAmpModeler::_VolumRestoreFromSettings(int ampIdx)");
  REQUIRE(restorePos != std::string::npos);
  const auto refreshPos = source.find("_VolumRefreshSupportChannels();", restorePos);

  REQUIRE(refreshPos != std::string::npos);
  CHECK(refreshPos > source.find("setParam(kSupportAmpPan, s.supportAmpPan);", restorePos));
  CHECK(refreshPos < source.find("mVolumSupportNeedsLoad.store(true);", restorePos));
}

TEST_CASE("Global VoLum settings writes are standalone-only")
{
  const std::string source = ReadPluginSource();
  const std::string needle = "_VolumSaveSettingsToFile();";
  size_t count = 0;
  size_t pos = source.find(needle);
  while (pos != std::string::npos)
  {
    const auto appGuard = source.rfind("#ifdef APP_API", pos);
    const auto previousEndif = source.rfind("#endif", pos);
    const auto nextEndif = source.find("#endif", pos);

    INFO("write call at offset " << pos);
    REQUIRE(appGuard != std::string::npos);
    REQUIRE(appGuard > previousEndif);
    REQUIRE(nextEndif != std::string::npos);

    ++count;
    pos = source.find(needle, pos + needle.size());
  }

  // Selection/arrow-key paths now defer the write via mVolumSettingsDirty
  // (flushed in OnIdle) to keep disk I/O off the selection hot path; only
  // OnIdle's flush and the two teardown paths still write synchronously.
  CHECK(count == 3);
}

TEST_CASE("OnIdle coalesces the deferred settings write")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  // Selections defer the two-file disk write by setting mVolumSettingsDirty;
  // OnIdle must keep draining it (clear the flag, then write) so deferred
  // selections still persist without stalling the selection hot path.
  const auto idlePos = source.find("void NeuralAmpModeler::OnIdle()");
  REQUIRE(idlePos != std::string::npos);
  const auto clearPos = source.find("mVolumSettingsDirty = false;", idlePos);
  REQUIRE(clearPos != std::string::npos);
  const auto writePos = source.find("_VolumSaveSettingsToFile();", clearPos);
  REQUIRE(writePos != std::string::npos);
  // The drain (clear + write) must come close together inside OnIdle.
  CHECK(writePos - clearPos < 120);
}

TEST_CASE("Only direct calibration UI edits update machine-global defaults")
{
  const std::string source = ReadPluginSource();
  RequireContains(source, "source == EParamSource::kUI");
  RequireContains(source, "paramIdx == kCalibrateInput || paramIdx == kInputCalibrationLevel");
  RequireContains(source, "mVolumCalibrationDefaultsDirty = true;");
  RequireContains(source, "if (mVolumCalibrationDefaultsDirty)");
  RequireContains(source, "_VolumSaveCalibrationDefaults();");
  // DAW/project restore sets the EParams directly and must not call the writer.
  const auto loadPos = source.find("void NeuralAmpModeler::_VolumLoadSettingsFromFile()");
  REQUIRE(loadPos != std::string::npos);
  const auto loadEnd = source.find("\n}", loadPos);
  REQUIRE(loadEnd != std::string::npos);
  CHECK(source.substr(loadPos, loadEnd - loadPos).find("_VolumSaveCalibrationDefaults") == std::string::npos);
}

TEST_CASE("Per-amp POST restore is guarded from mode snapshot re-entry")
{
  const std::string source = ReadPluginSource();
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");

  RequireContains(header, "bool mVolumPostRestoreInProgress = false;");
  RequireContains(source, "postGuard(mVolumPostRestoreInProgress);");
  RequireContains(source, "if (!s.postValid)");
  RequireContains(source, "const volum::VoLumAmpSettings defaults;");
  RequireContains(source, "if (mVolumPostRestoreInProgress)");
  RequireContains(source, "mVolumInitComplete && !mVolumPostRestoreInProgress");
  RequireContains(source, "const int restoredDelayMode = std::clamp(s.postDelayMode");
  RequireContains(source, "const int restoredReverbMode = std::clamp(s.postReverbMode");
  RequireContains(source, "_VolumRestoreDelayModeSnapshot(restoredDelayMode);");
  RequireContains(source, "_VolumRestoreReverbModeSnapshot(restoredReverbMode);");
  RequireContains(source, "_UpdateVoLumLayout(pGfx);");

  // No lane's cab selection is pushed from a restore path any more. mVolumSpeakerIdx
  // is a raw persisted index there: for a custom lane the resolver still has to snap
  // it to a slot the channel carries, and when SUPPORT is focused it belongs to the
  // other lane - the row is shared. The pin that was meant to prevent this matched on
  // the receiver name `spkRow->` and so missed the `spkCtrl->` spelling in
  // _VolumApplyAmpSettings; matched on the call instead, it catches either.
  RequireDoesNotContain(source, "SetSelected(mVolumSpeakerIdx);");
}

TEST_CASE("POST Tremolo per-mode switch is guarded from snapshot re-entry")
{
  // Mirrors the Reverb/Delay per-mode pattern: switching mode saves the outgoing
  // mode's knobs and recalls the incoming mode's, wrapped in a re-entrancy guard
  // so the setParam cascade does not overwrite the snapshot mid-restore. This is
  // the exact bug class that previously bit Reverb (B-reverb re-entry).
  const std::string source = ReadPluginSource();
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");

  RequireContains(header, "bool mVolumTremoloRestoreInProgress = false;");
  RequireContains(source, "} guard(mVolumTremoloRestoreInProgress);");
  RequireContains(source, "_VolumSaveTremoloModeSnapshot(oldMode);");
  RequireContains(source, "_VolumRestoreTremoloModeSnapshot(newMode);");
  // The mode handler skips the save/restore while a POST restore is in flight.
  RequireContains(source, "mVolumEffectSettings.tremoloMode = newMode;");
}

TEST_CASE("PRE Pitch per-mode switch is guarded from snapshot re-entry")
{
  // PRE has no POST-style effect-settings struct, so the live per-mode snapshots
  // live on the plugin (mVolumPrePitchModes) and the mode-switch save/restore is
  // wrapped in its own re-entrancy guard.
  const std::string source = ReadPluginSource();
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");

  RequireContains(header, "bool mVolumPreRestoreInProgress = false;");
  RequireContains(header, "int mVolumPrePitchMode = volum::kVoLumPitchModeTranspose;");
  RequireContains(source, "} guard(mVolumPreRestoreInProgress);");
  RequireContains(source, "if (mVolumPreRestoreInProgress)");
  RequireContains(source, "_VolumSavePrePitchModeSnapshot(oldMode);");
  RequireContains(source, "_VolumRestorePrePitchModeSnapshot(newMode);");
}

TEST_CASE("Tremolo depth floor + Delay/Tremolo tempo sync are wired into the audio path")
{
  // The audible-depth-floor mapping and the tempo-sync time/rate derivations are
  // unit-tested as pure helpers; pin that the process block actually routes the
  // live params through them (and clamps the synced delay time) so the wiring
  // cannot silently regress to the raw knob value.
  const std::string source = ReadPluginSource();

  RequireContains(source, "volum::VoLumTremoloDepthKnobToInternal(GetParam(kTremoloDepth)->Value())");
  RequireContains(
    source, "std::clamp(volum::VoLumTremoloSyncMs(postBpm, GetParam(kDelayDivision)->Int()), 10.0, 2000.0)");
  RequireContains(source, "volum::VoLumTremoloSyncRateHz(postBpm, GetParam(kTremoloDivision)->Int())");
}

TEST_CASE("Chorus runs first in the POST chain and is wired to its own params")
{
  // Bus order is Chorus -> Delay -> Reverb -> Tremolo. Pin the ordering by source
  // position: if Chorus ever drifts behind Delay the repeats start smearing the
  // modulation, which is audible but easy to miss in a diff.
  const std::string post = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumProcessBlock.inc.cpp");
  const std::string plan = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumProcessingPlan.h");

  const auto chorus = post.find("mChorus.Process(postPointers");
  const auto delay = post.find("if (processingPlan.runDelay)");
  const auto reverb = post.find("if (processingPlan.runReverb)");
  REQUIRE(chorus != std::string::npos);
  REQUIRE(delay != std::string::npos);
  REQUIRE(reverb != std::string::npos);
  CHECK(chorus < delay);
  CHECK(delay < reverb);

  RequireContains(post, "GetParam(kChorusMode)->Int()");
  // Bypass edge and missing-model scrub must clear the line like Delay/Reverb do.
  RequireContains(post, "mChorus.Reset();");
  RequireContains(plan, "plan.runChorus = (haveMainModel || plan.runSupportModel) && chorusActive;");
}

TEST_CASE("Delay/Tremolo Sync toggles swap the free-running knob for a division stepper")
{
  // Engaging Sync replaces the Time/Rate knob with the tempo DIVISION stepper, so
  // both toggles must trigger a layout refresh, and the delay division stepper
  // must exist as a refreshed view.
  const std::string source = ReadPluginSource();
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");

  RequireContains(header, "class VoLumChannelStepControl* mVolumDelayDivStep = nullptr;");
  RequireContains(source, "case kDelaySync:");
  RequireContains(source, "for the tempo DIVISION");
}

TEST_CASE("PRE pedal capture menu toggles closed on second click of same pedal")
{
  const std::string source = ReadPluginSource(); // _VolumShowPreCaptureMenu now in VoLumSceneRig.inc.cpp
  // VoLumPreCaptureMenuControl moved to its own header on the 1.0 hygiene
  // split (see VoLumTriptych.h umbrella include).
  const std::string menus = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptychMenus.h");

  RequireContains(menus, "int GetSlot() const { return mSlot; }");
  RequireContains(source, "if (!rawCtrl->IsHidden() && menu && menu->GetSlot() == slot)");
  RequireContains(source, "_VolumHidePreCaptureMenu();");
}

TEST_CASE("PRE pedal capture menu closes from main-area outside click")
{
  const std::string source = ReadPluginSource(); // layout now in VoLumLayoutBuild.inc.cpp

  RequireContains(source, "_ClearVoLumKnobSelection();");
  RequireContains(source, "_VolumHidePreCaptureMenu();");
}

TEST_CASE("Collapsed AMP strip block is taller than the PRE/POST blocks")
{
  // AMP block is 180 H (vs 140 H for PRE/POST) so it visually anchors the row
  // when the user is in PRE or POST view. If somebody changes blockH back to
  // 140 the row loses its centerpiece, so lock the value here.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "void _DrawAmpStrip(IGraphics& g, const IRECT& r)");
  RequireContains(triptych, "const float blockH = 180.f;");
}

TEST_CASE("Collapsed AMP strip auto-shrinks the spine font to fit long amp names")
{
  // The amp name is rotated -90 deg and rendered as a single line. Long
  // names like "Diezel Herbert Mk1" only fit at smaller sizes, so the
  // strip MeasureText-probes a descending size table and picks the largest
  // size that fits. Locking the table prevents accidental regression to a
  // single fixed size that would clip long names.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "static const float kSpineSizes[] = {16.f, 14.f, 12.f, 11.f, 10.f, 9.f, 8.f};");
  RequireContains(triptych, "g.MeasureText(probe, name, measured);");
  RequireContains(triptych, "spineText.mAngle = -90.f;");
}

TEST_CASE("Collapsed AMP strip falls back to 'AMP' label when amp name is empty")
{
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "const char* name = mAmpName.empty() ? \"AMP\" : mAmpName.c_str();");
}

TEST_CASE("VoLum layer caches use the !g.CheckLayer idiom (re-render only when invalid)")
{
  // iPlug2 CheckLayer returns true when the layer is still valid (cache hit)
  // and false when it must be re-rendered. The standard idiom across the
  // iPlug2 codebase is `if (!g.CheckLayer(layer)) { ... rebuild ... }`.
  //
  // The inverted condition `if (... || g.CheckLayer(layer) || ...)` rebuilds
  // the layer every frame the cache is valid - the opposite of what was
  // intended. That bug caused dropped hover frames under CPU pressure and
  // broke the AMP/POST hover lift when COMP was focused (because COMP's own
  // pedal-card art layer was thrashing on the same pattern). This test
  // pins the correct idiom in place so future cleanup does not flip it back.
  //
  // NOTE: The hero-image art layer was intentionally removed when the Dual
  // Amp UX added per-frame overlays (DUAL chip + per-lane PAN dot). Caching
  // the hero art behind a layer made those overlays painful to keep in sync,
  // and the procedural fractal is cheap enough to redraw each frame, so the
  // hero now draws directly. This test no longer pins that specific cache.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");
  const std::string pedalCard = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPedalCardControl.h");
  // mIconLayers lives in VoLumAmpListControl which moved to VoLumAmpList.h on
  // the 1.0 hygiene split.
  const std::string ampList = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpList.h");
  const std::string coreControls = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCoreControls.h");

  RequireContains(triptych, "if (!g.CheckLayer(motifLayer)");
  RequireContains(
    pedalCard, "if (!g.CheckLayer(mArtLayer) || mCachedBypassed != bypassed || mCachedVariant != variant)");
  RequireContains(ampList, "if (!g.CheckLayer(mIconLayers[i]))");
  RequireDoesNotContain(triptych, "|| g.CheckLayer(");
  RequireDoesNotContain(pedalCard, "|| g.CheckLayer(");
  RequireDoesNotContain(ampList, "|| g.CheckLayer(");
  RequireDoesNotContain(coreControls, "|| g.CheckLayer(");
}

TEST_CASE("Amp-list scrollbar is draggable and keeps a gutter from the labels")
{
  // Regression: the sidebar scrollbar used to be draw-only (no OnMouseDown
  // scrollbar zone, no OnMouseDrag), so it could not be grabbed, and rows ran
  // right up to the bar. Pin the drag handler + the gutter so neither silently
  // regresses.
  const std::string ampList = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpList.h");
  RequireContains(ampList, "void OnMouseDrag(");
  RequireContains(ampList, "mDraggingScrollbar = true;");
  RequireContains(ampList, "kScrollGutter");
}

TEST_CASE("Cached thumbnails blit scale-invariant and invalidate on rescale (Q1)")
{
  // Regression B3: cached row/hero art layers must blit through DrawFittedLayer
  // (logical bounds, scale-invariant) NOT DrawFittedBitmap (pixel-width
  // denominator -> art rendered at the old resolution after a window resize),
  // and OnRescale() must null the cached layers so they re-render crisp at the
  // new backing scale. This exact invariant regressed once during 1.2.0, so it
  // is pinned with teeth before any refactor pass.
  const std::string ampList = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpList.h");
  const std::string hero = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumHero.h");

  // Scale-invariant blits for the cached sidebar thumbnails + custom art.
  RequireContains(ampList, "g.DrawFittedLayer(mIconLayers[i], iconArea, nullptr);");
  RequireContains(ampList, "g.DrawFittedLayer(mCustomArtLayers[art], iconArea, nullptr);");
  // The cached row thumbnails must never be blitted with DrawFittedBitmap.
  RequireDoesNotContain(ampList, "DrawFittedBitmap(mIconLayers");
  RequireDoesNotContain(ampList, "DrawFittedBitmap(mCustomArtLayers");

  // OnRescale invalidation in the sidebar list...
  RequireContains(ampList, "void OnRescale() override");
  RequireContains(ampList, "for (auto& l : mIconLayers)");
  RequireContains(ampList, "for (auto& l : mCustomArtLayers)");
  // ...and in the hero (mono + dual MAIN/SUPPORT art layers).
  RequireContains(hero, "void OnRescale() override");
  RequireContains(hero, "mMonoArtLayer = nullptr;");
  RequireContains(hero, "mMainArtLayer = nullptr;");
  RequireContains(hero, "mSupportArtLayer = nullptr;");
}

TEST_CASE("Custom overlay hover highlight is wired through mHoverAction (Q1/B12)")
{
  // Regression B12: the custom overlay's per-hotspot hover glow is driven by
  // mHoverAction. OnMouseOver sets it from the hovered hotspot, OnMouseOut
  // resets it to -1, and Draw gates the glow on mHoverAction >= 0. If any of
  // these three is dropped the hover affordance silently dies (a "missed
  // highlighting" class of bug). Pinned so the Phase 3 overlay decomposition
  // keeps the wiring intact.
  const std::string customUi = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomOverlay.h");
  RequireContains(customUi, "if (!mPopupOpen && mHoverAction >= 0)"); // Draw gate
  RequireContains(customUi, "mHoverAction = hoverAction;"); // OnMouseOver set
  RequireContains(customUi, "mHoverAction = -1;"); // OnMouseOut reset
}

TEST_CASE("Mode pickers route selection through the shared DrawVoLumSelection helper (Phase 2)")
{
  // Selection language SSOT: every mutually-exclusive mode control must draw its
  // active/hover chrome through DrawVoLumSelection (VoLumColorHelpers.h) instead
  // of hand-rolling an amber fill. This is the enforcement guard for the "missed
  // highlighting on a new mode picker" bug class - a new picker that forgets the
  // highlight will not call the helper and will fail this pin in review.
  const std::string colors = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumColorHelpers.h");
  const std::string core = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCoreControls.h");

  // The helper exists with an explicit style enum.
  RequireContains(colors, "enum class VoLumSelectionStyle");
  RequireContains(colors, "void DrawVoLumSelection(");
  RequireContains(colors, "AmberPicker");

  // Both amber mode controls route through it (picker = square, pill = rounded).
  RequireContains(core, "DrawVoLumSelection(g, itemArea, isSelected, static_cast<int>(i) == mHovered,");
  RequireContains(core, "DrawVoLumSelection(g, itemArea, isSelected, i == mHovered, VoLumSelectionStyle::AmberPicker,");
  // And neither hand-rolls the amber fill any more.
  RequireDoesNotContain(core, "g.FillRect(VoLumColors::AMBER, itemArea.GetPadded(-1.f));");
  RequireDoesNotContain(core, "g.FillRoundRect(VoLumColors::AMBER, itemArea.GetPadded(-1.5f), 3.f);");
}

TEST_CASE("Standalone settings persist the active preset id (Q1/B5)")
{
  // The active preset id round-trips through the standalone settings file:
  // written in _VolumSaveSettingsToFile and read back into mVolumRestorePresetId,
  // which OnUIOpen consumes via _VolumRestoreSessionSelection. Pin the write +
  // read sides so a settings refactor cannot silently drop preset restore.
  const std::string plugin = ReadPluginSource();
  RequireContains(plugin, "j[\"volumActivePresetId\"] = mVolumActivePresetId;");
  RequireContains(plugin, "mVolumRestorePresetId = j[\"volumActivePresetId\"].get<std::string>();");
}

TEST_CASE("VST3/AU reopen routes the chunk's custom amp + preset through the deferred restore")
{
  // The "VST3/AU reopen drops the focused custom amp" fix: UnserializeState must
  // seed the SAME deferred-restore members standalone uses (consumed by OnUIOpen
  // -> _VolumRestoreSessionSelection), sourcing them from the CHUNK id tail and
  // resetting the one-shot guard so the restore re-runs against the freshly built
  // UI. Without this the plugin re-applied the machine-global settings pick (or
  // nothing) and fell back to a factory amp. Pin the wiring so a future refactor
  // cannot silently drop it back to the immediate-select-only path.
  const std::string plugin = ReadPluginSource();
  RequireContains(plugin, "const volum::RestoreSelection restored = volum::ResolveRestoreSelection(");
  RequireContains(plugin, "mVolumRestoreCustomMainId = restored.customMainId;");
  RequireContains(plugin, "mVolumRestorePresetId = restored.activePresetId;");
  RequireContains(plugin, "mVolumDidRestorePresetSelection = false;");
  // Second stage: the editor-open consumer drops ids the content store cannot
  // resolve, so a deleted custom amp cannot leave an ownerless preset label.
  RequireContains(plugin, "volum::ValidateRestoreSelection(");
  RequireContains(plugin, "mVolumLastRecalledPlaySlot = idTail.lastPlaySlot;");
}

TEST_CASE("AMP rotated spine is drawn directly, not cached behind a layer")
{
  // Wrapping the rotated DrawText in StartLayer/EndLayer/DrawLayer caused
  // intermittently-empty spine bitmaps on some hover transitions (the layer
  // would draw with no glyphs visible while the header still rendered fine).
  // The rotated text is a single glyph-run draw - cheap enough that caching
  // is not needed, especially since the auto-shrink font size is already
  // cached via _ResolveSpineFontSize. Locking this in place prevents a
  // future "perf optimisation" from resurrecting the regression.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "g.DrawText(spineText, name, spineR);");
  RequireDoesNotContain(triptych, "mAmpSpineLayer");
}

TEST_CASE("Mini-pill toggle propagates the new value to peer controls bound to the same param")
{
  // _ToggleParam pushes the new value via SendParameterValueFromUI, but that
  // only notifies the host. Peer controls (e.g. the on/off switch in the
  // expanded POST view's knob row) keep their stale cached value unless we
  // explicitly call SetValueFromDelegate on them. Lock the peer refresh in
  // place so a future cleanup does not drop it and resurrect the bug where
  // toggling Reverb from AMP view leaves the POST switch visually OFF.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "void _ToggleParam(int paramIdx)");
  RequireContains(triptych, "gfx->ForControlWithParam(paramIdx,");
  RequireContains(triptych, "pControl->SetValueFromDelegate(normalized, v);");
}

TEST_CASE("Collapsed AMP strip hover is gated on the visible block, not the strip rect")
{
  // mAmpRect is the full 70 W x 196 H strip rect (used for click hit-testing
  // so clicks near the block still register). mAmpBlockRect is the visible
  // 70 W x 180 H block; the hover lift only fires when the cursor is over
  // the block so empty whitespace above/below does not light up.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "mAmpBlockRect = block;");
  RequireContains(triptych, "&& mAmpBlockRect.W() > 0");
  RequireContains(triptych, "&& mAmpBlockRect.Contains(x, y);");
}

TEST_CASE("Triptych shared layout keeps PRE AMP POST geometry aligned")
{
  const auto triptych = volum::triptych_layout::BoundsForCenter(450.f, 100.f);
  CHECK(triptych.L == doctest::Approx(107.f));
  CHECK(triptych.R == doctest::Approx(793.f));
  CHECK(triptych.H() == doctest::Approx(volum::triptych_layout::kTriptychH));

  const auto ampFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::AMP);
  CHECK(ampFrames.pre.L == doctest::Approx(107.f));
  CHECK(ampFrames.pre.R == doctest::Approx(223.f));
  CHECK(ampFrames.amp.L == doctest::Approx(233.f));
  CHECK(ampFrames.amp.R == doctest::Approx(667.f));
  CHECK(ampFrames.post.L == doctest::Approx(677.f));
  CHECK(ampFrames.post.R == doctest::Approx(793.f));

  const auto postFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::POST);
  CHECK(postFrames.pre.L == doctest::Approx(107.f));
  CHECK(postFrames.amp.L == doctest::Approx(233.f));
  CHECK(postFrames.amp.R == doctest::Approx(303.f));
  CHECK(postFrames.post.L == doctest::Approx(313.f));
  CHECK(postFrames.post.R == doctest::Approx(793.f));
}

TEST_CASE("Triptych shared layout keeps expanded pedal card geometry aligned")
{
  const auto triptych = volum::triptych_layout::BoundsForCenter(450.f, 100.f);
  const auto preFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::PRE);
  const auto preCards = volum::triptych_layout::ComputePreCards(preFrames.pre);

  CHECK(preCards.pitch.L == doctest::Approx(121.f));
  CHECK(preCards.pitch.R == doctest::Approx(208.74f));
  CHECK(preCards.comp.L == doctest::Approx(216.74f));
  CHECK(preCards.comp.R == doctest::Approx(304.48f));
  CHECK(preCards.nam1.L == doctest::Approx(312.48f));
  CHECK(preCards.nam2.R == doctest::Approx(573.f));
  CHECK(preCards.connector1.L == doctest::Approx(preCards.pitch.R));
  CHECK(preCards.connector1.R == doctest::Approx(preCards.comp.L));
  CHECK(preCards.connector2.L == doctest::Approx(preCards.comp.R));
  CHECK(preCards.connector2.R == doctest::Approx(preCards.nam1.L));
  CHECK(preCards.connector3.L == doctest::Approx(preCards.nam1.R));
  CHECK(preCards.connector3.R == doctest::Approx(preCards.nam2.L));

  const auto postFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::POST);
  const auto postCards = volum::triptych_layout::ComputePostCards(postFrames.post);
  // Four POST cards in bus order (Chorus -> Delay -> Reverb -> Tremolo). The span
  // is unchanged from the 3-card layout: only the per-card width shrank.
  CHECK(postCards.chorus.L == doctest::Approx(327.f));
  CHECK(postCards.chorus.R == doctest::Approx(432.5f));
  CHECK(postCards.delay.L == doctest::Approx(442.5f));
  CHECK(postCards.delay.R == doctest::Approx(548.f));
  CHECK(postCards.reverb.L == doctest::Approx(558.f));
  CHECK(postCards.reverb.R == doctest::Approx(663.5f));
  CHECK(postCards.tremolo.L == doctest::Approx(673.5f));
  CHECK(postCards.tremolo.R == doctest::Approx(779.f));
  CHECK(postCards.connector1.L == doctest::Approx(postCards.chorus.R));
  CHECK(postCards.connector1.R == doctest::Approx(postCards.delay.L));
  CHECK(postCards.connector2.L == doctest::Approx(postCards.delay.R));
  CHECK(postCards.connector2.R == doctest::Approx(postCards.reverb.L));
  CHECK(postCards.connector3.L == doctest::Approx(postCards.reverb.R));
  CHECK(postCards.connector3.R == doctest::Approx(postCards.tremolo.L));
}

TEST_CASE("PRE/POST lock UI and settings helpers are wired")
{
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");
  const std::string settings = ReadPluginSource();

  RequireContains(triptych, "_DrawLockIcon");
  RequireContains(triptych, "_DrawStoreToAmpIcon");
  RequireContains(triptych, "arrowStroke");
  RequireContains(triptych, "PathStroke");
  RequireContains(triptych, "PathCubicBezierTo");
  RequireContains(triptych, "_MeasureHeaderLabelWidth");
  RequireContains(triptych, "mHeaderTooltip");
  RequireContains(triptych, "_VolumSetPreLocked");
  RequireContains(triptych, "mPreLockRect");
  RequireContains(triptych, "mPreStoreRect");
  RequireContains(settings, "_VolumSavePreToSlot");
  RequireContains(settings, "_VolumSavePostToSlot");
  RequireContains(settings, "_VolumRestorePreFromSlot");
  RequireContains(settings, "_VolumStorePreToCurrentAmp");
  RequireContains(settings, "_VolumIsPreDirty");
  RequireContains(settings, "mVolumPreLocked");
  // Dirty checks compare the live locked block against the *active* scene, which is
  // the focused custom amp's scene when a custom amp is focused (factory amps map
  // back to mVolumAmpSettings[...] via _VolumActiveScene()). This is what makes
  // lock/unlock work on custom amps, not just factory ones.
  RequireContains(settings, "PreBlockEquals(mVolumLiveLockedPre, scene)");
  RequireContains(settings, "PostBlockEquals(mVolumLiveLockedPost, scene)");
  RequireContains(settings, "if (!mVolumPostLocked)");
  RequireContains(settings, "_VolumRestoreEffectSettings()");
}

TEST_CASE("SerializeState carries custom-amp scenes in the chunk, not the library")
{
  // 1.2.0 kept a focused custom amp's live knobs in the shared content library, so
  // SerializeState had to flush the whole store to keep them - and that write was
  // also how one instance's catalog save moved another instance's knobs.
  //
  // 1.3.0 (two-writer library): custom amps behave like factory amps. The scene
  // belongs to the instance and travels in the DAW chunk's id tail, so a host
  // state-save must NOT write the library at all.
  const std::string source = ReadPluginSource();

  // Single-line substrings only: file line endings differ across platforms (CRLF
  // on Windows checkouts, LF on macOS/Linux), so a multi-line "\r\n" match would
  // pass on Windows but fail on macOS CI.
  RequireContains(source, "const_cast<NeuralAmpModeler*>(this)->_VolumSaveCurrentToSettings();");
  RequireContains(source, "idTail.customScenes = mVolumCustomScenes;");
  RequireContains(source, "if (!idTail.customScenes.empty())");

  // The instance's scene map is the only home for a custom amp's live knobs. A
  // reference to the library's map here would be the shared-state bug returning.
  RequireDoesNotContain(source, "reg().customScenes");

  // The constructor must not re-read the file over a live sibling's unflushed
  // catalog: every plugin instance's constructor reaches the same global store.
  RequireContains(source, "volum::content::GlobalContentStore().EnsureLoaded();");
  RequireDoesNotContain(source, "volum::content::GlobalContentStore().Load();");
}

TEST_CASE("Deleting content that is playing moves the sounding rig, not just the list")
{
  // Before 1.3.0 a delete updated the catalog and part of the chrome and stopped:
  // nothing set mVolumNeedsLoad, so the audio thread kept running a capture whose
  // file had just been removed - factory amp in the sidebar, dead custom amp in
  // the speakers. Every delete path now plans the repair (VoLumRigRepair.h) before
  // the catalog mutation and applies it after.
  const std::string source = ReadPluginSource();
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomOverlay.h");

  // Sidebar bin (custom amps): plan -> confirm with the planned copy -> delete ->
  // repair. The copy has to be the planned one, or the dialog stops naming the
  // in-use case and the destination.
  RequireContains(source, "_VolumPlanLibraryDelete(volum::rig::LibraryKind::CustomAmp,");
  RequireContains(source, "dlg->As<VoLumConfirmDialogControl>()->Show(\"Delete?\", confirmBody, doDelete);");
  RequireDoesNotContain(source, "\"Delete custom amp \\\"\" + nm + \"\\\"? This cannot be undone.\"");
  RequireContains(source, "_VolumApplyPendingRigRepair();");

  // Manage panel (IRs, pedals, presets) goes through the same two callbacks.
  RequireContains(source, "return pPlugin->_VolumPlanLibraryDelete(kind, id, name);");
  RequireContains(source, "[pPlugin]() { pPlugin->_VolumApplyPendingRigRepair(); });");
  RequireContains(overlay, "mPlanRigRepair(RigKind(), id, nm)");
  RequireContains(overlay, "if (mApplyRigRepair)");

  // The MAIN fallback is a real amp selection - scene restore plus a capture
  // reload - not a chrome update. And it must not fold the deleted amp's knobs
  // into the factory slot it is reverting to.
  RequireContains(source, "void NeuralAmpModeler::_VolumSelectFactoryAmp(int ampIdx, bool snapshotOutgoing)");
  RequireContains(source, "if (snapshotOutgoing)");
  RequireContains(source, "_VolumSelectFactoryAmp(mVolumAmpIdx, /*snapshotOutgoing=*/false);");
  RequireContains(source, "[this](int ampIdx) { _VolumSelectFactoryAmp(ampIdx); }");

  // The IR teardown is deferred so the lane does not expose a burst of raw,
  // cab-less amp while the baked-cab capture loads (VoLumDspStaging.h).
  RequireContains(source, "_VolumClearIR(false, true);");
  RequireContains(source, "_VolumClearIR(true, true);");

  // A dropped PRE slot drops the live model too: capture EMPTY, pill off.
  RequireContains(source, "_VolumSetPreNamCapture(0, 0); // EMPTY: drops the live PRE model");
  RequireContains(source, "_VolumSetPreNamCapture(1, 0);");

  // SUPPORT drops to "(none)" through the ordinary path, which clears the custom
  // partner as well as the factory reference.
  RequireContains(source, "_VolumSetSupportAmp(-1); // \"(none)\": clears the custom partner too");
}

TEST_CASE("IR staging passes the UTF-8 path into ImpulseResponse")
{
  // AudioDSPTools #25: filesystem::path::string() is the ANSI code page on
  // Windows. _StageIR must hand irPath.Get() (UTF-8) to ImpulseResponse.
  const std::string source = ReadPluginSource();
  RequireContains(source, "stagedIR = std::make_unique<dsp::ImpulseResponse>(irPath.Get(), sampleRate);");
  RequireDoesNotContain(source, "irPathU8.string()");
}

TEST_CASE("PRE/POST lock header layout keeps store icon gated and amp-facing")
{
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "const float storeW = (locked && dirty) ? iconSize : 0.f;");
  RequireContains(triptych, "const float groupRight = showChevron ? outChevron.L - 3.f : header.R - 2.f;");
  RequireContains(triptych, "float x = groupRight - groupW;");
  RequireContains(triptych, "if (!isPre && storeW > 0.f)");
  RequireContains(triptych, "if (isPre && storeW > 0.f)");
  RequireContains(triptych, "Unlock PRE (restore this amp's saved scene)");
  RequireContains(triptych, "Unlock POST (restore this amp's saved scene)");
  RequireContains(triptych, "Store PRE to ");
  RequireContains(triptych, "Store POST to ");
}

TEST_CASE("A chunk truncated inside the per-amp tail leaves every scene alone")
{
  // The tail is fifteen scenes, the lock flags and the lock snapshots. A failed read
  // spares the numbers but still writes every boolean from a local carrying the
  // decoder's default (see the codec test), so one truncation used to reset the
  // toggles of all fifteen amps, and the instance then saved them. Rejecting the
  // load instead is not available here: the same version predicate matches upstream
  // NAM projects, which have no VoLum tail at all.
  const std::string source = ReadPluginSource();

  RequireContains(source, "auto pendingAmpSettings = mVolumAmpSettings;");
  RequireContains(source, "auto& s = pendingAmpSettings[i];");
  RequireContains(source, "const bool perAmpTailComplete = haveSelection && pos >= 0;");
  RequireContains(source, "mVolumAmpSettings = pendingAmpSettings;");
  // The decoders must never see the live members again.
  RequireDoesNotContain(source, "auto& s = mVolumAmpSettings[i];");
  RequireDoesNotContain(source, "GetPrePostLockFlags(chunk, pos, mVolumPreLocked, mVolumPostLocked)");
  RequireDoesNotContain(source, "chunk, pos, mVolumPreLocked, mVolumPostLocked, mVolumLiveLockedPre");

  // The selection is part of the same commit: it used to be assigned straight out
  // of a read that may have failed.
  RequireContains(source, "const int selectionEnd = volum::GetVoLumChunkSelection(chunk, pos, selection);");
  RequireContains(source, "const bool haveSelection = selectionEnd >= 0;");
  RequireDoesNotContain(source, "pos = volum::GetVoLumChunkSelection(chunk, pos, selection);");

  // And there is no id tail behind a tail that did not read.
  RequireContains(source, "perAmpTailComplete && volum::TryGetChunkIdTail(");

  // The headerless legacy path applied its config even when the read failed.
  RequireContains(source, "rejecting truncated headerless chunk");
}

TEST_CASE("VoLum loader queue coalesces duplicate support and PRE requests")
{
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");

  RequireContains(loader, "_VolumDropQueuedLoadRequests");
  RequireContains(loader, "return queued.kind == VoLumLoadKind::Support;");
  RequireContains(loader, "return queued.kind == VoLumLoadKind::Pre && queued.slot == slot;");
}

TEST_CASE("VoLum NAM loaders are owned and publish through DSP staging")
{
  // The loader thread + queue helpers moved to VoLumLoader.inc.cpp on the 1.0
  // hygiene split. ReadPluginSource() aggregates the whole TU.
  const std::string source = ReadPluginSource();
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");

  RequireContains(
    source, "volum::dsp_staging::CopyPathNoAlloc(pendingPath, volum::dsp_staging::kRtPathCapacity, irPath.Get());");
  RequireContains(source, "volum::dsp_staging::ApplyPublishedPath(lane.action, lane.text, lane.paths);");
  RequireContains(
    source,
    "volum::dsp_staging::CopyPathNoAlloc(mPendingNamPath, volum::dsp_staging::kRtPathCapacity, modelPath.Get());");
  RequireContains(source, "_VolumProcessMainAmpChain");
  RequireContains(source, "_VolumProcessDualAmpSupportLane");
  RequireContains(loader, "std::lock_guard<std::mutex> lock(mStagingMutex);");
  RequireContains(
    loader,
    "volum::dsp_staging::CopyPathNoAlloc(mPendingNamPath, volum::dsp_staging::kRtPathCapacity, result.path.c_str());");
  RequireContains(header, "volum::dsp_staging::WdlStagedPathPair mNAMPaths;");
  RequireContains(header, "void _VolumDropQueuedLoadRequests(Pred pred)");
  RequireDoesNotContain(source, ".detach()");
  RequireContains(header, "std::thread mVolumLoaderThread;");
  RequireContains(source, "_VolumStopLoader();");
  RequireContains(source, "_VolumDrainLoaderResults();");
  RequireContains(source, "mVolumLoadResults.push_back(std::move(result));");
}

TEST_CASE("VoLum NAM cache copies dspData before Core consumes cached fields")
{
  const std::string source = ReadPluginSource();

  RequireContains(source, "nam::dspData cachedConfig = cacheIt->second;");
  RequireContains(source, "return nam::get_dsp(cachedConfig);");
  RequireDoesNotContain(source, "return nam::get_dsp(cacheIt->second);");
}

TEST_CASE("Switching from a custom IR to a baked cab keeps convolving until the swap")
{
  // 1.2.1 deferred the convolver teardown but left _VolumClearIR turning the IR
  // toggle off immediately, and the audio thread gates convolution on that toggle -
  // so the cab-less burst survived the fix and was reported again. The gate must
  // read the deferral, and the shaping reset must wait with it or a shaped IR drops
  // to unity mid-note. The timing itself is unit-tested in test_volum_dsp_staging.
  const std::string source = ReadPluginSource();
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");

  RequireContains(source, "volum::dsp_staging::IrConvolutionActive(");
  RequireContains(source, "mVolumDeferredRemoveIR.load(std::memory_order_relaxed))");
  RequireContains(source, "mVolumDeferredRemoveSupportIR.load(std::memory_order_relaxed))");
  // The raw param must not reach the plan again; that is exactly the regression.
  RequireDoesNotContain(source, "toneStackActive, GetParam(kIRToggle)->Value()");

  RequireContains(header, "bool mVolumIrShapingPushPending[2]{false, false};");
  RequireContains(source, "mVolumIrShapingPushPending[lane] = deferToCabSwap;");
  RequireContains(source, "void NeuralAmpModeler::_VolumFlushDeferredIrShaping()");
  RequireContains(source, "_VolumFlushDeferredIrShaping();");

  // Choosing an IR again during the wait cancels the swap: the pending removal
  // clears mStagedIR too, so firing it afterwards would discard the new IR.
  const auto selectPos = source.find("void NeuralAmpModeler::_VolumSelectIR(int irIdx");
  REQUIRE(selectPos != std::string::npos);
  const auto cancelPos = source.find("mVolumDeferredRemoveIR).store(false);", selectPos);
  REQUIRE(cancelPos != std::string::npos);
  CHECK(cancelPos < source.find("_VolumForceDirectCapture(support);", selectPos));
}

TEST_CASE("Switching from a baked cab to a custom IR waits for the DIRECT capture")
{
  // The mirror image of the gap above, reported as a volume jump: the IR applied on
  // the next block while the baked-cab capture it replaces was still live, so the
  // lane briefly ran cab plus IR. The staged IR is held until its capture is staged.
  const std::string source = ReadPluginSource();
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");

  RequireContains(header, "std::atomic<bool> mVolumDeferredApplyIR = false;");
  RequireContains(header, "std::atomic<bool> mVolumDeferredApplySupportIR = false;");
  RequireContains(source, "const bool captureLoading = _VolumForceDirectCapture(support);");
  RequireContains(source, "mVolumDeferredApplySupportIR : mVolumDeferredApplyIR).store(captureLoading);");
  // The hold is what keeps the staged IR parked for those blocks.
  RequireContains(source, "if (mStagedIR != nullptr && !holdMainIr)");
  RequireContains(source, "if (mStagedSupportIR != nullptr && !holdSupportIr)");
  RequireContains(source, "_VolumStepDeferredIrSwaps(holdMainIr, holdSupportIr);");
  // Only wait when a capture is actually on its way; _VolumForceDirectCapture says so.
  RequireContains(source, "bool NeuralAmpModeler::_VolumForceDirectCapture(bool support)");
  RequireContains(source, "return !alreadyDirect;");
}

TEST_CASE("Picking No Cab while an IR is active switches at once instead of waiting")
{
  // An active IR already holds the lane on DIRECT, so No Cab reuses the live capture
  // and nothing is staged. Deferring there held the IR for the whole bounded wait,
  // reported as "from custom IR to no cab takes forever".
  const std::string source = ReadPluginSource();

  RequireContains(source, "const bool captureChanges =");
  RequireContains(source, "_VolumClearIR(supportFocus, /*deferToCabSwap=*/captureChanges);");
}

TEST_CASE("VoLum settings panel reports round-trip latency, not just plugin PDC")
{
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

  // The wording and arithmetic live in the pure header (test_volum_latency_report);
  // here we pin that the control renders it and the plugin feeds it real device data.
  RequireContains(controls, "SetCurrentLatency(const volum::LatencyReport& report)");
  RequireContains(controls, "volum::FormatLatencyLines(report, kStandalone)");
  RequireContains(source, "host->GetStreamLatencyFrames()");
  RequireContains(source, "host->GetIOBufferSize()");
  // The old PDC-only line read 0.0 ms while the player heard 21 ms of ASIO.
  RequireDoesNotContain(controls, "Current latency: %.1f ms (%d samples)");
  RequireDoesNotContain(source, " |  Latency:");

  // iPlug2's standalone host calls OnReset() before openStream(), so a report taken
  // only there can never see the driver's latency and the line silently degrades to
  // "driver reports none" after every audio-settings change. The OnIdle poll is what
  // makes it correct itself; without it the readout is stale exactly when looked at.
  RequireContains(source, "void NeuralAmpModeler::_VolumRefreshLatencyReport(bool force)");
  RequireContains(source, "_VolumRefreshLatencyReport();");
  RequireContains(source, "_VolumRefreshLatencyReport(/*force=*/true);");

  // Both rows must exist, or the caveat line has nowhere to render and the readout
  // clips mid-sentence like it did in 1.2.1.
  RequireContains(controls, "mControlNames.latencyDetail");
}

TEST_CASE("Custom SUPPORT cab/channel + IR-direct gate + amp-name helper are wired")
{
  const std::string source = ReadPluginSource();
  const std::string catalog = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpeteCatalog.h");
  const std::string speakerRow = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSpeakerRow.h");

  // A: custom SUPPORT cab/channel persist onto the active scene.
  RequireContains(catalog, "int supportCustomSlot");
  RequireContains(source, "_VolumActiveScene().supportCustomSlot = mVolumCustomSupportSlot;");
  RequireContains(source, "s.supportCustomSlot = mVolumCustomSupportSlot;");

  // B/C: a single helper names the MAIN amp (custom or factory) for the spine,
  // sub-row, and preset-manage subtitle.
  RequireContains(source, "const char* NeuralAmpModeler::_VolumMainAmpDisplayName() const");
  RequireContains(source, "_VolumMainAmpDisplayName(),"); // triptych SetState
  RequireContains(source, "pPlugin->_VolumMainAmpDisplayName());"); // manage-presets subtitle

  // D: a custom IR needs a DIRECT capture; the row greys out and selection is
  // hard-blocked otherwise. Channel-first: the gate is now per-channel (the IR
  // enable comes from the resolved plan; selection checks ChannelHasDirect).
  RequireContains(speakerRow, "void SetIrEnabled(bool enabled");
  RequireContains(source, "row->SetIrEnabled(plan.irEnabled");
  RequireContains(source, "if (!volum::custom::ChannelHasDirect(amp, laneChannel))");
}

TEST_CASE("Custom cab navigation is channel-first")
{
  const std::string source = ReadPluginSource();
  const std::string speakerRow = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSpeakerRow.h");
  const std::string syncPlan = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumUiSyncPlan.h");

  // The speaker row can gate the No Cab button (per-channel DIRECT availability).
  RequireContains(speakerRow, "void SetNoCabEnabled(bool enabled");
  // Cab refresh runs through the pure channel-first resolver, now reached via the
  // shared UI sync planner rather than open-coded in the plugin.
  RequireContains(syncPlan, "custom::ResolveLaneCabs(amp, in.customSlot, channel)");
  RequireContains(source, "row->SetNoCabEnabled(plan.noCabEnabled");
  RequireContains(source, "row->SetIrEnabled(plan.irEnabled");
  // The channel stepper lists the amp-WIDE gain stages.
  RequireContains(source, "const auto channels = volum::custom::AssignedChannels(amp);");
  // Custom IR selection is gated per-channel, not amp-wide.
  RequireContains(source, "volum::custom::ChannelHasDirect(amp, laneChannel)");
}

TEST_CASE("Editor reopen re-derives the whole visible selection from backend state")
{
  // Regression (1.2.1): closing and reopening the window with a custom IR active
  // showed "No Cab". The editor rebuilds every control from constructor defaults,
  // and the old reopen path only pushed SetSelected(speakerIdx) - which an IR had
  // forced to 0 - so nothing restored the copper IR chip. Custom amps escaped it
  // only because their restore happened to run the full cab-resolve path.
  const std::string source = ReadPluginSource();

  // One entry point, called once the whole editor exists.
  RequireContains(source, "void NeuralAmpModeler::_VolumSyncUiFromState()");
  RequireContains(source, "_VolumSyncUiFromState();");

  // The layout build must NOT push a partial selection of its own; that split
  // between "build" and "apply" is what let the IR chip go missing. Matched without
  // the receiver name: as written with `spkRow->` this pin sat next to a live
  // `spkCtrl->` copy in _VolumApplyAmpSettings and never saw it.
  RequireDoesNotContain(source, "SetSelected(mVolumSpeakerIdx);");

  // Both lanes resolve through the same pure planner.
  RequireContains(source, "volum::MakeUiSyncPlan(_VolumMakeUiSyncInput(supportFocus, unusedAmp))");
  RequireContains(source, "volum::MakeUiSyncPlan(_VolumMakeUiSyncInput(supportLane, amp))");
}

TEST_CASE("Loading DAW state into an open editor re-derives the visible selection")
{
  // Regression (1.2.1): the chunk reader only pushed individual params, on the
  // assumption that the editor is built after state arrives. Hosts also load state
  // into a window that is already open - reopening a project with the plug-in window
  // up, undo, switching host presets - and that left the cab row, IR chip and amp
  // list describing the rig the chunk had just replaced. Clicking any of them then
  // committed the stale reading back over the restored state.
  const std::string unserialize = ReadText(RepoRoot() / "NeuralAmpModeler" / "Unserialization.cpp");

  const auto fnPos = unserialize.find("int NeuralAmpModeler::_UnserializeStateWithKnownVersion(");
  REQUIRE(fnPos != std::string::npos);
  const auto fnEnd = unserialize.find("\n}", fnPos);
  REQUIRE(fnEnd != std::string::npos);
  const std::string body = unserialize.substr(fnPos, fnEnd - fnPos);

  INFO("chunk restore must end by requesting a re-derive of the whole visible selection");
  CHECK(body.find("mVolumUiSyncPending.store(true);") != std::string::npos);

  // But it must not run the applier itself. UnserializeState is called on the host's
  // thread; the applier writes IGraphics controls and can rescan a rig directory into
  // shared channel vectors, so running it there races the editor's own drawing and
  // input. The request crosses to OnIdle, which is the UI-thread pump.
  CHECK(body.find("_VolumSyncUiFromState();") == std::string::npos);

  const std::string source = ReadPluginSource();
  RequireContains(source, "if (GetUI() && mVolumUiSyncPending.exchange(false))");
  const auto idlePos = source.find("void NeuralAmpModeler::OnIdle()");
  REQUIRE(idlePos != std::string::npos);
  const auto consume = source.find("mVolumUiSyncPending.exchange(false)", idlePos);
  REQUIRE(consume != std::string::npos);
  CHECK(consume - idlePos < 400);

  // A request that arrives with the window shut is satisfied by the open path, which
  // runs the same applier - so the flag is cleared there rather than left to fire a
  // second, redundant sync on the first idle.
  const auto openPos = source.find("void NeuralAmpModeler::OnUIOpen()");
  REQUIRE(openPos != std::string::npos);
  const auto clear = source.find("mVolumUiSyncPending.store(false);", openPos);
  REQUIRE(clear != std::string::npos);
  CHECK(clear < source.find("\n}", openPos));
}

TEST_CASE("Forcing DIRECT for a custom IR reads the persisted channel position, not the runtime cache")
{
  // Regression (1.2.1): a custom amp saved on gain stage 5 with an active IR came
  // back on stage 1 after an app restart. _VolumForceDirectCapture read
  // mVolumCustomMainChannel, a runtime cache still at its default (1) during a
  // restore, instead of deriving the stage from the persisted stepper position.
  const std::string source = ReadPluginSource();

  // The same stale read survived one function up, in _VolumSelectIR's DIRECT gate,
  // which every restore path reaches first - and that one does not merely pick the
  // wrong stage, it clears the scene's IR id and lets the next save write the
  // clearing out. So the pin covers both functions, not just the one that was fixed.
  auto bodyOf = [&source](const char* signature) {
    const auto fnPos = source.find(signature);
    REQUIRE(fnPos != std::string::npos);
    const auto fnEnd = source.find("\n}", fnPos);
    REQUIRE(fnEnd != std::string::npos);
    return source.substr(fnPos, fnEnd - fnPos);
  };

  for (const char* fn : {"bool NeuralAmpModeler::_VolumForceDirectCapture(",
                         "void NeuralAmpModeler::_VolumSelectIR(int irIdx, bool support, bool interactive)"})
  {
    const std::string body = bodyOf(fn);
    INFO("MAIN must derive its gain stage from the persisted stepper position: " << fn);
    CHECK(body.find("volum::CustomChannelAtStep(amp, mVolumChannelIdx)") != std::string::npos);
    INFO("reading the runtime gain-stage cache for MAIN is the bug being pinned out: " << fn);
    CHECK(body.find(": mVolumCustomMainChannel;") == std::string::npos);
  }
}

TEST_CASE("Reopen restores the dirty baseline from preset content")
{
  // Funnel C: both the standalone session-restore and the DAW chunk-restore
  // paths must seed the recalled snapshot from the preset bank entry's stored
  // settings (pr.settings), not from the just-restored live scene.
  const std::string source = ReadPluginSource();
  size_t count = 0;
  size_t pos = source.find("mVolumRecalledSnapshot = pr.settings;");
  while (pos != std::string::npos)
  {
    ++count;
    pos = source.find("mVolumRecalledSnapshot = pr.settings;", pos + 1);
  }
  INFO("expected the preset-content baseline in both restore paths");
  CHECK(count >= 2);
}

TEST_CASE("Preset recall refreshes the focused custom amp's cabs, not the factory rig")
{
  // Regression: recalling a preset while a custom amp is focused used to call the
  // factory _VolumRefreshChannels() unconditionally. That rescans the underlying
  // factory rig folder, clobbers the custom cab row / channel stepper, and leaves
  // mVolumCustomMainSlot/Channel stale so the wrong .nam loads - reproduced by
  // custom preset A -> factory amp -> back to custom -> recall preset B. The recall
  // path must mirror _VolumSelectCustomAmp: use _VolumApplyCustomMainCabs when a
  // custom amp is focused.
  const std::string source = ReadPluginSource();

  const auto fnPos = source.find("void NeuralAmpModeler::_VolumApplyRecalledPreset(");
  REQUIRE(fnPos != std::string::npos);
  const auto fnEnd = source.find("\n}", fnPos);
  REQUIRE(fnEnd != std::string::npos);
  const std::string body = source.substr(fnPos, fnEnd - fnPos);

  INFO("recall must branch on the focused custom main amp");
  CHECK(body.find("if (mVolumCustomMainIdx >= 0)") != std::string::npos);
  CHECK(body.find("_VolumApplyCustomMainCabs(mVolumCustomMainIdx, false)") != std::string::npos);
  // The factory refresh must still be the else branch for factory amps.
  CHECK(body.find("_VolumRefreshChannels();") != std::string::npos);
  // A focused custom SUPPORT amp is refreshed too so its cab chip tracks the preset.
  CHECK(body.find("_VolumApplyCustomMainCabs(mVolumCustomSupportIdx, true)") != std::string::npos);
}

TEST_CASE("Destructive confirmations act on the item they named, not on a row number")
{
  // The prompt captured the row and re-applied it when the user confirmed. Another
  // editor removing an earlier row in the meantime made that row belong to a
  // different item, so "Delete Foo" deleted Bar - irreversibly, and against exactly
  // the promise the prompt makes.
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomOverlay.h");

  RequireContains(overlay, "std::string RowIdAt(int idx) const");
  RequireContains(overlay, "int RowIndexById(const std::string& id) const");
  // Both destructive confirmations resolve at confirm time and bail out by name.
  RequireContains(overlay, "const int now = volum::ResolveConfirmRowIndex(id, idx, RowIndexById(id));");
  RequireContains(overlay, "is no longer in your library.");
  RequireContains(overlay, "ApplyDelete(now);");
  RequireContains(overlay, "mOverwritePreset(now);");
  // And the stale-index calls are gone.
  RequireDoesNotContain(overlay, "ApplyDelete(idx);");
  RequireDoesNotContain(overlay, "mOverwritePreset(idx);");

  // Rename too. It was left on the row index while delete and overwrite moved to
  // identity, and it is the operation whose field stays open longest - the whole
  // time the user is typing - so it has the widest window for another editor to
  // shift the rows underneath it.
  RequireContains(overlay, "mRenameId = RowIdAt(mSel);");
  RequireContains(
    overlay, "const int target = volum::ResolveConfirmRowIndex(mRenameId, mSel, RowIndexById(mRenameId));");
  RequireContains(overlay, "ApplyRename(target, s);");
  RequireContains(overlay, "NameTaken(s, target)");
  RequireDoesNotContain(overlay, "ApplyRename(mSel, s);");
  RequireDoesNotContain(overlay, "NameTaken(s, mSel)");

  // Saving an open amp builder re-resolves its target the same way: a deletion
  // before that index would otherwise make UpdateCustomAmp adopt another amp's id
  // and overwrite its record with this draft.
  const std::string source = ReadPluginSource();
  RequireContains(source, "editIdx = volum::custom::CustomAmpIndexById(ampIn.id);");
  RequireContains(source, "return \"Save failed: this amp is no longer in your library\";");
}

TEST_CASE("Every preset operation names the owner of the bank it acts on")
{
  // The capture/apply hooks and the active owner key are process-global, so the
  // instance that last installed them decides whose rig a preset records and whose
  // rig a recall changes. Installing at construction handed that to whichever
  // instance the host created last; re-claiming per operation fixed the hooks.
  //
  // 1.3.0 (two-writer library) finishes the job for the *bank*: the owner key is a
  // parameter, not ambient state. Reading it back out of a global still meant a
  // second editor switching amps mid-operation could redirect the first editor's
  // read, and nothing in the code said which bank was intended.
  const std::string source = ReadPluginSource();

  RequireContains(source, "std::string NeuralAmpModeler::_VolumClaimPresetOps()");
  RequireContains(source, "volum::custom::PresetHookOwner() = this;");
  RequireContains(source, "volum::custom::ClearPresetHooksIfOwnedBy(this);");

  auto count = [&source](const char* needle) {
    const std::string n(needle);
    std::size_t total = 0;
    for (std::size_t pos = source.find(n); pos != std::string::npos; pos = source.find(n, pos + n.size()))
      ++total;
    return total;
  };

  // No production path resolves a preset bank through the ambient key. The legacy
  // index-based signatures still exist for the bridge's own tests; the plugin does
  // not use them.
  RequireDoesNotContain(source, "MockPresetsForAmp");
  RequireDoesNotContain(source, "volum::custom::AddPreset(");
  RequireDoesNotContain(source, "volum::custom::OverwritePreset(");
  RequireDoesNotContain(source, "volum::custom::RecallPreset(");
  RequireDoesNotContain(source, "volum::custom::PresetIdAt(");

  // Every write names its owner.
  RequireContains(source, "volum::custom::AddPresetForOwner(_VolumActiveOwnerKey(), name)");
  RequireContains(source, "volum::custom::OverwritePresetForOwner(_VolumActiveOwnerKey(), index)");
  RequireContains(source, "volum::custom::RecallPresetForOwner(_VolumActiveOwnerKey(), index)");

  // ... and so does every read. Five: the preset bar, the preset menu, the two
  // callbacks that bounds-check a chosen row before recalling it, and Ctrl+S.
  CHECK(count("volum::custom::PresetsForOwner(") == 5);

  // The overlay gets a key supplier, not a bare "claim" it cannot inspect, so its
  // rename/delete (which bypass the plugin) act on this instance's bank.
  RequireContains(source, "[pPlugin]() { return pPlugin->_VolumClaimPresetOps(); }");

  // Two owner-key publishes: one inside the claim helper (for the legacy bridge
  // signatures) and one on amp switch. A third inside an operation would look like
  // a claim while leaving the hooks pointing at another instance.
  CHECK(count("volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());") == 2);
}

TEST_CASE("A double-click the overlay did not begin cannot run a row's action")
{
  // Windows delivers down, up, dblclick, up. The confirmation modal acts and hides
  // on the down, so the dblclick that follows was hit-tested again and landed on the
  // Manage overlay underneath - at the default window size, on row five. Confirming
  // a delete with a double-click therefore also recalled an unrelated preset (or
  // selected an unrelated IR, or loaded an unrelated pedal) and closed Manage.
  //
  // The overlay needs a real graphics host to instantiate, so this pins the wiring:
  // the gesture flag is set on mouse-down and required by the double-click handler
  // before it does anything.
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomOverlay.h");

  RequireContains(
    overlay, "const bool ownsGesture = mOwnsGesture && mGestureConfirmEpoch == VoLumConfirmClickEpoch();");
  RequireContains(overlay, "if (!ownsGesture)");

  // It BEGINS with a mouse-down the overlay's own panel received, and the modal's
  // click counter is sampled with it.
  RequireContains(overlay, "mOwnsGesture = PanelRect().Contains(x, y);");
  RequireContains(overlay, "mGestureConfirmEpoch = VoLumConfirmClickEpoch();");
  RequireDoesNotContain(overlay, "mOwnsGesture = true;");

  // What it must NOT do is clear the claim on mouse-up. That was the second attempt
  // at this fix and it rejected every legitimate double-click as well: Windows sends
  // the up BEFORE the dblclick, so by the time the handler ran the claim from the
  // matching down was already gone, and double-clicking a Manage row did nothing.
  // The overlay's own events are identical in both cases; only the modal's counter
  // distinguishes them.
  RequireDoesNotContain(overlay, "void OnMouseUp(float, float, const IMouseMod&) override { mOwnsGesture = false; }");

  // The counter only moves where it should: the modal bumps it on the click it
  // consumes, nowhere else.
  const std::string dialog = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumConfirmDialog.h");
  RequireContains(dialog, "++VoLumConfirmClickEpoch();");
  const auto bump = dialog.find("++VoLumConfirmClickEpoch();");
  const auto down = dialog.find("void OnMouseDown(float x, float y, const IMouseMod&) override");
  REQUIRE(down != std::string::npos);
  CHECK(bump > down);

  // The guard has to come before the row dispatch, or it guards nothing.
  const auto guard = overlay.find("if (!ownsGesture)");
  const auto rowDispatch = overlay.find("mPrimaryAction(mManageKind, mAmpIdx, mPedalSlot, idx);");
  REQUIRE(guard != std::string::npos);
  REQUIRE(rowDispatch != std::string::npos);
  CHECK(guard < rowDispatch);
}

TEST_CASE("A keyboard-selected knob that a mode switch hid consumes the key that drops it")
{
  // The bail-out returned false, meaning "not handled", so the chain below it ran:
  // Up/Down is amp navigation and Left/Right steps the channel. Both stage a model
  // load and are immediately audible, which is a worse outcome than the silent edit
  // of an off-screen knob that the bail-out was added to stop. The Left/Right guard
  // could not save it either, because it tests the very selection just cleared.
  const std::string keyboard = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboard.inc.cpp");

  const auto hidden = keyboard.find("if (pControl->IsHidden())");
  REQUIRE(hidden != std::string::npos);
  const auto blockEnd = keyboard.find("}", keyboard.find("_UpdateVoLumKeyboardFocusHint();", hidden));
  REQUIRE(blockEnd != std::string::npos);
  const std::string bail = keyboard.substr(hidden, blockEnd - hidden);
  RequireContains(bail, "return true;");
  RequireDoesNotContain(bail, "return false;");

  // And it drops the whole selection rather than two of its four fields, or the knob
  // keeps its keyboard ring - coming back looking selected while no longer answering
  // arrows - and an open exact-value box is left with nothing driving it.
  RequireContains(bail, "_ClearVoLumKnobSelection();");

  // Consuming the key is right for the arrows and wrong for the two keys the focus
  // handler suppresses while a knob is selected. It ran before this one and declined
  // them on behalf of the selection just dropped, so eating them here as well costs
  // the user a press: Enter would not open the knob that replaced the hidden one, and
  // the on/off key would not toggle the focused block.
  RequireContains(bail, "return _ActivateVoLumKeyboardTarget();");
  RequireContains(bail, "return _ToggleVoLumKeyboardTarget();");
  RequireContains(bail, "key.VK == kVK_RETURN");
}

TEST_CASE("Clamping focus off an empty SUPPORT lane re-derives the row it invalidates")
{
  // The cab row is one control shared by both lanes and every write to it is now
  // conditioned on which lane is focused. A clamp that only flipped the flag left the
  // row describing SUPPORT while MAIN was focused - the exact state that guard exists
  // to prevent - and a click on a cab then edited MAIN with an index belonging to the
  // support amp's layout. The decision lives in CommitFocus; clamp installs it.
  const std::string source = ReadPluginSource();

  // The clamp is shared with the hero's click protocol. Both used to be written
  // separately and disagreed - the hero demanded SUPPORT focus before it would
  // open the picker, this refused focus to a lane with no amp, and the empty
  // lane became unfillable (v1.2.3). Behaviour lives in VoLumDualAmpInput.h now
  // so one test can drive the whole round trip (test_volum_dual_amp_input.cpp).
  // The clamp installs its verdict through CommitFocus, which calls
  // ClampSupportFocus and additionally reports whether the shared cab row has to
  // be re-derived - focus and that rederive travel together so a path cannot
  // take one without the other.
  const std::string body = MemberFnUntilNext(source, "void NeuralAmpModeler::_VolumClampSupportFocus(");
  RequireContains(body, "volum::dualamp::CommitFocus(");
  RequireContains(body, "volum::dualamp::ApplyFocusCommit(");
  RequireContains(body, "_VolumApplyFocusedLaneCabs();");

  // The hero must not re-derive the protocol locally; that divergence is the bug.
  const std::string hero = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumHero.h");
  RequireContains(hero, "volum::dualamp::DecideHeroClick(state, hitDualChip, hitSupportHalf)");
  // Both platforms deliver the second click of a fast double-click as
  // OnMouseDblClick, so a two-click protocol is unreachable without this.
  RequireContains(hero, "mDblAsSingleClick = true;");

  // A lane whose amp the library no longer contains is not a lane either: a custom
  // support amp deleted from another instance left a stale index behind.
  RequireContains(source, "mVolumCustomSupportIdx < static_cast<int>(volum::custom::MockCustomAmps().size())");

  // The layout pass reads the focus flag twice - once to choose between the two amp
  // knob groups, once to choose between the two lane toggle rows - so it has to clamp
  // before the FIRST of those, not just before the second. Clamping in between put
  // the support amp's knobs on screen under a hero, cab row and hint bar that had all
  // already moved back to MAIN, and left them there until some later pass.
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutRuntime.inc.cpp");
  const auto clampCall = runtime.find("_VolumClampSupportFocus();");
  const auto knobGroups = runtime.find("supportFocus ? \"SUPPORT_AMP_KNOBS\" : \"AMP_KNOBS\"");
  const auto snapshot = runtime.find("const bool supportFocusNow = dualActiveNow && mVolumDualAmpFocusedSupport;");
  REQUIRE(clampCall != std::string::npos);
  REQUIRE(knobGroups != std::string::npos);
  REQUIRE(snapshot != std::string::npos);
  CHECK(clampCall < knobGroups);
  CHECK(clampCall < snapshot);
}

TEST_CASE("Custom NAM save and async load failures cannot masquerade as success")
{
  const std::string source = ReadPluginSource();
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomOverlay.h");
  RequireContains(source, "PrepareCustomNamImport(");
  RequireContains(overlay, "PathFromUtf8(fn.Get())");
  RequireContains(source, "PathToUtf8(volum::content::GlobalContentStore().ResolveStored(rel))");
  RequireContains(source, "if (!prepared)");
  RequireContains(source, "return \"Save failed: \" + prepared.error;");
  RequireContains(source, "mVolumMainLoadFailed.store(true);");
  // A load the user has already moved on from must not be applied as if it were
  // the one they asked for. The drain used to spell that `if (superseded)`; it now
  // feeds the same fact into the shared decision so the retire-vs-apply choice is
  // testable off the audio thread (test_volum_dsp_staging.cpp).
  RequireContains(source, "superseded = true;");
  RequireContains(source, "volum::dsp_staging::DecideLoaderResult(");
  RequireContains(source, "result.model != nullptr, superseded,");
  RequireContains(source, "LOAD FAILED");
  RequireContains(source, "(still playing ");
  // Keep every WDL/iPlug path string UTF-8 all the way to the native filesystem
  // boundary. Reconstructing with path(std::string) invokes the Windows ANSI
  // code page and can recreate the original failure under a Unicode profile.
  RequireContains(source, "mVolumRigsRoot = volum::content::PathToUtf8(root);");
  RequireContains(source, "std::filesystem::is_regular_file(volum::content::PathFromUtf8(fileToLoad)");
  RequireContains(overlay, "std::filesystem::file_size(volum::content::PathFromUtf8(fn.Get()), ec)");
  RequireDoesNotContain(source, "std::filesystem::path(fileToLoad)");
  RequireDoesNotContain(source, "std::filesystem::path(mVolumRigsRoot)");
  RequireDoesNotContain(overlay, "std::filesystem::path(fn.Get())");
  // The active footer filename is committed from mNAMPaths only after the DSP
  // staging path reports a successful model swap.
  RequireContains(source, "volum::content::PathToUtf8(volum::content::PathFromUtf8(mNAMPaths.live.Get()).filename());");
}

TEST_CASE("Keyboard and mouse toggles share one dirty-marking funnel")
{
  // Funnel B: the keyboard toggle historically skipped _VolumMarkPresetDirty
  // while the mouse chip called it. Both must now route through _VolumUserToggleParam.
  const std::string source = ReadPluginSource();

  // The shared funnel exists and marks the preset dirty.
  RequireContains(source, "bool NeuralAmpModeler::_VolumUserToggleParam(int paramIdx)");
  {
    const auto funnelPos = source.find("bool NeuralAmpModeler::_VolumUserToggleParam(int paramIdx)");
    REQUIRE(funnelPos != std::string::npos);
    const auto dirtyPos = source.find("_VolumMarkPresetDirty();", funnelPos);
    const auto endPos = source.find("\n}", funnelPos);
    REQUIRE(dirtyPos != std::string::npos);
    REQUIRE(endPos != std::string::npos);
    CHECK(dirtyPos < endPos); // dirty marked inside the funnel body
  }

  // Keyboard toggle routes through the funnel (so every keyboard-actionable
  // toggle: dual amp, COMP, PRE_NAM1/2, DELAY, REVERB now marks dirty).
  RequireContains(source, "const bool next = _VolumUserToggleParam(paramIdx);");
  // Mouse DUAL chip routes through the same funnel.
  RequireContains(source, "_VolumUserToggleParam(kDualAmpActive);");

  // The dead, unreachable PRE-capture keyboard cycler was removed (it had no
  // callers and wiring it needs a key-binding decision).
  RequireDoesNotContain(source, "_VolumCyclePreNamCapture");
}

TEST_CASE("Restore re-applies cached DSP gains and tone coefficients")
{
  // Funnel A: programmatic restore (preset recall / amp switch / session / DAW)
  // pushes params via SendParameterValueFromDelegate, which skips OnParamChange.
  // _VolumApplyAmpSettings must therefore re-apply every cached DSP value, or
  // OUTPUT recalled from -inf shows 0 dB on the knob but stays silent.
  const std::string source = ReadPluginSource();

  // _VolumApplyAmpSettings ends by re-applying the caches.
  RequireContains(source, "_VolumApplyDspCaches();");

  // The funnel re-applies exactly the cached set: 3 gains + 6 tone coefficients.
  RequireContains(source, "void NeuralAmpModeler::_VolumApplyDspCaches()");
  RequireContains(source, "_SetInputGain();");
  RequireContains(source, "_SetOutputGain();");
  RequireContains(source, "_SetSupportOutputGain();");
  RequireContains(source, "mToneStack->SetParam(\"bass\", GetParam(kToneBass)->Value());");
  RequireContains(source, "mToneStack->SetParam(\"middle\", GetParam(kToneMid)->Value());");
  RequireContains(source, "mToneStack->SetParam(\"treble\", GetParam(kToneTreble)->Value());");
  RequireContains(source, "mSupportToneStack->SetParam(\"bass\", GetParam(kSupportToneBass)->Value());");
  RequireContains(source, "mSupportToneStack->SetParam(\"middle\", GetParam(kSupportToneMid)->Value());");
  RequireContains(source, "mSupportToneStack->SetParam(\"treble\", GetParam(kSupportToneTreble)->Value());");
}

TEST_CASE("A custom SUPPORT partner is admitted to the audio graph")
{
  // A custom (library) SUPPORT amp has no factory index, so the selection path
  // parks kSupportAmpIdx at -1. ProcessBlock used to derive "a support amp is
  // selected" from that param alone, so runSupportModel/runDualAmp stayed false
  // and the custom dual-amp lane was silent while the UI showed it loaded.
  const std::string source = ReadPluginSource();
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");

  const auto processBlock = source.find("void NeuralAmpModeler::ProcessBlock(");
  REQUIRE(processBlock != std::string::npos);
  const std::string processBody = source.substr(processBlock, 2500);

  RequireContains(processBody, "mVolumSupportSelected.load(std::memory_order_relaxed)");
  RequireDoesNotContain(processBody, "GetParam(kSupportAmpIdx)->Int() >= 0");

  // The flag is only trustworthy if the one function that owns it maintains it on
  // every path. Two load paths (custom partner, factory amp) arm it; every exit
  // that asks the audio thread to drop the model clears it. Counting them is what
  // makes a newly added exit that forgets the flag fail here.
  auto countOf = [](const std::string& haystack, const std::string& needle) {
    size_t n = 0;
    for (size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1))
      ++n;
    return n;
  };
  const auto request = loader.find("void NeuralAmpModeler::_VolumRequestSupportModelLoad()");
  REQUIRE(request != std::string::npos);
  const std::string requestBody = loader.substr(request);

  CHECK(countOf(requestBody, "mVolumSupportSelected.store(true)") == 2);
  CHECK(countOf(requestBody, "mVolumSupportSelected.store(false)") == 5);
  CHECK(countOf(requestBody, "mShouldRemoveSupportModel.store(true)")
        == countOf(requestBody, "mVolumSupportSelected.store(false)"));
}

TEST_CASE("The audio-thread loader drain does no diagnostic-log file I/O")
{
  // _VolumDrainLoaderResults runs on the audio thread: ProcessBlock ->
  // _ApplyDSPStaging -> _VolumDrainLoaderResults. Every VOLUM_LOG entry takes a
  // mutex, stats the log file, may rename it, and opens an ofstream. Logging load
  // outcomes from the drain therefore put blocking file I/O in the realtime
  // callback on every amp, channel or cab switch - and VoLumDiagLog.h's own
  // contract says never to call it from the audio thread.
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");

  const auto drain = loader.find("void NeuralAmpModeler::_VolumDrainLoaderResults()");
  REQUIRE(drain != std::string::npos);
  const auto loaderMain = loader.find("void NeuralAmpModeler::_VolumLoaderThreadMain()");
  REQUIRE(loaderMain != std::string::npos);
  REQUIRE(drain < loaderMain); // the drain body is bounded by the next function

  const std::string drainBody = loader.substr(drain, loaderMain - drain);
  RequireDoesNotContain(drainBody, "VOLUM_LOG");
  // T1-2: a stale-rate result used to ResetAndPrewarm on this thread. The helper
  // decides RetireAndReload; the drain must not call Reset itself.
  RequireDoesNotContain(drainBody, "->Reset(");
  RequireContains(drainBody, "DecideLoaderResult");
  RequireContains(drainBody, "RetireToGraveyard");

  // The outcomes are still logged, just from the worker thread that produced them -
  // and worded for what that thread actually knows. It has read and parsed the file;
  // whether the model reaches the audio graph is decided later, in the drain, which
  // may discard it as superseded. "loaded" claimed the second thing.
  const std::string loaderBody = loader.substr(loaderMain);
  RequireContains(loaderBody, "VOLUM_LOG(\"model\"");
  RequireContains(loaderBody, "\" read \"");
  RequireContains(loaderBody, "\" load FAILED \"");
}

TEST_CASE("Audio-thread model apply retires to the graveyard and never throws")
{
  // NeuralAmpModeler cannot be constructed in this binary (iPlug). The helpers in
  // test_volum_dsp_staging.cpp and test_process_io.cpp own the behaviour, and
  // test_volum_golden_render.cpp renders through ResamplingNAM; these pins are
  // only the call sites.
  const std::string source = ReadPluginSource();
  const std::string pluginHeader = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");
  RequireContains(pluginHeader, "#include \"VoLumResamplingNam.h\"");
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumResamplingNam.h");

  const auto apply = source.find("void NeuralAmpModeler::_ApplyDSPStaging()");
  REQUIRE(apply != std::string::npos);
  const auto applyEnd = source.find("void NeuralAmpModeler::_VolumFlushDeferredIrShaping()", apply);
  REQUIRE(applyEnd != std::string::npos);
  const std::string applyBody = source.substr(apply, applyEnd - apply);
  RequireContains(applyBody, "PublishStagedModel");
  RequireContains(applyBody, "PublishPathNoAlloc(mPublishedNamPath, mPendingNamPath)");
  RequireDoesNotContain(applyBody, "CommitStagedPathOnApply(mNAMPaths)");

  // 1.3.0 hardening: IRs follow the same rules. The apply used to free the outgoing
  // convolver (`mIR = std::move(mStagedIR)`, `mIR = nullptr`) and copy or clear the
  // WDL_String paths in the callback. The helpers are unit-tested in
  // test_volum_dsp_staging.cpp; these are the call sites.
  RequireContains(applyBody, "PublishStagedModel(mIR, mStagedIR, mIrGraveyard)");
  RequireContains(applyBody, "PublishStagedModel(mSupportIR, mStagedSupportIR, mIrGraveyard)");
  RequireContains(applyBody, "RetireLiveAndStaged(mIR, mStagedIR, mIrGraveyard)");
  RequireContains(applyBody, "RetireLiveAndStaged(mSupportIR, mStagedSupportIR, mIrGraveyard)");
  RequireContains(applyBody, "PublishPathNoAlloc(mPublishedIRPath, mPendingIRPath)");
  RequireContains(applyBody, "PublishPathNoAlloc(mPublishedSupportIRPath, mPendingSupportIRPath)");
  RequireContains(applyBody, "PublishPathClearNoAlloc(mPublishedIRPath)");
  RequireContains(applyBody, "PublishPathClearNoAlloc(mPublishedSupportIRPath)");
  RequireContains(applyBody, "PublishPathClearNoAlloc(mPublishedNamPath)");
  RequireDoesNotContain(applyBody, "mIR = ");
  RequireDoesNotContain(applyBody, "mSupportIR = ");
  RequireDoesNotContain(applyBody, "mStagedIR = ");
  RequireDoesNotContain(applyBody, "mStagedSupportIR = ");
  RequireDoesNotContain(applyBody, "CommitStagedPathOnApply");
  RequireDoesNotContain(applyBody, "ClearLiveAndStagedPath");
  RequireContains(source, "mIrGraveyard.reserve(volum::dsp_staging::kDspGraveyardCapacity);");

  // OnIdle reaps both graveyards and the drained loader batches, and commits all
  // three published paths.
  const std::string reap = MemberFnUntilNext(source, "void NeuralAmpModeler::_VolumReapAudioThreadRetirees()");
  RequireContains(reap, "doomedIrs.swap(mIrGraveyard);");
  RequireContains(reap, "doomed.swap(mDspGraveyard);");
  RequireContains(reap, "doomedResults.swap(mVolumSpentLoadResults);");
  RequireContains(reap, "{mPublishedNamPath, mNAMPaths, {}}");
  RequireContains(reap, "{mPublishedIRPath, mIRPaths, {}}");
  RequireContains(reap, "{mPublishedSupportIRPath, mSupportIRPaths, {}}");
  RequireContains(MemberFnUntilNext(source, "void NeuralAmpModeler::OnIdle()"), "_VolumReapAudioThreadRetirees();");

  // _StageIR / _StageModel destroy what they replace after dropping mStagingMutex.
  const std::string stageIr = MemberFnUntilNext(source, "dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(");
  RequireContains(stageIr, "replacedIR = volum::dsp_staging::ReplaceStaged(stagedSlot, std::move(stagedIR));");
  RequireContains(stageIr, "replacedIR = volum::dsp_staging::ReplaceStaged(stagedSlot, nullptr);");
  RequireDoesNotContain(stageIr, "stagedSlot = std::move");
  RequireDoesNotContain(stageIr, "stagedSlot = nullptr");
  RequireDoesNotContain(stageIr, "StagePathOnSuccess");
  RequireDoesNotContain(stageIr, "ClearStagedPath");
  const std::string stageModel = MemberFnUntilNext(source, "std::string NeuralAmpModeler::_StageModel(");
  RequireContains(stageModel, "replacedModel = volum::dsp_staging::ReplaceStaged(mStagedModel, nullptr);");
  RequireDoesNotContain(stageModel, "mStagedModel = nullptr");
  RequireDoesNotContain(stageModel, "StagePathOnSuccess");

  // The drained batch is handed to OnIdle, never a local that dies in the callback.
  const std::string loaderSource = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");
  const std::string drain = MemberFnUntilNext(loaderSource, "void NeuralAmpModeler::_VolumDrainLoaderResults()");
  RequireDoesNotContain(drain, "std::deque<VoLumLoadResult> results;");
  RequireContains(drain, "auto& results = mVolumDrainBatch;");
  size_t handOffs = 0;
  for (auto at = drain.find("HandOffSpentBatch("); at != std::string::npos;
       at = drain.find("HandOffSpentBatch(", at + 1))
    ++handOffs;
  CHECK(handOffs == 2); // the parked retry and the end of every drain

  // No Reset in the ResamplingNAM constructor: it prewarmed the Full slice of
  // every Lite load (behaviour pinned in test_volum_dsp_staging.cpp).
  const auto ctor = header.find("ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated");
  REQUIRE(ctor != std::string::npos);
  const auto ctorEnd = header.find("~ResamplingNAM()", ctor);
  REQUIRE(ctorEnd != std::string::npos);
  RequireDoesNotContain(header.substr(ctor, ctorEnd - ctor), "Reset(");

  const auto process = header.find("void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames)");
  REQUIRE(process != std::string::npos);
  const auto processEnd =
    header.find("void process(NAM_SAMPLE* input, NAM_SAMPLE* output, const int num_frames)", process);
  REQUIRE(processEnd != std::string::npos);
  const std::string processBody = header.substr(process, processEnd - process);
  RequireContains(processBody, "ProcessNamInChunks");
  RequireDoesNotContain(processBody, "throw std::runtime_error");

  // Every NAM Reset goes through NamResetBlockSize. The 8192 scratch reserve on
  // a NAM is the 1.3.0 crackle (see test_volum_realtime_budget.cpp).
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");
  RequireDoesNotContain(loader, "ReservedAudioBlockSize");
  RequireContains(loader, "request.blockSize = volum::dsp_staging::NamResetBlockSize(GetBlockSize());");
  RequireContains(source, "_ResetModelAndIR(sampleRate, volum::dsp_staging::NamResetBlockSize(maxBlockSize));");
  RequireContains(source, "temp->Reset(GetSampleRate(), volum::dsp_staging::NamResetBlockSize(GetBlockSize()));");
}

TEST_CASE("Closing the editor deactivates the tuner so the instance cannot stay muted")
{
  // An active tuner memsets every output channel (silenceForTuner in
  // ProcessBlock). mTunerDSP is a plugin member, so it outlives the editor, and
  // the only two places that can clear it -- the tuner toggle and the tuner
  // control's dismiss action -- both require an editor. Closing the plugin window
  // with the tuner open therefore silenced the instance for good, invisibly: the
  // editor is rebuilt with the tuner hidden, so reopening showed a normal UI over
  // a dead signal path. The metronome click is the sibling: same editor-owned
  // DSP, same OnUIClose, one helper so the second copy cannot be forgotten.
  const std::string source = ReadPluginSource();

  const auto onUIClose = source.find("void NeuralAmpModeler::OnUIClose()");
  REQUIRE(onUIClose != std::string::npos);
  const auto body = source.substr(onUIClose, 900);

  RequireContains(body, "HaltEditorOwnedOverlayDsp(");
  RequireContains(body, "mTunerDSP");
  RequireContains(body, "mMetronomeDSP");

  // Pin the two facts that make the above load-bearing, so this test keeps
  // failing for the right reason if either moves.
  RequireContains(source, "processingPlan.silenceForTuner");
  RequireContains(source, "mTunerDSP.IsActive()");
}

TEST_CASE("Keyboard Dual Amp focus commits through the shared cab-row helper")
{
  // Tab, the `2` key, and the Dual-on key used to assign mVolumDualAmpFocusedSupport
  // and rebuild layout without re-deriving the shared cab row. Knobs followed the
  // new lane; a cab click then wrote that lane using the other lane's names.
  const std::string keyboard = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboard.inc.cpp");

  const std::string cycle = MemberFnUntilNext(keyboard, "bool NeuralAmpModeler::_CycleVoLumKeyboardTarget(");
  RequireContains(cycle, "volum::dualamp::CommitFocus(");
  RequireContains(cycle, "volum::dualamp::ApplyFocusCommit(");
  RequireContains(cycle, "_VolumApplyFocusedLaneCabs();");

  const std::string section = MemberFnUntilNext(keyboard, "bool NeuralAmpModeler::_SwitchVoLumKeyboardSection(");
  RequireContains(section, "volum::dualamp::CommitFocus(");
  RequireContains(section, "volum::dualamp::ApplyFocusCommit(");
  RequireContains(section, "_VolumApplyFocusedLaneCabs();");

  const std::string toggle = MemberFnUntilNext(keyboard, "bool NeuralAmpModeler::_ToggleVoLumKeyboardTarget(");
  RequireContains(toggle, "volum::dualamp::CommitFocus(");
  RequireContains(toggle, "volum::dualamp::ApplyFocusCommit(");
  RequireContains(toggle, "_VolumApplyFocusedLaneCabs();");
}

TEST_CASE("Custom sidebar selection re-derives the focused lane's cab row")
{
  // _VolumApplyAmpSettings claims every caller ends in _VolumApplyFocusedLaneCabs.
  // The custom sidebar path called _VolumApplyCustomMainCabs(Y) with MAIN, so a
  // SUPPORT-focused click left the previous partner's names on the shared row.
  const std::string select = MemberFnUntilNext(ReadPluginSource(), "void NeuralAmpModeler::_VolumSelectCustomAmp(");
  RequireContains(select, "_VolumApplyCustomMainCabs(customIdx);");

  // The headless early-return also calls ApplyCustomMainCabs (a no-op without UI).
  // The focused-lane rederive has to run on the UI path after that return, or a
  // SUPPORT-focused sidebar click still leaves the previous partner on the row.
  const auto noUi = select.find("if (!pGfx)");
  REQUIRE(noUi != std::string::npos);
  const auto noUiReturn = select.find("return;", noUi);
  REQUIRE(noUiReturn != std::string::npos);
  REQUIRE(select.find("_VolumApplyFocusedLaneCabs();", noUiReturn) != std::string::npos);
}

TEST_CASE("Hero lane clicks ask the shared Dual Amp click protocol")
{
  // The protocol itself, including the empty-lane behaviour that #29 was about,
  // is covered in test_volum_dual_amp_input.cpp. This only pins that the control
  // asks: a correct protocol nobody calls is how that bug shipped.
  const std::string hero = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumHero.h");
  RequireContains(hero, "volum::dualamp::DecideHeroClick(state, hitDualChip, hitSupportHalf)");
  RequireContains(hero, "mDblAsSingleClick = true;");
}

TEST_CASE("Polarity writes the active scene, not the parked factory slot")
{
  // While a custom MAIN is focused, mVolumAmpIdx still names the parked factory
  // amp. The glyph and Dual-on heal used to write that slot; returning to the
  // factory amp restored a polarity it never had. The save path already writes
  // _VolumActiveScene().
  const std::string source = ReadPluginSource();
  RequireContains(source, "_VolumActiveScene().supportPolarityInvert");
  RequireDoesNotContain(source, "mVolumAmpSettings[mVolumAmpIdx].supportPolarityInvert");
}

TEST_CASE("tier2a ProcessBlock keeps denormals off through the safety clip")
{
  const std::string source = ReadPluginSource();
  const auto pb = source.find("void NeuralAmpModeler::ProcessBlock(");
  REQUIRE(pb != std::string::npos);
  const auto end = source.find("void NeuralAmpModeler::OnReset()", pb);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(pb, end - pb);
  const auto safety = body.rfind("SoftSafetyClip");
  const auto restore = body.rfind("feupdateenv");
  REQUIRE(safety != std::string::npos);
  REQUIRE(restore != std::string::npos);
  CHECK(restore > safety);
}

TEST_CASE("tier2a tuner mute leaves the metronome click on the bus")
{
  const std::string source = ReadPluginSource();
  const auto pb = source.find("void NeuralAmpModeler::ProcessBlock(");
  REQUIRE(pb != std::string::npos);
  const auto end = source.find("void NeuralAmpModeler::OnReset()", pb);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(pb, end - pb);
  const auto tuner = body.find("silenceForTuner");
  const auto metro = body.find("mMetronomeDSP.Process");
  REQUIRE(tuner != std::string::npos);
  REQUIRE(metro != std::string::npos);
  CHECK(tuner < metro);
}

TEST_CASE("tier2a PRE pitch and compressor reset on the bypass edge")
{
  const std::string source = ReadPluginSource();
  const auto pre = source.find("NeuralAmpModeler::_VolumProcessPreChain(");
  REQUIRE(pre != std::string::npos);
  const auto end = source.find("NeuralAmpModeler::_VolumProcessMainAmpChain(", pre);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(pre, end - pre);
  RequireContains(body, "mPitch.Reset()");
  RequireContains(body, "mPreCompressor.Reset()");
}

TEST_CASE("tier2a model apply latches latency instead of updating it on the audio thread")
{
  const std::string source = ReadPluginSource();
  const auto apply = source.find("void NeuralAmpModeler::_ApplyDSPStaging()");
  REQUIRE(apply != std::string::npos);
  const auto end = source.find("void NeuralAmpModeler::_VolumFlushDeferredIrShaping()", apply);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(apply, end - apply);
  RequireDoesNotContain(body, "_UpdateLatency()");
  RequireContains(body, "mLatencyDirty");
}

TEST_CASE("tier2a loader drain does not block on the loader mutex")
{
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");
  const auto drain = loader.find("void NeuralAmpModeler::_VolumDrainLoaderResults()");
  REQUIRE(drain != std::string::npos);
  const auto next = loader.find("void NeuralAmpModeler::_VolumLoaderThreadMain()", drain);
  REQUIRE(next != std::string::npos);
  const std::string body = loader.substr(drain, next - drain);
  RequireContains(body, "try_to_lock");
  RequireContains(body, "superseded = true;");
  RequireDoesNotContain(body, "lock_guard<std::mutex> lock(mVolumLoaderMutex)");
}

TEST_CASE("tier2a OnReset reserves the dual-amp latency line")
{
  const std::string source = ReadPluginSource();
  const auto reset = source.find("void NeuralAmpModeler::OnReset()");
  REQUIRE(reset != std::string::npos);
  const auto end = source.find("void NeuralAmpModeler::ProcessMidiMsg(", reset);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(reset, end - reset);
  RequireContains(body, "mDualMainLatencyDelay.Reserve(");
  RequireContains(body, "mDualSupportLatencyDelay.Reserve(");
}

TEST_CASE("tier2b a missing IR id is cleared on the scene that recalled it")
{
  const std::string source = ReadPluginSource();
  const std::string body = MemberFnUntilNext(source, "void NeuralAmpModeler::_VolumApplyActiveIr(");
  const auto missing = body.find("if (idx < 0)");
  REQUIRE(missing != std::string::npos);
  const std::string branch = body.substr(missing);
  RequireContains(branch, "_VolumActiveScene().activeIrId.clear();");
  RequireContains(branch, "_VolumActiveScene().supportActiveIrId.clear();");
}

TEST_CASE("tier2b sidebar delete tells the user when the library write fails")
{
  const std::string build = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const auto fail = build.find("if (volum::custom::Store().TakeWriteFailure())");
  REQUIRE(fail != std::string::npos);
  const std::string branch = build.substr(fail, 700);
  RequireContains(branch, "_ShowMessageBox(");
  RequireContains(branch, "Your library could not be saved - this change will be lost.");
}

TEST_CASE("tier2b opening the window shows a corrupt-library recovery")
{
  const std::string source = ReadPluginSource();
  const auto open = source.find("void NeuralAmpModeler::OnUIOpen()");
  REQUIRE(open != std::string::npos);
  const auto end = source.find("void NeuralAmpModeler::", open + 10);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(open, end - open);
  RequireContains(body, "TakeCorruptRecoveryNotice()");
  RequireContains(body, "_ShowMessageBox(gfx, notice.c_str(), \"VoLum\", EMsgBoxType::kMB_OK)");
}

TEST_CASE("tier2b preset rename uniqueness uses this overlay's owner")
{
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomOverlay.h");
  const auto start = overlay.find("bool NameTaken(");
  REQUIRE(start != std::string::npos);
  const auto end = overlay.find("void SetNameError(", start);
  REQUIRE(end != std::string::npos);
  const std::string body = overlay.substr(start, end - start);
  RequireContains(body, "PresetsForOwner(PresetOwnerKey())");
  RequireDoesNotContain(body, "PresetNameExists");
}

TEST_CASE("tier2b editing an amp queues a delete for a capture the edit dropped")
{
  const std::string api = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomContentApi.h");
  const auto start = api.find("inline int UpdateCustomAmp(");
  REQUIRE(start != std::string::npos);
  const auto end = api.find("inline int AddCustomAmp(", start);
  REQUIRE(end != std::string::npos);
  const std::string body = api.substr(start, end - start);
  RequireContains(body, "QueueStoredFileDelete(oldFile.storedPath)");
}

TEST_CASE("tier2c a plugin import preview names the MIDI map replace")
{
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPackOverlay.h");
  const auto start = overlay.find("if (preview.writesSettings)");
  REQUIRE(start != std::string::npos);
  const std::string body = overlay.substr(start, 500);
  RequireContains(body, "preview.replacesMidiSoundMap");
  RequireContains(body, "\"MIDI slots\"");
}

TEST_CASE("tier2c a committed library reloads when the settings write fails")
{
  const std::string actions = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPackActions.inc.cpp");
  const auto start = actions.find("const auto result = volum::pack::ApplyPack(");
  REQUIRE(start != std::string::npos);
  const auto end = actions.find("return {};", start);
  REQUIRE(end != std::string::npos);
  const std::string body = actions.substr(start, end - start);
  const auto committed = body.find("if (result.libraryCommitted)");
  const auto failed = body.find("if (!result.ok)");
  REQUIRE(committed != std::string::npos);
  REQUIRE(failed != std::string::npos);
  CHECK(committed < failed);
  const std::string beforeError = body.substr(committed, failed - committed);
  RequireContains(beforeError, "_VolumReloadReplacedLibraryIds(result.replacedIds)");
}

TEST_CASE("tier2d the PLAY rail accepts a drop on Add and in the row gap")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const auto drop = play.find("void UpdateDropTarget(");
  REQUIRE(drop != std::string::npos);
  const auto end = play.find("void CommitRailDrop(", drop);
  REQUIRE(end != std::string::npos);
  const std::string body = play.substr(drop, end - drop);
  RequireContains(body, "row == kHoverAdd");
  RequireContains(body, "within >= kRowH");
}

TEST_CASE("tier2d PLAY plates commit on mouse-up and a drag still reorders")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const auto up = play.find("void OnMouseUp(");
  REQUIRE(up != std::string::npos);
  const auto end = play.find("void OnMouseDblClick(", up);
  REQUIRE(end != std::string::npos);
  const std::string body = play.substr(up, end - up);
  const auto drag = body.find("if (wasDrag)");
  const auto clear = body.find("kPressClear");
  REQUIRE(drag != std::string::npos);
  REQUIRE(clear != std::string::npos);
  CHECK(drag < clear);
  RequireContains(body, "CommitRailDrop(pressSlot, x, y)");
}

TEST_CASE("tier2d PLAY SetData follows the pressed Sound and OnMouseOut cancels the gesture")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const auto data = play.find("void SetData(");
  REQUIRE(data != std::string::npos);
  const auto dataEnd = play.find("void OnRescale()", data);
  REQUIRE(dataEnd != std::string::npos);
  const std::string setBody = play.substr(data, dataEnd - data);
  RequireContains(setBody, "mSlots[static_cast<size_t>(i)].slot == mPressSlot");
  const auto firstOut = play.find("void OnMouseOut() override");
  REQUIRE(firstOut != std::string::npos);
  const auto out = play.find("void OnMouseOut() override", firstOut + 1);
  REQUIRE(out != std::string::npos);
  const std::string outBody = play.substr(out, 400);
  RequireContains(outBody, "mDragging = false");
  RequireContains(outBody, "mPressSlot = -1");
}

TEST_CASE("tier2d a full PLAY map opens the replace picker instead of doing nothing")
{
  const std::string play = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlaySurface.h");
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPlayRuntime.inc.cpp");
  RequireContains(play, "FirstFreeSlot() >= 0");
  RequireContains(play, "void OpenReplacePicker()");
  RequireContains(runtime, "OpenReplacePicker()");
}

TEST_CASE("tier2e Calibrated n/a disables that radio state")
{
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const auto start = controls.find("void SetCalibratedDisable(");
  REQUIRE(start != std::string::npos);
  const auto end = controls.find("void OnMouseDown(", start);
  REQUIRE(end != std::string::npos);
  const std::string body = controls.substr(start, end - start);
  RequireContains(body, "SetStateDisabled(2, disable)");
  // A bare Resize leaves Raw/Normalized holding garbage and the radio stuck.
  RequireContains(body, "EnsureRadioDisabledStates(mDisabledState, mNumStates)");
  RequireDoesNotContain(body, "mDisabledState.Resize(");
  const std::string click = controls.substr(end, 280);
  RequireContains(click, "GetStateDisabled(index)");
}

TEST_CASE("tier2e the disabled dBu field draws through the grey blend")
{
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const auto start = controls.find("class InputLevelControl");
  REQUIRE(start != std::string::npos);
  const std::string body = controls.substr(start, 900);
  RequireContains(body, "g.FillRect(VoLumColors::HERO_BG, mRECT, &mBlend)");
}

TEST_CASE("tier2e an empty MIDI map does not teach drag or clear")
{
  // The drag / clear lines are the default; an empty map swaps them for the one
  // thing that can happen next.
  const std::string view = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumMidiFootswitch.h");
  const auto drag = view.find("Drag onto another to swap. The cross clears.");
  REQUIRE(drag != std::string::npos);
  const auto empty = view.find("else if (mSlots.empty())", drag);
  REQUIRE(empty != std::string::npos);
  CHECK(empty - drag < 900);
  const auto emptyLine = view.find("Click a switch to give its program number a Sound.", empty);
  REQUIRE(emptyLine != std::string::npos);
  CHECK(emptyLine - empty < 200);
  // No cross to hit on an empty switch.
  RequireContains(view, "if (tileHover && assigned)");
}

TEST_CASE("tier2e Manage in a menu is teal and a clipped Manage row has no hotspot")
{
  const std::string menu = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumListMenu.h");
  const auto draw = menu.find("const IColor col");
  REQUIRE(draw != std::string::npos);
  const std::string col = menu.substr(draw, 240);
  RequireContains(col, "r.action ? VoLumColors::TEAL");
  RequireDoesNotContain(col, "kManage");
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumCustomOverlay.h");
  const auto manage = overlay.find("void DrawManage(");
  REQUIRE(manage != std::string::npos);
  const auto clamp = overlay.find("void ClampManageScroll(", manage);
  REQUIRE(clamp != std::string::npos);
  const std::string body = overlay.substr(manage, clamp - manage);
  RequireContains(body, "const bool rowVisible");
  const auto rename = overlay.find("case TextTarget::RenameItem:");
  REQUIRE(rename != std::string::npos);
  RequireContains(overlay.substr(rename, 700), "Enter a name.");
  const auto visible = body.find("const bool rowVisible");
  const auto hotspot = body.find("AddHotspot(row,", visible);
  REQUIRE(hotspot != std::string::npos);
  CHECK(body.find("if (rowVisible)", visible) < hotspot);
}

TEST_CASE("tier2e a failed update check does not stamp the 24 hour clock")
{
  const std::string inc = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumUpdateCheck.inc.cpp");
  const auto thread = inc.find("std::thread([result");
  REQUIRE(thread != std::string::npos);
  const auto get = inc.find("VolumHttpGetString", thread);
  REQUIRE(get != std::string::npos);
  const std::string before = inc.substr(thread, get - thread);
  CHECK(before.find("lastCheckUtc") == std::string::npos);
  RequireContains(inc, "CheckFailureNotice()");
}

TEST_CASE("tier2f chorus last-knob memory is inside the target array")
{
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");
  RequireContains(header, "std::array<int, 10> mVolumLastKeyboardKnobByTarget");
  const std::string keyboard = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboard.inc.cpp");
  const auto read = keyboard.find("int NeuralAmpModeler::_RememberedVoLumKeyboardKnobForFocus()");
  REQUIRE(read != std::string::npos);
  const std::string body = keyboard.substr(read, 500);
  RequireContains(body, "target < static_cast<int>(mVolumLastKeyboardKnobByTarget.size())");
}

TEST_CASE("tier2f keyboard POST lands on the same first pedal as the header")
{
  const std::string keyboard = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboard.inc.cpp");
  const auto post = keyboard.find("case EVoLumSection::POST:");
  REQUIRE(post != std::string::npos);
  const std::string body = keyboard.substr(post, 700);
  RequireContains(body, "kChorusActive");
  RequireContains(body, "EVoLumEffectFocus::CHORUS");
  CHECK(body.find("kChorusActive") < body.find("kDelayActive"));
}

TEST_CASE("tier2f post lock chrome includes chorus and tremolo")
{
  const std::string cpp = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const auto start = cpp.find("bool IsPostBlockParam(");
  REQUIRE(start != std::string::npos);
  const auto end = cpp.find("void NeuralAmpModeler::_VolumRefreshPrePostLockChrome", start);
  REQUIRE(end != std::string::npos);
  const std::string body = cpp.substr(start, end - start);
  RequireContains(body, "kChorusRate");
  RequireContains(body, "kTremoloRate");
  RequireContains(body, "kDelaySync");
  RequireContains(body, "kDelayDivision");
}

TEST_CASE("tier2f the expanded pedal LED toggles bypass")
{
  const std::string card = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumPedalCardControl.h");
  const auto down = card.find("void OnMouseDown(");
  REQUIRE(down != std::string::npos);
  const std::string body = card.substr(down, 500);
  RequireContains(body, "ledRect.Contains(x, y)");
  RequireContains(body, "mCallback(this, true)");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const auto click = layout.find("auto onPedalClick");
  REQUIRE(click != std::string::npos);
  const std::string handler = layout.substr(click, 900);
  RequireContains(handler, "if (isBypassClick)");
  RequireContains(handler, "kChorusActive");
  RequireDoesNotContain(handler.substr(0, 80), "(void)isBypassClick");
}

TEST_CASE("tier2f MAIN cab fallback does not paint the row while SUPPORT is focused")
{
  const std::string rig = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSceneRig.inc.cpp");
  const auto start = rig.find("void NeuralAmpModeler::_VolumFallbackToAvailableCab()");
  REQUIRE(start != std::string::npos);
  const std::string body = rig.substr(start, 2200);
  RequireContains(body, "row && !_VolumSupportFocused()");
}

TEST_CASE("tier2f the hero name stops before the PAN knob")
{
  const std::string hero = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumHero.h");
  const auto lane = hero.find("void DrawLane(");
  REQUIRE(lane != std::string::npos);
  const std::string body = hero.substr(lane, 4000);
  RequireContains(body, "titleStrip.R - kPanKnobSize");
  RequireContains(body, "nameR.W() - 6.f");
}

TEST_CASE("tier2f SUPPORT identity follows the custom amp id")
{
  const std::string menus = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpMenus.inc.cpp");
  const auto start = menus.find("void NeuralAmpModeler::_VolumRebindCustomSupportIdx()");
  REQUIRE(start != std::string::npos);
  const std::string body = menus.substr(start, 400);
  RequireContains(body, "supportCustomId");
  RequireContains(body, "CustomAmpIndexById(id)");
  const std::string cpp = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const auto idle = cpp.find("void NeuralAmpModeler::OnIdle()");
  REQUIRE(idle != std::string::npos);
  RequireContains(cpp.substr(idle, 800), "_VolumRebindCustomSupportIdx()");
}

TEST_CASE("tier2h host undo refreshes the unsaved flag and Enter confirms off the dialog")
{
  const std::string source = ReadPluginSource();
  const auto ui = source.find("void NeuralAmpModeler::OnParamChangeUI");
  REQUIRE(ui != std::string::npos);
  const auto modeCase = source.find("case kDelayMode:", ui);
  REQUIRE(modeCase != std::string::npos);
  CHECK(source.substr(ui, modeCase - ui).find("_VolumRecomputePresetDirty") == std::string::npos);
  const auto recompute = source.find("_VolumRecomputePresetDirty()", modeCase);
  REQUIRE(recompute != std::string::npos);
  RequireContains(source.substr(recompute - 220, 260), "source == EParamSource::kUI || source == EParamSource::kHost");

  const std::string keys = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumKeyboardModel.h");
  RequireContains(keys, "return KeyConsumer::ConfirmEnter;");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");
  const auto enter = layout.find("case KeyConsumer::ConfirmEnter:");
  REQUIRE(enter != std::string::npos);
  RequireContains(layout.substr(enter, 280), "confirm->OnKeyDown(0.f, 0.f, key);");

  const std::string menus = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumAmpMenus.inc.cpp");
  const auto pick = menus.find("void NeuralAmpModeler::_VolumSetSupportCustom");
  REQUIRE(pick != std::string::npos);
  const std::string pickBody = menus.substr(pick, 1600);
  RequireContains(pickBody, "CaptureSelectionOrDefault(amp, s, c)");
  RequireDoesNotContain(pickBody, "if (volum::content::DefaultCaptureSelection(amp, s, c))");
}

TEST_CASE("tier2h macOS alert swap matches the panel and WinMM drops a status-less byte")
{
  const std::string mac = ReadText(RepoRoot() / "iPlug2" / "IGraphics" / "Platforms" / "IGraphicsMac.mm");
  const auto panel = mac.find("EMsgBoxResult IGraphicsMac::ShowMessageBox");
  REQUIRE(panel != std::string::npos);
  RequireContains(mac.substr(panel, 1600), "NSRunAlertPanel(msg, @\"%@\", @\"OK\"");
  const std::string plugin = ReadPluginSource();
  RequireContains(plugin, "return pGraphics->ShowMessageBox(caption, str, type);");

  const std::string rtmidi = ReadText(RepoRoot() / "iPlug2" / "Dependencies" / "IPlug" / "RTMidi" / "RtMidi.cpp");
  const auto winmm = rtmidi.find("if ( inputStatus == MIM_DATA )");
  REQUIRE(winmm != std::string::npos);
  RequireContains(rtmidi.substr(winmm, 400), "if ( !(status & 0x80) ) return;");
}

TEST_CASE("tier2g the settings reader is the block readers and chorus has no private restore flag")
{
  const std::string io = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumUserSettingsIO.h");
  const auto from = io.find("inline void VolumUserSettingsFromJson(");
  REQUIRE(from != std::string::npos);
  const std::string body = io.substr(from, 8000);
  RequireContains(body, "ReadAmpCoreBlock(a, s)");
  RequireContains(body, "PreBlockFromJson(a, s, &preHealed)");
  RequireContains(body, "PostBlockFromJson(a, s)");
  RequireContains(body, "ReadDualAmpUserSettings(a, s, ampCount)");
  RequireDoesNotContain(body, "loadBool(a, \"postChorusActive\"");
  const std::string header = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.h");
  RequireDoesNotContain(header, "mVolumChorusRestoreInProgress");
  const std::string tail = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumChunkIdTail.h");
  RequireContains(tail, "double octDown = 0.8");
  const std::string repair = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumRigRepair.inc.cpp");
  const auto plan = repair.find("SiblingDeletedAmpNeedsRepair(ampGone(rig.mainCustomAmpId))");
  const auto support = repair.find("SiblingDeletedAmpNeedsRepair(ampGone(rig.supportCustomAmpId))", plan);
  REQUIRE(plan != std::string::npos);
  REQUIRE(support != std::string::npos);
}

namespace
{
// Every \u / \U escape inside a narrow ("...") string literal, outside comments.
std::vector<std::string> NarrowLiteralUnicodeEscapes(const std::string& text)
{
  std::vector<std::string> hits;
  bool inBlockComment = false;
  size_t lineNo = 1;
  for (size_t i = 0; i < text.size(); ++i)
  {
    const char c = text[i];
    const char next = i + 1 < text.size() ? text[i + 1] : '\0';
    if (c == '\n')
    {
      ++lineNo;
      continue;
    }
    if (inBlockComment)
    {
      if (c == '*' && next == '/')
      {
        inBlockComment = false;
        ++i;
      }
      continue;
    }
    if (c == '/' && next == '/')
    {
      while (i < text.size() && text[i] != '\n')
        ++i;
      --i;
      continue;
    }
    if (c == '/' && next == '*')
    {
      inBlockComment = true;
      ++i;
      continue;
    }
    if (c == '\'')
    {
      for (++i; i < text.size() && text[i] != '\'' && text[i] != '\n'; ++i)
        if (text[i] == '\\')
          ++i;
      continue;
    }
    if (c != '"')
      continue;
    const char before = i > 0 ? text[i - 1] : ' ';
    const bool wide = before == 'L' || before == 'u' || before == 'U' || (before == '8' && i > 1 && text[i - 2] == 'u');
    for (++i; i < text.size() && text[i] != '"' && text[i] != '\n'; ++i)
    {
      if (text[i] != '\\')
        continue;
      if (!wide && i + 1 < text.size() && (text[i + 1] == 'u' || text[i + 1] == 'U'))
        hits.push_back("line " + std::to_string(lineNo));
      ++i;
    }
  }
  return hits;
}
} // namespace

TEST_CASE("Product strings spell non-ASCII as UTF-8 bytes, never \\u escapes")
{
  // MSVC builds without /utf-8, so "\u2026" in a narrow literal compiles to one
  // cp1252 byte. That is not UTF-8, and NanoVG stops drawing there: the name
  // dialog hint lost "· Esc to cancel" and truncated labels lost their tail.
  // Write the bytes ("\xE2\x80\xA6") the way the About card does.
  CHECK(NarrowLiteralUnicodeEscapes("auto s = \"a\\u2026\";").size() == 1);
  CHECK(NarrowLiteralUnicodeEscapes("auto s = u8\"a\\u2026\"; // \"\\u00B7\"").empty());
  CHECK(NarrowLiteralUnicodeEscapes("auto s = \"a\\xE2\\x80\\xA6\";").empty());

  namespace fs = std::filesystem;
  std::vector<std::string> offenders;
  const fs::path root = RepoRoot() / "NeuralAmpModeler";
  for (fs::recursive_directory_iterator it(root), end; it != end; ++it)
  {
    const std::string rel = fs::relative(it->path(), root).generic_string();
    if (it->is_directory()
        && (rel.rfind("build", 0) == 0 || rel == "tests" || rel.find("third_party") != std::string::npos))
    {
      it.disable_recursion_pending();
      continue;
    }
    const auto ext = it->path().extension();
    if (!it->is_regular_file() || (ext != ".h" && ext != ".cpp"))
      continue;
    for (const auto& hit : NarrowLiteralUnicodeEscapes(ReadText(it->path())))
      offenders.push_back(rel + " " + hit);
  }
  std::string list;
  for (const auto& o : offenders)
    list += o + "\n";
  CHECK_MESSAGE(offenders.empty(), list);
}