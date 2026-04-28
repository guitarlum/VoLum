#include "third_party/doctest.h"
#include <cstring>
#include <set>
#include <string>

#include "../VoLumAmpeteCatalog.h"

TEST_CASE("Catalog has 15 amps and Diezel Herbert Mk1 is present")
{
  REQUIRE(volum::kAmpCount == 15);

  bool found = false;
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    if (std::strcmp(volum::kAmps[i].folderName, "Diezel Herbert Mk1") == 0)
    {
      found = true;
      // Display name should not be empty and should match the folder name choice.
      REQUIRE(volum::kAmps[i].displayName != nullptr);
      REQUIRE(std::strlen(volum::kAmps[i].displayName) > 0);
      break;
    }
  }
  CHECK(found);
}

TEST_CASE("Amp folder names are unique and non-empty")
{
  std::set<std::string> seen;
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    REQUIRE(volum::kAmps[i].folderName != nullptr);
    REQUIRE(std::strlen(volum::kAmps[i].folderName) > 0);
    auto inserted = seen.insert(volum::kAmps[i].folderName);
    CHECK(inserted.second);
  }
}

TEST_CASE("Amp fractal case mapping is unique and within range")
{
  std::set<int> seen;
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    int caseIdx = volum::kAmpFractalCase[i];
    CHECK(caseIdx >= 0);
    CHECK(caseIdx < volum::kAmpCount);
    auto inserted = seen.insert(caseIdx);
    CHECK(inserted.second);
  }
}

TEST_CASE("Diezel Herbert Mk1 maps to the Lichtenberg fractal case (14)")
{
  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    if (std::strcmp(volum::kAmps[i].folderName, "Diezel Herbert Mk1") == 0)
    {
      CHECK(volum::kAmpFractalCase[i] == 14);
      return;
    }
  }
  FAIL("Diezel Herbert Mk1 not found in catalog");
}

TEST_CASE("Legacy Ampete state constants stay backward-compatible")
{
  // These constants exist only for unserializing pre-0.7.14 sessions.
  // They must not be resized or renamed without a migration path.
  REQUIRE(volum::kAmpeteRigCount == 16);
  REQUIRE(volum::kSpeakerPrefixes[0] == std::string("AMP"));
  REQUIRE(volum::kSpeakerPrefixes[1] == std::string("G12"));
  REQUIRE(volum::kSpeakerPrefixes[2] == std::string("G65"));
  REQUIRE(volum::kSpeakerPrefixes[3] == std::string("V30"));
}
