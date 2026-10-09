#include "third_party/doctest.h"

#include "../VoLumMidi.h"
#include "../../iPlug2/IPlug/VST3/IPlugVST3_MidiParamRouter.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using iplug::IMidiMsg;
using iplug::VST3MidiParamRouter;
using Steinberg::int32;
using Steinberg::tresult;
using Steinberg::uint32;
using Steinberg::Vst::ParamID;
using Steinberg::Vst::ParamValue;
using Steinberg::Vst::ProcessContext;
using Steinberg::Vst::ProcessData;

namespace
{
std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::filesystem::path Vst3Dir()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2" / "IPlug" / "VST3";
}

// The three ways hosts compute data / 127 for a VST3 MIDI parameter.
double FloatQuotient(int data)
{
  return static_cast<float>(data) / 127.f;
}
double FloatRoundedDouble(int data)
{
  return static_cast<float>(data / 127.);
}
double DoubleQuotient(int data)
{
  return data / 127.;
}

// The decode the processor used before 1.3.0, kept here only to show what it got wrong.
int TruncatingDecode(double value)
{
  return static_cast<int>(value * 127.);
}

constexpr int kProgramCh1 = iplug::kVST3MIDIProgramParamStartIdx;
constexpr int kLegacyProgramCh1 = iplug::kMIDICCParamStartIdx + Steinberg::Vst::kCtrlProgramChange;

// One point of one parameter, as a host's IParamValueQueue hands it to process().
class HostParamQueue : public Steinberg::Vst::IParamValueQueue
{
public:
  HostParamQueue(ParamID id, ParamValue value)
  : mId(id)
  , mValue(value)
  {
  }

  ParamID PLUGIN_API getParameterId() override { return mId; }
  int32 PLUGIN_API getPointCount() override { return 1; }
  tresult PLUGIN_API getPoint(int32 index, int32& sampleOffset, ParamValue& value) override
  {
    if (index != 0)
      return Steinberg::kResultFalse;
    sampleOffset = 0;
    value = mValue;
    return Steinberg::kResultTrue;
  }
  tresult PLUGIN_API addPoint(int32, ParamValue, int32&) override { return Steinberg::kResultFalse; }
  tresult PLUGIN_API queryInterface(const Steinberg::TUID, void** obj) override
  {
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }

private:
  ParamID mId;
  ParamValue mValue;
};

class HostParamChanges : public Steinberg::Vst::IParameterChanges
{
public:
  void Add(ParamID id, ParamValue value) { mQueues.emplace_back(id, value); }

  int32 PLUGIN_API getParameterCount() override { return static_cast<int32>(mQueues.size()); }
  Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(int32 index) override
  {
    return index >= 0 && index < getParameterCount() ? &mQueues[index] : nullptr;
  }
  Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) override { return nullptr; }
  tresult PLUGIN_API queryInterface(const Steinberg::TUID, void** obj) override
  {
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }

private:
  std::vector<HostParamQueue> mQueues;
};

enum class ETransport
{
  kStopped,
  kPlaying,
  kNoContext
};

// The ProcessData of one process() call.
struct HostBlock
{
  explicit HostBlock(int frames, ETransport transport = ETransport::kStopped,
                     std::vector<std::pair<int, double>> params = {})
  {
    for (const auto& [id, value] : params)
      changes.Add(static_cast<ParamID>(id), value);
    context.sampleRate = 48000.;
    context.state = transport == ETransport::kPlaying ? ProcessContext::kPlaying : 0;
    data.processMode = Steinberg::Vst::kRealtime;
    data.symbolicSampleSize = Steinberg::Vst::kSample32;
    data.numSamples = frames;
    data.inputParameterChanges = &changes;
    data.processContext = transport == ETransport::kNoContext ? nullptr : &context;
  }

  HostParamChanges changes;
  ProcessContext context{};
  ProcessData data;
};

// IPlugVST3ProcessorBase's use of the router: ProcessParameterChanges before the audio, EndBlock after it,
// Arm from setState. Plug-in parameter changes and VoLum Sound recalls are recorded.
struct Vst3ProcessorHarness
{
  static constexpr double kSampleRate = 48000.;
  static constexpr int kBlock = 512;

