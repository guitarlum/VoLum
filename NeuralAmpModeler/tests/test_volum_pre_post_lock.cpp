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

  // Mirrors plugin dirty compare while locked: overlay snapshot vs active slot.
  volum::VoLumAmpSettings liveLockedPre{};
  volum::VoLumAmpSettings liveLockedPost{};

  bool IsPreDirtyLocked() const
  {
    return preLocked && !volum::PreBlockEquals(liveLockedPre, stored[ampIdx]);
  }

  bool IsPostDirtyLocked() const
  {
    return postLocked && !volum::PostBlockEquals(liveLockedPost, stored[ampIdx]);
  }

  void SyncLockedSnapshotsFromLive()
  {
    if (preLocked)
      CopyPreBlock(live, liveLockedPre);
    if (postLocked)
      CopyPostBlock(live, liveLockedPost);
  }
};
} // namespace

TEST_CASE("User settings round-trips PRE/POST lock flags at current version")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, true, true, true);

  REQUIRE(j["version"] == volum::kVoLumUserSettingsVersion);
  REQUIRE(j["preLocked"] == true);
  REQUIRE(j["postLocked"] == true);

  bool preLocked = false;
  bool postLocked = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE(preLocked);
  REQUIRE(postLocked);
}

TEST_CASE("User settings preserves independent PRE and POST lock combinations")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, true, true, false);

  bool preLocked = false;
  bool postLocked = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE(preLocked);
  REQUIRE_FALSE(postLocked);
}

TEST_CASE("User settings v6 with lock flags is read as locked (no version gate)")
{
  nlohmann::json j;
  j["version"] = 6;
  j["lastAmpIdx"] = 0;
  j["amps"] = nlohmann::json::object();
  j["preLocked"] = true;
  j["postLocked"] = true;

  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  bool preLocked = false;
  bool postLocked = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE(preLocked);
  REQUIRE(postLocked);
}

TEST_CASE("User settings missing lock keys leave caller defaults untouched")
{
  nlohmann::json j;
  j["version"] = volum::kVoLumUserSettingsVersion;
  j["lastAmpIdx"] = 0;
  j["amps"] = nlohmann::json::object();

  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  bool preLocked = false;
  bool postLocked = false;
  volum::VolumUserSettingsFromJson(j, amps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked);
  REQUIRE_FALSE(preLocked);
  REQUIRE_FALSE(postLocked);
}

TEST_CASE("User settings invalid lock types heal to unlocked")
{
  nlohmann::json j;
  j["version"] = volum::kVoLumUserSettingsVersion;
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

TEST_CASE("PRE lock reload on B then switch back to origin A clears locked dirty")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePreSlot(1.0);
  sim.stored[1] = MakePreSlot(9.0);
  sim.live = MakePreSlot(1.0);
  sim.SetPreLocked(true);
  sim.SyncLockedSnapshotsFromLive();
  sim.SwitchAmp(1);
  REQUIRE(sim.IsPreDirtyLocked());

  const nlohmann::json j = volum::VolumUserSettingsToJson(sim.stored, volum::kAmpCount, sim.ampIdx, nullptr, true,
                                                          sim.preLocked, sim.postLocked, &sim.liveLockedPre, nullptr);

  PrePostLockSim reloaded;
  bool preLocked = false;
  bool haveLivePre = false;
  volum::VolumUserSettingsFromJson(j, reloaded.stored, volum::kAmpCount, &reloaded.ampIdx, nullptr, nullptr, &preLocked,
                                   nullptr, &reloaded.liveLockedPre, nullptr, &haveLivePre, nullptr);
  reloaded.preLocked = preLocked;
  reloaded.live = reloaded.liveLockedPre;
  REQUIRE(haveLivePre);
  REQUIRE(reloaded.ampIdx == 1);
  REQUIRE(reloaded.IsPreDirtyLocked());

  reloaded.SwitchAmp(0);
  REQUIRE_FALSE(reloaded.IsPreDirtyLocked());
}

