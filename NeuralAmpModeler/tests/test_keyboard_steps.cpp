#include "third_party/doctest.h"
#include "../config.h"
#include "../VoLumParams.h"
#include "../VoLumKeyboardModel.h"

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
