#include "third_party/doctest.h"
#include "../config.h"
#include "../VoLumParams.h"
#include "../VoLumKeyboardModel.h"
#include "../VoLumDualAmpInput.h"

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
  for (auto other : {EVoLumEffectFocus::DELAY, EVoLumEffectFocus::REVERB, EVoLumEffectFocus::TREMOLO,
                     EVoLumEffectFocus::PITCH, EVoLumEffectFocus::COMP, EVoLumEffectFocus::PRE_NAM1,
                     EVoLumEffectFocus::PRE_NAM2, EVoLumEffectFocus::AMP})
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

