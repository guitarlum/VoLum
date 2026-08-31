#include "third_party/doctest.h"

#include "VoLumCustomContentApi.h"
#include "VoLumFactoryPresets.h"
#include "VoLumPlayModel.h"

#include <filesystem>
#include <fstream>

namespace
{
struct GlobalStoreReset
{
  GlobalStoreReset()
  {
    auto& store = volum::content::GlobalContentStore();
    store.SetBaseDir({});
    store.reg() = {};
    volum::custom::SetActivePresetOwner(volum::content::FactoryOwnerKey(0));
    volum::custom::PresetCaptureHook() = nullptr;
    volum::custom::PresetApplyHook() = nullptr;
  }
  ~GlobalStoreReset()
  {
    volum::custom::PresetCaptureHook() = nullptr;
    volum::custom::PresetApplyHook() = nullptr;
    volum::custom::PresetHookOwner() = nullptr;
  }
};
} // namespace

TEST_CASE("Factory bank exposes one stable Ready preset for every factory amp")
{
  const auto bank = volum::DefaultFactoryPresets();
  REQUIRE(bank.size() == volum::kAmpCount);
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    CAPTURE(i);
    CHECK(bank[(size_t)i].id == "factory:" + std::to_string(i) + ":v1");
    CHECK(bank[(size_t)i].ampIdx == i);
    CHECK(volum::IsFactoryPresetId(bank[(size_t)i].id));
  }
}

TEST_CASE("Factory rows are immutable and custom amps never gain one")
{
  const auto bank = volum::DefaultFactoryPresets();
  CHECK_FALSE(volum::FactoryPresetCanMutate(bank[0].id));
  CHECK(volum::FactoryPresetCanMutate("preset_user"));
  CHECK(volum::FindFactoryPresetForAmp(bank, 4) != nullptr);
  CHECK(volum::FindFactoryPresetForAmp(bank, volum::kAmpCount) == nullptr);
}

TEST_CASE("Dirty Factory Save creates a User preset and leaves Factory unchanged")
{
  GlobalStoreReset reset;
  auto& store = volum::content::GlobalContentStore();
  const auto factory = volum::DefaultFactoryPresets();
  const auto before = factory[3].settings;
  CHECK(volum::SaveActionForActivePreset(factory[3].id) == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset("preset_user") == volum::PresetSaveAction::OverwriteUser);

  volum::custom::SetActivePresetOwner(volum::content::FactoryOwnerKey(3));
  volum::custom::PresetCaptureHook() = [] {
    volum::VoLumAmpSettings changed;
    changed.toneBass = 8.5;
    return changed;
  };
  const int index = volum::custom::AddPreset(3, "Ready edit");

  REQUIRE(index == 0);
  const auto& user = store.reg().presetBanks.at("factory:3")[0];
  CHECK(user.id.rfind("preset_", 0) == 0);
  CHECK(user.settings.toneBass == doctest::Approx(8.5));
  CHECK(volum::AmpSettingsEqual(factory[3].settings, before));
}

TEST_CASE("Factory snapshot file can revoice a preset without changing its id")
{
  const auto temp = std::filesystem::temp_directory_path() / "volum-factory-presets-test.json";
  volum::VoLumAmpSettings revoiced;
  revoiced.toneMid = 7.25;
  nlohmann::json root;
  root["factory:6:v1"] = {{"name", "Ready"}, {"settings", volum::AmpSettingsToJson(revoiced)}};
  {
    std::ofstream out(temp);
    out << root.dump(2);
  }

  const auto bank = volum::LoadFactoryPresets(temp);
  std::error_code ec;
  std::filesystem::remove(temp, ec);
  REQUIRE(bank.size() == volum::kAmpCount);
  REQUIRE(bank[6].id == "factory:6:v1");
  CHECK(bank[6].settings.toneMid == doctest::Approx(7.25));
}

TEST_CASE("Factory Sounds are available to PLAY without seeding midiSoundMap")
{
  volum::content::Registry registry;
  const auto sounds = volum::BuildSoundChoices(volum::DefaultFactoryPresets(), registry);
  REQUIRE(sounds.size() == volum::kAmpCount);
  CHECK(sounds[0].presetName == "Ready");
  CHECK(sounds[0].factory);
  CHECK(registry.midiSoundMap.empty());
}
