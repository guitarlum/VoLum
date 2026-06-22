#include "third_party/doctest.h"
#include "../VoLumChunkCodec.h"
#include "../VoLumChunkIdTail.h"
#include "../VoLumJsonMigration.h"
#include "../VoLumUserSettingsIO.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
struct MemoryChunk
{
  std::vector<unsigned char> bytes;

  template <typename T>
  void Put(const T* value)
  {
    const auto* first = reinterpret_cast<const unsigned char*>(value);
    bytes.insert(bytes.end(), first, first + sizeof(T));
  }

  template <typename T>
  int Get(T* value, int pos) const
  {
    REQUIRE(pos >= 0);
    REQUIRE(static_cast<size_t>(pos) + sizeof(T) <= bytes.size());
    std::memcpy(value, bytes.data() + pos, sizeof(T));
    return pos + static_cast<int>(sizeof(T));
  }
};
} // namespace

TEST_CASE("VoLum chunk codec round-trips current per-amp settings")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].speakerIdx = 1;
  amps[0].channelIdx = 2;
  amps[0].inputLevel = 1.25;
  amps[0].noiseGateActive = false;
  amps[0].eqActive = true;
  amps[0].preCompActive = true;
  amps[0].preCompAmount = 6.5;
  amps[0].preCompRatio = 8.0;
  amps[0].preCompAttack = 2.5;
  amps[0].preCompRelease = 180.0;
  amps[0].preCompMix = 0.65;
  amps[0].preNam1Active = true;
  amps[0].preNam1Capture = 7;
  amps[0].preNam2Active = true;
  amps[0].preNam2Capture = 8;
  amps[0].dualAmpActive = true;
  amps[0].dualAmpRoute = 1;
  amps[0].mainAmpPan = -0.25;
  amps[0].supportAmpIdx = 13;
  amps[0].supportSpeakerIdx = 2;
  amps[0].supportChannelIdx = 1;
  amps[0].supportInputLevel = -1.5;
  amps[0].supportGateThreshold = -65.0;
  amps[0].supportToneBass = 4.0;
  amps[0].supportToneMid = 6.0;
  amps[0].supportToneTreble = 7.0;
  amps[0].supportOutputLevel = -6.0;
  amps[0].supportNoiseGateActive = false;
  amps[0].supportEqActive = true;
  amps[0].supportAmpPan = 0.75;
  amps[0].supportPolarityInvert = true;

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {3, 2, 1}, amps, volum::kAmpCount);

  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);
  CHECK(selection.ampIdx == 3);
  CHECK(selection.speakerIdx == 2);
  CHECK(selection.channelIdx == 1);

  volum::VoLumAmpSettings loaded;
  pos = volum::GetLegacyPerAmpSettings(chunk, pos, loaded);
  pos = volum::GetExtendedPerAmpSettings(chunk, pos, loaded, true);
  pos = volum::GetDualAmpPerAmpSettings(chunk, pos, loaded);

  CHECK(loaded.speakerIdx == 1);
  CHECK(loaded.channelIdx == 2);
  CHECK(loaded.inputLevel == doctest::Approx(1.25));
  CHECK_FALSE(loaded.noiseGateActive);
  CHECK(loaded.eqActive);
  CHECK(loaded.preCompActive);
  CHECK(loaded.preCompAmount == doctest::Approx(6.5));
  CHECK(loaded.preCompRatio == doctest::Approx(8.0));
  CHECK(loaded.preCompAttack == doctest::Approx(2.5));
  CHECK(loaded.preCompRelease == doctest::Approx(180.0));
  CHECK(loaded.preCompMix == doctest::Approx(0.65));
  CHECK(loaded.preNam1Active);
  CHECK(loaded.preNam1Capture == 7);
  CHECK(loaded.preNam2Active);
  CHECK(loaded.preNam2Capture == 8);
  CHECK(loaded.dualAmpActive);
  CHECK(loaded.dualAmpRoute == 1);
  CHECK(loaded.mainAmpPan == doctest::Approx(-0.25));
  CHECK(loaded.supportAmpIdx == 13);
  CHECK(loaded.supportSpeakerIdx == 2);
  CHECK(loaded.supportChannelIdx == 1);
  CHECK(loaded.supportInputLevel == doctest::Approx(-1.5));
  CHECK(loaded.supportGateThreshold == doctest::Approx(-65.0));
  CHECK(loaded.supportToneBass == doctest::Approx(4.0));
  CHECK(loaded.supportToneMid == doctest::Approx(6.0));
  CHECK(loaded.supportToneTreble == doctest::Approx(7.0));
  CHECK(loaded.supportOutputLevel == doctest::Approx(-6.0));
  CHECK_FALSE(loaded.supportNoiseGateActive);
  CHECK(loaded.supportEqActive);
  CHECK(loaded.supportAmpPan == doctest::Approx(0.75));
  CHECK(loaded.supportPolarityInvert);
}

