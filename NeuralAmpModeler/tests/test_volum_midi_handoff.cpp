#include "third_party/doctest.h"

#include "../VoLumContentStore.h"
#include "../VoLumChunkIdTail.h"
#include "../VoLumMidi.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using iplug::IMidiMsg;

namespace
{
std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}
} // namespace

TEST_CASE("MIDI latest-wins handoff drains only the newest slot")
{
  volum::MidiLatestWinsQueue queue;
  CHECK_FALSE(queue.Drain().has_value());
  queue.Enqueue(4);
  queue.Enqueue(91);
  REQUIRE(queue.Drain().has_value());

  queue.Enqueue(4);
  queue.Enqueue(91);
  const auto newest = queue.Drain();
  REQUIRE(newest.has_value());
  CHECK(*newest == 91);
  CHECK_FALSE(queue.Drain().has_value());
}

TEST_CASE("Headless MIDI resolution applies assigned factory and custom User presets")
{
  using namespace volum::content;
  Registry registry;

  Preset factoryPreset;
  factoryPreset.id = "preset_factory_owner";
  factoryPreset.name = "Lead";
  factoryPreset.settings.toneBass = 8.25;
  registry.presetBanks["factory:7"] = {factoryPreset};
  REQUIRE(AssignMidiSound(registry, 12, "factory:7", factoryPreset.id));

  volum::custom::CustomAmp customAmp;
  customAmp.id = "amp_custom";
  customAmp.name = "Custom";
  registry.amps.push_back(customAmp);
  Preset customPreset;
  customPreset.id = "preset_custom";
  customPreset.name = "Clean";
  customPreset.settings.outputLevel = -4.5;
  registry.presetBanks[customAmp.id] = {customPreset};
  REQUIRE(AssignMidiSound(registry, 13, customAmp.id, customPreset.id));

  const auto factory = ResolveMidiSound(registry, 12);
  REQUIRE(factory.has_value());
  CHECK(factory->ampId == "factory:7");
  CHECK(factory->presetId == factoryPreset.id);
  CHECK(factory->settings.toneBass == doctest::Approx(8.25));

  const auto custom = ResolveMidiSound(registry, 13);
  REQUIRE(custom.has_value());
  CHECK(custom->ampId == customAmp.id);
  CHECK(custom->settings.outputLevel == doctest::Approx(-4.5));
}

TEST_CASE("Headless MIDI resolution ignores unassigned missing invalid and out-of-range Sounds")
{
  using namespace volum::content;
  Registry registry;
  REQUIRE(AssignMidiSound(registry, 2, "factory:99", "preset_missing_amp"));
  REQUIRE(AssignMidiSound(registry, 3, "factory:2", "preset_missing"));
  REQUIRE(AssignMidiSound(registry, 4, "amp_deleted", "preset_deleted"));

  CHECK_FALSE(ResolveMidiSound(registry, -1).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 128).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 1).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 2).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 3).has_value());
  CHECK_FALSE(ResolveMidiSound(registry, 4).has_value());
}

TEST_CASE("Headless MIDI resolution applies a Factory Ready Sound")
{
  using namespace volum::content;
  Registry registry;
  REQUIRE(AssignMidiSound(registry, 8, "factory:7", "factory:7:v1"));
  const auto ready = ResolveMidiSound(registry, 8);
  REQUIRE(ready.has_value());
  CHECK(ready->ampId == "factory:7");
  CHECK(ready->presetId == "factory:7:v1");
}

TEST_CASE("MIDI listen filter in the id tail defaults to all channels and clamps to 1 through 16")
{
  volum::ChunkIdTail tail;
  CHECK(tail.midiCh == 0);
  tail.midiCh = 12;
  CHECK(volum::IdTailFromJson(volum::IdTailToJson(tail)).midiCh == 12);
  CHECK(volum::IdTailFromJson(nlohmann::json::object()).midiCh == 0);
  CHECK(volum::IdTailFromJson({{"midiCh", 99}}).midiCh == 16);
  CHECK(volum::IdTailFromJson({{"midiCh", -2}}).midiCh == 0);
}

