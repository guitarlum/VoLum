#include "third_party/doctest.h"
#include "../VoLumUserSettingsIO.h"

TEST_CASE("VolumUserSettings JSON roundtrip preserves amp state")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].speakerIdx = 2;
  amps[0].channelIdx = 1;
  amps[0].inputLevel = 1.5;
  amps[0].gateThreshold = -40.0;
  amps[0].toneBass = 3.0;
  amps[0].toneMid = 4.0;
  amps[0].toneTreble = 6.0;
  amps[0].outputLevel = -2.0;
  amps[0].noiseGateActive = false;
  amps[0].eqActive = false;
  amps[0].preCompActive = true;
  amps[0].preCompAmount = 6.5;
  amps[0].preCompRatio = 8.0;
  amps[0].preCompAttack = 2.5;
  amps[0].preCompRelease = 180.0;
  amps[0].preCompMix = 0.65;
  amps[0].preCompLevel = 1.5;
  amps[0].preNam1Active = true;
  amps[0].preNam1Capture = 2;
  amps[0].preNam1Gain = -3.0;
  amps[0].preNam1Bass = 4.0;
  amps[0].preNam1Mid = 6.0;
  amps[0].preNam1MidFreq = 750.0;
  amps[0].preNam1Treble = 7.0;
  amps[0].preNam1Level = -1.0;
  amps[0].preNam2Active = true;
  amps[0].preNam2Capture = 4;
  amps[0].preNam2Gain = 2.0;
  amps[0].preNam2Bass = 3.0;
  amps[0].preNam2Mid = 5.5;
  amps[0].preNam2MidFreq = 1200.0;
  amps[0].preNam2Treble = 8.0;
  amps[0].preNam2Level = 0.5;

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  int lastAmp = -1;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, &lastAmp);

  REQUIRE(lastAmp == 0);
  REQUIRE(loaded[0].speakerIdx == 2);
  REQUIRE(loaded[0].channelIdx == 1);
  REQUIRE(loaded[0].inputLevel == doctest::Approx(1.5));
  REQUIRE(loaded[0].gateThreshold == doctest::Approx(-40.0));
  REQUIRE(loaded[0].toneBass == doctest::Approx(3.0));
  REQUIRE(loaded[0].toneMid == doctest::Approx(4.0));
  REQUIRE(loaded[0].toneTreble == doctest::Approx(6.0));
  REQUIRE(loaded[0].outputLevel == doctest::Approx(-2.0));
  REQUIRE(loaded[0].noiseGateActive == false);
  REQUIRE(loaded[0].eqActive == false);
  REQUIRE(loaded[0].preCompActive == true);
  REQUIRE(loaded[0].preCompAmount == doctest::Approx(6.5));
  REQUIRE(loaded[0].preCompRatio == doctest::Approx(8.0));
  REQUIRE(loaded[0].preCompAttack == doctest::Approx(2.5));
  REQUIRE(loaded[0].preCompRelease == doctest::Approx(180.0));
  REQUIRE(loaded[0].preCompMix == doctest::Approx(0.65));
  REQUIRE(loaded[0].preCompLevel == doctest::Approx(1.5));
  REQUIRE(loaded[0].preNam1Active == true);
  REQUIRE(loaded[0].preNam1Capture == 2);
  REQUIRE(loaded[0].preNam1Gain == doctest::Approx(-3.0));
  REQUIRE(loaded[0].preNam1Bass == doctest::Approx(4.0));
  REQUIRE(loaded[0].preNam1Mid == doctest::Approx(6.0));
  REQUIRE(loaded[0].preNam1MidFreq == doctest::Approx(750.0));
  REQUIRE(loaded[0].preNam1Treble == doctest::Approx(7.0));
  REQUIRE(loaded[0].preNam1Level == doctest::Approx(-1.0));
  REQUIRE(loaded[0].preNam2Active == true);
  REQUIRE(loaded[0].preNam2Capture == 4);
  REQUIRE(loaded[0].preNam2Gain == doctest::Approx(2.0));
  REQUIRE(loaded[0].preNam2Bass == doctest::Approx(3.0));
  REQUIRE(loaded[0].preNam2Mid == doctest::Approx(5.5));
  REQUIRE(loaded[0].preNam2MidFreq == doctest::Approx(1200.0));
  REQUIRE(loaded[0].preNam2Treble == doctest::Approx(8.0));
  REQUIRE(loaded[0].preNam2Level == doctest::Approx(0.5));

  // Other amps unchanged (defaults)
  for (int i = 1; i < volum::kAmpCount; ++i)
  {
    REQUIRE(loaded[i].speakerIdx == 3);
    REQUIRE(loaded[i].noiseGateActive == true);
    REQUIRE(loaded[i].preNam1Active == false);
  }
}

