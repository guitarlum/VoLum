#include "third_party/doctest.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumChunkLayout.h"
#include "../VoLumChunkVersion.h"
#include "../VoLumJsonMigration.h"

#include <cmath>

#include <vector>

namespace
{
template<typename T>
void AppendBytes(std::vector<unsigned char>& bytes, const T& value)
{
  const auto* first = reinterpret_cast<const unsigned char*>(&value);
  bytes.insert(bytes.end(), first, first + sizeof(T));
}

void AppendLegacyPerAmpBlock(std::vector<unsigned char>& bytes)
{
  const int speaker = 3;
  const int channel = 1;
  const double input = 0.0;
  const double gate = -60.0;
  const double bass = 5.0;
  const double mid = 5.0;
  const double treble = 5.0;
  const double output = 0.0;
  const int noiseGate = 1;
  const int eq = 1;

  AppendBytes(bytes, speaker);
  AppendBytes(bytes, channel);
  AppendBytes(bytes, input);
  AppendBytes(bytes, gate);
  AppendBytes(bytes, bass);
  AppendBytes(bytes, mid);
  AppendBytes(bytes, treble);
  AppendBytes(bytes, output);
  AppendBytes(bytes, noiseGate);
  AppendBytes(bytes, eq);
}

void AppendCurrentPerAmpBlock(std::vector<unsigned char>& bytes)
{
  AppendLegacyPerAmpBlock(bytes);

  const int inactive = 0;
  const int emptyCapture = 0;
  const double zero = 0.0;
  const double five = 5.0;
  const double midFreq = 650.0;
  const double ratio = 4.0;
  const double attack = 10.0;
  const double release = 120.0;
  const double mix = 1.0;

  AppendBytes(bytes, inactive);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, ratio);
  AppendBytes(bytes, attack);
  AppendBytes(bytes, release);
  AppendBytes(bytes, mix);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, inactive);
  AppendBytes(bytes, emptyCapture);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, five);
  AppendBytes(bytes, five);
  AppendBytes(bytes, midFreq);
  AppendBytes(bytes, five);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, inactive);
  AppendBytes(bytes, emptyCapture);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, five);
  AppendBytes(bytes, five);
  AppendBytes(bytes, midFreq);
  AppendBytes(bytes, five);
  AppendBytes(bytes, zero);

  const int stackRoute = 0;
  const int emptySupportAmp = -1;
  const int defaultSpeaker = 3;
  const double supportGate = -80.0;
  const double minusSix = -6.0;
  const int active = 1;
  const int supportPolarityInvert = 0;
  AppendBytes(bytes, inactive);
  AppendBytes(bytes, stackRoute);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, emptySupportAmp);
  AppendBytes(bytes, defaultSpeaker);
  AppendBytes(bytes, inactive);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, supportGate);
  AppendBytes(bytes, five);
  AppendBytes(bytes, five);
  AppendBytes(bytes, five);
  AppendBytes(bytes, minusSix);
  AppendBytes(bytes, active);
  AppendBytes(bytes, active);
  AppendBytes(bytes, zero);
  AppendBytes(bytes, supportPolarityInvert);

  // POST per-amp tail: live values plus hidden delay/reverb/Oktaverb mode snapshots.
  const int postValid = 0;
  const int delayMode = 0;
  const int reverbMode = 0;
  const int reverbSubMode = 1;
  const double delayTime = 380.0;
  const double delayFeedback = 0.35;
  const double delayMix = 0.28;
  const double delayTone = 0.5;
  const double delayAge = 0.0;
  const double reverbMix = 0.20;
  const double reverbDecay = 2.5;
  const double reverbTone = 5.0;
  const double reverbPreDelay = 30.0;
  const double reverbShimmer = 0.0;
  AppendBytes(bytes, postValid);
  AppendBytes(bytes, inactive);     // postDelayActive
  AppendBytes(bytes, delayMode);
  AppendBytes(bytes, inactive);     // postDelayPingPong
  AppendBytes(bytes, inactive);     // postReverbActive
  AppendBytes(bytes, reverbMode);
  AppendBytes(bytes, reverbSubMode);
  AppendBytes(bytes, delayTime);
  AppendBytes(bytes, delayFeedback);
  AppendBytes(bytes, delayMix);
  AppendBytes(bytes, delayTone);
  AppendBytes(bytes, delayAge);
  AppendBytes(bytes, reverbMix);
  AppendBytes(bytes, reverbDecay);
  AppendBytes(bytes, reverbTone);
  AppendBytes(bytes, reverbPreDelay);
  AppendBytes(bytes, reverbShimmer);

  for (int i = 0; i < volum::kVoLumDelayModeCount; ++i)
  {
    AppendBytes(bytes, delayTime);
    AppendBytes(bytes, delayFeedback);
    AppendBytes(bytes, delayMix);
    AppendBytes(bytes, delayTone);
    AppendBytes(bytes, delayAge);
    AppendBytes(bytes, inactive); // postDelayModes[i].pingPong
  }
  for (int i = 0; i < volum::kVoLumReverbModeCount; ++i)
  {
    AppendBytes(bytes, reverbMix);
    AppendBytes(bytes, reverbDecay);
    AppendBytes(bytes, reverbTone);
    AppendBytes(bytes, reverbPreDelay);
    AppendBytes(bytes, reverbShimmer);
    AppendBytes(bytes, reverbSubMode);
  }
  for (int i = 0; i < 3; ++i)
  {
    AppendBytes(bytes, reverbMix);
    AppendBytes(bytes, reverbDecay);
    AppendBytes(bytes, reverbTone);
    AppendBytes(bytes, reverbPreDelay);
    AppendBytes(bytes, reverbShimmer);
  }
}
} // namespace

