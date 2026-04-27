#include "third_party/doctest.h"
#include "../VoLumTunerDSP.h"

#include <cmath>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static std::vector<float> GenerateSine(float freq, float sampleRate, int numSamples, float amplitude = 0.5f)
{
  std::vector<float> buf(numSamples);
  for (int i = 0; i < numSamples; ++i)
    buf[i] = amplitude * std::sin(2.f * static_cast<float>(M_PI) * freq * i / sampleRate);
  return buf;
}

TEST_CASE("TunerDSP: detects A4 = 440 Hz")
{
  volum::TunerDSP tuner;
  tuner.Reset(48000.0);
  tuner.SetActive(true);

  auto signal = GenerateSine(440.f, 48000.f, 4096);
  tuner.Process(signal.data(), 4096);

  auto result = tuner.GetResult();
  REQUIRE(result.valid);
  CHECK(result.frequency == doctest::Approx(440.f).epsilon(0.02));
  CHECK(result.noteIndex == 9); // A
  CHECK(result.octave == 4);
  CHECK(std::fabs(result.cents) < 5.f);
}

TEST_CASE("TunerDSP: detects E2 ~82.4 Hz (low E string)")
{
  volum::TunerDSP tuner;
  tuner.Reset(48000.0);
  tuner.SetActive(true);

  auto signal = GenerateSine(82.41f, 48000.f, 4096);
  tuner.Process(signal.data(), 4096);

  auto result = tuner.GetResult();
  REQUIRE(result.valid);
  CHECK(result.frequency == doctest::Approx(82.41f).epsilon(0.03));
  CHECK(result.noteIndex == 4); // E
  CHECK(result.octave == 2);
}

TEST_CASE("TunerDSP: smoothed A4 stays stable through small pitch jitter")
{
  volum::TunerDSP tuner;
  tuner.Reset(48000.0);
  tuner.SetActive(true);

  const float freqs[] = {440.f, 441.2f, 438.8f, 440.4f, 439.7f, 440.f};
  for (float freq : freqs)
  {
    auto signal = GenerateSine(freq, 48000.f, volum::TunerDSP::kBufferSize);
    tuner.Process(signal.data(), volum::TunerDSP::kBufferSize);
  }

  auto result = tuner.GetResult();
  REQUIRE(result.valid);
  CHECK(result.noteIndex == 9); // A
  CHECK(result.octave == 4);
  CHECK(std::fabs(result.cents) < 4.f);
}

TEST_CASE("TunerDSP: pure A4 can settle near zero cents")
{
  volum::TunerDSP tuner;
  tuner.Reset(48000.0);
  tuner.SetActive(true);

  for (int i = 0; i < 4; ++i)
  {
    auto signal = GenerateSine(440.f, 48000.f, volum::TunerDSP::kBufferSize);
    tuner.Process(signal.data(), volum::TunerDSP::kBufferSize);
  }

  auto result = tuner.GetResult();
  REQUIRE(result.valid);
  CHECK(std::fabs(result.cents) < 1.f);
}

TEST_CASE("TunerDSP: no output when inactive")
{
  volum::TunerDSP tuner;
  tuner.Reset(48000.0);
  tuner.SetActive(false);

  auto signal = GenerateSine(440.f, 48000.f, 4096);
  tuner.Process(signal.data(), 4096);

  auto result = tuner.GetResult();
  CHECK_FALSE(result.valid);
}

TEST_CASE("TunerDSP: no valid result on silence")
{
  volum::TunerDSP tuner;
  tuner.Reset(48000.0);
  tuner.SetActive(true);

  std::vector<float> silence(4096, 0.f);
  tuner.Process(silence.data(), 4096);

  auto result = tuner.GetResult();
  CHECK_FALSE(result.valid);
}

TEST_CASE("TunerDSP: NoteName covers all 12 notes")
{
  const char* expected[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  for (int i = 0; i < 12; ++i)
    CHECK(std::string(volum::TunerDSP::NoteName(i)) == expected[i]);

  CHECK(std::string(volum::TunerDSP::NoteName(-1)) == "?");
  CHECK(std::string(volum::TunerDSP::NoteName(12)) == "?");
}
