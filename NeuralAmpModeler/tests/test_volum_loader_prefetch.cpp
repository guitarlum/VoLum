#include "third_party/doctest.h"

#include "../VoLumNamDspData.h"

#include "activations.h"
#include "get_dsp.h"

#include <algorithm>
#include <chrono>
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

TEST_CASE("VoLum loader uses FillDspDataFromNamFile and capped SelectPrefetchPaths")
{
  const std::string loader = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumLoader.inc.cpp");
  REQUIRE(loader.find("FillDspDataFromNamFile") != std::string::npos);
  REQUIRE(loader.find("SelectPrefetchPaths") != std::string::npos);
  REQUIRE(loader.find("kPrefetchMaxEntries") != std::string::npos);
  // Prefetch must not call the path overload that builds twice.
  const auto prefetch = loader.find("else if (request.kind == VoLumLoadKind::MainPrefetch)");
  REQUIRE(prefetch != std::string::npos);
  const auto prefetchBody = loader.substr(prefetch, 600);
  REQUIRE(prefetchBody.find("FillDspDataFromNamFile") != std::string::npos);
  REQUIRE(prefetchBody.find("nam::get_dsp(fs::u8path") == std::string::npos);
}

TEST_CASE("Loader parse-only prefetch is faster than get_dsp(path, conf) discard")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto siblings = ListAmpeteNamPaths();
  REQUIRE(siblings.size() >= 8);

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

  const auto toPrefetch =
    volum::nam_cache::SelectPrefetchPaths(loaded, siblings, volum::nam_cache::kPrefetchMaxEntries);

  // Warm the filesystem once so both timings measure parse/build, not cold disk.
  for (const auto& p : toPrefetch)
  {
    nam::dspData warm;
    volum::nam_cache::FillDspDataFromNamFile(p, warm);
  }

  using clock = std::chrono::steady_clock;
  const auto beforeStart = clock::now();
  for (const auto& p : siblings)
  {
    if (p == loaded)
      continue;
    nam::dspData conf;
    // Old prefetch: build (twice inside get_dsp) and discard the model.
    (void)nam::get_dsp(std::filesystem::path(p), conf);
  }
  const auto beforeMs = std::chrono::duration<double, std::milli>(clock::now() - beforeStart).count();

  const auto afterStart = clock::now();
  for (const auto& p : toPrefetch)
  {
    nam::dspData conf;
    volum::nam_cache::FillDspDataFromNamFile(p, conf);
  }
  const auto afterMs = std::chrono::duration<double, std::milli>(clock::now() - afterStart).count();

  INFO("before_ms=" << beforeMs << " after_ms=" << afterMs << " siblings=" << (siblings.size() - 1)
                    << " prefetch_cap=" << toPrefetch.size());
  {
    const auto evidenceDir = RepoRoot() / ".scratch" / "perf-safe-wins" / "evidence" / "18";
    std::error_code ec;
    std::filesystem::create_directories(evidenceDir, ec);
    std::ofstream out(evidenceDir / "timing.txt", std::ios::binary);
    out << "before_loader_ms_per_amp_switch_old_prefetch=" << beforeMs << "\n";
    out << "after_loader_ms_per_amp_switch_parse_only_capped=" << afterMs << "\n";
    out << "old_sibling_count=" << (siblings.size() - 1) << "\n";
    out << "new_prefetch_count=" << toPrefetch.size() << "\n";
  }
  CHECK(afterMs < beforeMs);
  // Keep a clear gap so OS noise cannot flip the verdict on a warm cache.
  CHECK(afterMs * 2.0 < beforeMs);
}
