#include "third_party/doctest.h"
#include "../VoLumAmpSettingsJson.h"
#include "../VoLumUserSettingsIO.h"

#include <filesystem>
#include <fstream>

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

TEST_CASE("Broken user settings JSON leaves defaults in place")
{
  const auto root = std::filesystem::temp_directory_path() / "volum-user-settings-broken-json-test";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  const auto path = root / "volum-settings.json";

  {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << "{ this is not valid json";
  }

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  int lastAmp = 9;
  bool parseFailed = false;
  try
  {
    std::ifstream in(path, std::ios::binary);
    nlohmann::json j;
    in >> j;
    volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, &lastAmp);
  }
  catch (...)
  {
    parseFailed = true; // Mirrors _VolumLoadSettingsFromFile: parse failures are swallowed.
  }

  CHECK(parseFailed);
  CHECK(lastAmp == 9);
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    INFO("amp " << i);
    CHECK(loaded[i].speakerIdx == volum::VoLumAmpSettings{}.speakerIdx);
    CHECK(loaded[i].channelIdx == volum::VoLumAmpSettings{}.channelIdx);
    CHECK(loaded[i].inputLevel == doctest::Approx(volum::VoLumAmpSettings{}.inputLevel));
    CHECK(loaded[i].noiseGateActive == volum::VoLumAmpSettings{}.noiseGateActive);
    CHECK_FALSE(loaded[i].postValid);
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
  amps[0].supportCustomId = "amp_custom_support";
  amps[0].supportCustomSlot = 1;
  amps[0].supportCustomChannel = 4;

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
  // 1.2.0: the custom SUPPORT partner id + its cab/channel ride the sidecar so a
  // factory-main + custom-support rig restores fully across a standalone restart.
  CHECK(loaded[0].supportCustomId == "amp_custom_support");
  CHECK(loaded[0].supportCustomSlot == 1);
  CHECK(loaded[0].supportCustomChannel == 4);
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
  CHECK(loaded.delayModes[0].time == doctest::Approx(320.0));
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
  amps[3].postDelaySync = true;
  amps[3].postDelayDivision = 3;
  amps[3].postReverbActive = true;
  amps[3].postReverbMix = 0.33;
  amps[3].postReverbDecay = 4.5;
  amps[3].postReverbTone = 5.0;
  amps[3].postReverbPreDelay = 22.0;
  amps[3].postReverbShimmer = 0.4;
  amps[3].postReverbMode = volum::kVoLumReverbModeOktaverb;
  amps[3].postReverbSubMode = volum::kVoLumOktaverbSubModeShimmer;
  amps[3].postDelayModes[volum::kVoLumDelayModeDigital].time = 444.0;
  amps[3].postDelayModes[volum::kVoLumDelayModeAnalog].age = 0.72;
  amps[3].postDelayModes[volum::kVoLumDelayModeReverse].mix = 0.37;
  amps[3].postReverbModes[volum::kVoLumReverbModePlate].decay = 6.7;
  amps[3].postReverbModes[volum::kVoLumReverbModeOktaverb].subMode = volum::kVoLumOktaverbSubModeBloom;
  amps[3].postOktaverbSubModes[volum::kVoLumOktaverbSubModeBloom].shimmer = 0.81;
  amps[3].postTremoloActive = true;
  amps[3].postTremoloMode = volum::kVoLumTremoloModeHarmonic;
  amps[3].postTremoloRate = 6.5;
  amps[3].postTremoloDepth = 0.72;
  amps[3].postTremoloShape = 0.4;
  amps[3].postTremoloMix = 0.9;
  amps[3].postTremoloCrossover = 1200.0;
  amps[3].postTremoloSync = true;
  amps[3].postTremoloDivision = 6;
  amps[3].postTremoloModes[volum::kVoLumTremoloModeOptical] = volum::TremoloModeSnapshot{2.0, 0.55, 0.10, 0.40, 600.0};
  amps[3].postTremoloModes[volum::kVoLumTremoloModeBias] = volum::TremoloModeSnapshot{4.5, 0.70, 0.25, 0.80, 900.0};
  amps[3].postTremoloModes[volum::kVoLumTremoloModeHarmonic] =
    volum::TremoloModeSnapshot{7.0, 0.95, 0.50, 1.00, 1500.0};

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
  CHECK(loaded[3].postDelaySync);
  CHECK(loaded[3].postDelayDivision == 3);
  CHECK(loaded[3].postReverbActive);
  CHECK(loaded[3].postReverbMix == doctest::Approx(0.33));
  CHECK(loaded[3].postReverbDecay == doctest::Approx(4.5));
  CHECK(loaded[3].postReverbMode == volum::kVoLumReverbModeOktaverb);
  CHECK(loaded[3].postReverbSubMode == volum::kVoLumOktaverbSubModeShimmer);
  CHECK(loaded[3].postDelayModes[volum::kVoLumDelayModeDigital].time == doctest::Approx(444.0));
  CHECK(loaded[3].postDelayModes[volum::kVoLumDelayModeAnalog].age == doctest::Approx(0.72));
  CHECK(loaded[3].postDelayModes[volum::kVoLumDelayModeReverse].mix == doctest::Approx(0.37));
  CHECK(loaded[3].postReverbModes[volum::kVoLumReverbModePlate].decay == doctest::Approx(6.7));
  CHECK(loaded[3].postReverbModes[volum::kVoLumReverbModeOktaverb].subMode == volum::kVoLumOktaverbSubModeBloom);
  CHECK(loaded[3].postOktaverbSubModes[volum::kVoLumOktaverbSubModeBloom].shimmer == doctest::Approx(0.81));
  CHECK(loaded[3].postTremoloActive);
  CHECK(loaded[3].postTremoloMode == volum::kVoLumTremoloModeHarmonic);
  CHECK(loaded[3].postTremoloRate == doctest::Approx(6.5));
  CHECK(loaded[3].postTremoloDepth == doctest::Approx(0.72));
  CHECK(loaded[3].postTremoloShape == doctest::Approx(0.4));
  CHECK(loaded[3].postTremoloMix == doctest::Approx(0.9));
  CHECK(loaded[3].postTremoloCrossover == doctest::Approx(1200.0));
  CHECK(loaded[3].postTremoloSync);
  CHECK(loaded[3].postTremoloDivision == 6);
  CHECK(loaded[3].postTremoloModes[volum::kVoLumTremoloModeOptical].rate == doctest::Approx(2.0));
  CHECK(loaded[3].postTremoloModes[volum::kVoLumTremoloModeOptical].depth == doctest::Approx(0.55));
  CHECK(loaded[3].postTremoloModes[volum::kVoLumTremoloModeBias].mix == doctest::Approx(0.80));
  CHECK(loaded[3].postTremoloModes[volum::kVoLumTremoloModeHarmonic].crossover == doctest::Approx(1500.0));
  CHECK(loaded[3].postTremoloModes[volum::kVoLumTremoloModeHarmonic].shape == doctest::Approx(0.50));

  // Untouched amps remain pending initialization (postValid=false) until selected; amp
  // restore turns that into an explicit factory POST scene.
  CHECK_FALSE(loaded[7].postValid);
}

