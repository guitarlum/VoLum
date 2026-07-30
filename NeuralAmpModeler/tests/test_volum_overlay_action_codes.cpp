#include "third_party/doctest.h"

#include "../VoLumOverlayActionCodes.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

// Regression coverage for the custom-content overlay's per-row hotspot codes.
//
// Every row-indexed hotspot encodes as "family base + row index" and is decoded by
// range. The families were 100 apart, which holds only while every list stays under
// 100 rows. At 100 rows the overwrite icon encoded as 500 + 100 = 600 - the rename
// base - and HandleManageAction tests rename first, so clicking overwrite on the
// 101st preset renamed the first one; the trash icon there opened the first IR's
// shaping editor instead of deleting. Row bodies were separately capped at 256, so
// rows beyond that were dead to clicks.
//
// The property that has to hold is cross-family exclusivity over every row index a
// library can reach, so that is what is asserted here rather than specific numbers.

using volum::custom::ActionBase;
using volum::custom::ActionCode;
using volum::custom::ActionFamily;
using volum::custom::ActionIndex;
using volum::custom::InActionFamily;
using volum::custom::kActionFamilyBase;
using volum::custom::kActionStride;

namespace
{
constexpr int kFamilyCount = static_cast<int>(ActionFamily::kCount);

std::string ReadOverlaySource()
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumCustomOverlay.h";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
} // namespace

TEST_CASE("No row index in one hotspot family can encode as another family's code")
{
  // Sampled across each family's whole range, including the boundaries where the
  // old 100-spacing broke.
  const int indices[] = {0, 1, 2, 99, 100, 101, 255, 256, 257, 999, 4095, kActionStride - 1};

  for (int f = 0; f < kFamilyCount; ++f)
  {
    const auto family = static_cast<ActionFamily>(f);

    for (const int index : indices)
    {
      const int code = ActionCode(family, index);

      CHECK(InActionFamily(code, family));
      CHECK(ActionIndex(code, family) == index);

      for (int other = 0; other < kFamilyCount; ++other)
      {
        if (other == f)
          continue;
        CHECK_FALSE(InActionFamily(code, static_cast<ActionFamily>(other)));
      }
    }
  }
}

TEST_CASE("The 101st row is decoded as itself, not as another family's first row")
{
  // The exact shape of the bug: at 100-apart spacing this was the rename base.
  const int overwriteRow100 = ActionCode(ActionFamily::Overwrite, 100);

  CHECK(overwriteRow100 != ActionBase(ActionFamily::Rename));
  CHECK(InActionFamily(overwriteRow100, ActionFamily::Overwrite));
  CHECK(ActionIndex(overwriteRow100, ActionFamily::Overwrite) == 100);

  const int deleteRow100 = ActionCode(ActionFamily::Delete, 100);
  CHECK(deleteRow100 != ActionBase(ActionFamily::IrCfg));
  CHECK(InActionFamily(deleteRow100, ActionFamily::Delete));
}

TEST_CASE("Family bases are distinct, ordered, and clear of the overlay's fixed codes")
{
  std::set<int> bases;
  for (int f = 0; f < kFamilyCount; ++f)
  {
    const int base = ActionBase(static_cast<ActionFamily>(f));
    CHECK(bases.insert(base).second);
    // The overlay's fixed codes (close, add, save, cab-name chips at 70, art
    // swatches at 80) all live below the family block.
    CHECK(base >= kActionFamilyBase);
  }

  // Room for a library far larger than anyone builds by hand - the previous limit
  // was 100 - while the whole layout stays inside int.
  CHECK(kActionStride >= 4096);
  const long long highest =
    static_cast<long long>(ActionBase(static_cast<ActionFamily>(kFamilyCount - 1))) + kActionStride;
  CHECK(highest < 2147483647LL);
}

TEST_CASE("The overlay decodes hotspots with the shared stride, not a hardcoded width")
{
  const std::string src = ReadOverlaySource();

  // Nine decode sites: overwrite, rename, delete, IR config, two row-body sites,
  // and the three builder file columns. A hardcoded width is what made the
  // families overlap, so none may come back.
  std::size_t strided = 0;
  for (std::size_t pos = src.find("+ volum::custom::kActionStride)"); pos != std::string::npos;
       pos = src.find("+ volum::custom::kActionStride)", pos + 1))
    ++strided;
  CHECK(strided == 9);

  CHECK(src.find("kRowBase + 256") == std::string::npos);
  CHECK(src.find("kRowDeleteBase + 100") == std::string::npos);
  CHECK(src.find("kRowOverwriteBase + 100") == std::string::npos);
  CHECK(src.find("kRowRenameBase + 100") == std::string::npos);
  CHECK(src.find("kRowIrCfgBase + 100") == std::string::npos);
  CHECK(src.find("kFileSpeakerBase + 100") == std::string::npos);
  CHECK(src.find("kFileChannelBase + 100") == std::string::npos);
  CHECK(src.find("kFileRemoveBase + 100") == std::string::npos);
}
