#pragma once

// VoLum: one cache of parsed .nam files for the whole process, shared by every
// plugin instance's loader thread, so twenty instances of one amp parse each
// capture once. Entries are immutable shared_ptr<const nam::dspData>: a loader
// takes one under the mutex, then copies it and builds its model with no lock
// held, so an entry evicted (or an instance destroyed) meanwhile stays alive
// until its last holder drops it. Loader threads only; never the audio thread.

#include <algorithm>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "VoLumNamDspData.h"

namespace volum
{
namespace nam_cache
{

// Room for two instances' working sets (loaded model + kPrefetchMaxEntries
// siblings) before one evicts another's; instances on the same amp share one.
// Memory bound: a bundled capture (~300 KB file) parses to about 0.35 MB
// (weights as floats plus the JSON config; 24 took 8 MiB of private bytes on
// Windows), so a full cache holds about 5.5 MB for the whole process, where
// each instance used to keep up to 8 entries (about 2.7 MB) of its own.
inline constexpr size_t kSharedDspCacheMaxEntries = 2 * kDspCacheMaxEntries;

// Canonical path, size and modification time, so a file rewritten in place
// misses. Empty when the file cannot be read (the parse then reports why).
inline std::string DspCacheKey(const std::string& utf8Path)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path given = fs::u8path(utf8Path);
  fs::path path = fs::weakly_canonical(given, ec);
  if (ec)
    path = given;
  const auto size = fs::file_size(path, ec);
  if (ec)
    return {};
  const auto mtime = fs::last_write_time(path, ec);
  if (ec)
    return {};
#if defined(__cpp_char8_t)
  const std::u8string u8 = path.u8string();
  std::string key(reinterpret_cast<const char*>(u8.data()), u8.size());
#else
  std::string key = path.u8string();
#endif
  key += '\n';
  key += std::to_string(size);
  key += '\n';
  key += std::to_string(static_cast<long long>(mtime.time_since_epoch().count()));
  return key;
}

class SharedDspCache
{
public:
  using Entry = std::shared_ptr<const nam::dspData>;

  explicit SharedDspCache(size_t maxEntries = kSharedDspCacheMaxEntries)
  : mMaxEntries(maxEntries)
  {
  }

  SharedDspCache(const SharedDspCache&) = delete;
  SharedDspCache& operator=(const SharedDspCache&) = delete;

  // A hit moves the key to the front of the LRU.
  Entry Find(const std::string& key)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    const auto it = mEntries.find(key);
    if (it == mEntries.end())
      return nullptr;
    TouchLocked(key);
    return it->second;
  }

  bool Contains(const std::string& key) const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mEntries.find(key) != mEntries.end();
  }

  void Touch(const std::string& key)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mEntries.find(key) != mEntries.end())
      TouchLocked(key);
  }

  // Keeps an entry another loader stored first and returns whichever is cached.
  Entry Store(const std::string& key, Entry entry)
  {
    std::vector<Entry> evicted;
    Entry stored;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      auto& slot = mEntries[key];
      if (!slot)
        slot = std::move(entry);
      stored = slot;
      TouchLocked(key);
      while (mOrder.size() > mMaxEntries)
      {
        const auto it = mEntries.find(mOrder.back());
        evicted.push_back(std::move(it->second));
        mEntries.erase(it);
        mOrder.pop_back();
      }
    }
    // The last reference to an evicted parse is freed here, outside the lock.
    return stored;
  }

  // Claims a prefetch parse. False when the key is cached or another loader
  // is already parsing it; a true claim must be closed with EndFetch.
  bool BeginFetch(const std::string& key)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mEntries.find(key) != mEntries.end())
      return false;
    return mInFlight.insert(key).second;
  }

  void EndFetch(const std::string& key)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mInFlight.erase(key);
  }

  size_t Size() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mEntries.size();
  }

private:
  void TouchLocked(const std::string& key)
  {
    mOrder.erase(std::remove(mOrder.begin(), mOrder.end(), key), mOrder.end());
    mOrder.push_front(key);
  }

  const size_t mMaxEntries;
  mutable std::mutex mMutex;
  std::unordered_map<std::string, Entry> mEntries;
  std::deque<std::string> mOrder;
  std::unordered_set<std::string> mInFlight;
};

inline SharedDspCache& GlobalDspCache()
{
  static SharedDspCache cache;
  return cache;
}

// The parsed `utf8Path`, from the cache or parsed now and stored. `parsed`
// reports a miss. Throws what FillDspDataFromNamFile throws.
inline SharedDspCache::Entry AcquireDspData(SharedDspCache& cache, const std::string& utf8Path, bool* parsed = nullptr)
{
  const std::string key = DspCacheKey(utf8Path);
  if (!key.empty())
  {
    if (auto hit = cache.Find(key))
    {
      if (parsed)
        *parsed = false;
      return hit;
    }
  }
  auto conf = std::make_shared<nam::dspData>();
  FillDspDataFromNamFile(std::filesystem::u8path(utf8Path), *conf);
  if (parsed)
    *parsed = true;
  if (key.empty())
    return conf;
  return cache.Store(key, std::move(conf));
}

// Parses `utf8Path` into the cache unless it is cached or another loader is
// parsing it. Returns whether this call parsed it.
inline bool PrefetchDspData(SharedDspCache& cache, const std::string& utf8Path)
{
  const std::string key = DspCacheKey(utf8Path);
  if (key.empty() || !cache.BeginFetch(key))
    return false;
  struct FetchClaim
  {
    SharedDspCache& cache;
    const std::string& key;
    ~FetchClaim() { cache.EndFetch(key); }
  } claim{cache, key};
  auto conf = std::make_shared<nam::dspData>();
  FillDspDataFromNamFile(std::filesystem::u8path(utf8Path), *conf);
  cache.Store(key, std::move(conf));
  return true;
}

} // namespace nam_cache
} // namespace volum