TEST_CASE("VolumUserSettings legacy JSON without per-amp POST keeps factory POST defaults pending init")
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

// Owner-of-settings test: this file owns the canonical user-settings I/O
// contract, so it must also cover PRE/POST lock flag round-trips even though
// the lock-feature-specific tests live in test_volum_pre_post_lock.cpp. This
// guards against either file silently dropping its lock coverage.
TEST_CASE("User settings IO round-trips PRE/POST lock flags")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].toneBass = 5.5;

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, /*lastAmpIdx=*/0,
                                                          /*fx=*/nullptr, /*includeDualAmp=*/true,
                                                          /*preLocked=*/true, /*postLocked=*/false);
  REQUIRE(j["preLocked"] == true);
  REQUIRE(j["postLocked"] == false);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool preLocked = false;
  bool postLocked = true;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed, &preLocked, &postLocked);
  REQUIRE_FALSE(healed);
  REQUIRE(preLocked);
  REQUIRE_FALSE(postLocked);
  REQUIRE(loaded[0].toneBass == doctest::Approx(5.5));
}

// VoLum 1.2.0: a factory amp's selected custom IR must survive a restart.
TEST_CASE("User settings IO round-trips per-amp activeIrId")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].activeIrId = "ir_abc123";
  amps[1].activeIrId = ""; // baked cab

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  REQUIRE(j["amps"][volum::kAmps[0].folderName]["activeIrId"] == "ir_abc123");

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);
  REQUIRE_FALSE(healed);
  CHECK(loaded[0].activeIrId == "ir_abc123");
  CHECK(loaded[1].activeIrId.empty());
}