TEST_CASE("POST lock reload on B then switch back to origin A clears locked dirty")
{
  PrePostLockSim sim;
  sim.stored[0] = MakePostSlot(0.20);
  sim.stored[1] = MakePostSlot(0.90);
  sim.live = MakePostSlot(0.20);
  sim.SetPostLocked(true);
  sim.SyncLockedSnapshotsFromLive();
  sim.SwitchAmp(1);
  REQUIRE(sim.IsPostDirtyLocked());

  const nlohmann::json j =
    volum::VolumUserSettingsToJson(sim.stored, volum::kAmpCount, sim.ampIdx, nullptr, true, sim.preLocked,
                                   sim.postLocked, nullptr, &sim.liveLockedPost);

  PrePostLockSim reloaded;
  bool postLocked = false;
  bool haveLivePost = false;
  volum::VolumUserSettingsFromJson(j, reloaded.stored, volum::kAmpCount, &reloaded.ampIdx, nullptr, nullptr, nullptr,
                                   &postLocked, nullptr, &reloaded.liveLockedPost, nullptr, &haveLivePost);
  reloaded.postLocked = postLocked;
  reloaded.live = reloaded.liveLockedPost;
  REQUIRE(haveLivePost);
  REQUIRE(reloaded.ampIdx == 1);
  REQUIRE(reloaded.IsPostDirtyLocked());

  reloaded.SwitchAmp(0);
  REQUIRE_FALSE(reloaded.IsPostDirtyLocked());
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

// -------------------------------------------------------------------------
// Live lock snapshot: persistence + restore lifecycle (B-Lock-1 regression)
// -------------------------------------------------------------------------
//
// Bug: locking PRE/POST, switching amp, then closing the app would lose the
// locked live values. Root cause: `_VolumSaveCurrentToSettings` skips writing
// PRE/POST into the per-amp slot when the corresponding lock is engaged, but
// nothing else persisted the live values. Fix: persist live PRE/POST in a
// dedicated `liveLockedPre` / `liveLockedPost` JSON snapshot and restore from
// it on load, WITHOUT touching any per-amp slot.

TEST_CASE("Live PRE lock snapshot round-trips through JSON without touching slots")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0] = MakePreSlot(1.0); // ampIdx 0 stored PRE comp amount = 1.0
  amps[1] = MakePreSlot(9.0); // ampIdx 1 stored PRE comp amount = 9.0
  // Snapshot of the live (locked) PRE the user was hearing on amp 1.
  // Note: comp amount = 4.2 is DIFFERENT from either stored slot - this is
  // exactly the "dirty locked PRE while on amp 1" state that the bug lost.
  volum::VoLumAmpSettings live{};
  live.preCompAmount = 4.2;
  live.preCompMix = 0.6;
  live.preNam1Active = true;
  live.preNam1Gain = -3.0;

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, /*lastAmpIdx=*/1,
                                                          /*fx=*/nullptr, /*includeDualAmp=*/true,
                                                          /*preLocked=*/true, /*postLocked=*/false,
                                                          /*liveLockedPre=*/&live, /*liveLockedPost=*/nullptr);
  REQUIRE(j.contains("liveLockedPre"));
  REQUIRE_FALSE(j.contains("liveLockedPost"));

  // Per-amp slots must remain at their stored values - locked save must NOT
  // mutate either amp's PRE.
  REQUIRE(j["amps"][volum::kAmps[0].folderName]["preCompAmount"].get<double>()
          == doctest::Approx(amps[0].preCompAmount));
  REQUIRE(j["amps"][volum::kAmps[1].folderName]["preCompAmount"].get<double>()
          == doctest::Approx(amps[1].preCompAmount));

  // Read it back.
  volum::VoLumAmpSettings loadedAmps[volum::kAmpCount]{};
  volum::VoLumAmpSettings loadedLivePre{};
  volum::VoLumAmpSettings loadedLivePost{};
  bool preLocked = false;
  bool postLocked = false;
  bool haveLivePre = false;
  bool haveLivePost = false;
  int lastAmp = -1;
  volum::VolumUserSettingsFromJson(j, loadedAmps, volum::kAmpCount, &lastAmp, nullptr, nullptr, &preLocked, &postLocked,
                                   &loadedLivePre, &loadedLivePost, &haveLivePre, &haveLivePost);

  REQUIRE(preLocked);
  REQUIRE_FALSE(postLocked);
  REQUIRE(haveLivePre);
  REQUIRE_FALSE(haveLivePost);
  REQUIRE(loadedLivePre.preCompAmount == doctest::Approx(4.2));
  REQUIRE(loadedLivePre.preCompMix == doctest::Approx(0.6));
  REQUIRE(loadedLivePre.preNam1Active);
  REQUIRE(loadedLivePre.preNam1Gain == doctest::Approx(-3.0));
  // Round-trip preserves per-amp slots unchanged.
  REQUIRE(loadedAmps[0].preCompAmount == doctest::Approx(1.0));
  REQUIRE(loadedAmps[1].preCompAmount == doctest::Approx(9.0));
}

