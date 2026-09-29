#pragma once

// VoLum: fill nam::dspData from a .nam without building a DSP, and pick which
// sibling captures to prefetch so an 8-entry LRU never evicts the model just
// loaded. Used by the loader thread (VoLumLoader.inc.cpp) and its doctests.

#include <algorithm>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
#include "VoLumCustomModel.h"

namespace volum
{
namespace nam_cache
{

// One instance's working set in the shared cache (VoLumSharedDspCache.h): the
// loaded model plus its prefetched siblings.
inline constexpr size_t kDspCacheMaxEntries = 8;
inline constexpr size_t kPrefetchMaxEntries = kDspCacheMaxEntries - 1;

// Mirror NeuralAmpModelerCore/NAM/get_dsp.cpp get_dsp(path, conf) fill of
// returnedConfig (exists check, istream >> j, version check, field copy) without
// the discarded create_dsp / get_dsp(dspData&) builds.
inline void FillDspDataFromNamFile(const std::filesystem::path& configFilename, nam::dspData& out)
{
  if (!std::filesystem::exists(configFilename))
    throw std::runtime_error("Config file doesn't exist!\n");
  std::ifstream i(configFilename);
  nlohmann::json j;
  i >> j;

  nam::verify_config_version(j["version"].get<std::string>());

  auto weightsIt = j.find("weights");
  if (weightsIt == j.end())
    throw std::runtime_error("Corrupted model file is missing weights.");

  out.version = j["version"].get<std::string>();
  out.architecture = j["architecture"].get<std::string>();
  out.config = j["config"];
  out.metadata = j.value("metadata", nlohmann::json());
  out.weights = weightsIt->get<std::vector<float>>();
  out.expected_sample_rate = nam::get_sample_rate_from_nam_file(j);
}

struct PrefetchCandidate
{
  std::string path;
  int tier = 2; // 0 same speaker, 1 same channel other cab, 2 other
  int distance = 0;
  std::string filename;
};

// Order siblings for prefetch: same speaker with adjacent channels first, then
// the same channel on other cabs. Cap at maxPrefetch (normally
// kPrefetchMaxEntries) so the loaded model stays in an 8-entry LRU.
inline std::vector<std::string> SelectPrefetchPaths(const std::string& loadedPath,
                                                    const std::vector<std::string>& siblingPaths, size_t maxPrefetch)
{
  const auto loaded = custom::ParseNamFileName(loadedPath);
  std::vector<PrefetchCandidate> candidates;
  candidates.reserve(siblingPaths.size());

  for (const auto& path : siblingPaths)
  {
    if (path.empty() || path == loadedPath)
      continue;

    PrefetchCandidate c;
    c.path = path;
    c.filename = std::filesystem::path(path).filename().string();
    const auto parsed = custom::ParseNamFileName(path);

    if (loaded.matched && parsed.matched && loaded.channel > 0 && parsed.channel > 0 && loaded.slot == parsed.slot)
    {
      c.tier = 0;
      c.distance = std::abs(parsed.channel - loaded.channel);
    }
    else if (loaded.matched && parsed.matched && loaded.channel > 0 && parsed.channel == loaded.channel)
    {
      c.tier = 1;
      c.distance = std::abs(parsed.slot - loaded.slot);
    }
    else
    {
      c.tier = 2;
      c.distance = 0;
    }
    candidates.push_back(std::move(c));
  }

  std::stable_sort(candidates.begin(), candidates.end(), [](const PrefetchCandidate& a, const PrefetchCandidate& b) {
    if (a.tier != b.tier)
      return a.tier < b.tier;
    if (a.distance != b.distance)
      return a.distance < b.distance;
    return a.filename < b.filename;
  });

  std::vector<std::string> out;
  out.reserve(std::min(maxPrefetch, candidates.size()));
  for (const auto& c : candidates)
  {
    if (out.size() >= maxPrefetch)
      break;
    out.push_back(c.path);
  }
  return out;
}

// Split a priority-ordered prefetch list. `promote` is the already-cached
// paths, lowest priority first, so touching them in order leaves the most
// likely next pick at the front of an LRU. `fetch` is the rest, best first.
struct PrefetchActions
{
  std::vector<std::string> promote;
  std::vector<std::string> fetch;
};

template <typename IsCached>
inline PrefetchActions PlanPrefetchActions(const std::vector<std::string>& selected, IsCached isCached)
{
  PrefetchActions actions;
  for (auto it = selected.rbegin(); it != selected.rend(); ++it)
  {
    if (isCached(*it))
      actions.promote.push_back(*it);
  }
  for (const auto& path : selected)
  {
    if (!isCached(path))
      actions.fetch.push_back(path);
  }
  return actions;
}

// Tiny LRU used by the cache-cap doctest (mirrors storeCache / touchCache).
inline void LruTouch(std::deque<std::string>& order, const std::string& key)
{
  order.erase(std::remove(order.begin(), order.end(), key), order.end());
  order.push_front(key);
}

inline void LruStore(std::unordered_map<std::string, int>& cache, std::deque<std::string>& order,
                     const std::string& key, size_t maxEntries)
{
  cache[key] = 1;
  LruTouch(order, key);
  while (order.size() > maxEntries)
  {
    cache.erase(order.back());
    order.pop_back();
  }
}

} // namespace nam_cache
} // namespace volum
