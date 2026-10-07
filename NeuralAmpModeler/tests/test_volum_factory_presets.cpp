#include "third_party/doctest.h"

#include "VoLumCustomContentApi.h"
#include "VoLumFactoryPresets.h"
#include "VoLumPlayModel.h"
#include "VoLumPrePedalCaptures.h"
#include "volum_factory_bank.h"

#include <filesystem>
#include <fstream>
#include <utility>

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
    volum::custom::PresetHooksByInstance().clear();
  }
  ~GlobalStoreReset()
  {
    volum::custom::PresetCaptureHook() = nullptr;
    volum::custom::PresetApplyHook() = nullptr;
    volum::custom::PresetHookOwner() = nullptr;
    volum::custom::PresetHooksByInstance().clear();
  }
};
} // namespace

namespace
{
// The owner's Sounds Pack (.scratch/1.3.0-rc/presetSounds.volumpack) in Pack order,
// with "Marshall 2204" renamed. PLAY pre-fill and old MIDI maps address these ids.
const std::pair<const char*, const char*> kShippedFactory[] = {
  {"factory:0:v1", "Ampete Rhythm"},  {"factory:0:v2", "Ampete Lead"},        {"factory:1:v1", "BadCat Crunch"},
  {"factory:2:v1", "American Lead"},  {"factory:3:v1", "Thicc Rhythm"},       {"factory:3:v2", "Sanitarium"},
  {"factory:4:v1", "Dry Rhythm"},     {"factory:5:v1", "HiFi Heavy"},         {"factory:5:v2", "HiFi Crunch"},
  {"factory:6:v1", "Modern Rhythm"},  {"factory:6:v2", "Modern Lead"},        {"factory:7:v1", "JCM800 Crunch"},
  {"factory:8:v1", "Crack the Skye"}, {"factory:9:v1", "Modern British"},     {"factory:10:v1", "Stoner"},
  {"factory:11:v1", "An old Soul"},   {"factory:12:v1", "The bestest Clean"}, {"factory:13:v1", "SLO Lead"},
  {"factory:13:v2", "SLO Crunch"},    {"factory:14:v1", "Sunset Crunch"},     {"factory:14:v2", "Sunset Clean"},
};

nlohmann::json ShippedFactoryJson()
{
  std::ifstream in(volum_test::ShippedFactoryPresetsPath());
  REQUIRE(in.good());
  nlohmann::json root;
  in >> root;
  return root;
}

nlohmann::json KeysWithPrefix(const nlohmann::json& settings, const std::string& prefix)
{
  nlohmann::json out = nlohmann::json::object();
  for (auto it = settings.begin(); it != settings.end(); ++it)
    if (it.key().rfind(prefix, 0) == 0)
      out[it.key()] = it.value();
  return out;
}

std::vector<volum::FactoryPreset> LoadJson(const nlohmann::json& root, const char* tempName)
{
  const auto temp = std::filesystem::temp_directory_path() / tempName;
  {
    std::ofstream out(temp);
    out << root.dump(2);
  }
  auto bank = volum::LoadFactoryPresets(temp);
  std::error_code ec;
  std::filesystem::remove(temp, ec);
  return bank;
}
} // namespace

TEST_CASE("Shipped Factory bank holds the owner's 21 presets in Pack order with stable ids")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  REQUIRE(bank.size() == std::size(kShippedFactory));
  for (size_t i = 0; i < bank.size(); ++i)
  {
    CAPTURE(i);
    CHECK(bank[i].id == kShippedFactory[i].first);
    CHECK(bank[i].name == kShippedFactory[i].second);
    int amp = -1, version = 0;
    REQUIRE(volum::ParseFactoryPresetId(bank[i].id, amp, version));
    CHECK(bank[i].ampIdx == amp);
    CHECK(bank[i].version == version);
    CHECK(volum::IsFactoryPresetId(bank[i].id));
  }
  for (int amp = 0; amp < volum::kAmpCount; ++amp)
  {
    CAPTURE(amp);
    const auto rows = volum::FactoryPresetsForAmp(bank, amp);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows.size() <= 2);
    for (size_t v = 0; v < rows.size(); ++v)
      CHECK(rows[v]->id == volum::FactoryPresetId(amp, static_cast<int>(v) + 1));
  }
}

