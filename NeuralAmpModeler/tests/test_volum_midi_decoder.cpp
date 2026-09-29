#include "third_party/doctest.h"

#include "../VoLumMidi.h"

using iplug::IMidiMsg;

namespace
{
IMidiMsg MakeCc(int cc, int value, int channel = 0)
{
  return IMidiMsg(0, static_cast<uint8_t>((IMidiMsg::kControlChange << 4) | channel), static_cast<uint8_t>(cc),
                  static_cast<uint8_t>(value));
}
} // namespace

TEST_CASE("MIDI Sound recall decoder accepts Omni Program Change slots 0 through 127")
{
  IMidiMsg msg;
  msg.MakeProgramChange(0, 7);
  REQUIRE(volum::DecodeMidiSoundRecall(msg, 0, volum::kMidiRecallCcDefault).has_value());
  CHECK(*volum::DecodeMidiSoundRecall(msg, 0, volum::kMidiRecallCcDefault) == 0);

  msg.MakeProgramChange(127, 15);
  REQUIRE(volum::DecodeMidiSoundRecall(msg, 0, volum::kMidiRecallCcDefault).has_value());
  CHECK(*volum::DecodeMidiSoundRecall(msg, 0, volum::kMidiRecallCcDefault) == 127);
}

TEST_CASE("MIDI Sound recall decoder accepts the configured CC with value as the slot")
{
  CHECK(*volum::DecodeMidiSoundRecall(MakeCc(102, 0), 0, 102) == 0);
  CHECK(*volum::DecodeMidiSoundRecall(MakeCc(102, 127), 0, 102) == 127);
  CHECK(*volum::DecodeMidiSoundRecall(MakeCc(102, 42), 0, 102) == 42);
  CHECK(*volum::DecodeMidiSoundRecall(MakeCc(20, 7), 0, 20) == 7);
}

TEST_CASE("MIDI Sound recall decoder ignores a CC that is not the configured recall CC")
{
  CHECK_FALSE(volum::DecodeMidiSoundRecall(MakeCc(74, 12), 0, 102).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(MakeCc(103, 12), 0, 102).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(MakeCc(0, 12), 0, 102).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(MakeCc(32, 12), 0, 102).has_value());
}

TEST_CASE("MIDI Sound recall decoder filters saved channels 1 through 16 for Program Change and CC")
{
  IMidiMsg pc;
  pc.MakeProgramChange(42, 8); // iPlug channel 8 is user-facing MIDI channel 9.
  REQUIRE(volum::DecodeMidiSoundRecall(pc, 9, 102).has_value());
  CHECK(*volum::DecodeMidiSoundRecall(pc, 9, 102) == 42);
  CHECK_FALSE(volum::DecodeMidiSoundRecall(pc, 8, 102).has_value());

  pc.MakeProgramChange(7, 0);
  CHECK(volum::DecodeMidiSoundRecall(pc, 1, 102).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(pc, 16, 102).has_value());

  const IMidiMsg ccMatch = MakeCc(102, 11, 8);
  CHECK(*volum::DecodeMidiSoundRecall(ccMatch, 9, 102) == 11);
  CHECK_FALSE(volum::DecodeMidiSoundRecall(ccMatch, 8, 102).has_value());

  const IMidiMsg ccCh1 = MakeCc(102, 3, 0);
  CHECK(volum::DecodeMidiSoundRecall(ccCh1, 1, 102).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(ccCh1, 16, 102).has_value());
}

TEST_CASE("MIDI Sound recall decoder ignores notes pitch bend and malformed programs")
{
  IMidiMsg msg;
  msg.MakeProgramChange(23, 0);
  REQUIRE(volum::DecodeMidiSoundRecall(msg, 0, 102) == 23);

  msg.MakeNoteOnMsg(60, 100, 0, 0);
  CHECK_FALSE(volum::DecodeMidiSoundRecall(msg, 0, 102).has_value());

  msg.MakePitchWheelMsg(0.5, 0);
  CHECK_FALSE(volum::DecodeMidiSoundRecall(msg, 0, 102).has_value());

  msg = IMidiMsg(0, static_cast<uint8_t>(IMidiMsg::kProgramChange << 4), 255, 0);
  CHECK_FALSE(volum::DecodeMidiSoundRecall(msg, 0, 102).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(msg, 17, 102).has_value());
}

TEST_CASE("MIDI Sound recall CC 120-127 cannot be configured")
{
  CHECK(volum::kMidiRecallCcDefault == 102);
  CHECK(volum::kMidiRecallCcMin == 0);
  CHECK(volum::kMidiRecallCcMax == 119);
  CHECK(volum::ClampMidiRecallCc(0) == 0);
  CHECK(volum::ClampMidiRecallCc(119) == 119);
  CHECK(volum::ClampMidiRecallCc(102) == 102);
  CHECK(volum::ClampMidiRecallCc(120) == volum::kMidiRecallCcDefault);
  CHECK(volum::ClampMidiRecallCc(123) == volum::kMidiRecallCcDefault);
  CHECK(volum::ClampMidiRecallCc(127) == volum::kMidiRecallCcDefault);
  CHECK(volum::ClampMidiRecallCc(-1) == volum::kMidiRecallCcDefault);
  CHECK(volum::ClampMidiRecallCc(200) == volum::kMidiRecallCcDefault);

  // Even if a caller skipped Clamp, the decoder refuses a channel-mode CC.
  CHECK_FALSE(volum::DecodeMidiSoundRecall(MakeCc(123, 4), 0, 123).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(MakeCc(120, 4), 0, 120).has_value());
  CHECK_FALSE(volum::DecodeMidiSoundRecall(MakeCc(127, 4), 0, 127).has_value());
}

TEST_CASE("DecodeMidiProgramChange is a thin wrapper around DecodeMidiSoundRecall")
{
  IMidiMsg pc;
  pc.MakeProgramChange(9, 2);
  CHECK(volum::DecodeMidiProgramChange(pc, 0) == volum::DecodeMidiSoundRecall(pc, 0, volum::kMidiRecallCcDefault));
  CHECK(volum::DecodeMidiProgramChange(MakeCc(102, 5), 0)
        == volum::DecodeMidiSoundRecall(MakeCc(102, 5), 0, volum::kMidiRecallCcDefault));
}
