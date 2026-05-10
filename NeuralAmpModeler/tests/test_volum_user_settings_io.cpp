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

TEST_CASE("effect-staging effect snapshot fields round-trip through user settings JSON")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumEffectSettings fx;
  fx.delayActive = true;
  fx.delayMode = volum::kVoLumDelayModeAnalog;
  fx.reverbActive = true;
  fx.reverbMode = volum::kVoLumReverbModeOktaverb;

  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    fx.delayModes[i].time = 250.0 + 50.0 * i;
    fx.delayModes[i].feedback = 0.20 + 0.05 * i;
    fx.delayModes[i].mix = 0.15 + 0.05 * i;
    fx.delayModes[i].tone = 0.30 + 0.10 * i;
    fx.delayModes[i].age = 0.10 + 0.10 * i;
    fx.delayModes[i].pingPong = (i == volum::kVoLumDelayModeDigital);
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    fx.reverbModes[i].mix = 0.30 + 0.05 * i;
    fx.reverbModes[i].decay = 2.0 + i;
    fx.reverbModes[i].tone = 4.0 + i;
    fx.reverbModes[i].preDelay = 10.0 + 5.0 * i;
    fx.reverbModes[i].shimmer = 0.10 + 0.10 * i;
    fx.reverbModes[i].subMode = i % 3;
  }
  for (int i = 0; i < 3; ++i)
  {
    fx.oktaverbSubModes[i].mix = 0.40 + 0.05 * i;
    fx.oktaverbSubModes[i].decay = 4.0 + i;
    fx.oktaverbSubModes[i].tone = 3.0 + i;
    fx.oktaverbSubModes[i].preDelay = 20.0 + 10.0 * i;
    fx.oktaverbSubModes[i].shimmer = 0.30 + 0.10 * i;
  }

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);
  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  CHECK(loaded.delayActive == true);
  CHECK(loaded.delayMode == volum::kVoLumDelayModeAnalog);
  CHECK(loaded.reverbActive == true);
  CHECK(loaded.reverbMode == volum::kVoLumReverbModeOktaverb);
  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    CHECK(loaded.delayModes[i].time == doctest::Approx(250.0 + 50.0 * i));
    CHECK(loaded.delayModes[i].feedback == doctest::Approx(0.20 + 0.05 * i));
    CHECK(loaded.delayModes[i].mix == doctest::Approx(0.15 + 0.05 * i));
    CHECK(loaded.delayModes[i].tone == doctest::Approx(0.30 + 0.10 * i));
    CHECK(loaded.delayModes[i].age == doctest::Approx(0.10 + 0.10 * i));
    CHECK(loaded.delayModes[i].pingPong == (i == volum::kVoLumDelayModeDigital));
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    CHECK(loaded.reverbModes[i].mix == doctest::Approx(0.30 + 0.05 * i));
    CHECK(loaded.reverbModes[i].decay == doctest::Approx(2.0 + i));
    CHECK(loaded.reverbModes[i].tone == doctest::Approx(4.0 + i));
    CHECK(loaded.reverbModes[i].preDelay == doctest::Approx(10.0 + 5.0 * i));
    CHECK(loaded.reverbModes[i].shimmer == doctest::Approx(0.10 + 0.10 * i));
    CHECK(loaded.reverbModes[i].subMode == i % 3);
  }
  for (int i = 0; i < 3; ++i)
  {
    CHECK(loaded.oktaverbSubModes[i].mix == doctest::Approx(0.40 + 0.05 * i));
    CHECK(loaded.oktaverbSubModes[i].decay == doctest::Approx(4.0 + i));
    CHECK(loaded.oktaverbSubModes[i].tone == doctest::Approx(3.0 + i));
    CHECK(loaded.oktaverbSubModes[i].preDelay == doctest::Approx(20.0 + 10.0 * i));
    CHECK(loaded.oktaverbSubModes[i].shimmer == doctest::Approx(0.30 + 0.10 * i));
  }
}

TEST_CASE("legacy v4 settings seed Oktaverb sub-mode snapshots from current Oktaverb values")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumEffectSettings fx;
  auto& okt = fx.reverbModes[volum::kVoLumReverbModeOktaverb];
  okt.mix = 0.77;
  okt.decay = 8.0;
  okt.tone = 2.5;
  okt.preDelay = 44.0;
  okt.shimmer = 0.33;

  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);
  j["version"] = 4;
  j["effects"].erase("oktaverbSubModes");

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  for (int i = 0; i < 3; ++i)
  {
    CHECK(loaded.oktaverbSubModes[i].mix == doctest::Approx(0.77));
    CHECK(loaded.oktaverbSubModes[i].decay == doctest::Approx(8.0));
    CHECK(loaded.oktaverbSubModes[i].tone == doctest::Approx(2.5));
    CHECK(loaded.oktaverbSubModes[i].preDelay == doctest::Approx(44.0));
    CHECK(loaded.oktaverbSubModes[i].shimmer == doctest::Approx(0.33));
  }
}

