#include "third_party/doctest.h"

#include "../VoLumMidi.h"

using iplug::IMidiMsg;

TEST_CASE("MIDI Program Change decoder accepts Omni slots 0 through 127")
{
  IMidiMsg msg;
  msg.MakeProgramChange(0, 7);
  REQUIRE(volum::DecodeMidiProgramChange(msg, 0).has_value());
  CHECK(*volum::DecodeMidiProgramChange(msg, 0) == 0);

  msg.MakeProgramChange(127, 15);
  REQUIRE(volum::DecodeMidiProgramChange(msg, 0).has_value());
  CHECK(*volum::DecodeMidiProgramChange(msg, 0) == 127);
}

TEST_CASE("MIDI Program Change decoder filters saved channels 1 through 16")
{
  IMidiMsg msg;
  msg.MakeProgramChange(42, 8); // iPlug channel 8 is user-facing MIDI channel 9.
  REQUIRE(volum::DecodeMidiProgramChange(msg, 9).has_value());
  CHECK(*volum::DecodeMidiProgramChange(msg, 9) == 42);
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 8).has_value());

  msg.MakeProgramChange(7, 0);
  CHECK(volum::DecodeMidiProgramChange(msg, 1).has_value());
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 16).has_value());
}

TEST_CASE("MIDI decoder ignores CC0 CC32 notes pitch bend and malformed programs")
{
  IMidiMsg msg;
  msg.MakeProgramChange(23, 0);
  REQUIRE(volum::DecodeMidiProgramChange(msg, 0) == 23);

  msg.MakeControlChangeMsg(static_cast<IMidiMsg::EControlChangeMsg>(0), 1.0, 0);
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 0).has_value());

  msg.MakeControlChangeMsg(static_cast<IMidiMsg::EControlChangeMsg>(32), 1.0, 0);
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 0).has_value());

  msg.MakeNoteOnMsg(60, 100, 0, 0);
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 0).has_value());

  msg.MakePitchWheelMsg(0.5, 0);
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 0).has_value());

  msg = IMidiMsg(0, static_cast<uint8_t>(IMidiMsg::kProgramChange << 4), 255, 0);
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 0).has_value());
  CHECK_FALSE(volum::DecodeMidiProgramChange(msg, 17).has_value());
}