TEST_CASE("invalid lastAmpIdx heals to default amp")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["lastAmpIdx"] = 9999;

  int lastAmp = 7;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, &lastAmp);
  REQUIRE(lastAmp == 0);

  j["lastAmpIdx"] = -50;
  lastAmp = 7;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, &lastAmp);
  REQUIRE(lastAmp == 0);
}

TEST_CASE("Negative per-amp channel settings reset amp settings to defaults")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["amps"]["Marshall JMP 2203 1976"]["speaker"] = 999;
  j["amps"]["Marshall JMP 2203 1976"]["channel"] = -1068236800;
  j["amps"]["Marshall JVM 210H OD1"]["speaker"] = -42;
  j["amps"]["Marshall JVM 210H OD1"]["channel"] = -1;

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);

  REQUIRE(healed == true);
  CHECK(loaded[8].speakerIdx == 3);
  CHECK(loaded[8].channelIdx == 0);
  CHECK(loaded[9].speakerIdx == 3);
  CHECK(loaded[9].channelIdx == 0);
}

TEST_CASE("Corrupt per-amp scalar settings heal to real defaults")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  auto& amp = j["amps"]["Soldano SLO100"];
  amp["speaker"] = 999;
  amp["input"] = 999.0;
  amp["gate"] = -999.0;
  amp["bass"] = "loud";
  amp["noiseGate"] = "yes";
  amp["preCompRatio"] = 999.0;
  amp["preCompAttack"] = 0.0;
  amp["preNam1MidFreq"] = 99999.0;

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  loaded[13].speakerIdx = 1;
  loaded[13].inputLevel = 7.0;
  loaded[13].gateThreshold = -20.0;
  loaded[13].toneBass = 8.0;
  loaded[13].noiseGateActive = false;
  loaded[13].preCompRatio = 12.0;
  loaded[13].preCompAttack = 12.0;
  loaded[13].preNam1MidFreq = 1200.0;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);

  REQUIRE(healed == true);
  CHECK(loaded[13].speakerIdx == 3);
  CHECK(loaded[13].inputLevel == doctest::Approx(0.0));
  CHECK(loaded[13].gateThreshold == doctest::Approx(-80.0));
  CHECK(loaded[13].toneBass == doctest::Approx(5.0));
  CHECK(loaded[13].noiseGateActive == true);
  CHECK(loaded[13].preCompRatio == doctest::Approx(4.0));
  CHECK(loaded[13].preCompAttack == doctest::Approx(4.0));
  CHECK(loaded[13].preNam1MidFreq == doctest::Approx(650.0));
}

TEST_CASE("Valid per-amp speaker and channel settings do not request auto-heal")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["amps"]["Soldano SLO100"]["speaker"] = 2;
  j["amps"]["Soldano SLO100"]["channel"] = 3;

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);

  REQUIRE(healed == false);
  CHECK(loaded[13].speakerIdx == 2);
  CHECK(loaded[13].channelIdx == 3);
}

TEST_CASE("Out-of-range supportOutputLevel above the +10 dB cap heals to default")
{
  // The OUTPUT knob (main + support) is capped at +10 dB. Old or hand-edited settings
  // files may contain >10 dB values — those must heal back to a safe default rather
  // than passing through and clipping the bus.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["amps"]["Soldano SLO100"]["supportOutput"] = 30.0;

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  loaded[13].supportOutputLevel = -3.0; // sentinel so we can prove the loader touched it
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);

  REQUIRE(healed == true);
  CHECK(loaded[13].supportOutputLevel <= 10.0);
  CHECK(loaded[13].supportOutputLevel == doctest::Approx(volum::VoLumAmpSettings{}.supportOutputLevel));
}

TEST_CASE("supportOutputLevel at the +10 dB cap loads through unchanged")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["amps"]["Soldano SLO100"]["supportOutput"] = 10.0;

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);

  CHECK(healed == false);
  CHECK(loaded[13].supportOutputLevel == doctest::Approx(10.0));
}

