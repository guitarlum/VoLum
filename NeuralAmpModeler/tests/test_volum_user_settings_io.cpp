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
  REQUIRE(j["version"] == volum::kVoLumUserSettingsVersion);

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

TEST_CASE("Legacy user settings reset PRE capture selections before real captures")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].preNam1Active = true;
  amps[0].preNam1Capture = 2;
  amps[0].preNam1Gain = 3.0;
  amps[0].preNam2Active = true;
  amps[0].preNam2Capture = 4;
  amps[0].preNam2Level = -2.0;

  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = 1;

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);

  REQUIRE(healed == true);
  CHECK(loaded[0].preNam1Active == true);
  CHECK(loaded[0].preNam1Capture == 0);
  CHECK(loaded[0].preNam1Gain == doctest::Approx(3.0));
  CHECK(loaded[0].preNam2Active == true);
  CHECK(loaded[0].preNam2Capture == 0);
  CHECK(loaded[0].preNam2Level == doctest::Approx(-2.0));
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
  amps[0].supportPolarityInvert = true;

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
  CHECK(loaded[0].supportPolarityInvert == true);
}

TEST_CASE("Legacy dual-amp settings default support polarity invert on")
{
  CHECK(volum::VoLumAmpSettings{}.supportPolarityInvert == true);

  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].dualAmpActive = true;
  amps[0].supportAmpIdx = 1;
  amps[0].supportPolarityInvert = false;

  nlohmann::json legacySidecar = volum::VolumDualAmpUserSettingsToJson(amps, volum::kAmpCount);
  legacySidecar["amps"]["Ampete One"].erase("supportPolarityInvert");

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VolumUserSettingsFromJson(legacySidecar, loaded, volum::kAmpCount, nullptr);

  CHECK(loaded[0].dualAmpActive == true);
  CHECK(loaded[0].supportAmpIdx == 1);
  CHECK(loaded[0].supportPolarityInvert == true);

  nlohmann::json explicitOff = volum::VolumDualAmpUserSettingsToJson(amps, volum::kAmpCount);
  volum::VolumUserSettingsFromJson(explicitOff, loaded, volum::kAmpCount, nullptr);
  CHECK(loaded[0].supportPolarityInvert == false);
}

TEST_CASE("Effect settings JSON roundtrip preserves all params")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumEffectSettings fx;
  fx.delayActive = true;
  fx.delayMode = 3;
  fx.delayModes[3].time = 650.0;
  fx.delayModes[3].feedback = 0.6;
  fx.delayModes[3].mix = 0.4;
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
  CHECK(loaded.delayMode == 3);
  CHECK(loaded.delayModes[3].time == doctest::Approx(650.0));
  CHECK(loaded.delayModes[3].feedback == doctest::Approx(0.6));
  CHECK(loaded.delayModes[3].mix == doctest::Approx(0.4));
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
  // v0.9.0 default delay mode is Digital (index 0) under the new mode order.
  CHECK(loaded.delayMode == volum::kVoLumDelayModeDigital);
  CHECK(loaded.reverbActive == false);
  CHECK(loaded.reverbMode == volum::kVoLumReverbModeHall);
  // Digital snapshot defaults: time/feedback/mix unchanged from previous version.
  CHECK(loaded.delayModes[0].time == doctest::Approx(380.0));
  CHECK(loaded.delayModes[0].feedback == doctest::Approx(0.35));
  CHECK(loaded.delayModes[0].mix == doctest::Approx(0.28));
  // Hall snapshot defaults stronger per design guide (mix 0.32, decay 3.5s, tone slightly bright).
  CHECK(loaded.reverbModes[0].mix == doctest::Approx(0.32));
  CHECK(loaded.reverbModes[0].decay == doctest::Approx(3.5));
  CHECK(loaded.reverbModes[0].tone == doctest::Approx(5.5));
  CHECK(loaded.reverbModes[0].preDelay == doctest::Approx(30.0));
  CHECK(loaded.reverbModes[0].shimmer == doctest::Approx(0.0));
}

