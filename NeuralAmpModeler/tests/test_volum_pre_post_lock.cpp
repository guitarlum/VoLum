#include "third_party/doctest.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumChunkCodec.h"
#include "../VoLumChunkLayout.h"
#include "../VoLumPrePostLock.h"
#include "../VoLumUserSettingsIO.h"

#include <cstring>
#include <vector>

namespace
{
class MemoryChunk
{
public:
  template<typename T>
  void Put(const T* value)
  {
    const auto* bytes = reinterpret_cast<const unsigned char*>(value);
    mBytes.insert(mBytes.end(), bytes, bytes + sizeof(T));
  }

  template<typename T>
  int Get(T* value, int pos) const
  {
    if (pos < 0 || pos + static_cast<int>(sizeof(T)) > static_cast<int>(mBytes.size()))
      return -1;
    std::memcpy(value, mBytes.data() + pos, sizeof(T));
    return pos + static_cast<int>(sizeof(T));
  }

  int Size() const { return static_cast<int>(mBytes.size()); }

private:
  std::vector<unsigned char> mBytes;
};

volum::VoLumAmpSettings MakePreSlot(double compAmount)
{
  volum::VoLumAmpSettings s;
  s.preCompAmount = compAmount;
  return s;
}

volum::VoLumAmpSettings MakePostSlot(double delayMix)
{
  volum::VoLumAmpSettings s;
  s.postDelayMix = delayMix;
  s.postValid = true;
  return s;
}

void CopyPreBlock(const volum::VoLumAmpSettings& src, volum::VoLumAmpSettings& dst)
{
  dst.preCompActive = src.preCompActive;
  dst.preCompAmount = src.preCompAmount;
  dst.preCompRatio = src.preCompRatio;
  dst.preCompAttack = src.preCompAttack;
  dst.preCompRelease = src.preCompRelease;
  dst.preCompMix = src.preCompMix;
  dst.preCompLevel = src.preCompLevel;
  dst.preNam1Active = src.preNam1Active;
  dst.preNam1Capture = src.preNam1Capture;
  dst.preNam1Gain = src.preNam1Gain;
  dst.preNam1Bass = src.preNam1Bass;
  dst.preNam1Mid = src.preNam1Mid;
  dst.preNam1MidFreq = src.preNam1MidFreq;
  dst.preNam1Treble = src.preNam1Treble;
  dst.preNam1Level = src.preNam1Level;
  dst.preNam2Active = src.preNam2Active;
  dst.preNam2Capture = src.preNam2Capture;
  dst.preNam2Gain = src.preNam2Gain;
  dst.preNam2Bass = src.preNam2Bass;
  dst.preNam2Mid = src.preNam2Mid;
  dst.preNam2MidFreq = src.preNam2MidFreq;
  dst.preNam2Treble = src.preNam2Treble;
  dst.preNam2Level = src.preNam2Level;
}

void CopyPostBlock(const volum::VoLumAmpSettings& src, volum::VoLumAmpSettings& dst)
{
  dst.postValid = src.postValid;
  dst.postDelayActive = src.postDelayActive;
  dst.postDelayTime = src.postDelayTime;
  dst.postDelayFeedback = src.postDelayFeedback;
  dst.postDelayMix = src.postDelayMix;
  dst.postDelayMode = src.postDelayMode;
  dst.postDelayTone = src.postDelayTone;
  dst.postDelayAge = src.postDelayAge;
  dst.postDelayPingPong = src.postDelayPingPong;
  dst.postReverbActive = src.postReverbActive;
  dst.postReverbMix = src.postReverbMix;
  dst.postReverbDecay = src.postReverbDecay;
  dst.postReverbTone = src.postReverbTone;
  dst.postReverbPreDelay = src.postReverbPreDelay;
  dst.postReverbShimmer = src.postReverbShimmer;
  dst.postReverbMode = src.postReverbMode;
  dst.postReverbSubMode = src.postReverbSubMode;
  for (int mode = 0; mode < volum::kVoLumDelayModeCount; ++mode)
    dst.postDelayModes[mode] = src.postDelayModes[mode];
  for (int mode = 0; mode < volum::kVoLumReverbModeCount; ++mode)
    dst.postReverbModes[mode] = src.postReverbModes[mode];
  for (int subMode = 0; subMode < 3; ++subMode)
    dst.postOktaverbSubModes[subMode] = src.postOktaverbSubModes[subMode];
}

// Mirrors _VolumSaveCurrentToSettings / _VolumRestoreFromSettings / lock helpers in VoLumSettings.inc.cpp.
struct PrePostLockSim
{
  volum::VoLumAmpSettings stored[volum::kAmpCount]{};
  volum::VoLumAmpSettings live{};
  int ampIdx = 0;
  bool preLocked = false;
  bool postLocked = false;
  bool preUiDirty = false;
  bool postUiDirty = false;

