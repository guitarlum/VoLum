#pragma once

#include "VoLumContentStore.h"
#include "VoLumFactoryPresets.h"
#include "VoLumPickerGroups.h"
#include "VoLumTriptychState.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace volum
{

enum class UiMode
{
  Build,
  Play
};

inline const char* UiModeToString(UiMode mode)
{
  return mode == UiMode::Play ? "play" : "build";
}

inline UiMode UiModeFromString(const std::string& value)
{
  return value == "play" ? UiMode::Play : UiMode::Build;
}

inline UiMode UiModeFromJson(const nlohmann::json& value, const char* key)
{
  if (!key || !value.is_object() || !value.contains(key) || !value[key].is_string())
    return UiMode::Build;
  return UiModeFromString(value[key].get<std::string>());
}

// Machine-file PLAY/BUILD is standalone-only. A plugin keeps `fallback`
// (constructor default or the project id-tail) so a PLAY standalone quit
// cannot open the next VST3 insert on the stage.
inline UiMode UiModeFromMachineSettings(bool standalone, const nlohmann::json& value, UiMode fallback)
{
  return standalone ? UiModeFromJson(value, "volumUiMode") : fallback;
}

inline int MidiChannelFromJson(const nlohmann::json& value, int fallback = 0)
{
  if (!value.is_object() || !value.contains("midiCh") || !value["midiCh"].is_number_integer())
    return fallback;
  return std::clamp(value["midiCh"].get<int>(), 0, 16);
}

// Same split as UiMode: the listen filter in volum-settings.json is the
// standalone window. A plugin keeps Omni (or the project id-tail) so a
// standalone channel lock cannot move the next VST3 insert.
inline int MidiChannelFromMachineSettings(bool standalone, const nlohmann::json& value, int fallback)
{
  return standalone ? MidiChannelFromJson(value, fallback) : fallback;
}

enum class UiModeTransitionAction
{
  RefreshOnly
};

inline UiModeTransitionAction ActionForUiModeTransition(UiMode, UiMode)
{
  return UiModeTransitionAction::RefreshOnly;
}

inline bool EnteringPlayDropsLocks(UiMode from, UiMode to)
{
  return from != UiMode::Play && to == UiMode::Play;
}

// PLAY key branch: arrows + 1-8 only. Ctrl+S, T/M/H, and plain S fall through.
inline bool PlayBranchConsumes(bool ctrl, bool arrow, bool stompDigit)
{
  return !ctrl && (arrow || stompDigit);
}

inline EVoLumSection SectionForEffectFocus(EVoLumEffectFocus f)
{
  switch (f)
  {
    case EVoLumEffectFocus::PITCH:
    case EVoLumEffectFocus::COMP:
    case EVoLumEffectFocus::PRE_NAM1:
    case EVoLumEffectFocus::PRE_NAM2: return EVoLumSection::PRE;
    case EVoLumEffectFocus::CHORUS:
    case EVoLumEffectFocus::DELAY:
    case EVoLumEffectFocus::REVERB:
    case EVoLumEffectFocus::TREMOLO: return EVoLumSection::POST;
    default: return EVoLumSection::AMP;
  }
}

inline constexpr std::array<const char*, 8> kPlayBypassParamNames = {"PrePitchActive", "PreCompActive", "PreNam1Active",
                                                                     "PreNam2Active",  "ChorusActive",  "DelayActive",
                                                                     "ReverbActive",   "TremoloActive"};

inline bool IsPlaySnapshotDirty(bool hasSnapshot, const VoLumAmpSettings& live, const VoLumAmpSettings& recalled)
{
  return hasSnapshot && !AmpSettingsEqual(live, recalled);
}

// Default (no recalled snapshot) dirties against factory-default settings.
inline bool LivePresetDirty(bool hasSnapshot, const VoLumAmpSettings& live, const VoLumAmpSettings& recalled)
{
  if (!hasSnapshot)
    return !AmpSettingsEqual(live, VoLumAmpSettings{});
  return IsPlaySnapshotDirty(true, live, recalled);
}

inline constexpr const char* kPlayInvalidSlotLabel = "Invalid slot";

inline std::string OccupiedSlotLabel(bool valid, const std::string& presetName)
{
  return valid ? presetName : std::string(kPlayInvalidSlotLabel);
}

struct SoundChoice
{
  std::string ampId;
  std::string presetId;
  std::string presetName;
  std::string ampName;
  bool factory = false;
  int art = 0;
  bool customArt = false;
};

struct PlaySlot
{
  int slot = 0;
  SoundChoice sound;
  bool valid = false;
};

inline std::string AmpNameForOwner(const content::Registry& registry, const std::string& owner)
{
  const std::string prefix = "factory:";
  if (owner.rfind(prefix, 0) == 0)
  {
    try
    {
      const int idx = std::stoi(owner.substr(prefix.size()));
      if (idx >= 0 && idx < kAmpCount && content::FactoryOwnerKey(idx) == owner)
        return kAmps[idx].displayName;
    }
    catch (...)
    {
    }
    return {};
  }
  for (const auto& amp : registry.amps)
    if (amp.id == owner)
      return amp.name;
  return {};
}

inline std::vector<SoundChoice> BuildSoundChoices(const std::vector<FactoryPreset>& factoryPresets,
                                                  const content::Registry& registry)
{
  std::vector<SoundChoice> out;
  out.reserve(factoryPresets.size() + registry.presetBanks.size());
  for (const auto& preset : factoryPresets)
  {
    if (preset.ampIdx >= 0 && preset.ampIdx < kAmpCount)
      out.push_back({content::FactoryOwnerKey(preset.ampIdx), preset.id, kFactoryPresetDisplayName,
                     kAmps[preset.ampIdx].displayName, true, preset.ampIdx, false});
  }
  for (const auto& bank : registry.presetBanks)
  {
    const std::string ampName = AmpNameForOwner(registry, bank.first);
    if (ampName.empty())
      continue;
    for (const auto& preset : bank.second)
    {
      int art = 0;
      for (const auto& amp : registry.amps)
        if (amp.id == bank.first)
          art = amp.art;
      out.push_back({bank.first, preset.id, preset.name, ampName, false, art, true});
    }
  }
  return out;
}

inline bool ResolveSound(const std::vector<FactoryPreset>& factoryPresets, const content::Registry& registry,
                         const std::string& ampId, const std::string& presetId, SoundChoice& out)
{
  if (const auto* factory = FindFactoryPresetById(factoryPresets, presetId))
  {
    if (ampId != content::FactoryOwnerKey(factory->ampIdx))
      return false;
    out = {
      ampId, presetId, kFactoryPresetDisplayName, kAmps[factory->ampIdx].displayName, true, factory->ampIdx, false};
    return true;
  }
  const std::string ampName = AmpNameForOwner(registry, ampId);
  if (ampName.empty())
    return false;
  const auto bank = registry.presetBanks.find(ampId);
  if (bank == registry.presetBanks.end())
    return false;
  for (const auto& preset : bank->second)
  {
    if (preset.id == presetId)
    {
      int art = 0;
      for (const auto& amp : registry.amps)
        if (amp.id == ampId)
          art = amp.art;
      out = {ampId, presetId, preset.name, ampName, false, art, true};
      return true;
    }
  }
  return false;
}

inline std::vector<PlaySlot> BuildPlaySlots(const std::vector<FactoryPreset>& factoryPresets,
                                            const content::Registry& registry)
{
  std::vector<PlaySlot> out;
  out.reserve(registry.midiSoundMap.size());
  // The registry keys the map by slot, so iteration is already in slot order.
  for (const auto& kv : registry.midiSoundMap)
  {
    PlaySlot slot;
    slot.slot = kv.first;
    slot.valid = ResolveSound(factoryPresets, registry, kv.second.ampId, kv.second.presetId, slot.sound);
    if (!slot.valid)
    {
      slot.sound.ampId = kv.second.ampId;
      slot.sound.presetId = kv.second.presetId;
      slot.sound.presetName = kPlayInvalidSlotLabel;
    }
    out.push_back(std::move(slot));
  }
  return out;
}

inline bool SoundIsAssigned(const std::vector<PlaySlot>& slots, const std::string& ampId, const std::string& presetId)
{
  for (const auto& slot : slots)
  {
    if (slot.valid && slot.sound.ampId == ampId && slot.sound.presetId == presetId)
      return true;
  }
  return false;
}

// + is "Add this sound" when the live rig is not already a PLAY row, or when
// a dirty Factory/Default must be saved as a new User Sound first.
inline bool PlayPlusAddsHeard(bool dirty, bool factoryOrDefaultOrigin, bool liveAssigned)
{
  if (factoryOrDefaultOrigin && dirty)
    return true;
  return !liveAssigned;
}

// Save As before assign: Factory/Default that is dirty, or Default (empty id)
// which can never be written to a MIDI slot. Clean Factory Ready can be
// assigned as-is.
inline bool AddHeardNeedsSaveAs(PresetSaveAction action, bool dirty, bool presetIdEmpty)
{
  return action == PresetSaveAction::SaveUserCopy && (dirty || presetIdEmpty);
}

// Add this sound can write a MIDI row only when a User Sound exists and the
// map still has a free program number. Otherwise finish() is a silent no-op.
inline bool AddHeardMarksLive(int firstFreeSlot, bool presetIdEmpty)
{
  return firstFreeSlot >= 0 && !presetIdEmpty;
}

inline bool IsLastRecalledSlot(const PlaySlot& slot, int lastSlot, const std::string& activeAmpId,
                               const std::string& activePresetId)
{
  return slot.valid && slot.slot == lastSlot && slot.sound.ampId == activeAmpId
         && slot.sound.presetId == activePresetId;
}

// The slot PLAY's up/down arrows should land on, given the slot playing now.
//
// Only slots a Program Change would actually recall are reachable: unassigned
// numbers do not exist in `slots` at all, and an assigned slot whose Sound is
// gone is skipped, exactly as ProcessMidiMsg drops a PC that will not resolve.
// Stepping wraps, because the rail is short and a player holding Down at the
// bottom of it means "the next one", not "nothing".
//
// `currentSlot` may be -1 (nothing recalled yet) or a slot that is no longer
// reachable - cleared, or gone invalid since it was recalled - in which case the
// step lands on the nearest reachable slot in the direction of travel.
// Returns -1 when there is nothing to step to.
inline int StepAssignedSlot(const std::vector<PlaySlot>& slots, int currentSlot, int dir)
{
  std::vector<int> reachable;
  reachable.reserve(slots.size());
  for (const auto& slot : slots)
    if (slot.valid)
      reachable.push_back(slot.slot);
  if (reachable.empty())
    return -1;
  // BuildPlaySlots walks a slot-keyed map, so this is already ascending; sort
  // anyway so the helper is honest about its own contract.
  std::sort(reachable.begin(), reachable.end());

  const int count = static_cast<int>(reachable.size());
  const bool forward = dir >= 0;
  if (currentSlot < 0)
    return forward ? reachable.front() : reachable.back();

  const auto at = std::find(reachable.begin(), reachable.end(), currentSlot);
  if (at != reachable.end())
  {
    const int index = static_cast<int>(at - reachable.begin());
    return reachable[static_cast<size_t>(((index + (forward ? 1 : -1)) % count + count) % count)];
  }

  // Current slot is not reachable any more: fall to the neighbour we were heading
  // towards, wrapping when there is none on that side.
  if (forward)
  {
    const auto next = std::upper_bound(reachable.begin(), reachable.end(), currentSlot);
    return next == reachable.end() ? reachable.front() : *next;
  }
  const auto prev = std::lower_bound(reachable.begin(), reachable.end(), currentSlot);
  return prev == reachable.begin() ? reachable.back() : *(prev - 1);
}

} // namespace volum