TEST_CASE("Every shipped Factory preset is a full explicit snapshot of bundled content")
{
  const auto root = ShippedFactoryJson();
  const auto bank = volum_test::ShippedFactoryPresets();
  REQUIRE(root.size() == bank.size());
  const auto preCaptures = volum::DiscoverPrePedalCaptures(volum_test::ShippedFactoryPresetsPath().parent_path());
  REQUIRE_FALSE(preCaptures.empty());
  const auto defaultJson = volum::AmpSettingsToJson(volum::VoLumAmpSettings{});
  for (const auto& preset : bank)
  {
    CAPTURE(preset.id);
    const auto& raw = root.at(preset.id).at("settings");
    // Every field written out, so a later default change cannot revoice it.
    for (auto it = defaultJson.begin(); it != defaultJson.end(); ++it)
    {
      CAPTURE(it.key());
      CHECK(raw.contains(it.key()));
    }
    CHECK(volum::AmpSettingsToJson(preset.settings) == raw);
    CHECK_FALSE(volum::AmpSettingsEqual(preset.settings, volum::VoLumAmpSettings{}));

    const auto& s = preset.settings;
    CHECK(s.activeIrId.empty());
    CHECK(s.supportActiveIrId.empty());
    CHECK(s.supportCustomId.empty());
    CHECK(s.supportAmpIdx >= -1);
    CHECK(s.supportAmpIdx < volum::kAmpCount);
    CHECK(s.preNam1Capture >= 0);
    CHECK(s.preNam1Capture <= static_cast<int>(preCaptures.size()));
    CHECK(s.preNam2Capture >= 0);
    CHECK(s.preNam2Capture <= static_cast<int>(preCaptures.size()));
  }
}

TEST_CASE("Shipped bypassed Pitch and Chorus sit on the new defaults; active pedals keep their dial")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  REQUIRE_FALSE(bank.empty());
  const auto defaults = volum::AmpSettingsToJson(volum::VoLumAmpSettings{});
  int activeChorus = 0;
  for (const auto& preset : bank)
  {
    CAPTURE(preset.id);
    const auto json = volum::AmpSettingsToJson(preset.settings);
    if (!preset.settings.prePitchActive)
      CHECK(KeysWithPrefix(json, "prePitch") == KeysWithPrefix(defaults, "prePitch"));
    if (!preset.settings.postChorusActive)
      CHECK(KeysWithPrefix(json, "postChorus") == KeysWithPrefix(defaults, "postChorus"));
    else
      ++activeChorus;
  }
  // The one dialed chorus in the Pack stays CLASSIC, not normalized to ENSEMBLE.
  CHECK(activeChorus == 1);
  const auto* sanitarium = volum::FindFactoryPresetById(bank, "factory:3:v2");
  REQUIRE(sanitarium != nullptr);
  CHECK(sanitarium->settings.postChorusActive);
  CHECK(sanitarium->settings.postChorusMode == volum::kVoLumChorusModeClassic);
}

TEST_CASE("Factory preset ids parse only in the canonical factory:<amp>:v<N> form")
{
  int amp = -1, version = 0;
  CHECK(volum::ParseFactoryPresetId("factory:0:v1", amp, version));
  CHECK(amp == 0);
  CHECK(version == 1);
  CHECK(volum::ParseFactoryPresetId("factory:13:v2", amp, version));
  CHECK(amp == 13);
  CHECK(version == 2);
  for (const char* bad :
       {"factory:15:v1", "factory:0:v0", "factory:01:v1", "factory:0:v01", "factory:0", "factory:0:v", "factory:-1:v1",
        "factory:0:v1x", "factory:0:v-1", "factory::v1", "preset_user", "", "factory:0:1", "factory:99999:v1"})
  {
    CAPTURE(bad);
    CHECK_FALSE(volum::ParseFactoryPresetId(bad, amp, version));
    CHECK_FALSE(volum::IsFactoryPresetId(bad));
  }
  CHECK(volum::FactoryPresetId(13, 2) == "factory:13:v2");
  CHECK(volum::FactoryPresetAmpIndex("factory:13:v2") == 13);
}