  bool IsPreDirty() const { return !volum::PreBlockEquals(live, stored[ampIdx]); }
  bool IsPostDirty() const { return !volum::PostBlockEquals(live, stored[ampIdx]); }

  void SaveCurrentToSettings()
  {
    if (!preLocked)
      CopyPreBlock(live, stored[ampIdx]);
    if (!postLocked)
      CopyPostBlock(live, stored[ampIdx]);
    stored[ampIdx].inputLevel = live.inputLevel;
    stored[ampIdx].dualAmpActive = live.dualAmpActive;
    stored[ampIdx].mainAmpPan = live.mainAmpPan;
  }

  void RestoreFromSettings(int idx)
  {
    ampIdx = idx;
    if (!preLocked)
      CopyPreBlock(stored[idx], live);
    if (!postLocked)
      CopyPostBlock(stored[idx], live);
  }

  void SetPreLocked(bool locked)
  {
    if (preLocked == locked)
      return;
    if (!locked)
      CopyPreBlock(stored[ampIdx], live);
    preLocked = locked;
    preUiDirty = locked && IsPreDirty();
  }

  void SetPostLocked(bool locked)
  {
    if (postLocked == locked)
      return;
    if (!locked)
      CopyPostBlock(stored[ampIdx], live);
    postLocked = locked;
    postUiDirty = locked && IsPostDirty();
  }

  void StorePreToCurrentAmp()
  {
    CopyPreBlock(live, stored[ampIdx]);
    preUiDirty = false;
  }

  void StorePostToCurrentAmp()
  {
    CopyPostBlock(live, stored[ampIdx]);
    postUiDirty = false;
  }

  void SwitchAmp(int newIdx)
  {
    SaveCurrentToSettings();
    RestoreFromSettings(newIdx);
    preUiDirty = preLocked && IsPreDirty();
    postUiDirty = postLocked && IsPostDirty();
  }
};
} // namespace

TEST_CASE("User settings v7 round-trips PRE/POST lock flags")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, true, true, true);

  REQUIRE(j["version"] == 7);
  REQUIRE(j["preLocked"] == true);
  REQUIRE(j["postLocked"] == true);

  bool preLocked = false;
  bool postLocked = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE(preLocked);
  REQUIRE(postLocked);
}

TEST_CASE("User settings v7 preserves independent PRE and POST lock combinations")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, true, true, false);

  bool preLocked = false;
  bool postLocked = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE(preLocked);
  REQUIRE_FALSE(postLocked);
}

TEST_CASE("User settings v6 defaults lock flags to false")
{
  nlohmann::json j;
  j["version"] = 6;
  j["lastAmpIdx"] = 0;
  j["amps"] = nlohmann::json::object();
  j["preLocked"] = true;
  j["postLocked"] = true;

  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  bool preLocked = true;
  bool postLocked = true;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE_FALSE(preLocked);
  REQUIRE_FALSE(postLocked);
}

TEST_CASE("User settings v7 missing lock keys leave caller defaults untouched")
{
  nlohmann::json j;
  j["version"] = 7;
  j["lastAmpIdx"] = 0;
  j["amps"] = nlohmann::json::object();

  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  bool preLocked = false;
  bool postLocked = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE_FALSE(preLocked);
  REQUIRE_FALSE(postLocked);
}

