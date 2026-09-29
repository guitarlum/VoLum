#pragma once

// Debug/test self-capture for the Windows standalone. Inert unless the process
// starts with VOLUM_SELF_CAPTURE_DIR set: only then is the control attached.
//
// A driver writes <dir>\request.txt holding an output name. Within ~100 ms the
// control reads and deletes it, marks the whole editor dirty, and on the next
// paint reads the NanoVG main framebuffer after every other control has drawn.
// It writes <dir>\<name>.bmp (client pixels, backing scale included) and then
// <dir>\done.txt holding "<name>.bmp <width> <height>".
//
// This works on a locked workstation: the paint runs from the app's own timer
// into an offscreen FBO, with no desktop composition involved. Controls iPlug
// draws after the standard list (corner resizer, text entry, popup, bubbles)
// are not in the image. Include only from the plugin translation unit, after
// the iPlug/IGraphics headers.

#include "VoLumSelfCaptureFormat.h"

#if defined(OS_WIN) && defined(IGRAPHICS_NANOVG) && defined(IGRAPHICS_GL)
  #include <chrono>
  #include <filesystem>
  #include <fstream>
  #include <iterator>
  #include <system_error>

namespace volum::selfcapture
{

class SelfCaptureControl : public iplug::igraphics::IControl
{
public:
  SelfCaptureControl(const iplug::igraphics::IRECT& bounds, std::filesystem::path dir)
  : IControl(bounds)
  , mDir(std::move(dir))
  {
    mIgnoreMouse = true;
  }

  // IGraphics asks every control on each display tick, so this doubles as the
  // request poll. A pending capture keeps the full window dirty until it lands.
  bool IsDirty() override
  {
    if (mPending.empty())
    {
      const auto now = std::chrono::steady_clock::now();
      if (now >= mNextPoll)
      {
        mNextPoll = now + std::chrono::milliseconds(100);
        mPending = TakeRequest();
        if (!mPending.empty())
          if (auto* ui = GetUI())
            ui->SetAllControlsDirty();
      }
    }
    return !mPending.empty() || IControl::IsDirty();
  }

  void Draw(iplug::igraphics::IGraphics& g) override
  {
    if (mPending.empty())
      return;
    auto* vg = static_cast<NVGcontext*>(g.GetDrawContext());
    if (!vg)
      return;
    // NanoVG queues paths until nvgEndFrame. Flush them into the bound main FBO,
    // read it back, then reopen the frame for IGraphics' own EndFrame.
    nvgEndFrame(vg);
    GLint viewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int w = viewport[2], h = viewport[3];
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w > 0 ? w : 0) * static_cast<std::size_t>(h > 0 ? h : 0)
                                   * 4);
    if (!rgba.empty())
    {
      GLint packAlignment = 4;
      glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
      glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    }
    nvgBeginFrame(vg, static_cast<float>(g.WindowWidth()), static_cast<float>(g.WindowHeight()), g.GetScreenScale());

    const std::string file = mPending + ".bmp";
    mPending.clear();
    const auto bmp = EncodeBmpFromGlRgba(w, h, rgba.data());
    if (bmp.empty())
      return;
    std::error_code ec;
    const auto tmp = mDir / (file + ".part");
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
      if (!out)
        return;
    }
    std::filesystem::rename(tmp, mDir / file, ec);
    if (ec)
      return;
    std::ofstream done(mDir / "done.txt", std::ios::trunc);
    done << file << ' ' << w << ' ' << h << '\n';
  }

private:
  std::string TakeRequest() const
  {
    const auto req = mDir / "request.txt";
    std::error_code ec;
    if (!std::filesystem::exists(req, ec))
      return {};
    std::string raw;
    {
      std::ifstream in(req, std::ios::binary);
      raw.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    std::filesystem::remove(req, ec);
    std::filesystem::remove(mDir / "done.txt", ec);
    return SanitizeName(raw);
  }

  std::filesystem::path mDir;
  std::string mPending;
  std::chrono::steady_clock::time_point mNextPoll{};
};

// Last thing the layout attaches, so its Draw runs after every standard control.
inline void AttachIfRequested(iplug::igraphics::IGraphics* pGraphics)
{
  wchar_t dir[1024] = {};
  const DWORD len = GetEnvironmentVariableW(L"VOLUM_SELF_CAPTURE_DIR", dir, 1024);
  if (!pGraphics || len == 0 || len >= 1024)
    return;
  std::filesystem::path path(dir);
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  pGraphics->AttachControl(new SelfCaptureControl(pGraphics->GetBounds(), std::move(path)));
}

} // namespace volum::selfcapture

#else

namespace volum::selfcapture
{
inline void AttachIfRequested(iplug::igraphics::IGraphics*) {}
} // namespace volum::selfcapture

#endif