// An older settings file (no activeIrId key) must still load cleanly with the
// default empty id and no heal flag (additive forward tolerance).
TEST_CASE("User settings IO tolerates settings without activeIrId")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].activeIrId = "ir_will_be_stripped";
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  // Simulate an older writer that never emitted the key.
  for (auto& item : j["amps"].items())
    item.value().erase("activeIrId");

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);
  REQUIRE_FALSE(healed);
  CHECK(loaded[0].activeIrId.empty());
}

// VoLum 1.2.0: the dual-amp SUPPORT lane owns its own custom IR; that id must
// round-trip independently of the MAIN lane's activeIrId.
TEST_CASE("User settings IO round-trips per-amp supportActiveIrId")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].activeIrId = "ir_main";
  amps[0].supportActiveIrId = "ir_support";
  amps[1].supportActiveIrId = ""; // support baked cab

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  REQUIRE(j["amps"][volum::kAmps[0].folderName]["supportActiveIrId"] == "ir_support");

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);
  REQUIRE_FALSE(healed);
  CHECK(loaded[0].activeIrId == "ir_main");
  CHECK(loaded[0].supportActiveIrId == "ir_support");
  CHECK(loaded[1].supportActiveIrId.empty());
}

// An older settings file (no supportActiveIrId key) loads cleanly with the
// default empty id and no heal flag (additive forward tolerance).
TEST_CASE("User settings IO tolerates settings without supportActiveIrId")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].supportActiveIrId = "ir_will_be_stripped";
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  for (auto& item : j["amps"].items())
    item.value().erase("supportActiveIrId");

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);
  REQUIRE_FALSE(healed);
  CHECK(loaded[0].supportActiveIrId.empty());
}

// VoLum 1.2.0: machine-global A2 Lite mode (Full/Lite). Persisted in the user
// settings JSON, not the plugin chunk, so it round-trips here.
TEST_CASE("User settings IO round-trips machine-global liteMode")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};

  const nlohmann::json jLite =
    volum::VolumUserSettingsToJson(amps, volum::kAmpCount, /*lastAmpIdx=*/0, /*fx=*/nullptr, /*includeDualAmp=*/true,
                                   /*preLocked=*/false, /*postLocked=*/false, /*liveLockedPre=*/nullptr,
                                   /*liveLockedPost=*/nullptr, /*liteMode=*/true);
  REQUIRE(jLite["liteMode"] == true);

  bool lite = false;
  bool healed = false;
  volum::VolumUserSettingsFromJson(jLite, amps, volum::kAmpCount, nullptr, nullptr, &healed, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, &lite);
  REQUIRE_FALSE(healed);
  CHECK(lite == true);

  // Default (Full) must also round-trip as false.
  const nlohmann::json jFull = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  REQUIRE(jFull["liteMode"] == false);
  lite = true;
  volum::VolumUserSettingsFromJson(jFull, amps, volum::kAmpCount, nullptr, nullptr, &healed, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, &lite);
  CHECK(lite == false);
}

// An older settings file (no liteMode key) must load cleanly defaulting to Full
// with no heal flag (additive forward tolerance, no version bump).
TEST_CASE("User settings IO tolerates settings without liteMode (defaults to Full)")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j.erase("liteMode"); // simulate a pre-1.2.0 writer

  bool lite = true; // sentinel: prove the loader sets it back to default false
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, &healed, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, &lite);
  REQUIRE_FALSE(healed);
  CHECK(lite == false);
}

