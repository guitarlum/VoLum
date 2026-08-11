// volum_pitch_ab.cpp - render the pitch engine to WAVs so it can be judged by ear.
//
// Why a tool rather than a doctest: pitch artifacts are a perceptual property. The
// automated ceilings in tests/test_volum_pitch_artifacts.cpp were themselves chosen
// by checking them against renders like these, and that calibration step is the part
// that has historically gone wrong - the POLY upshift crackle shipped twice under a
// green suite because the metric being asserted did not correspond to what a player
// hears. When you change the splice logic, listen first, then set thresholds.
//
// Two modes:
//   plain    - render the current working tree across a source/shift matrix.
//   baseline - additionally render a second engine compiled from another git revision
//              (see scripts/pitch-ab-render-win.ps1 -Baseline), writing matched
//              _A_baseline / _B_current pairs for direct comparison.
//
// Everything is rendered 100% wet and mono at 48 kHz: dry blend would mask exactly
// the artifacts you are trying to hear.
//
// Usage: driven by NeuralAmpModeler/scripts/pitch-ab-render-win.ps1. Run directly as
//   volum_pitch_ab.exe <outDir> [inputWav]

#include "../../VoLumPitchShifter.h" // dsp::effect - current working tree

#ifdef VOLUM_PITCH_AB_BASELINE
  #include "baseline_pitch.h" // dsp::effect_baseline - generated from a git revision
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr double kSR = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr size_t kBlock = 256;

std::string gOutDir = ".";

// ---------- minimal 16-bit mono WAV I/O ----------

std::vector<double> ReadWav(const std::string& path)
{
  std::vector<double> out;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f)
    return out;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  const bool ok = n > 0 && std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  if (!ok)
    return out;

  size_t p = 12; // walk chunks past the RIFF header to find "data"
  while (p + 8 <= b.size())
  {
    uint32_t sz = 0;
    std::memcpy(&sz, b.data() + p + 4, 4);
    if (std::memcmp(b.data() + p, "data", 4) == 0)
    {
      const size_t count = std::min(static_cast<size_t>(sz) / 2, (b.size() - p - 8) / 2);
      out.resize(count);
      for (size_t i = 0; i < count; ++i)
      {
        int16_t s = 0;
        std::memcpy(&s, b.data() + p + 8 + i * 2, 2);
        out[i] = static_cast<double>(s) / 32768.0;
      }
      return out;
    }
    p += 8 + sz + (sz & 1);
  }
  return out;
}

void WriteWav(const std::string& name, const std::vector<double>& x)
{
  const std::string path = gOutDir + "/" + name + ".wav";
  std::vector<int16_t> s(x.size());
  for (size_t i = 0; i < x.size(); ++i)
    s[i] = static_cast<int16_t>(std::lround(std::clamp(x[i], -1.0, 1.0) * 32767.0));

  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f)
  {
    std::printf("  !! cannot write %s\n", path.c_str());
    return;
  }
  const uint32_t dataBytes = static_cast<uint32_t>(s.size() * 2);
  const uint32_t riff = 36 + dataBytes, fmtSz = 16, sr = 48000, byteRate = sr * 2;
  const uint16_t pcm = 1, ch = 1, blockAlign = 2, bits = 16;
  std::fwrite("RIFF", 1, 4, f);
  std::fwrite(&riff, 4, 1, f);
  std::fwrite("WAVEfmt ", 1, 8, f);
  std::fwrite(&fmtSz, 4, 1, f);
  std::fwrite(&pcm, 2, 1, f);
  std::fwrite(&ch, 2, 1, f);
  std::fwrite(&sr, 4, 1, f);
  std::fwrite(&byteRate, 4, 1, f);
  std::fwrite(&blockAlign, 2, 1, f);
  std::fwrite(&bits, 2, 1, f);
  std::fwrite("data", 1, 4, f);
  std::fwrite(&dataBytes, 4, 1, f);
  std::fwrite(s.data(), 2, s.size(), f);
  std::fclose(f);
}

// ---------- synthetic sources ----------

void AddPluck(std::vector<double>& v, double f0, double amp, double tau, size_t start)
{
  const double B = 0.0004; // steel-string inharmonicity
  for (int h = 1; h <= 16; ++h)
  {
    const double fh = f0 * h * std::sqrt(1.0 + B * h * h);
    if (fh > 0.45 * kSR)
      break;
    const double a = amp / h;
    const double hTau = tau / (1.0 + 0.6 * (h - 1));
    const double phase = 0.37 * h + 0.11 * f0;
    for (size_t i = start; i < v.size(); ++i)
    {
      const double t = static_cast<double>(i - start) / kSR;
      v[i] += a * std::exp(-t / hTau) * std::sin(2.0 * kPi * fh * t + phase);
    }
  }
}

void Normalize(std::vector<double>& v, double peak)
{
  double pk = 1e-9;
  for (double s : v)
    pk = std::max(pk, std::fabs(s));
  for (double& s : v)
    s *= peak / pk;
}

// ---------- rendering ----------

