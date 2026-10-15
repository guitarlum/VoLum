#include "third_party/doctest.h"

#include "../VoLumSelfCaptureFormat.h"

#include <cstdint>
#include <string>
#include <vector>

using volum::selfcapture::EncodeBmpFromGlRgba;
using volum::selfcapture::SanitizeName;

namespace
{
std::uint32_t Read32(const std::vector<std::uint8_t>& b, std::size_t at)
{
  return static_cast<std::uint32_t>(b[at]) | (static_cast<std::uint32_t>(b[at + 1]) << 8)
         | (static_cast<std::uint32_t>(b[at + 2]) << 16) | (static_cast<std::uint32_t>(b[at + 3]) << 24);
}
} // namespace

TEST_CASE("A self-capture request name cannot leave the capture folder")
{
  CHECK(SanitizeName("../evil") == "_._evil");
  CHECK(SanitizeName("C:\\x\\y") == "C__x_y");
  CHECK(SanitizeName("  play-dual_01.v2  ") == "play-dual_01.v2");
  CHECK(SanitizeName("shot.") == "shot");
  CHECK(SanitizeName("") == "capture");
  CHECK(SanitizeName("   ") == "capture");
  CHECK(SanitizeName(std::string(200, 'a')).size() == volum::selfcapture::kMaxNameLength);
}

TEST_CASE("A GL readback becomes a bottom-up 24-bit BMP with padded rows")
{
  // GL row 0 is the bottom row, and so is BMP's first stored row.
  const std::uint8_t rgba[2 * 2 * 4] = {
    10, 20, 30, 255, 40,  50,  60,  255, // GL row 0
    70, 80, 90, 255, 100, 110, 120, 255, // GL row 1
  };
  const auto bmp = EncodeBmpFromGlRgba(2, 2, rgba);
  REQUIRE(bmp.size() == 70);
  CHECK(bmp[0] == 'B');
  CHECK(bmp[1] == 'M');
  CHECK(Read32(bmp, 2) == 70);
  CHECK(Read32(bmp, 10) == 54);
  CHECK(Read32(bmp, 18) == 2);
  CHECK(Read32(bmp, 22) == 2);
  CHECK((bmp[28] | (bmp[29] << 8)) == 24);
  // Row stride is 2 * 3 = 6 bytes padded to 8; pixels are BGR.
  CHECK(bmp[54] == 30);
  CHECK(bmp[55] == 20);
  CHECK(bmp[56] == 10);
  CHECK(bmp[57] == 60);
  CHECK(bmp[60] == 0);
  CHECK(bmp[61] == 0);
  CHECK(bmp[62] == 90);
  CHECK(bmp[64] == 70);

  CHECK(EncodeBmpFromGlRgba(0, 2, rgba).empty());
  CHECK(EncodeBmpFromGlRgba(2, 2, nullptr).empty());
}
