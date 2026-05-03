#include "third_party/doctest.h"
#include "../VoLumProcessingPlan.h"

TEST_CASE("Processing plan falls back without a main NAM model")
{
  const bool preNamActive[2] = {true, true};
  const bool havePreNam[2] = {true, false};

  const auto plan = volum::MakeProcessingPlan(false, true, true, true, true, true, preNamActive, havePreNam, true, true,
                                             false);

  CHECK(plan.runPreComp);
  CHECK(plan.runPreNam[0]);
  CHECK_FALSE(plan.runPreNam[1]);
  CHECK(plan.runNoiseGate);
  CHECK_FALSE(plan.runMainModel);
  CHECK(plan.runFallback);
  CHECK_FALSE(plan.runToneStack);
  CHECK_FALSE(plan.runIR);
  CHECK_FALSE(plan.runDelay);
  CHECK_FALSE(plan.runReverb);
}

TEST_CASE("Processing plan enables model-only post chain when prerequisites exist")
{
  const bool preNamActive[2] = {false, true};
  const bool havePreNam[2] = {true, true};

  const auto plan = volum::MakeProcessingPlan(true, false, true, true, true, false, preNamActive, havePreNam, true, true,
                                             true);

  CHECK_FALSE(plan.runPreComp);
  CHECK_FALSE(plan.runPreNam[0]);
  CHECK(plan.runPreNam[1]);
  CHECK_FALSE(plan.runNoiseGate);
  CHECK(plan.runMainModel);
  CHECK_FALSE(plan.runFallback);
  CHECK(plan.runToneStack);
  CHECK(plan.runIR);
  CHECK(plan.runDelay);
  CHECK(plan.runReverb);
  CHECK(plan.silenceForTuner);
}

TEST_CASE("Processing plan refuses IR when the IR is unavailable")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  const auto plan = volum::MakeProcessingPlan(true, false, false, true, false, false, preNamActive, havePreNam, false,
                                             false, false);

  CHECK(plan.runMainModel);
  CHECK_FALSE(plan.runIR);
}

TEST_CASE("Processing plan enables dual amp only when main and support models exist")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  const auto plan = volum::MakeProcessingPlan(true, true, true, false, false, false, preNamActive, havePreNam, true,
                                             true, false, true, true, true);

  CHECK(plan.runMainModel);
  CHECK(plan.runSupportModel);
  CHECK(plan.runDualAmp);
  CHECK(plan.runToneStack);
  CHECK(plan.runSupportToneStack);
  CHECK(plan.runDelay);
  CHECK(plan.runReverb);
}

TEST_CASE("Processing plan keeps dual amp disabled until support model loads")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  const auto plan = volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam, true,
                                             true, false, true, false, true);

  CHECK(plan.runMainModel);
  CHECK_FALSE(plan.runSupportModel);
  CHECK_FALSE(plan.runDualAmp);
  CHECK(plan.runDelay);
  CHECK(plan.runReverb);
}