TEST_CASE("Factory rows are immutable and custom amps never gain one")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  REQUIRE_FALSE(bank.empty());
  CHECK_FALSE(volum::FactoryPresetCanMutate(bank[0].id));
  CHECK_FALSE(volum::FactoryPresetCanMutate("factory:13:v2"));
  CHECK(volum::FactoryPresetCanMutate("preset_user"));
  CHECK(volum::FactoryPresetsForAmp(bank, 4).size() == 1);
  CHECK(volum::FactoryPresetsForAmp(bank, volum::kAmpCount).empty());
  CHECK(volum::FactoryPresetsForAmp(bank, -1).empty());
}

TEST_CASE("A two-preset amp lists Factory rows in file order, then its User bank")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  const auto factory = volum::FactoryPresetsForAmp(bank, 13);
  REQUIRE(factory.size() == 2);
  const std::vector<std::string> users = {"My Lead", "My Clean"};

  // The bar's list and the BUILD menu's row codes are one index space.
  CHECK(volum::PresetRowNames(factory, users)
        == std::vector<std::string>{"SLO Lead", "SLO Crunch", "My Lead", "My Clean"});
  CHECK(volum::FactoryPresetRow(factory, "factory:13:v1") == 0);
  CHECK(volum::FactoryPresetRow(factory, "factory:13:v2") == 1);
  CHECK(volum::FactoryPresetRow(factory, "factory:12:v1") == -1);

  const auto r0 = volum::PresetRowAt(factory, 2, 0);
  REQUIRE(r0.factory != nullptr);
  CHECK(r0.factory->id == "factory:13:v1");
  const auto r1 = volum::PresetRowAt(factory, 2, 1);
  REQUIRE(r1.factory != nullptr);
  CHECK(r1.factory->id == "factory:13:v2");
  CHECK(r1.factory->name == "SLO Crunch");
  const auto r2 = volum::PresetRowAt(factory, 2, 2);
  CHECK(r2.factory == nullptr);
  CHECK(r2.userIdx == 0);
  const auto r3 = volum::PresetRowAt(factory, 2, 3);
  CHECK(r3.factory == nullptr);
  CHECK(r3.userIdx == 1);
  for (int outside : {-1, 4, 99})
  {
    CAPTURE(outside);
    const auto r = volum::PresetRowAt(factory, 2, outside);
    CHECK(r.factory == nullptr);
    CHECK(r.userIdx == -1);
  }
  // A custom amp (no Factory rows) indexes its User bank from 0.
  const auto custom = volum::PresetRowAt({}, 2, 0);
  CHECK(custom.factory == nullptr);
  CHECK(custom.userIdx == 0);
}

