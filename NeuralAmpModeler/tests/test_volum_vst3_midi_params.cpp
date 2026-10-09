#include "third_party/doctest.h"

#include "../VoLumMidi.h"
#include "../../iPlug2/IPlug/VST3/IPlugVST3_MidiParams.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

using iplug::IMidiMsg;

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

// IPlugVST3ProcessorBase's program-parameter path for one channel, feeding VoLum's recall decoder and handoff.
struct ProgramParamProcessor
{
  static constexpr double kSampleRate = 48000.;
  static constexpr int kBlock = 512;

  iplug::VST3ProgramRestoreGuard guard;
  volum::MidiLatestWinsQueue recalls;

  void SetState() { guard.Arm(); }

  // One process() call carrying a host change of the channel-1 program parameter, or none if program < 0.
  void Process(int program, bool transportRunning = false, bool offline = false)
  {
    if (program >= 0)
    {
      IMidiMsg msg;
      if (iplug::VST3ProgramParamToMidi(
            iplug::kVST3MIDIProgramParamStartIdx, FloatQuotient(program), 0, transportRunning, offline, guard, msg)
          == iplug::EVST3ProgramParamResult::kProgramChange)
      {
        if (const auto slot = volum::DecodeMidiSoundRecall(msg, 1, volum::kMidiRecallCcDefault))
          recalls.Enqueue(*slot);
      }
    }
    guard.EndBlock(kBlock, kSampleRate, transportRunning, offline);
  }

