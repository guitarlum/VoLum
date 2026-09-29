#pragma once

// Pure helpers for the debug/test self-capture (VoLumSelfCapture.h): the request
// name a driver drops next to the app, and the BMP written from a GL readback.
// Standard library only, so the format can be tested without a GL context.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace volum::selfcapture
{

inline constexpr std::size_t kMaxNameLength = 64;

// Any process can write the request file, so its content is reduced to a flat
// file stem: no separators, no drive letters, no leading dots.
inline std::string SanitizeName(std::string_view raw)
{
  std::size_t b = 0, e = raw.size();
  while (b < e && static_cast<unsigned char>(raw[b]) <= ' ')
    ++b;
  while (e > b && static_cast<unsigned char>(raw[e - 1]) <= ' ')
    --e;
  std::string out;
  for (std::size_t i = b; i < e && out.size() < kMaxNameLength; ++i)
  {
    const char c = raw[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'
                    || (c == '.' && !out.empty());
    out.push_back(ok ? c : '_');
  }
  while (!out.empty() && out.back() == '.')
    out.pop_back();
  return out.empty() ? std::string("capture") : out;
}

// 24-bit BI_RGB BMP from glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE) output. GL rows
// run bottom-up and so do BMP rows with a positive height, so rows are copied in
// order; only R and B swap and alpha is dropped. Empty on bad dimensions.
inline std::vector<std::uint8_t> EncodeBmpFromGlRgba(int width, int height, const std::uint8_t* rgba)
{
  if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || rgba == nullptr)
    return {};
  const std::size_t w = static_cast<std::size_t>(width), h = static_cast<std::size_t>(height);
  const std::size_t stride = (w * 3 + 3) & ~static_cast<std::size_t>(3);
  const std::size_t pixelBytes = stride * h;
  const std::size_t fileBytes = 54 + pixelBytes;
  std::vector<std::uint8_t> out(fileBytes, 0);
  auto put16 = [&](std::size_t at, std::uint32_t v) {
    out[at] = static_cast<std::uint8_t>(v & 0xFF);
    out[at + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
  };
  auto put32 = [&](std::size_t at, std::uint32_t v) {
    put16(at, v & 0xFFFF);
    put16(at + 2, v >> 16);
  };
  out[0] = 'B';
  out[1] = 'M';
  put32(2, static_cast<std::uint32_t>(fileBytes));
  put32(10, 54);
  put32(14, 40);
  put32(18, static_cast<std::uint32_t>(width));
  put32(22, static_cast<std::uint32_t>(height));
  put16(26, 1);
  put16(28, 24);
  put32(34, static_cast<std::uint32_t>(pixelBytes));
  put32(38, 2835); // 72 dpi
  put32(42, 2835);
  for (std::size_t y = 0; y < h; ++y)
  {
    const std::uint8_t* src = rgba + y * w * 4;
    std::uint8_t* dst = out.data() + 54 + y * stride;
    for (std::size_t x = 0; x < w; ++x)
    {
      dst[x * 3 + 0] = src[x * 4 + 2];
      dst[x * 3 + 1] = src[x * 4 + 1];
      dst[x * 3 + 2] = src[x * 4 + 0];
    }
  }
  return out;
}

} // namespace volum::selfcapture
