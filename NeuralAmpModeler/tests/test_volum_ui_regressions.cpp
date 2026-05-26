#include "third_party/doctest.h"
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
  blob += ReadText(root / "VoLumProcessBlock.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumLoader.inc.cpp");
  blob += "\n";
  blob += ReadText(root / "VoLumSettings.inc.cpp");
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
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(source, "card->SetActiveState(GetParam(kDelayActive)->Bool());");
  RequireContains(source, "card->SetActiveState(GetParam(kReverbActive)->Bool());");
  RequireContains(triptych, "{ EVoLumEffectFocus::REVERB, \"REVRB\", kReverbActive }");
}

TEST_CASE("Collapsed PRE slots show selected pedal short labels")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "mPreNam1Label = (preNam1Label && preNam1Label[0] != '\\0') ? preNam1Label : \"NAM 1\";");
  RequireContains(triptych, "preSlots[1].label = mPreNam1Label.c_str();");
  RequireContains(source, "_VolumGetPreCaptureShortLabel(GetParam(kPreNam1Capture)->Int(), \"NAM 1\")");
  RequireContains(source, "_VolumGetPreCaptureShortLabel(GetParam(kPreNam2Capture)->Int(), \"NAM 2\")");
  RequireContains(triptych, "std::toupper(c)");
  RequireContains(triptych, "IText labelText(10.f");
}

TEST_CASE("Dual amp pan knobs only show in AMP view")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

  RequireContains(source, "const bool showPanKnobs = dualActive && mVolumExpandedSection == EVoLumSection::AMP;");
  RequireContains(source, "c->Hide(!showPanKnobs);");
  RequireContains(source, "Pan the SUPPORT amp lane.");
  RequireContains(source, "Pan the MAIN amp lane.");
}

TEST_CASE("Support amp keyboard channel navigation refreshes support stepper")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

  RequireContains(source, "GetControlWithTag(kCtrlTagVoLumSupportChannelStep)");
  RequireContains(source, "SetChannels(mVolumSupportChannelLabels, next)");
  RequireContains(source, "mVolumSettingsDirty = true;");
}

TEST_CASE("Keyboard accessibility layer keeps section and target shortcuts")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
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
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

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
  RequireContains(hero, "name, titleStrip);");
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

  CHECK(count == 5);
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

TEST_CASE("PRE pedal capture menu toggles closed on second click of same pedal")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  // VoLumPreCaptureMenuControl moved to its own header on the 1.0 hygiene
  // split (see VoLumTriptych.h umbrella include).
  const std::string menus = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptychMenus.h");

  RequireContains(menus, "int GetSlot() const { return mSlot; }");
  RequireContains(source, "if (!rawCtrl->IsHidden() && menu && menu->GetSlot() == slot)");
  RequireContains(source, "_VolumHidePreCaptureMenu();");
}

TEST_CASE("PRE pedal capture menu closes from main-area outside click")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

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
  RequireContains(pedalCard, "if (!g.CheckLayer(mArtLayer) || mCachedBypassed != bypassed)");
  RequireContains(ampList, "if (!g.CheckLayer(mIconLayers[i]))");
  RequireDoesNotContain(triptych, "|| g.CheckLayer(");
  RequireDoesNotContain(pedalCard, "|| g.CheckLayer(");
  RequireDoesNotContain(ampList, "|| g.CheckLayer(");
  RequireDoesNotContain(coreControls, "|| g.CheckLayer(");
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
  CHECK(triptych.L == doctest::Approx(140.f));
  CHECK(triptych.R == doctest::Approx(760.f));
  CHECK(triptych.H() == doctest::Approx(volum::triptych_layout::kTriptychH));

  const auto ampFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::AMP);
  CHECK(ampFrames.pre.L == doctest::Approx(140.f));
  CHECK(ampFrames.pre.R == doctest::Approx(240.f));
  CHECK(ampFrames.amp.L == doctest::Approx(250.f));
  CHECK(ampFrames.amp.R == doctest::Approx(650.f));
  CHECK(ampFrames.post.L == doctest::Approx(660.f));
  CHECK(ampFrames.post.R == doctest::Approx(760.f));

  const auto postFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::POST);
  CHECK(postFrames.pre.L == doctest::Approx(140.f));
  CHECK(postFrames.amp.L == doctest::Approx(250.f));
  CHECK(postFrames.amp.R == doctest::Approx(320.f));
  CHECK(postFrames.post.L == doctest::Approx(330.f));
  CHECK(postFrames.post.R == doctest::Approx(760.f));
}

TEST_CASE("Triptych shared layout keeps expanded pedal card geometry aligned")
{
  const auto triptych = volum::triptych_layout::BoundsForCenter(450.f, 100.f);
  const auto preFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::PRE);
  const auto preCards = volum::triptych_layout::ComputePreCards(preFrames.pre);

  CHECK(preCards.comp.L == doctest::Approx(154.f));
  CHECK(preCards.comp.R == doctest::Approx(282.6667f));
  CHECK(preCards.nam1.L == doctest::Approx(290.6667f));
  CHECK(preCards.nam2.R == doctest::Approx(556.f));
  CHECK(preCards.connector1.L == doctest::Approx(preCards.comp.R));
  CHECK(preCards.connector1.R == doctest::Approx(preCards.nam1.L));

  const auto postFrames = volum::triptych_layout::ComputeFrames(triptych, EVoLumSection::POST);
  const auto postCards = volum::triptych_layout::ComputePostCards(postFrames.post);
  CHECK(postCards.delay.L == doctest::Approx(344.f));
  CHECK(postCards.delay.R == doctest::Approx(540.f));
  CHECK(postCards.reverb.L == doctest::Approx(550.f));
  CHECK(postCards.reverb.R == doctest::Approx(746.f));
  CHECK(postCards.connector.L == doctest::Approx(postCards.delay.R));
  CHECK(postCards.connector.R == doctest::Approx(postCards.reverb.L));
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

  RequireContains(settings, "_VolumRestorePreFromSlot(mVolumAmpSettings[mVolumAmpIdx]);");
  RequireContains(settings, "_VolumRestorePostFromSlot(mVolumAmpSettings[mVolumAmpIdx]);");
  RequireContains(settings, "if (!mVolumPreLocked)");
  RequireContains(settings, "if (!mVolumPostLocked)");
  RequireContains(settings, "_VolumStorePreToCurrentAmp()");
  RequireContains(settings, "_VolumStorePostToCurrentAmp()");
  RequireDoesNotContain(settings, "_VolumSavePreToSlot(mVolumAmpSettings[mVolumAmpIdx]);\r\n  mVolumPreLocked = false;");
  RequireDoesNotContain(settings, "_VolumSavePostToSlot(mVolumAmpSettings[mVolumAmpIdx]);\r\n  mVolumPostLocked = false;");
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

  RequireContains(source, "volum::dsp_staging::StagePathOnSuccess(mIRPaths, irPath);");
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

TEST_CASE("VoLum NAM cache copies dspData before Core consumes weights")
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