TEST_CASE("Live POST lock snapshot round-trips through JSON without touching slots")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0] = MakePostSlot(0.10);
  amps[1] = MakePostSlot(0.90);
  volum::VoLumAmpSettings live{};
  live.postValid = true;
  live.postDelayActive = true;
  live.postDelayMix = 0.42;
  live.postReverbActive = true;
  live.postReverbDecay = 5.5;
  live.postReverbMode = 1;

  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, /*lastAmpIdx=*/1,
                                                          /*fx=*/nullptr, /*includeDualAmp=*/true,
                                                          /*preLocked=*/false, /*postLocked=*/true,
                                                          /*liveLockedPre=*/nullptr, /*liveLockedPost=*/&live);
  REQUIRE_FALSE(j.contains("liveLockedPre"));
  REQUIRE(j.contains("liveLockedPost"));
  REQUIRE(j["amps"][volum::kAmps[0].folderName]["postDelayMix"].get<double>() == doctest::Approx(0.10));
  REQUIRE(j["amps"][volum::kAmps[1].folderName]["postDelayMix"].get<double>() == doctest::Approx(0.90));

  volum::VoLumAmpSettings loadedAmps[volum::kAmpCount]{};
  volum::VoLumAmpSettings loadedLivePre{};
  volum::VoLumAmpSettings loadedLivePost{};
  bool preLocked = false;
  bool postLocked = false;
  bool haveLivePre = false;
  bool haveLivePost = false;
  volum::VolumUserSettingsFromJson(j, loadedAmps, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked,
                                   &loadedLivePre, &loadedLivePost, &haveLivePre, &haveLivePost);

  REQUIRE_FALSE(preLocked);
  REQUIRE(postLocked);
  REQUIRE_FALSE(haveLivePre);
  REQUIRE(haveLivePost);
  REQUIRE(loadedLivePost.postDelayActive);
  REQUIRE(loadedLivePost.postDelayMix == doctest::Approx(0.42));
  REQUIRE(loadedLivePost.postReverbActive);
  REQUIRE(loadedLivePost.postReverbDecay == doctest::Approx(5.5));
  REQUIRE(loadedLivePost.postReverbMode == 1);
  REQUIRE(loadedAmps[0].postDelayMix == doctest::Approx(0.10));
  REQUIRE(loadedAmps[1].postDelayMix == doctest::Approx(0.90));
}

TEST_CASE("Unlocked settings save omits live lock snapshot keys")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumAmpSettings live{};
  live.preCompAmount = 5.5;
  // Lock flags off -> snapshots are stale and must not be written.
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, true,
                                                          /*preLocked=*/false, /*postLocked=*/false, &live, &live);
  REQUIRE_FALSE(j.contains("liveLockedPre"));
  REQUIRE_FALSE(j.contains("liveLockedPost"));
}

// -------------------------------------------------------------------------
// Chunk-path lock lifecycle (B-Lock-1, chunk variant)
// -------------------------------------------------------------------------