  void ProcessSilence(double seconds, bool transportRunning = false)
  {
    for (int i = 0; i < static_cast<int>(seconds * kSampleRate / kBlock) + 1; ++i)
      Process(-1, transportRunning);
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

TEST_CASE("VST3 Program Change 1 sent as float 1/127 recalls Sound slot 1, every program on every channel")
{
  for (int channel = 0; channel < iplug::kVST3MaxMIDIChannels; ++channel)
  {
    const int paramId = iplug::kVST3MIDIProgramParamStartIdx + channel;
    for (int program = 0; program < iplug::kVST3MIDIProgramCount; ++program)
    {
      CAPTURE(channel);
      CAPTURE(program);
      // Same steps as IPlugVST3ProcessorBase::ProcessParameterChanges for a program-list parameter.
      const int decodedChannel = iplug::VST3MIDIProgramParamChannel(paramId, iplug::kVST3MaxMIDIChannels);
      REQUIRE(decodedChannel == channel);
      IMidiMsg msg;
      msg.MakeProgramChange(iplug::VST3NormalizedToMIDI7Bit(FloatQuotient(program)), decodedChannel);
      const auto slot = volum::DecodeMidiSoundRecall(msg, channel + 1, volum::kMidiRecallCcDefault);
      REQUIRE(slot.has_value());
      CHECK(*slot == program);
    }
  }
}

TEST_CASE("VST3 channel aftertouch and recall CC keep their value from a float normalized value")
{
  for (int data = 0; data <= 127; ++data)
  {
    CAPTURE(data);
    IMidiMsg at;
    at.MakeChannelATMsg(iplug::VST3NormalizedToMIDI7Bit(FloatQuotient(data)), 0, 3);
    CHECK(at.ChannelAfterTouch() == data);

    const IMidiMsg cc(0, static_cast<uint8_t>(IMidiMsg::kControlChange << 4),
                      static_cast<uint8_t>(volum::kMidiRecallCcDefault),
                      static_cast<uint8_t>(iplug::VST3NormalizedToMIDI7Bit(FloatQuotient(data))));
    CHECK(volum::DecodeMidiSoundRecall(cc, 0, volum::kMidiRecallCcDefault) == data);
  }
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

TEST_CASE("VST3 project reload: a host restoring the program parameter does not overwrite the restored Sound")
{
  ProgramParamProcessor processor;

  SUBCASE("fresh instance, REAPER restores the last program before the first block")
  {
    processor.Process(5);
    CHECK_FALSE(processor.recalls.Drain().has_value());
  }
  SUBCASE("setState, then Cubase's program 0 a few blocks later")
  {
    processor.ProcessSilence(2.);
    processor.SetState();
    processor.Process(-1);
    processor.Process(-1);
    processor.Process(0);
    CHECK_FALSE(processor.recalls.Drain().has_value());
  }

  processor.ProcessSilence(iplug::VST3ProgramRestoreGuard::kWindowSeconds);
  CHECK_FALSE(processor.guard.IsArmed());
  processor.Process(3);
  CHECK(processor.recalls.Drain() == 3);
}

TEST_CASE("VST3 Program Change from MIDI recalls during playback and offline render, even right after setState")
{
  ProgramParamProcessor processor;
  processor.SetState();

  SUBCASE("playing")
  {
    processor.Process(7, true);
    CHECK(processor.recalls.Drain() == 7);
  }
  SUBCASE("offline render")
  {
    processor.Process(7, false, true);
    CHECK(processor.recalls.Drain() == 7);
  }

  // The first playing or offline block ends the restore window, so a stopped-transport change recalls too.
  CHECK_FALSE(processor.guard.IsArmed());
  processor.Process(9);
  CHECK(processor.recalls.Drain() == 9);
}

TEST_CASE("VST3 the same Program Change twice recalls twice when the host delivers it twice")
{
  ProgramParamProcessor processor;
  processor.ProcessSilence(1.);

  processor.Process(4);
  CHECK(processor.recalls.Drain() == 4);
  processor.Process(4);
  CHECK(processor.recalls.Drain() == 4);
  processor.Process(4, true);
  CHECK(processor.recalls.Drain() == 4);
}

TEST_CASE("VST3 restore guard re-arms on every setState and only for the restore window")
{
  ProgramParamProcessor processor;
  CHECK(processor.guard.IsArmed());
  processor.ProcessSilence(iplug::VST3ProgramRestoreGuard::kWindowSeconds);
  CHECK_FALSE(processor.guard.IsArmed());

  processor.SetState();
  CHECK(processor.guard.IsArmed());
  processor.ProcessSilence(iplug::VST3ProgramRestoreGuard::kWindowSeconds / 4.);
  CHECK(processor.guard.IsArmed());
  processor.ProcessSilence(iplug::VST3ProgramRestoreGuard::kWindowSeconds);
  CHECK_FALSE(processor.guard.IsArmed());

  IMidiMsg msg;
  CHECK(
    iplug::VST3ProgramParamToMidi(iplug::kVST3MIDIProgramParamStartIdx - 1, 0.5, 0, false, false, processor.guard, msg)
    == iplug::EVST3ProgramParamResult::kNotProgramParam);
}

TEST_CASE("VST3 processor and plug-in state restore are wired to the program restore guard")
{
  const std::string processor = ReadText(Vst3Dir() / "IPlugVST3_ProcessorBase.cpp");
  REQUIRE_FALSE(processor.empty());
  CHECK(
    processor.find("VST3ProgramParamToMidi(idx, value, offsetSamples, GetTransportIsRunning(), GetRenderingOffline(), "
                   "mProgramRestoreGuard, msg)")
    != std::string::npos);
  CHECK(processor.find("mProgramRestoreGuard.Absorbs(GetTransportIsRunning(), GetRenderingOffline())")
        != std::string::npos);
  CHECK(processor.find("mProgramRestoreGuard.EndBlock(data.numSamples, setup.sampleRate") != std::string::npos);

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

TEST_CASE("VST3 processor decodes Program Change, aftertouch and CC with the rounding helper")
{
  const std::string src = ReadText(Vst3Dir() / "IPlugVST3_ProcessorBase.cpp");
  REQUIRE_FALSE(src.empty());
  CHECK(src.find("(int) (value * 127.)") == std::string::npos);
  CHECK(src.find("MakeChannelATMsg(VST3NormalizedToMIDI7Bit(value)") != std::string::npos);
  CHECK(src.find("MakeProgramChange(VST3NormalizedToMIDI7Bit(value), channel") != std::string::npos);
  CHECK(src.find("MakeControlChangeMsg((IMidiMsg::EControlChangeMsg) ctrlr, value") == std::string::npos);

  const auto programParam = src.find("VST3MIDIProgramParamChannel(idx, kVST3MaxMIDIChannels) >= 0");
  const auto ccBlock = src.find("idx >= kMIDICCParamStartIdx && idx < kVST3MIDIProgramParamStartIdx");
  REQUIRE(programParam != std::string::npos);
  REQUIRE(ccBlock != std::string::npos);
  CHECK(programParam < ccBlock);
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
