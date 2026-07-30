#include "third_party/doctest.h"
#include "../VoLumMetronomeDSP.h"

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

static std::vector<int> DetectClickStarts(volum::MetronomeDSP& met, double sampleRate, int totalSamples,
                                          const std::vector<int>& blockSizes)
{
  std::vector<int> starts;
  std::vector<double> buffer(4096, 0.0);
  double* outputs[1] = {buffer.data()};

  bool wasAbove = false;
  int absoluteSample = 0;
  int blockIndex = 0;
  constexpr double threshold = 0.04;
  constexpr int minGapSamples = 1000; // ignore sine zero-crossings inside one click burst

  while (absoluteSample < totalSamples)
  {
    int blockSize = blockSizes[blockIndex++ % blockSizes.size()];
    blockSize = std::min(blockSize, totalSamples - absoluteSample);
    std::fill(buffer.begin(), buffer.begin() + blockSize, 0.0);
    met.Process(outputs, blockSize, 1);

    for (int i = 0; i < blockSize; ++i)
    {
      bool above = std::fabs(buffer[i]) > threshold;
      if (above && !wasAbove && (starts.empty() || (absoluteSample + i - starts.back()) > minGapSamples))
        starts.push_back(absoluteSample + i);
      wasAbove = above;
    }

    absoluteSample += blockSize;
  }

  return starts;
}

static std::vector<double> RenderBeatPeaks(volum::MetronomeTimeSig sig, int numBeats)
{
  volum::MetronomeDSP met;
  met.Reset(48000.0);
  met.SetBPM(120.f);
  met.SetVolume(1.f);
  met.SetTimeSig(sig);
  met.SetActive(true);

  const int beatSamples = sig == volum::MetronomeTimeSig::Eighth_6_8 ? 8000 : 24000;
  const int totalSamples = beatSamples * numBeats;
  std::vector<double> buffer(totalSamples, 0.0);
  double* outputs[1] = {buffer.data()};

  met.Process(outputs, totalSamples, 1);

  std::vector<double> peaks;
  for (int beat = 0; beat < numBeats; ++beat)
  {
    double peak = 0.0;
    const int start = beat * beatSamples;
    for (int i = start; i < start + 1024; ++i)
      peak = std::max(peak, std::fabs(buffer[i]));
    peaks.push_back(peak);
  }
  return peaks;
}

TEST_CASE("MetronomeDSP: produces click when active")
{
  volum::MetronomeDSP met;
  met.Reset(48000.0);
  met.SetBPM(120.f);
  met.SetVolume(1.f);
  met.SetActive(true);

  const int frames = 1024;
  std::vector<double> left(frames, 0.0);
  std::vector<double> right(frames, 0.0);
  double* outputs[2] = {left.data(), right.data()};

  met.Process(outputs, frames, 2);

  double maxAbs = 0.0;
  for (int i = 0; i < frames; ++i)
    maxAbs = std::max(maxAbs, std::fabs(left[i]));

  CHECK(maxAbs > 0.01);
  CHECK(maxAbs < 0.85);
}

TEST_CASE("MetronomeDSP: silent when inactive")
{
  volum::MetronomeDSP met;
  met.Reset(48000.0);
  met.SetBPM(120.f);
  met.SetVolume(1.f);
  met.SetActive(false);

  const int frames = 1024;
  std::vector<double> left(frames, 0.0);
  std::vector<double> right(frames, 0.0);
  double* outputs[2] = {left.data(), right.data()};

  met.Process(outputs, frames, 2);

  double maxAbs = 0.0;
  for (int i = 0; i < frames; ++i)
    maxAbs = std::max(maxAbs, std::fabs(left[i]));

  CHECK(maxAbs == 0.0);
}

TEST_CASE("MetronomeDSP: BPM clamped to valid range")
{
  volum::MetronomeDSP met;
  met.SetBPM(10.f);
  CHECK(met.GetBPM() == volum::MetronomeDSP::kMinBPM);

  met.SetBPM(500.f);
  CHECK(met.GetBPM() == volum::MetronomeDSP::kMaxBPM);
}