TEST_CASE("VoLum chunk codec round-trips every current per-amp field")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  auto& s = amps[4];
  s.speakerIdx = 2;
  s.channelIdx = 5;
  s.inputLevel = -3.25;
  s.gateThreshold = -72.5;
  s.toneBass = 2.5;
  s.toneMid = 6.5;
  s.toneTreble = 8.0;
  s.outputLevel = -4.5;
  s.noiseGateActive = false;
  s.eqActive = false;
  s.preCompActive = true;
  s.preCompAmount = 7.5;
  s.preCompRatio = 10.0;
  s.preCompAttack = 12.0;
  s.preCompRelease = 260.0;
  s.preCompMix = 0.58;
  s.preCompLevel = -2.25;
  s.preNam1Active = true;
  s.preNam1Capture = 9;
  s.preNam1Gain = 1.5;
  s.preNam1Bass = 2.0;
  s.preNam1Mid = 3.0;
  s.preNam1MidFreq = 950.0;
  s.preNam1Treble = 4.0;
  s.preNam1Level = -1.5;
  s.preNam2Active = true;
  s.preNam2Capture = 10;
  s.preNam2Gain = -2.5;
  s.preNam2Bass = 6.0;
  s.preNam2Mid = 7.0;
  s.preNam2MidFreq = 1400.0;
  s.preNam2Treble = 8.0;
  s.preNam2Level = 2.0;
  s.dualAmpActive = true;
  s.dualAmpRoute = 2;
  s.mainAmpPan = -0.33;
  s.supportAmpIdx = 6;
  s.supportSpeakerIdx = 1;
  s.supportChannelIdx = 3;
  s.supportInputLevel = -1.0;
  s.supportGateThreshold = -68.0;
  s.supportToneBass = 4.5;
  s.supportToneMid = 5.5;
  s.supportToneTreble = 6.5;
  s.supportOutputLevel = -7.5;
  s.supportNoiseGateActive = false;
  s.supportEqActive = false;
  s.supportAmpPan = 0.66;
  s.supportPolarityInvert = false;
  s.postValid = true;
  s.postDelayActive = true;
  s.postDelayTime = 777.0;
  s.postDelayFeedback = 0.61;
  s.postDelayMix = 0.43;
  s.postDelayMode = volum::kVoLumDelayModeReverse;
  s.postDelayTone = 0.73;
  s.postDelayAge = 0.44;
  s.postDelayPingPong = true;
  s.postReverbActive = true;
  s.postReverbMix = 0.36;
  s.postReverbDecay = 6.2;
  s.postReverbTone = 4.2;
  s.postReverbPreDelay = 55.0;
  s.postReverbShimmer = 0.77;
  s.postReverbMode = volum::kVoLumReverbModeOktaverb;
  s.postReverbSubMode = volum::kVoLumOktaverbSubModeHalo;
  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    s.postDelayModes[i].time = 240.0 + 100.0 * i;
    s.postDelayModes[i].feedback = 0.20 + 0.10 * i;
    s.postDelayModes[i].mix = 0.15 + 0.08 * i;
    s.postDelayModes[i].tone = 0.30 + 0.10 * i;
    s.postDelayModes[i].age = 0.10 + 0.20 * i;
    s.postDelayModes[i].pingPong = (i == 1);
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    s.postReverbModes[i].mix = 0.20 + 0.05 * i;
    s.postReverbModes[i].decay = 2.0 + i;
    s.postReverbModes[i].tone = 3.0 + i;
    s.postReverbModes[i].preDelay = 15.0 + 10.0 * i;
    s.postReverbModes[i].shimmer = 0.10 + 0.20 * i;
    s.postReverbModes[i].subMode = i % 3;
  }
  for (int i = 0; i < 3; ++i)
  {
    s.postOktaverbSubModes[i].mix = 0.25 + 0.05 * i;
    s.postOktaverbSubModes[i].decay = 4.0 + i;
    s.postOktaverbSubModes[i].tone = 5.0 + i;
    s.postOktaverbSubModes[i].preDelay = 20.0 + 8.0 * i;
    s.postOktaverbSubModes[i].shimmer = 0.40 + 0.10 * i;
  }

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {4, 2, 5}, amps, volum::kAmpCount);

  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);
  CHECK(selection.ampIdx == 4);
  CHECK(selection.speakerIdx == 2);
  CHECK(selection.channelIdx == 5);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, loaded[i]);
    pos = volum::GetExtendedPerAmpSettings(chunk, pos, loaded[i], true);
    pos = volum::GetDualAmpPerAmpSettings(chunk, pos, loaded[i], true);
    pos = volum::GetPostPerAmpSettings(chunk, pos, loaded[i]);
  }

  const auto& out = loaded[4];
  CHECK(out.speakerIdx == s.speakerIdx);
  CHECK(out.channelIdx == s.channelIdx);
  CHECK(out.inputLevel == doctest::Approx(s.inputLevel));
  CHECK(out.gateThreshold == doctest::Approx(s.gateThreshold));
  CHECK(out.toneBass == doctest::Approx(s.toneBass));
  CHECK(out.toneMid == doctest::Approx(s.toneMid));
  CHECK(out.toneTreble == doctest::Approx(s.toneTreble));
  CHECK(out.outputLevel == doctest::Approx(s.outputLevel));
  CHECK(out.noiseGateActive == s.noiseGateActive);
  CHECK(out.eqActive == s.eqActive);
  CHECK(out.preCompActive == s.preCompActive);
  CHECK(out.preCompAmount == doctest::Approx(s.preCompAmount));
  CHECK(out.preCompRatio == doctest::Approx(s.preCompRatio));
  CHECK(out.preCompAttack == doctest::Approx(s.preCompAttack));
  CHECK(out.preCompRelease == doctest::Approx(s.preCompRelease));
  CHECK(out.preCompMix == doctest::Approx(s.preCompMix));
  CHECK(out.preCompLevel == doctest::Approx(s.preCompLevel));
  CHECK(out.preNam1Active == s.preNam1Active);
  CHECK(out.preNam1Capture == s.preNam1Capture);
  CHECK(out.preNam1Gain == doctest::Approx(s.preNam1Gain));
  CHECK(out.preNam1Bass == doctest::Approx(s.preNam1Bass));
  CHECK(out.preNam1Mid == doctest::Approx(s.preNam1Mid));
  CHECK(out.preNam1MidFreq == doctest::Approx(s.preNam1MidFreq));
  CHECK(out.preNam1Treble == doctest::Approx(s.preNam1Treble));
  CHECK(out.preNam1Level == doctest::Approx(s.preNam1Level));
  CHECK(out.preNam2Active == s.preNam2Active);
  CHECK(out.preNam2Capture == s.preNam2Capture);
  CHECK(out.preNam2Gain == doctest::Approx(s.preNam2Gain));
  CHECK(out.preNam2Bass == doctest::Approx(s.preNam2Bass));
  CHECK(out.preNam2Mid == doctest::Approx(s.preNam2Mid));
  CHECK(out.preNam2MidFreq == doctest::Approx(s.preNam2MidFreq));
  CHECK(out.preNam2Treble == doctest::Approx(s.preNam2Treble));
  CHECK(out.preNam2Level == doctest::Approx(s.preNam2Level));
  CHECK(out.dualAmpActive == s.dualAmpActive);
  CHECK(out.dualAmpRoute == s.dualAmpRoute);
  CHECK(out.mainAmpPan == doctest::Approx(s.mainAmpPan));
  CHECK(out.supportAmpIdx == s.supportAmpIdx);
  CHECK(out.supportSpeakerIdx == s.supportSpeakerIdx);
  CHECK(out.supportChannelIdx == s.supportChannelIdx);
  CHECK(out.supportInputLevel == doctest::Approx(s.supportInputLevel));
  CHECK(out.supportGateThreshold == doctest::Approx(s.supportGateThreshold));
  CHECK(out.supportToneBass == doctest::Approx(s.supportToneBass));
  CHECK(out.supportToneMid == doctest::Approx(s.supportToneMid));
  CHECK(out.supportToneTreble == doctest::Approx(s.supportToneTreble));
  CHECK(out.supportOutputLevel == doctest::Approx(s.supportOutputLevel));
  CHECK(out.supportNoiseGateActive == s.supportNoiseGateActive);
  CHECK(out.supportEqActive == s.supportEqActive);
  CHECK(out.supportAmpPan == doctest::Approx(s.supportAmpPan));
  CHECK(out.supportPolarityInvert == s.supportPolarityInvert);
  CHECK(out.postValid == s.postValid);
  CHECK(out.postDelayActive == s.postDelayActive);
  CHECK(out.postDelayTime == doctest::Approx(s.postDelayTime));
  CHECK(out.postDelayFeedback == doctest::Approx(s.postDelayFeedback));
  CHECK(out.postDelayMix == doctest::Approx(s.postDelayMix));
  CHECK(out.postDelayMode == s.postDelayMode);
  CHECK(out.postDelayTone == doctest::Approx(s.postDelayTone));
  CHECK(out.postDelayAge == doctest::Approx(s.postDelayAge));
  CHECK(out.postDelayPingPong == s.postDelayPingPong);
  CHECK(out.postReverbActive == s.postReverbActive);
  CHECK(out.postReverbMix == doctest::Approx(s.postReverbMix));
  CHECK(out.postReverbDecay == doctest::Approx(s.postReverbDecay));
  CHECK(out.postReverbTone == doctest::Approx(s.postReverbTone));
  CHECK(out.postReverbPreDelay == doctest::Approx(s.postReverbPreDelay));
  CHECK(out.postReverbShimmer == doctest::Approx(s.postReverbShimmer));
  CHECK(out.postReverbMode == s.postReverbMode);
  CHECK(out.postReverbSubMode == s.postReverbSubMode);
  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    CHECK(out.postDelayModes[i].time == doctest::Approx(s.postDelayModes[i].time));
    CHECK(out.postDelayModes[i].feedback == doctest::Approx(s.postDelayModes[i].feedback));
    CHECK(out.postDelayModes[i].mix == doctest::Approx(s.postDelayModes[i].mix));
    CHECK(out.postDelayModes[i].tone == doctest::Approx(s.postDelayModes[i].tone));
    CHECK(out.postDelayModes[i].age == doctest::Approx(s.postDelayModes[i].age));
    CHECK(out.postDelayModes[i].pingPong == s.postDelayModes[i].pingPong);
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    CHECK(out.postReverbModes[i].mix == doctest::Approx(s.postReverbModes[i].mix));
    CHECK(out.postReverbModes[i].decay == doctest::Approx(s.postReverbModes[i].decay));
    CHECK(out.postReverbModes[i].tone == doctest::Approx(s.postReverbModes[i].tone));
    CHECK(out.postReverbModes[i].preDelay == doctest::Approx(s.postReverbModes[i].preDelay));
    CHECK(out.postReverbModes[i].shimmer == doctest::Approx(s.postReverbModes[i].shimmer));
    CHECK(out.postReverbModes[i].subMode == s.postReverbModes[i].subMode);
  }
  for (int i = 0; i < 3; ++i)
  {
    CHECK(out.postOktaverbSubModes[i].mix == doctest::Approx(s.postOktaverbSubModes[i].mix));
    CHECK(out.postOktaverbSubModes[i].decay == doctest::Approx(s.postOktaverbSubModes[i].decay));
    CHECK(out.postOktaverbSubModes[i].tone == doctest::Approx(s.postOktaverbSubModes[i].tone));
    CHECK(out.postOktaverbSubModes[i].preDelay == doctest::Approx(s.postOktaverbSubModes[i].preDelay));
    CHECK(out.postOktaverbSubModes[i].shimmer == doctest::Approx(s.postOktaverbSubModes[i].shimmer));
  }

  CHECK(static_cast<size_t>(pos) == chunk.bytes.size());
}