TEST_CASE("User settings v7 invalid lock types heal to unlocked")
{
  nlohmann::json j;
  j["version"] = 7;
  j["lastAmpIdx"] = 0;
  j["amps"] = nlohmann::json::object();
  j["preLocked"] = "yes";
  j["postLocked"] = 1;

  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  bool preLocked = true;
  bool postLocked = true;
  bool healed = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, &healed, &preLocked, &postLocked);
  REQUIRE(healed);
  REQUIRE_FALSE(preLocked);
  REQUIRE_FALSE(postLocked);
}

TEST_CASE("Chunk appends and reads PRE/POST lock flags after per-amp payload")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {0, 0, 0}, amps, volum::kAmpCount);
  volum::PutPrePostLockFlags(chunk, true, false);

  const int payloadBytes = volum::CurrentPerAmpSettingsPayloadBytes(volum::kAmpCount);
  REQUIRE(volum::ChunkHasPrePostLockFlags(payloadBytes + volum::kPrePostLockFlagsBytes, volum::kAmpCount));
  REQUIRE_FALSE(volum::ChunkHasPrePostLockFlags(payloadBytes, volum::kAmpCount));

  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    auto& s = amps[i];
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, s);
    pos = volum::GetExtendedPerAmpSettings(chunk, pos, s, true);
    pos = volum::GetDualAmpPerAmpSettings(chunk, pos, s, true);
    pos = volum::GetPostPerAmpSettings(chunk, pos, s);
  }

  bool preLocked = false;
  bool postLocked = true;
  pos = volum::GetPrePostLockFlags(chunk, pos, preLocked, postLocked);
  REQUIRE(preLocked);
  REQUIRE_FALSE(postLocked);
  REQUIRE(pos == chunk.Size());
}

TEST_CASE("Chunk lock flags round-trip all four combinations")
{
  const bool combos[4][2] = {{false, false}, {true, false}, {false, true}, {true, true}};
  for (const auto& combo : combos)
  {
    volum::VoLumAmpSettings amps[volum::kAmpCount]{};
    MemoryChunk chunk;
    volum::PutCurrentVoLumChunkState(chunk, {0, 0, 0}, amps, volum::kAmpCount);
    volum::PutPrePostLockFlags(chunk, combo[0], combo[1]);

    int pos = 0;
    volum::VoLumChunkSelection selection;
    pos = volum::GetVoLumChunkSelection(chunk, pos, selection);
    for (int i = 0; i < volum::kAmpCount; ++i)
    {
      auto& s = amps[i];
      pos = volum::GetLegacyPerAmpSettings(chunk, pos, s);
      pos = volum::GetExtendedPerAmpSettings(chunk, pos, s, true);
      pos = volum::GetDualAmpPerAmpSettings(chunk, pos, s, true);
      pos = volum::GetPostPerAmpSettings(chunk, pos, s);
    }

    bool preLocked = !combo[0];
    bool postLocked = !combo[1];
    pos = volum::GetPrePostLockFlags(chunk, pos, preLocked, postLocked);
    REQUIRE(preLocked == combo[0]);
    REQUIRE(postLocked == combo[1]);
    REQUIRE(pos == chunk.Size());
  }
}

TEST_CASE("Chunk round-trip preserves per-amp PRE slots while lock flags vary")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0] = MakePreSlot(1.0);
  amps[1] = MakePreSlot(9.0);

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {1, 0, 0}, amps, volum::kAmpCount);
  volum::PutPrePostLockFlags(chunk, true, true);

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    auto& s = loaded[i];
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, s);
    pos = volum::GetExtendedPerAmpSettings(chunk, pos, s, true);
    pos = volum::GetDualAmpPerAmpSettings(chunk, pos, s, true);
    pos = volum::GetPostPerAmpSettings(chunk, pos, s);
  }

  bool preLocked = false;
  bool postLocked = false;
  pos = volum::GetPrePostLockFlags(chunk, pos, preLocked, postLocked);

  REQUIRE(selection.ampIdx == 1);
  REQUIRE(preLocked);
  REQUIRE(postLocked);
  REQUIRE(loaded[0].preCompAmount == doctest::Approx(1.0));
  REQUIRE(loaded[1].preCompAmount == doctest::Approx(9.0));
  REQUIRE(pos == chunk.Size());
}

