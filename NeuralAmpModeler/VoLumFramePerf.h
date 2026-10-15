#pragma once

// Debug frame timer for the Windows standalone. Inert unless the process starts
// with VOLUM_FRAME_PERF set: only then are the two probes attached.
//
// iPlug repaints each dirty region by drawing every control that intersects it,
// in attach order. The begin probe sits right after the background and the end
// probe is attached last, both covering the window, so each region is bracketed
// by them. The end probe flushes NanoVG and waits for the GPU (glFinish), so the
// time includes rasterisation, not just path building.
//
// One "[frame]" line per painted frame whose draw took at least VOLUM_FRAME_PERF
// milliseconds (so VOLUM_FRAME_PERF=0 logs every frame): draw time, the number
// of regions repainted and each region's time. Each region pays one GPU sync, so
// compare region times, not totals of frames with different region counts.
// Include only from the plugin translation
// unit, after the iPlug/IGraphics headers and VoLumDiagLog.h.

#if defined(OS_WIN) && defined(IGRAPHICS_NANOVG) && defined(IGRAPHICS_GL)
  #include <chrono>
  #include <cstdio>
  #include <cstdlib>
  #include <memory>
  #include <string>

namespace volum::frameperf
{

struct Clock
{
  double thresholdMs = 0.0;
  std::chrono::steady_clock::time_point regionStart{};
  double frameMs = 0.0;
  int regions = 0;
  std::string perRegion;
};

class BeginProbe : public iplug::igraphics::IControl
{
public:
  BeginProbe(const iplug::igraphics::IRECT& bounds, std::shared_ptr<Clock> clock)
  : IControl(bounds)
  , mClock(std::move(clock))
  {
    mIgnoreMouse = true;
  }

  // Asked once per display tick, before that tick paints: the previous frame is done.
  bool IsDirty() override
  {
    if (mClock->regions > 0)
    {
      if (mClock->frameMs >= mClock->thresholdMs)
      {
        char line[96];
        std::snprintf(line, sizeof(line), "draw %.2f ms  regions %d  [", mClock->frameMs, mClock->regions);
        VOLUM_LOG("frame", line + mClock->perRegion + "]");
      }
      mClock->frameMs = 0.0;
      mClock->regions = 0;
      mClock->perRegion.clear();
    }
    return IControl::IsDirty();
  }

  void Draw(iplug::igraphics::IGraphics&) override { mClock->regionStart = std::chrono::steady_clock::now(); }

private:
  std::shared_ptr<Clock> mClock;
};

class EndProbe : public iplug::igraphics::IControl
{
public:
  EndProbe(const iplug::igraphics::IRECT& bounds, std::shared_ptr<Clock> clock)
  : IControl(bounds)
  , mClock(std::move(clock))
  {
    mIgnoreMouse = true;
  }

  void Draw(iplug::igraphics::IGraphics& g) override
  {
    auto* vg = static_cast<NVGcontext*>(g.GetDrawContext());
    if (!vg)
      return;
    nvgEndFrame(vg);
    glFinish();
    nvgBeginFrame(vg, static_cast<float>(g.WindowWidth()), static_cast<float>(g.WindowHeight()), g.GetScreenScale());
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - mClock->regionStart).count();
    mClock->frameMs += ms;
    ++mClock->regions;
    char one[16];
    std::snprintf(one, sizeof(one), mClock->perRegion.empty() ? "%.2f" : " %.2f", ms);
    mClock->perRegion += one;
  }

private:
  std::shared_ptr<Clock> mClock;
};

inline std::shared_ptr<Clock> ClockFromEnv()
{
  const char* v = std::getenv("VOLUM_FRAME_PERF");
  if (!v || !v[0])
    return nullptr;
  auto clock = std::make_shared<Clock>();
  clock->thresholdMs = std::atof(v);
  return clock;
}

// Right after the background control.
inline std::shared_ptr<Clock> AttachBeginIfRequested(iplug::igraphics::IGraphics* pGraphics)
{
  auto clock = pGraphics ? ClockFromEnv() : nullptr;
  if (clock)
    pGraphics->AttachControl(new BeginProbe(pGraphics->GetBounds(), clock));
  return clock;
}

// After every other standard control.
inline void AttachEnd(iplug::igraphics::IGraphics* pGraphics, const std::shared_ptr<Clock>& clock)
{
  if (pGraphics && clock)
    pGraphics->AttachControl(new EndProbe(pGraphics->GetBounds(), clock));
}

} // namespace volum::frameperf

#else

  #include <memory>

namespace volum::frameperf
{
struct Clock
{
};
inline std::shared_ptr<Clock> AttachBeginIfRequested(iplug::igraphics::IGraphics*)
{
  return nullptr;
}
inline void AttachEnd(iplug::igraphics::IGraphics*, const std::shared_ptr<Clock>&) {}
} // namespace volum::frameperf

#endif
