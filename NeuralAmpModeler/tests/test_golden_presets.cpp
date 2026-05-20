#include "third_party/doctest.h"

#include "../VoLumChunkCodec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
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

  void PutBytes(const std::vector<unsigned char>& value)
  {
    bytes.insert(bytes.end(), value.begin(), value.end());
  }

  template<typename T>
  int Get(T* value, int pos) const
  {
    REQUIRE(pos >= 0);
    REQUIRE(static_cast<std::size_t>(pos) + sizeof(T) <= bytes.size());
    std::memcpy(value, bytes.data() + pos, sizeof(T));
    return pos + static_cast<int>(sizeof(T));
  }
};

std::filesystem::path PresetDir()
{
  return std::filesystem::path(__FILE__).parent_path() / "golden_presets";
}

bool UpdatePresets()
{
  return std::getenv("VOLUM_UPDATE_GOLDEN_PRESETS") != nullptr;
}

void PutU32(std::vector<unsigned char>& out, std::uint32_t value)
{
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<unsigned char>((value >> (i * 8)) & 0xffU));
}

void PutU64(std::vector<unsigned char>& out, std::uint64_t value)
{
  for (int i = 0; i < 8; ++i)
    out.push_back(static_cast<unsigned char>((value >> (i * 8)) & 0xffU));
}

std::uint32_t ReadU32(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  REQUIRE(offset + 4 <= bytes.size());
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i)
    value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8);
  return value;
}

std::uint64_t ReadU64(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  REQUIRE(offset + 8 <= bytes.size());
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i)
    value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8);
  return value;
}

std::vector<unsigned char> MakeCurrentVoLumChunk()
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  auto& s = amps[2];
  s.speakerIdx = 1;
  s.channelIdx = 3;
  s.inputLevel = -2.0;
  s.gateThreshold = -71.0;
  s.toneBass = 6.0;
  s.toneMid = 4.5;
  s.toneTreble = 7.0;
  s.outputLevel = -3.0;
  s.preCompActive = true;
  s.preCompAmount = 7.2;
  s.preCompAttack = 0.4;
  s.preCompRelease = 180.0;
  s.preCompMix = 0.73;
  s.preCompLevel = 1.25;
  s.preNam1Active = true;
  s.preNam1Capture = 4;
  s.dualAmpActive = true;
  s.dualAmpRoute = 1;
  s.mainAmpPan = -0.2;
  s.supportAmpIdx = 5;
  s.supportSpeakerIdx = 2;
  s.supportChannelIdx = 1;
  s.supportInputLevel = -1.0;
  s.supportAmpPan = 0.4;
  s.supportPolarityInvert = true;
  s.postValid = true;
  s.postDelayActive = true;
  s.postDelayTime = 420.0;
  s.postDelayFeedback = 0.44;
  s.postDelayMix = 0.38;
  s.postDelayMode = volum::kVoLumDelayModeAnalog;
  s.postDelayTone = 0.6;
  s.postDelayAge = 0.25;
  s.postDelayPingPong = true;
  s.postReverbActive = true;
  s.postReverbMix = 0.31;
  s.postReverbDecay = 6.0;
  s.postReverbTone = 5.2;
  s.postReverbPreDelay = 28.0;
  s.postReverbShimmer = 0.66;
  s.postReverbMode = volum::kVoLumReverbModeOktaverb;
  s.postReverbSubMode = volum::kVoLumOktaverbSubModeShimmer;

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {2, 1, 3}, amps, volum::kAmpCount);
  return chunk.bytes;
}