TEST_CASE("Legacy flat effect settings populate existing mode snapshots")
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

  for (int i = 0; i < volum::kVoLumReverseDelayMode; ++i)
  {
    CHECK(loaded.delayModes[i].time == doctest::Approx(625.0));
    CHECK(loaded.delayModes[i].feedback == doctest::Approx(0.7));
    CHECK(loaded.delayModes[i].mix == doctest::Approx(0.33));
  }
  CHECK(loaded.delayModes[volum::kVoLumReverseDelayMode].time == doctest::Approx(600.0));
  CHECK(loaded.delayModes[volum::kVoLumReverseDelayMode].feedback == doctest::Approx(0.30));
  CHECK(loaded.delayModes[volum::kVoLumReverseDelayMode].mix == doctest::Approx(0.40));

  // Legacy flat reverb fields populate every reverb-mode snapshot, including the new
  // TremVerb (index 3) that didn't exist in old saves.
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    CHECK(loaded.reverbModes[i].mix == doctest::Approx(0.44));
    CHECK(loaded.reverbModes[i].decay == doctest::Approx(6.0));
    CHECK(loaded.reverbModes[i].tone == doctest::Approx(3.0));
    CHECK(loaded.reverbModes[i].preDelay == doctest::Approx(20.0));
    CHECK(loaded.reverbModes[i].shimmer == doctest::Approx(0.5));
  }
}

TEST_CASE("Legacy v2 delayModes array migrates to v0.9.0 with correct per-slot v3 defaults")
{
  // Regression for: v2 settings.json saves the delayModes array in old order
  // {Tape, Digital, PingPong, Reverse} but only authors {time, feedback, mix} per slot.
  // After migration the new slots {Digital, Analog, Tape, Reverse} must each carry the
  // user's saved time/feedback/mix in the right place AND get the v0.9.0 design-guide
  // defaults for age/tone/pingPong/tapeSubMode (since v2 couldn't author them).
  // Specifically, new Tape.age must be 0.5 (not Digital's 0.0) — the bug was that the
  // pre-load step seeded raw slot 0 with new-Digital defaults, then the mapping carried
  // those wrong defaults into new Tape.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  // Force v2 layout: the writer emits the current version, so override + supply a
  // 4-entry delayModes array in the OLD order with only legacy fields.
  j["version"] = 2;
  j["effects"] = {
    {"delayActive", true},
    {"delayMode", 0}, // old "Tape" -> new Tape (index 2)
    {"delayModes", nlohmann::json::array({
                     {{"time", 480.0}, {"feedback", 0.55}, {"mix", 0.40}},  // old Tape
                     {{"time", 250.0}, {"feedback", 0.30}, {"mix", 0.20}},  // old Digital
                     {{"time", 350.0}, {"feedback", 0.40}, {"mix", 0.35}},  // old PingPong
                     {{"time", 700.0}, {"feedback", 0.25}, {"mix", 0.45}},  // old Reverse
                   })},
    {"reverbActive", false},
    {"reverbMode", 0},
  };

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  // Active mode: old Tape (0) maps to new Tape (kVoLumDelayModeTape == 2).
  CHECK(loaded.delayMode == volum::kVoLumDelayModeTape);

  // User's saved time/feedback/mix carry over to the right new slot.
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].time == doctest::Approx(250.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].feedback == doctest::Approx(0.30));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].mix == doctest::Approx(0.20));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].time == doctest::Approx(480.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].feedback == doctest::Approx(0.55));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].mix == doctest::Approx(0.40));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].time == doctest::Approx(700.0));

  // v3 fields take per-slot design-guide defaults (NOT zero, NOT the wrong slot's defaults).
  // Defaults from VoLumUserSettingsIO.h: Digital age=0.0, Analog age=0.5, Tape age=0.5, Reverse age=0.5.
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].age == doctest::Approx(0.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeAnalog].age == doctest::Approx(0.5));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].age == doctest::Approx(0.5));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].age == doctest::Approx(0.5));

  // Tape sub-mode default: Vintage (1).
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].tapeSubMode == 1);

  // PingPong default off for all slots when the user wasn't actively in PingPong mode.
  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
    CHECK_FALSE(loaded.delayModes[i].pingPong);

  // Analog slot is brand new; it inherits new-default time/feedback/mix.
  CHECK(loaded.delayModes[volum::kVoLumDelayModeAnalog].time == doctest::Approx(320.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeAnalog].feedback == doctest::Approx(0.42));
}

