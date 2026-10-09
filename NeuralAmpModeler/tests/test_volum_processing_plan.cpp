#include "third_party/doctest.h"
#include "../VoLumProcessingPlan.h"

TEST_CASE("Processing plan falls back without a main NAM model")
{
  const bool preNamActive[2] = {true, true};
  const bool havePreNam[2] = {true, false};

  const auto plan =
    volum::MakeProcessingPlan(false, true, true, true, true, true, preNamActive, havePreNam, true, true, false);

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

  const auto plan =
    volum::MakeProcessingPlan(true, false, true, true, true, false, preNamActive, havePreNam, true, true, true);

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

  const auto plan =
    volum::MakeProcessingPlan(true, false, false, true, false, false, preNamActive, havePreNam, false, false, false);

  CHECK(plan.runMainModel);
  CHECK_FALSE(plan.runIR);
}

TEST_CASE("Processing plan enables dual amp only when main and support models exist")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  const auto plan = volum::MakeProcessingPlan(
    true, true, true, false, false, false, preNamActive, havePreNam, true, true, false, true, true, true);

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

  const auto plan = volum::MakeProcessingPlan(
    true, false, false, false, false, false, preNamActive, havePreNam, true, true, false, true, false, true);

  CHECK(plan.runMainModel);
  CHECK_FALSE(plan.runSupportModel);
  CHECK_FALSE(plan.runDualAmp);
  CHECK(plan.runDelay);
  CHECK(plan.runReverb);
}

TEST_CASE("Processing plan sets silenceForTuner without disabling other stages")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  const auto plan =
    volum::MakeProcessingPlan(true, true, true, true, true, false, preNamActive, havePreNam, true, true, true);

  CHECK(plan.silenceForTuner);
  CHECK(plan.runMainModel);
  CHECK(plan.runNoiseGate);
  CHECK(plan.runToneStack);
  CHECK(plan.runIR);
  CHECK(plan.runDelay);
  CHECK(plan.runReverb);
}

TEST_CASE("Processing plan enables PRE NAM slots only when capture is loaded")
{
  const bool preNamActive[2] = {true, true};
  const bool havePreNam[2] = {true, false};

  const auto plan =
    volum::MakeProcessingPlan(false, false, false, false, false, false, preNamActive, havePreNam, false, false, false);

  CHECK(plan.runPreNam[0]);
  CHECK_FALSE(plan.runPreNam[1]);
  CHECK_FALSE(plan.runMainModel);
  CHECK(plan.runFallback);
  CHECK_FALSE(plan.runDelay);
  CHECK_FALSE(plan.runReverb);
}

TEST_CASE("Processing plan claims neither SUPPORT nor POST while MAIN is missing")
{
  // Dual Amp on, SUPPORT and its IR loaded, MAIN still loading. ProcessBlock only
  // runs SUPPORT beside MAIN, so the block is the silent fallback. The plan used to
  // say runSupportModel anyway, and the host latency counted SUPPORT for it.
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  const auto plan = volum::MakeProcessingPlan(false, false, false, false, false, false, preNamActive, havePreNam, true,
                                              true, false, /*dualAmpActive=*/true, /*haveSupportModel=*/true,
                                              /*supportToneStackActive=*/true, /*supportIrActive=*/true,
                                              /*haveSupportIR=*/true);

  CHECK_FALSE(plan.runMainModel);
  CHECK(plan.runFallback);
  CHECK_FALSE(plan.runSupportModel);
  CHECK_FALSE(plan.runDualAmp);
  CHECK_FALSE(plan.runSupportToneStack);
  CHECK_FALSE(plan.runSupportIR);
  CHECK_FALSE(plan.runDelay);
  CHECK_FALSE(plan.runReverb);
  CHECK_FALSE(plan.runToneStack);
  CHECK_FALSE(plan.runIR);
}

