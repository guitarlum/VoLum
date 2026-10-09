// volum_glitchscan.cpp - feed a steady tone through the loopback cable and record what
// VoLum plays back, so dropouts and clicks can be found in real audio.
//
// LOCAL ONLY. Needs an interface whose output 1 is patched into input 1, and VoLum
// running on DirectSound (shared mode) with its output routed to output 2 only.
//
//   tone  -> render endpoint, channel 1 (L) only -> cable -> input 1 -> VoLum
//   VoLum -> same render endpoint, channel 2 (R) only
//   this  <- WASAPI loopback of that endpoint: L = the tone as rendered (control),
//                                              R = VoLum's output
//
// A gap on R that L does not share is VoLum's (or its DirectSound path's). A gap on
// both is this tool or the Windows mixer. ASIO cannot be scanned this way: the driver
// takes the device exclusively and nothing else can play or listen.
//
// The tone's period is a whole number of samples at 48 kHz (200 Hz = 240 samples) so
// the analyser can subtract each cycle from the previous one; an amp model is
// non-linear but time-invariant, so its output stays periodic too.

#define NOMINMAX
#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "ole32.lib")

namespace
{

struct Options
{
  std::wstring render = L"UA-2X2";
  double seconds = 60.0;
  double freq = 200.0;
  double amp = 0.25;
  int toneChannel = 0;
  std::string wav = "glitchscan.wav";
  std::string stopFile;
};

long long UnixMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

bool WriteWav(const std::string& path, const std::vector<float>& data, unsigned channels, unsigned rate)
{
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || !f)
    return false;
  const unsigned dataBytes = static_cast<unsigned>(data.size() * sizeof(float));
  const unsigned short fmtTag = 3; // IEEE float
  const unsigned short bits = 32;
  const unsigned short blockAlign = static_cast<unsigned short>(channels * 4);
  const unsigned byteRate = rate * blockAlign;
  const unsigned riffSize = 36 + dataBytes;
  const unsigned fmtSize = 16;
  const unsigned short ch = static_cast<unsigned short>(channels);
  fwrite("RIFF", 1, 4, f);
  fwrite(&riffSize, 4, 1, f);
  fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmtSize, 4, 1, f);
  fwrite(&fmtTag, 2, 1, f);
  fwrite(&ch, 2, 1, f);
  fwrite(&rate, 4, 1, f);
  fwrite(&byteRate, 4, 1, f);
  fwrite(&blockAlign, 2, 1, f);
  fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f);
  fwrite(&dataBytes, 4, 1, f);
  fwrite(data.data(), sizeof(float), data.size(), f);
  fclose(f);
  return true;
}

IMMDevice* FindRender(IMMDeviceEnumerator* en, const std::wstring& match, std::wstring& nameOut)
{
  IMMDeviceCollection* col = nullptr;
  if (FAILED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col)))
    return nullptr;
  UINT n = 0;
  col->GetCount(&n);
  IMMDevice* found = nullptr;
  for (UINT i = 0; i < n && !found; ++i)
  {
    IMMDevice* dev = nullptr;
    if (FAILED(col->Item(i, &dev)))
      continue;
    IPropertyStore* props = nullptr;
    if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)))
    {
      PROPVARIANT v;
      PropVariantInit(&v);
      if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
      {
        std::wstring name = v.pwszVal;
        if (name.find(match) != std::wstring::npos)
        {
          found = dev;
          nameOut = name;
        }
      }
      PropVariantClear(&v);
      props->Release();
    }
    if (!found)
      dev->Release();
  }
  col->Release();
  return found;
}