TEST_CASE("VoLum chunk codec clamps legacy out-of-range dualAmpRoute and supportOutputLevel")
{
  // Synthesize a chunk whose dual-amp block carries values outside the new accepted ranges.
  // The decoder must clamp on the way out so old / hand-edited presets can't poison runtime state.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].dualAmpActive = true;
  amps[0].dualAmpRoute = 99;
  amps[0].mainAmpPan = -5.0;
  amps[0].supportAmpIdx = 999;
  amps[0].supportSpeakerIdx = 99;
  amps[0].supportInputLevel = 50.0;
  amps[0].supportGateThreshold = 50.0;
  amps[0].supportToneBass = 99.0;
  amps[0].supportToneMid = -10.0;
  amps[0].supportToneTreble = 99.0;
  amps[0].supportOutputLevel = 30.0;
  amps[0].supportAmpPan = 5.0;

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {0, 0, 0}, amps, volum::kAmpCount);

  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);

  volum::VoLumAmpSettings loaded;
  pos = volum::GetLegacyPerAmpSettings(chunk, pos, loaded);
  pos = volum::GetExtendedPerAmpSettings(chunk, pos, loaded, true);
  pos = volum::GetDualAmpPerAmpSettings(chunk, pos, loaded);

  CHECK(loaded.dualAmpRoute >= 0);
  CHECK(loaded.dualAmpRoute <= 2);
  CHECK(loaded.dualAmpRoute == 2);
  CHECK(loaded.mainAmpPan == doctest::Approx(-1.0));
  CHECK(loaded.supportAmpIdx == volum::kAmpCount - 1);
  CHECK(loaded.supportSpeakerIdx == 3);
  CHECK(loaded.supportInputLevel == doctest::Approx(20.0));
  CHECK(loaded.supportGateThreshold == doctest::Approx(0.0));
  CHECK(loaded.supportToneBass == doctest::Approx(10.0));
  CHECK(loaded.supportToneMid == doctest::Approx(0.0));
  CHECK(loaded.supportToneTreble == doctest::Approx(10.0));
  CHECK(loaded.supportOutputLevel == doctest::Approx(10.0));
  CHECK(loaded.supportAmpPan == doctest::Approx(1.0));
}

