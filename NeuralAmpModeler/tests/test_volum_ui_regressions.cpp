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

TEST_CASE("All pedal controls remain editable while their block is bypassed")
{
  const std::string runtime = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLayoutRuntime.inc.cpp");

  // Pin every bypassable PRE/POST block before asserting the shared policy.
  for (const char* group : {"PITCH_TRANSPOSE_KNOBS", "PITCH_OCTAVER_KNOBS", "COMP_KNOBS", "PRE_NAM1_KNOBS",
                            "PRE_NAM2_KNOBS", "DELAY_KNOBS", "REVERB_KNOBS", "TREMOLO_KNOBS"})
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
  RequireContains(source, "return _CycleVoLumKeyboardTarget(key.VK == kVK_LEFT ? -1 : 1)");
  // POST Tab/arrow cycling must reach all three cards (Delay/Reverb/Tremolo), not
  // just toggle Delay<->Reverb (the "can't arrow to Tremolo in POST" bug).
  RequireContains(source, "mVolumFocusedEffect = targets[wrap(current + direction, 3)];");
  RequireContains(source, "Left/Right channel  |  S cab  |  Tab target");
  RequireContains(source, "spkCtrl->As<VoLumSpeakerRowControl>()->SetSelected");
  RequireContains(source, "Left/Right or Tab target");
  RequireContains(settings, "Shortcut info");
  RequireContains(settings, "\"1/2/3\", \"PRE / AMP / POST\"");
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
  RequireContains(source, "spkCtrl->As<VoLumSpeakerRowControl>()->SetSelected(mVolumSpeakerIdx);");
  RequireContains(source, "_UpdateVoLumLayout(pGfx);");
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
  RequireContains(source, "std::clamp(volum::VoLumTremoloSyncMs(postBpm, GetParam(kDelayDivision)->Int()), 10.0, 2000.0)");
  RequireContains(source, "volum::VoLumTremoloSyncRateHz(postBpm, GetParam(kTremoloDivision)->Int())");
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
  RequireContains(plugin, "mVolumRestoreCustomMainId = volum::ResolveRestoreCustomMainId(");
  RequireContains(plugin, "mVolumRestorePresetId = idTail.activePresetId;");
  RequireContains(plugin, "mVolumDidRestorePresetSelection = false;");
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
  CHECK(postCards.delay.L == doctest::Approx(327.f));
  CHECK(postCards.delay.R == doctest::Approx(471.f));
  CHECK(postCards.reverb.L == doctest::Approx(481.f));
  CHECK(postCards.reverb.R == doctest::Approx(625.f));
  CHECK(postCards.tremolo.L == doctest::Approx(635.f));
  CHECK(postCards.tremolo.R == doctest::Approx(779.f));
  CHECK(postCards.connector1.L == doctest::Approx(postCards.delay.R));
  CHECK(postCards.connector1.R == doctest::Approx(postCards.reverb.L));
  CHECK(postCards.connector2.L == doctest::Approx(postCards.reverb.R));
  CHECK(postCards.connector2.R == doctest::Approx(postCards.tremolo.L));
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

TEST_CASE("Current-version chunk reader consumes every serialized param (VST/AU state restore)")
{
  // Regression (1.2.0 critical): SerializeParams() writes ALL kNumParams param
  // doubles, but the pre-fix reader used a hand-maintained 71-entry list for every
  // version >= 0.9.0. When 1.2.0 appended ~22 params (kSupportIRToggle, PRE Pitch,
  // Tremolo, Delay sync) the reader stopped short, `pos` misaligned, and the
  // per-amp selection/scene read garbage -> VST3/AU "everything resets to default
  // on every load" (standalone masked it via volum-settings.json). Pin the
  // current-version branch that reads params by LIVE name so it can never drift
  // from the enum again.
  const std::string source = ReadPluginSource();

  RequireContains(source, "if (version >= volum::ChunkVersion(1, 2, 0))");
  RequireContains(source, "for (int i = 0; i < kNumParams; ++i)");
  RequireContains(source, "paramNames.push_back(GetParam(i)->GetName());");
  RequireContains(source, "pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);");
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

  RequireContains(source, "if (hasPrePostLockFlags)");
  RequireContains(source, "pos = volum::GetPrePostLockFlags(chunk, pos, mVolumPreLocked, mVolumPostLocked);");
  RequireContains(source, "mVolumPreLocked = false;");
  RequireContains(source, "mVolumPostLocked = false;");
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

TEST_CASE("VoLum settings panel shows current latency under model information")
{
  const std::string controls = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModelerControls.h");
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

  RequireContains(controls, "Current latency: %.1f ms (%d samples)");
  RequireContains(controls, "SetCurrentLatency(int samples, double sampleRate)");
  RequireContains(source, "SetCurrentLatency(GetLatency(), GetSampleRate())");
  RequireDoesNotContain(source, " |  Latency:");
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
  // enable comes from the resolver; selection checks ChannelHasDirect).
  RequireContains(speakerRow, "void SetIrEnabled(bool enabled");
  RequireContains(source, "row->SetIrEnabled(v.irEnabled");
  RequireContains(source, "!volum::custom::ChannelHasDirect(volum::custom::CustomAmpAt(customLane), laneChannel)");
}

TEST_CASE("Custom cab navigation is channel-first")
{
  const std::string source = ReadPluginSource();
  const std::string speakerRow = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumSpeakerRow.h");

  // The speaker row can gate the No Cab button (per-channel DIRECT availability).
  RequireContains(speakerRow, "void SetNoCabEnabled(bool enabled");
  // Custom lane cab refresh runs through the pure channel-first resolver.
  RequireContains(source, "volum::custom::ResolveLaneCabs(amp, curSlot, curCh)");
  RequireContains(source, "row->SetNoCabEnabled(v.noCabEnabled");
  RequireContains(source, "row->SetIrEnabled(v.irEnabled");
  // The custom channel stepper lists the amp-WIDE gain stages.
  RequireContains(source, "const auto channels = volum::custom::AssignedChannels(amp);");
  // Factory amps always re-enable No Cab (DIRECT on every channel).
  RequireContains(source, "row->SetNoCabEnabled(true);");
  // Custom IR selection is gated per-channel, not amp-wide.
  RequireContains(source, "volum::custom::ChannelHasDirect(volum::custom::CustomAmpAt(customLane), laneChannel)");
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

TEST_CASE("Keyboard and mouse toggles share one dirty-marking funnel")
{
  // Funnel B: the keyboard Space toggle historically skipped _VolumMarkPresetDirty
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