TEST_CASE("PLAY and MIDI reach the second Factory preset; an unknown version reads Invalid")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  volum::content::Registry registry;

  volum::SoundChoice resolved;
  REQUIRE(volum::ResolveSound(bank, registry, "factory:13", "factory:13:v2", resolved));
  CHECK(resolved.presetName == "SLO Crunch");
  CHECK(resolved.factory);
  const auto settings = volum::ResolveSoundSettings(bank, registry, "factory:13", "factory:13:v2");
  REQUIRE(settings.has_value());
  CHECK(volum::AmpSettingsEqual(*settings, volum::FindFactoryPresetById(bank, "factory:13:v2")->settings));
  CHECK_FALSE(volum::ResolveSound(bank, registry, "factory:12", "factory:13:v2", resolved));
  CHECK_FALSE(volum::ResolveSound(bank, registry, "factory:13", "factory:13:v3", resolved));

  REQUIRE(volum::content::AssignMidiSound(registry, 1, "factory:13", "factory:13:v2"));
  REQUIRE(volum::content::AssignMidiSound(registry, 2, "factory:13", "factory:13:v3"));
  REQUIRE(volum::content::AssignMidiSound(registry, 3, "factory:12", "factory:13:v2"));
  // The footswitch path: identity from the content layer, then the bank-aware recall.
  const auto midi = volum::content::ResolveMidiSound(registry, 1);
  REQUIRE(midi.has_value());
  CHECK(midi->presetId == "factory:13:v2");
  CHECK(volum::ResolveSoundSettings(bank, registry, midi->ampId, midi->presetId).has_value());
  const auto unknown = volum::content::ResolveMidiSound(registry, 2);
  if (unknown)
    CHECK_FALSE(volum::ResolveSoundSettings(bank, registry, unknown->ampId, unknown->presetId).has_value());
  CHECK_FALSE(volum::content::ResolveMidiSound(registry, 3).has_value());

  const auto slots = volum::BuildPlaySlots(bank, registry);
  REQUIRE(slots.size() == 3);
  CHECK(slots[0].valid);
  CHECK(slots[0].sound.presetName == "SLO Crunch");
  CHECK_FALSE(slots[1].valid);
  CHECK(slots[1].sound.presetName == std::string(volum::kPlayInvalidSlotLabel));
  CHECK_FALSE(slots[2].valid);

  const auto choices = volum::BuildSoundChoices(bank, registry);
  int sloFactory = 0;
  for (const auto& c : choices)
    if (c.factory && c.ampId == "factory:13")
      ++sloFactory;
  CHECK(sloFactory == 2);
}

TEST_CASE("A MIDI map that pointed at Ready now plays the baked sound")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  volum::content::Registry registry;
  REQUIRE(volum::content::AssignMidiSound(registry, 6, "factory:7", "factory:7:v1"));
  const auto midi = volum::content::ResolveMidiSound(registry, 6);
  REQUIRE(midi.has_value());
  volum::SoundChoice resolved;
  REQUIRE(volum::ResolveSound(bank, registry, midi->ampId, midi->presetId, resolved));
  CHECK(resolved.presetName == "JCM800 Crunch");
  const auto applied = volum::ResolveSoundSettings(bank, registry, midi->ampId, midi->presetId);
  REQUIRE(applied.has_value());
  CHECK_FALSE(volum::AmpSettingsEqual(*applied, volum::VoLumAmpSettings{}));
  CHECK(volum::AmpSettingsEqual(*applied, volum::FindFactoryPresetById(bank, "factory:7:v1")->settings));
}

TEST_CASE("Dirty Factory Save creates a User preset and leaves Factory unchanged")
{
  GlobalStoreReset reset;
  auto& store = volum::content::GlobalContentStore();
  const auto factory = volum_test::ShippedFactoryPresets();
  const auto* second = volum::FindFactoryPresetById(factory, "factory:13:v2");
  REQUIRE(second != nullptr);
  const auto before = second->settings;
  // Ctrl+S on the second Factory preset of an amp is still a Save As, never an
  // overwrite of a shipped row.
  CHECK(volum::SaveActionForActivePreset(second->id) == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset("factory:13:v1") == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset("") == volum::PresetSaveAction::SaveUserCopy);
  CHECK(volum::SaveActionForActivePreset("preset_user") == volum::PresetSaveAction::OverwriteUser);

  volum::custom::SetActivePresetOwner(volum::content::FactoryOwnerKey(13));
  volum::custom::PresetCaptureHook() = [before] {
    volum::VoLumAmpSettings changed = before;
    changed.toneBass = 8.5;
    return changed;
  };
  const int index = volum::custom::AddPreset(13, "SLO Crunch edit");

  REQUIRE(index == 0);
  const auto& user = store.reg().presetBanks.at("factory:13")[0];
  CHECK(user.id.rfind("preset_", 0) == 0);
  CHECK(user.settings.toneBass == doctest::Approx(8.5));
  CHECK(volum::AmpSettingsEqual(second->settings, before));
  CHECK(volum::FindFactoryPresetById(factory, "factory:13:v2") == second);
}

