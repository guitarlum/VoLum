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
// A custom amp is a name plus a set of .nam files, each assigned to a
// (speaker, channel). Speakers are arbitrary cab names; the special DIRECT
// speaker means an amp-only / no-cab capture that can be paired with any
// custom IR to act as a cab across all of its channels. Channels are gain
// stages (numbered); the matrix is sparse - not every (speaker, channel) need
// exist.

inline const char* kDirectSpeaker = "DIRECT";

struct CustomNamFile
{
  std::string file; // display filename, e.g. "G65-Plexi-Ch2.nam"
  std::string speaker; // cab name, or kDirectSpeaker / "" for amp-only direct
  int channel = 0; // gain stage (>= 1); 0 means unassigned
};

struct CustomAmp
{
  std::string name;
  std::vector<CustomNamFile> files;
};

// A DIRECT (amp-only) speaker is the empty string or the literal "DIRECT".
inline bool IsDirectSpeaker(const std::string& speaker)
{
  return speaker.empty() || speaker == kDirectSpeaker;
}

// True once a file has both a speaker and a real channel assigned.
inline bool FileAssigned(const CustomNamFile& f)
{
  return f.channel >= 1 && (f.speaker == kDirectSpeaker || !f.speaker.empty());
}

// Number of files still missing a speaker/channel assignment.
inline int UnassignedCount(const CustomAmp& amp)
{
  int n = 0;
  for (const auto& f : amp.files)
    if (!FileAssigned(f))
      ++n;
  return n;
}

// Ordered, de-duplicated list of speakers present in the amp. DIRECT (if any)
// is sorted first (normalized to the kDirectSpeaker label), the rest follow in
// first-seen order. Unassigned files are ignored.
inline std::vector<std::string> AmpSpeakers(const CustomAmp& amp)
{
  std::vector<std::string> out;
  bool hasDirect = false;
  for (const auto& f : amp.files)
  {
    if (!FileAssigned(f))
      continue;
    if (IsDirectSpeaker(f.speaker))
    {
      hasDirect = true;
      continue;
    }
    if (std::find(out.begin(), out.end(), f.speaker) == out.end())
      out.push_back(f.speaker);
  }
  if (hasDirect)
    out.insert(out.begin(), kDirectSpeaker);
  return out;
}

// Sorted, de-duplicated channels available for a given speaker within the amp.
// `speaker` is matched case-sensitively; pass kDirectSpeaker (or "") for direct.
inline std::vector<int> AmpSpeakerChannels(const CustomAmp& amp, const std::string& speaker)
{
  const bool wantDirect = IsDirectSpeaker(speaker);
  std::vector<int> out;
  for (const auto& f : amp.files)
  {
    if (!FileAssigned(f))
      continue;
    const bool isDirect = IsDirectSpeaker(f.speaker);
    if (isDirect != wantDirect)
      continue;
    if (!wantDirect && f.speaker != speaker)
      continue;
    if (std::find(out.begin(), out.end(), f.channel) == out.end())
      out.push_back(f.channel);
  }
  std::sort(out.begin(), out.end());
  return out;
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
// DIRECT + one named cab, sparse channel coverage, and one unassigned file.
inline CustomAmp MockDemoCustomAmp()
{
  CustomAmp a;
  a.name = "My Plexi A/B";
  a.files = {
    {"AMP-Plexi-direct-1.nam", kDirectSpeaker, 1},
    {"AMP-Plexi-direct-2.nam", kDirectSpeaker, 2},
    {"G65-Plexi-Ch1.nam", "G65 4x12", 1},
    {"V30-Plexi-Ch3.nam", "V30 2x12", 3},
    {"dump-take7.nam", "", 0}, // needs assignment
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

// Add a custom amp, de-duplicating the display name. Returns its index.
inline int AddCustomAmp(const std::string& name)
{
  auto& amps = MockCustomAmps();
  std::string unique = name.empty() ? "New custom amp" : name;
  int suffix = 2;
  while (std::find(amps.begin(), amps.end(), unique) != amps.end())
    unique = (name.empty() ? "New custom amp" : name) + " " + std::to_string(suffix++);
  amps.push_back(unique);
  return (int)amps.size() - 1;
}

inline void RemoveCustomAmp(int idx)
{
  auto& amps = MockCustomAmps();
  if (idx >= 0 && idx < (int)amps.size())
    amps.erase(amps.begin() + idx);
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
