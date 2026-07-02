#include "third_party/doctest.h"

#include "../VoLumIrFileGuard.h"

// VoLum custom-IR file-size guard: small/normal captures are accepted; absurdly
// large WAVs (e.g. a whole song picked by mistake) are rejected before decoding.

TEST_CASE("IR byte guard accepts normal-sized cabinet captures")
{
  CHECK(volum::IrFileBytesAcceptable(0));
  CHECK(volum::IrFileBytesAcceptable(144 * 1024)); // ~1s mono 24-bit 48k
  CHECK(volum::IrFileBytesAcceptable(6ull * 1024 * 1024)); // ~10s stereo 24-bit 96k
}

TEST_CASE("IR byte guard accepts exactly the cap and rejects beyond it")
{
  CHECK(volum::IrFileBytesAcceptable(volum::kMaxIrFileBytes));
  CHECK_FALSE(volum::IrFileBytesAcceptable(volum::kMaxIrFileBytes + 1));
  CHECK_FALSE(volum::IrFileBytesAcceptable(500ull * 1024 * 1024)); // a 500 MB "IR"
}

TEST_CASE("IR too-large message names the size and the cap")
{
  const std::string msg = volum::IrTooLargeMessage(volum::kMaxIrFileBytes * 2);
  CHECK(msg.find("too large") != std::string::npos);
  CHECK(msg.find("128") != std::string::npos); // 2x the 64 MB cap
  CHECK(msg.find("64") != std::string::npos); // the cap itself
}
