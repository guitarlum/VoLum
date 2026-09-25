#include "third_party/doctest.h"
#include "../config.h"
#include "../VoLumParams.h"
#include "../VoLumKeyboardModel.h"
#include "../VoLumDualAmpInput.h"
#include "../VoLumHeaderChrome.h"
#include "../VoLumTunerDSP.h"
#include "../VoLumMetronomeDSP.h"

TEST_CASE("Keyboard step sizes")
{
  struct Row
  {
    int param;
    double coarse;
    double fine;
  };
  const Row rows[] = {
    {kDelayTime, 5.0, 1.0},
    {kTremoloRate, 0.5, 0.1},
    {kTremoloCrossover, 5.0, 1.0},
    {kTremoloDepth, 0.05, 0.01},
    {kTremoloShape, 0.05, 0.01},
    {kTremoloMix, 0.05, 0.01},
    {kChorusRate, 0.05, 0.01},
    {kChorusDepth, 0.05, 0.01},
    {kChorusTone, 0.05, 0.01},
    {kChorusWidth, 0.05, 0.01},
    {kChorusMix, 0.05, 0.01},
    {kDelayMix, 0.05, 0.01},
    {kDelayFeedback, 0.05, 0.01},
    {kReverbMix, 0.05, 0.01},
    {kReverbDecay, 0.05, 0.01},
    {kReverbPreDelay, 5.0, 1.0},
    {kReverbShimmer, 0.05, 0.01},
    {kToneBass, 0.5, 0.1},
    {kToneMid, 0.5, 0.1},
    {kToneTreble, 0.5, 0.1},
    {kReverbTone, 0.5, 0.1},
    {kSupportToneBass, 0.5, 0.1},
    {kSupportToneMid, 0.5, 0.1},
    {kSupportToneTreble, 0.5, 0.1},
    {kInputLevel, 0.5, 0.1},
    {kOutputLevel, 0.5, 0.1},
    {kSupportInputLevel, 0.5, 0.1},
    {kSupportOutputLevel, 0.5, 0.1},
    {kPreNam1Gain, 0.5, 0.1},
    {kPreNam1Level, 0.5, 0.1},
    {kPreNam2Gain, 0.5, 0.1},
    {kPreNam2Level, 0.5, 0.1},
    {kPreCompLevel, 0.5, 0.1},
    {kNoiseGateThreshold, 1.0, 0.1},
    {kSupportNoiseGateThreshold, 1.0, 0.1},
    {kDelayTone, 0.05, 0.01},
    {kDelayAge, 0.05, 0.01},
    {kPrePitchSemitones, 1.0, 1.0},
    {kPrePitchMix, 0.05, 0.01},
    {kPrePitchOctDown, 0.05, 0.01},
    {kPrePitchOctUp, 0.05, 0.01},
    {kPrePitchDry, 0.05, 0.01},
    {kPrePitchLevel, 0.5, 0.1},
    {kMainAmpPan, 0.05, 0.01},
    {kSupportAmpPan, 0.05, 0.01},
  };
  for (const auto& row : rows)
  {
    INFO("param " << row.param);
    CHECK(volum::keyboard::StepForParam(row.param, false) == row.coarse);
    CHECK(volum::keyboard::StepForParam(row.param, true) == row.fine);
  }
}

TEST_CASE("Keyboard: CHORUS is a distinct focus target with its own knob memory slot")
{
  using namespace volum::keyboard;
  const int chorus = TargetIndex(EVoLumEffectFocus::CHORUS, false);
  CHECK(chorus < kTargetCount);
  for (auto other :
       {EVoLumEffectFocus::DELAY, EVoLumEffectFocus::REVERB, EVoLumEffectFocus::TREMOLO, EVoLumEffectFocus::PITCH,
        EVoLumEffectFocus::COMP, EVoLumEffectFocus::PRE_NAM1, EVoLumEffectFocus::PRE_NAM2, EVoLumEffectFocus::AMP})
  {
    INFO("vs focus " << static_cast<int>(other));
    CHECK(TargetIndex(other, false) != chorus);
  }
  CHECK(kChorusParams.size() == 5);
  CHECK(Contains(kChorusParams, kChorusWidth));
}

