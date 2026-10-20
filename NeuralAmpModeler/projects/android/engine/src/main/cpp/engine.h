#pragma once

#include <cstddef>
#include <string>

namespace volum::android::engine
{

struct RenderResult
{
  std::size_t frames = 0;
  double sampleRate = 0.0;
  double renderSeconds = 0.0;
  bool slimmable = false;
  std::string architecture;
};

RenderResult RenderFixedChain(const std::string& inputWav, const std::string& rig, const std::string& outputWav);
std::string ResultJson(const RenderResult& result);

} // namespace volum::android::engine