TEST_CASE("Factory snapshot file can revoice a preset without changing its id")
{
  volum::VoLumAmpSettings revoiced;
  revoiced.toneMid = 7.25;
  nlohmann::json root;
  root["factory:6:v1"] = {{"name", "Modern Rhythm"}, {"settings", volum::AmpSettingsToJson(revoiced)}};
  const auto bank = LoadJson(root, "volum-factory-presets-test.json");
  REQUIRE(bank.size() == 1);
  REQUIRE(bank[0].id == "factory:6:v1");
  CHECK(bank[0].settings.toneMid == doctest::Approx(7.25));
}

TEST_CASE("Factory file loads by id, in version order, and drops what it cannot name")
{
  nlohmann::json root;
  root["factory:3:v2"] = {{"name", "Second"}, {"settings", nlohmann::json::object()}};
  root["factory:3:v10"] = {{"name", "Tenth"}, {"settings", nlohmann::json::object()}};
  root["factory:3:v1"] = {{"name", "First"}, {"settings", nlohmann::json::object()}};
  root["factory:1:v1"] = {{"name", "One"}, {"settings", nlohmann::json::object()}};
  root["factory:2:v1"] = {{"name", "No settings"}};
  root["factory:2:v2"] = "not an object";
  root["factory:15:v1"] = {{"name", "No such amp"}, {"settings", nlohmann::json::object()}};
  root["factory:4:v0"] = {{"name", "Version zero"}, {"settings", nlohmann::json::object()}};
  root["preset_user"] = {{"name", "User"}, {"settings", nlohmann::json::object()}};
  const auto bank = LoadJson(root, "volum-factory-presets-order-test.json");
  REQUIRE(bank.size() == 4);
  CHECK(bank[0].id == "factory:1:v1");
  CHECK(bank[1].id == "factory:3:v1");
  CHECK(bank[2].id == "factory:3:v2");
  CHECK(bank[3].id == "factory:3:v10");
  CHECK(volum::FactoryPresetsForAmp(bank, 2).empty());

  CHECK(volum::LoadFactoryPresets(std::filesystem::temp_directory_path() / "volum-no-such-factory.json").empty());
}

TEST_CASE("Without a readable Factory file every amp keeps a v1 that resolves")
{
  const auto fallback = volum::FactoryPresetsOrFallback({});
  REQUIRE(fallback.size() == static_cast<size_t>(volum::kAmpCount));
  for (int amp = 0; amp < volum::kAmpCount; ++amp)
  {
    const auto rows = volum::FactoryPresetsForAmp(fallback, amp);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0]->id == volum::FactoryPresetId(amp, 1));
    CHECK(rows[0]->version == 1);
    CHECK(volum::AmpSettingsEqual(rows[0]->settings, volum::VoLumAmpSettings{}));
    CHECK_FALSE(rows[0]->name.empty());
  }

  const auto shipped = volum_test::ShippedFactoryPresets();
  REQUIRE_FALSE(shipped.empty());
  CHECK(volum::FactoryPresetsOrFallback(shipped).size() == shipped.size());
}