TEST_CASE("Chunk live lock snapshot round-trips and preserves per-amp slots")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0] = MakePreSlot(2.0);
  amps[1] = MakePreSlot(7.0);
  volum::VoLumAmpSettings livePre{};
  livePre.preCompAmount = 3.3;
  livePre.preCompMix = 0.75;
  livePre.preNam1Active = true;
  livePre.preNam1Capture = 2;
  livePre.preNam1Gain = -1.5;
  volum::VoLumAmpSettings livePost{};
  livePost.postValid = true;
  livePost.postDelayActive = true;
  livePost.postDelayMix = 0.55;

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {1, 0, 0}, amps, volum::kAmpCount);
  volum::PutPrePostLockFlags(chunk, true, true);
  volum::PutPrePostLockSnapshots(chunk, true, true, livePre, livePost);

  const int payloadBytes = volum::CurrentPerAmpSettingsPayloadBytes(volum::kAmpCount);
  // Detector recognizes flags+snapshots tail and rejects shorter tails.
  REQUIRE(volum::ChunkHasPrePostLockSnapshots(chunk.Size(), volum::kAmpCount, true, true));
  REQUIRE_FALSE(volum::ChunkHasPrePostLockSnapshots(payloadBytes + volum::kPrePostLockFlagsBytes, volum::kAmpCount, true,
                                                     true));

  // Read it back.
  volum::VoLumAmpSettings loadedAmps[volum::kAmpCount]{};
  volum::VoLumChunkSelection selection;
  int pos = volum::GetVoLumChunkSelection(chunk, 0, selection);
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    auto& s = loadedAmps[i];
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, s);
    pos = volum::GetExtendedPerAmpSettings(chunk, pos, s, true);
    pos = volum::GetDualAmpPerAmpSettings(chunk, pos, s, true);
    pos = volum::GetPostPerAmpSettings(chunk, pos, s);
  }
  bool preLocked = false;
  bool postLocked = false;
  pos = volum::GetPrePostLockFlags(chunk, pos, preLocked, postLocked);
  REQUIRE(preLocked);
  REQUIRE(postLocked);

  volum::VoLumAmpSettings loadedLivePre{};
  volum::VoLumAmpSettings loadedLivePost{};
  pos = volum::GetPrePostLockSnapshots(chunk, pos, preLocked, postLocked, loadedLivePre, loadedLivePost);
  REQUIRE(pos == chunk.Size());

  // Per-amp slots preserved.
  REQUIRE(loadedAmps[0].preCompAmount == doctest::Approx(2.0));
  REQUIRE(loadedAmps[1].preCompAmount == doctest::Approx(7.0));
  // Snapshot exactly restored.
  REQUIRE(loadedLivePre.preCompAmount == doctest::Approx(3.3));
  REQUIRE(loadedLivePre.preCompMix == doctest::Approx(0.75));
  REQUIRE(loadedLivePre.preNam1Active);
  REQUIRE(loadedLivePre.preNam1Capture == 2);
  REQUIRE(loadedLivePre.preNam1Gain == doctest::Approx(-1.5));
  REQUIRE(loadedLivePost.postDelayActive);
  REQUIRE(loadedLivePost.postDelayMix == doctest::Approx(0.55));
}

TEST_CASE("Chunk lock snapshot is omitted for blocks whose lock is off")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::VoLumAmpSettings livePre{};
  livePre.preCompAmount = 1.1;
  volum::VoLumAmpSettings livePost{};
  livePost.postValid = true;
  livePost.postDelayMix = 0.22;

  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {0, 0, 0}, amps, volum::kAmpCount);
  // PRE locked only.
  volum::PutPrePostLockFlags(chunk, true, false);
  volum::PutPrePostLockSnapshots(chunk, true, false, livePre, livePost);

  // Chunk layout: 3-int selection header + per-amp payload + lock flags + snapshot(s).
  const int basePlusFlags = volum::kPerAmpSettingsHeaderBytes
                            + volum::CurrentPerAmpSettingsPayloadBytes(volum::kAmpCount)
                            + volum::kPrePostLockFlagsBytes;
  REQUIRE(chunk.Size() == basePlusFlags + volum::kPreLockSnapshotBytes);
  // POST snapshot must be absent: chunk size MUST NOT include kPostLockSnapshotBytes.
  REQUIRE_FALSE(chunk.Size() == basePlusFlags + volum::kPreLockSnapshotBytes + volum::kPostLockSnapshotBytes);
}

TEST_CASE("Chunk without lock snapshots is detected as old format (back-compat)")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  MemoryChunk chunk;
  volum::PutCurrentVoLumChunkState(chunk, {0, 0, 0}, amps, volum::kAmpCount);
  // Lock flags only, no snapshots (old 1.0.1-rc format).
  volum::PutPrePostLockFlags(chunk, true, true);

  REQUIRE(volum::ChunkHasPrePostLockFlags(chunk.Size(), volum::kAmpCount));
  // Detector says snapshots are NOT present even though flags claim both locked.
  REQUIRE_FALSE(volum::ChunkHasPrePostLockSnapshots(chunk.Size(), volum::kAmpCount, true, true));
}

// -------------------------------------------------------------------------
// Settings version forward-compat (B-Compat-1)
// -------------------------------------------------------------------------

TEST_CASE("Settings reader is forward-tolerant: future version does not nuke data")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].toneBass = 7.5;
  amps[0].toneTreble = 8.5;
  amps[0].outputLevel = -4.0;

  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  // Pretend a newer build wrote the file with a higher version + an unknown key.
  j["version"] = 999;
  j["someFutureField"] = "ignore me";

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  int lastAmp = -1;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, &lastAmp, nullptr, &healed);

  REQUIRE_FALSE(healed); // Future version must NOT mark healed (no destructive migration).
  REQUIRE(loaded[0].toneBass == doctest::Approx(7.5));
  REQUIRE(loaded[0].toneTreble == doctest::Approx(8.5));
  REQUIRE(loaded[0].outputLevel == doctest::Approx(-4.0));
}