TEST_CASE("Legacy dual-amp chunk defaults support polarity invert on")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].dualAmpActive = true;
  amps[0].supportAmpIdx = 1;
  amps[0].supportPolarityInvert = false;

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {0, 0, 0}, amps, volum::kAmpCount);

  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);

  volum::VoLumAmpSettings loaded;
  loaded.supportPolarityInvert = true;
  pos = volum::GetLegacyPerAmpSettings(chunk, pos, loaded);
  pos = volum::GetExtendedPerAmpSettings(chunk, pos, loaded, true);
  pos = volum::GetDualAmpPerAmpSettings(chunk, pos, loaded, /*hasSupportPolarityInvert=*/false);

  CHECK(loaded.dualAmpActive == true);
  CHECK(loaded.supportAmpIdx == 1);
  CHECK(loaded.supportPolarityInvert == true);
}

TEST_CASE("VoLum chunk codec clamps selection and PRE capture indices")
{
  volum::VoLumChunkSelection selection{-99, 99, -1};
  selection = volum::ClampChunkSelection(selection);
  CHECK(selection.ampIdx == 0);
  CHECK(selection.speakerIdx == 3);
  CHECK(selection.channelIdx == 0);

  volum::VoLumAmpSettings settings;
  settings.preNam1Capture = -4;
  settings.preNam2Capture = 300;
  volum::ClampPreCaptureSlots(settings, 200);
  CHECK(settings.preNam1Capture == 0);
  CHECK(settings.preNam2Capture == volum::kPreCaptureMaxParamIndex);
}

