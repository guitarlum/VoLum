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

TEST_CASE("VoLum 0.5.x uses 0.5.0 serialized config branch")
{
  REQUIRE(volum::ChunkUses0500SerializedConfig(volum::ChunkVersion("0.5.0")));
  REQUIRE(volum::ChunkUses0500SerializedConfig(volum::ChunkVersion("0.5.99")));
  REQUIRE_FALSE(volum::ChunkUses0500SerializedConfig(volum::ChunkVersion("0.6.0")));
}

TEST_CASE("VoLum 0.6.x uses 0.6.0 serialized config branch")
{
  REQUIRE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.6.0")));
  REQUIRE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.6.99")));
  REQUIRE_FALSE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.7.0")));
}

TEST_CASE("VoLum 0.7.0-0.7.8 uses 0.7.0 serialized config branch")
{
  REQUIRE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.7.0")));
  REQUIRE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.7.8")));
  REQUIRE_FALSE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.7.9")));
  REQUIRE_FALSE(volum::ChunkUses0700SerializedConfig(volum::ChunkVersion("0.6.0")));
}

TEST_CASE("VoLum 0.7.0 does not use 0.6.0 branch")
{
  REQUIRE_FALSE(volum::ChunkUses0600SerializedConfig(volum::ChunkVersion("0.7.0")));
}