TEST_CASE("VoLum 0.1.x uses 0.7.15 serialized config branch")
{
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.1.0")));
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.1.99")));
}

TEST_CASE("NAM 0.7.15+ uses 0.7.15 branch")
{
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.7.15")));
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.8.0")));
}

TEST_CASE("NAM 0.7.14 does not use 0.7.15 branch")
{
  REQUIRE_FALSE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.7.14")));
}

TEST_CASE("Pre-VoLum 0.1.0 does not use 0.7.15 branch")
{
  REQUIRE_FALSE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.0.9")));
}

TEST_CASE("VoLum 0.5.x uses 0.5.0 serialized config branch")
{
  REQUIRE(volum::ChunkUses0500SerializedConfig(volum::ChunkVersion("0.5.0")));
  REQUIRE(volum::ChunkUses0500SerializedConfig(volum::ChunkVersion("0.5.99")));
  REQUIRE_FALSE(volum::ChunkUses0500SerializedConfig(volum::ChunkVersion("0.6.0")));
}

TEST_CASE("VoLum 0.6.x uses 0.6.0 serialized config branch")
{
  REQUIRE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.6.0")));
  REQUIRE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.6.99")));
  REQUIRE_FALSE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.7.0")));
}

TEST_CASE("VoLum 0.7.0-0.7.8 uses 0.7.0 serialized config branch")
{
  REQUIRE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.7.0")));
  REQUIRE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.7.8")));
  REQUIRE_FALSE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.7.9")));
  REQUIRE_FALSE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.6.0")));
}

TEST_CASE("VoLum 0.7.0 does not use 0.6.0 branch")
{
  REQUIRE_FALSE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.7.0")));
}

TEST_CASE("Oktaverb sub-mode remap applies before 0.9.1 only")
{
  CHECK(volum::ShouldRemapOktaverbSubModeForChunkVersion(volum::ChunkVersion("0.9.0")));
  CHECK_FALSE(volum::ShouldRemapOktaverbSubModeForChunkVersion(volum::ChunkVersion("0.9.1")));
  CHECK_FALSE(volum::ShouldRemapOktaverbSubModeForChunkVersion(volum::ChunkVersion("0.10.0")));
}

