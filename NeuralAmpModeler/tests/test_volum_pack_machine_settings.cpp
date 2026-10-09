#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "../VoLumAmpSettingsJson.h"
#include "../VoLumPackMachineSettings.h"
#include "../VoLumUserSettingsIO.h"
#include "../VoLumWriteDebounce.h"

using volum::LiveSceneGate;
using volum::VoLumAmpSettings;
using volum::pack::DualAmpSidecarFromSettings;
using volum::pack::SettingsWithDualAmp;

namespace
{
constexpr int kAmpeteOne = 0;
constexpr int kSoldano = 13;
constexpr int kThcSunset = 14;

// The standalone's settings state as _VolumLoadSettingsFromFile,
// _VolumSaveCurrentToSettings, _VolumSaveSettingsToFile and OnIdle move it.
// `live` stands for the live params; the two strings are the two files on disk.
struct Standalone
{
  VoLumAmpSettings amps[volum::kAmpCount]{};
  int ampIdx = 0;
  VoLumAmpSettings live;
  LiveSceneGate gate;
  volum::WriteDebounce debounce;
  std::string mainFile;
  std::string sidecarFile;

  void SnapshotLive()
  {
    if (!gate.Allows())
      return;
    amps[ampIdx] = live;
  }

  // In PLAY the load refreshes the surface, and its dirty check snapshots live
  // into the active scene after the main file has moved ampIdx and the scenes.
  void LoadSettingsFiles(bool playMode)
  {
    volum::VolumUserSettingsFromJson(nlohmann::json::parse(mainFile), amps, volum::kAmpCount, &ampIdx);
    if (playMode)
      SnapshotLive();
    if (!sidecarFile.empty())
      volum::VolumUserSettingsFromJson(nlohmann::json::parse(sidecarFile), amps, volum::kAmpCount, nullptr);
  }

  void ApplyScene() { live = amps[ampIdx]; }

  void SaveSettingsFiles()
  {
    mainFile = volum::VolumUserSettingsToJson(amps, volum::kAmpCount, ampIdx, nullptr, /*includeDualAmp=*/false).dump();
    sidecarFile = volum::VolumDualAmpUserSettingsToJson(amps, volum::kAmpCount).dump();
  }

  void IdleTick(double nowMs)
  {
    SnapshotLive();
    if (debounce.shouldWrite(nowMs))
    {
      debounce.markWritten(nowMs);
      SaveSettingsFiles();
    }
  }

  // _VolumExportPack: flush, read the main file, fold the sidecar in.
  std::string ExportEverythingSettings()
  {
    SnapshotLive();
    SaveSettingsFiles();
    return SettingsWithDualAmp(mainFile, volum::VolumDualAmpUserSettingsToJson(amps, volum::kAmpCount));
  }

  // _VolumImportPack with "Also restore machine settings": ApplyPack writes the
  // Pack's document as the main file, then the restore runs in this order.
  void ImportEverythingWithSettings(const std::string& packSettings, bool playMode)
  {
    mainFile = packSettings;
    nlohmann::json sidecar;
    if (DualAmpSidecarFromSettings(packSettings, sidecar))
      sidecarFile = sidecar.dump();
    {
      LiveSceneGate::Hold restoring(gate);
      LoadSettingsFiles(playMode);
      ApplyScene();
    }
    debounce.dirty(0.0); // _VolumSelectFactoryAmp sets mVolumSettingsDirty
  }
};

VoLumAmpSettings AmpeteLeadScene()
{
  VoLumAmpSettings s;
  s.channelIdx = 1;
  s.preNam1Active = true;
  s.preNam1Capture = 2;
  s.preNam2Active = false;
  s.postDelayActive = true;
  s.postReverbActive = true;
  s.dualAmpActive = true;
  s.supportAmpIdx = kSoldano;
  s.supportChannelIdx = 2;
  s.supportOutputLevel = -40.0;
  return s;
}

// The outgoing rig from the bug report: THC Sunset, Klon at 2.0, Halcyon at 1.5.
VoLumAmpSettings ThcSunsetLiveScene()
{
  VoLumAmpSettings s;
  s.channelIdx = 2;
  s.preNam1Active = true;
  s.preNam1Capture = 1;
  s.preNam1Gain = 2.0;
  s.preNam2Active = true;
  s.preNam2Capture = 3;
  s.preNam2Gain = 1.5;
  s.postDelayActive = false;
  return s;
}

void SeedSender(Standalone& sender)
{
  sender.ampIdx = kAmpeteOne;
  sender.amps[kAmpeteOne] = AmpeteLeadScene();
  sender.live = sender.amps[kAmpeteOne];
}

void SeedReceiver(Standalone& receiver)
{
  receiver.ampIdx = kThcSunset;
  receiver.amps[kThcSunset] = ThcSunsetLiveScene();
  receiver.live = receiver.amps[kThcSunset];
  receiver.SaveSettingsFiles();
  // An unsaved knob edit on the outgoing rig: the debounced write is still pending.
  receiver.live.preNam1Gain = 2.5;
  receiver.debounce.dirty(0.0);
}

std::string ReadSource(const char* leaf)
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / leaf;
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string FunctionBody(const std::string& source, const char* signature)
{
  const auto begin = source.find(signature);
  REQUIRE(begin != std::string::npos);
  const auto end = source.find("\n}", begin);
  REQUIRE(end != std::string::npos);
  return source.substr(begin, end - begin);
}
} // namespace

