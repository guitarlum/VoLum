#include "third_party/doctest.h"

#include "../VoLumWriteDebounce.h"
#include "../VoLumContentStore.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using volum::WriteDebounce;

namespace
{
// Naive "write every tick while dirty" — used only to prove the real helper's
// bounds fail against it (see evidence/17 red logs).
struct NaiveWriteEveryTick
{
  void dirty(double) { mDirty = true; }
  bool shouldWrite(double, bool force = false) const { return mDirty || force; }
  void markWritten(double) { mDirty = false; }
  bool isDirty() const { return mDirty; }
  bool mDirty = false;
};

template <typename Debounce>
int CountWritesDuringDrag(Debounce& d, double durationMs, double stepMs)
{
  int writes = 0;
  for (double t = 0.0; t <= durationMs; t += stepMs)
  {
    d.dirty(t);
    if (d.shouldWrite(t))
    {
      ++writes;
      d.markWritten(t);
    }
  }
  // Quiet tail after the drag stops (last dirty at durationMs).
  for (double t = durationMs + stepMs; t <= durationMs + WriteDebounce::kQuietMs + stepMs; t += stepMs)
  {
    if (d.shouldWrite(t))
    {
      ++writes;
      d.markWritten(t);
    }
  }
  return writes;
}

std::filesystem::path ContentTestBase(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-write-debounce-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}
} // namespace

TEST_CASE("WriteDebounce: single change writes once after 500 ms quiet")
{
  WriteDebounce d;
  d.dirty(0.0);
  CHECK_FALSE(d.shouldWrite(499.0));
  CHECK(d.shouldWrite(500.0));
  d.markWritten(500.0);
  CHECK_FALSE(d.shouldWrite(1000.0));
  CHECK_FALSE(d.isDirty());
}

TEST_CASE("WriteDebounce: continuous changes write at most every 2 s")
{
  WriteDebounce d;
  const int writes = CountWritesDuringDrag(d, 5000.0, 16.0);
  // 2 s and 4 s max-interval writes, plus one quiet write after the drag ends.
  CHECK(writes == 3);

  NaiveWriteEveryTick naive;
  const int naiveWrites = CountWritesDuringDrag(naive, 5000.0, 16.0);
  // Prove the acceptance bound rejects write-every-tick (~300+).
  CHECK(naiveWrites > 100);
  CHECK_FALSE(naiveWrites == 3);
}

TEST_CASE("WriteDebounce: flush writes immediately")
{
  WriteDebounce d;
  d.dirty(0.0);
  CHECK_FALSE(d.shouldWrite(0.0));
  CHECK(d.shouldWrite(0.0, /*force=*/true));
  d.markWritten(0.0);
  CHECK_FALSE(d.shouldWrite(0.0, /*force=*/true));
}

TEST_CASE("ContentStore Save returns early when registry matches baseline")
{
  using namespace volum::content;
  const auto base = ContentTestBase("save-early");
  ContentStore store(base);
  store.reg().irs.push_back({"ir_early", "Early IR", "ir/ir_early__t.wav"});
  REQUIRE(store.Save());

  const auto regPath = store.RegistryPath();
  REQUIRE(std::filesystem::exists(regPath));
  const auto before = std::filesystem::last_write_time(regPath);

  // Nothing changed: Save must not rewrite the file.
  REQUIRE(store.Save());
  const auto after = std::filesystem::last_write_time(regPath);
  CHECK(after == before);
  CHECK_FALSE(store.HasUnflushedChanges());
}
