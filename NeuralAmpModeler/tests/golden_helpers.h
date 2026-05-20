#pragma once

#include "third_party/sha256.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace volum::test
{
inline std::vector<double> MakeReferenceInput(std::size_t frames, double sampleRate, std::uint32_t seed)
{
  std::vector<double> input(frames, 0.0);
  std::uint32_t state = seed ? seed : 0x12345678U;
  auto nextNoise = [&]() {
    state = state * 1664525U + 1013904223U;
    const double unit = static_cast<double>((state >> 8) & 0x00ffffffU) / static_cast<double>(0x00ffffffU);
    return (unit * 2.0) - 1.0;
  };

  for (std::size_t i = 0; i < frames; ++i)
  {
    const double t = static_cast<double>(i) / sampleRate;
    const double sweepHz = 70.0 + 2400.0 * static_cast<double>(i) / static_cast<double>(frames);
    const double sine = 0.16 * std::sin(2.0 * 3.14159265358979323846 * sweepHz * t);
    const double low = 0.08 * std::sin(2.0 * 3.14159265358979323846 * 110.0 * t);
    const double burst = (i > frames / 4 && i < frames / 2) ? 0.06 * nextNoise() : 0.0;
    input[i] = sine + low + burst;
  }
  if (!input.empty())
    input[0] += 0.5; // Impulse component catches delay/reverb tap movement.
  return input;
}

inline std::vector<float> ToFloat(const std::vector<double>& values)
{
  std::vector<float> out(values.size());
  for (std::size_t i = 0; i < values.size(); ++i)
    out[i] = static_cast<float>(values[i]);
  return out;
}

inline std::string Sha256Hex(const std::vector<double>& samples)
{
  return Sha256Hex(samples.data(), samples.size() * sizeof(double));
}

inline std::string Sha256Hex(const std::vector<float>& samples)
{
  return Sha256Hex(samples.data(), samples.size() * sizeof(float));
}

template<typename Sample>
inline std::string Sha256HexSamples(const std::vector<Sample>& samples)
{
  return Sha256Hex(samples.data(), samples.size() * sizeof(Sample));
}

inline void AppendChannel(std::vector<double>& dst, double* src, std::size_t frames)
{
  dst.insert(dst.end(), src, src + frames);
}

inline void AppendChannel(std::vector<float>& dst, float* src, std::size_t frames)
{
  dst.insert(dst.end(), src, src + frames);
}

template<typename Sample>
inline void AppendSamples(std::vector<Sample>& dst, Sample* src, std::size_t frames)
{
  dst.insert(dst.end(), src, src + frames);
}
} // namespace volum::test