// Templated on the engine so the current tree and a baseline revision - which live in
// different namespaces - go through identical code. Anything else invites an
// accidental difference in the harness being mistaken for a difference in the engine.
template <typename PitchT>
std::vector<double> Render(const std::vector<double>& in, typename PitchT::Mode mode, double semitones,
                           typename PitchT::Character character)
{
  PitchT pitch;
  pitch.Configure(kSR, static_cast<int>(kBlock));
  const double octDown = (mode == PitchT::Mode::Octaver) ? 1.0 : 0.0;
  const double octUp = 0.0;
  // mix = 1.0: fully wet. Any dry blend hides the artifact under the clean signal.
  pitch.SetParams(mode, semitones, 1.0, octDown, octUp, 1.0, PitchT::Voicing::Modern, 0.0, character);
  pitch.Reset();

  std::vector<double> out;
  out.reserve(in.size());
  std::vector<DSP_SAMPLE> buf(kBlock);
  for (size_t off = 0; off < in.size(); off += kBlock)
  {
    const size_t n = std::min(kBlock, in.size() - off);
    std::copy(in.begin() + static_cast<long>(off), in.begin() + static_cast<long>(off + n), buf.begin());
    DSP_SAMPLE* ptr = buf.data();
    DSP_SAMPLE** o = pitch.Process(&ptr, 1, n);
    out.insert(out.end(), o[0], o[0] + n);
  }
  return out;
}

struct Job
{
  std::string label;
  bool octaver;
  double semitones;
};

} // namespace

int main(int argc, char** argv)
{
  if (argc > 1)
    gOutDir = argv[1];

  std::vector<std::pair<std::string, std::vector<double>>> sources;

  // A real DI is the most trustworthy material: synthetic plucks cannot reproduce
  // pick noise, fret buzz or a fretting-hand transient.
  if (argc > 2)
  {
    std::vector<double> di = ReadWav(argv[2]);
    if (di.empty())
      std::printf("!! could not read %s - continuing with synthetic sources only\n", argv[2]);
    else
    {
      Normalize(di, 0.5);
      sources.emplace_back("realDI", std::move(di));
      std::printf("loaded real DI: %s\n", argv[2]);
    }
  }

  // Successive plucks: exposes splice behaviour through attack AND long decay, which
  // is where the upshift crackle lives - it needs a note ringing out to be audible.
  {
    std::vector<double> v(static_cast<size_t>(3.0 * kSR), 0.0);
    AddPluck(v, 82.41, 1.0, 2.0, static_cast<size_t>(0.05 * kSR)); // E2
    AddPluck(v, 110.00, 1.0, 2.0, static_cast<size_t>(1.05 * kSR)); // A2
    AddPluck(v, 146.83, 1.0, 2.0, static_cast<size_t>(2.05 * kSR)); // D3
    Normalize(v, 0.5);
    sources.emplace_back("plucks", std::move(v));
  }
  // E major triad with a true third, strummed. Note this is NOT E2/B2/E3: those are
  // in 2:3:4 ratio and share a common period, so they behave like one complex tone.
  {
    std::vector<double> v(static_cast<size_t>(3.0 * kSR), 0.0);
    AddPluck(v, 82.41, 1.0, 3.0, static_cast<size_t>(0.05 * kSR)); // E2
    AddPluck(v, 207.65, 0.9, 3.0, static_cast<size_t>(0.07 * kSR)); // G#3
    AddPluck(v, 246.94, 0.8, 3.0, static_cast<size_t>(0.09 * kSR)); // B3
    Normalize(v, 0.5);
    sources.emplace_back("chordE", std::move(v));
  }

  // Transpose is limited to -12..+7 by kPrePitchSemitones in NeuralAmpModeler.cpp;
  // rendering outside that range would be judging sounds no user can dial in.
  const std::vector<Job> jobs = {{"POLY-12", false, -12.0}, {"POLY-07", false, -7.0}, {"POLY-05", false, -5.0},
                                 {"POLY+02", false, 2.0},   {"POLY+05", false, 5.0},  {"POLY+07", false, 7.0},
                                 {"OCTdn", true, 0.0}};

  using Pitch = dsp::effect::VoLumPitch;
#ifdef VOLUM_PITCH_AB_BASELINE
  using Base = dsp::effect_baseline::VoLumPitch;
  std::printf("baseline engine compiled in: writing _A_baseline / _B_current pairs\n");
#else
  std::printf("no baseline: writing single renders of the current working tree\n");
#endif

  for (const auto& src : sources)
  {
    WriteWav(src.first + "_00dry", src.second);
    for (const Job& job : jobs)
    {
      const auto mode = job.octaver ? Pitch::Mode::Octaver : Pitch::Mode::Transpose;
      const auto character = job.octaver ? Pitch::Character::Drop : Pitch::Character::Poly;
      const std::vector<double> cur = Render<Pitch>(src.second, mode, job.semitones, character);
#ifdef VOLUM_PITCH_AB_BASELINE
      const auto bMode = job.octaver ? Base::Mode::Octaver : Base::Mode::Transpose;
      const auto bChar = job.octaver ? Base::Character::Drop : Base::Character::Poly;
      WriteWav(src.first + "_" + job.label + "_A_baseline", Render<Base>(src.second, bMode, job.semitones, bChar));
      WriteWav(src.first + "_" + job.label + "_B_current", cur);
#else
      WriteWav(src.first + "_" + job.label, cur);
#endif
      std::printf("  %-8s %s\n", src.first.c_str(), job.label.c_str());
    }
  }

  std::printf("\nWrote WAVs to %s\n", gOutDir.c_str());
  return 0;
}
