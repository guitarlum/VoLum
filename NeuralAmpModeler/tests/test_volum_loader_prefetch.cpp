#include "third_party/doctest.h"

#include "../VoLumNamDspData.h"

#include "activations.h"
#include "get_dsp.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::filesystem::path AmpeteDir()
{
  return RepoRoot() / "rigs" / "Ampete One";
}

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<NAM_SAMPLE> RenderBlock(nam::DSP& model, const std::vector<NAM_SAMPLE>& input)
{
  std::vector<NAM_SAMPLE> output(input.size(), static_cast<NAM_SAMPLE>(0.0));
  NAM_SAMPLE* inPtr = const_cast<NAM_SAMPLE*>(input.data());
  NAM_SAMPLE* outPtr = output.data();
  model.process(&inPtr, &outPtr, static_cast<int>(output.size()));
  return output;
}

std::vector<std::string> ListAmpeteNamPaths()
{
  namespace fs = std::filesystem;
  std::vector<std::string> paths;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(AmpeteDir(), ec))
  {
    if (!entry.is_regular_file(ec))
      continue;
    if (entry.path().extension() != ".nam")
      continue;
    paths.push_back(fs::weakly_canonical(entry.path(), ec).string());
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}
} // namespace

TEST_CASE("FillDspDataFromNamFile builds a model bit-identical to get_dsp(path, conf)")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = AmpeteDir() / "G12-Ampt-2.nam";
  REQUIRE(std::filesystem::exists(path));

  nam::dspData viaPath;
  auto refModel = nam::get_dsp(path, viaPath);
  REQUIRE(refModel != nullptr);
  refModel->Reset(48000.0, 64);

  nam::dspData viaFill;
  volum::nam_cache::FillDspDataFromNamFile(path, viaFill);

  // Every dspData field must round-trip (red if expected_sample_rate / metadata
  // / weights are dropped from Fill).
  CHECK(viaFill.version == viaPath.version);
  CHECK(viaFill.architecture == viaPath.architecture);
  CHECK(viaFill.config == viaPath.config);
  CHECK(viaFill.metadata == viaPath.metadata);
  CHECK(viaFill.weights == viaPath.weights);
  CHECK(viaFill.expected_sample_rate == viaPath.expected_sample_rate);
  REQUIRE(viaFill.expected_sample_rate > 0.0);

  nam::dspData forBuild = viaFill;
  auto fillModel = nam::get_dsp(forBuild);
  REQUIRE(fillModel != nullptr);
  fillModel->Reset(48000.0, 64);

  std::vector<NAM_SAMPLE> input(64);
  for (size_t i = 0; i < input.size(); ++i)
    input[i] = static_cast<NAM_SAMPLE>(0.05 * std::sin(0.17 * static_cast<double>(i)));

  const auto refOut = RenderBlock(*refModel, input);
  const auto fillOut = RenderBlock(*fillModel, input);
  REQUIRE(refOut.size() == fillOut.size());
  for (size_t i = 0; i < refOut.size(); ++i)
    REQUIRE(std::memcmp(&refOut[i], &fillOut[i], sizeof(NAM_SAMPLE)) == 0);
}

TEST_CASE("Prefetch queue caps at cache-size-minus-one and keeps the loaded model")
{
  using volum::nam_cache::kDspCacheMaxEntries;
  using volum::nam_cache::kPrefetchMaxEntries;
  using volum::nam_cache::LruStore;
  using volum::nam_cache::SelectPrefetchPaths;

  CHECK(kPrefetchMaxEntries == kDspCacheMaxEntries - 1);

  const auto siblings = ListAmpeteNamPaths();
  REQUIRE(siblings.size() > kDspCacheMaxEntries);

  std::string loaded;
  for (const auto& p : siblings)
  {
    if (std::filesystem::path(p).filename() == "G12-Ampt-2.nam")
    {
      loaded = p;
      break;
    }
  }
  REQUIRE_FALSE(loaded.empty());

  // Old behavior: queue every sibling in directory order -> loaded is evicted.
  {
    std::unordered_map<std::string, int> cache;
    std::deque<std::string> order;
    LruStore(cache, order, loaded, kDspCacheMaxEntries);
    for (const auto& p : siblings)
    {
      if (p == loaded)
        continue;
      LruStore(cache, order, p, kDspCacheMaxEntries);
    }
    CHECK(cache.find(loaded) == cache.end()); // red against old queueing
  }

  // New behavior: at most kPrefetchMaxEntries, ordered by likely next pick.
  const auto prefetched = SelectPrefetchPaths(loaded, siblings, kPrefetchMaxEntries);
  CHECK(prefetched.size() == kPrefetchMaxEntries);
  CHECK(std::find(prefetched.begin(), prefetched.end(), loaded) == prefetched.end());

  std::unordered_map<std::string, int> cache;
  std::deque<std::string> order;
  LruStore(cache, order, loaded, kDspCacheMaxEntries);
  for (const auto& p : prefetched)
    LruStore(cache, order, p, kDspCacheMaxEntries);

  CHECK(cache.find(loaded) != cache.end());
  CHECK(cache.size() == kDspCacheMaxEntries);

  // Same speaker adjacent channels and same channel on other cabs stay warm.
  auto expectHit = [&](const char* filename) {
    bool found = false;
    for (const auto& p : siblings)
    {
      if (std::filesystem::path(p).filename() == filename)
      {
        CHECK(cache.find(p) != cache.end());
        found = true;
        break;
      }
    }
    CHECK(found);
  };
  expectHit("G12-Ampt-1.nam");
  expectHit("G12-Ampt-3.nam");
  expectHit("AMP-Ampt-2.nam");
  expectHit("G65-Ampt-2.nam");
  expectHit("V30-Ampt-2.nam");
}