TEST_CASE("Legacy-safe settings omit dual-amp fields")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].dualAmpActive = true;
  amps[0].mainAmpPan = -1.0;
  amps[0].supportAmpIdx = 1;
  amps[0].supportOutputLevel = 3.0;
  amps[0].supportAmpPan = 1.0;

  const nlohmann::json j =
    volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, /*includeDualAmp=*/false);

  REQUIRE(volum::HasDualAmpUserSettings(j) == false);
  CHECK(j["amps"]["Ampete One"].contains("speaker"));
  CHECK_FALSE(j["amps"]["Ampete One"].contains("dualAmpActive"));
  CHECK_FALSE(j["amps"]["Ampete One"].contains("supportOutput"));
}

TEST_CASE("Dual-amp sidecar overlays current-only support settings")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].dualAmpActive = true;
  amps[0].dualAmpRoute = 2;
  amps[0].mainAmpPan = -1.0;
  amps[0].supportAmpIdx = 1;
  amps[0].supportSpeakerIdx = 2;
  amps[0].supportChannelIdx = 3;
  amps[0].supportInputLevel = 1.5;
  amps[0].supportGateThreshold = -60.0;
  amps[0].supportToneBass = 4.0;
  amps[0].supportToneMid = 6.0;
  amps[0].supportToneTreble = 7.0;
  amps[0].supportOutputLevel = 3.0;
  amps[0].supportNoiseGateActive = false;
  amps[0].supportEqActive = false;
  amps[0].supportAmpPan = 1.0;

  const nlohmann::json legacy =
    volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, /*includeDualAmp=*/false);
  const nlohmann::json sidecar = volum::VolumDualAmpUserSettingsToJson(amps, volum::kAmpCount);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VolumUserSettingsFromJson(legacy, loaded, volum::kAmpCount, nullptr);
  CHECK(loaded[0].dualAmpActive == false);
  CHECK(loaded[0].supportAmpIdx == -1);

  volum::VolumUserSettingsFromJson(sidecar, loaded, volum::kAmpCount, nullptr);
  CHECK(loaded[0].dualAmpActive == true);
  CHECK(loaded[0].dualAmpRoute == 2);
  CHECK(loaded[0].mainAmpPan == doctest::Approx(-1.0));
  CHECK(loaded[0].supportAmpIdx == 1);
  CHECK(loaded[0].supportSpeakerIdx == 2);
  CHECK(loaded[0].supportChannelIdx == 3);
  CHECK(loaded[0].supportInputLevel == doctest::Approx(1.5));
  CHECK(loaded[0].supportGateThreshold == doctest::Approx(-60.0));
  CHECK(loaded[0].supportToneBass == doctest::Approx(4.0));
  CHECK(loaded[0].supportToneMid == doctest::Approx(6.0));
  CHECK(loaded[0].supportToneTreble == doctest::Approx(7.0));
  CHECK(loaded[0].supportOutputLevel == doctest::Approx(3.0));
  CHECK(loaded[0].supportNoiseGateActive == false);
  CHECK(loaded[0].supportEqActive == false);
  CHECK(loaded[0].supportAmpPan == doctest::Approx(1.0));
}

TEST_CASE("Effect settings JSON roundtrip preserves all params")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumEffectSettings fx;
  fx.delayActive = true;
  fx.delayMode = 2;
  fx.delayModes[2].time = 500.0;
  fx.delayModes[2].feedback = 0.6;
  fx.delayModes[2].mix = 0.4;
  fx.reverbActive = true;
  fx.reverbMode = 1;
  fx.reverbModes[1].mix = 0.7;
  fx.reverbModes[1].decay = 5.5;
  fx.reverbModes[1].tone = 8.0;
  fx.reverbModes[1].preDelay = 37.0;
  fx.reverbModes[1].shimmer = 0.25;

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  CHECK(loaded.delayActive == true);
  CHECK(loaded.delayMode == 2);
  CHECK(loaded.delayModes[2].time == doctest::Approx(500.0));
  CHECK(loaded.delayModes[2].feedback == doctest::Approx(0.6));
  CHECK(loaded.delayModes[2].mix == doctest::Approx(0.4));
  CHECK(loaded.reverbActive == true);
  CHECK(loaded.reverbMode == 1);
  CHECK(loaded.reverbModes[1].mix == doctest::Approx(0.7));
  CHECK(loaded.reverbModes[1].decay == doctest::Approx(5.5));
  CHECK(loaded.reverbModes[1].tone == doctest::Approx(8.0));
  CHECK(loaded.reverbModes[1].preDelay == doctest::Approx(37.0));
  CHECK(loaded.reverbModes[1].shimmer == doctest::Approx(0.25));
}