TEST_CASE("VoLum chunk codec resets legacy PRE captures before 0.8.4")
{
  CHECK(volum::ShouldResetPreCaptureSlotsForChunkVersion(volum::ChunkVersion("0.8.3")));
  CHECK_FALSE(volum::ShouldResetPreCaptureSlotsForChunkVersion(volum::ChunkVersion("0.8.4")));

  volum::VoLumAmpSettings settings;
  settings.preNam1Active = true;
  settings.preNam1Capture = 3;
  settings.preNam1Gain = 4.0;
  settings.preNam2Active = true;
  settings.preNam2Capture = 7;
  settings.preNam2Level = -2.0;

  volum::ResetPreCaptureSlots(settings);

  CHECK(settings.preNam1Active == true);
  CHECK(settings.preNam1Capture == 0);
  CHECK(settings.preNam1Gain == doctest::Approx(4.0));
  CHECK(settings.preNam2Active == true);
  CHECK(settings.preNam2Capture == 0);
  CHECK(settings.preNam2Level == doctest::Approx(-2.0));
}

TEST_CASE("VoLum JSON migration does not synthesize null params for missing legacy keys")
{
  nlohmann::json config = {
    {"RigFile", 2.0},
    {"Input", 0.0},
  };

  CHECK_FALSE(volum::RenameJsonKeyIfPresent(config, "AmpeteRig", "RigFile"));
  REQUIRE(config.contains("RigFile"));
  CHECK(config["RigFile"].is_number());
  CHECK(config["RigFile"].get<double>() == doctest::Approx(2.0));

  CHECK(volum::RenameJsonKeyIfPresent(config, "RigFile", "RigFileMigrated"));
  CHECK_FALSE(config.contains("RigFile"));
  REQUIRE(config.contains("RigFileMigrated"));
  CHECK(config["RigFileMigrated"].get<double>() == doctest::Approx(2.0));
}

TEST_CASE("effect-staging migration: old Tape and PingPong modes fold into Digital")
{
  {
    nlohmann::json config = {{"DelayMode", 0.0}};
    volum::MigrateDelayReverbToV0_9_0(config);
    CHECK(config["DelayMode"].get<double>() == doctest::Approx(0.0));
    CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));
  }
  {
    nlohmann::json config = {{"DelayMode", 2.0}};
    volum::MigrateDelayReverbToV0_9_0(config);
    CHECK(config["DelayMode"].get<double>() == doctest::Approx(0.0));
    CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(1.0));
  }
  {
    nlohmann::json config = {{"DelayMode", 3.0}};
    volum::MigrateDelayReverbToV0_9_0(config);
    CHECK(config["DelayMode"].get<double>() == doctest::Approx(2.0));
    CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));
  }
}

TEST_CASE("effect-staging migration: adds only live staging params")
{
  nlohmann::json config = nlohmann::json::object();
  volum::MigrateDelayReverbToV0_9_0(config);

  REQUIRE(config.contains("DelayTone"));
  REQUIRE(config.contains("DelayAge"));
  REQUIRE(config.contains("DelayPingPong"));
  REQUIRE(config.contains("ReverbSubMode"));
  CHECK_FALSE(config.contains("DelayTapeSubMode"));
  CHECK_FALSE(config.contains("ReverbTremRate"));
  CHECK(config["DelayTone"].get<double>() == doctest::Approx(0.5));
  CHECK(config["DelayAge"].get<double>() == doctest::Approx(0.0));
  CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));
  CHECK(config["ReverbSubMode"].get<double>() == doctest::Approx(0.0));
}