TEST_CASE("Settings v6 with additive lock keys loads cleanly without healing")
{
  // Simulates VoLum 1.0.0 reading a 1.0.1 settings file: version stayed at 6
  // because lock fields are purely additive.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[0].inputLevel = 2.5;
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["preLocked"] = true;
  j["postLocked"] = false;
  j["liveLockedPre"] = volum::PreBlockToJson(MakePreSlot(3.7));

  REQUIRE(j["version"] == 6); // Confirms we did not silently bump it.

  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  bool healed = false;
  bool preLocked = false;
  bool postLocked = true;
  int lastAmp = 0;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, &lastAmp, nullptr, &healed, &preLocked, &postLocked);
  REQUIRE_FALSE(healed);
  REQUIRE(preLocked);
  REQUIRE_FALSE(postLocked);
  REQUIRE(loaded[0].inputLevel == doctest::Approx(2.5));
}

TEST_CASE("Settings reader heals when version is non-integer garbage")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = "garbage";

  bool healed = false;
  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);
  REQUIRE(healed);
}

TEST_CASE("Settings reader heals when version is below 1")
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0);
  j["version"] = 0;

  bool healed = false;
  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, &healed);
  REQUIRE(healed);
}

// -------------------------------------------------------------------------
// Slot-non-mutation invariant
// -------------------------------------------------------------------------

TEST_CASE("Live lock snapshot save/load never modifies per-amp slot bytes")
{
  // Build initial per-amp state with distinct PRE/POST values per amp so any
  // accidental cross-pollination from the lock snapshot logic shows up.
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    amps[i].toneBass = 1.0 + i * 0.1;
    amps[i].toneMid = 2.0 + i * 0.1;
    amps[i].toneTreble = 3.0 + i * 0.1;
    amps[i].preCompAmount = 4.0 + i * 0.1;
    amps[i].preNam1Gain = -1.0 - i * 0.1;
    amps[i].postValid = true;
    amps[i].postDelayMix = 0.10 + i * 0.05;
    amps[i].postReverbDecay = 1.5 + i * 0.25;
  }

  // Compare snapshots before/after a full save+load roundtrip with the lock on
  // and a clearly different live snapshot.
  volum::VoLumAmpSettings expected[volum::kAmpCount];
  for (int i = 0; i < volum::kAmpCount; ++i)
    expected[i] = amps[i];

  volum::VoLumAmpSettings livePre = MakePreSlot(8.8);
  volum::VoLumAmpSettings livePost = MakePostSlot(0.99);
  const nlohmann::json j = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, 0, nullptr, true, true, true, &livePre,
                                                          &livePost);
  // Reload into a fresh array.
  volum::VoLumAmpSettings loaded[volum::kAmpCount]{};
  volum::VoLumAmpSettings loadedLivePre{};
  volum::VoLumAmpSettings loadedLivePost{};
  bool preLocked = false;
  bool postLocked = false;
  bool haveLivePre = false;
  bool haveLivePost = false;
  volum::VolumUserSettingsFromJson(j, loaded, volum::kAmpCount, nullptr, nullptr, nullptr, &preLocked, &postLocked,
                                   &loadedLivePre, &loadedLivePost, &haveLivePre, &haveLivePost);

  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    REQUIRE(loaded[i].toneBass == doctest::Approx(expected[i].toneBass));
    REQUIRE(loaded[i].toneMid == doctest::Approx(expected[i].toneMid));
    REQUIRE(loaded[i].toneTreble == doctest::Approx(expected[i].toneTreble));
    REQUIRE(loaded[i].preCompAmount == doctest::Approx(expected[i].preCompAmount));
    REQUIRE(loaded[i].preNam1Gain == doctest::Approx(expected[i].preNam1Gain));
    REQUIRE(loaded[i].postDelayMix == doctest::Approx(expected[i].postDelayMix));
    REQUIRE(loaded[i].postReverbDecay == doctest::Approx(expected[i].postReverbDecay));
  }
  REQUIRE(haveLivePre);
  REQUIRE(haveLivePost);
  REQUIRE(loadedLivePre.preCompAmount == doctest::Approx(8.8));
  REQUIRE(loadedLivePost.postDelayMix == doctest::Approx(0.99));
}
