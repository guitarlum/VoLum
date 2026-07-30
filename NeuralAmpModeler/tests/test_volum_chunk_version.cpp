#include "third_party/doctest.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumChunkLayout.h"
#include "../VoLumChunkVersion.h"
#include "../VoLumJsonMigration.h"

#include <cmath>

#include <vector>

namespace
{
template <typename T>
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
  const double delayTime = 320.0;
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
  AppendBytes(bytes, inactive); // postDelayActive
  AppendBytes(bytes, delayMode);
  AppendBytes(bytes, inactive); // postDelayPingPong
  AppendBytes(bytes, inactive); // postReverbActive
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
  CHECK_FALSE(volum::ChunkHasPrePostLockFlags(static_cast<int>(bytes.size()), volum::kAmpCount));
}

TEST_CASE("VoLum per-amp chunk plus lock flags byte count is stable")
{
  std::vector<unsigned char> bytes;
  for (int i = 0; i < volum::kAmpCount; ++i)
    AppendCurrentPerAmpBlock(bytes);

  const int withLocks = static_cast<int>(bytes.size()) + volum::kPrePostLockFlagsBytes;
  CHECK(volum::ChunkHasPrePostLockFlags(withLocks, volum::kAmpCount));
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
  auto hallPlate = [&](double oldMix) { return std::asin(oldMix / kHallPlateTrim) / kHalfPi; };
  auto oktaverbCap = [&](double oldMix, double cap) { return std::asin(oldMix * cap) / (cap * kHalfPi); };

  // Hall (mode 0)
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.32, 0, 0) == doctest::Approx(hallPlate(0.32)));
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(1.0, 0, 0) == doctest::Approx(hallPlate(1.0)));
  // Plate (mode 1)
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.5, 1, 0) == doctest::Approx(hallPlate(0.5)));
  // Oktaverb Halo / Shimmer (mode 2, sub 0/1) - cap 0.5
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.4, 2, 0) == doctest::Approx(oktaverbCap(0.4, 0.5)));
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(1.0, 2, 1) == doctest::Approx(oktaverbCap(1.0, 0.5)));
  // Oktaverb Bloom (mode 2, sub 2) now shares the 0.5 cap with Halo/Shimmer
  CHECK(volum::RemapReverbMixToEqualPowerV0_9_3(0.30, 2, 2) == doctest::Approx(oktaverbCap(0.30, 0.5)));
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

TEST_CASE("VoLum 1.x chunks use the 0.7.15 serialized config + extended per-amp tail")
{
  // Contract for the 1.2.0 BYO/preset id tail: the id tail is appended AFTER the
  // 0.7.15 extended per-amp tail and is version-agnostic, so 1.0.0 / 1.0.1 /
  // 1.1.0 / 1.2.0 chunks all flow through the same config + per-amp branches.
  for (const char* v : {"1.0.0", "1.0.1", "1.1.0", "1.2.0"})
  {
    CHECK(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion(v)));
    CHECK_FALSE(volum::ShouldRemapReverbMixForChunkVersion(volum::ChunkVersion(v)));
    CHECK_FALSE(volum::ShouldRemapOktaverbSubModeForChunkVersion(volum::ChunkVersion(v)));
  }

  // The extended/dual/post detectors are byte-count based and monotonic, so a
  // trailing id tail (extra bytes after the fixed tail) keeps every block that
  // SHOULD be present detected as present.
  std::vector<unsigned char> bytes;
  for (int i = 0; i < volum::kAmpCount; ++i)
    AppendCurrentPerAmpBlock(bytes);
  const int withIdTail = static_cast<int>(bytes.size()) + 64; // arbitrary id-tail bytes
  CHECK(volum::ChunkHasExtendedPerAmpSettings(withIdTail, volum::kAmpCount));
  CHECK(volum::ChunkHasDualAmpPerAmpSettings(withIdTail, volum::kAmpCount));
  CHECK(volum::ChunkHasPostPerAmpSettings(withIdTail, volum::kAmpCount));
  CHECK(volum::ChunkHasPostSnapshotPerAmpSettings(withIdTail, volum::kAmpCount));
}

