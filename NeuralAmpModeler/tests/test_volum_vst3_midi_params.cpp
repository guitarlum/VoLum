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
double FloatQuotient(int data) { return static_cast<float>(data) / 127.f; }
double FloatRoundedDouble(int data) { return static_cast<float>(data / 127.); }
double DoubleQuotient(int data) { return data / 127.; }
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
  const std::string programParamBody = parameter.substr(programParamClass, 1500);
  CHECK(programParamBody.find("ParameterInfo::kIsProgramChange") != std::string::npos);
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