TEST_CASE("Old settings without effects key loads defaults")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  // j has no "effects" key (nullptr passed)

  volum::VoLumEffectSettings loaded;
  loaded.delayActive = true;
  loaded.delayModes[0].time = 999.0;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  // Should remain unchanged — no "effects" in JSON
  CHECK(loaded.delayActive == true);
  CHECK(loaded.delayModes[0].time == doctest::Approx(999.0));
}

TEST_CASE("Effect settings nullptr is safe")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumEffectSettings fx;
  fx.delayActive = true;
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);

  // Pass nullptr for fx — should not crash
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr);
}

TEST_CASE("Corrupt effect settings heal to defaults")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["effects"] = {
    {"delayActive", "yes"},
    {"delayMode", 99},
    {"reverbActive", 1},
    {"reverbMode", -1},
    {"delayModes", nlohmann::json::array({{{"time", 99999.0}, {"feedback", -0.1}, {"mix", "wet"}}})},
    {"reverbModes", nlohmann::json::array({{{"mix", 5.0}, {"decay", -1.0}, {"tone", "dark"},
                                             {"preDelay", 999.0}, {"shimmer", -0.5}}})},
  };

  volum::VoLumEffectSettings loaded;
  loaded.delayActive = true;
  loaded.delayMode = 2;
  loaded.reverbActive = true;
  loaded.reverbMode = 2;
  loaded.delayModes[0].time = 700.0;
  loaded.delayModes[0].feedback = 0.8;
  loaded.delayModes[0].mix = 0.8;
  loaded.reverbModes[0].mix = 0.8;
  loaded.reverbModes[0].decay = 8.0;
  loaded.reverbModes[0].tone = 8.0;
  loaded.reverbModes[0].preDelay = 60.0;
  loaded.reverbModes[0].shimmer = 0.8;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded, &healed);

  REQUIRE(healed == true);
  CHECK(loaded.delayActive == false);
  CHECK(loaded.delayMode == 1);
  CHECK(loaded.reverbActive == false);
  CHECK(loaded.reverbMode == 0);
  CHECK(loaded.delayModes[0].time == doctest::Approx(380.0));
  CHECK(loaded.delayModes[0].feedback == doctest::Approx(0.35));
  CHECK(loaded.delayModes[0].mix == doctest::Approx(0.28));
  CHECK(loaded.reverbModes[0].mix == doctest::Approx(0.3));
  CHECK(loaded.reverbModes[0].decay == doctest::Approx(3.0));
  CHECK(loaded.reverbModes[0].tone == doctest::Approx(4.5));
  CHECK(loaded.reverbModes[0].preDelay == doctest::Approx(20.0));
  CHECK(loaded.reverbModes[0].shimmer == doctest::Approx(0.5));
}

TEST_CASE("Legacy flat effect settings populate every mode snapshot")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["effects"] = {
    {"delayActive", true},
    {"delayTime", 625.0},
    {"delayFeedback", 0.7},
    {"delayMix", 0.33},
    {"delayMode", 1},
    {"reverbActive", true},
    {"reverbMix", 0.44},
    {"reverbDecay", 6.0},
    {"reverbTone", 3.0},
    {"reverbMode", 2},
  };

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  for (int i = 0; i < 3; ++i)
  {
    CHECK(loaded.delayModes[i].time == doctest::Approx(625.0));
    CHECK(loaded.delayModes[i].feedback == doctest::Approx(0.7));
    CHECK(loaded.delayModes[i].mix == doctest::Approx(0.33));
    CHECK(loaded.reverbModes[i].mix == doctest::Approx(0.44));
    CHECK(loaded.reverbModes[i].decay == doctest::Approx(6.0));
    CHECK(loaded.reverbModes[i].tone == doctest::Approx(3.0));
    CHECK(loaded.reverbModes[i].preDelay == doctest::Approx(20.0));
    CHECK(loaded.reverbModes[i].shimmer == doctest::Approx(0.5));
  }
}