TEST_CASE("PRE save is gated while locked but amp-specific fields still persist")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.live = MakePreSlot(5.0);
  sim.live.inputLevel = 3.0;
  sim.preLocked = true;

  sim.SaveCurrentToSettings();

  REQUIRE(sim.stored[0].preCompAmount == doctest::Approx(1.0));
  REQUIRE(sim.stored[0].inputLevel == doctest::Approx(3.0));
}

TEST_CASE("POST save is gated while locked")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePostSlot(0.20);
  sim.live = MakePostSlot(0.75);
  sim.postLocked = true;

  sim.SaveCurrentToSettings();

  REQUIRE(sim.stored[0].postDelayMix == doctest::Approx(0.20));
}

TEST_CASE("Unlock restores local PRE without writing carried live scene to slot")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.live = MakePreSlot(5.0);
  sim.preLocked = true;

  sim.SetPreLocked(false);

  REQUIRE(sim.stored[0].preCompAmount == doctest::Approx(1.0));
  REQUIRE(sim.live.preCompAmount == doctest::Approx(1.0));
  REQUIRE_FALSE(sim.preUiDirty);
}

TEST_CASE("Unlock restores local POST without writing carried live scene to slot")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePostSlot(0.20);
  sim.live = MakePostSlot(0.75);
  sim.postLocked = true;

  sim.SetPostLocked(false);

  REQUIRE(sim.stored[0].postDelayMix == doctest::Approx(0.20));
  REQUIRE(sim.live.postDelayMix == doctest::Approx(0.20));
  REQUIRE_FALSE(sim.postUiDirty);
}

TEST_CASE("Store while locked updates only the current amp slot")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.stored[1] = MakePreSlot(9.0);
  sim.live = MakePreSlot(5.0);
  sim.ampIdx = 0;
  sim.preLocked = true;

  sim.StorePreToCurrentAmp();

  REQUIRE(sim.stored[0].preCompAmount == doctest::Approx(5.0));
  REQUIRE(sim.stored[1].preCompAmount == doctest::Approx(9.0));
  REQUIRE_FALSE(sim.preUiDirty);
}

TEST_CASE("Switching amps while PRE locked keeps the carried overlay live")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.stored[1] = MakePreSlot(9.0);
  sim.live = MakePreSlot(5.0);
  sim.preLocked = true;

  sim.SwitchAmp(1);

  REQUIRE(sim.ampIdx == 1);
  REQUIRE(sim.live.preCompAmount == doctest::Approx(5.0));
  REQUIRE(sim.stored[0].preCompAmount == doctest::Approx(1.0));
  REQUIRE(sim.stored[1].preCompAmount == doctest::Approx(9.0));
}

TEST_CASE("Switching amps while PRE locked marks dirty when target slot differs from overlay")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.stored[1] = MakePreSlot(9.0);
  sim.live = MakePreSlot(5.0);
  sim.preLocked = true;

  sim.SwitchAmp(1);

  REQUIRE(sim.IsPreDirty());
  REQUIRE(sim.preUiDirty);
}

TEST_CASE("Switching amps while PRE locked is clean when target slot already matches overlay")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.stored[1] = MakePreSlot(5.0);
  sim.live = MakePreSlot(5.0);
  sim.preLocked = true;

  sim.SwitchAmp(1);

  REQUIRE_FALSE(sim.IsPreDirty());
  REQUIRE_FALSE(sim.preUiDirty);
}

TEST_CASE("Switching amps while unlocked restores PRE from the target slot")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.stored[1] = MakePreSlot(9.0);
  sim.live = MakePreSlot(5.0);

  sim.SwitchAmp(1);

  REQUIRE(sim.ampIdx == 1);
  REQUIRE(sim.live.preCompAmount == doctest::Approx(9.0));
  REQUIRE(sim.stored[0].preCompAmount == doctest::Approx(5.0));
}

