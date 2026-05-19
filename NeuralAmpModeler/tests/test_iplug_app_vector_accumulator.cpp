#include "third_party/doctest.h"

#include "VoLumIPlugAPPVectorAccumulator.h"

#include <array>
#include <vector>

TEST_CASE("IPlugAPPVectorAccumulator carries non-divisible driver buffers")
{
  constexpr int kVectorSize = 64;
  constexpr int kChannels = 2;
  constexpr int kCallbacksPerSize = 17;
  const std::array<int, 11> bufferSizes = {32, 64, 96, 128, 192, 256, 512, 1024, 2048, 4096, 8192};

  for (const int bufferSize : bufferSizes)
  {
    CAPTURE(bufferSize);

    iplug::VoLumIPlugAPPVectorAccumulator accumulator;
    accumulator.Reset(kChannels, kChannels, kVectorSize);

    std::vector<double> outL;
    std::vector<double> outR;
    int processCalls = 0;
    int64_t sampleIndex = 0;

    auto processBlock = [&]() {
      ++processCalls;
      for (int i = 0; i < kVectorSize; ++i)
      {
        accumulator.GetOutputChannel(0)[i] = accumulator.GetInputChannel(0)[i] + 0.25;
        accumulator.GetOutputChannel(1)[i] = accumulator.GetInputChannel(1)[i] - 0.5;
      }
      accumulator.CommitProcessedOutput();
    };

    for (int callback = 0; callback < kCallbacksPerSize; ++callback)
    {
      for (int frame = 0; frame < bufferSize; ++frame)
      {
        if (accumulator.HasOutput())
        {
          const int readIdx = accumulator.GetOutputReadIndex();
          outL.push_back(accumulator.GetOutputChannel(0)[readIdx]);
          outR.push_back(accumulator.GetOutputChannel(1)[readIdx]);
          accumulator.PopOutputFrame();
        }

        const int writeIdx = accumulator.GetInputWriteIndex();
        accumulator.GetInputChannel(0)[writeIdx] = static_cast<double>(sampleIndex);
        accumulator.GetInputChannel(1)[writeIdx] = static_cast<double>(sampleIndex + 100000);
        ++sampleIndex;

        if (accumulator.PushInputFrame())
          processBlock();
      }
    }

    while (accumulator.HasOutput())
    {
      const int readIdx = accumulator.GetOutputReadIndex();
      outL.push_back(accumulator.GetOutputChannel(0)[readIdx]);
      outR.push_back(accumulator.GetOutputChannel(1)[readIdx]);
      accumulator.PopOutputFrame();
    }

    const int totalInputFrames = bufferSize * kCallbacksPerSize;
    const int expectedProcessedFrames = (totalInputFrames / kVectorSize) * kVectorSize;
    CHECK(processCalls == expectedProcessedFrames / kVectorSize);
    REQUIRE(outL.size() == static_cast<size_t>(expectedProcessedFrames));
    REQUIRE(outR.size() == static_cast<size_t>(expectedProcessedFrames));

    for (int i = 0; i < expectedProcessedFrames; ++i)
    {
      CHECK(outL[static_cast<size_t>(i)] == doctest::Approx(static_cast<double>(i) + 0.25));
      CHECK(outR[static_cast<size_t>(i)] == doctest::Approx(static_cast<double>(i + 100000) - 0.5));
    }
  }
}