TEST_CASE("legacy v3 Oktaverb sub-modes migrate to Halo and Shimmer")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumEffectSettings fx;
  fx.reverbMode = volum::kVoLumReverbModeOktaverb;
  fx.reverbModes[volum::kVoLumReverbModeOktaverb].subMode = 2; // old Oct+Sub

  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, &fx);
  j["version"] = 3;

  volum::VoLumEffectSettings loaded;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded, &healed);
  CHECK(healed);
  CHECK(loaded.reverbModes[volum::kVoLumReverbModeOktaverb].subMode == volum::kVoLumOktaverbSubModeDark);

  j["effects"]["reverbModes"][volum::kVoLumReverbModeOktaverb]["subMode"] = 0; // old Oct
  healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded, &healed);
  CHECK(healed);
  CHECK(loaded.reverbModes[volum::kVoLumReverbModeOktaverb].subMode == volum::kVoLumOktaverbSubModeShimmer);
}

TEST_CASE("legacy v2 delayModes migrate to effect-staging mode order")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = 2;
  j["effects"] = {
    {"delayActive", true},
    {"delayMode", 0}, // old Tape, removed in staging
    {"delayModes", nlohmann::json::array({
                     {{"time", 480.0}, {"feedback", 0.55}, {"mix", 0.40}}, // old Tape
                     {{"time", 250.0}, {"feedback", 0.30}, {"mix", 0.20}}, // old Digital
                     {{"time", 333.0}, {"feedback", 0.66}, {"mix", 0.55}}, // old PingPong
                     {{"time", 700.0}, {"feedback", 0.25}, {"mix", 0.60}}, // old Reverse
                   })},
    {"reverbActive", false},
    {"reverbMode", 0},
  };

  volum::VoLumEffectSettings loaded;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded, &healed);

  CHECK(healed);
  CHECK(loaded.delayMode == volum::kVoLumDelayModeDigital);
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].time == doctest::Approx(480.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].feedback == doctest::Approx(0.55));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeDigital].mix == doctest::Approx(0.40));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeAnalog].time == doctest::Approx(320.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeAnalog].age == doctest::Approx(0.5));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].time == doctest::Approx(700.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].age == doctest::Approx(0.0));
}

TEST_CASE("legacy v2 PingPong mode folds into Digital ping-pong toggle")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = 2;
  j["effects"] = {
    {"delayActive", true},
    {"delayMode", 2},
    {"delayModes", nlohmann::json::array({
                     {{"time", 480.0}, {"feedback", 0.55}, {"mix", 0.40}},
                     {{"time", 250.0}, {"feedback", 0.30}, {"mix", 0.20}},
                     {{"time", 333.0}, {"feedback", 0.66}, {"mix", 0.55}},
                     {{"time", 700.0}, {"feedback", 0.25}, {"mix", 0.60}},
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
  fx.delayMode = volum::kVoLumDelayModeReverse;
  fx.delayModes[volum::kVoLumDelayModeReverse].time = 650.0;
  fx.delayModes[volum::kVoLumDelayModeReverse].feedback = 0.6;
  fx.delayModes[volum::kVoLumDelayModeReverse].mix = 0.4;
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
  CHECK(loaded.delayMode == volum::kVoLumDelayModeReverse);
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].time == doctest::Approx(650.0));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].feedback == doctest::Approx(0.6));
  CHECK(loaded.delayModes[volum::kVoLumDelayModeReverse].mix == doctest::Approx(0.4));
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
  CHECK(loaded.delayMode == volum::kVoLumDelayModeDigital);
  CHECK(loaded.reverbActive == false);
  CHECK(loaded.reverbMode == 0);
  CHECK(loaded.delayModes[0].time == doctest::Approx(380.0));
  CHECK(loaded.delayModes[0].feedback == doctest::Approx(0.35));
  CHECK(loaded.delayModes[0].mix == doctest::Approx(0.28));
  CHECK(loaded.reverbModes[0].mix == doctest::Approx(0.20));
  CHECK(loaded.reverbModes[0].decay == doctest::Approx(2.5));
  CHECK(loaded.reverbModes[0].tone == doctest::Approx(5.0));
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
  // Reverse default Mix dropped from 0.40 to 0.32 alongside the reverse-blend-law fix
  // (additive instead of crossfade), so a fresh Reverse patch sits at the same level as
  // Digital / Analog at the same Mix.
  CHECK(loaded.delayModes[volum::kVoLumReverseDelayMode].mix == doctest::Approx(0.32));

  for (int i = 0; i < 3; ++i)
  {
    CHECK(loaded.reverbModes[i].mix == doctest::Approx(0.44));
    CHECK(loaded.reverbModes[i].decay == doctest::Approx(6.0));
    CHECK(loaded.reverbModes[i].tone == doctest::Approx(3.0));
    CHECK(loaded.reverbModes[i].preDelay == doctest::Approx(20.0));
    CHECK(loaded.reverbModes[i].shimmer == doctest::Approx(0.5));
  }
}