TEST_CASE("MIDI recall CC in the id tail defaults to 102 and refuses 120-127")
{
  volum::ChunkIdTail tail;
  CHECK(tail.midiRecallCc == volum::kMidiRecallCcDefault);
  tail.midiRecallCc = 20;
  CHECK(volum::IdTailFromJson(volum::IdTailToJson(tail)).midiRecallCc == 20);
  CHECK(volum::IdTailFromJson(nlohmann::json::object()).midiRecallCc == volum::kMidiRecallCcDefault);
  CHECK(volum::IdTailFromJson({{"midiRecallCc", 119}}).midiRecallCc == 119);
  CHECK(volum::IdTailFromJson({{"midiRecallCc", 0}}).midiRecallCc == 0);
  CHECK(volum::IdTailFromJson({{"midiRecallCc", 120}}).midiRecallCc == volum::kMidiRecallCcDefault);
  CHECK(volum::IdTailFromJson({{"midiRecallCc", 123}}).midiRecallCc == volum::kMidiRecallCcDefault);
  CHECK(volum::IdTailFromJson({{"midiRecallCc", 127}}).midiRecallCc == volum::kMidiRecallCcDefault);
  CHECK(volum::IdTailFromJson({{"midiRecallCc", -2}}).midiRecallCc == volum::kMidiRecallCcDefault);
}

TEST_CASE("Settings MIDI chrome owns the listen filter and the assignment list, but stores neither")
{
  // The listen filter is per instance and rides the DAW id tail. The assignment
  // list is machine global and lives in the content registry. Both are edited on
  // the MIDI tab, and the tab must persist neither itself: a Settings-local copy
  // of the sound map is how PLAY and Settings would start disagreeing.
  const auto root = RepoRoot() / "NeuralAmpModeler";
  const std::string controls = ReadText(root / "NeuralAmpModelerControls.h");
  const std::string tabs = ReadText(root / "VoLumSettingsTabs.h");
  const std::string settings = ReadText(root / "VoLumSettingsScene.inc.cpp");

  CHECK(tabs.find("class VoLumMidiChannelControl") != std::string::npos);
  CHECK(tabs.find("class VoLumMidiRecallCcControl") != std::string::npos);
  CHECK(tabs.find("\"Recall CC\"") != std::string::npos);
  CHECK(tabs.find("Value is the program number.") != std::string::npos);
  CHECK(tabs.find("Use this when Program Change never arrives.") != std::string::npos);
  // 0 is still "every channel" in the data; only the words in front of it changed.
  CHECK(tabs.find("const bool all = mChannel == 0;") != std::string::npos);
  CHECK(tabs.find("\"All channels\"") != std::string::npos);
  CHECK(tabs.find("DrawVoLumSegmentSwitch(") != std::string::npos);
  CHECK(tabs.find("MIDI calls this Omni.") != std::string::npos);
  CHECK(tabs.find("class VoLumMidiSoundMapControl") != std::string::npos);
  CHECK(controls.find("SetMidiCallbacks") != std::string::npos);
  CHECK(controls.find("SetMidiSoundMapCallbacks") != std::string::npos);
  CHECK(controls.find("SetMidiSoundMapSwap") != std::string::npos);
  CHECK(controls.find("SetMidiSoundMapInsert") != std::string::npos);
  CHECK(controls.find("void SetMidiChannel(int channel)") != std::string::npos);
  CHECK(controls.find("void SetMidiRecallCc(int cc)") != std::string::npos);
  CHECK(controls.find("void SetMidiSoundMap(") != std::string::npos);
  // The pre-1.3.0 duplicate-list control stays gone.
  CHECK(controls.find("VoLumMidiSettingsControl") == std::string::npos);
  // The filter persists per instance; the sound map does not ride this document.
  CHECK(settings.find("j[\"midiCh\"] = mVolumMidiChannel.load()") != std::string::npos);
  CHECK(settings.find("j[\"midiRecallCc\"] = mVolumMidiRecallCc.load()") != std::string::npos);
  CHECK(settings.find("MidiChannelFromMachineSettings(true, j,") != std::string::npos);
  CHECK(settings.find("MidiRecallCcFromMachineSettings(true, j,") != std::string::npos);
  CHECK(settings.find("midiSoundMap") == std::string::npos);
  // The tab reads the registry's map; it never assigns into it directly.
  CHECK(tabs.find("midiSoundMap =") == std::string::npos);

  const std::string nam = ReadText(root / "NeuralAmpModeler.cpp");
  const std::string unser = ReadText(root / "Unserialization.cpp");
  const std::string header = ReadText(root / "NeuralAmpModeler.h");
  const std::string presets = ReadText(root / "VoLumSettingsPresets.inc.cpp");
  CHECK(nam.find("idTail.midiRecallCc = mVolumMidiRecallCc.load()") != std::string::npos);
  CHECK(unser.find("mVolumMidiRecallCc.store(idTail.midiRecallCc)") != std::string::npos);
  CHECK(header.find("mVolumMidiRecallCc{volum::kMidiRecallCcDefault}") != std::string::npos);
  CHECK(presets.find("void NeuralAmpModeler::_VolumSetMidiRecallCc(int cc)") != std::string::npos);
  CHECK(presets.find("page->SetMidiRecallCc(mVolumMidiRecallCc.load())") != std::string::npos);
}

