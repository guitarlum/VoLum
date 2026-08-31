#pragma once

// `.volumpack`: what is in a Pack, and what happens on conflict.
//
// One format, two jobs. **Everything** is a backup of this machine's library plus
// its machine settings and MIDI slot assignments. **Share** is a subset of library
// items and their files - no settings, no MIDI map - so a Pack you send someone
// cannot quietly reconfigure their rig.
//
// The version in the manifest is a *contract*, not the app version. An older VoLum
// refuses a newer contract by name ("needs VoLum 1.3.0") instead of guessing; a
// newer VoLum reads an older contract, and unknown keys are ignored, so a Pack
// exported by a later build imports here as long as the contract matches.
//
// Everything below is pure or takes an explicit ContentStore. The picker, the
// preview chrome and the verb radio live in VoLumPackOverlay.h; keeping the rules
// out of the control is what makes the permutations testable.
//
// Tests: tests/test_volum_pack.cpp.

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "VoLumContentStore.h"
#include "VoLumPackArchive.h"

namespace volum::pack
{

// Bump only when a Pack written by this build cannot be read correctly by the
// previous contract's reader. Adding an ignorable key is not a bump.
inline constexpr int kContractVersion = 1;
// The first VoLum that writes kContractVersion. Quoted verbatim in the refusal, so
// the user is told which version to get rather than that something went wrong.
inline constexpr const char* kContractFirstApp = "1.3.0";

inline constexpr const char* kManifestEntry = "manifest.json";
inline constexpr const char* kLibraryEntry = "library.json";
inline constexpr const char* kSettingsEntry = "settings.json";
inline constexpr const char* kPayloadPrefix = "payload/";

enum class Job
{
  Share, // library items + files only
  Everything // + machine settings + MIDI sound map
};

// What happens to a library id that is already here. Factory amps are shipped and
// cannot be deleted, so no verb touches them.
enum class ImportVerb
{
  Overwrite, // Pack wins; local-only items stay
  Add, // keep mine; local-only items stay
  Reset // Pack wins; local-only items are deleted
};

inline const char* JobName(Job job)
{
  return job == Job::Everything ? "everything" : "share";
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

struct ExportSelection
{
  bool everything = true;
  std::vector<std::string> ampIds; // custom amps the user ticked
  std::vector<std::string> presetIds; // named presets the user ticked
};

struct ExportPlan
{
  Job job = Job::Everything;
  std::vector<std::string> ampIds;
  std::vector<std::string> irIds;
  std::vector<std::string> pedalIds;
  std::vector<std::string> presetIds;
  // Items the selection dragged in. The export preview lists these under "also
  // including" and must not let them be unchecked: a Pack whose presets reference
  // an IR it does not carry imports as a rig full of holes.
  std::vector<std::string> alsoIncluding; // display strings
  bool includeSettings = false;
  bool includeMidiSoundMap = false;

  bool Empty() const { return ampIds.empty() && irIds.empty() && pedalIds.empty() && presetIds.empty(); }
};

namespace detail
{
inline const custom::CustomAmp* FindAmp(const content::Registry& r, const std::string& id)
{
  for (const auto& a : r.amps)
    if (a.id == id)
      return &a;
  return nullptr;
}

inline const content::IRItem* FindIr(const content::Registry& r, const std::string& id)
{
  for (const auto& ir : r.irs)
    if (ir.id == id)
      return &ir;
  return nullptr;
}

inline const content::PedalItem* FindPedal(const content::Registry& r, const std::string& id)
{
  for (const auto& p : r.pedals)
    if (p.id == id)
      return &p;
  return nullptr;
}

inline const content::PedalItem* FindPedalByCapture(const content::Registry& r, int capture)
{
  if (capture < content::kCustomPedalIndexBase)
    return nullptr; // factory capture: shipped, never packed
  for (const auto& p : r.pedals)
    if (p.legacyIndex == capture)
      return &p;
  return nullptr;
}

// ownerKey + index of a preset id, or false.
inline bool FindPreset(const content::Registry& r, const std::string& id, std::string& ownerKey, size_t& at)
{
  for (const auto& bank : r.presetBanks)
    for (size_t i = 0; i < bank.second.size(); ++i)
      if (bank.second[i].id == id)
      {
        ownerKey = bank.first;
        at = i;
        return true;
      }
  return false;
}

inline void AddUnique(std::vector<std::string>& v, const std::string& s)
{
  if (!s.empty() && std::find(v.begin(), v.end(), s) == v.end())
    v.push_back(s);
}
} // namespace detail

// Resolve a selection into the full set of items a Pack has to carry.
//
// Selecting a custom amp takes its preset bank with it - that is what "export this
// amp" means to the person clicking it - and every capture, IR and pedal those
// presets and amps reference. A preset on a *factory* amp pulls in its custom IR
// and pedal but no amp entry: the factory capture is shipped, so packing it would
// be shipping VoLum's own content back to VoLum.
inline ExportPlan BuildExportPlan(const content::Registry& r, const ExportSelection& sel)
{
  using namespace detail;
  ExportPlan plan;
  plan.job = sel.everything ? Job::Everything : Job::Share;
  plan.includeSettings = sel.everything;
  plan.includeMidiSoundMap = sel.everything;

  if (sel.everything)
  {
    for (const auto& a : r.amps)
      plan.ampIds.push_back(a.id);
    for (const auto& ir : r.irs)
      plan.irIds.push_back(ir.id);
    for (const auto& p : r.pedals)
      plan.pedalIds.push_back(p.id);
    for (const auto& bank : r.presetBanks)
      for (const auto& pr : bank.second)
        plan.presetIds.push_back(pr.id);
    return plan;
  }

  for (const auto& id : sel.ampIds)
    if (FindAmp(r, id))
      AddUnique(plan.ampIds, id);
  for (const auto& id : sel.presetIds)
  {
    std::string owner;
    size_t at = 0;
    if (FindPreset(r, id, owner, at))
      AddUnique(plan.presetIds, id);
  }

  // A selected amp brings its own bank.
  for (const auto& ampId : plan.ampIds)
  {
    const auto bank = r.presetBanks.find(ampId);
    if (bank == r.presetBanks.end())
      continue;
    for (const auto& pr : bank->second)
      if (std::find(plan.presetIds.begin(), plan.presetIds.end(), pr.id) == plan.presetIds.end())
      {
        plan.presetIds.push_back(pr.id);
        plan.alsoIncluding.push_back("Preset \"" + pr.name + "\"");
      }
  }

  // Closure over the settings every selected preset carries, plus the support
  // partner a selected amp's presets point at.
  auto pullSettings = [&](const VoLumAmpSettings& s) {
    for (const std::string* irId : {&s.activeIrId, &s.supportActiveIrId})
      if (const auto* ir = FindIr(r, *irId))
        if (std::find(plan.irIds.begin(), plan.irIds.end(), ir->id) == plan.irIds.end())
        {
          plan.irIds.push_back(ir->id);
          plan.alsoIncluding.push_back("IR \"" + ir->name + "\"");
        }
    for (int capture : {s.preNam1Capture, s.preNam2Capture})
      if (const auto* p = FindPedalByCapture(r, capture))
        if (std::find(plan.pedalIds.begin(), plan.pedalIds.end(), p->id) == plan.pedalIds.end())
        {
          plan.pedalIds.push_back(p->id);
          plan.alsoIncluding.push_back("Pedal \"" + p->name + "\"");
        }
    if (const auto* amp = FindAmp(r, s.supportCustomId))
      if (std::find(plan.ampIds.begin(), plan.ampIds.end(), amp->id) == plan.ampIds.end())
      {
        plan.ampIds.push_back(amp->id);
        plan.alsoIncluding.push_back("Custom amp \"" + amp->name + "\"");
      }
  };

  // Indexed, not range-for: pullSettings can append to plan.ampIds, whose bank may
  // in turn add presets, and a partner amp's own requirements have to be pulled in
  // too. Growing while iterating is the point.
  for (size_t i = 0; i < plan.presetIds.size(); ++i)
  {
    std::string owner;
    size_t at = 0;
    if (FindPreset(r, plan.presetIds[i], owner, at))
      pullSettings(r.presetBanks.at(owner)[at].settings);
  }
  for (size_t i = 0; i < plan.ampIds.size(); ++i)
  {
    const auto bank = r.presetBanks.find(plan.ampIds[i]);
    if (bank == r.presetBanks.end())
      continue;
    for (const auto& pr : bank->second)
      if (std::find(plan.presetIds.begin(), plan.presetIds.end(), pr.id) == plan.presetIds.end())
      {
        plan.presetIds.push_back(pr.id);
        plan.alsoIncluding.push_back("Preset \"" + pr.name + "\"");
        pullSettings(pr.settings);
      }
  }
  return plan;
}

// The library fragment a Pack carries: exactly the planned items, nothing else.
inline content::Registry PackRegistrySubset(const content::Registry& r, const ExportPlan& plan)
{
  using namespace detail;
  content::Registry out;
  for (const auto& id : plan.ampIds)
    if (const auto* a = FindAmp(r, id))
      out.amps.push_back(*a);
  for (const auto& id : plan.irIds)
    if (const auto* ir = FindIr(r, id))
      out.irs.push_back(*ir);
  for (const auto& id : plan.pedalIds)
    if (const auto* p = FindPedal(r, id))
      out.pedals.push_back(*p);
  for (const auto& id : plan.presetIds)
  {
    std::string owner;
    size_t at = 0;
    if (FindPreset(r, id, owner, at))
      out.presetBanks[owner].push_back(r.presetBanks.at(owner)[at]);
  }
  out.nextPedalIndex = r.nextPedalIndex;
  // Only Everything carries the MIDI map: a shared Pack must not renumber somebody
  // else's footswitches.
  if (plan.includeMidiSoundMap)
    out.midiSoundMap = r.midiSoundMap;
  return out;
}

// Every registry-relative payload file the planned items need.
inline std::vector<std::string> PackPayloadFiles(const content::Registry& r, const ExportPlan& plan)
{
  using namespace detail;
  std::vector<std::string> files;
  for (const auto& id : plan.ampIds)
    if (const auto* a = FindAmp(r, id))
      for (const auto& f : a->files)
        if (custom::FileAssigned(f))
          AddUnique(files, f.storedPath);
  for (const auto& id : plan.irIds)
    if (const auto* ir = FindIr(r, id))
      AddUnique(files, ir->file);
  for (const auto& id : plan.pedalIds)
    if (const auto* p = FindPedal(r, id))
      AddUnique(files, p->file);
  return files;
}

inline nlohmann::json BuildManifest(const ExportPlan& plan, const std::vector<std::string>& files)
{
  nlohmann::json j;
  j["contractVersion"] = kContractVersion;
  j["job"] = JobName(plan.job);
  j["customAmps"] = plan.ampIds;
  j["irLibrary"] = plan.irIds;
  j["pedals"] = plan.pedalIds;
  j["presets"] = plan.presetIds;
  j["files"] = files;
  j["includesSettings"] = plan.includeSettings;
  j["includesMidiSoundMap"] = plan.includeMidiSoundMap;
  return j;
}

// Assemble the archive entries. `settingsJson` is the machine settings document,
// used only for an Everything Pack; pass "" from a plugin, which never has one.
inline std::vector<ArchiveEntry> BuildPackEntries(content::ContentStore& store, const ExportPlan& plan,
                                                  const std::string& settingsJson, std::string* error = nullptr)
{
  const auto& reg = store.reg();
  const auto files = PackPayloadFiles(reg, plan);

  std::vector<ArchiveEntry> entries;
  entries.push_back({kManifestEntry, BuildManifest(plan, files).dump(2)});
  entries.push_back({kLibraryEntry, content::RegistryToJson(PackRegistrySubset(reg, plan)).dump(2)});
  if (plan.includeSettings && !settingsJson.empty())
    entries.push_back({kSettingsEntry, settingsJson});

  for (const auto& rel : files)
  {
    const auto abs = store.ResolveStored(rel);
    std::string data;
    if (abs.empty() || !ReadWholeFile(abs, data))
    {
      // A missing capture means the Pack would import as a broken amp. Refuse now,
      // where we can name the file, rather than at import on someone else's machine.
      if (error)
        *error = "Could not read \"" + rel + "\" from your library.";
      return {};
    }
    entries.push_back({std::string(kPayloadPrefix) + rel, std::move(data)});
  }
  return entries;
}

inline bool WritePack(content::ContentStore& store, const ExportPlan& plan, const std::string& settingsJson,
                      const std::filesystem::path& outPath, std::string* error = nullptr)
{
  if (plan.Empty())
  {
    if (error)
      *error = "Nothing selected to export.";
    return false;
  }
  const auto entries = BuildPackEntries(store, plan, settingsJson, error);
  if (entries.empty())
    return false;
  if (!WriteArchiveToFile(outPath, entries))
  {
    if (error)
      *error = "Could not write the Pack file.";
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Import: open + validate
// ---------------------------------------------------------------------------

struct PackContents
{
  bool ok = false;
  std::string error;
  int contractVersion = 0;
  Job job = Job::Share;
  content::Registry library;
  std::map<std::string, std::string> files; // registry-relative path -> bytes
  std::string settingsJson; // "" when the Pack carries none
  bool includesMidiSoundMap = false;

  explicit operator bool() const { return ok; }
};

inline PackContents ReadPackFromArchive(const ReadResult& archive)
{
  PackContents out;
  if (!archive)
  {
    out.error = archive.error.empty() ? "This is not a VoLum Pack." : archive.error;
    return out;
  }

  const std::string* manifestText = archive.Find(kManifestEntry);
  const std::string* libraryText = archive.Find(kLibraryEntry);
  if (!manifestText || !libraryText)
  {
    out.error = "This is not a VoLum Pack.";
    return out;
  }

  nlohmann::json manifest;
  nlohmann::json library;
  try
  {
    manifest = nlohmann::json::parse(*manifestText);
    library = nlohmann::json::parse(*libraryText);
  }
  catch (...)
  {
    out.error = "This Pack is damaged.";
    return out;
  }
  if (!manifest.is_object() || !library.is_object())
  {
    out.error = "This Pack is damaged.";
    return out;
  }

  out.contractVersion = manifest.value("contractVersion", 0);
  if (out.contractVersion <= 0)
  {
    out.error = "This is not a VoLum Pack.";
    return out;
  }
  if (out.contractVersion > kContractVersion)
  {
    // Refuse by name. Guessing at a format we do not know would import a
    // half-understood library, which is worse than not importing.
    out.error = std::string("This Pack needs VoLum ") + kContractFirstApp + " or newer.";
    return out;
  }

  out.job = manifest.value("job", std::string("share")) == "everything" ? Job::Everything : Job::Share;
  bool healed = false;
  out.library = content::RegistryFromJson(library, &healed);

  // Every file the manifest promises has to be here. A truncated download that
  // still unzips would otherwise import amps whose captures are missing.
  if (manifest.contains("files") && manifest["files"].is_array())
  {
    for (const auto& f : manifest["files"])
    {
      if (!f.is_string())
        continue;
      const std::string rel = f.get<std::string>();
      const std::string* data = archive.Find(std::string(kPayloadPrefix) + rel);
      if (!data)
      {
        out.error = "This Pack is incomplete (missing \"" + rel + "\").";
        return out;
      }
      if (!content::IsSafeStoredRelPath(rel))
      {
        out.error = "This Pack contains an unsafe file path.";
        return out;
      }
      out.files[rel] = *data;
    }
  }

  // A Share Pack must not carry settings or a MIDI map even if something wrote
  // them: the job in the manifest is the contract with the user, so enforce it
  // here rather than trusting the payload.
  if (out.job == Job::Everything)
  {
    if (const std::string* settings = archive.Find(kSettingsEntry))
      out.settingsJson = *settings;
    out.includesMidiSoundMap = !out.library.midiSoundMap.empty();
  }
  else
  {
    out.library.midiSoundMap.clear();
  }
  out.library.legacyCustomScenes.clear(); // never travels: the rig belongs to the instance

  out.ok = true;
  return out;
}

inline PackContents OpenPack(const std::filesystem::path& path)
{
  return ReadPackFromArchive(ReadArchiveFromFile(path));
}

// ---------------------------------------------------------------------------
// Import: preview
// ---------------------------------------------------------------------------

struct ImportPreview
{
  std::vector<std::string> adds;
  std::vector<std::string> replaces;
  // Same name, different id. Not a replace: both are kept, and the preview says so,
  // because silently merging two people's "Plexi" loses one of them.
  std::vector<std::string> nameCollisions;
  std::vector<std::string> removals; // Reset only
  std::vector<std::string> inUseReloads; // ids this instance is currently playing
  bool writesSettings = false;
  bool replacesMidiSoundMap = false;

  bool Empty() const { return adds.empty() && replaces.empty() && removals.empty(); }
};

// `soundingIds` is whatever this instance's rig is playing (custom amp ids, IR ids,
// pedal ids). Empty from a headless caller.
inline ImportPreview BuildImportPreview(const content::Registry& current, const PackContents& packContents,
                                        ImportVerb verb, bool alsoSettings, bool standalone,
                                        const std::vector<std::string>& soundingIds = {})
{
  using namespace detail;
  ImportPreview out;
  const auto& incoming = packContents.library;

  auto sounding = [&soundingIds](const std::string& id) {
    return !id.empty() && std::find(soundingIds.begin(), soundingIds.end(), id) != soundingIds.end();
  };

  auto note = [&](bool present, const std::string& label, const std::string& id) {
    if (!present)
      out.adds.push_back(label);
    else if (verb == ImportVerb::Add)
      return; // keep mine: nothing happens to this item
    else
    {
      out.replaces.push_back(label);
      if (sounding(id))
        out.inUseReloads.push_back(label);
    }
  };

  for (const auto& a : incoming.amps)
  {
    note(FindAmp(current, a.id) != nullptr, "Custom amp \"" + a.name + "\"", a.id);
    if (!FindAmp(current, a.id))
      for (const auto& mine : current.amps)
        if (mine.id != a.id && mine.name == a.name)
          out.nameCollisions.push_back("Custom amp \"" + a.name + "\"");
  }
  for (const auto& ir : incoming.irs)
  {
    note(FindIr(current, ir.id) != nullptr, "IR \"" + ir.name + "\"", ir.id);
    if (!FindIr(current, ir.id))
      for (const auto& mine : current.irs)
        if (mine.id != ir.id && mine.name == ir.name)
          out.nameCollisions.push_back("IR \"" + ir.name + "\"");
  }
  for (const auto& p : incoming.pedals)
  {
    note(FindPedal(current, p.id) != nullptr, "Pedal \"" + p.name + "\"", p.id);
    if (!FindPedal(current, p.id))
      for (const auto& mine : current.pedals)
        if (mine.id != p.id && mine.name == p.name)
          out.nameCollisions.push_back("Pedal \"" + p.name + "\"");
  }
  for (const auto& bank : incoming.presetBanks)
    for (const auto& pr : bank.second)
    {
      std::string owner;
      size_t at = 0;
      const bool present = FindPreset(current, pr.id, owner, at);
      note(present, "Preset \"" + pr.name + "\"", pr.id);
      if (!present)
      {
        const auto mine = current.presetBanks.find(bank.first);
        if (mine != current.presetBanks.end())
          for (const auto& other : mine->second)
            if (other.id != pr.id && other.name == pr.name)
              out.nameCollisions.push_back("Preset \"" + pr.name + "\"");
      }
    }

  if (verb == ImportVerb::Reset)
  {
    // Named, not counted: "3 items will be deleted" is not consent.
    for (const auto& a : current.amps)
      if (!FindAmp(incoming, a.id))
        out.removals.push_back("Custom amp \"" + a.name + "\"");
    for (const auto& ir : current.irs)
      if (!FindIr(incoming, ir.id))
        out.removals.push_back("IR \"" + ir.name + "\"");
    for (const auto& p : current.pedals)
      if (!FindPedal(incoming, p.id))
        out.removals.push_back("Pedal \"" + p.name + "\"");
    for (const auto& bank : current.presetBanks)
      for (const auto& pr : bank.second)
      {
        std::string owner;
        size_t at = 0;
        if (!FindPreset(incoming, pr.id, owner, at))
          out.removals.push_back("Preset \"" + pr.name + "\"");
      }
  }

  // Machine settings are standalone-only and ride their own checkbox, never one of
  // the three verbs. A plugin has no volum-settings.json to write.
  out.writesSettings = standalone && alsoSettings && !packContents.settingsJson.empty();
  out.replacesMidiSoundMap = standalone && alsoSettings && packContents.includesMidiSoundMap;
  return out;
}

// ---------------------------------------------------------------------------
// Import: transactional apply
// ---------------------------------------------------------------------------

struct ImportResult
{
  bool ok = false;
  std::string error;
  std::vector<std::string> replacedIds; // ids whose payload changed, for a rig reload
  std::filesystem::path backupPath; // the prior library file, kept

  explicit operator bool() const { return ok; }
};

// Validate, stage, swap. A Pack that fails validation changes nothing; a Pack that
// fails at the registry write leaves the prior library file in `backupPath` and the
// on-disk registry untouched (ContentStore::Save is atomic).
inline ImportResult ApplyPack(content::ContentStore& store, const PackContents& packContents, ImportVerb verb,
                              bool alsoSettings, bool standalone, const std::filesystem::path& settingsPath = {})
{
  using namespace detail;
  ImportResult out;
  if (!packContents)
  {
    out.error = packContents.error.empty() ? "This Pack could not be read." : packContents.error;
    return out;
  }

  std::error_code ec;
  const auto base = store.BaseDir();
  if (base.empty())
  {
    out.error = "No content library folder.";
    return out;
  }

  // 1. Stage every payload file beside the library. Nothing in the live tree is
  // touched yet, so a failure here is a no-op for the user.
  const auto stage = base / ".volumpack-stage";
  std::filesystem::remove_all(stage, ec);
  for (const auto& f : packContents.files)
  {
    if (!content::IsSafeStoredRelPath(f.first))
    {
      std::filesystem::remove_all(stage, ec);
      out.error = "This Pack contains an unsafe file path.";
      return out;
    }
    if (!WriteWholeFile(stage / content::PathFromUtf8(f.first), f.second))
    {
      std::filesystem::remove_all(stage, ec);
      out.error = "Could not stage the Pack's files.";
      return out;
    }
  }

  // 2. Back up the library file before anything replaces it.
  const auto backup = base / "volum-content.json.packbak";
  if (std::filesystem::exists(store.RegistryPath(), ec))
  {
    std::filesystem::remove(backup, ec);
    std::filesystem::copy_file(store.RegistryPath(), backup, ec);
    if (ec)
    {
      std::filesystem::remove_all(stage, ec);
      out.error = "Could not back up your library before importing.";
      return out;
    }
    out.backupPath = backup;
  }

  // 3. Swap the staged files into the live tree.
  for (const auto& f : packContents.files)
  {
    const auto dst = store.ResolveStored(f.first);
    if (dst.empty())
      continue;
    std::filesystem::create_directories(dst.parent_path(), ec);
    std::filesystem::remove(dst, ec);
    std::filesystem::rename(stage / content::PathFromUtf8(f.first), dst, ec);
    if (ec)
    {
      // Rename across devices can fail; a copy is still correct here.
      ec.clear();
      std::filesystem::copy_file(
        stage / content::PathFromUtf8(f.first), dst, std::filesystem::copy_options::overwrite_existing, ec);
      if (ec)
      {
        std::filesystem::remove_all(stage, ec);
        out.error = "Could not write the Pack's files into your library.";
        return out;
      }
    }
  }
  std::filesystem::remove_all(stage, ec);

  // 4. Merge the library. Under ContentStore::Save's lock this is one more catalog
  // writer, so a concurrent standalone import and DAW preset save both survive.
  auto& reg = store.reg();
  const auto& incoming = packContents.library;
  const bool packWins = verb != ImportVerb::Add;

  auto mergeVector = [&](auto& mine, const auto& theirs, auto idOf) {
    for (const auto& item : theirs)
    {
      bool found = false;
      for (auto& existing : mine)
        if (idOf(existing) == idOf(item))
        {
          found = true;
          if (packWins)
          {
            existing = item;
            out.replacedIds.push_back(idOf(item));
          }
          break;
        }
      if (!found)
        mine.push_back(item);
    }
  };

  mergeVector(reg.amps, incoming.amps, [](const custom::CustomAmp& a) { return a.id; });
  mergeVector(reg.irs, incoming.irs, [](const content::IRItem& i) { return i.id; });

  // Pedals are the one item referenced by something other than its id: a PRE slot
  // holds a capture *index*. Two libraries that each minted index 64 would collide
  // on import, and the loser's presets would silently play the winner's pedal, so
  // an incoming pedal may have to be renumbered and the Pack's own presets remapped
  // to follow it.
  reg.nextPedalIndex = std::max(reg.nextPedalIndex, incoming.nextPedalIndex);
  std::map<int, int> pedalIndexRemap;
  for (const auto& theirs : incoming.pedals)
  {
    content::PedalItem* mine = nullptr;
    for (auto& p : reg.pedals)
      if (p.id == theirs.id)
      {
        mine = &p;
        break;
      }
    if (mine)
    {
      // Same pedal, already here. Its local index is what local presets and scenes
      // point at, so that index survives the replace whatever the Pack says.
      const int keep = mine->legacyIndex;
      if (packWins)
      {
        *mine = theirs;
        mine->legacyIndex = keep;
        out.replacedIds.push_back(theirs.id);
      }
      if (theirs.legacyIndex != keep)
        pedalIndexRemap[theirs.legacyIndex] = keep;
      continue;
    }
    bool taken = false;
    for (const auto& p : reg.pedals)
      if (p.legacyIndex == theirs.legacyIndex)
        taken = true;
    content::PedalItem added = theirs;
    if (taken || added.legacyIndex < content::kCustomPedalIndexBase)
    {
      added.legacyIndex = std::max(reg.nextPedalIndex, content::kCustomPedalIndexBase);
      pedalIndexRemap[theirs.legacyIndex] = added.legacyIndex;
    }
    reg.nextPedalIndex = std::max(reg.nextPedalIndex, added.legacyIndex + 1);
    reg.pedals.push_back(added);
  }

  auto remapPedals = [&pedalIndexRemap](VoLumAmpSettings& s) {
    for (int* capture : {&s.preNam1Capture, &s.preNam2Capture})
    {
      const auto it = pedalIndexRemap.find(*capture);
      if (it != pedalIndexRemap.end())
        *capture = it->second;
    }
  };

  for (const auto& bank : incoming.presetBanks)
  {
    auto& mine = reg.presetBanks[bank.first];
    for (const auto& incomingPreset : bank.second)
    {
      content::Preset pr = incomingPreset;
      remapPedals(pr.settings);
      bool found = false;
      for (auto& existing : mine)
        if (existing.id == pr.id)
        {
          found = true;
          if (packWins)
            existing = pr;
          break;
        }
      if (!found)
        mine.push_back(pr);
    }
  }

  if (verb == ImportVerb::Reset)
  {
    // Collect first, delete after: the Remove* methods sweep references and erase
    // from the very vectors being walked.
    std::vector<std::string> dropAmps, dropIrs, dropPedals;
    for (const auto& a : reg.amps)
      if (!FindAmp(incoming, a.id))
        dropAmps.push_back(a.id);
    for (const auto& ir : reg.irs)
      if (!FindIr(incoming, ir.id))
        dropIrs.push_back(ir.id);
    for (const auto& p : reg.pedals)
      if (!FindPedal(incoming, p.id))
        dropPedals.push_back(p.id);
    std::vector<std::pair<std::string, std::string>> dropPresets;
    for (const auto& bank : reg.presetBanks)
      for (const auto& pr : bank.second)
      {
        std::string owner;
        size_t at = 0;
        if (!FindPreset(incoming, pr.id, owner, at))
          dropPresets.push_back({bank.first, pr.id});
      }
    for (const auto& e : dropPresets)
    {
      auto& mine = reg.presetBanks[e.first];
      mine.erase(std::remove_if(mine.begin(), mine.end(), [&e](const content::Preset& p) { return p.id == e.second; }),
                 mine.end());
    }
    for (const auto& id : dropAmps)
      store.RemoveCustomAmp(id);
    for (const auto& id : dropIrs)
      store.RemoveIR(id);
    for (const auto& id : dropPedals)
      store.RemovePedal(id);
  }

  // 5. MIDI slots and machine settings ride the standalone checkbox, not the verbs.
  const bool applySettings = standalone && alsoSettings;
  if (applySettings && packContents.includesMidiSoundMap)
    reg.midiSoundMap = incoming.midiSoundMap;

  if (!store.Save())
  {
    out.error = "Your library could not be saved - the import was not applied.";
    return out;
  }

  if (applySettings && !packContents.settingsJson.empty() && !settingsPath.empty())
  {
    if (!WriteWholeFile(settingsPath, packContents.settingsJson))
    {
      out.error = "The library was imported, but the machine settings could not be written.";
      return out;
    }
  }

  out.ok = true;
  return out;
}

} // namespace volum::pack
