#include "engine.h"

#include "RecursiveLinearFilter.h"
#include "activations.h"
#include "get_dsp.h"
#include "json.hpp"
#include "slimmable.h"
#include "wav.h"

#include "VoLumMasterSafety.h"
#include "VoLumNanGuard.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace volum::android::engine
{
namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr double kDcBlockerHz = 5.0;

void WriteU16(std::ofstream& out, std::uint16_t value)
{
  const unsigned char bytes[2] = {
    static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8)};
  out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void WriteU32(std::ofstream& out, std::uint32_t value)
{
  const unsigned char bytes[4] = {
    static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8),
    static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24)};
  out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void WriteMonoFloatWav(const std::string& path, const std::vector<float>& samples)
{
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    throw std::runtime_error("cannot open output WAV: " + path);
  const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * sizeof(float));
  out.write("RIFF", 4);
  WriteU32(out, 36U + dataBytes);
  out.write("WAVEfmt ", 8);
  WriteU32(out, 16);
  WriteU16(out, 3); // IEEE float
  WriteU16(out, 1);
  WriteU32(out, static_cast<std::uint32_t>(kSampleRate));
  WriteU32(out, static_cast<std::uint32_t>(kSampleRate) * sizeof(float));
  WriteU16(out, sizeof(float));
  WriteU16(out, 32);
  out.write("data", 4);
  WriteU32(out, dataBytes);
  out.write(reinterpret_cast<const char*>(samples.data()), dataBytes);
  if (!out)
    throw std::runtime_error("failed writing output WAV: " + path);
}

std::string ReadArchitecture(const std::string& rig)
{
  std::ifstream input(rig);
  if (!input)
    throw std::runtime_error("cannot open NAM file: " + rig);
  nlohmann::json json;
  input >> json;
  return json.at("architecture").get<std::string>();
}
} // namespace

RenderResult RenderFixedChain(const std::string& inputWav, const std::string& rig, const std::string& outputWav)
{
  std::vector<float> input;
  double sampleRate = 0.0;
  const auto wavResult = dsp::wav::Load(inputWav.c_str(), input, sampleRate);
  if (wavResult != dsp::wav::LoadReturnCode::SUCCESS)
    throw std::runtime_error("input WAV load failed: " + dsp::wav::GetMsgForLoadReturnCode(wavResult));
  if (sampleRate != kSampleRate)
    throw std::runtime_error("fixed parity input must be 48 kHz");

  nam::activations::Activation::enable_fast_tanh();
  auto model = nam::get_dsp(std::filesystem::path(rig));
  if (!model)
    throw std::runtime_error("NAM loader returned null");
  auto* slimmable = dynamic_cast<nam::SlimmableModel*>(model.get());
  if (slimmable)
    slimmable->SetSlimmableSize(1.0);
  model->Reset(kSampleRate, kBlockSize);

  recursive_linear_filter::HighPass dcBlocker;
  dcBlocker.SetParams(recursive_linear_filter::HighPassParams(kSampleRate, kDcBlockerHz));
  dcBlocker.ReserveOutputs(1, kBlockSize);

  std::vector<float> output(input.size(), 0.0f);
  std::vector<NAM_SAMPLE> inBlock(kBlockSize, 0.0f);
  std::vector<NAM_SAMPLE> modelBlock(kBlockSize, 0.0f);
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t offset = 0; offset < input.size(); offset += kBlockSize)
  {
    const int frames = static_cast<int>(std::min<std::size_t>(kBlockSize, input.size() - offset));
    std::copy_n(input.data() + offset, frames, inBlock.data());
    NAM_SAMPLE* in = inBlock.data();
    NAM_SAMPLE* modelOut = modelBlock.data();
    model->process(&in, &modelOut, frames);
    ScrubNonFiniteInPlace(modelOut, static_cast<std::size_t>(frames));
    double* filterIn[1] = {modelOut};
    double* filtered = dcBlocker.Process(filterIn, 1, static_cast<std::size_t>(frames))[0];
    for (int frame = 0; frame < frames; ++frame)
      output[offset + static_cast<std::size_t>(frame)] = static_cast<float>(SoftSafetyClip(filtered[frame]));
  }
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  WriteMonoFloatWav(outputWav, output);
  return {output.size(), sampleRate, elapsed, slimmable != nullptr, ReadArchitecture(rig)};
}

std::string ResultJson(const RenderResult& result)
{
  std::ostringstream out;
  out << std::fixed << std::setprecision(9)
      << "{\"frames\":" << result.frames << ",\"sample_rate\":" << result.sampleRate
      << ",\"render_seconds\":" << result.renderSeconds
      << ",\"audio_seconds\":" << (result.frames / result.sampleRate)
      << ",\"rtf\":" << (result.renderSeconds * result.sampleRate / static_cast<double>(result.frames))
      << ",\"slimmable\":" << (result.slimmable ? "true" : "false")
      << ",\"architecture\":\"" << result.architecture << "\"}";
  return out.str();
}

} // namespace volum::android::engine