TEST_CASE("VoLum chunk codec round-trips POST per-amp settings")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[2].postValid = true;
  amps[2].postDelayActive = true;
  amps[2].postDelayMode = volum::kVoLumDelayModeReverse;
  amps[2].postDelayMix = 0.42;
  amps[2].postDelayTime = 555.0;
  amps[2].postDelayFeedback = 0.55;
  amps[2].postDelayTone = 0.7;
  amps[2].postDelayAge = 0.4;
  amps[2].postDelayPingPong = false;
  amps[2].postReverbActive = true;
  amps[2].postReverbMix = 0.37;
  amps[2].postReverbDecay = 5.5;
  amps[2].postReverbTone = 6.5;
  amps[2].postReverbPreDelay = 45.0;
  amps[2].postReverbShimmer = 0.65;
  amps[2].postReverbMode = volum::kVoLumReverbModeOktaverb;
  amps[2].postReverbSubMode = volum::kVoLumOktaverbSubModeBloom;
  amps[2].postDelayModes[volum::kVoLumDelayModeDigital].time = 444.0;
  amps[2].postDelayModes[volum::kVoLumDelayModeAnalog].age = 0.72;
  amps[2].postDelayModes[volum::kVoLumDelayModeReverse].mix = 0.37;
  amps[2].postReverbModes[volum::kVoLumReverbModePlate].decay = 6.7;
  amps[2].postReverbModes[volum::kVoLumReverbModeOktaverb].subMode = volum::kVoLumOktaverbSubModeBloom;
  amps[2].postOktaverbSubModes[volum::kVoLumOktaverbSubModeBloom].shimmer = 0.81;

  MemoryChunk chunk;
  volum::VoLumChunkSelection selection{2, 1, 0};
  volum::PutCurrentVoLumChunkState(chunk, selection, amps, volum::kAmpCount);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VoLumChunkSelection loadedSel{};
  int pos = volum::GetVoLumChunkSelection(chunk, 0, loadedSel);
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, loaded[i]);
    pos = volum::GetExtendedPerAmpSettings(chunk, pos, loaded[i], true);
    pos = volum::GetDualAmpPerAmpSettings(chunk, pos, loaded[i], true);
    pos = volum::GetPostPerAmpSettings(chunk, pos, loaded[i]);
  }

  CHECK(loaded[2].postValid);
  CHECK(loaded[2].postDelayActive);
  CHECK(loaded[2].postDelayMode == volum::kVoLumDelayModeReverse);
  CHECK(loaded[2].postDelayMix == doctest::Approx(0.42));
  CHECK(loaded[2].postDelayTime == doctest::Approx(555.0));
  CHECK(loaded[2].postDelayFeedback == doctest::Approx(0.55));
  CHECK(loaded[2].postDelayTone == doctest::Approx(0.7));
  CHECK(loaded[2].postDelayAge == doctest::Approx(0.4));
  CHECK_FALSE(loaded[2].postDelayPingPong);
  CHECK(loaded[2].postReverbActive);
  CHECK(loaded[2].postReverbMix == doctest::Approx(0.37));
  CHECK(loaded[2].postReverbDecay == doctest::Approx(5.5));
  CHECK(loaded[2].postReverbTone == doctest::Approx(6.5));
  CHECK(loaded[2].postReverbPreDelay == doctest::Approx(45.0));
  CHECK(loaded[2].postReverbShimmer == doctest::Approx(0.65));
  CHECK(loaded[2].postReverbMode == volum::kVoLumReverbModeOktaverb);
  CHECK(loaded[2].postReverbSubMode == volum::kVoLumOktaverbSubModeBloom);
  CHECK(loaded[2].postDelayModes[volum::kVoLumDelayModeDigital].time == doctest::Approx(444.0));
  CHECK(loaded[2].postDelayModes[volum::kVoLumDelayModeAnalog].age == doctest::Approx(0.72));
  CHECK(loaded[2].postDelayModes[volum::kVoLumDelayModeReverse].mix == doctest::Approx(0.37));
  CHECK(loaded[2].postReverbModes[volum::kVoLumReverbModePlate].decay == doctest::Approx(6.7));
  CHECK(loaded[2].postReverbModes[volum::kVoLumReverbModeOktaverb].subMode == volum::kVoLumOktaverbSubModeBloom);
  CHECK(loaded[2].postOktaverbSubModes[volum::kVoLumOktaverbSubModeBloom].shimmer == doctest::Approx(0.81));

  // Untouched amp slots round-trip with postValid==false (the struct default), so the
  // restore path leaves the active EParams alone for those amps.
  CHECK_FALSE(loaded[5].postValid);
}

