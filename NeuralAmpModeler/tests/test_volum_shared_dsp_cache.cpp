#include "third_party/doctest.h"

#include "../VoLumSharedDspCache.h"

#include "activations.h"
#include "get_dsp.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <psapi.h>
#endif

// The parsed-.nam cache is one per process: every plugin instance's loader
// thread reads and fills it. Threads stand in for instances here.

namespace
{
namespace fs = std::filesystem;
using volum::nam_cache::AcquireDspData;
using volum::nam_cache::DspCacheKey;
using volum::nam_cache::GlobalDspCache;
using volum::nam_cache::PrefetchDspData;
using volum::nam_cache::SharedDspCache;

fs::path RepoRoot()
{
  return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

fs::path Rig(const char* name)
{
  return RepoRoot() / "rigs" / "Ampete One" / name;
}

std::string Utf8(const fs::path& p)
{
#if defined(__cpp_char8_t)
  const std::u8string u8 = p.u8string();
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
#else
  return p.u8string();
#endif
}

std::string ReadText(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// The global cache outlives a test case, so every case parses copies under a
// directory no other case uses.
class ScratchDir
{
public:
  explicit ScratchDir(const char* tag)
  {
    static std::atomic<int> counter{0};
    dir = fs::temp_directory_path()
          / ("volum-dsp-cache-" + std::string(tag) + "-"
             + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-"
             + std::to_string(counter++));
    fs::create_directories(dir);
  }
  ~ScratchDir()
  {
    std::error_code ec;
    fs::remove_all(dir, ec);
  }
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;

  std::string Copy(const char* rig)
  {
    const fs::path to = dir / rig;
    fs::copy_file(Rig(rig), to);
    return Utf8(to);
  }

  fs::path dir;
};

std::vector<NAM_SAMPLE> Render(nam::DSP& model)
{
  model.Reset(48000.0, 64);
  std::vector<NAM_SAMPLE> in(2048), out(2048, 0);
  for (size_t i = 0; i < in.size(); ++i)
    in[i] = static_cast<NAM_SAMPLE>(0.2 * std::sin(0.031 * static_cast<double>(i)));
  for (size_t off = 0; off < in.size(); off += 64)
  {
    NAM_SAMPLE* ip = in.data() + off;
    NAM_SAMPLE* op = out.data() + off;
    model.process(&ip, &op, 64);
  }
  return out;
}

// What the loader's makeModel does with a shared entry.
std::vector<NAM_SAMPLE> RenderShared(const SharedDspCache::Entry& entry)
{
  nam::dspData copy = *entry;
  auto model = nam::get_dsp(copy);
  REQUIRE(model != nullptr);
  return Render(*model);
}

std::vector<NAM_SAMPLE> RenderDirect(const std::string& path)
{
  auto model = nam::get_dsp(fs::u8path(path));
  REQUIRE(model != nullptr);
  return Render(*model);
}

bool SameBits(const std::vector<NAM_SAMPLE>& a, const std::vector<NAM_SAMPLE>& b)
{
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(NAM_SAMPLE)) == 0;
}
} // namespace

TEST_CASE("Shared NAM cache: two instances' loaders parse a capture once")
{
  nam::activations::Activation::enable_fast_tanh();
  ScratchDir scratch("share");
  const std::string path = scratch.Copy("G12-Ampt-2.nam");

  SharedDspCache::Entry a, b;
  bool parsedA = false, parsedB = true, prefetched = true;
  std::thread([&]() { a = AcquireDspData(GlobalDspCache(), path, &parsedA); }).join();
  std::thread([&]() { b = AcquireDspData(GlobalDspCache(), path, &parsedB); }).join();
  std::thread([&]() { prefetched = PrefetchDspData(GlobalDspCache(), path); }).join();

  CHECK(parsedA);
  CHECK_FALSE(parsedB);
  CHECK(a == b);
  CHECK_FALSE(prefetched);
  CHECK(SameBits(RenderShared(b), RenderDirect(path)));
}

TEST_CASE("Shared NAM cache: a capture rewritten in place is parsed again")
{
  ScratchDir scratch("key");
  const std::string path = scratch.Copy("G12-Ampt-2.nam");
  const fs::path file = fs::u8path(path);
  SharedDspCache cache;
  bool parsed = false;

  const auto first = AcquireDspData(cache, path, &parsed);
  CHECK(parsed);
  AcquireDspData(cache, path, &parsed);
  CHECK_FALSE(parsed);
  AcquireDspData(cache, Utf8(scratch.dir / "." / "G12-Ampt-2.nam"), &parsed);
  CHECK_FALSE(parsed);

  const auto mtime = fs::last_write_time(file);
  fs::last_write_time(file, mtime + std::chrono::seconds(5));
  AcquireDspData(cache, path, &parsed);
  CHECK(parsed);

  // One byte longer, back at the first parse's modification time.
  {
    std::ofstream(file, std::ios::binary | std::ios::app) << ' ';
  }
  fs::last_write_time(file, mtime);
  AcquireDspData(cache, path, &parsed);
  CHECK(parsed);

  fs::copy_file(Rig("G12-Ampt-3.nam"), file, fs::copy_options::overwrite_existing);
  const auto replaced = AcquireDspData(cache, path, &parsed);
  CHECK(parsed);
  CHECK_FALSE((replaced->config == first->config && replaced->weights == first->weights));
}

TEST_CASE("Shared NAM cache: an entry outlives its eviction and the instance that loaded it")
{
  nam::activations::Activation::enable_fast_tanh();
  ScratchDir scratch("life");
  const std::string p1 = scratch.Copy("G12-Ampt-1.nam");
  const std::string p2 = scratch.Copy("G12-Ampt-2.nam");
  const std::string p3 = scratch.Copy("G12-Ampt-3.nam");
  SharedDspCache cache(2);

  // One instance's loader parses p1 and the instance goes away; another
  // instance's loads then evict p1 while a third still builds from it.
  SharedDspCache::Entry held;
  std::thread([&]() { held = AcquireDspData(cache, p1); }).join();
  std::thread([&]() {
    AcquireDspData(cache, p2);
    AcquireDspData(cache, p3);
  }).join();
  CHECK_FALSE(cache.Contains(DspCacheKey(p1)));
  CHECK(cache.Size() == 2);
  REQUIRE(held != nullptr);
  CHECK(SameBits(RenderShared(held), RenderDirect(p1)));

  // Loaders racing on a cache too small for their captures: every model built
  // from whatever entry it got renders the file's own bits.
  const std::string paths[] = {p1, p2, p3};
  std::vector<std::vector<NAM_SAMPLE>> reference;
  for (const auto& p : paths)
    reference.push_back(RenderDirect(p));
  std::atomic<int> mismatches{0};
  std::vector<std::thread> loaders;
  for (int t = 0; t < 4; ++t)
  {
    loaders.emplace_back([&, t]() {
      for (int i = 0; i < 6; ++i)
      {
        const int which = (t + i) % 3;
        const auto entry = AcquireDspData(cache, paths[which]);
        if (!SameBits(RenderShared(entry), reference[static_cast<size_t>(which)]))
          ++mismatches;
      }
    });
  }
  for (auto& l : loaders)
    l.join();
  CHECK(mismatches.load() == 0);
  CHECK(cache.Size() <= 2);
}

TEST_CASE("Shared NAM cache: prefetch skips captures cached or being parsed elsewhere")
{
  ScratchDir scratch("fetch");
  const std::string p1 = scratch.Copy("G12-Ampt-1.nam");
  const std::string p2 = scratch.Copy("G12-Ampt-2.nam");
  const std::string p3 = scratch.Copy("G12-Ampt-3.nam");
  SharedDspCache cache(2);

  const std::string key = DspCacheKey(p1);
  REQUIRE_FALSE(key.empty());
  REQUIRE(cache.BeginFetch(key));
  CHECK_FALSE(cache.BeginFetch(key));
  CHECK_FALSE(PrefetchDspData(cache, p1));
  cache.EndFetch(key);

  CHECK(PrefetchDspData(cache, p1));
  CHECK(cache.Contains(key));
  CHECK_FALSE(PrefetchDspData(cache, p1));

  CHECK(PrefetchDspData(cache, p2));
  CHECK(PrefetchDspData(cache, p3));
  CHECK_FALSE(cache.Contains(key));
  CHECK(PrefetchDspData(cache, p1));

  // A failed parse gives its claim back.
  const fs::path broken = scratch.dir / "broken.nam";
  std::ofstream(broken, std::ios::binary) << R"({"version": "0.5.4", "architecture": "WaveNet", "config": {}})";
  const std::string brokenPath = Utf8(broken);
  CHECK_THROWS(PrefetchDspData(cache, brokenPath));
  CHECK(cache.BeginFetch(DspCacheKey(brokenPath)));

  CHECK(DspCacheKey(Utf8(scratch.dir / "missing.nam")).empty());
}

TEST_CASE("Shared NAM cache: only loader threads reach it")
{
  const fs::path src = RepoRoot() / "NeuralAmpModeler";
  for (const auto& e : fs::directory_iterator(src))
  {
    const auto ext = e.path().extension();
    if (ext != ".h" && ext != ".cpp")
      continue;
    const std::string name = e.path().filename().string();
    if (name == "VoLumSharedDspCache.h" || name == "VoLumLoader.inc.cpp")
      continue;
    const std::string text = ReadText(e.path());
    INFO(name);
    for (const char* token : {"GlobalDspCache", "AcquireDspData", "PrefetchDspData", "BeginFetch"})
      CHECK(text.find(token) == std::string::npos);
  }

  // In the loader, every use sits in _VolumQueueMainPrefetch or the loader
  // thread's main, never in the drain that runs on the audio thread.
  const std::string loader = ReadText(src / "VoLumLoader.inc.cpp");
  int uses = 0;
  for (const char* token : {"GlobalDspCache", "dspCache", "DspCacheKey", "AcquireDspData", "PrefetchDspData"})
  {
    for (size_t at = loader.find(token); at != std::string::npos; at = loader.find(token, at + 1))
    {
      const size_t fn = loader.rfind("void NeuralAmpModeler::", at);
      REQUIRE(fn != std::string::npos);
      const std::string owner = loader.substr(fn, loader.find('(', fn) - fn);
      INFO(token << " in " << owner);
      CHECK((owner == "void NeuralAmpModeler::_VolumQueueMainPrefetch"
             || owner == "void NeuralAmpModeler::_VolumLoaderThreadMain"));
      ++uses;
    }
  }
  CHECK(uses > 0);
  CHECK(ReadText(src / "NeuralAmpModeler.h").find("mVolumDspCache") == std::string::npos);
}

TEST_CASE("Shared NAM cache: parse cost and memory of a full cache (info)")
{
  nam::activations::Activation::enable_fast_tanh();
  std::vector<std::string> paths;
  for (const auto& e : fs::recursive_directory_iterator(RepoRoot() / "rigs"))
    if (e.path().extension() == ".nam")
      paths.push_back(Utf8(e.path()));
  std::sort(paths.begin(), paths.end());
  REQUIRE(paths.size() >= volum::nam_cache::kSharedDspCacheMaxEntries);
  paths.resize(volum::nam_cache::kSharedDspCacheMaxEntries);

  using Clock = std::chrono::steady_clock;
  auto ms = [](Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); };
  SharedDspCache cache;
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX before{};
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before), sizeof(before));
#endif
  const auto t0 = Clock::now();
  size_t estimate = 0;
  for (const auto& p : paths)
  {
    const auto entry = AcquireDspData(cache, p);
    estimate += entry->weights.size() * sizeof(float) + entry->config.dump().size() + entry->metadata.dump().size();
  }
  const auto t1 = Clock::now();
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX after{};
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after), sizeof(after));
  const double privateMiB =
    (static_cast<double>(after.PrivateUsage) - static_cast<double>(before.PrivateUsage)) / 1048576.0;
#else
  const double privateMiB = 0.0;
#endif
  for (const auto& p : paths)
    AcquireDspData(cache, p);
  const auto t2 = Clock::now();
  nam::dspData copy = *AcquireDspData(cache, paths.front());
  const auto t3 = Clock::now();
  auto model = nam::get_dsp(copy);
  const auto t4 = Clock::now();
  REQUIRE(model != nullptr);

  const double n = static_cast<double>(paths.size());
  MESSAGE("INFO: " << paths.size() << " captures: parse " << ms(t1 - t0) / n << " ms each, hit " << ms(t2 - t1) / n
                   << " ms each, copy " << ms(t3 - t2) << " ms, build " << ms(t4 - t3) << " ms; full cache "
                   << privateMiB << " MiB private bytes, " << static_cast<double>(estimate) / 1048576.0
                   << " MiB weights + JSON text");
}