// Default (_VolumResetAmpToFactory) assigns VoLumAmpSettings{}, and a Factory
// snapshot of "settings": {} inherits it, so every one of them has to make a
// pedal audible the moment it is switched on.
TEST_CASE("Fresh scene, Default and an empty Factory snapshot switch pedals on audibly")
{
  nlohmann::json root;
  root["factory:0:v1"] = {{"name", "Empty"}, {"settings", nlohmann::json::object()}};
  const auto loaded = LoadJson(root, "volum-factory-pedal-defaults-test.json");
  REQUIRE(loaded.size() == 1);
  const volum::VoLumAmpSettings fresh;
  const auto healed = volum::HealedFactoryPresetSettings(loaded[0].settings);

  const auto ensemble = volum::kVoLumChorusModeDefaults[volum::kVoLumChorusModeEnsemble];
  const volum::VoLumAmpSettings* scenes[] = {&fresh, &loaded[0].settings, &healed};
  for (const auto* s : scenes)
  {
    CAPTURE(s - scenes[0]);
    CHECK_FALSE(s->prePitchActive);
    CHECK(s->prePitchMode == volum::kVoLumPitchModeOctaver);
    CHECK(s->prePitchSemitones == doctest::Approx(-2.0));
    // The Octaver's own knobs keep their factory blend.
    CHECK(s->prePitchMix == doctest::Approx(1.0));
    CHECK(s->prePitchOctDown == doctest::Approx(0.8));
    CHECK(s->prePitchOctUp == doctest::Approx(0.0));
    CHECK(s->prePitchDry == doctest::Approx(1.0));
    CHECK(s->prePitchVoicing == volum::kVoLumPitchVoicingModern);
    CHECK(s->prePitchLevel == doctest::Approx(0.0));

    CHECK_FALSE(s->postChorusActive);
    CHECK(s->postChorusMode == volum::kVoLumChorusModeEnsemble);
    CHECK(s->postChorusRate == doctest::Approx(ensemble.rate));
    CHECK(s->postChorusDepth == doctest::Approx(ensemble.depth));
    CHECK(s->postChorusTone == doctest::Approx(ensemble.tone));
    CHECK(s->postChorusWidth == doctest::Approx(ensemble.width));
    CHECK(s->postChorusMix == doctest::Approx(ensemble.mix));

    // PRE NAM slots stay EMPTY: no capture is chosen for the user.
    CHECK_FALSE(s->preNam1Active);
    CHECK_FALSE(s->preNam2Active);
    CHECK(s->preNam1Capture == 0);
    CHECK(s->preNam2Capture == 0);
  }

  // The live POST working copy seeds the same voice.
  CHECK(volum::VoLumEffectSettings{}.chorusMode == volum::kVoLumChorusModeEnsemble);
}

TEST_CASE("A factory preset shows the name its snapshot file gives it")
{
  nlohmann::json root;
  root["factory:12:v1"] = {{"name", "  Texas Crunch  "}, {"settings", nlohmann::json::object()}};
  root["factory:3:v1"] = {{"name", ""}, {"settings", nlohmann::json::object()}};
  root["factory:3:v2"] = {{"settings", nlohmann::json::object()}};
  root["factory:4:v1"] = {{"name", std::string(80, 'x')}, {"settings", nlohmann::json::object()}};
  const auto bank = LoadJson(root, "volum-factory-preset-names-test.json");
  REQUIRE(bank.size() == 4);
  const auto* texas = volum::FindFactoryPresetById(bank, "factory:12:v1"); // the id PLAY and MIDI store
  REQUIRE(texas != nullptr);
  CHECK(texas->name == "Texas Crunch");
  CHECK(volum::FindFactoryPresetById(bank, "factory:3:v1")->name == "Factory 1");
  CHECK(volum::FindFactoryPresetById(bank, "factory:3:v2")->name == "Factory 2");
  CHECK(volum::FindFactoryPresetById(bank, "factory:4:v1")->name.size() == volum::kFactoryPresetNameMaxBytes);
  CHECK(volum::FactoryPresetsForAmp(bank, 0).empty()); // absent from the file: no row, no placeholder

  volum::content::Registry registry;
  volum::SoundChoice resolved;
  REQUIRE(volum::ResolveSound(bank, registry, volum::content::FactoryOwnerKey(12), "factory:12:v1", resolved));
  CHECK(resolved.presetName == "Texas Crunch");
  CHECK_FALSE(volum::ResolveSound(bank, registry, volum::content::FactoryOwnerKey(0), "factory:0:v1", resolved));
}