// 1.0 contract: the chunk codec must survive any payload (corrupt session, hand-edited
// preset, partial write, version skew) without crashing, and the dedicated Clamp helpers
// (ClampChunkSelection / ClampPreCaptureSlots / GetDualAmpPerAmpSettings dual-amp range
// clamp) must produce safe runtime state for the fields they own. Other fields are
// clamped by iPlug2's IParam ranges when written back into the live plugin; the codec
// itself is allowed to pass them through verbatim. This fuzz pass confirms (a) no crash
// or hang on extreme / non-finite input and (b) the clamped fields always land in range.
TEST_CASE("VoLum chunk codec round-trip fuzz: extreme / non-finite values stay bounded")
{
  const double bigPos = 1e100;
  const double bigNeg = -1e100;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();

  struct Poison
  {
    double value;
    const char* label;
  };
  const Poison poisonValues[] = {
    {0.0, "zero"}, {-0.0, "neg zero"}, {bigPos, "bigpos"}, {bigNeg, "bigneg"},
    {nan, "nan"},  {inf, "+inf"},      {-inf, "-inf"},     {1e-300, "subnorm"},
  };

  for (const auto& poison : poisonValues)
  {
    CAPTURE(poison.label);
    volum::VoLumAmpSettings amps[volum::kAmpCount]{};
    amps[0].speakerIdx = 99999;
    amps[0].channelIdx = -50;
    amps[0].inputLevel = poison.value;
    amps[0].preCompAmount = poison.value;
    amps[0].preCompRatio = poison.value;
    amps[0].preCompMix = poison.value;
    amps[0].mainAmpPan = poison.value;
    amps[0].supportInputLevel = poison.value;
    amps[0].supportToneBass = poison.value;
    amps[0].supportToneMid = poison.value;
    amps[0].supportToneTreble = poison.value;
    amps[0].supportOutputLevel = poison.value;
    amps[0].supportGateThreshold = poison.value;
    amps[0].supportAmpPan = poison.value;
    amps[0].preNam1Capture = -42;
    amps[0].preNam2Capture = 99999;

    // Serializing must not crash on any input.
    MemoryChunk chunk;
    volum::PutCurrentVoLumChunkState(chunk, {99, 99, -7}, amps, volum::kAmpCount);
    CHECK(chunk.bytes.size() > 0);

    // The selection clamp helper must always produce in-range values.
    volum::VoLumChunkSelection selection;
    int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);
    selection = volum::ClampChunkSelection(selection);
    CHECK(selection.ampIdx >= 0);
    CHECK(selection.ampIdx < volum::kAmpCount);
    CHECK(selection.speakerIdx >= 0);
    CHECK(selection.speakerIdx <= 3);
    CHECK(selection.channelIdx >= 0);

    // Decoding must not crash; we don't assert per-field clamps that the codec
    // doesn't own (iPlug2 IParam ranges do that on the write-back path).
    volum::VoLumAmpSettings loaded;
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, loaded);
    pos = volum::GetExtendedPerAmpSettings(chunk, pos, loaded, true);
    pos = volum::GetDualAmpPerAmpSettings(chunk, pos, loaded);

    // PRE capture clamp helper must always produce in-range indices.
    volum::ClampPreCaptureSlots(loaded, 200);
    CHECK(loaded.preNam1Capture >= 0);
    CHECK(loaded.preNam1Capture <= volum::kPreCaptureMaxParamIndex);
    CHECK(loaded.preNam2Capture >= 0);
    CHECK(loaded.preNam2Capture <= volum::kPreCaptureMaxParamIndex);

    // Dual-amp clamp built into GetDualAmpPerAmpSettings: route and pan stay in range.
    CHECK(loaded.dualAmpRoute >= 0);
    CHECK(loaded.dualAmpRoute <= 2);
    auto inFinitePanRange = [](double v) {
      // Caller (plugin restore path) zeroes any non-finite value via the IParam clamp;
      // codec's documented invariant here is: any FINITE pan is in [-1, 1].
      return !std::isfinite(v) || (v >= -1.0 && v <= 1.0);
    };
    CHECK(inFinitePanRange(loaded.mainAmpPan));
    CHECK(inFinitePanRange(loaded.supportAmpPan));
  }
}

TEST_CASE("Oktaverb v0.9.1 migration remaps legacy sub-modes")
{
  {
    nlohmann::json config = {{"ReverbSubMode", 0.0}};
    volum::MigrateOktaverbSubModeToV0_9_1(config);
    CHECK(config["ReverbSubMode"].get<double>() == doctest::Approx(1.0));
  }
  {
    nlohmann::json config = {{"ReverbSubMode", 1.0}};
    volum::MigrateOktaverbSubModeToV0_9_1(config);
    CHECK(config["ReverbSubMode"].get<double>() == doctest::Approx(1.0));
  }
  {
    nlohmann::json config = {{"ReverbSubMode", 2.0}};
    volum::MigrateOktaverbSubModeToV0_9_1(config);
    CHECK(config["ReverbSubMode"].get<double>() == doctest::Approx(0.0));
  }
}

// ---------------------------------------------------------------------------
// VoLum 1.2.0 DAW-chunk id tail (BYO/preset references).
// ---------------------------------------------------------------------------

TEST_CASE("VoLum 1.2.0 id tail round-trips through the chunk")
{
  volum::ChunkIdTail in;
  in.customMainId = "amp_main_abc";
  in.customSupportId = "amp_sup_def";
  in.activePresetId = "preset_xyz";
  in.perAmpIrId[0] = "ir_one";
  in.perAmpIrId[volum::kAmpCount - 1] = "ir_last";
  in.perAmpSupportIrId[0] = "ir_sup_one";
  in.perAmpSupportId[1] = "amp_sup_ghi";

  MemoryChunk chunk;
  volum::PutChunkIdTail(chunk, in);

  volum::ChunkIdTail out;
  int posOut = -1;
  const bool ok = volum::TryGetChunkIdTail(chunk, 0, static_cast<int>(chunk.bytes.size()), out, &posOut);
  REQUIRE(ok);
  CHECK(posOut == static_cast<int>(chunk.bytes.size()));
  CHECK(out.customMainId == in.customMainId);
  CHECK(out.customSupportId == in.customSupportId);
  CHECK(out.activePresetId == in.activePresetId);
  CHECK(out.perAmpIrId[0] == "ir_one");
  CHECK(out.perAmpIrId[volum::kAmpCount - 1] == "ir_last");
  CHECK(out.perAmpSupportIrId[0] == "ir_sup_one");
  CHECK(out.perAmpSupportId[1] == "amp_sup_ghi");
  // Untouched slots stay empty.
  CHECK(out.perAmpIrId[1].empty());
  CHECK(out.perAmpSupportIrId[1].empty());
  CHECK(out.perAmpSupportId[0].empty());
}

