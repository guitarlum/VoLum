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
                                      nlohmann::json& synced, std::error_code& ec, int lockTimeoutMs = 4000)
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
    ec, lockTimeoutMs);
  if (wrote)
    synced = liveShared;
  return wrote;
}

// A single-writer merge: only `keys` (an object) change in the file; every other
// key stays as the file has it. An unreadable file is left alone (returns false,
// ec clear, *unreadable = true) rather than replaced by a document holding only
// these keys. On success each key is recorded in `synced`.
inline bool MergeMachineSettingsKeys(const std::filesystem::path& settingsPath, const nlohmann::json& keys,
                                     nlohmann::json& synced, std::error_code& ec, bool* unreadable = nullptr,
                                     int lockTimeoutMs = 4000)
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
    ec, lockTimeoutMs);
  if (wrote)
  {
    if (!synced.is_object())
      synced = nlohmann::json::object();
    for (auto it = keys.begin(); it != keys.end(); ++it)
      synced[it.key()] = it.value();
  }
  return wrote;
}

// Lock waits for machine-file writes. UI-thread writes (idle, clicks) wait
// briefly and retry later; teardown and Pack export have no later, so wait longer.
constexpr int kMachineSettingsIdleLockMs = 150;
constexpr int kMachineSettingsFinalLockMs = 2000;

// Which calibration default the user edited since the last save. The two keys
// are separate settings: a level-only edit must not write this instance's
// (possibly stale) toggle over another instance's newer one, and the reverse.
class CalibrationEdits
{
public:
  void Mark(bool toggleEdited)
  {
    if (toggleEdited)
      mToggle = true;
    else
      mLevel = true;
  }
  bool Any() const { return mToggle || mLevel; }
  // The edited keys at their current values, then forgets the edits.
  nlohmann::json TakeKeys(bool calibrateInput, double inputCalibrationLevel)
  {
    nlohmann::json keys = nlohmann::json::object();
    if (mToggle)
      keys["CalibrateInput"] = calibrateInput;
    if (mLevel)
      keys["InputCalibrationLevel"] = inputCalibrationLevel;
    mToggle = mLevel = false;
    return keys;
  }

private:
  bool mToggle = false;
  bool mLevel = false;
};

// One process's (one plugin instance's) writes to the machine file: what it last
// synced per shared key, and the single keys it changed that have not reached
// disk yet. A lock timeout or write failure keeps those keys queued and backs
// off, so a busy lock costs a short wait now and a retry later, never the change.
class MachineSettingsWriter
{
public:
  static constexpr int kRetryAfterMs = 1000;

  enum class Flush
  {
    Nothing,
    Written,
    Unreadable, // left alone and dropped, as before: the next standalone save rewrites the file
    Failed // still queued
  };

  // After this process (re)loaded the file: its live values now come from disk,
  // so a key still queued from before would only overwrite them.
  void NoteLoaded(const nlohmann::json& sharedValues)
  {
    mSynced = sharedValues;
    mPending = nlohmann::json::object();
  }
  const nlohmann::json& Synced() const { return mSynced; }

  void Queue(const nlohmann::json& keys)
  {
    for (auto it = keys.begin(); it != keys.end(); ++it)
      mPending[it.key()] = it.value();
  }
  bool HasPending() const { return !mPending.empty(); }
  const nlohmann::json& Pending() const { return mPending; }

  bool RetryDue(double nowMs) const { return nowMs >= mRetryAtMs; }

  Flush FlushPending(const std::filesystem::path& settingsPath, int lockTimeoutMs, double nowMs, std::error_code& ec)
  {
    ec.clear();
    if (mPending.empty())
      return Flush::Nothing;
    bool unreadable = false;
    if (MergeMachineSettingsKeys(settingsPath, mPending, mSynced, ec, &unreadable, lockTimeoutMs))
    {
      mPending = nlohmann::json::object();
      NoteSuccess();
      return Flush::Written;
    }
    if (unreadable)
    {
      mPending = nlohmann::json::object();
      return Flush::Unreadable;
    }
    NoteFailure(nowMs);
    return Flush::Failed;
  }

  bool WriteWhole(const std::filesystem::path& settingsPath, nlohmann::json mine, int lockTimeoutMs, double nowMs,
                  std::error_code& ec)
  {
    if (WriteWholeMachineSettings(settingsPath, std::move(mine), mSynced, ec, lockTimeoutMs))
    {
      NoteSuccess();
      return true;
    }
    NoteFailure(nowMs);
    return false;
  }

  // True once per run of failures, so a wedged lock logs one line, not one per idle.
  bool TakeFailureToLog()
  {
    if (!mFailing || mFailureLogged)
      return false;
    mFailureLogged = true;
    return true;
  }

private:
  void NoteSuccess()
  {
    mFailing = false;
    mFailureLogged = false;
    mRetryAtMs = 0.0;
  }
  void NoteFailure(double nowMs)
  {
    mFailing = true;
    mRetryAtMs = nowMs + kRetryAfterMs;
  }

  nlohmann::json mSynced;
  nlohmann::json mPending = nlohmann::json::object();
  double mRetryAtMs = 0.0;
  bool mFailing = false;
  bool mFailureLogged = false;
};

} // namespace volum