  VST3MidiParamRouter router;
  volum::MidiLatestWinsQueue recalls;
  std::vector<int> plugParams;
  std::vector<IMidiMsg> midi;
  int64_t nowNs = 1'000'000'000;
  bool renderingOffline = false;

  void SetState() { router.Arm(); }

  void BeginProcess(HostBlock& block)
  {
    router.ProcessParameterChanges(
      block.data, renderingOffline, nowNs, [&](int idx, double, int32) { plugParams.push_back(idx); },
      [&](const IMidiMsg& msg) {
        midi.push_back(msg);
        if (const auto slot = volum::DecodeMidiSoundRecall(msg, 1, volum::kMidiRecallCcDefault))
          recalls.Enqueue(*slot);
      });
  }

  void EndProcess(HostBlock& block)
  {
    router.EndBlock(block.data, kSampleRate);
    Wait(block.data.numSamples / kSampleRate);
  }

  void Process(HostBlock& block)
  {
    BeginProcess(block);
    EndProcess(block);
  }

  void Process(HostBlock&& block) { Process(block); }

  void Wait(double seconds) { nowNs += static_cast<int64_t>(seconds * 1e9); }

  void ProcessSilence(double seconds, ETransport transport = ETransport::kStopped)
  {
    for (int i = 0; i < static_cast<int>(seconds * kSampleRate / kBlock) + 1; ++i)
      Process(HostBlock(kBlock, transport));
  }
};
} // namespace

TEST_CASE("VST3 MIDI value decode returns every 7-bit value from float and double normalized values")
{
  for (int data = 0; data <= 127; ++data)
  {
    CAPTURE(data);
    CHECK(iplug::VST3NormalizedToMIDI7Bit(FloatQuotient(data)) == data);
    CHECK(iplug::VST3NormalizedToMIDI7Bit(FloatRoundedDouble(data)) == data);
    CHECK(iplug::VST3NormalizedToMIDI7Bit(DoubleQuotient(data)) == data);
  }
}

TEST_CASE("VST3 MIDI value decode fixes the 7-bit values the old truncating decode lost from float")
{
  int truncatingWrong = 0;
  int roundingWrong = 0;
  for (int data = 0; data <= 127; ++data)
  {
    truncatingWrong += TruncatingDecode(FloatQuotient(data)) != data;
    roundingWrong += iplug::VST3NormalizedToMIDI7Bit(FloatQuotient(data)) != data;
  }
  CHECK(TruncatingDecode(FloatQuotient(1)) == 0);
  CHECK(truncatingWrong == 67);
  CHECK(roundingWrong == 0);
}

TEST_CASE("VST3 MIDI value decode clamps out-of-range and NaN values")
{
  CHECK(iplug::VST3NormalizedToMIDI7Bit(-0.25) == 0);
  CHECK(iplug::VST3NormalizedToMIDI7Bit(1.5) == 127);
  CHECK(iplug::VST3NormalizedToMIDI7Bit(std::numeric_limits<double>::quiet_NaN()) == 0);
}

TEST_CASE("VST3 program-change parameters sit after the CC block of all 16 channels")
{
  CHECK(iplug::kVST3MIDIParamsPerChannel == 131);
  CHECK(iplug::kVST3MIDIProgramParamStartIdx
        == iplug::kMIDICCParamStartIdx + iplug::kVST3MaxMIDIChannels * iplug::kVST3MIDIParamsPerChannel);
  CHECK(iplug::VST3MIDIProgramParamChannel(iplug::kVST3MIDIProgramParamStartIdx - 1, 16) == -1);
  CHECK(iplug::VST3MIDIProgramParamChannel(iplug::kVST3MIDIProgramParamStartIdx, 16) == 0);
  CHECK(iplug::VST3MIDIProgramParamChannel(iplug::kVST3MIDIProgramParamStartIdx + 15, 16) == 15);
  CHECK(iplug::VST3MIDIProgramParamChannel(iplug::kVST3MIDIProgramParamStartIdx + 16, 16) == -1);
  CHECK(iplug::VST3MIDIProgramParamChannel(iplug::kMIDICCParamStartIdx, 16) == -1);
}

