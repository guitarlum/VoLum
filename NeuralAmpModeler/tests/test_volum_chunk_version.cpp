#include "third_party/doctest.h"
#include "../VoLumChunkVersion.h"

TEST_CASE("VoLum 0.1.x uses 0.7.15 serialized config branch")
{
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.1.0")));
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.1.99")));
}

TEST_CASE("NAM 0.7.15+ uses 0.7.15 branch")
{
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.7.15")));
  REQUIRE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.8.0")));
}

TEST_CASE("NAM 0.7.14 does not use 0.7.15 branch")
{
  REQUIRE_FALSE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.7.14")));
}

TEST_CASE("Pre-VoLum 0.1.0 does not use 0.7.15 branch")
{
  REQUIRE_FALSE(volum::ChunkUses0715SerializedConfig(volum::ChunkVersion("0.0.9")));
}