TEST_CASE("Keyboard Dual Amp focus changes require a cab-row rederive")
{
  using volum::dualamp::CommitFocus;

  // Tab MAIN -> SUPPORT with a partner loaded.
  {
    const auto c = CommitFocus(/*previous=*/false, /*requested=*/true, /*hasSupportAmp=*/true);
    CHECK(c.supportFocused);
    CHECK(c.rederiveCabs);
  }
  // Tab SUPPORT -> MAIN.
  {
    const auto c = CommitFocus(true, false, true);
    CHECK_FALSE(c.supportFocused);
    CHECK(c.rederiveCabs);
  }
  // `2` while SUPPORT is focused lands AMP on MAIN.
  {
    const auto c = CommitFocus(true, false, true);
    CHECK_FALSE(c.supportFocused);
    CHECK(c.rederiveCabs);
  }
  // Dual-on with a partner follows into SUPPORT.
  {
    const auto c = CommitFocus(false, true, true);
    CHECK(c.supportFocused);
    CHECK(c.rederiveCabs);
  }
  // Dual-on with an empty SUPPORT lane cannot keep focus, so the row stays MAIN.
  {
    const auto c = CommitFocus(false, true, false);
    CHECK_FALSE(c.supportFocused);
    CHECK_FALSE(c.rederiveCabs);
  }
  // Pressing `2` while MAIN is already focused is not a focus change.
  {
    const auto c = CommitFocus(false, false, true);
    CHECK_FALSE(c.supportFocused);
    CHECK_FALSE(c.rederiveCabs);
  }
}