TEST_CASE("VST3 wrapper: Program Change 1 sent as float 1/127 recalls Sound slot 1, every program on every channel")
{
  Vst3ProcessorHarness processor;
  processor.ProcessSilence(1.);
  REQUIRE_FALSE(processor.router.IsArmed());

  for (int channel = 0; channel < iplug::kVST3MaxMIDIChannels; ++channel)
  {
    for (int program = 0; program < iplug::kVST3MIDIProgramCount; ++program)
    {
      CAPTURE(channel);
      CAPTURE(program);
      processor.midi.clear();
      processor.Process(HostBlock(
        Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1 + channel, FloatQuotient(program)}}));
      REQUIRE(processor.midi.size() == 1);
      CHECK(processor.midi[0].Channel() == channel);
      const auto slot = volum::DecodeMidiSoundRecall(processor.midi[0], channel + 1, volum::kMidiRecallCcDefault);
      REQUIRE(slot.has_value());
      CHECK(*slot == program);
    }
  }
}

TEST_CASE("VST3 wrapper: aftertouch, recall CC and the IMidiMapping Program Change keep their value from float")
{
  Vst3ProcessorHarness processor;
  processor.ProcessSilence(1.);

  for (int data = 0; data <= 127; ++data)
  {
    CAPTURE(data);
    processor.midi.clear();
    const int ch3 = iplug::kMIDICCParamStartIdx + 2 * iplug::kVST3MIDIParamsPerChannel;
    processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped,
                                {{ch3 + Steinberg::Vst::kAfterTouch, FloatQuotient(data)},
                                 {iplug::kMIDICCParamStartIdx + volum::kMidiRecallCcDefault, FloatQuotient(data)},
                                 {kLegacyProgramCh1, FloatQuotient(data)}}));
    REQUIRE(processor.midi.size() == 3);
    CHECK(processor.midi[0].Channel() == 2);
    CHECK(processor.midi[0].ChannelAfterTouch() == data);
    CHECK(volum::DecodeMidiSoundRecall(processor.midi[1], 0, volum::kMidiRecallCcDefault) == data);
    CHECK(processor.midi[2].StatusMsg() == IMidiMsg::kProgramChange);
    CHECK(processor.midi[2].Program() == data);
  }
}

TEST_CASE("VST3 wrapper: a project reload's program value is absorbed, a later Program Change recalls")
{
  Vst3ProcessorHarness processor;

  SUBCASE("fresh instance, REAPER restores the last program in the first block")
  {
    processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(5)}}));
  }
  SUBCASE("setState, then Cubase's program 0 a few blocks later, on both program paths")
  {
    processor.ProcessSilence(2.);
    processor.SetState();
    processor.ProcessSilence(0.02);
    processor.Process(HostBlock(
      Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, 0.}, {kLegacyProgramCh1, 0.}, {3, 0.25}}));
    CHECK(processor.plugParams == std::vector<int>{3});
  }
  CHECK_FALSE(processor.recalls.Drain().has_value());
  CHECK(processor.midi.empty());

  processor.ProcessSilence(VST3MidiParamRouter::kWindowSeconds);
  CHECK_FALSE(processor.router.IsArmed());
  processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(3)}}));
  CHECK(processor.recalls.Drain() == 3);
}

TEST_CASE("VST3 wrapper: a block without ProcessContext after setState counts as stopped")
{
  Vst3ProcessorHarness processor;
  processor.ProcessSilence(1., ETransport::kPlaying);
  processor.SetState();

  SUBCASE("no ProcessContext")
  {
    processor.Process(HostBlock(0, ETransport::kNoContext, {{kProgramCh1, FloatQuotient(5)}}));
  }
  SUBCASE("ProcessContext without kPlaying")
  {
    processor.Process(HostBlock(0, ETransport::kStopped, {{kProgramCh1, FloatQuotient(5)}}));
  }
  CHECK_FALSE(processor.recalls.Drain().has_value());
  CHECK(processor.router.IsArmed());
}

