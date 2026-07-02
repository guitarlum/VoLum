#include "third_party/doctest.h"

#include "../VoLumDspCacheParams.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace
{
std::filesystem::path DspCacheRepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string DspCacheReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
} // namespace

TEST_CASE("Restore-reapplied cache list is the expected cached DSP param set")
{
  // Pin the set so adding a new cached param is a conscious act (extend
  // OnParamChange, _VolumApplyDspCaches, and this list together).
  CHECK(volum::dsp_cache::kRestoreReappliedCaches.size() == 9);

  std::set<std::string> names;
  for (const char* n : volum::dsp_cache::kRestoreReappliedCaches)
    names.insert(n);
  CHECK(names.size() == 9); // no duplicates

  CHECK(volum::dsp_cache::IsRestoreReappliedCache("Output"));
  CHECK(volum::dsp_cache::IsRestoreReappliedCache("Input"));
  CHECK(volum::dsp_cache::IsRestoreReappliedCache("SupportOutput"));
  CHECK(volum::dsp_cache::IsRestoreReappliedCache("SupportTreble"));
  CHECK_FALSE(volum::dsp_cache::IsRestoreReappliedCache("DelayMix"));
  CHECK_FALSE(volum::dsp_cache::IsRestoreReappliedCache(nullptr));
}

TEST_CASE("Each cached param name is a real parameter in NeuralAmpModeler.cpp")
{
  // If a param is renamed, this fails until the cache list is updated to match,
  // keeping the pure list honest against the actual Init*() names.
  const std::string src = DspCacheReadText(DspCacheRepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  for (const char* n : volum::dsp_cache::kRestoreReappliedCaches)
  {
    const std::string quoted = std::string("\"") + n + "\"";
    INFO(quoted);
    CHECK(src.find(quoted) != std::string::npos);
  }
}