TEST_CASE("ProcessMidiMsg is an integer-only RT handoff")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const std::string beginNeedle = "void NeuralAmpModeler::ProcessMidiMsg";
  const auto begin = source.find(beginNeedle);
  REQUIRE(begin != std::string::npos);
  const auto end = source.find("void NeuralAmpModeler::OnIdle", begin);
  REQUIRE(end != std::string::npos);
  const std::string body = source.substr(begin, end - begin);

  CHECK(body.find("DecodeMidiSoundRecall") != std::string::npos);
  CHECK(body.find("mVolumMidiRecallCc.load") != std::string::npos);
  CHECK(body.find("DecodeMidiProgramChange") == std::string::npos);
  CHECK(body.find("mVolumMidiQueue.Enqueue") != std::string::npos);
  CHECK(body.find("GlobalContentStore") == std::string::npos);
  CHECK(body.find("filesystem") == std::string::npos);
  CHECK(body.find("GetUI") == std::string::npos);
}

TEST_CASE("A Program Change moves PLAY's LIVE slot, not only the live rig")
{
  // OnIdle recalled the Sound but left mVolumLastRecalledPlaySlot on the previous
  // number, so the rail's LIVE chip and the stage overlay stayed put while the
  // amp behind them changed. Rail clicks and Up/Down already wrote the slot;
  // MIDI has to as well, or a pedalboard cannot switch the current play.
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const auto idle = source.find("void NeuralAmpModeler::OnIdle()");
  REQUIRE(idle != std::string::npos);
  const auto drain = source.find("mVolumMidiQueue.Drain()", idle);
  REQUIRE(drain != std::string::npos);
  const std::string body = source.substr(drain, 400);
  CHECK(body.find("_VolumRecallSound(sound->ampId, sound->presetId)") != std::string::npos);
  CHECK(body.find("mVolumLastRecalledPlaySlot = *slot") != std::string::npos);
}

// Closest controller mock that CI can run: the same IMidiMsg a host delivers on
// the audio thread, then the exact Decode → latest-wins queue → ResolveMidiSound
// path ProcessMidiMsg + OnIdle use. Does not construct NeuralAmpModeler.
namespace
{
struct MidiControllerApply
{
  std::optional<int> liveSlot;
  std::optional<volum::content::ResolvedMidiSound> sound;

  void Send(const IMidiMsg& msg, int savedChannel, volum::MidiLatestWinsQueue& queue,
            const volum::content::Registry& registry, int recallCc = volum::kMidiRecallCcDefault)
  {
    if (const auto slot = volum::DecodeMidiSoundRecall(msg, savedChannel, recallCc))
      queue.Enqueue(*slot);
    const auto drained = queue.Drain();
    if (!drained)
      return;
    sound = volum::content::ResolveMidiSound(registry, *drained);
    if (sound)
      liveSlot = *drained;
  }
};
} // namespace

