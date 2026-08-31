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

// User-facing saved channel: 0 = Omni, 1..16 = one MIDI channel. iPlug exposes
// incoming channels as 0..15.
inline std::optional<int> DecodeMidiProgramChange(const iplug::IMidiMsg& msg, int savedChannel)
{
  if (savedChannel < kMidiOmniChannel || savedChannel > kMidiChannelCount)
    return std::nullopt;
  if (savedChannel != kMidiOmniChannel && msg.Channel() != savedChannel - 1)
    return std::nullopt;
  if (msg.StatusMsg() != iplug::IMidiMsg::kProgramChange)
    return std::nullopt;
  const int slot = msg.Program();
  if (slot < 0 || slot >= kMidiSoundSlotCount)
    return std::nullopt;
  return slot;
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