TEST_CASE("A visible overlay blocks global hotkeys; Escape peels the topmost overlay")
{
  using namespace volum::keyboard;

  const OverlayStack empty{};

  OverlayStack manage;
  manage.custom = true;

  OverlayStack tuner;
  tuner.tuner = true;
  tuner.knobSelected = true;

  OverlayStack tunerOverSettings;
  tunerOverSettings.tuner = true;
  tunerOverSettings.settings = true;

  OverlayStack metro;
  metro.metronome = true;

  OverlayStack settings;
  settings.settings = true;

  OverlayStack settingsMidi;
  settingsMidi.settings = true;
  settingsMidi.settingsMidiArmed = true;

  OverlayStack midiBoard;
  midiBoard.settings = true;
  midiBoard.settingsMidiBoard = true;

  OverlayStack tunerOverMidiBoard = midiBoard;
  tunerOverMidiBoard.tuner = true;

  OverlayStack pack;
  pack.pack = true;

  OverlayStack dropdown;
  dropdown.dropdown = true;

  OverlayStack confirm;
  confirm.confirm = true;

  OverlayStack nameDialog;
  nameDialog.nameDialog = true;

  OverlayStack tunerOverName;
  tunerOverName.nameDialog = true;
  tunerOverName.tuner = true;

  OverlayStack knob;
  knob.knobSelected = true;

  OverlayStack exact;
  exact.exactEntry = true;

  OverlayStack text;
  text.textEntry = true;

  struct Row
  {
    const char* name;
    OverlayStack stack;
    KeyKind kind;
    KeyConsumer want;
  };
  const Row rows[] = {
    {"empty Esc", empty, KeyKind::Escape, KeyConsumer::Rig},
    {"empty H", empty, KeyKind::HotkeyH, KeyConsumer::Rig},
    {"empty T", empty, KeyKind::HotkeyT, KeyConsumer::Rig},
    {"empty M", empty, KeyKind::HotkeyM, KeyConsumer::Rig},
    {"empty arrow", empty, KeyKind::Arrow, KeyConsumer::Rig},
    {"manage Esc", manage, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"manage H", manage, KeyKind::HotkeyH, KeyConsumer::Swallow},
    {"manage T", manage, KeyKind::HotkeyT, KeyConsumer::Swallow},
    {"manage M", manage, KeyKind::HotkeyM, KeyConsumer::Swallow},
    {"manage 1/S/Tab", manage, KeyKind::Other, KeyConsumer::Swallow},
    {"manage arrow", manage, KeyKind::Arrow, KeyConsumer::OverlayNav},
    {"tuner Esc peels tuner before knob", tuner, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"tuner H", tuner, KeyKind::HotkeyH, KeyConsumer::Swallow},
    {"tuner T", tuner, KeyKind::HotkeyT, KeyConsumer::Swallow},
    {"tuner over settings Esc", tunerOverSettings, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"tuner over settings H", tunerOverSettings, KeyKind::HotkeyH, KeyConsumer::Swallow},
    {"metro Esc", metro, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"metro H", metro, KeyKind::HotkeyH, KeyConsumer::Swallow},
    {"settings Esc", settings, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"settings H closes", settings, KeyKind::HotkeyH, KeyConsumer::CloseOverlay},
    {"settings T", settings, KeyKind::HotkeyT, KeyConsumer::Swallow},
    {"settings MIDI Esc peels picker", settingsMidi, KeyKind::Escape, KeyConsumer::PeelSettingsMidi},
    {"MIDI board PageUp/PageDown page the banks", midiBoard, KeyKind::Page, KeyConsumer::SettingsMidiPage},
    {"other Settings tab or the picker: page keys swallowed", settings, KeyKind::Page, KeyConsumer::Swallow},
    {"tuner above the MIDI board swallows page keys", tunerOverMidiBoard, KeyKind::Page, KeyConsumer::Swallow},
    {"MIDI board still swallows other keys", midiBoard, KeyKind::Other, KeyConsumer::Swallow},
    {"page keys with nothing open go to the rig", empty, KeyKind::Page, KeyConsumer::Rig},
    {"pack Esc", pack, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"pack H closes", pack, KeyKind::HotkeyH, KeyConsumer::CloseOverlay},
    {"dropdown Esc", dropdown, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"dropdown T", dropdown, KeyKind::HotkeyT, KeyConsumer::Swallow},
    {"confirm Enter reaches the dialog", confirm, KeyKind::Enter, KeyConsumer::ConfirmEnter},
    {"confirm H", confirm, KeyKind::HotkeyH, KeyConsumer::Swallow},
    {"name dialog Enter commits through the dialog", nameDialog, KeyKind::Enter, KeyConsumer::NameDialogKey},
    {"name dialog Esc cancels through the dialog", nameDialog, KeyKind::Escape, KeyConsumer::NameDialogKey},
    {"name dialog H is typed, not Settings", nameDialog, KeyKind::HotkeyH, KeyConsumer::NameDialogKey},
    {"name dialog T is typed, not the tuner", nameDialog, KeyKind::HotkeyT, KeyConsumer::NameDialogKey},
    {"name dialog arrows move the caret", nameDialog, KeyKind::Arrow, KeyConsumer::NameDialogKey},
    {"name dialog letters are typed", nameDialog, KeyKind::Other, KeyConsumer::NameDialogKey},
    {"tuner above the name dialog peels first", tunerOverName, KeyKind::Escape, KeyConsumer::CloseOverlay},
    {"knob Esc", knob, KeyKind::Escape, KeyConsumer::Knob},
    {"knob arrow stays on knob", knob, KeyKind::Arrow, KeyConsumer::Knob},
    {"knob T still opens tuner", knob, KeyKind::HotkeyT, KeyConsumer::Rig},
    {"knob Enter opens the exact value", knob, KeyKind::Enter, KeyConsumer::Knob},
    {"knob Delete resets the knob", knob, KeyKind::Delete, KeyConsumer::Knob},
    {"knob Ctrl+S / Tab / 1-3 / S keep their global meaning", knob, KeyKind::Other, KeyConsumer::Rig},
    {"exact Esc", exact, KeyKind::Escape, KeyConsumer::CancelExactEntry},
    {"text Esc", text, KeyKind::Escape, KeyConsumer::PassToTextEntry},
    {"text H", text, KeyKind::HotkeyH, KeyConsumer::PassToTextEntry},
  };

  for (const auto& row : rows)
  {
    INFO(row.name);
    CHECK(RouteKey(row.stack, row.kind) == row.want);
  }

  CHECK(TopOverlay(tunerOverSettings) == OverlayId::Tuner);
  CHECK(TopOverlay(manage) == OverlayId::Custom);
  CHECK(ClassifyVk(kKeyEscape) == KeyKind::Escape);
  CHECK(ClassifyVk('H') == KeyKind::HotkeyH);
  CHECK(ClassifyVk('t') == KeyKind::HotkeyT);
  CHECK(ClassifyVk(kKeyUp) == KeyKind::Arrow);
  CHECK(ClassifyVk(kKeyDelete) == KeyKind::Delete);
  CHECK(ClassifyVk(kKeyBack) == KeyKind::Delete);
  CHECK(ClassifyVk(kKeyPageUp) == KeyKind::Page);
  CHECK(ClassifyVk(kKeyPageDown) == KeyKind::Page);
}