// --- Exhaustive settings round-trip pin (Phase 1 enforcement) ----------------
//
// The per-amp serializer in VolumUserSettingsToJson/FromJson hand-lists every
// VoLumAmpSettings field. Historically that list was duplicated (PreBlock/Post
// helpers AND the inline amps loop), so a new param could be added to one path
// and silently dropped from settings persistence with the suite still green.
//
// This pin sets *every* persisted field to a non-default, in-range value and
// asserts a full round-trip through the real settings JSON path. Equality uses
// the canonical composed codec (AmpSettingsEqual), so any field the settings
// path drops shows up as inequality. It also asserts the per-amp JSON object
// contains every top-level key the canonical codec emits, catching a dropped
// field structurally even if its value happened to match the default.
//
// When adding a persisted VoLumAmpSettings field you MUST also perturb it here.

namespace
{
volum::VoLumAmpSettings MakeFullyPopulatedAmpSettings()
{
  volum::VoLumAmpSettings s;
  // Core (note: settings reader clamps speaker to 0..3).
  s.speakerIdx = 1;
  s.channelIdx = 5;
  s.inputLevel = 1.5;
  s.gateThreshold = -40.0;
  s.toneBass = 3.0;
  s.toneMid = 4.0;
  s.toneTreble = 6.0;
  s.outputLevel = -2.0;
  s.noiseGateActive = false;
  s.eqActive = false;
  // PRE comp
  s.preCompActive = true;
  s.preCompAmount = 6.5;
  s.preCompRatio = 8.0;
  s.preCompAttack = 2.5;
  s.preCompRelease = 180.0;
  s.preCompMix = 0.65;
  s.preCompLevel = 1.5;
  // PRE NAM 1/2
  s.preNam1Active = true;
  s.preNam1Capture = 2;
  s.preNam1Gain = -3.0;
  s.preNam1Bass = 4.0;
  s.preNam1Mid = 6.0;
  s.preNam1MidFreq = 750.0;
  s.preNam1Treble = 7.0;
  s.preNam1Level = -1.0;
  s.preNam2Active = true;
  s.preNam2Capture = 4;
  s.preNam2Gain = 2.0;
  s.preNam2Bass = 3.0;
  s.preNam2Mid = 5.5;
  s.preNam2MidFreq = 1200.0;
  s.preNam2Treble = 8.0;
  s.preNam2Level = 0.5;
  // PRE Pitch
  s.prePitchActive = true;
  s.prePitchMode = 1;
  s.prePitchSemitones = -5.0;
  s.prePitchMix = 0.5;
  s.prePitchOctDown = 0.3;
  s.prePitchOctUp = 0.4;
  s.prePitchDry = 0.6;
  s.prePitchVoicing = 0;
  s.prePitchLevel = 2.0;
  s.prePitchTransChar = 0;
  // Dual-amp / SUPPORT lane
  s.dualAmpActive = true;
  s.dualAmpRoute = 1;
  s.mainAmpPan = -0.5;
  s.supportAmpIdx = 2;
  s.supportSpeakerIdx = 1;
  s.supportChannelIdx = 7;
  s.supportInputLevel = 1.0;
  s.supportGateThreshold = -30.0;
  s.supportToneBass = 2.0;
  s.supportToneMid = 3.0;
  s.supportToneTreble = 4.0;
  s.supportOutputLevel = -1.0;
  s.supportNoiseGateActive = false;
  s.supportEqActive = false;
  s.supportAmpPan = 0.4;
  s.supportPolarityInvert = false;
  // POST delay/reverb live
  s.postValid = true;
  s.postDelayActive = true;
  s.postDelayTime = 500.0;
  s.postDelayFeedback = 0.5;
  s.postDelayMix = 0.4;
  s.postDelayMode = 2;
  s.postDelayTone = 0.7;
  s.postDelayAge = 0.3;
  s.postDelayPingPong = true;
  s.postDelaySync = true;
  s.postDelayDivision = 2;
  s.postReverbActive = true;
  s.postReverbMix = 0.5;
  s.postReverbDecay = 4.0;
  s.postReverbTone = 7.0;
  s.postReverbPreDelay = 50.0;
  s.postReverbShimmer = 0.6;
  s.postReverbMode = 2;
  s.postReverbSubMode = 0;
  // POST tremolo
  s.postTremoloActive = true;
  s.postTremoloMode = 0;
  s.postTremoloRate = 8.0;
  s.postTremoloDepth = 0.5;
  s.postTremoloShape = 0.7;
  s.postTremoloMix = 0.6;
  s.postTremoloCrossover = 1200.0;
  s.postTremoloSync = true;
  s.postTremoloDivision = 0;
  // POST per-mode snapshots: perturb every element/field.
  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    s.postDelayModes[i].time = 100.0 + 10.0 * i;
    s.postDelayModes[i].feedback = 0.1 + 0.05 * i;
    s.postDelayModes[i].mix = 0.2 + 0.05 * i;
    s.postDelayModes[i].tone = 0.3 + 0.05 * i;
    s.postDelayModes[i].age = 0.4 + 0.05 * i;
    s.postDelayModes[i].pingPong = (i % 2 == 0);
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    s.postReverbModes[i].mix = 0.11 + 0.05 * i;
    s.postReverbModes[i].decay = 1.0 + 0.5 * i;
    s.postReverbModes[i].tone = 2.0 + 0.5 * i;
    s.postReverbModes[i].preDelay = 10.0 + 5.0 * i;
    s.postReverbModes[i].shimmer = 0.12 + 0.05 * i;
    s.postReverbModes[i].subMode = i % 3;
  }
  for (int i = 0; i < 3; ++i)
  {
    s.postOktaverbSubModes[i].mix = 0.13 + 0.05 * i;
    s.postOktaverbSubModes[i].decay = 1.5 + 0.5 * i;
    s.postOktaverbSubModes[i].tone = 2.5 + 0.5 * i;
    s.postOktaverbSubModes[i].preDelay = 12.0 + 5.0 * i;
    s.postOktaverbSubModes[i].shimmer = 0.14 + 0.05 * i;
  }
  for (int i = 0; i < volum::kVoLumTremoloModeCount; ++i)
  {
    s.postTremoloModes[i].rate = 2.0 + 1.0 * i;
    s.postTremoloModes[i].depth = 0.5 + 0.1 * i;
    s.postTremoloModes[i].shape = 0.1 + 0.1 * i;
    s.postTremoloModes[i].mix = 0.4 + 0.1 * i;
    s.postTremoloModes[i].crossover = 500.0 + 100.0 * i;
  }
  for (int i = 0; i < volum::kVoLumPitchModeCount; ++i)
  {
    s.prePitchModes[i].mix = 0.3 + 0.1 * i;
    s.prePitchModes[i].dry = 0.6 + 0.1 * i;
    s.prePitchModes[i].level = -5.0 + 2.0 * i;
    s.prePitchModes[i].voicing = i % 2;
  }
  // BYO id-based custom-content refs
  s.activeIrId = "ir_main_xyz";
  s.supportActiveIrId = "ir_support_xyz";
  s.supportCustomId = "amp_custom_xyz";
  s.supportCustomSlot = 1;
  s.supportCustomChannel = 3;
  return s;
}
} // namespace

