#pragma once

// VoLum custom-content model: the free-form custom-amp manifest types and the
// pure (speaker x channel) / validation / parsing helpers behind the 1.2.0 BYO
// features (F5-F8). These are the intended production shapes and are covered by
// doctests in test_volum_custom_content.cpp.
//
// This header holds *only* pure logic + POD types. The session-facing API that
// the UI calls (MockCustomAmps, AddIR, AddPreset, ...) lives in
// VoLumCustomContentMock.h, which now projects/mutates the real backend store
// (VoLumContentStore.h). Splitting the types out lets the store include the
// model without a circular dependency on the session bridge.

#include <algorithm>
#include <array>
#include <cctype>
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

inline const char* kDirectSpeaker = "DIRECT"; // DIRECT row display label

inline constexpr int kDirectSlot = -1; // amp-only DIRECT capture
inline constexpr int kUnassignedSlot = -2; // file not yet assigned to a slot
inline constexpr int kNumCabSlots = 3; // renameable cab slots per custom amp
inline constexpr int kMaxChannels = 8; // hard channel cap

struct CustomNamFile
{
  std::string file; // display leaf, e.g. "G65-Plexi-Ch2.nam" (dedup/parse)
  int slot = kUnassignedSlot; // kDirectSlot, 0..kNumCabSlots-1, or kUnassignedSlot
  int channel = 0; // gain stage (>= 1); 0 means unassigned
  std::string storedPath; // registry-relative resolvable path ("amps/..."); set on save
  std::string sourcePath; // absolute source path (builder draft only; transient, not saved)
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

// A custom amp needs a user-supplied name: the original .nam files are often
// opaque codes (e.g. "2204"), so the builder default is treated as "unnamed".
inline bool IsUnnamed(const std::string& name)
{
  return name.empty() || name == "New custom amp";
}

// "" when the amp is saveable; otherwise the reason the Save button is blocked.
// Order: name, then no files, then unassigned files, then duplicate cells.
inline std::string SaveDisabledReason(const CustomAmp& amp)
{
  if (IsUnnamed(amp.name))
    return "Name your amp";
  if (amp.files.empty())
    return "Add a .nam file";
  if (UnassignedCount(amp) > 0)
    return "Assign every file a cab + channel";
  if (HasDuplicate(amp))
    return "Two files share a cab + channel";
  return "";
}

// Parsed result of a factory-convention .nam filename: PREFIX-CODE-CHANNEL.nam.
struct ParsedNam
{
  int slot = kUnassignedSlot;
  int channel = 0;
  std::string cabName; // suggested cab-slot name (empty for DIRECT / unmatched)
  bool matched = false;
};

inline ParsedNam ParseNamFileName(const std::string& filename)
{
  ParsedNam r;
  std::string s = filename;
  const size_t slash = s.find_last_of("/\\");
  if (slash != std::string::npos)
    s = s.substr(slash + 1);
  const size_t dot = s.find_last_of('.');
  if (dot != std::string::npos && dot > 0)
    s = s.substr(0, dot);

  std::vector<std::string> tok;
  std::string cur;
  for (char c : s)
  {
    if (c == '-')
    {
      tok.push_back(cur);
      cur.clear();
    }
    else
      cur.push_back(c);
  }
  tok.push_back(cur);
  if (tok.size() < 2 || tok.front().empty())
    return r;

  std::string pre;
  for (char c : tok.front())
    pre.push_back((char)std::toupper((unsigned char)c));

  if (pre == "AMP" || pre == "DI" || pre == "DIRECT")
    r.slot = kDirectSlot;
  else if (pre == "G12")
  {
    r.slot = 0;
    r.cabName = "G12";
  }
  else if (pre == "G65")
  {
    r.slot = 1;
    r.cabName = "G65";
  }
  else if (pre == "V30")
  {
    r.slot = 2;
    r.cabName = "V30";
  }
  else
    return r; // unrecognized prefix -> leave unassigned

  r.matched = true;

  const std::string& last = tok.back();
  bool numeric = !last.empty();
  for (char c : last)
    if (!std::isdigit((unsigned char)c))
    {
      numeric = false;
      break;
    }
  if (numeric)
  {
    int ch = 0;
    for (char c : last)
      ch = ch * 10 + (c - '0');
    r.channel = ch;
  }
  return r;
}

// True when a speaker slot has at least one assigned capture.
inline bool SpeakerEnabled(const std::vector<int>& availableChannels)
{
  return !availableChannels.empty();
}

// Channel to snap to when the focused speaker changes (see VoLumCustomContentMock
// history): keep the current channel if still available, else first available,
// else -1 (empty/disabled slot).
inline int SnapChannel(const std::vector<int>& availableChannels, int currentChannel)
{
  if (availableChannels.empty())
    return -1;
  for (int c : availableChannels)
    if (c == currentChannel)
      return currentChannel;
  return availableChannels.front();
}

// Case-insensitive name comparison + within-list uniqueness check. Names must be
// unique within each content type (IRs among IRs, pedals among pedals, presets
// among an amp's presets); cross-type duplicates are allowed.
inline bool NameMatchesCI(const std::string& a, const std::string& b)
{
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
      return false;
  return true;
}

// True if the amp's file manifest already contains a capture with this filename
// (case-insensitive). Used to make re-importing the same .nam a no-op.
inline bool ManifestHasFile(const CustomAmp& amp, const std::string& file)
{
  for (const auto& f : amp.files)
    if (NameMatchesCI(f.file, file))
      return true;
  return false;
}

inline bool NameExistsCI(const std::vector<std::string>& list, const std::string& name, int exceptIdx = -1)
{
  for (int i = 0; i < (int)list.size(); ++i)
    if (i != exceptIdx && NameMatchesCI(list[(size_t)i], name))
      return true;
  return false;
}

} // namespace custom
} // namespace volum
