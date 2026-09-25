#pragma once

// VoLum MIDI-in primitives. This header deliberately knows nothing about the
// content store, filesystem, or UI so ProcessMidiMsg can stay a bounded RT path.

#include "../iPlug2/IPlug/IPlugMidi.h"

#include <algorithm>
#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace volum
{

inline constexpr int kMidiSoundSlotCount = 128;
inline constexpr int kMidiOmniChannel = 0;
inline constexpr int kMidiChannelCount = 16;
// MIDI leaves 102-119 undefined; 102 is therefore the default Sound-recall CC.
// 120-127 are channel-mode (All Notes Off is 123) and hosts consume them.
inline constexpr int kMidiRecallCcDefault = 102;
inline constexpr int kMidiRecallCcMin = 0;
inline constexpr int kMidiRecallCcMax = 119;

// Anything outside 0-119, including the channel-mode band, snaps to the default
// rather than onto 119: a stored 123 must not silently become a working CC.
inline int ClampMidiRecallCc(int cc)
{
  if (cc < kMidiRecallCcMin || cc > kMidiRecallCcMax)
    return kMidiRecallCcDefault;
  return cc;
}

// User-facing saved channel: 0 = Omni, 1..16 = one MIDI channel. iPlug exposes
// incoming channels as 0..15. Program Change and the recall CC share this
// function so the channel filter and the 0-127 slot range cannot diverge.
inline std::optional<int> DecodeMidiSoundRecall(const iplug::IMidiMsg& msg, int savedChannel, int recallCc)
{
  if (savedChannel < kMidiOmniChannel || savedChannel > kMidiChannelCount)
    return std::nullopt;
  if (savedChannel != kMidiOmniChannel && msg.Channel() != savedChannel - 1)
    return std::nullopt;

  int slot = -1;
  switch (msg.StatusMsg())
  {
    case iplug::IMidiMsg::kProgramChange: slot = msg.Program(); break;
    case iplug::IMidiMsg::kControlChange:
      if (recallCc < kMidiRecallCcMin || recallCc > kMidiRecallCcMax)
        return std::nullopt;
      if (static_cast<int>(msg.ControlChangeIdx()) != recallCc)
        return std::nullopt;
      slot = static_cast<int>(msg.mData2);
      break;
    default: return std::nullopt;
  }
  if (slot < 0 || slot >= kMidiSoundSlotCount)
    return std::nullopt;
  return slot;
}

inline std::optional<int> DecodeMidiProgramChange(const iplug::IMidiMsg& msg, int savedChannel)
{
  return DecodeMidiSoundRecall(msg, savedChannel, kMidiRecallCcDefault);
}

// Capacity-one audio -> main handoff. A burst intentionally overwrites the
// pending value: recalling the newest requested Sound is preferable to queuing
// a backlog of expensive model loads.
class MidiLatestWinsQueue
{
public:
  void Enqueue(int slot)
  {
    if (slot >= 0 && slot < kMidiSoundSlotCount)
      mSlot.store(slot, std::memory_order_release);
  }

  std::optional<int> Drain()
  {
    const int slot = mSlot.exchange(kEmpty, std::memory_order_acq_rel);
    return slot == kEmpty ? std::nullopt : std::optional<int>(slot);
  }

private:
  static constexpr int kEmpty = -1;
  std::atomic<int> mSlot{kEmpty};
};

// A slot's Sound as the RT-side decoder describes it: `ampId` is factory:<idx> or
// a custom amp id, `presetId` a User preset id or a shipped Factory preset id.
// The stored library keys these by slot (content::Registry::midiSoundMap), so this
// carries the slot with it and never owns the collection.
struct MidiSound
{
  int slot = -1;
  std::string ampId;
  std::string presetId;
};

inline int FactoryAmpIndexFromId(const std::string& ampId)
{
  constexpr const char* prefix = "factory:";
  if (ampId.rfind(prefix, 0) != 0 || ampId.size() == 8)
    return -1;
  int value = 0;
  for (std::size_t i = 8; i < ampId.size(); ++i)
  {
    const char c = ampId[i];
    if (c < '0' || c > '9')
      return -1;
    value = value * 10 + (c - '0');
    if (value >= 1000)
      return -1;
  }
  return value;
}

} // namespace volum