TEST_CASE("A warm channel step keeps the previous channel and its cab neighbours")
{
  using volum::nam_cache::kDspCacheMaxEntries;
  using volum::nam_cache::kPrefetchMaxEntries;
  using volum::nam_cache::LruStore;
  using volum::nam_cache::LruTouch;
  using volum::nam_cache::PlanPrefetchActions;
  using volum::nam_cache::SelectPrefetchPaths;

  const auto siblings = ListAmpeteNamPaths();
  auto pathNamed = [&](const char* filename) {
    for (const auto& p : siblings)
      if (std::filesystem::path(p).filename() == filename)
        return p;
    return std::string();
  };
  const std::string ch2 = pathNamed("G12-Ampt-2.nam");
  const std::string ch3 = pathNamed("G12-Ampt-3.nam");
  const std::string ch1 = pathNamed("G12-Ampt-1.nam");
  const std::string ch4 = pathNamed("G12-Ampt-4.nam");
  REQUIRE_FALSE(ch2.empty());
  REQUIRE_FALSE(ch3.empty());

  auto isCached = [](const std::unordered_map<std::string, int>& cache) {
    return [&](const std::string& path) { return cache.find(path) != cache.end(); };
  };

  std::unordered_map<std::string, int> cache;
  std::deque<std::string> order;
  LruStore(cache, order, ch2, kDspCacheMaxEntries);
  {
    const auto selected = SelectPrefetchPaths(ch2, siblings, kPrefetchMaxEntries);
    const auto plan = PlanPrefetchActions(selected, isCached(cache));
    for (const auto& path : plan.promote)
      LruTouch(order, path);
    for (const auto& path : plan.fetch)
      LruStore(cache, order, path, kDspCacheMaxEntries);
  }

  // Old warm step: a hit moves ch3 to the front and skips every sibling that
  // is already cached, so the new parses evict the cab you just left.
  {
    auto skipped = cache;
    auto skippedOrder = order;
    LruTouch(skippedOrder, ch3);
    const auto selected = SelectPrefetchPaths(ch3, siblings, kPrefetchMaxEntries);
    for (const auto& path : selected)
    {
      if (skipped.find(path) == skipped.end())
        LruStore(skipped, skippedOrder, path, kDspCacheMaxEntries);
    }
    CHECK(skipped.find(ch2) == skipped.end());
  }

  LruTouch(order, ch3);
  {
    const auto selected = SelectPrefetchPaths(ch3, siblings, kPrefetchMaxEntries);
    const auto plan = PlanPrefetchActions(selected, isCached(cache));
    for (const auto& path : plan.promote)
      LruTouch(order, path);
    for (const auto& path : plan.fetch)
      LruStore(cache, order, path, kDspCacheMaxEntries);
  }
  CHECK(cache.find(ch2) != cache.end());
  CHECK(cache.find(ch3) != cache.end());
  if (!ch1.empty())
    CHECK(cache.find(ch1) != cache.end());
  if (!ch4.empty())
    CHECK(cache.find(ch4) != cache.end());
}

TEST_CASE("VoLum loader uses FillDspDataFromNamFile and capped SelectPrefetchPaths")
{
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");
  REQUIRE(loader.find("FillDspDataFromNamFile") != std::string::npos);
  REQUIRE(loader.find("SelectPrefetchPaths") != std::string::npos);
  REQUIRE(loader.find("kPrefetchMaxEntries") != std::string::npos);
  REQUIRE(loader.find("PlanPrefetchActions") != std::string::npos);
  // Prefetch must not call the path overload that builds twice.
  const auto prefetch = loader.find("else if (request.kind == VoLumLoadKind::MainPrefetch)");
  REQUIRE(prefetch != std::string::npos);
  const auto prefetchBody = loader.substr(prefetch, 600);
  REQUIRE(prefetchBody.find("FillDspDataFromNamFile") != std::string::npos);
  REQUIRE(prefetchBody.find("nam::get_dsp(fs::u8path") == std::string::npos);
}