TEST_CASE("Legacy v2 PingPong-active save folds into Digital with pingPong=true and correct age")
{
  // When the user was actively in old PingPong mode (delayMode=2), the migration must:
  //   - Map the active mode to new Digital (0).
  //   - Carry the old PingPong snapshot's time/feedback/mix into new Digital.
  //   - Set pingPong=true on new Digital.
  //   - Still apply the new-Digital design-guide defaults for age/tone (age=0.0, tone=0.5).
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = 2;
  j["effects"] = {
    {"delayActive", true},
    {"delayMode", 2}, // old PingPong
    {"delayModes", nlohmann::json::array({
                     {{"time", 480.0}, {"feedback", 0.55}, {"mix", 0.40}},
                     {{"time", 250.0}, {"feedback", 0.30}, {"mix", 0.20}},
                     {{"time", 333.0}, {"feedback", 0.66}, {"mix", 0.55}}, // old PingPong values
                     {{"time", 700.0}, {"feedback", 0.25}, {"mix", 0.45}},
                   })},
    {"reverbActive", false},
    {"reverbMode", 0},
  };

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  CHECK(loaded.delayMode == volum::kVoLumDelayModeDigital);
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].pingPong == true);
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].time == doctest::Approx(333.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].feedback == doctest::Approx(0.66));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].mix == doctest::Approx(0.55));
  // Even with PingPong fold-in, age stays at the new-Digital design-guide default.
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].age == doctest::Approx(0.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].tone == doctest::Approx(0.5));
}

TEST_CASE("v3->v4 heals zero-age Analog/Tape that came from buggy early v2->v3 migration")
{
  // A v3 file produced by the early buggy migration has Analog.age == Tape.age == 0.0,
  // which is wrong (design-guide defaults are 0.5 for both). v3->v4 resets those ages.
  // Other fields (time/feedback/mix/tone/pingPong) MUST be preserved.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = 3;
  j["effects"] = {
    {"delayActive", true},
    {"delayMode", 2},
    {"delayModes", nlohmann::json::array({
                     {{"time", 250.0}, {"feedback", 0.30}, {"mix", 0.20},
                      {"tone", 0.5}, {"age", 0.0}, {"pingPong", false}, {"tapeSubMode", 1}},
                     {{"time", 320.0}, {"feedback", 0.42}, {"mix", 0.32},
                      {"tone", 0.5}, {"age", 0.0}, {"pingPong", false}, {"tapeSubMode", 1}}, // bug
                     {{"time", 420.0}, {"feedback", 0.45}, {"mix", 0.30},
                      {"tone", 0.45}, {"age", 0.0}, {"pingPong", true}, {"tapeSubMode", 2}}, // bug
                     {{"time", 600.0}, {"feedback", 0.30}, {"mix", 0.40},
                      {"tone", 0.5}, {"age", 0.5}, {"pingPong", false}, {"tapeSubMode", 1}},
                   })},
    {"reverbActive", false},
    {"reverbMode", 0},
  };

  volum::VoLumEffectSettings loaded;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded, &healed);

  CHECK(healed);
  // Ages restored to defaults for Analog and Tape.
  CHECK(loaded.delayModes[volum::kVoLumDelayModeAnalog].age == doctest::Approx(0.5));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].age == doctest::Approx(0.5));
  // Digital and Reverse ages untouched.
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].age == doctest::Approx(0.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].age == doctest::Approx(0.5));
  // Other Tape fields preserved.
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].time == doctest::Approx(420.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].feedback == doctest::Approx(0.45));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].mix == doctest::Approx(0.30));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].tone == doctest::Approx(0.45));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].pingPong == true);
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].tapeSubMode == 2);
}

TEST_CASE("v3->v4 leaves non-buggy v3 file alone (one of Analog/Tape age != 0)")
{
  // If the user intentionally has Analog.age=0.0 but Tape.age=0.6 (or any non-zero), don't
  // touch ages — the buggy pattern requires BOTH to be exactly 0.0.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = 3;
  j["effects"] = {
    {"delayActive", true},
    {"delayMode", 2},
    {"delayModes", nlohmann::json::array({
                     {{"time", 250.0}, {"feedback", 0.30}, {"mix", 0.20},
                      {"tone", 0.5}, {"age", 0.0}, {"pingPong", false}, {"tapeSubMode", 1}},
                     {{"time", 320.0}, {"feedback", 0.42}, {"mix", 0.32},
                      {"tone", 0.5}, {"age", 0.0}, {"pingPong", false}, {"tapeSubMode", 1}},
                     {{"time", 420.0}, {"feedback", 0.45}, {"mix", 0.30},
                      {"tone", 0.45}, {"age", 0.6}, {"pingPong", true}, {"tapeSubMode", 2}}, // user set
                     {{"time", 600.0}, {"feedback", 0.30}, {"mix", 0.40},
                      {"tone", 0.5}, {"age", 0.5}, {"pingPong", false}, {"tapeSubMode", 1}},
                   })},
    {"reverbActive", false},
    {"reverbMode", 0},
  };

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);
  CHECK(loaded.delayModes[volum::kVoLumDelayModeAnalog].age == doctest::Approx(0.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeTape].age == doctest::Approx(0.6));
}

