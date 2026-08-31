#pragma once

#include "VoLumContentStore.h"
#include "VoLumFactoryPresets.h"

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

enum class UiModeTransitionAction
{
  RefreshOnly
};

inline UiModeTransitionAction ActionForUiModeTransition(UiMode, UiMode)
{
  return UiModeTransitionAction::RefreshOnly;
}

inline constexpr std::array<const char*, 8> kPlayBypassParamNames = {
  "PrePitchActive", "PreCompActive", "PreNam1Active", "PreNam2Active",
  "ChorusActive",   "DelayActive",   "ReverbActive", "TremoloActive"};

inline bool IsPlaySnapshotDirty(bool hasSnapshot, const VoLumAmpSettings& live, const VoLumAmpSettings& recalled)
{
  return hasSnapshot && !AmpSettingsEqual(live, recalled);
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
    out = {ampId, presetId, kFactoryPresetDisplayName, kAmps[factory->ampIdx].displayName, true, factory->ampIdx, false};
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
      slot.sound.presetName = "Invalid slot";
    }
    out.push_back(std::move(slot));
  }
  return out;
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