TEST_CASE("MetronomeDSP: volume clamped 0..1")
{
  volum::MetronomeDSP met;
  met.SetVolume(-0.5f);
  CHECK(met.GetVolume() == 0.f);

  met.SetVolume(2.f);
  CHECK(met.GetVolume() == 1.f);
}

TEST_CASE("MetronomeDSP: beat phase advances")
{
  volum::MetronomeDSP met;
  met.Reset(48000.0);
  met.SetBPM(120.f);
  met.SetActive(true);

  const int frames = 4800; // 0.1s at 48kHz
  std::vector<double> buf(frames, 0.0);
  double* outputs[1] = {buf.data()};

  met.Process(outputs, frames, 1);

  float phase = met.GetBeatPhase();
  CHECK(phase > 0.f);
  CHECK(phase < 1.f);
}

TEST_CASE("MetronomeDSP: quarter-note click interval is stable across sample rates and uneven blocks")
{
  const std::vector<double> sampleRates = {41000.0, 44100.0, 48000.0};
  const std::vector<int> blockSizes = {64, 127, 256, 511, 96};

  for (double sampleRate : sampleRates)
  {
    volum::MetronomeDSP met;
    met.Reset(sampleRate);
    met.SetBPM(120.f);
    met.SetVolume(1.f);
    met.SetTimeSig(volum::MetronomeTimeSig::Quarter_4_4);
    met.SetActive(true);

    const int expectedInterval = static_cast<int>(sampleRate * 60.0 / 120.0);
    auto starts = DetectClickStarts(met, sampleRate, expectedInterval * 8, blockSizes);

    REQUIRE(starts.size() >= 7);
    for (size_t i = 1; i < starts.size(); ++i)
      CHECK(starts[i] - starts[i - 1] == doctest::Approx(expectedInterval).epsilon(0.001));
  }
}

TEST_CASE("MetronomeDSP: enabling requests one audio-thread reset only")
{
  volum::MetronomeDSP met;
  met.Reset(48000.0);
  met.SetBPM(120.f);
  met.SetVolume(1.f);
  met.SetActive(true);

  std::vector<double> buffer(1024, 0.0);
  double* outputs[1] = {buffer.data()};

  met.Process(outputs, 1024, 1);
  float phaseAfterFirstBlock = met.GetBeatPhase();
  met.Process(outputs, 1024, 1);
  float phaseAfterSecondBlock = met.GetBeatPhase();

  CHECK(phaseAfterSecondBlock > phaseAfterFirstBlock);
}

TEST_CASE("MetronomeDSP: 4/4 has strong beat 1 and medium beat 3 without clipping")
{
  auto peaks = RenderBeatPeaks(volum::MetronomeTimeSig::Quarter_4_4, 4);
  const double beat1 = peaks[0];
  const double beat2 = peaks[1];
  const double beat3 = peaks[2];
  const double beat4 = peaks[3];

  CHECK(beat1 < 0.85);
  CHECK(beat1 > beat3);
  CHECK(beat3 > beat2);
  CHECK(beat3 > beat4);
}