bool IsFloat(const WAVEFORMATEX* wf)
{
  if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    return true;
  if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
  {
    auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wf);
    static const GUID kFloat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
    return IsEqualGUID(ext->SubFormat, kFloat) && wf->wBitsPerSample == 32;
  }
  return false;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
  Options opt;
  for (int i = 1; i < argc; ++i)
  {
    std::wstring a = argv[i];
    auto next = [&]() { return (i + 1 < argc) ? std::wstring(argv[++i]) : std::wstring(); };
    auto narrow = [](const std::wstring& w) { return std::string(w.begin(), w.end()); };
    if (a == L"--render")
      opt.render = next();
    else if (a == L"--seconds")
      opt.seconds = std::stod(next());
    else if (a == L"--freq")
      opt.freq = std::stod(next());
    else if (a == L"--amp")
      opt.amp = std::stod(next());
    else if (a == L"--tone-channel")
      opt.toneChannel = std::stoi(next());
    else if (a == L"--wav")
      opt.wav = narrow(next());
    else if (a == L"--stop-file")
      opt.stopFile = narrow(next());
    else
    {
      std::printf(
        "usage: volum_glitchscan [--render <substr>] [--seconds s] [--freq hz] [--amp 0..1]\n"
        "                        [--tone-channel 0|1] [--wav path] [--stop-file path]\n");
      return 2;
    }
  }

  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  IMMDeviceEnumerator* en = nullptr;
  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en))))
  {
    std::printf("RESULT no-enumerator\n");
    return 3;
  }
  std::wstring devName;
  IMMDevice* dev = FindRender(en, opt.render, devName);
  if (!dev)
  {
    std::printf("RESULT no-device\n");
    return 4;
  }

  IAudioClient* renderClient = nullptr;
  IAudioClient* captureClient = nullptr;
  dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&renderClient));
  dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&captureClient));
  WAVEFORMATEX* mix = nullptr;
  renderClient->GetMixFormat(&mix);
  if (!IsFloat(mix) || mix->nChannels < 2)
  {
    std::printf(
      "RESULT unsupported-mix-format tag=%u ch=%u bits=%u\n", mix->wFormatTag, mix->nChannels, mix->wBitsPerSample);
    return 5;
  }
  const unsigned rate = mix->nSamplesPerSec;
  const unsigned ch = mix->nChannels;

  const REFERENCE_TIME bufferHns = 2000000; // 200 ms: this tool must never be the one that glitches
  HRESULT hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferHns, 0, mix, nullptr);
  if (FAILED(hr))
  {
    std::printf("RESULT render-init-failed hr=0x%08lx\n", hr);
    return 6;
  }
  hr = captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, bufferHns, 0, mix, nullptr);
  if (FAILED(hr))
  {
    std::printf("RESULT capture-init-failed hr=0x%08lx\n", hr);
    return 7;
  }
  IAudioRenderClient* render = nullptr;
  IAudioCaptureClient* capture = nullptr;
  renderClient->GetService(IID_PPV_ARGS(&render));
  captureClient->GetService(IID_PPV_ARGS(&capture));
  UINT32 renderFrames = 0;
  renderClient->GetBufferSize(&renderFrames);

  const double twoPi = 6.283185307179586;
  const double radPerFrame = twoPi * opt.freq / rate;
  unsigned long long phaseFrame = 0;
  auto fill = [&](UINT32 frames) {
    BYTE* buf = nullptr;
    if (frames == 0 || FAILED(render->GetBuffer(frames, &buf)))
      return;
    float* out = reinterpret_cast<float*>(buf);
    for (UINT32 f = 0; f < frames; ++f)
    {
      const float v = static_cast<float>(opt.amp * std::sin(radPerFrame * static_cast<double>(phaseFrame++)));
      for (unsigned c = 0; c < ch; ++c)
        out[f * ch + c] = (static_cast<int>(c) == opt.toneChannel) ? v : 0.0f;
    }
    render->ReleaseBuffer(frames, 0);
  };
  fill(renderFrames);

  std::vector<float> captured;
  captured.reserve(static_cast<size_t>(rate * 2 * (opt.seconds + 5)));
  std::vector<unsigned long long> discontinuities;
  int silentPackets = 0;
  long long firstFrameUnixMs = 0;

  renderClient->Start();
  captureClient->Start();
  const long long startUnixMs = UnixMs();
  std::printf("START unix_ms=%lld device='%ls' rate=%u ch=%u freq=%.1f amp=%.3f\n", startUnixMs, devName.c_str(), rate,
              ch, opt.freq, opt.amp);
  std::fflush(stdout);

  const auto t0 = std::chrono::steady_clock::now();
  int stopCheck = 0;
  for (;;)
  {
    Sleep(5);
    UINT32 padding = 0;
    if (SUCCEEDED(renderClient->GetCurrentPadding(&padding)))
      fill(renderFrames - padding);

    UINT32 packet = 0;
    while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0)
    {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
        break;
      if (firstFrameUnixMs == 0)
        firstFrameUnixMs = UnixMs();
      if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
        discontinuities.push_back(captured.size() / 2);
      const float* in = reinterpret_cast<const float*>(data);
      for (UINT32 f = 0; f < frames; ++f)
      {
        const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
        captured.push_back(silent ? 0.0f : in[f * ch + 0]);
        captured.push_back(silent ? 0.0f : in[f * ch + 1]);
      }
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
        ++silentPackets;
      capture->ReleaseBuffer(frames);
    }

    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (elapsed >= opt.seconds)
      break;
    if (!opt.stopFile.empty() && (++stopCheck % 20) == 0
        && GetFileAttributesA(opt.stopFile.c_str()) != INVALID_FILE_ATTRIBUTES)
      break;
  }

  renderClient->Stop();
  captureClient->Stop();

  WriteWav(opt.wav, captured, 2, rate);
  std::string meta = opt.wav + ".json";
  FILE* m = nullptr;
  if (fopen_s(&m, meta.c_str(), "w") == 0 && m)
  {
    std::fprintf(m, "{\n  \"startUnixMs\": %lld,\n  \"firstFrameUnixMs\": %lld,\n  \"rate\": %u,\n", startUnixMs,
                 firstFrameUnixMs, rate);
    std::fprintf(m, "  \"freq\": %.3f,\n  \"amp\": %.4f,\n  \"toneChannel\": %d,\n  \"silentPackets\": %d,\n", opt.freq,
                 opt.amp, opt.toneChannel, silentPackets);
    std::fprintf(m, "  \"discontinuityFrames\": [");
    for (size_t i = 0; i < discontinuities.size(); ++i)
      std::fprintf(m, "%s%llu", i ? ", " : "", discontinuities[i]);
    std::fprintf(m, "]\n}\n");
    fclose(m);
  }
  std::printf("RESULT ok frames=%zu discontinuities=%zu silent_packets=%d wav=%s\n", captured.size() / 2,
              discontinuities.size(), silentPackets, opt.wav.c_str());
  return 0;
}