// ---------------------------------------------------------------------------
// TryParseChunkVersion
//
// The state chunk's version string is corruption- and attacker-controlled, and
// UnserializeState is called across a C++ ABI boundary that hosts do not wrap in
// a try/catch. ChunkVersion(std::string) throws (std::stoi, plus an explicit
// throw on the segment count), so a project file with a valid VoLum header and a
// malformed version took the DAW down while opening the project instead of being
// rejected as bad state.
// ---------------------------------------------------------------------------

TEST_CASE("Every version string VoLum has ever serialized parses")
{
  volum::ChunkVersion v(0, 0, 0);

  REQUIRE(volum::TryParseChunkVersion("1.2.1", v));
  CHECK(v.GetMajor() == 1);
  CHECK(v.GetMinor() == 2);
  CHECK(v.GetPatch() == 1);

  // Oldest NAM chunks, and a two-digit patch component.
  REQUIRE(volum::TryParseChunkVersion("0.7.15", v));
  CHECK(v.GetMajor() == 0);
  CHECK(v.GetMinor() == 7);
  CHECK(v.GetPatch() == 15);

  REQUIRE(volum::TryParseChunkVersion("0.1.0", v));
  CHECK(v.GetMajor() == 0);
  CHECK(v.GetMinor() == 1);
  CHECK(v.GetPatch() == 0);

  // Leading zeros and multi-digit majors are still just digits.
  REQUIRE(volum::TryParseChunkVersion("10.20.30", v));
  CHECK(v.GetMajor() == 10);
  CHECK(v.GetMinor() == 20);
  CHECK(v.GetPatch() == 30);
}

TEST_CASE("The parser agrees with the throwing constructor on well-formed input")
{
  for (const char* text : {"0.0.0", "0.1.0", "0.7.15", "0.9.3", "1.0.0", "1.1.2", "1.2.0", "1.2.1"})
  {
    CAPTURE(text);
    const volum::ChunkVersion expected{std::string(text)};
    volum::ChunkVersion actual(0, 0, 0);
    REQUIRE(volum::TryParseChunkVersion(text, actual));
    CHECK(actual.GetMajor() == expected.GetMajor());
    CHECK(actual.GetMinor() == expected.GetMinor());
    CHECK(actual.GetPatch() == expected.GetPatch());
  }
}

TEST_CASE("Malformed versions are rejected without throwing")
{
  // Each of these threw out of ChunkVersion(std::string) before the fix: the
  // first group from the explicit segment-count throw, the rest from std::stoi.
  const char* malformed[] = {
    "", // header present, version absent
    "1",
    "1.2",
    "1.2.3.4", // wrong segment count
    "1.x.0",
    "x.2.1",
    "1.2.x", // non-numeric segment
    "1..0",
    "..",
    ".1.2",
    "1.2.", // empty segment
    "-1.2.3",
    "1.-2.3", // negative component
    "99999999999999999999.0.0", // out of int range
    "1.2.3-beta",
    "1.2.3 ",
    " 1.2.3",
    "v1.2.1", // trailing or leading junk
    "1,2,1", // decimal-comma locale mangling
  };

  for (const char* text : malformed)
  {
    CAPTURE(text);
    volum::ChunkVersion v(7, 7, 7);
    CHECK_FALSE(volum::TryParseChunkVersion(text, v));

    // A rejected parse must leave the caller's version untouched, so a failure
    // cannot be mistaken for "0.0.0" and routed into a legacy migration path.
    CHECK(v.GetMajor() == 7);
    CHECK(v.GetMinor() == 7);
    CHECK(v.GetPatch() == 7);
  }
}

TEST_CASE("Parsing arbitrary bytes never throws")
{
  // The version field can contain anything at all once a chunk is corrupt.
  std::string text;
  for (int b = 0; b < 256; ++b)
  {
    text.assign(3, static_cast<char>(b));
    volum::ChunkVersion v(0, 0, 0);
    CAPTURE(b);
    CHECK_NOTHROW(volum::TryParseChunkVersion(text, v));
  }
}