TEST_CASE("v0.9.0 effect snapshot fields round-trip through user settings JSON")
{
  // Round-trip test for the new per-mode-snapshot fields added in v0.9.0:
  //   delay: tone, age, pingPong, tapeSubMode
  //   reverb: subMode, tremRate
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumEffectSettings fx;
  fx.delayActive = true;
  fx.delayMode = volum::kVoLumDelayModeTape;
  fx.reverbActive = true;
  fx.reverbMode = volum::kVoLumReverbModeTremVerb;

  // Distinguishable per-mode values so we can detect any cross-talk.
  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    fx.delayModes[i].time = 250.0 + 50.0 * i;
    fx.delayModes[i].feedback = 0.30 + 0.05 * i;
    fx.delayModes[i].mix = 0.20 + 0.05 * i;
    fx.delayModes[i].tone = 0.20 + 0.10 * i;
    fx.delayModes[i].age = 0.10 + 0.10 * i;
    fx.delayModes[i].pingPong = (i == volum::kVoLumDelayModeDigital);
    fx.delayModes[i].tapeSubMode = i % 3;
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    fx.reverbModes[i].mix = 0.30 + 0.05 * i;
    fx.reverbModes[i].decay = 2.0 + 0.5 * i;
    fx.reverbModes[i].tone = 4.0 + 0.5 * i;
    fx.reverbModes[i].preDelay = 15.0 + 5.0 * i;
    fx.reverbModes[i].shimmer = 0.10 + 0.10 * i;
    fx.reverbModes[i].subMode = i % 3;
    fx.reverbModes[i].tremRate = 3.0 + 0.5 * i;
  }

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  CHECK(loaded.delayActive == true);
  CHECK(loaded.delayMode == volum::kVoLumDelayModeTape);
  CHECK(loaded.reverbActive == true);
  CHECK(loaded.reverbMode == volum::kVoLumReverbModeTremVerb);

  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    CHECK(loaded.delayModes[i].time == doctest::Approx(250.0 + 50.0 * i));
    CHECK(loaded.delayModes[i].feedback == doctest::Approx(0.30 + 0.05 * i));
    CHECK(loaded.delayModes[i].mix == doctest::Approx(0.20 + 0.05 * i));
    CHECK(loaded.delayModes[i].tone == doctest::Approx(0.20 + 0.10 * i));
    CHECK(loaded.delayModes[i].age == doctest::Approx(0.10 + 0.10 * i));
    CHECK(loaded.delayModes[i].pingPong == (i == volum::kVoLumDelayModeDigital));
    CHECK(loaded.delayModes[i].tapeSubMode == i % 3);
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    CHECK(loaded.reverbModes[i].mix == doctest::Approx(0.30 + 0.05 * i));
    CHECK(loaded.reverbModes[i].decay == doctest::Approx(2.0 + 0.5 * i));
    CHECK(loaded.reverbModes[i].tone == doctest::Approx(4.0 + 0.5 * i));
    CHECK(loaded.reverbModes[i].preDelay == doctest::Approx(15.0 + 5.0 * i));
    CHECK(loaded.reverbModes[i].shimmer == doctest::Approx(0.10 + 0.10 * i));
    CHECK(loaded.reverbModes[i].subMode == i % 3);
    CHECK(loaded.reverbModes[i].tremRate == doctest::Approx(3.0 + 0.5 * i));
  }
}

TEST_CASE("Three-entry delay mode settings leave reverse delay defaults")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["effects"] = {
    {"delayMode", 3},
    {"delayModes", nlohmann::json::array({
      {{"time", 400.0}, {"feedback", 0.2}, {"mix", 0.1}},
      {{"time", 500.0}, {"feedback", 0.3}, {"mix", 0.2}},
      {{"time", 700.0}, {"feedback", 0.4}, {"mix", 0.3}},
    })},
  };

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  CHECK(loaded.delayMode == 3);
  CHECK(loaded.delayModes[0].time == doctest::Approx(400.0));
  CHECK(loaded.delayModes[1].time == doctest::Approx(500.0));
  CHECK(loaded.delayModes[2].time == doctest::Approx(700.0));
  // Slot 3 (Reverse) keeps its iteration-2 default (design-guide values).
  CHECK(loaded.delayModes[3].time == doctest::Approx(600.0));
  CHECK(loaded.delayModes[3].feedback == doctest::Approx(0.30));
  CHECK(loaded.delayModes[3].mix == doctest::Approx(0.40));
}