TEST_CASE("User settings IO round-trips EVERY VoLumAmpSettings field (exhaustive pin)")
{
  const volum::VoLumAmpSettings full = MakeFullyPopulatedAmpSettings();
  // Sanity: the fixture must actually differ from defaults, otherwise the pin
  // would pass vacuously.
  REQUIRE_FALSE(volum::AmpSettingsEqual(full, volum::VoLumAmpSettings{}));

  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0] = full;

  const nlohmann::json j =
    volum::VolumUserSettingsToJson(amps, volum::kAmpCount, /*lastAmpIdx=*/0, /*fx=*/nullptr, /*includeDualAmp=*/true);

  // Structural: the per-amp object must emit every top-level key the canonical
  // composed codec emits. Catches a field dropped from the settings writer even
  // if its value coincidentally equals the default.
  const nlohmann::json canonical = volum::AmpSettingsToJson(full);
  const nlohmann::json& ampEntry = j["amps"][volum::kAmps[0].folderName];
  for (auto it = canonical.begin(); it != canonical.end(); ++it)
  {
    INFO("settings writer is missing canonical key: " << it.key());
    REQUIRE(ampEntry.contains(it.key()));
  }

  // Value: every field survives the real settings round-trip.
  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);
  REQUIRE_FALSE(healed);
  CHECK(volum::AmpSettingsEqual(loaded[0], full));
}

