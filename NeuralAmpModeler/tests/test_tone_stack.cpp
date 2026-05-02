#include "third_party/doctest.h"
#include "../ToneStack.h"

#include <cmath>
#include <vector>

TEST_CASE("Basic NAM tone stack processes finite bounded output")
{
  dsp::tone_stack::BasicNamToneStack toneStack;
  constexpr int frames = 128;
  std::vector<DSP_SAMPLE> left(frames, static_cast<DSP_SAMPLE>(0.05));
  std::vector<DSP_SAMPLE> right(frames, static_cast<DSP_SAMPLE>(-0.05));
  DSP_SAMPLE* inputs[2] = {left.data(), right.data()};

  toneStack.Reset(48000.0, frames);
  toneStack.SetParam("bass", 7.0);
  toneStack.SetParam("middle", 4.0);
  toneStack.SetParam("treble", 6.0);

  DSP_SAMPLE** outputs = toneStack.Process(inputs, 2, frames);
  REQUIRE(outputs != nullptr);

  for (int c = 0; c < 2; ++c)
  {
    for (int i = 0; i < frames; ++i)
    {
      CAPTURE(c);
      CAPTURE(i);
      CHECK(std::isfinite(static_cast<double>(outputs[c][i])));
      CHECK(std::abs(static_cast<double>(outputs[c][i])) < 10.0);
    }
  }
}
