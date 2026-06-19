#pragma once

// VoLum 1.2.0 custom-content mock model (UI-shell phase).
//
// Pure, in-memory stand-in for the future user-content backend behind the
// 1.2.0 features:
//   F5 per-amp presets, F6 bring-your-own amp (free-form speaker x channel
//   manifest), F7 custom IR cabs (DIRECT capture + IR), F8 imported pedal
//   captures. No disk IO, no serialization, no DSP - the C++ UI shells render
//   and navigate against this fake data so the refined D8 information
//   architecture can be evaluated before the backend lands.
//
// The free-form manifest model and the (speaker x channel) snap/derive helpers
// ARE the intended production logic and are covered by doctests
// (test_volum_custom_content.cpp). Everything prefixed Mock* is throwaway
// display data.

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace volum
{
namespace custom
{

// Which screen the header/sidebar overlays are showing.
enum class Screen
{
  None = 0,
  Presets, // F5 preset browser (header)
  Builder // F6 custom amp create/edit
};

// ---------------------------------------------------------------------------
// Free-form custom amp manifest (F6)
// ---------------------------------------------------------------------------
//
// A custom amp is a name plus three renameable cab slots (DIRECT is fixed and
// implicit) and a set of .nam files, each assigned to a (slot, channel). The
// special DIRECT slot means an amp-only / no-cab capture that can be paired
// with any custom IR. Cab slots 0..2 carry a short user-chosen label. Channels
// are gain stages (numbered 1..kMaxChannels); the matrix is sparse - not every
// (slot, channel) need exist.

inline const char* kDirectSpeaker = "DIRECT"; // DIRECT row display label

inline constexpr int kDirectSlot = -1; // amp-only DIRECT capture
inline constexpr int kUnassignedSlot = -2; // file not yet assigned to a slot
inline constexpr int kNumCabSlots = 3; // renameable cab slots per custom amp
inline constexpr int kMaxChannels = 8; // hard channel cap

struct CustomNamFile
{
  std::string file; // display filename, e.g. "G65-Plexi-Ch2.nam"
  int slot = kUnassignedSlot; // kDirectSlot, 0..kNumCabSlots-1, or kUnassignedSlot
  int channel = 0; // gain stage (>= 1); 0 means unassigned
};

// Number of selectable custom-amp fractal-art styles (see DrawCustomAmpArt).
inline constexpr int kNumCustomArts = 6;

struct CustomAmp
{
  std::string id; // stable opaque id (backend referencing); empty in builder draft
  std::string name;
  std::array<std::string, kNumCabSlots> cabNames = {"CB1", "CB2", "CB3"};
  std::vector<CustomNamFile> files;
  int art = 0; // assigned fractal-art style [0..kNumCustomArts)
};

inline bool IsDirectSlot(int slot)
{
  return slot == kDirectSlot;
}

// True when `slot` is a real assigned slot (DIRECT or a cab slot).
inline bool SlotAssigned(int slot)
{
  return slot == kDirectSlot || (slot >= 0 && slot < kNumCabSlots);
}

// True once a file has both a slot and a real channel assigned.
inline bool FileAssigned(const CustomNamFile& f)
{
  return f.channel >= 1 && SlotAssigned(f.slot);
}

// Display label for a slot within an amp ("DIRECT" or the cab-slot name).
inline std::string SlotLabel(const CustomAmp& amp, int slot)
{
  if (slot == kDirectSlot)
    return kDirectSpeaker;
  if (slot >= 0 && slot < kNumCabSlots)
    return amp.cabNames[(size_t)slot];
  return std::string();
}

// Normalize a cab name: drop whitespace, uppercase, cap at 3 chars. Empty input
// returns "" so callers can fall back to the slot default.
inline std::string NormalizeCabName(const std::string& in)
{
  std::string s;
  for (char c : in)
  {
    if (std::isspace((unsigned char)c))
      continue;
    s.push_back((char)std::toupper((unsigned char)c));
    if (s.size() >= 3)
      break;
  }
  return s;
}

// Number of files still missing a slot/channel assignment.
inline int UnassignedCount(const CustomAmp& amp)
{
  int n = 0;
  for (const auto& f : amp.files)
    if (!FileAssigned(f))
      ++n;
  return n;
}

// Present slots in canonical order: DIRECT first (if populated), then cab slots
// 0..2 that carry at least one assigned capture. Unassigned files are ignored.
inline std::vector<int> AmpSlots(const CustomAmp& amp)
{
  std::vector<int> out;
  auto populated = [&](int slot) {
    for (const auto& f : amp.files)
      if (FileAssigned(f) && f.slot == slot)
        return true;
    return false;
  };
  if (populated(kDirectSlot))
    out.push_back(kDirectSlot);
  for (int s = 0; s < kNumCabSlots; s++)
    if (populated(s))
      out.push_back(s);
  return out;
}

// Sorted, de-duplicated channels available for a given slot within the amp.
inline std::vector<int> AmpSlotChannels(const CustomAmp& amp, int slot)
{
  std::vector<int> out;
  for (const auto& f : amp.files)
  {
    if (!FileAssigned(f) || f.slot != slot)
      continue;
    if (std::find(out.begin(), out.end(), f.channel) == out.end())
      out.push_back(f.channel);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Files assigned to a (slot, channel) cell (a count >= 2 is a collision).
inline int CellFileCount(const CustomAmp& amp, int slot, int channel)
{
  int n = 0;
  for (const auto& f : amp.files)
    if (FileAssigned(f) && f.slot == slot && f.channel == channel)
      ++n;
  return n;
}

// True when this file shares its (slot, channel) with another assigned file.
inline bool FileIsDuplicate(const CustomAmp& amp, size_t fileIdx)
{
  if (fileIdx >= amp.files.size())
    return false;
  const auto& f = amp.files[fileIdx];
  if (!FileAssigned(f))
    return false;
  return CellFileCount(amp, f.slot, f.channel) >= 2;
}

inline bool HasDuplicate(const CustomAmp& amp)
{
  for (size_t i = 0; i < amp.files.size(); i++)
    if (FileIsDuplicate(amp, i))
      return true;
  return false;
}

// Highest assigned channel across all files (0 when none).
inline int MaxAssignedChannel(const CustomAmp& amp)
{
  int m = 0;
  for (const auto& f : amp.files)
    if (FileAssigned(f))
      m = std::max(m, f.channel);
  return m;
}

// "" when the amp is saveable; otherwise the reason the Save button is blocked.
// Order: no files, then unassigned files, then duplicate (slot,channel) cells.
inline std::string SaveDisabledReason(const CustomAmp& amp)
{
  if (amp.files.empty())
    return "Add a .nam file";
  if (UnassignedCount(amp) > 0)
    return "Assign every file a cab + channel";
  if (HasDuplicate(amp))
    return "Two files share a cab + channel";
  return "";
}

// ---------------------------------------------------------------------------
// (speaker x channel) snap helpers (production logic, doctested)
// ---------------------------------------------------------------------------

// True when a speaker slot has at least one assigned capture.
inline bool SpeakerEnabled(const std::vector<int>& availableChannels)
{
  return !availableChannels.empty();
}

// Channel to snap to when the focused speaker changes to one whose available
// gain-stage channels are `availableChannels`, given the previously selected
// `currentChannel`:
//   - keep the current channel if it is still available;
//   - otherwise snap to the first available channel;
//   - return -1 when the speaker has no captures (an empty/disabled slot).
inline int SnapChannel(const std::vector<int>& availableChannels, int currentChannel)
{
  if (availableChannels.empty())
    return -1;
  for (int c : availableChannels)
    if (c == currentChannel)
      return currentChannel;
  return availableChannels.front();
}

// ---------------------------------------------------------------------------
// display-only mock data (throwaway)
// ---------------------------------------------------------------------------

// A demo custom amp used by the builder/sidebar shells: a partial-rig NAM with
// DIRECT + two named cabs, sparse channel coverage, and one unassigned file.
inline CustomAmp MockDemoCustomAmp()
{
  CustomAmp a;
  a.id = "demo-plexi";
  a.name = "My Plexi A/B";
  a.art = 0;
  a.cabNames = {"G65", "V30", "CB3"};
  a.files = {
    {"AMP-Plexi-direct-1.nam", kDirectSlot, 1},
    {"AMP-Plexi-direct-2.nam", kDirectSlot, 2},
    {"G65-Plexi-Ch1.nam", 0, 1},
    {"V30-Plexi-Ch3.nam", 1, 3},
    {"dump-take7.nam", kUnassignedSlot, 0}, // needs assignment
  };
  return a;
}

// Session-mutable list of custom amp names. The shell mutates this in place so
// adding/editing/deleting from the builder + sidebar reflects live in the UI
// (no disk). Seeded with a few demo entries.
inline std::vector<std::string>& MockCustomAmps()
{
  static std::vector<std::string> v = {"My Plexi A/B", "Studio JCM800", "DIY Tweed"};
  return v;
}

// Per-custom-amp fractal-art id, kept in lockstep (same index) with
// MockCustomAmps(). Seeded so the demo amps look distinct.
inline std::vector<int>& MockCustomAmpArts()
{
  static std::vector<int> v = {0, 1, 2};
  return v;
}

// Per-custom-amp cab-slot names, kept in lockstep with MockCustomAmps(). Drives
// the custom-aware cabinet row (slice 3) and the builder coverage chips.
inline std::vector<std::array<std::string, kNumCabSlots>>& MockCustomAmpCabs()
{
  static std::vector<std::array<std::string, kNumCabSlots>> v = {
    {"G65", "V30", "CB3"},
    {"4X1", "2X1", "CB3"},
    {"TWD", "CB2", "CB3"},
  };
  return v;
}

// Per-custom-amp file manifest, kept in lockstep with MockCustomAmps(). Drives
// which cab slots/channels a custom amp exposes when focused.
inline std::vector<std::vector<CustomNamFile>>& MockCustomAmpFiles()
{
  static std::vector<std::vector<CustomNamFile>> v = {
    {{"direct-1.nam", kDirectSlot, 1}, {"direct-2.nam", kDirectSlot, 2}, {"g65-1.nam", 0, 1}, {"v30-3.nam", 1, 3}},
    {{"jcm-1.nam", kDirectSlot, 1}, {"4x1-1.nam", 0, 1}, {"4x1-2.nam", 0, 2}},
    {{"twd-1.nam", 0, 1}},
  };
  return v;
}

// Assigned art for a custom amp index (0 when out of range).
inline int CustomAmpArt(int idx)
{
  auto& a = MockCustomAmpArts();
  return (idx >= 0 && idx < (int)a.size()) ? a[(size_t)idx] : 0;
}

// Reconstruct a full CustomAmp for a stored index (display use in slices 3/4).
inline CustomAmp CustomAmpAt(int idx)
{
  CustomAmp a;
  auto& names = MockCustomAmps();
  if (idx < 0 || idx >= (int)names.size())
    return a;
  a.name = names[(size_t)idx];
  a.id = "amp-" + std::to_string(idx);
  if (idx < (int)MockCustomAmpArts().size())
    a.art = MockCustomAmpArts()[(size_t)idx];
  if (idx < (int)MockCustomAmpCabs().size())
    a.cabNames = MockCustomAmpCabs()[(size_t)idx];
  if (idx < (int)MockCustomAmpFiles().size())
    a.files = MockCustomAmpFiles()[(size_t)idx];
  return a;
}

// Add a custom amp from a full builder draft, de-duplicating the display name.
// Returns its index. Keeps the four parallel session stores aligned.
inline int AddCustomAmp(const CustomAmp& amp)
{
  auto& amps = MockCustomAmps();
  const std::string base = amp.name.empty() ? "New custom amp" : amp.name;
  std::string unique = base;
  int suffix = 2;
  while (std::find(amps.begin(), amps.end(), unique) != amps.end())
    unique = base + " " + std::to_string(suffix++);
  amps.push_back(unique);
  MockCustomAmpArts().push_back(((amp.art % kNumCustomArts) + kNumCustomArts) % kNumCustomArts);
  MockCustomAmpCabs().push_back(amp.cabNames);
  MockCustomAmpFiles().push_back(amp.files);
  return (int)amps.size() - 1;
}

// Convenience overload (name + art only) used by light callers/tests.
inline int AddCustomAmp(const std::string& name, int art = 0)
{
  CustomAmp a;
  a.name = name;
  a.art = art;
  return AddCustomAmp(a);
}

inline void RemoveCustomAmp(int idx)
{
  auto eraseAt = [idx](auto& v) {
    if (idx >= 0 && idx < (int)v.size())
      v.erase(v.begin() + idx);
  };
  eraseAt(MockCustomAmps());
  eraseAt(MockCustomAmpArts());
  eraseAt(MockCustomAmpCabs());
  eraseAt(MockCustomAmpFiles());
}

// Global IR library (F7) - usable as a cab on any amp via the speaker row.
// Session-mutable so Import adds entries live.
inline std::vector<std::string>& MockIRLibrary()
{
  static std::vector<std::string> v = {"Mesa 4x12 sm57", "Greenback room", "DI blend 50/50", "Marshall 1960 r121"};
  return v;
}

// Imported pedal captures (F8) - shown in the PRE capture dropdown CUSTOM group.
// Session-mutable so Import adds entries live.
inline std::vector<std::string>& MockCustomPedals()
{
  static std::vector<std::string> v = {"My Klon clone", "Rat (LM308)", "Tweed boost"};
  return v;
}

inline int AddPedal(const std::string& name)
{
  auto& v = MockCustomPedals();
  v.push_back(name.empty() ? "Imported pedal" : name);
  return (int)v.size() - 1;
}

inline void RenamePedal(int idx, const std::string& name)
{
  auto& v = MockCustomPedals();
  if (idx >= 0 && idx < (int)v.size() && !name.empty())
    v[(size_t)idx] = name;
}

inline void DeletePedal(int idx)
{
  auto& v = MockCustomPedals();
  if (idx >= 0 && idx < (int)v.size())
    v.erase(v.begin() + idx);
}

// Per-amp named presets (F5), session-mutable so save/rename/delete persist in
// the running shell (no disk). Index-keyed for the shell; the real backend keys
// by amp identity.
inline std::map<int, std::vector<std::string>>& SessionPresets()
{
  static std::map<int, std::vector<std::string>> m = {
    {0, {"Sat night gig", "Bedroom crunch", "Ambient clean"}},
    {3, {"Doom wall", "Lead boost"}},
    {7, {"Church clean"}},
  };
  return m;
}

inline std::vector<std::string> MockPresetsForAmp(int ampIdx)
{
  auto& m = SessionPresets();
  auto it = m.find(ampIdx);
  return it == m.end() ? std::vector<std::string>{} : it->second;
}

// Add a preset, de-duplicating the display name. Returns its index.
inline int AddPreset(int ampIdx, const std::string& name)
{
  auto& list = SessionPresets()[ampIdx];
  std::string unique = name.empty() ? "Preset" : name;
  int suffix = 2;
  while (std::find(list.begin(), list.end(), unique) != list.end())
    unique = (name.empty() ? "Preset" : name) + " " + std::to_string(suffix++);
  list.push_back(unique);
  return (int)list.size() - 1;
}

inline void RenamePreset(int ampIdx, int idx, const std::string& name)
{
  auto& list = SessionPresets()[ampIdx];
  if (idx >= 0 && idx < (int)list.size() && !name.empty())
    list[(size_t)idx] = name;
}

inline void DeletePreset(int ampIdx, int idx)
{
  auto& list = SessionPresets()[ampIdx];
  if (idx >= 0 && idx < (int)list.size())
    list.erase(list.begin() + idx);
}

// Append a custom IR to the global library (F7 import stub). Returns its index.
inline int AddIR(const std::string& name)
{
  auto& v = MockIRLibrary();
  v.push_back(name.empty() ? "Imported IR" : name);
  return (int)v.size() - 1;
}

inline void RenameIR(int idx, const std::string& name)
{
  auto& v = MockIRLibrary();
  if (idx >= 0 && idx < (int)v.size() && !name.empty())
    v[(size_t)idx] = name;
}

inline void DeleteIR(int idx)
{
  auto& v = MockIRLibrary();
  if (idx >= 0 && idx < (int)v.size())
    v.erase(v.begin() + idx);
}

} // namespace custom
} // namespace volum
