#include "third_party/doctest.h"
#include "../VoLumChunkCodec.h"
#include "../VoLumJsonMigration.h"

#include <cstring>
#include <vector>

namespace
{
struct MemoryChunk
{
  std::vector<unsigned char> bytes;

  template<typename T>
  void Put(const T* value)
  {
    const auto* first = reinterpret_cast<const unsigned char*>(value);
    bytes.insert(bytes.end(), first, first + sizeof(T));
  }

  template<typename T>
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

// =====================================================================================
// v0.9.0 chunk-state migration: pre-0.9.0 saves stored DelayMode under the old
// {Tape, Digital, PingPong, Reverse} order. v0.9.0 reorders to {Digital, Analog, Tape,
// Reverse} and folds PingPong into Digital + DelayPingPong=true. Six new EParams must be
// populated with neutral defaults so _UnserializeApplyConfig has values to write.
// =====================================================================================

TEST_CASE("v0.9.0 migration: old Tape (mode 0) -> new Tape (mode 2), PingPong stays off")
{
  nlohmann::json config = {{"DelayMode", 0.0}};
  volum::MigrateDelayReverbToV0_9_0(config);
  CHECK(config["DelayMode"].get<double>() == doctest::Approx(2.0));
  CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));
}

TEST_CASE("v0.9.0 migration: old Digital (mode 1) -> new Digital (mode 0), PingPong stays off")
{
  nlohmann::json config = {{"DelayMode", 1.0}};
  volum::MigrateDelayReverbToV0_9_0(config);
  CHECK(config["DelayMode"].get<double>() == doctest::Approx(0.0));
  CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));
}

TEST_CASE("v0.9.0 migration: old PingPong (mode 2) -> new Digital (mode 0) + DelayPingPong=true")
{
  nlohmann::json config = {{"DelayMode", 2.0}};
  volum::MigrateDelayReverbToV0_9_0(config);
  CHECK(config["DelayMode"].get<double>() == doctest::Approx(0.0));
  CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(1.0));
}

TEST_CASE("v0.9.0 migration: old Reverse (mode 3) stays at new Reverse (mode 3)")
{
  nlohmann::json config = {{"DelayMode", 3.0}};
  volum::MigrateDelayReverbToV0_9_0(config);
  CHECK(config["DelayMode"].get<double>() == doctest::Approx(3.0));
  CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));
}

TEST_CASE("v0.9.0 migration: populates neutral defaults for new EParams when missing")
{
  // A pre-0.9.0 chunk only has the old keys; the migration must add neutral defaults for
  // every new v0.9.0 EParam so _UnserializeApplyConfig can write them. We don't assume the
  // chunk had a DelayMode at all (some early versions pre-date it).
  nlohmann::json config = nlohmann::json::object();
  volum::MigrateDelayReverbToV0_9_0(config);

  REQUIRE(config.contains("DelayTone"));
  REQUIRE(config.contains("DelayAge"));
  REQUIRE(config.contains("DelayPingPong"));
  REQUIRE(config.contains("DelayTapeSubMode"));
  REQUIRE(config.contains("ReverbSubMode"));
  REQUIRE(config.contains("ReverbTremRate"));
  CHECK(config["DelayTone"].get<double>() == doctest::Approx(0.5));
  CHECK(config["DelayAge"].get<double>() == doctest::Approx(0.0));
  CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));
  CHECK(config["DelayTapeSubMode"].get<double>() == doctest::Approx(1.0));
  CHECK(config["ReverbSubMode"].get<double>() == doctest::Approx(1.0));
  CHECK(config["ReverbTremRate"].get<double>() == doctest::Approx(4.0));
}

TEST_CASE("v0.9.0 migration: leaves reverb mode index untouched (Hall/Plate/Oktaverb)")
{
  // Old reverb modes 0/1/2 keep their meaning under v0.9.0; only mode 3 (TremVerb) is new.
  for (double m : {0.0, 1.0, 2.0})
  {
    nlohmann::json config = {{"ReverbMode", m}};
    volum::MigrateDelayReverbToV0_9_0(config);
    CHECK(config["ReverbMode"].get<double>() == doctest::Approx(m));
  }
}

TEST_CASE("v0.9.0 migration: ignores non-numeric or out-of-range DelayMode safely")
{
  // Make sure malformed input doesn't crash and lands on a sane new mode.
  nlohmann::json config = {{"DelayMode", "garbage"}};
  volum::MigrateDelayReverbToV0_9_0(config);
  // Non-number input: original DelayMode stays as-is (string), but new EParams are populated.
  CHECK(config["DelayPingPong"].get<double>() == doctest::Approx(0.0));

  nlohmann::json config2 = {{"DelayMode", 99.0}};
  volum::MigrateDelayReverbToV0_9_0(config2);
  // Out-of-range: maps to new Digital (0) with PingPong off.
  CHECK(config2["DelayMode"].get<double>() == doctest::Approx(0.0));
  CHECK(config2["DelayPingPong"].get<double>() == doctest::Approx(0.0));
}