TEST_CASE("Everything import with machine settings persists the Pack's last amp and Dual partner (F-88)")
{
  Standalone sender;
  SeedSender(sender);
  const std::string packSettings = sender.ExportEverythingSettings();

  Standalone receiver;
  SeedReceiver(receiver);
  receiver.ImportEverythingWithSettings(packSettings, /*playMode=*/true);

  CHECK(receiver.ampIdx == kAmpeteOne);
  CHECK(volum::AmpSettingsEqual(receiver.live, AmpeteLeadScene()));

  // The pending debounced save and the idle snapshots run after the restore.
  receiver.IdleTick(100.0);
  receiver.IdleTick(600.0);
  REQUIRE_FALSE(receiver.debounce.isDirty());

  Standalone relaunch;
  relaunch.mainFile = receiver.mainFile;
  relaunch.sidecarFile = receiver.sidecarFile;
  relaunch.LoadSettingsFiles(/*playMode=*/false);
  CHECK(relaunch.ampIdx == kAmpeteOne);
  const VoLumAmpSettings& restored = relaunch.amps[kAmpeteOne];
  CHECK(restored.channelIdx == 1);
  CHECK(restored.preNam1Capture == 2);
  CHECK(restored.preNam1Gain == doctest::Approx(0.0));
  CHECK_FALSE(restored.preNam2Active);
  CHECK(restored.postDelayActive);
  CHECK(restored.dualAmpActive);
  CHECK(restored.supportAmpIdx == kSoldano);
  CHECK(volum::AmpSettingsEqual(restored, AmpeteLeadScene()));
  // The outgoing amp's own slot comes from the Pack too, not from the live edit.
  CHECK(volum::AmpSettingsEqual(relaunch.amps[kThcSunset], VoLumAmpSettings{}));
}

TEST_CASE("A Pack from a build that did not carry Dual state keeps the receiver's own sidecar")
{
  Standalone sender;
  SeedSender(sender);
  sender.SaveSettingsFiles();
  const std::string olderPackSettings = sender.mainFile;

  Standalone receiver;
  SeedReceiver(receiver);
  receiver.amps[kAmpeteOne].dualAmpActive = true;
  receiver.amps[kAmpeteOne].supportAmpIdx = 4;
  receiver.SaveSettingsFiles();
  receiver.ImportEverythingWithSettings(olderPackSettings, /*playMode=*/true);

  CHECK(receiver.ampIdx == kAmpeteOne);
  CHECK(receiver.live.channelIdx == 1);
  CHECK(receiver.live.dualAmpActive);
  CHECK(receiver.live.supportAmpIdx == 4);
}