TEST_CASE("Every Factory Sound is a PLAY choice on an empty or a pre-filled board")
{
  const auto bank = volum_test::ShippedFactoryPresets();
  volum::content::Registry empty;
  volum::content::Registry prefilled;
  prefilled.midiSoundMap = volum::PlayPrefillAssignments(bank);
  REQUIRE(prefilled.midiSoundMap.size() == 5);
  for (const auto* registry : {&empty, &prefilled})
  {
    const auto sounds = volum::BuildSoundChoices(bank, *registry);
    REQUIRE(sounds.size() == std::size(kShippedFactory));
    CHECK(sounds[0].presetName == "Ampete Rhythm");
    CHECK(sounds[1].presetName == "Ampete Lead");
    CHECK(sounds[0].factory);
    CHECK(sounds[1].factory);
  }
  // The five pre-filled Sounds are named presets of the shipped list.
  for (const auto& sound : volum::kPlayPrefillSounds)
  {
    CAPTURE(sound.presetId);
    const auto shipped = std::find_if(std::begin(kShippedFactory), std::end(kShippedFactory),
                                      [&](const auto& e) { return std::string(e.first) == sound.presetId; });
    REQUIRE(shipped != std::end(kShippedFactory));
    CHECK(std::string(shipped->second) == sound.name);
  }
}

TEST_CASE("Factory dirty ignores the postValid restore sentinel")
{
  // Live save always stamps postValid=true. A Factory snapshot may carry
  // postValid=false. That sentinel is not a knob: after a no-edit relaunch the
  // sounding scenes match, so PLAY + must assign the Factory preset without Save As.
  const volum::VoLumAmpSettings factory;
  REQUIRE_FALSE(factory.postValid);

  volum::VoLumAmpSettings live = factory;
  live.postValid = true;
  REQUIRE(volum::AmpSettingsEqual(live, factory));
  REQUIRE_FALSE(volum::LivePresetDirty(true, live, factory));
  REQUIRE_FALSE(volum::AddHeardNeedsSaveAs(volum::PresetSaveAction::SaveUserCopy, false, false));
  REQUIRE_FALSE(volum::AddHeardNeedsSaveAs(
    volum::PresetSaveAction::SaveUserCopy, volum::LivePresetDirty(true, live, factory), false));

  live.toneBass = 8.0;
  REQUIRE(volum::LivePresetDirty(true, live, factory));
}

namespace
{
using volum::content::MidiSoundAssignment;

// A pedalboard with the Factory Sound of amp 0 on program 3 and a User Sound of
// amp 2 on program 12.
volum::content::Registry TwoSwitchBoard()
{
  volum::content::Registry reg;
  REQUIRE(volum::content::AssignMidiSound(reg, 3, "factory:0", "factory:0:v1"));
  REQUIRE(volum::content::AssignMidiSound(reg, 12, "factory:2", "preset_b"));
  return reg;
}

// _VolumPromptSaveAs's commit without the dialog: decide from the LIVE switch as
// it is at commit and the pair recorded at open, then point it at the new preset.
void CommitSave(volum::content::Registry& reg, volum::SaveOrigin origin, volum::UiMode modeAtStart, int liveSlot,
                const MidiSoundAssignment& editSource, const std::string& savedOwner, const std::string& savedId)
{
  if (volum::SaveRetargetsLiveSlot(origin, modeAtStart, volum::content::MidiSoundAtSlot(reg, liveSlot), editSource))
    volum::content::AssignMidiSound(reg, liveSlot, savedOwner, savedId);
}
} // namespace

TEST_CASE("A save started in BUILD leaves every PLAY switch as it was")
{
  // The LIVE marker stays set through BUILD, so it alone cannot license a write.
  auto reg = TwoSwitchBoard();
  const auto before = reg.midiSoundMap;
  const int live = 3;

  SUBCASE("Ctrl+S on another amp")
  {
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Build, live, {"factory:5", "preset_x"}, "factory:5",
               "preset_new");
  }
  SUBCASE("Save As after recalling the preset that sits on the LIVE switch")
  {
    // BUILD recall of an assigned preset moves the LIVE marker onto its switch
    // (_VolumSyncLivePlaySlotFromActivePair), so the pair matches exactly.
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Build, live, {"factory:0", "factory:0:v1"}, "factory:0",
               "preset_new");
  }
  CHECK(reg.midiSoundMap.size() == before.size());
  for (const auto& [slot, sound] : before)
  {
    const auto* now = volum::content::MidiSoundAtSlot(reg, slot);
    REQUIRE(now != nullptr);
    CHECK(now->ampId == sound.ampId);
    CHECK(now->presetId == sound.presetId);
  }
}