// The preset/scene persistence path is AmpSettingsToJson/FromJson (see
// VoLumContentStore RegistryToJson). Every existing preset round-trip asserts
// fidelity via AmpSettingsEqual, but that comparator is defined as
// AmpSettingsToJson(a) == AmpSettingsToJson(b) -- circular w.r.t. the codec
// under test, so a field DROPPED from AmpSettingsToJson would vanish from both
// sides and the check would pass vacuously. This pin instead compares the
// DECODED STRUCT FIELDS directly to the non-default input, which fails loudly
// if a 1.2.0 effect/BYO field stops surviving a preset save/reload.
TEST_CASE("Preset/scene path (AmpSettingsToJson) round-trips 1.2.0 fields struct-direct (non-circular)")
{
  volum::VoLumAmpSettings in = MakeFullyPopulatedAmpSettings();
  in.prePitchTransChar = 2; // POLY: exercise the new transpose character value

  volum::VoLumAmpSettings out{};
  // Return value is the "healed" flag (true => a field was clamped), not a
  // success code; a fully-valid snapshot needs no healing.
  CHECK_FALSE(volum::AmpSettingsFromJson(volum::AmpSettingsToJson(in), out));

  // PRE pitch (incl. POLY character + per-mode snapshots).
  CHECK(out.prePitchTransChar == in.prePitchTransChar);
  for (int i = 0; i < volum::kVoLumPitchModeCount; ++i)
  {
    INFO("prePitchModes[" << i << "]");
    CHECK(out.prePitchModes[i].mix == doctest::Approx(in.prePitchModes[i].mix));
    CHECK(out.prePitchModes[i].dry == doctest::Approx(in.prePitchModes[i].dry));
    CHECK(out.prePitchModes[i].level == doctest::Approx(in.prePitchModes[i].level));
    CHECK(out.prePitchModes[i].voicing == in.prePitchModes[i].voicing);
  }

  // POST tremolo (incl. tempo-sync + per-mode snapshots).
  CHECK(out.postTremoloSync == in.postTremoloSync);
  CHECK(out.postTremoloDivision == in.postTremoloDivision);
  for (int i = 0; i < volum::kVoLumTremoloModeCount; ++i)
  {
    INFO("postTremoloModes[" << i << "]");
    CHECK(out.postTremoloModes[i].rate == doctest::Approx(in.postTremoloModes[i].rate));
    CHECK(out.postTremoloModes[i].depth == doctest::Approx(in.postTremoloModes[i].depth));
    CHECK(out.postTremoloModes[i].shape == doctest::Approx(in.postTremoloModes[i].shape));
    CHECK(out.postTremoloModes[i].mix == doctest::Approx(in.postTremoloModes[i].mix));
    CHECK(out.postTremoloModes[i].crossover == doctest::Approx(in.postTremoloModes[i].crossover));
  }

  // POST delay tempo-sync (the two newest EParams).
  CHECK(out.postDelaySync == in.postDelaySync);
  CHECK(out.postDelayDivision == in.postDelayDivision);

  // BYO id-based custom-content references must survive a preset save/reload.
  CHECK(out.activeIrId == in.activeIrId);
  CHECK(out.supportActiveIrId == in.supportActiveIrId);
  CHECK(out.supportCustomId == in.supportCustomId);
  CHECK(out.supportCustomSlot == in.supportCustomSlot);
  CHECK(out.supportCustomChannel == in.supportCustomChannel);

  // Representative core/dual spread so an accidental block-codec divergence in
  // the preset path (vs the user-settings path) is caught here too.
  CHECK(out.toneBass == doctest::Approx(in.toneBass));
  CHECK(out.outputLevel == doctest::Approx(in.outputLevel));
  CHECK(out.dualAmpActive == in.dualAmpActive);
  CHECK(out.supportToneTreble == doctest::Approx(in.supportToneTreble));
}