TEST_CASE("Ctrl+S reaches the save shortcut from every BUILD focus state, never through an overlay")
{
  // The Rig consumer is the path that runs _HandleVoLumKeyboardFocusKey, where Ctrl+S
  // opens the save dialog. A selected knob is what PRE / POST editing leaves behind:
  // click a knob, then Ctrl+S.
  using namespace volum::keyboard;
  const KeyKind ctrlS = ClassifyVk('S');
  REQUIRE(ctrlS == ClassifyVk('s'));

  OverlayStack nothing;
  OverlayStack knob;
  knob.knobSelected = true;
  OverlayStack knobWithExactBox;
  knobWithExactBox.knobSelected = true;
  knobWithExactBox.exactEntry = true;
  for (const auto& s : {nothing, knob, knobWithExactBox})
    CHECK(RouteKey(s, ctrlS) == KeyConsumer::Rig);

  for (int i = 0; i < 8; ++i)
  {
    OverlayStack s;
    s.knobSelected = (i % 2) == 1;
    switch (i / 2)
    {
      case 0: s.settings = true; break;
      case 1: s.custom = true; break;
      case 2: s.tuner = true; break;
      default: s.dropdown = true; break;
    }
    INFO("overlay case " << i);
    CHECK(RouteKey(s, ctrlS) == KeyConsumer::Swallow);
  }
}

TEST_CASE("Up/Down on a selected knob are consumed even when the value cannot move")
{
  using namespace volum::keyboard;
  CHECK(SelectedKnobConsumesKind(KeyKind::Arrow, false));
  CHECK(SelectedKnobConsumesKind(KeyKind::Arrow, true));
  CHECK_FALSE(SelectedKnobConsumesKind(KeyKind::Other, false));
  CHECK(SelectedKnobConsumesKind(KeyKind::Enter, true));
  CHECK_FALSE(SelectedKnobConsumesKind(KeyKind::Enter, false));
}

TEST_CASE("Delay/Tremolo keyboard lists drop TIME/RATE while tempo-sync is on")
{
  using namespace volum::keyboard;
  CHECK_FALSE(Contains(kDelaySyncedParams, kDelayTime));
  CHECK(Contains(kDelaySyncedParams, kDelayFeedback));
  CHECK(kDelaySyncedParams.front() == DefaultDelayKnob(true));
  CHECK(DefaultDelayKnob(false) == kDelayTime);
  CHECK_FALSE(Contains(kTremoloSyncedParams, kTremoloRate));
  CHECK(kTremoloSyncedParams.front() == DefaultTremoloKnob(true));
  CHECK(DefaultTremoloKnob(false) == kTremoloRate);
  CHECK_FALSE(Contains(kTremoloHarmonicSyncedParams, kTremoloRate));
  CHECK(Contains(kTremoloHarmonicSyncedParams, kTremoloCrossover));
  CHECK(kTremoloHarmonicSyncedParams.front() == DefaultTremoloKnob(true));

  const int rememberedTime = kDelayTime;
  const int landed = Contains(kDelaySyncedParams, rememberedTime) ? rememberedTime : kDelaySyncedParams.front();
  CHECK(landed == kDelayFeedback);
}

TEST_CASE("Leaving the MIDI tab disarms Add/picker")
{
  using volum::keyboard::HideDisarmsMidiSubscreen;
  CHECK(HideDisarmsMidiSubscreen(true, false));
  CHECK_FALSE(HideDisarmsMidiSubscreen(true, true));
  CHECK_FALSE(HideDisarmsMidiSubscreen(false, false));
  CHECK_FALSE(HideDisarmsMidiSubscreen(false, true));
}

TEST_CASE("Disabled controls keep hover for tooltips and refuse clicks")
{
  CHECK(volum::DisabledPointerPolicy::kMouseOverWhenDisabled);
  CHECK_FALSE(volum::DisabledPointerPolicy::kMouseEventsWhenDisabled);
}

TEST_CASE("Editor close stops tuner mute and metronome click together")
{
  volum::TunerDSP tuner;
  volum::MetronomeDSP metro;
  tuner.SetActive(true);
  metro.SetActive(true);
  REQUIRE(tuner.IsActive());
  REQUIRE(metro.IsActive());
  volum::HaltEditorOwnedOverlayDsp(tuner, metro);
  CHECK_FALSE(tuner.IsActive());
  CHECK_FALSE(metro.IsActive());
}