TEST_CASE("PLAY Ctrl+S moves the LIVE switch only while it holds the Sound the edit started from")
{
  auto reg = TwoSwitchBoard();
  const int live = 3;
  const MidiSoundAssignment factoryOnLive{"factory:0", "factory:0:v1"};

  SUBCASE("a tweaked Factory Sound on its LIVE switch moves that switch")
  {
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    REQUIRE(volum::content::MidiSoundAtSlot(reg, live) != nullptr);
    CHECK(volum::content::MidiSoundAtSlot(reg, live)->presetId == "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, 12)->presetId == "preset_b");
  }
  SUBCASE("the LIVE switch was reassigned to another Sound since the edit began")
  {
    REQUIRE(volum::content::AssignMidiSound(reg, live, "factory:7", "preset_other"));
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, live)->presetId == "preset_other");
  }
  SUBCASE("the LIVE switch was cleared")
  {
    REQUIRE(volum::content::ClearMidiSound(reg, live));
    CommitSave(reg, volum::SaveOrigin::Shortcut, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, live) == nullptr);
  }
  SUBCASE("Add this sound adds a switch of its own")
  {
    CommitSave(reg, volum::SaveOrigin::AddSound, volum::UiMode::Play, live, factoryOnLive, "factory:0", "preset_new");
    CHECK(volum::content::MidiSoundAtSlot(reg, live)->presetId == "factory:0:v1");
  }
  SUBCASE("the Default sound has no preset id to match")
  {
    CHECK_FALSE(volum::SaveRetargetsLiveSlot(
      volum::SaveOrigin::Shortcut, volum::UiMode::Play, volum::content::MidiSoundAtSlot(reg, live), {"factory:0", ""}));
  }
}

TEST_CASE("Healed factory snapshot stamps postValid the way apply does")
{
  volum::VoLumAmpSettings factory;
  factory.toneMid = 6.5;
  const auto healed = volum::HealedFactoryPresetSettings(factory);
  REQUIRE(healed.postValid);
  REQUIRE_FALSE(factory.postValid);

  volum::VoLumAmpSettings live = factory;
  live.postValid = true;
  REQUIRE(volum::AmpSettingsEqual(live, healed));
}

TEST_CASE("BUILD preset menu, bar and recall walk every Factory row of the focused amp")
{
  const auto read = [](const char* name) {
    std::ifstream in(volum_test::ShippedFactoryPresetsPath().parent_path().parent_path() / "NeuralAmpModeler" / name,
                     std::ios::binary);
    REQUIRE(in.good());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  };
  const std::string menus = read("VoLumAmpMenus.inc.cpp");
  const std::string presets = read("VoLumSettingsPresets.inc.cpp");
  const std::string layout = read("VoLumLayoutBuild.inc.cpp");

  CHECK(menus.find("for (int i = 0; i < factoryCount; i++)") != std::string::npos);
  CHECK(menus.find("rows.push_back({factory[(size_t)i]->name, i, false, false});") != std::string::npos);
  CHECK(menus.find("rows.push_back({presets[(size_t)i], i + factoryCount, false, false});") != std::string::npos);
  CHECK(presets.find("bar->SetList(volum::PresetRowNames(factory, users));") != std::string::npos);
  CHECK(presets.find("volum::PresetRowAt(factory, userCount, index)") != std::string::npos);
  CHECK(layout.find("code < static_cast<int>(presets.size()) + factoryCount") != std::string::npos);
  for (const auto* src : {&menus, &presets, &layout})
    CHECK(src->find("hasFactory ? 1 : 0") == std::string::npos);
}
