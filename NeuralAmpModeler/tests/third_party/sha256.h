// Minimal SHA-256 implementation for test fixtures.
// Public domain style implementation adapted for small, header-only test use.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace volum::test
{
class Sha256
{
public:
  Sha256() { Reset(); }

  void Reset()
  {
    mDataLen = 0;
    mBitLen = 0;
    mState = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
              0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  }

  void Update(const std::uint8_t* data, std::size_t len)
  {
    for (std::size_t i = 0; i < len; ++i)
    {
      mData[mDataLen++] = data[i];
      if (mDataLen == 64)
      {
        Transform();
        mBitLen += 512;
        mDataLen = 0;
      }
    }
  }

  std::array<std::uint8_t, 32> Final()
  {
    std::size_t i = mDataLen;

    if (mDataLen < 56)
    {
      mData[i++] = 0x80;
      while (i < 56)
        mData[i++] = 0x00;
    }
    else
    {
      mData[i++] = 0x80;
      while (i < 64)
        mData[i++] = 0x00;
      Transform();
      mData.fill(0);
    }

    mBitLen += static_cast<std::uint64_t>(mDataLen) * 8;
    mData[63] = static_cast<std::uint8_t>(mBitLen);
    mData[62] = static_cast<std::uint8_t>(mBitLen >> 8);
    mData[61] = static_cast<std::uint8_t>(mBitLen >> 16);
    mData[60] = static_cast<std::uint8_t>(mBitLen >> 24);
    mData[59] = static_cast<std::uint8_t>(mBitLen >> 32);
    mData[58] = static_cast<std::uint8_t>(mBitLen >> 40);
    mData[57] = static_cast<std::uint8_t>(mBitLen >> 48);
    mData[56] = static_cast<std::uint8_t>(mBitLen >> 56);
    Transform();

    std::array<std::uint8_t, 32> hash{};
    for (i = 0; i < 4; ++i)
    {
      for (std::size_t j = 0; j < 8; ++j)
        hash[i + j * 4] = static_cast<std::uint8_t>((mState[j] >> (24 - i * 8)) & 0xffU);
    }
    return hash;
  }

private:
  static std::uint32_t RotRight(std::uint32_t x, std::uint32_t n)
  {
    return (x >> n) | (x << (32U - n));
  }

  static std::uint32_t Choose(std::uint32_t e, std::uint32_t f, std::uint32_t g)
  {
    return (e & f) ^ (~e & g);
  }

  static std::uint32_t Majority(std::uint32_t a, std::uint32_t b, std::uint32_t c)
  {
    return (a & b) ^ (a & c) ^ (b & c);
  }

  static std::uint32_t Sig0(std::uint32_t x)
  {
    return RotRight(x, 7) ^ RotRight(x, 18) ^ (x >> 3);
  }

  static std::uint32_t Sig1(std::uint32_t x)
  {
    return RotRight(x, 17) ^ RotRight(x, 19) ^ (x >> 10);
  }

  void Transform()
  {
    static constexpr std::uint32_t k[64] = {
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
      0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
      0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
      0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
      0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
      0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
      0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
      0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
      0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
      0xc67178f2U};

    std::uint32_t m[64]{};
    for (std::size_t i = 0, j = 0; i < 16; ++i, j += 4)
    {
      m[i] = (static_cast<std::uint32_t>(mData[j]) << 24)
           | (static_cast<std::uint32_t>(mData[j + 1]) << 16)
           | (static_cast<std::uint32_t>(mData[j + 2]) << 8)
           | (static_cast<std::uint32_t>(mData[j + 3]));
    }
    for (std::size_t i = 16; i < 64; ++i)
      m[i] = Sig1(m[i - 2]) + m[i - 7] + Sig0(m[i - 15]) + m[i - 16];

    std::uint32_t a = mState[0], b = mState[1], c = mState[2], d = mState[3];
    std::uint32_t e = mState[4], f = mState[5], g = mState[6], h = mState[7];

    for (std::size_t i = 0; i < 64; ++i)
    {
      const std::uint32_t s1 = RotRight(e, 6) ^ RotRight(e, 11) ^ RotRight(e, 25);
      const std::uint32_t temp1 = h + s1 + Choose(e, f, g) + k[i] + m[i];
      const std::uint32_t s0 = RotRight(a, 2) ^ RotRight(a, 13) ^ RotRight(a, 22);
      const std::uint32_t temp2 = s0 + Majority(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    mState[0] += a;
    mState[1] += b;
    mState[2] += c;
    mState[3] += d;
    mState[4] += e;
    mState[5] += f;
    mState[6] += g;
    mState[7] += h;
  }

  std::array<std::uint8_t, 64> mData{};
  std::array<std::uint32_t, 8> mState{};
  std::size_t mDataLen = 0;
  std::uint64_t mBitLen = 0;
};

inline std::string Sha256Hex(const void* data, std::size_t len)
{
  Sha256 sha;
  sha.Update(static_cast<const std::uint8_t*>(data), len);
  const auto hash = sha.Final();
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (auto byte : hash)
    oss << std::setw(2) << static_cast<int>(byte);
  return oss.str();
}
} // namespace volum::test