TEST_CASE("MetronomeDSP: mode accent patterns are deterministic")
{
  auto p1 = RenderBeatPeaks(volum::MetronomeTimeSig::Quarter_1_4, 4);
  CHECK(p1[0] == doctest::Approx(p1[1]).epsilon(0.01));
  CHECK(p1[1] == doctest::Approx(p1[2]).epsilon(0.01));
  CHECK(p1[2] == doctest::Approx(p1[3]).epsilon(0.01));

  auto p2 = RenderBeatPeaks(volum::MetronomeTimeSig::Quarter_2_4, 4);
  CHECK(p2[0] > p2[1]);
  CHECK(p2[2] > p2[3]);
  CHECK(p2[0] == doctest::Approx(p2[2]).epsilon(0.01));
  CHECK(p2[1] == doctest::Approx(p2[3]).epsilon(0.01));

  auto p3 = RenderBeatPeaks(volum::MetronomeTimeSig::Quarter_3_4, 6);
  CHECK(p3[0] > p3[1]);
  CHECK(p3[0] > p3[2]);
  CHECK(p3[3] > p3[4]);
  CHECK(p3[3] > p3[5]);

  auto p6 = RenderBeatPeaks(volum::MetronomeTimeSig::Eighth_6_8, 6);
  CHECK(p6[0] > p6[3]);
  CHECK(p6[3] > p6[1]);
  CHECK(p6[1] == doctest::Approx(p6[2]).epsilon(0.01));
  CHECK(p6[1] == doctest::Approx(p6[4]).epsilon(0.01));
  CHECK(p6[1] == doctest::Approx(p6[5]).epsilon(0.01));
}

TEST_CASE("MetronomeTimeSig: names are valid strings")
{
  CHECK(std::string(volum::MetronomeTimeSigName(volum::MetronomeTimeSig::Quarter_1_4)) == "1/4");
  CHECK(std::string(volum::MetronomeTimeSigName(volum::MetronomeTimeSig::Quarter_4_4)) == "4/4");
  CHECK(std::string(volum::MetronomeTimeSigName(volum::MetronomeTimeSig::Eighth_6_8)) == "6/8");
}

TEST_CASE("MetronomeTimeSig: beats per measure")
{
  CHECK(volum::MetronomeBeatsPerMeasure(volum::MetronomeTimeSig::Quarter_1_4) == 1);
  CHECK(volum::MetronomeBeatsPerMeasure(volum::MetronomeTimeSig::Quarter_4_4) == 4);
  CHECK(volum::MetronomeBeatsPerMeasure(volum::MetronomeTimeSig::Eighth_6_8) == 6);
}

TEST_CASE("A non-finite tempo is never stored")
{
  // The BPM field is typed into, and strtof happily returns NaN for "nan" and
  // infinity for "inf". std::clamp does not filter either out: both of its
  // comparisons are false for NaN, so it returns the NaN unchanged. That value
  // then divided into samplesPerBeat, whose conversion to int is undefined, and
  // never compared equal to itself so the recalculation ran on every block.
  volum::MetronomeDSP m;
  m.Reset(48000.0);

  m.SetBPM(90.f);
  REQUIRE(m.GetBPM() == doctest::Approx(90.f));

  for (const float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity()})
  {
    m.SetBPM(bad);
    const float stored = m.GetBPM();
    CAPTURE(bad);
    CHECK(std::isfinite(stored));
    CHECK(stored >= volum::MetronomeDSP::kMinBPM);
    CHECK(stored <= volum::MetronomeDSP::kMaxBPM);
    // Rejected, not replaced: substituting the factory default would discard the
    // tempo the user actually set and leave the overlay's readout lying about
    // what is clicking.
    CHECK(stored == doctest::Approx(90.f));
  }

  // Ordinary out-of-range values still clamp rather than being rejected.
  m.SetBPM(5.f);
  CHECK(m.GetBPM() == doctest::Approx(volum::MetronomeDSP::kMinBPM));
  m.SetBPM(10000.f);
  CHECK(m.GetBPM() == doctest::Approx(volum::MetronomeDSP::kMaxBPM));
}

TEST_CASE("A non-finite tempo cannot produce a pathological beat interval")
{
  // Guards the audio-thread half: even if a NaN ever reached the cached tempo, the
  // beat interval has to stay a usable number of samples.
  volum::MetronomeDSP m;
  m.Reset(48000.0);
  m.SetBPM(std::numeric_limits<float>::quiet_NaN());
  m.SetActive(true);

  std::vector<double> buf(512, 0.0);
  double* io[1] = {buf.data()};
  CHECK_NOTHROW(m.Process(io, 512, 1));

  for (const double s : buf)
    CHECK(std::isfinite(s));
}
