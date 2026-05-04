#include "third_party/doctest.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumChunkLayout.h"
#include "../VoLumChunkVersion.h"

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
}