TEST_CASE("Host latency counts SUPPORT exactly when the plan runs it")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};
  constexpr int kMain = 0;
  constexpr int kSupport = 4096; // a resampled SUPPORT capture

  // MAIN missing, SUPPORT loaded: nothing plays, so there is nothing to delay.
  CHECK(volum::AmpLatencySamples(false, 0, true, true, kSupport) == 0);
  CHECK(volum::AmpLatencySamples(true, kMain, true, true, kSupport) == kSupport);
  CHECK(volum::AmpLatencySamples(true, 32, false, true, kSupport) == 32);
  CHECK(volum::AmpLatencySamples(true, 8192, true, true, kSupport) == 8192);

  for (int bits = 0; bits < 8; ++bits)
  {
    const bool haveMain = (bits & 1) != 0;
    const bool dual = (bits & 2) != 0;
    const bool haveSupport = (bits & 4) != 0;
    const auto plan = volum::MakeProcessingPlan(
      haveMain, false, false, false, false, false, preNamActive, havePreNam, false, false, false, dual, haveSupport);
    const bool latencyCountsSupport =
      volum::AmpLatencySamples(haveMain, kMain, dual, haveSupport, kSupport) == kSupport;
    INFO("main=" << haveMain << " dual=" << dual << " support=" << haveSupport);
    CHECK(plan.runSupportModel == latencyCountsSupport);
    CHECK(plan.runSupportModel == plan.runDualAmp);
  }
}

TEST_CASE("A loaded but deselected SUPPORT adds no host latency")
{
  CHECK(volum::HaveSelectedSupportModel(true, true));
  CHECK_FALSE(volum::HaveSelectedSupportModel(false, true));
  CHECK_FALSE(volum::HaveSelectedSupportModel(true, false));
  CHECK_FALSE(volum::HaveSelectedSupportModel(false, false));

  constexpr int kMain = 32;
  constexpr int kSupport = 4096;
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};
  for (const bool selected : {false, true})
  {
    const bool haveSupport = volum::HaveSelectedSupportModel(selected, /*supportLoaded=*/true);
    const auto plan = volum::MakeProcessingPlan(
      true, false, false, false, false, false, preNamActive, havePreNam, false, false, false, true, haveSupport);
    INFO("selected=" << selected);
    CHECK(plan.runSupportModel == selected);
    CHECK(volum::AmpLatencySamples(true, kMain, true, haveSupport, kSupport) == (selected ? kSupport : kMain));
  }
}

TEST_CASE("Processing plan disables support tone stack without a support model")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  const auto plan = volum::MakeProcessingPlan(
    true, false, false, false, false, false, preNamActive, havePreNam, false, false, false, true, false, true);

  CHECK(plan.runMainModel);
  CHECK_FALSE(plan.runSupportModel);
  CHECK_FALSE(plan.runSupportToneStack);
}

TEST_CASE("Processing plan runs the support IR only with a support model + IR present + toggle on")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};
  // dual active + main + support models present, support IR toggle on, support IR loaded.
  const auto plan = volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam, false,
                                              false, false, /*dualAmpActive=*/true, /*haveSupportModel=*/true,
                                              /*supportToneStackActive=*/false, /*supportIrActive=*/true,
                                              /*haveSupportIR=*/true);
  CHECK(plan.runSupportModel);
  CHECK(plan.runSupportIR);
  // The MAIN convolver is independent (no main IR loaded here).
  CHECK_FALSE(plan.runIR);
}

TEST_CASE("Processing plan refuses the support IR when its toggle is off or the IR is unavailable")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  // Toggle off.
  const auto toggleOff = volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam,
                                                   false, false, false, true, true, false, /*supportIrActive=*/false,
                                                   /*haveSupportIR=*/true);
  CHECK(toggleOff.runSupportModel);
  CHECK_FALSE(toggleOff.runSupportIR);

  // Toggle on but no IR loaded.
  const auto noIr = volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam, false,
                                              false, false, true, true, false, /*supportIrActive=*/true,
                                              /*haveSupportIR=*/false);
  CHECK(noIr.runSupportModel);
  CHECK_FALSE(noIr.runSupportIR);
}