TEST_CASE("IMidiMsg Program Change composes decode queue resolve and LIVE slot")
{
  using namespace volum::content;
  Registry registry;
  Preset lead;
  lead.id = "preset_lead";
  lead.name = "Lead";
  lead.settings.toneBass = 7.5;
  registry.presetBanks["factory:7"] = {lead};
  REQUIRE(AssignMidiSound(registry, 12, "factory:7", lead.id));
  REQUIRE(AssignMidiSound(registry, 3, "factory:2", "factory:2:v1"));
  REQUIRE(AssignMidiSound(registry, 9, "missing-amp", "gone"));

  volum::MidiLatestWinsQueue queue;
  MidiControllerApply apply;

  IMidiMsg pc;
  pc.MakeProgramChange(12, 0);
  apply.Send(pc, volum::kMidiOmniChannel, queue, registry);
  REQUIRE(apply.sound.has_value());
  CHECK(apply.liveSlot == 12);
  CHECK(apply.sound->ampId == "factory:7");
  CHECK(apply.sound->presetId == lead.id);
  CHECK(apply.sound->settings.toneBass == doctest::Approx(7.5));

  pc.MakeProgramChange(3, 4); // iPlug channel 4 = user MIDI channel 5
  apply.Send(pc, 5, queue, registry);
  REQUIRE(apply.sound.has_value());
  CHECK(apply.liveSlot == 3);
  CHECK(apply.sound->ampId == "factory:2");
  CHECK(apply.sound->presetId == "factory:2:v1");

  const auto previousLive = apply.liveSlot;
  pc.MakeProgramChange(12, 0); // channel 1, filter is channel 5
  apply.Send(pc, 5, queue, registry);
  CHECK(apply.liveSlot == previousLive); // filtered: LIVE stays

  pc.MakeProgramChange(9, 4);
  apply.Send(pc, 5, queue, registry);
  CHECK_FALSE(apply.sound.has_value()); // assigned but invalid
  CHECK(apply.liveSlot == previousLive); // OnIdle only writes LIVE on successful recall

  pc.MakeProgramChange(1, 4);
  apply.Send(pc, 5, queue, registry);
  CHECK_FALSE(apply.sound.has_value()); // unassigned
  CHECK(apply.liveSlot == previousLive);

  pc.MakeNoteOnMsg(60, 100, 0, 4);
  apply.Send(pc, 5, queue, registry);
  CHECK(apply.liveSlot == previousLive);
}

TEST_CASE("IMidiMsg recall CC composes decode queue resolve and LIVE slot")
{
  using namespace volum::content;
  Registry registry;
  Preset lead;
  lead.id = "preset_lead";
  lead.name = "Lead";
  registry.presetBanks["factory:7"] = {lead};
  REQUIRE(AssignMidiSound(registry, 12, "factory:7", lead.id));

  volum::MidiLatestWinsQueue queue;
  MidiControllerApply apply;
  const IMidiMsg cc = IMidiMsg(0, static_cast<uint8_t>(IMidiMsg::kControlChange << 4), 102, 12);
  apply.Send(cc, volum::kMidiOmniChannel, queue, registry, 102);
  REQUIRE(apply.sound.has_value());
  CHECK(apply.liveSlot == 12);
  CHECK(apply.sound->ampId == "factory:7");

  const auto previousLive = apply.liveSlot;
  const IMidiMsg otherCc = IMidiMsg(0, static_cast<uint8_t>(IMidiMsg::kControlChange << 4), 74, 12);
  apply.Send(otherCc, volum::kMidiOmniChannel, queue, registry, 102);
  CHECK(apply.liveSlot == previousLive);

  const IMidiMsg wrongCh = IMidiMsg(0, static_cast<uint8_t>((IMidiMsg::kControlChange << 4) | 0), 102, 12);
  apply.Send(wrongCh, 5, queue, registry, 102);
  CHECK(apply.liveSlot == previousLive);
}

TEST_CASE("IMidiMsg burst keeps only the newest Program Change")
{
  using namespace volum::content;
  Registry registry;
  REQUIRE(AssignMidiSound(registry, 4, "factory:1", "factory:1:v1"));
  REQUIRE(AssignMidiSound(registry, 7, "factory:2", "factory:2:v1"));

  volum::MidiLatestWinsQueue queue;
  IMidiMsg first;
  first.MakeProgramChange(4, 0);
  IMidiMsg second;
  second.MakeProgramChange(7, 0);
  REQUIRE(volum::DecodeMidiSoundRecall(first, 0, volum::kMidiRecallCcDefault).has_value());
  queue.Enqueue(*volum::DecodeMidiSoundRecall(first, 0, volum::kMidiRecallCcDefault));
  queue.Enqueue(*volum::DecodeMidiSoundRecall(second, 0, volum::kMidiRecallCcDefault));

  MidiControllerApply apply;
  // Drain once, as OnIdle does: latest-wins, no backlog.
  const auto drained = queue.Drain();
  REQUIRE(drained.has_value());
  CHECK(*drained == 7);
  apply.sound = ResolveMidiSound(registry, *drained);
  if (apply.sound)
    apply.liveSlot = *drained;
  REQUIRE(apply.sound.has_value());
  CHECK(apply.liveSlot == 7);
  CHECK(apply.sound->ampId == "factory:2");
  CHECK_FALSE(queue.Drain().has_value());
}