TEST_CASE("Switching amps while POST locked keeps the carried overlay live")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePostSlot(0.20);
  sim.stored[1] = MakePostSlot(0.90);
  sim.live = MakePostSlot(0.55);
  sim.postLocked = true;

  sim.SwitchAmp(1);

  REQUIRE(sim.live.postDelayMix == doctest::Approx(0.55));
  REQUIRE(sim.stored[1].postDelayMix == doctest::Approx(0.90));
  REQUIRE(sim.IsPostDirty());
}

TEST_CASE("Dual amp fields still save while PRE is locked")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.live = MakePreSlot(5.0);
  sim.live.dualAmpActive = true;
  sim.live.mainAmpPan = 0.75;
  sim.preLocked = true;

  sim.SaveCurrentToSettings();

  REQUIRE(sim.stored[0].preCompAmount == doctest::Approx(1.0));
  REQUIRE(sim.stored[0].dualAmpActive);
  REQUIRE(sim.stored[0].mainAmpPan == doctest::Approx(0.75));
}

TEST_CASE("Store POST while locked updates only the current amp slot")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePostSlot(0.20);
  sim.stored[1] = MakePostSlot(0.90);
  sim.live = MakePostSlot(0.55);
  sim.ampIdx = 1;
  sim.postLocked = true;

  sim.StorePostToCurrentAmp();

  REQUIRE(sim.stored[1].postDelayMix == doctest::Approx(0.55));
  REQUIRE(sim.stored[0].postDelayMix == doctest::Approx(0.20));
  REQUIRE_FALSE(sim.postUiDirty);
}

TEST_CASE("Locking PRE marks UI dirty when live scene differs from stored slot")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.live = MakePreSlot(5.0);

  sim.SetPreLocked(true);

  REQUIRE(sim.preLocked);
  REQUIRE(sim.IsPreDirty());
  REQUIRE(sim.preUiDirty);
}

TEST_CASE("Locking POST marks UI dirty when live scene differs from stored slot")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePostSlot(0.20);
  sim.live = MakePostSlot(0.75);

  sim.SetPostLocked(true);

  REQUIRE(sim.postLocked);
  REQUIRE(sim.IsPostDirty());
  REQUIRE(sim.postUiDirty);
}

TEST_CASE("Repeated lock toggle is a no-op once already locked")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.live = MakePreSlot(5.0);

  sim.SetPreLocked(true);
  sim.live.preCompAmount = 9.0;
  sim.SetPreLocked(true);

  REQUIRE(sim.live.preCompAmount == doctest::Approx(9.0));
  REQUIRE(sim.stored[0].preCompAmount == doctest::Approx(1.0));
}

TEST_CASE("PRE dirty compare ignores amp-specific fields")
{
  volum::VoLumAmpSettings stored = MakePreSlot(1.0);
  volum::VoLumAmpSettings live = MakePreSlot(1.0);
  live.inputLevel = 4.0;

  REQUIRE(volum::PreBlockEquals(live, stored));
}

TEST_CASE("POST dirty compare includes mode snapshots")
{
  volum::VoLumAmpSettings a;
  volum::VoLumAmpSettings b;
  a.postDelayModes[0].mix = 0.25;
  b.postDelayModes[0].mix = 0.30;

  REQUIRE(volum::PostBlockEquals(a, b) == false);
}

TEST_CASE("POST dirty compare includes oktaverb sub-mode snapshots")
{
  volum::VoLumAmpSettings a;
  volum::VoLumAmpSettings b;
  a.postOktaverbSubModes[1].decay = 2.0;
  b.postOktaverbSubModes[1].decay = 2.5;

  REQUIRE(volum::PostBlockEquals(a, b) == false);
}

TEST_CASE("PRE dirty compare detects NAM capture changes")
{
  volum::VoLumAmpSettings a = MakePreSlot(1.0);
  volum::VoLumAmpSettings b = MakePreSlot(1.0);
  b.preNam2Capture = 3;

  REQUIRE_FALSE(volum::PreBlockEquals(a, b));
}