TEST_CASE("Id tail probe coexists with preceding fixed-tail bytes")
{
  // Simulate the real layout: arbitrary fixed-tail bytes, then the id tail.
  MemoryChunk chunk;
  for (int i = 0; i < 37; ++i)
  {
    unsigned char b = static_cast<unsigned char>(i * 7 + 3);
    chunk.Put(&b);
  }
  const int tailStart = static_cast<int>(chunk.bytes.size());

  volum::ChunkIdTail in;
  in.customMainId = "main";
  volum::PutChunkIdTail(chunk, in);

  volum::ChunkIdTail out;
  CHECK(volum::TryGetChunkIdTail(chunk, tailStart, static_cast<int>(chunk.bytes.size()), out));
  CHECK(out.customMainId == "main");
  // Probing at the wrong offset (no sentinel there) must fail cleanly.
  CHECK_FALSE(volum::TryGetChunkIdTail(chunk, 0, static_cast<int>(chunk.bytes.size()), out));
}

TEST_CASE("Pre-1.2.0 chunk (no id tail) probes empty, never throws")
{
  // 1.0.0/1.0.1/1.1.0 chunks end right after the fixed binary tail. A 1.2.0
  // reader probing for the id tail must report 'absent' and leave refs empty.
  MemoryChunk chunk;
  for (int i = 0; i < 24; ++i)
  {
    double d = i * 0.5;
    chunk.Put(&d);
  }
  volum::ChunkIdTail out;
  out.customMainId = "should_be_overwritten_only_on_success";
  const int sz = static_cast<int>(chunk.bytes.size());

  CHECK_FALSE(volum::TryGetChunkIdTail(chunk, sz, sz, out)); // pos at EOF
  CHECK_FALSE(volum::TryGetChunkIdTail(chunk, sz - 4, sz, out)); // not enough room
  CHECK_FALSE(volum::TryGetChunkIdTail(chunk, 0, sz, out)); // no sentinel
  // Empty chunk.
  MemoryChunk empty;
  CHECK_FALSE(volum::TryGetChunkIdTail(empty, 0, 0, out));
}

TEST_CASE("Id tail with malformed JSON body is rejected (lenient)")
{
  MemoryChunk chunk;
  int sentinel = volum::kVoLumIdTailSentinel;
  const std::string bad = "{not valid json";
  int len = static_cast<int>(bad.size());
  chunk.Put(&sentinel);
  chunk.Put(&len);
  for (char c : bad)
    chunk.Put(&c);

  volum::ChunkIdTail out;
  CHECK_FALSE(volum::TryGetChunkIdTail(chunk, 0, static_cast<int>(chunk.bytes.size()), out));
}

TEST_CASE("Id tail with length running past the chunk is rejected")
{
  MemoryChunk chunk;
  int sentinel = volum::kVoLumIdTailSentinel;
  int len = 9999; // claims far more than is present
  chunk.Put(&sentinel);
  chunk.Put(&len);
  const char c = 'x';
  chunk.Put(&c);

  volum::ChunkIdTail out;
  CHECK_FALSE(volum::TryGetChunkIdTail(chunk, 0, static_cast<int>(chunk.bytes.size()), out));
}

TEST_CASE("Empty id tail round-trips (all refs blank)")
{
  volum::ChunkIdTail in; // all empty
  MemoryChunk chunk;
  volum::PutChunkIdTail(chunk, in);

  volum::ChunkIdTail out;
  out.customMainId = "dirty";
  REQUIRE(volum::TryGetChunkIdTail(chunk, 0, static_cast<int>(chunk.bytes.size()), out));
  CHECK(out.customMainId.empty());
  CHECK(out.customSupportId.empty());
  CHECK(out.activePresetId.empty());
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    CHECK(out.perAmpIrId[i].empty());
    CHECK(out.perAmpSupportIrId[i].empty());
    CHECK(out.perAmpSupportId[i].empty());
  }
}

// A schema-1 id tail (1.2.0 pre-support-IR) has no "supIr" per-amp key. A current
// reader must load it cleanly, leaving every supportActiveIrId empty.
TEST_CASE("Id tail without supIr keys (older schema) reads support IR ids empty")
{
  // Hand-build a schema-1 perAmp entry (only ir + sup, no supIr).
  nlohmann::json j;
  j["v"] = 1;
  j["customMainId"] = "";
  j["customSupportId"] = "";
  j["activePresetId"] = "";
  nlohmann::json perAmp = nlohmann::json::array();
  for (int i = 0; i < volum::kAmpCount; ++i)
    perAmp.push_back({{"ir", i == 0 ? "ir_legacy" : ""}, {"sup", ""}});
  j["perAmp"] = perAmp;
  const std::string body = j.dump();

  MemoryChunk chunk;
  int sentinel = volum::kVoLumIdTailSentinel;
  int len = static_cast<int>(body.size());
  chunk.Put(&sentinel);
  chunk.Put(&len);
  for (char c : body)
    chunk.Put(&c);

  volum::ChunkIdTail out;
  REQUIRE(volum::TryGetChunkIdTail(chunk, 0, static_cast<int>(chunk.bytes.size()), out));
  CHECK(out.perAmpIrId[0] == "ir_legacy");
  for (int i = 0; i < volum::kAmpCount; ++i)
    CHECK(out.perAmpSupportIrId[i].empty());
}