TEST_CASE("The Pack's Dual Amp key is additive: amps untouched, no heal, no dual fields in the shared file")
{
  VoLumAmpSettings amps[volum::kAmpCount]{};
  amps[kAmpeteOne] = AmpeteLeadScene();
  const nlohmann::json main =
    volum::VolumUserSettingsToJson(amps, volum::kAmpCount, kAmpeteOne, nullptr, /*includeDualAmp=*/false);
  const nlohmann::json sidecar = volum::VolumDualAmpUserSettingsToJson(amps, volum::kAmpCount);

  const nlohmann::json merged = nlohmann::json::parse(SettingsWithDualAmp(main.dump(), sidecar));
  CHECK(merged["amps"] == main["amps"]);
  CHECK(merged[volum::pack::kPackDualAmpSettingsKey] == sidecar);
  CHECK_FALSE(volum::HasDualAmpUserSettings(merged));

  VoLumAmpSettings loaded[volum::kAmpCount]{};
  int lastAmpIdx = -1;
  bool healed = true;
  volum::VolumUserSettingsFromJson(merged, loaded, volum::kAmpCount, &lastAmpIdx, nullptr, &healed);
  CHECK_FALSE(healed);
  CHECK(lastAmpIdx == kAmpeteOne);
  CHECK_FALSE(loaded[kAmpeteOne].dualAmpActive); // the main reader never takes dual from the Pack key

  nlohmann::json extracted;
  REQUIRE(DualAmpSidecarFromSettings(merged.dump(), extracted));
  CHECK(extracted == sidecar);

  CHECK_FALSE(DualAmpSidecarFromSettings(main.dump(), extracted));
  CHECK_FALSE(DualAmpSidecarFromSettings("not json", extracted));
  CHECK_FALSE(DualAmpSidecarFromSettings("{\"volumDualAmpSettings\":{\"amps\":[]}}", extracted));
  CHECK(SettingsWithDualAmp("not json", sidecar) == "not json");
}

TEST_CASE("LiveSceneGate holds nest and release")
{
  LiveSceneGate gate;
  CHECK(gate.Allows());
  {
    LiveSceneGate::Hold outer(gate);
    CHECK_FALSE(gate.Allows());
    {
      LiveSceneGate::Hold inner(gate);
      CHECK_FALSE(gate.Allows());
    }
    CHECK_FALSE(gate.Allows());
  }
  CHECK(gate.Allows());
}

TEST_CASE("The plugin restores machine settings in the order the F-88 test drives")
{
  const std::string locks = ReadSource("VoLumSettingsLocks.inc.cpp");
  const std::string save = FunctionBody(locks, "void NeuralAmpModeler::_VolumSaveCurrentToSettings()");
  const auto gateCheck = save.find("if (!mVolumLiveSceneGate.Allows())");
  REQUIRE(gateCheck != std::string::npos);
  CHECK(save.find("return;", gateCheck) < save.find("volum::VoLumAmpSettings* target"));

  const std::string actions = ReadSource("VoLumPackActions.inc.cpp");
  const std::string exportBody = FunctionBody(actions, "std::string NeuralAmpModeler::_VolumExportPack(");
  CHECK(exportBody.find("volum::pack::SettingsWithDualAmp(") != std::string::npos);

  const std::string importBody = FunctionBody(actions, "std::string NeuralAmpModeler::_VolumImportPack(");
  const auto sidecar = importBody.find("volum::pack::DualAmpSidecarFromSettings(pack.settingsJson");
  const auto hold = importBody.find("volum::LiveSceneGate::Hold restoring(mVolumLiveSceneGate);");
  const auto load = importBody.find("_VolumLoadSettingsFromFile();");
  const auto apply = importBody.find("_VolumSelectFactoryAmp(mVolumAmpIdx, /*snapshotOutgoing=*/false);");
  const auto locksApplied = importBody.find("_VolumApplyLiveLockSnapshots();");
  const auto session = importBody.find("_VolumRestoreSessionSelection();");
  REQUIRE(sidecar != std::string::npos);
  REQUIRE(hold != std::string::npos);
  REQUIRE(load != std::string::npos);
  REQUIRE(apply != std::string::npos);
  REQUIRE(locksApplied != std::string::npos);
  REQUIRE(session != std::string::npos);
  CHECK(sidecar < hold);
  CHECK(hold < load);
  CHECK(load < apply);
  CHECK(apply < locksApplied);
  // The hold's scope closes after the lock snapshots and before the session
  // selection, which may snapshot the now-live restored scene.
  const auto scopeEnd = importBody.find('}', locksApplied);
  CHECK(scopeEnd < session);
  CHECK(importBody.find('{', hold) > scopeEnd);
}
