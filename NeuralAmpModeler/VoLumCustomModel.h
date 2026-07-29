#pragma once

// VoLum custom-content model: the free-form custom-amp manifest types and the
// pure (speaker x channel) / validation / parsing helpers behind the 1.2.0 BYO
// features (F5-F8). These are the intended production shapes and are covered by
// doctests in test_volum_custom_content.cpp.
//
// This header holds *only* pure logic + POD types. The session-facing API that
// the UI calls (MockCustomAmps, AddIR, AddPreset, ...) lives in
// VoLumCustomContentApi.h, which now projects/mutates the real backend store
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

// Byte length of the UTF-8 sequence a lead byte starts, or 0 if it is not a valid
// lead byte. Used everywhere a name is cut to a length: every one of those cuts
// ends up in volum-content.json, and nlohmann::json::dump() throws on invalid
// UTF-8, so half a glyph is not a cosmetic problem.
inline std::size_t Utf8SequenceLength(unsigned char lead)
{
  if (lead < 0x80)
    return 1;
  if ((lead & 0xE0) == 0xC0)
    return 2;
  if ((lead & 0xF0) == 0xE0)
    return 3;
  if ((lead & 0xF8) == 0xF0)
    return 4;
  return 0; // continuation byte or invalid lead
}

// Longest prefix of s that is at most maxBytes long and never splits a UTF-8
// sequence. Malformed input stops the scan rather than being copied through.
inline std::string Utf8Prefix(const std::string& s, std::size_t maxBytes)
{
  std::size_t end = 0;
  while (end < s.size())
  {
    const std::size_t len = Utf8SequenceLength(static_cast<unsigned char>(s[end]));
    if (len == 0 || end + len > s.size() || end + len > maxBytes)
      break;
    end += len;
  }
  return s.substr(0, end);
}

// Normalize a cab name: drop whitespace, uppercase, cap at 3 characters. Empty
// input returns "" so callers can fall back to the slot default.
//
// Counts characters, not bytes. The old byte counter cut a four-byte glyph after
// three bytes, and that invalid fragment went into the builder draft and then into
// the registry, where dump() throws - so naming a cabinet with an emoji could take
// the plug-in down while saving the amp.
inline std::string NormalizeCabName(const std::string& in)
{
  std::string s;
  int chars = 0;
  std::size_t i = 0;
  while (i < in.size() && chars < 3)
  {
    const unsigned char lead = static_cast<unsigned char>(in[i]);
    const std::size_t len = Utf8SequenceLength(lead);
    if (len == 0 || i + len > in.size())
      break; // malformed: stop rather than emit a partial character

    if (len == 1)
    {
      if (std::isspace(lead))
      {
        ++i;
        continue;
      }
      s.push_back(static_cast<char>(std::toupper(lead)));
    }
    else
    {
      // No case mapping outside ASCII: std::toupper is byte-wise and would corrupt
      // the sequence.
      s.append(in, i, len);
    }
    ++chars;
    i += len;
  }
  return s;
}

