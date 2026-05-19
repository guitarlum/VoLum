// Bypass-identity locking tests.
//
// The 1.0 contract is: when every optional VoLum DSP stage is in its bypassed
// configuration, the audio chain delivers the input unchanged (within float
// round-trip tolerance). This protects users from "is the plugin doing
// something even when I bypass everything?" surprises and also catches future
// regressions where a stage stops respecting its own bypass flag.
//
// We exercise each DSP unit's stand-alone bypass behavior here because the full
// signal chain lives in NeuralAmpModeler::ProcessBlock (iPlug2 plugin scope)
// and can't be cheaply instantiated in doctest. The chain-level guarantees
// reduce to the unit-level guarantees plus the runX flags in
// VoLumProcessingPlan, which has its own tests.

#include "third_party/doctest.h"
#include "../../AudioDSPTools/dsp/Delay.h"
#include "../../AudioDSPTools/dsp/Reverb.h"
#include "../VoLumLevelMute.h"
#include "../VoLumMasterSafety.h"
#include "../VoLumNanGuard.h"

#include <cmath>
#include <vector>

namespace
{
bool BuffersEqual(const std::vector<double>& a, const std::vector<double>& b, double tol)
{
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::abs(a[i] - b[i]) > tol)
      return false;
  return true;
}

std::vector<double> MakeSine(size_t frames, double amp = 0.3)
{
  std::vector<double> v(frames);
  for (size_t i = 0; i < frames; ++i)
    v[i] = amp * std::sin(static_cast<double>(i) * 0.05);
  return v;
}
} // namespace

TEST_CASE("BypassIdentity: Delay at mix=0 is identity")
{
  for (int mode : {dsp::effect::Delay::kModeDigital, dsp::effect::Delay::kModeAnalog,
                   dsp::effect::Delay::kModeReverse})
  {
    INFO("delay mode=" << mode);
    dsp::effect::Delay delay;
    delay.SetParams(120.0, 0.0, 0.0, mode, 48000.0);
    auto input = MakeSine(256);
    std::vector<double> work = input;
    double* in[1] = {work.data()};
    auto** out = delay.Process(in, 1, work.size());
    std::vector<double> outV(out[0], out[0] + work.size());
    CHECK(BuffersEqual(outV, input, 1e-4));
  }
}

TEST_CASE("BypassIdentity: Reverb at mix=0 is identity (Hall and Plate)")
{
  for (int mode : {dsp::effect::Reverb::kModeHall, dsp::effect::Reverb::kModePlate})
  {
    INFO("reverb mode=" << mode);
    dsp::effect::Reverb reverb;
    reverb.SetParams(0.0, 3.0, 5.0, 0.0, 0.5, mode, 48000.0);
    auto input = MakeSine(256);
    std::vector<double> workL = input;
    std::vector<double> workR = input;
    double* in[2] = {workL.data(), workR.data()};
    auto** out = reverb.Process(in, 2, workL.size());
    std::vector<double> outL(out[0], out[0] + workL.size());
    std::vector<double> outR(out[1], out[1] + workR.size());
    CHECK(BuffersEqual(outL, input, 1e-4));
    CHECK(BuffersEqual(outR, input, 1e-4));
  }
}

TEST_CASE("BypassIdentity: Delay -> Reverb both at mix=0 is full-chain identity")
{
  dsp::effect::Delay delay;
  dsp::effect::Reverb reverb;
  delay.SetParams(150.0, 0.0, 0.0, dsp::effect::Delay::kModeDigital, 48000.0);
  reverb.SetParams(0.0, 3.0, 5.0, 0.0, 0.5, dsp::effect::Reverb::kModeHall, 48000.0);

  auto input = MakeSine(256);
  std::vector<double> workL = input;
  std::vector<double> workR = input;
  double* in[2] = {workL.data(), workR.data()};

  auto** afterDelay = delay.Process(in, 2, workL.size());
  auto** afterReverb = reverb.Process(afterDelay, 2, workL.size());
  std::vector<double> outL(afterReverb[0], afterReverb[0] + workL.size());
  std::vector<double> outR(afterReverb[1], afterReverb[1] + workR.size());
  CHECK(BuffersEqual(outL, input, 1e-4));
  CHECK(BuffersEqual(outR, input, 1e-4));
}

TEST_CASE("BypassIdentity: SoftSafetyClip is identity below the knee")
{
  // Master safety must be inert for any musical-range signal so it doesn't
  // colour normal use. The full knee-and-ceiling continuity is locked elsewhere
  // (test_volum_master_safety.cpp); this case is the bypass-chain assertion.
  for (double x = -1.3; x <= 1.3 + 1e-9; x += 0.01)
    CHECK(volum::SoftSafetyClip(x) == doctest::Approx(x));
}

TEST_CASE("BypassIdentity: NanGuard is identity for finite samples")
{
  std::vector<double> buf = MakeSine(128);
  const auto before = buf;
  CHECK_FALSE(volum::ScrubNonFiniteInPlace(buf.data(), buf.size()));
  CHECK(BuffersEqual(buf, before, 0.0));
}

TEST_CASE("BypassIdentity: output-style level minimum is silence")
{
  CHECK(volum::DbToAmpWithMuteFloor(-20.0, -20.0) == doctest::Approx(0.0));
  CHECK(volum::DbToAmpWithMuteFloor(-40.0, -40.0) == doctest::Approx(0.0));
  CHECK(volum::DbToAmpWithMuteFloor(0.0, -40.0) == doctest::Approx(1.0));
}