TEST_CASE("VST3 wrapper: Program Change recalls during playback and offline render right after setState")
{
  Vst3ProcessorHarness processor;
  processor.SetState();

  SUBCASE("playing")
  {
    processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kPlaying, {{kProgramCh1, FloatQuotient(7)}}));
  }
  SUBCASE("offline process mode")
  {
    HostBlock block(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(7)}});
    block.data.processMode = Steinberg::Vst::kOffline;
    processor.Process(block);
  }
  SUBCASE("offline setup")
  {
    processor.renderingOffline = true;
    processor.Process(
      HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kNoContext, {{kProgramCh1, FloatQuotient(7)}}));
  }
  CHECK(processor.recalls.Drain() == 7);

  // The first playing or offline block ends the restore window, so a stopped-transport change recalls too.
  CHECK_FALSE(processor.router.IsArmed());
  processor.renderingOffline = false;
  processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(9)}}));
  CHECK(processor.recalls.Drain() == 9);
}

TEST_CASE("VST3 wrapper: the same Program Change twice recalls twice when the host delivers it twice")
{
  Vst3ProcessorHarness processor;
  processor.ProcessSilence(1.);

  processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(4)}}));
  CHECK(processor.recalls.Drain() == 4);
  processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(4)}}));
  CHECK(processor.recalls.Drain() == 4);
  processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kPlaying, {{kProgramCh1, FloatQuotient(4)}}));
  CHECK(processor.recalls.Drain() == 4);
}

TEST_CASE("VST3 wrapper: setState during a block is not cleared by that block's end")
{
  Vst3ProcessorHarness processor;
  processor.ProcessSilence(1.);
  REQUIRE_FALSE(processor.router.IsArmed());

  SUBCASE("a stopped block long enough to end the window")
  {
    HostBlock block(static_cast<int>(Vst3ProcessorHarness::kSampleRate), ETransport::kStopped);
    processor.BeginProcess(block);
    processor.SetState();
    processor.EndProcess(block);
  }
  SUBCASE("a playing block")
  {
    HostBlock block(Vst3ProcessorHarness::kBlock, ETransport::kPlaying);
    processor.BeginProcess(block);
    processor.SetState();
    processor.EndProcess(block);
  }
  CHECK(processor.router.IsArmed());
  processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(5)}}));
  CHECK_FALSE(processor.recalls.Drain().has_value());
}

TEST_CASE("VST3 wrapper: zero-frame flushes and suspended processing end the restore window")
{
  Vst3ProcessorHarness processor;
  processor.SetState();

  SUBCASE("zero-frame parameter flushes")
  {
    for (int i = 0; i < 6; ++i)
    {
      processor.Process(HostBlock(0, ETransport::kNoContext));
      processor.Wait(0.1);
    }
  }
  SUBCASE("processing suspended for five minutes")
  {
    processor.Process(HostBlock(Vst3ProcessorHarness::kBlock));
    processor.Wait(300.);
  }
  processor.Process(HostBlock(0, ETransport::kNoContext, {{kProgramCh1, FloatQuotient(6)}}));
  CHECK(processor.recalls.Drain() == 6);
  CHECK_FALSE(processor.router.IsArmed());
}

TEST_CASE("VST3 wrapper: the restore window starts at the first block after setState")
{
  // Hosts deliver restore values in the first blocks they process, which may come long after setState
  // in a large project, so the window is measured from that first block.
  Vst3ProcessorHarness processor;
  processor.SetState();
  processor.Wait(30.);
  processor.Process(HostBlock(Vst3ProcessorHarness::kBlock, ETransport::kStopped, {{kProgramCh1, FloatQuotient(5)}}));
  CHECK_FALSE(processor.recalls.Drain().has_value());

  processor.SetState();
  processor.ProcessSilence(VST3MidiParamRouter::kWindowSeconds / 4.);
  CHECK(processor.router.IsArmed());
  processor.ProcessSilence(VST3MidiParamRouter::kWindowSeconds);
  CHECK_FALSE(processor.router.IsArmed());
}

