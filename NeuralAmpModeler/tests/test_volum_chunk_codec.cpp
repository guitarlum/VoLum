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