TEST_CASE("Three-entry delay mode settings load staging slots")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["effects"] = {
    {"delayMode", volum::kVoLumDelayModeReverse},
    {"delayModes", nlohmann::json::array({
      {{"time", 400.0}, {"feedback", 0.2}, {"mix", 0.1}},
      {{"time", 500.0}, {"feedback", 0.3}, {"mix", 0.2}},
      {{"time", 700.0}, {"feedback", 0.4}, {"mix", 0.3}},
    })},
  };

  volum::VoLumEffectSettings loaded;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, &loaded);

  CHECK(loaded.delayMode == volum::kVoLumDelayModeReverse);
  CHECK(loaded.delayModes[0].time == doctest::Approx(400.0));
  CHECK(loaded.delayModes[1].time == doctest::Approx(500.0));
  CHECK(loaded.delayModes[2].time == doctest::Approx(700.0));
}

TEST_CASE("VolumUserSettings JSON roundtrips per-amp POST live values")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[3].postValid = true;
  amps[3].postDelayActive = true;
  amps[3].postDelayMode = volum::kVoLumDelayModeAnalog;
  amps[3].postDelayMix = 0.41;
  amps[3].postDelayTime = 320.0;
  amps[3].postDelayFeedback = 0.45;
  amps[3].postDelayTone = 0.62;
  amps[3].postDelayAge = 0.5;
  amps[3].postDelayPingPong = true;
  amps[3].postReverbActive = true;
  amps[3].postReverbMix = 0.33;
  amps[3].postReverbDecay = 4.5;
  amps[3].postReverbTone = 5.0;
  amps[3].postReverbPreDelay = 22.0;
  amps[3].postReverbShimmer = 0.4;
  amps[3].postReverbMode = volum::kVoLumReverbModeOktaverb;
  amps[3].postReverbSubMode = volum::kVoLumOktaverbSubModeShimmer;

  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr);

  CHECK(loaded[3].postValid);
  CHECK(loaded[3].postDelayActive);
  CHECK(loaded[3].postDelayMode == volum::kVoLumDelayModeAnalog);
  CHECK(loaded[3].postDelayMix == doctest::Approx(0.41));
  CHECK(loaded[3].postDelayTime == doctest::Approx(320.0));
  CHECK(loaded[3].postDelayFeedback == doctest::Approx(0.45));
  CHECK(loaded[3].postDelayTone == doctest::Approx(0.62));
  CHECK(loaded[3].postDelayAge == doctest::Approx(0.5));
  CHECK(loaded[3].postDelayPingPong);
  CHECK(loaded[3].postReverbActive);
  CHECK(loaded[3].postReverbMix == doctest::Approx(0.33));
  CHECK(loaded[3].postReverbDecay == doctest::Approx(4.5));
  CHECK(loaded[3].postReverbMode == volum::kVoLumReverbModeOktaverb);
  CHECK(loaded[3].postReverbSubMode == volum::kVoLumOktaverbSubModeShimmer);

  // Untouched amps remain at struct defaults (postValid=false).
  CHECK_FALSE(loaded[7].postValid);
}

TEST_CASE("VolumUserSettings legacy JSON without per-amp POST yields postValid=false")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  // Pre-build a v5 JSON (no per-amp post* keys). Simulate an upgrade load.
  nlohmann::json j;
  j["version"] = 5;
  j["lastAmpIdx"] = 0;
  j["amps"] = nlohmann::json::object();
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    nlohmann::json a;
    a["speaker"] = 3;
    a["channel"] = 0;
    j["amps"][volum::kAmps[i].folderName] = a;
  }

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr);

  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    INFO("amp " << i);
    CHECK_FALSE(loaded[i].postValid);
    CHECK(loaded[i].postDelayMix == doctest::Approx(0.28));
    CHECK(loaded[i].postReverbMix == doctest::Approx(0.20));
  }
}
