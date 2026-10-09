#pragma once

// volum-settings.json has more than one writer on a machine: the standalone
// saves the whole document, and every plugin instance (VST3 / AU, any host)
// merges single keys into it (Lite, Animate art, calibration defaults). The
// writers live in different processes. WriteJsonAtomically keeps the file whole,
// not the writes: a read-merge-write that reads before another process replaces
// the file and replaces it afterwards puts the older document back. Every writer
// therefore holds this lock from its read to its replace.

#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>

#include "VoLumContentStore.h" // content::RegistryFileLock
#include "VoLumSettingsFileIO.h"
#include "VoLumUserSettingsIO.h" // kMachineSharedKeys, KeepOtherWritersMachineKeys

namespace volum
{

enum class MachineSettingsRead
{
  Missing,
  Ok,
  Unreadable
};

// Sibling of the settings file, never the file itself: the file is replaced by
// rename on every write (same reasoning as ContentStore::LockPath).
inline std::filesystem::path MachineSettingsLockPath(const std::filesystem::path& settingsPath)
{
  auto lockPath = settingsPath;
  lockPath += ".lock";
  return lockPath;
}

// One mutex for every writer in this process; two plugin instances on two host
// threads reach the file at the same time.
inline std::mutex& MachineSettingsMutex()
{
  static std::mutex m;
  return m;
}

// Runs `fn` with the settings file locked against this process and every other
// VoLum process. False with ec = timed_out when the lock could not be taken.
template <typename Fn>
bool WithMachineSettingsLock(const std::filesystem::path& settingsPath, Fn&& fn, std::error_code& ec,
                             int lockTimeoutMs = 4000)
{
  ec.clear();
  if (settingsPath.empty())
  {
    ec = std::make_error_code(std::errc::invalid_argument);
    return false;
  }
  std::lock_guard<std::mutex> guard(MachineSettingsMutex());
  content::RegistryFileLock lock;
  if (!lock.Acquire(MachineSettingsLockPath(settingsPath), lockTimeoutMs))
  {
    ec = std::make_error_code(std::errc::timed_out);
    return false;
  }
  return fn();
}

// Locked read-modify-write. `mutate(doc, state)` edits the current document in
// place (an empty object unless state == Ok) and returns false to skip the write;
// ec stays clear in that case.
template <typename Mutate>
bool UpdateMachineSettingsFile(const std::filesystem::path& settingsPath, Mutate&& mutate, std::error_code& ec,
                               int lockTimeoutMs = 4000)
{
  bool wrote = false;
  const bool ran = WithMachineSettingsLock(
    settingsPath,
    [&]() {
      nlohmann::json doc = nlohmann::json::object();
      MachineSettingsRead state = MachineSettingsRead::Missing;
      std::error_code existsEc;
      if (std::filesystem::exists(settingsPath, existsEc))
      {
        try
        {
          std::ifstream in(settingsPath);
          in >> doc;
          state = doc.is_object() ? MachineSettingsRead::Ok : MachineSettingsRead::Unreadable;
        }
        catch (...)
        {
          state = MachineSettingsRead::Unreadable;
        }
        if (state != MachineSettingsRead::Ok)
          doc = nlohmann::json::object();
      }
      if (!mutate(doc, state))
        return true;
      wrote = WriteJsonAtomically(settingsPath, doc, ec);
      return wrote;
    },
    ec, lockTimeoutMs);
  return ran && wrote;
}

// The standalone's whole-file save. `mine` replaces the file, except that a
// kMachineSharedKeys entry this process has not changed since `synced` keeps
// the value on disk (see KeepOtherWritersMachineKeys). On success `synced`
// becomes mine's shared values, so a later save still knows what it changed.
inline bool WriteWholeMachineSettings(const std::filesystem::path& settingsPath, nlohmann::json mine,
                                      nlohmann::json& synced, std::error_code& ec)
{
  const nlohmann::json liveShared = MachineSharedKeyValues(mine);
  const bool wrote = UpdateMachineSettingsFile(
    settingsPath,
    [&](nlohmann::json& disk, MachineSettingsRead state) {
      if (state == MachineSettingsRead::Ok)
        KeepOtherWritersMachineKeys(mine, disk, synced);
      disk = std::move(mine);
      return true;
    },
    ec);
  if (wrote)
    synced = liveShared;
  return wrote;
}

// A single-writer merge: only `keys` (an object) change in the file; every other
// key stays as the file has it. An unreadable file is left alone (returns false,
// ec clear, *unreadable = true) rather than replaced by a document holding only
// these keys. On success each key is recorded in `synced`.
inline bool MergeMachineSettingsKeys(const std::filesystem::path& settingsPath, const nlohmann::json& keys,
                                     nlohmann::json& synced, std::error_code& ec, bool* unreadable = nullptr)
{
  if (unreadable)
    *unreadable = false;
  const bool wrote = UpdateMachineSettingsFile(
    settingsPath,
    [&](nlohmann::json& doc, MachineSettingsRead state) {
      if (state == MachineSettingsRead::Unreadable)
      {
        if (unreadable)
          *unreadable = true;
        return false;
      }
      if (!doc.contains("version"))
        doc["version"] = kVoLumUserSettingsVersion;
      for (auto it = keys.begin(); it != keys.end(); ++it)
        doc[it.key()] = it.value();
      return true;
    },
    ec);
  if (wrote)
  {
    if (!synced.is_object())
      synced = nlohmann::json::object();
    for (auto it = keys.begin(); it != keys.end(); ++it)
      synced[it.key()] = it.value();
  }
  return wrote;
}

} // namespace volum