TEST_CASE("Processing plan runs the PRE pitch pedal independently of the main model")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  // Pitch flag is the final argument. Default (omitted) keeps it off.
  const auto offByDefault =
    volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam, false, false, false);
  CHECK_FALSE(offByDefault.runPrePitch);

  // Pitch active even without a main model (fallback path) — it sits at the very
  // front of the PRE chain and is gated only by its own active flag.
  const auto pitchNoMain = volum::MakeProcessingPlan(false, false, false, false, false, false, preNamActive, havePreNam,
                                                     false, false, false, false, false, false, false, false,
                                                     /*prePitchActive=*/true);
  CHECK(pitchNoMain.runPrePitch);
  CHECK_FALSE(pitchNoMain.runMainModel);
  CHECK(pitchNoMain.runFallback);

  // Pitch active alongside a full main chain.
  const auto pitchWithMain = volum::MakeProcessingPlan(true, false, true, false, false, true, preNamActive, havePreNam,
                                                       false, false, false, false, false, false, false, false,
                                                       /*prePitchActive=*/true);
  CHECK(pitchWithMain.runPrePitch);
  CHECK(pitchWithMain.runPreComp);
  CHECK(pitchWithMain.runMainModel);
}

TEST_CASE("Processing plan gates the POST tremolo behind a model and its active flag")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  // Tremolo flag is the final argument. Default (omitted) keeps it off.
  const auto offByDefault =
    volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam, false, false, false);
  CHECK_FALSE(offByDefault.runTremolo);

  // Active with a main model -> runs.
  const auto withMain = volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam,
                                                  false, false, false, false, false, false, false, false,
                                                  /*prePitchActive=*/false, /*tremoloActive=*/true);
  CHECK(withMain.runTremolo);

  // Active but no model at all -> stays off (POST chain has nothing to modulate).
  const auto noModel = volum::MakeProcessingPlan(false, false, false, false, false, false, preNamActive, havePreNam,
                                                 false, false, false, false, false, false, false, false,
                                                 /*prePitchActive=*/false, /*tremoloActive=*/true);
  CHECK_FALSE(noModel.runTremolo);

  // Only a support model (MAIN still loading): the block is silent, so nothing runs.
  const auto supportOnly = volum::MakeProcessingPlan(false, false, false, false, false, false, preNamActive, havePreNam,
                                                     false, false, false, /*dualAmpActive=*/true,
                                                     /*haveSupportModel=*/true, false, false, false,
                                                     /*prePitchActive=*/false, /*tremoloActive=*/true);
  CHECK_FALSE(supportOnly.runSupportModel);
  CHECK_FALSE(supportOnly.runTremolo);
}

TEST_CASE("Processing plan gates the POST chorus behind a model and its active flag")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};

  // Chorus flag is the last argument. Default (omitted) keeps it off, which is
  // kChorusActive false: ProcessBlock must not call mChorus.Process.
  const auto offByDefault =
    volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam, false, false, false);
  CHECK_FALSE(offByDefault.runChorus);

  const auto withMain = volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam,
                                                  false, false, false, false, false, false, false, false,
                                                  /*prePitchActive=*/false, /*tremoloActive=*/false,
                                                  /*chorusActive=*/true);
  CHECK(withMain.runChorus);

  const auto noModel = volum::MakeProcessingPlan(false, false, false, false, false, false, preNamActive, havePreNam,
                                                 false, false, false, false, false, false, false, false, false, false,
                                                 /*chorusActive=*/true);
  CHECK_FALSE(noModel.runChorus);

  const auto supportOnly = volum::MakeProcessingPlan(false, false, false, false, false, false, preNamActive, havePreNam,
                                                     false, false, false, /*dualAmpActive=*/true,
                                                     /*haveSupportModel=*/true, false, false, false, false, false,
                                                     /*chorusActive=*/true);
  CHECK_FALSE(supportOnly.runSupportModel);
  CHECK_FALSE(supportOnly.runChorus);
}

TEST_CASE("Processing plan never runs the support IR while the support model is silent")
{
  const bool preNamActive[2] = {false, false};
  const bool havePreNam[2] = {false, false};
  // Support IR toggle on + IR present, but dual amp off -> no support model -> no support IR.
  const auto plan = volum::MakeProcessingPlan(true, false, false, false, false, false, preNamActive, havePreNam, false,
                                              false, false, /*dualAmpActive=*/false, /*haveSupportModel=*/true, false,
                                              /*supportIrActive=*/true, /*haveSupportIR=*/true);
  CHECK_FALSE(plan.runSupportModel);
  CHECK_FALSE(plan.runSupportIR);
}