TEST_CASE("VST3 processor and plug-in state restore are wired to the MIDI parameter router")
{
  const std::string processor = ReadText(Vst3Dir() / "IPlugVST3_ProcessorBase.cpp");
  REQUIRE_FALSE(processor.empty());
  CHECK(processor.find("(int) (value * 127.)") == std::string::npos);
  CHECK(processor.find("mMidiParamRouter.ProcessParameterChanges(data, GetRenderingOffline(), "
                       "VST3MidiParamRouter::NowNs(),")
        != std::string::npos);
  const auto paramChanges = processor.find("ProcessParameterChanges(data, fromProcessor);");
  const auto audio = processor.find("ProcessAudio(data, setup, ins, outs);");
  const auto endBlock = processor.find("mMidiParamRouter.EndBlock(data, setup.sampleRate);");
  REQUIRE(paramChanges != std::string::npos);
  REQUIRE(audio != std::string::npos);
  REQUIRE(endBlock != std::string::npos);
  CHECK(paramChanges < audio);
  CHECK(audio < endBlock);
  CHECK(
    ReadText(Vst3Dir() / "IPlugVST3_ProcessorBase.h").find("void ArmProgramRestoreGuard() { mMidiParamRouter.Arm(); }")
    != std::string::npos);

  for (const char* file : {"IPlugVST3.cpp", "IPlugVST3_Processor.cpp"})
  {
    CAPTURE(file);
    const std::string src = ReadText(Vst3Dir() / file);
    const auto arm = src.find("ArmProgramRestoreGuard();");
    const auto setState = src.find("IPlugVST3State::SetState(this, pState)");
    REQUIRE(arm != std::string::npos);
    REQUIRE(setState != std::string::npos);
    CHECK(arm < setState);
  }
}

TEST_CASE("VST3 controller gives each MIDI channel unit a program list and maps the bus to it")
{
  const std::string controller = ReadText(Vst3Dir() / "IPlugVST3_ControllerBase.h");
  REQUIRE_FALSE(controller.empty());
  CHECK(controller.find("unitInfo.programListId = kVST3MIDIProgramParamStartIdx + chan;") != std::string::npos);
  CHECK(controller.find("new IPlugVST3MIDIProgramParameter(chan, unitID)") != std::string::npos);
  CHECK(controller.find("mMIDIProgramUnitIDs[chan] = unitID;") != std::string::npos);
  CHECK(controller.find("type == Steinberg::Vst::kEvent && dir == Steinberg::Vst::kInput") != std::string::npos);
  CHECK(controller.find("info.programCount = kVST3MIDIProgramCount;") != std::string::npos);

  const std::string parameter = ReadText(Vst3Dir() / "IPlugVST3_Parameter.h");
  const auto programParamClass = parameter.find("class IPlugVST3MIDIProgramParameter");
  REQUIRE(programParamClass != std::string::npos);
  const auto programParamEnd = parameter.find("\n};", programParamClass);
  REQUIRE(programParamEnd != std::string::npos);
  const std::string programParamBody = parameter.substr(programParamClass, programParamEnd - programParamClass);
  CHECK(programParamBody.find("ParameterInfo::kIsProgramChange") != std::string::npos);
  CHECK(programParamBody.find("ParameterInfo::kCanAutomate") == std::string::npos);
  CHECK(programParamBody.find("info.stepCount = kVST3MIDIProgramCount - 1;") != std::string::npos);
  CHECK(programParamBody.find("info.id = kVST3MIDIProgramParamStartIdx + channel;") != std::string::npos);

  for (const char* file : {"IPlugVST3.h", "IPlugVST3_Controller.h"})
  {
    CAPTURE(file);
    CHECK(ReadText(Vst3Dir() / file).find("return GetUnitByBus(type, dir, busIndex, channel, unitId);")
          != std::string::npos);
  }
}

TEST_CASE("VST3 IMidiMapping refuses controller numbers past Program Change")
{
  for (const char* file : {"IPlugVST3.cpp", "IPlugVST3_Controller.cpp"})
  {
    CAPTURE(file);
    CHECK(ReadText(Vst3Dir() / file).find("midiCCNumber >= 0 && midiCCNumber <= kCtrlProgramChange")
          != std::string::npos);
  }
}
