#include "third_party/doctest.h"
#include "../config.h"
#include "../VoLumTriptychLayout.h"

#include <filesystem>
#include <fstream>
#include <sstream>
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
  INFO(needle);
  REQUIRE(haystack.find(needle) != std::string::npos);
}

void RequireDoesNotContain(const std::string& haystack, const char* needle)
{
  INFO(needle);
  REQUIRE(haystack.find(needle) == std::string::npos);
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
  // Josefin ships no U+00D7, so "×" renders as a tofu box. Every clear/close
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
  RequireContains(play, "DrawHeroFractalArt(g, artRect, FractalCaseForAmp(art));");
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
  RequireContains(controls, "mUpdateNotice->SetUpdate(available, version);");
  RequireDoesNotContain(controls, "mUpdateButton->Hide(!available);");
  // Auto-check state must be visible; IVToggleControl drew neither frame nor value here.
  RequireContains(overlay, "class VoLumSettingsCheckboxControl");
  RequireContains(controls, "mAutoCheck->SetChecked(autoCheck);");
  RequireDoesNotContain(controls, "new IVToggleControl");
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

TEST_CASE("Settings is exactly two tabs and every locked capability still has a home")
{
  // The one-page overlay could not hold calibration, output mode, performance,
  // MIDI, shortcuts, model info, about and update at 900x600 without clipping its
  // own footer onto the panel's corner accents.
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");

  RequireContains(tabs, "class VoLumSettingsTabStripControl");
  RequireContains(controls, "kTabNames[kTabCount] = {\"SIGNAL\", \"SYSTEM\"}");
  RequireContains(controls, "static constexpr int kTabCount = 2;");
  RequireContains(controls, "void _BuildSignalTab(const IRECT& body)");
  RequireContains(controls, "void _BuildSystemTab(const IRECT& body)");

  // SIGNAL owns the audio/MIDI path.
  RequireContains(controls, "\"Input calibration\", mControlNames.inputGroupFrame");
  RequireContains(controls, "\"Output mode\", mControlNames.outputGroupFrame");
  RequireContains(controls, "\"Performance\", mControlNames.perfGroupFrame");
  RequireContains(controls, "\"MIDI\", mControlNames.midiGroupFrame");
  RequireContains(controls, "audioHintStr");
  // SYSTEM owns this install.
  RequireContains(controls, "\"Keyboard shortcuts\"");
  RequireContains(controls, "\"Model information\"");
  RequireContains(controls, "\"Content library\"");
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

TEST_CASE("Settings keeps the MIDI channel only; PLAY owns the Sound assignment list")
{
  // .scratch/midi-control/spec.md: PLAY owns the list. The interim Settings list
  // was duplicate chrome and had to go, channel and all its plumbing stay.
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string overlay = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsOverlay.h");
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");
  const std::string layout = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutBuild.inc.cpp");

  RequireContains(tabs, "class VoLumMidiChannelControl");
  RequireContains(tabs, "mChannel == 0 ? \"Omni\"");
  RequireContains(controls, "void SetMidiChannel(int channel)");
  RequireContains(layout, "settingsPage->SetMidiCallbacks([pPlugin](int channel)");

  // No Add / slot rows / choice menu anywhere in the Settings chrome.
  RequireDoesNotContain(overlay, "VoLumMidiSettingsControl");
  RequireDoesNotContain(tabs, "Add Sound");
  RequireDoesNotContain(tabs, "SOUND MAP");
  RequireDoesNotContain(controls, "Add Sound");
}

TEST_CASE("Settings reserves a labelled, inert Content library slot for Pack IO")
{
  // .scratch/pack/spec.md puts Export/Import Pack in Gear -> Settings, but Pack IO
  // is not merged on this branch: the row must be designed and dead, never a
  // button that looks live and crashes.
  const std::string tabs = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSettingsTabs.h");

  RequireContains(tabs, "class VoLumSettingsPackRowControl");
  RequireContains(tabs, "\"Export Pack\\xE2\\x80\\xA6\"");
  RequireContains(tabs, "\"Import Pack\\xE2\\x80\\xA6\"");
  RequireContains(tabs, "Not enabled in this build.");
  // Inert: no click handler, no action function, no callback.
  RequireDoesNotContain(tabs, "ExportPack(");
  RequireDoesNotContain(tabs, "ImportPack(");
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
  RequireContains(header, "kMainAmpPan");
  RequireContains(header, "kSupportAmpPan");
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
  RequireContains(hero, "FitTextToWidth(g, nameText, name, titleStrip.W()");
  RequireContains(hero, "g.DrawText(nameText, fitted.c_str(), titleStrip);");
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
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
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

TEST_CASE("Standalone settings persist every amp's preset selection, not just the focused amp's")
{
  // Reported from 1.2.1 testing: "only the last active amp remembers the selected
  // preset". The key pinned above is a single string describing whichever amp was
  // focused when the file was written, so every other amp reopened reading
  // "No Preset" - and an exit from an amp with nothing recalled wrote an empty id,
  // taking the focused one with it.
  const std::string plugin = ReadPluginSource();
  RequireContains(plugin, "j[\"volumActivePresetIdByOwner\"] = volum::VolumActivePresetIdsToJson(");
  RequireContains(
    plugin, "mVolumActivePresetIdByOwner = volum::VolumActivePresetIdsFromJson(j[\"volumActivePresetIdByOwner\"]);");

  // The live pair is folded into what gets written, so a save landing between a
  // recall and the next amp switch still records that recall.
  RequireContains(plugin, "activePresetIdsByOwner[_VolumActiveOwnerKey()] = mVolumActivePresetId;");

  // And the read side has to accept an id with no snapshot beside it, because the
  // file stores ids only. Requiring both - which is what shipped - meant every
  // selection restored from the file was discarded on the amp switch that should
  // have shown it, so persisting them would have changed nothing.
  RequireContains(plugin, "mVolumRecalledSnapshotByOwner[key] = pr.settings;");
  // Written as the conjunction rather than the whole statement: the old code made the
  // snapshot a precondition of using the id, so no persisted id could ever apply.
  // One line, so this does not depend on wrapping or line endings.
  RequireDoesNotContain(plugin, "&& itSnap != mVolumRecalledSnapshotByOwner.end())");
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

TEST_CASE("SerializeState flushes the content store when a custom amp is focused")
{
  // Regression (1.2.0): a focused custom amp keeps its scene in the shared
  // content library (only the id is in the DAW chunk). _VolumSaveCurrentToSettings
  // writes live knob edits into customScenes[id] in memory, but in a DAW/plugin
  // the store is otherwise only flushed on content CRUD - so live custom-amp
  // knob tweaks were lost on host restart. SerializeState (host state-save) and
  // the plugin-format destructor must flush the store when a custom amp is
  // focused. Pin both so the flush can't silently regress.
  const std::string source = ReadPluginSource();

  // Single-line substrings only: file line endings differ across platforms (CRLF
  // on Windows checkouts, LF on macOS/Linux), so a multi-line "\r\n" match would
  // pass on Windows but fail on macOS CI. Pin the guard + the flush call and the
  // save-current call so the persistence path can't silently regress.
  RequireContains(source, "const_cast<NeuralAmpModeler*>(this)->_VolumSaveCurrentToSettings();");
  RequireContains(source, "if (mVolumCustomMainIdx >= 0)");
  RequireContains(source, "volum::content::GlobalContentStore().Save();");
}

TEST_CASE("IR staging passes the UTF-8 path into ImpulseResponse")
{
  // AudioDSPTools #25: filesystem::path::string() is the ANSI code page on
  // Windows. _StageIR must hand irPath.Get() (UTF-8) to ImpulseResponse.
  const std::string source = ReadPluginSource();
  RequireContains(source, "stagedIR = std::make_unique<dsp::ImpulseResponse>(irPath.Get(), sampleRate);");
  RequireDoesNotContain(source, "irPathU8.string()");
}

TEST_CASE("Current-version chunk reader consumes the frozen 1.2.2 param prefix")
{
  // 1.2.0 read a live kNumParams loop. 1.3.0 freezes the prefix at 93 so a
  // later Chorus (or any) param bump cannot misalign a shipped 1.2.2 reader.
  // Extra EParams overlay from id-tail JSON, not extra prefix doubles.
  const std::string source = ReadPluginSource();

  RequireContains(source, "if (version >= volum::ChunkVersion(1, 2, 0))");
  RequireContains(source, "for (int i = 0; i < kVoLumChunkParamPrefixCount; ++i)");
  RequireContains(source, "paramNames.push_back(GetParam(i)->GetName());");
  RequireContains(source, "pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);");
  RequireContains(source, "for (int i = 0; i < kVoLumChunkParamPrefixCount && ok; ++i)");
  RequireDoesNotContain(source, "bool ok = SerializeParams(chunk);");
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

TEST_CASE("PRE/POST lock unlock restores local slot instead of flushing overlay")
{
  const std::string settings = ReadPluginSource();

  // Unlock restores from the active scene (focused custom amp scene, or the factory
  // amp slot via _VolumActiveScene()) instead of flushing the live overlay.
  RequireContains(settings, "_VolumRestorePreFromSlot(_VolumActiveScene());");
  RequireContains(settings, "_VolumRestorePostFromSlot(_VolumActiveScene());");
  RequireContains(settings, "if (!mVolumPreLocked)");
  RequireContains(settings, "if (!mVolumPostLocked)");
  RequireContains(settings, "_VolumStorePreToCurrentAmp()");
  RequireContains(settings, "_VolumStorePostToCurrentAmp()");
  RequireDoesNotContain(
    settings, "_VolumSavePreToSlot(mVolumAmpSettings[mVolumAmpIdx]);\r\n  mVolumPreLocked = false;");
  RequireDoesNotContain(
    settings, "_VolumSavePostToSlot(mVolumAmpSettings[mVolumAmpIdx]);\r\n  mVolumPostLocked = false;");
}

TEST_CASE("Legacy chunks without lock tail default to unlocked on deserialize")
{
  const std::string source = ReadPluginSource();

  RequireContains(source, "hasPrePostLockFlags");
  RequireContains(source, "volum::GetPrePostLockFlags(chunk, pos, pendingPreLocked, pendingPostLocked)");
  // The pending copies start unlocked, so a chunk with no lock tail still restores
  // unlocked - without the decoders being handed the live members.
  RequireContains(source, "bool pendingPreLocked = false;");
  RequireContains(source, "bool pendingPostLocked = false;");
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

  RequireContains(source, "volum::dsp_staging::StagePathOnSuccess(pathPair, irPath);");
  RequireContains(source, "volum::dsp_staging::CommitStagedPathOnApply(mNAMPaths);");
  RequireContains(source, "volum::dsp_staging::StagePathOnSuccess(mNAMPaths, modelPath);");
  RequireContains(source, "_VolumProcessMainAmpChain");
  RequireContains(source, "_VolumProcessDualAmpSupportLane");
  RequireContains(loader, "std::lock_guard<std::mutex> lock(mStagingMutex);");
  RequireContains(loader, "volum::dsp_staging::StagePathOnSuccess(mNAMPaths, result.path.c_str());");
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
  RequireContains(overlay, "const int now = id.empty() ? idx : RowIndexById(id);");
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
  RequireContains(overlay, "const int target = mRenameId.empty() ? mSel : RowIndexById(mRenameId);");
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

TEST_CASE("Every preset operation claims the shared bridge before using it")
{
  // The capture/apply hooks and the active owner key are process-global, so the
  // instance that last installed them decides whose rig a preset records and whose
  // rig a recall changes. Installing at construction handed that to whichever
  // instance the host created last.
  const std::string source = ReadPluginSource();

  RequireContains(source, "void NeuralAmpModeler::_VolumClaimPresetOps()");
  RequireContains(source, "volum::custom::PresetHookOwner() = this;");
  RequireContains(source, "volum::custom::ClearPresetHooksIfOwnedBy(this);");

  // Seven claims. Save, overwrite and recall are the three operations. The fourth is
  // the callback the layout hands the Manage overlay, so its rename and delete -
  // which go straight to the bridge rather than through the plugin - claim the bank
  // too. The last three are the READ sites: opening the preset menu, and the two
  // callbacks that bounds-check a chosen row. MockPresetsForAmp ignores its ampIdx
  // and resolves the bank through the global key, so listing without claiming shows
  // another instance's presets and hands their row numbers to this instance's bank.
  auto count = [&source](const char* needle) {
    const std::string n(needle);
    std::size_t total = 0;
    for (std::size_t pos = source.find(n); pos != std::string::npos; pos = source.find(n, pos + n.size()))
      ++total;
    return total;
  };

  CHECK(count("_VolumClaimPresetOps();") == 7);
  // Each read of the bank is preceded by a claim rather than following one.
  for (const char* readSite : {"const auto presets = volum::custom::MockPresetsForAmp(mVolumAmpIdx);",
                               "const auto presets = volum::custom::MockPresetsForAmp(pPlugin->mVolumAmpIdx);",
                               "const auto presets = volum::custom::MockPresetsForAmp(ampIdx);"})
  {
    const auto read = source.find(readSite);
    REQUIRE(read != std::string::npos);
    const auto claim = source.rfind("_VolumClaimPresetOps();", read);
    REQUIRE(claim != std::string::npos);
    // The first bank read after that claim is this one, so nothing reads the bank
    // between the two.
    const auto firstReadAfterClaim = source.find("MockPresetsForAmp", claim);
    CHECK(firstReadAfterClaim > read);
    CHECK(firstReadAfterClaim < read + std::string(readSite).size());
  }
  RequireContains(source, "[pPlugin]() { pPlugin->_VolumClaimPresetOps(); }");
  RequireContains(source, "volum::custom::AddPreset(mVolumAmpIdx");
  RequireContains(source, "volum::custom::OverwritePreset(mVolumAmpIdx");
  RequireContains(source, "volum::custom::RecallPreset(mVolumAmpIdx");

  // Three owner-key publishes: one inside the claim helper, plus the two read-only
  // sites that use no hook (the preset-bar refresh and the amp-switch sync). A
  // fourth appearing inside an operation would look like a claim while leaving the
  // hooks pointing at another instance.
  CHECK(count("volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());") == 3);
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
  // support amp's layout.
  const std::string source = ReadPluginSource();

  const auto clamp = source.find("void NeuralAmpModeler::_VolumClampSupportFocus()");
  REQUIRE(clamp != std::string::npos);
  const auto clampEnd = source.find("\n}", clamp);
  REQUIRE(clampEnd != std::string::npos);
  const std::string body = source.substr(clamp, clampEnd - clamp);
  RequireContains(body, "mVolumDualAmpFocusedSupport = false;");
  RequireContains(body, "_VolumApplyFocusedLaneCabs();");

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
  RequireContains(source, "if (superseded)");
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

  // The outcomes are still logged, just from the worker thread that produced them -
  // and worded for what that thread actually knows. It has read and parsed the file;
  // whether the model reaches the audio graph is decided later, in the drain, which
  // may discard it as superseded. "loaded" claimed the second thing.
  const std::string loaderBody = loader.substr(loaderMain);
  RequireContains(loaderBody, "VOLUM_LOG(\"model\"");
  RequireContains(loaderBody, "\" read \"");
  RequireContains(loaderBody, "\" load FAILED \"");
}

TEST_CASE("Closing the editor deactivates the tuner so the instance cannot stay muted")
{
  // An active tuner memsets every output channel (silenceForTuner in
  // ProcessBlock). mTunerDSP is a plugin member, so it outlives the editor, and
  // the only two places that can clear it -- the tuner toggle and the tuner
  // control's dismiss action -- both require an editor. Closing the plugin window
  // with the tuner open therefore silenced the instance for good, invisibly: the
  // editor is rebuilt with the tuner hidden, so reopening showed a normal UI over
  // a dead signal path.
  const std::string source = ReadPluginSource();

  const auto onUIClose = source.find("void NeuralAmpModeler::OnUIClose()");
  REQUIRE(onUIClose != std::string::npos);
  const auto body = source.substr(onUIClose, 700);

  // The deactivation has to live in OnUIClose itself, not merely somewhere in the
  // translation unit.
  RequireContains(body, "mTunerDSP.SetActive(false);");

  // Pin the two facts that make the above load-bearing, so this test keeps
  // failing for the right reason if either moves.
  RequireContains(source, "processingPlan.silenceForTuner");
  RequireContains(source, "mTunerDSP.IsActive()");
}