std::vector<unsigned char> WrapVstPresetComponentChunk(const std::vector<unsigned char>& componentChunk)
{
  std::vector<unsigned char> bytes;
  bytes.insert(bytes.end(), {'V', 'S', 'T', '3'});
  PutU32(bytes, 1);

  const char classId[] = "VoLum golden preset fixture";
  bytes.insert(bytes.end(), classId, classId + std::strlen(classId));
  bytes.resize(40, 0); // 4 magic + 4 version + 32-byte class id

  const std::uint64_t componentOffset = 48;
  const std::uint64_t listOffset = componentOffset + componentChunk.size();
  PutU64(bytes, listOffset);
  REQUIRE(bytes.size() == componentOffset);

  bytes.insert(bytes.end(), componentChunk.begin(), componentChunk.end());

  PutU32(bytes, 1); // one entry
  bytes.insert(bytes.end(), {'C', 'o', 'm', 'p'});
  PutU64(bytes, componentOffset);
  PutU64(bytes, componentChunk.size());
  return bytes;
}

std::vector<unsigned char> ReadFileBytes(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), {});
}

void WriteFileBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  REQUIRE(out.good());
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<unsigned char> ExtractComponentChunkFromVstPreset(const std::vector<unsigned char>& preset)
{
  REQUIRE(preset.size() >= 52);
  REQUIRE(std::memcmp(preset.data(), "VST3", 4) == 0);
  const auto listOffset = static_cast<std::size_t>(ReadU64(preset, 40));
  REQUIRE(listOffset + 4 <= preset.size());
  const auto count = ReadU32(preset, listOffset);
  std::size_t entry = listOffset + 4;
  for (std::uint32_t i = 0; i < count; ++i)
  {
    REQUIRE(entry + 20 <= preset.size());
    const bool isComponent = std::memcmp(preset.data() + entry, "Comp", 4) == 0;
    const auto offset = static_cast<std::size_t>(ReadU64(preset, entry + 4));
    const auto size = static_cast<std::size_t>(ReadU64(preset, entry + 12));
    REQUIRE(offset + size <= preset.size());
    if (isComponent)
      return std::vector<unsigned char>(preset.begin() + offset, preset.begin() + offset + size);
    entry += 20;
  }
  FAIL("VST preset did not contain a component-state chunk");
  return {};
}

void AssertCurrentChunkDecodes(const std::vector<unsigned char>& bytes)
{
  MemoryChunk chunk;
  chunk.PutBytes(bytes);

  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);
  CHECK(selection.ampIdx == 2);
  CHECK(selection.speakerIdx == 1);
  CHECK(selection.channelIdx == 3);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, loaded[i]);
    pos = volum::GetExtendedPerAmpSettings(chunk, pos, loaded[i], true);
    pos = volum::GetDualAmpPerAmpSettings(chunk, pos, loaded[i], true);
    pos = volum::GetPostPerAmpSettings(chunk, pos, loaded[i]);
  }

  const auto& s = loaded[2];
  CHECK(s.preCompActive);
  CHECK(s.preCompAmount == doctest::Approx(7.2));
  CHECK(s.preNam1Active);
  CHECK(s.preNam1Capture == 4);
  CHECK(s.dualAmpActive);
  CHECK(s.supportPolarityInvert);
  CHECK(s.postValid);
  CHECK(s.postDelayActive);
  CHECK(s.postDelayPingPong);
  CHECK(s.postReverbActive);
  CHECK(s.postReverbMode == volum::kVoLumReverbModeOktaverb);
  CHECK(s.postReverbSubMode == volum::kVoLumOktaverbSubModeShimmer);

  const double values[] = {s.inputLevel, s.gateThreshold, s.toneBass, s.toneMid, s.toneTreble,
                           s.outputLevel, s.preCompAmount, s.postDelayTime, s.postReverbMix};
  for (double value : values)
    CHECK(std::isfinite(value));
}
} // namespace

TEST_CASE("Golden presets: VST3 preset corpus component chunks decode")
{
  const auto presetPath = PresetDir() / "current-vo-lum-state.vstpreset";
  if (UpdatePresets())
    WriteFileBytes(presetPath, WrapVstPresetComponentChunk(MakeCurrentVoLumChunk()));

  REQUIRE(std::filesystem::exists(presetPath));
  const auto componentChunk = ExtractComponentChunkFromVstPreset(ReadFileBytes(presetPath));
  AssertCurrentChunkDecodes(componentChunk);
}