// Clamp every cab-slot name to the 3-char rule (see NormalizeCabName). A slot
// whose stored name normalizes to empty falls back to its "CBn" default. Apply
// this at every persistence boundary (registry load + add/update) so a
// hand-edited or migrated registry can never carry an over-long cab name that
// overflows the cabinet row. The builder text-entry path already normalizes per
// field; this is the matching backend guarantee.
inline void NormalizeAmpCabNames(CustomAmp& amp)
{
  for (int i = 0; i < kNumCabSlots; ++i)
  {
    std::string norm = NormalizeCabName(amp.cabNames[(size_t)i]);
    if (norm.empty())
      norm = "CB" + std::to_string(i + 1);
    amp.cabNames[(size_t)i] = norm;
  }
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

// Sorted, de-duplicated gain-stage channels that carry at least one assigned
// file anywhere in the amp. Used so the coverage grid shows only the channels
// actually present (e.g. a Fryette with only channels 3 & 4 shows just "3"/"4"
// instead of an empty 1..4 range that looks like missing content). Capped at
// kMaxChannels; empty when no file is assigned yet.
inline std::vector<int> AssignedChannels(const CustomAmp& amp)
{
  std::vector<int> out;
  for (const auto& f : amp.files)
  {
    if (!FileAssigned(f) || f.channel < 1 || f.channel > kMaxChannels)
      continue;
    if (std::find(out.begin(), out.end(), f.channel) == out.end())
      out.push_back(f.channel);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Channel-first navigation helpers (1.2.0 redesign). The amp is the entry point;
// the gain-stage channel is the primary sub-axis; the speaker/cab row only offers
// what actually exists for the selected channel. These invert AmpSlotChannels:
// given a channel, which slots carry it. For factory amps the channel set is
// uniform across cabs (verified by test_volum_paths), so this concern is custom-
// amp specific -- a user can assign, say, DIRECT only on channel 1 and a cab only
// on channel 2.

// Slots (DIRECT first, then cab slots 0..2) that carry an assigned capture for
// `channel`, in canonical row order. Empty when the channel is unassigned.
inline std::vector<int> SlotsForChannel(const CustomAmp& amp, int channel)
{
  std::vector<int> out;
  auto has = [&](int slot) {
    for (const auto& f : amp.files)
      if (FileAssigned(f) && f.slot == slot && f.channel == channel)
        return true;
    return false;
  };
  if (has(kDirectSlot))
    out.push_back(kDirectSlot);
  for (int s = 0; s < kNumCabSlots; s++)
    if (has(s))
      out.push_back(s);
  return out;
}

// True when the amp has a DIRECT (cab-less) capture FOR THIS CHANNEL. The Custom
// IR cab convolves the DIRECT signal and the "No Cab" row plays it raw, so both
// are only reachable on channels that actually own a DIRECT capture. (Amp-wide
// HasDirectCapture is too coarse: an amp can have DIRECT on channel 1 only.)
inline bool ChannelHasDirect(const CustomAmp& amp, int channel)
{
  for (const auto& f : amp.files)
    if (FileAssigned(f) && f.slot == kDirectSlot && f.channel == channel)
      return true;
  return false;
}

// Slot to settle on when the channel changes. Keep the current slot if it still
// carries the new channel; otherwise snap to the first available real CAB slot;
// otherwise DIRECT ("No Cab") as the last resort; otherwise -2 (kUnassignedSlot)
// when the channel has no captures at all (caller should treat as "no change").
inline int SnapSlotForChannel(const CustomAmp& amp, int channel, int currentSlot)
{
  const std::vector<int> slots = SlotsForChannel(amp, channel);
  if (slots.empty())
    return kUnassignedSlot;
  for (int s : slots)
    if (s == currentSlot)
      return currentSlot; // current cab still valid for this channel
  for (int s : slots)
    if (s != kDirectSlot)
      return s; // prefer a real cab
  return kDirectSlot; // No Cab is the last resort
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

// True when the amp has a DIRECT (raw, cab-less) capture. A custom IR convolves
// the DIRECT signal, so without one the Custom IR cab has nothing to run and is
// disabled in the UI.
inline bool HasDirectCapture(const CustomAmp& amp)
{
  return !AmpSlotChannels(amp, kDirectSlot).empty();
}

// Channel to snap to when the focused speaker changes (see VoLumCustomContentApi
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

// Stepper position (0-based index) of `channel` within availableChannels, or 0
// when it is not present / the list is empty. Pairs with SnapChannel so a cab or
// IR switch keeps the channel stepper row aligned with the resolved gain stage
// instead of snapping back to position 0.
inline int ChannelStepIndex(const std::vector<int>& availableChannels, int channel)
{
  for (int i = 0; i < static_cast<int>(availableChannels.size()); ++i)
    if (availableChannels[static_cast<size_t>(i)] == channel)
      return i;
  return 0;
}

// The resolved channel-first view for a focused custom lane. Pure output of
// ResolveLaneCabs so the whole navigation policy is unit-testable headlessly and
// the UI wiring stays thin.
struct LaneCabView
{
  std::vector<int> channels; // amp-wide gain stages -> channel stepper labels
  int channelPos = 0; // selected position within `channels`
  int channel = 1; // resolved gain stage
  int slot = kDirectSlot; // resolved slot (kDirectSlot or 0..kNumCabSlots-1)
  int selUiIndex = 0; // speaker-row selection (0 = No Cab, 1..3 = cab slot+1)
  bool noCabEnabled = false; // channel owns a DIRECT capture
  bool irEnabled = false; // == noCabEnabled (custom IR convolves DIRECT)
  std::array<bool, kNumCabSlots> cabEnabled = {false, false, false};
};

// Resolve the channel-first cab view for a focused custom lane from the lane's
// current (slot, channel). Channel is the primary axis: the stepper lists the
// amp-wide channel set; the speaker row only offers cabs that carry the selected
// channel; No Cab / Custom IR require a DIRECT capture ON that channel. The
// current channel is kept when still present (else first available); the slot is
// snapped via SnapSlotForChannel (current kept if valid, else first real cab,
// else No Cab).
inline LaneCabView ResolveLaneCabs(const CustomAmp& amp, int curSlot, int curChannel)
{
  LaneCabView v;
  v.channels = AssignedChannels(amp);

  int channel = curChannel;
  if (!v.channels.empty() && std::find(v.channels.begin(), v.channels.end(), channel) == v.channels.end())
    channel = v.channels.front();
  if (channel < 1)
    channel = v.channels.empty() ? 1 : v.channels.front();
  v.channel = channel;
  v.channelPos = ChannelStepIndex(v.channels, channel);

  int slot = SnapSlotForChannel(amp, channel, curSlot);
  if (slot == kUnassignedSlot)
    slot = curSlot; // channel has nothing assigned (degenerate); leave as-is
  v.slot = slot;
  v.selUiIndex = (slot == kDirectSlot) ? 0 : slot + 1;

  v.noCabEnabled = ChannelHasDirect(amp, channel);
  v.irEnabled = v.noCabEnabled;
  for (int s = 0; s < kNumCabSlots; ++s)
  {
    const auto chs = AmpSlotChannels(amp, s);
    v.cabEnabled[(size_t)s] = std::find(chs.begin(), chs.end(), channel) != chs.end();
  }
  return v;
}

// Max characters for user-entered custom names. Caps stop long names from
// overflowing the hero / sub-row / list labels. Cab-slot names are separately
// capped to 3 by NormalizeCabName.
inline constexpr std::size_t kMaxCustomNameLen = 24; // amp / IR / pedal name
inline constexpr std::size_t kMaxPresetNameLen = 28; // preset name

// Clamp a free-form name to maxChars, dropping any dangling trailing UTF-8
// continuation bytes so a multibyte glyph is never split. The text-entry length
// cap is the primary guard; this is defense-in-depth at the persistence edge.
inline std::string ClampName(const std::string& s, std::size_t maxChars)
{
  if (s.size() <= maxChars)
    return s;
  return Utf8Prefix(s, maxChars);
}

// Short pill label for a custom capture name. Names longer than maxChars bytes
// are clipped to maxChars + a single-glyph ellipsis so they fit the tiny Amp-view
// quiet-slot pill instead of overflowing into the neighbouring pill. Curated
// factory short labels are already <=5 chars, so this only ever trims custom ones.
inline std::string ShortCaptureLabel(const std::string& name, std::size_t maxChars = 5)
{
  if (name.size() <= maxChars)
    return name;
  return Utf8Prefix(name, maxChars) + "\u2026";
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