TEST_CASE("VoLum legacy per-amp chunk byte count is stable")
{
  std::vector<unsigned char> bytes;
  for (int i = 0; i < volum::kAmpCount; ++i)
    AppendLegacyPerAmpBlock(bytes);

  REQUIRE(bytes.size() == static_cast<size_t>(volum::LegacyPerAmpSettingsPayloadBytes(volum::kAmpCount)));
  CHECK_FALSE(volum::ChunkHasExtendedPerAmpSettings(static_cast<int>(bytes.size()), volum::kAmpCount));
}

TEST_CASE("VoLum current per-amp chunk byte count is stable")
{
  std::vector<unsigned char> bytes;
  for (int i = 0; i < volum::kAmpCount; ++i)
    AppendCurrentPerAmpBlock(bytes);

  REQUIRE(bytes.size() == static_cast<size_t>(volum::CurrentPerAmpSettingsPayloadBytes(volum::kAmpCount)));
  CHECK(volum::ChunkHasExtendedPerAmpSettings(static_cast<int>(bytes.size()), volum::kAmpCount));
  CHECK(volum::ChunkHasDualAmpPerAmpSettings(static_cast<int>(bytes.size()), volum::kAmpCount));
  CHECK(volum::ChunkHasPostPerAmpSettings(static_cast<int>(bytes.size()), volum::kAmpCount));
  CHECK(volum::ChunkHasPostSnapshotPerAmpSettings(static_cast<int>(bytes.size()), volum::kAmpCount));
}

TEST_CASE("Reverb mix equal-power remap matches expected values per mode")
{
  // Locks the v0.9.3 chunk migration: stored ReverbMix on pre-0.9.3 chunks must be
  // remapped through arcsin(...) so the new equal-power blend produces the same wet
  // contribution as the old additive blend at the same playing scene. Hall and Plate
  // also pick up kReverbWetTrim=1.55 in the wet bus, so their remap divides by 1.55
  // before the arcsin.
  constexpr double kHallPlateTrim = 1.55;
  constexpr double kHalfPi = 1.57079632679489661923;
  auto hallPlate = [&](double oldMix) {
    return std::asin(oldMix / kHallPlateTrim) / kHalfPi;
  };
  auto oktaverbCap = [&](double oldMix, double cap) {
    return std::asin(oldMix * cap) / (cap * kHalfPi);
  };

  // Hall (mode 0)
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.32, 0, 0) == doctest::Approx(hallPlate(0.32)));
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(1.0, 0, 0) == doctest::Approx(hallPlate(1.0)));
  // Plate (mode 1)
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.5, 1, 0) == doctest::Approx(hallPlate(0.5)));
  // Oktaverb Halo / Shimmer (mode 2, sub 0/1) - cap 0.5
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.4, 2, 0) == doctest::Approx(oktaverbCap(0.4, 0.5)));
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(1.0, 2, 1) == doctest::Approx(oktaverbCap(1.0, 0.5)));
  // Oktaverb Bloom (mode 2, sub 2) now shares the 0.5 cap with Halo/Shimmer
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.42, 2, 2) == doctest::Approx(oktaverbCap(0.42, 0.5)));
  // Edge cases
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.0, 0, 0) == doctest::Approx(0.0));
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.0, 2, 1) == doctest::Approx(0.0));
}

TEST_CASE("Reverb mix equal-power remap predicate")
{
  CHECK(volum::ShouldRemapReverbMixForChunkVersion(volum::ChunkVersion("0.9.0")));
  CHECK(volum::ShouldRemapReverbMixForChunkVersion(volum::ChunkVersion("0.9.2")));
  CHECK_FALSE(volum::ShouldRemapReverbMixForChunkVersion(volum::ChunkVersion("0.9.3")));
  CHECK_FALSE(volum::ShouldRemapReverbMixForChunkVersion(volum::ChunkVersion("0.10.0")));
}
