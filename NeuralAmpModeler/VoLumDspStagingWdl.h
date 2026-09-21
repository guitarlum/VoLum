#pragma once

#include "VoLumDspStaging.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <vector>

// WDL_String is provided by iPlug headers before this include in NeuralAmpModeler.h.
// Tests define VOLUM_DSP_STAGING_SKIP_WDL so they can exercise the RT helpers
// without pulling iPlug.

namespace volum::dsp_staging
{

// Same cap as VoLumPitch::kRealtimeBlockReserve. Hosts grow the callback without
// another OnReset; ProcessBlock must not allocate or throw past this.
constexpr int kRealtimeBlockReserve = 8192;
constexpr size_t kDspGraveyardCapacity = 16;
constexpr size_t kRtPathCapacity = 32768;

inline int ReservedAudioBlockSize(int hostBlockSize)
{
  return std::max({hostBlockSize, 64, kRealtimeBlockReserve});
}

inline bool AudioBlockFitsReserve(int nFrames, int reserved)
{
  return reserved > 0 && nFrames > 0 && nFrames <= reserved;
}

template <typename T>
bool ResizeScratchNoAlloc(std::vector<T>& buf, size_t n)
{
  if (n > buf.capacity())
    return false;
  buf.resize(n);
  return true;
}

template <typename Sample>
void CopyOrSilenceExternalBlock(Sample** inputs, Sample** outputs, int nFrames, int nIn, int nOut)
{
  if (nFrames <= 0 || !outputs)
    return;
  const size_t bytes = static_cast<size_t>(nFrames) * sizeof(Sample);
  for (int c = 0; c < nOut; ++c)
  {
    if (!outputs[c])
      continue;
    if (c < nIn && inputs && inputs[c])
      std::memcpy(outputs[c], inputs[c], bytes);
    else
      std::memset(outputs[c], 0, bytes);
  }
}

// Returns true when the NAM should run. Oversized blocks copy dry and never throw.
template <typename Sample>
bool ProcessOrBypassNamBlock(int numFrames, int maxBlock, Sample** input, Sample** output, int nChans)
{
  if (numFrames >= 0 && numFrames <= maxBlock)
    return true;
  if (numFrames > 0 && input && output)
  {
    for (int c = 0; c < nChans; ++c)
    {
      if (input[c] && output[c] && input[c] != output[c])
        std::memcpy(output[c], input[c], static_cast<size_t>(numFrames) * sizeof(Sample));
    }
  }
  return false;
}

template <typename T>
void RetireToGraveyard(std::unique_ptr<T>& dying, std::vector<std::unique_ptr<T>>& graveyard)
{
  if (!dying)
    return;
  if (graveyard.size() == graveyard.capacity())
  {
    dying.reset();
    return;
  }
  graveyard.push_back(std::move(dying));
}

template <typename T>
void StageIncomingModel(std::unique_ptr<T>& staged, std::unique_ptr<T>& incoming,
                        std::vector<std::unique_ptr<T>>& graveyard)
{
  RetireToGraveyard(staged, graveyard);
  staged = std::move(incoming);
}

template <typename T>
void PublishStagedModel(std::unique_ptr<T>& live, std::unique_ptr<T>& staged,
                        std::vector<std::unique_ptr<T>>& graveyard)
{
  if (!staged)
    return;
  RetireToGraveyard(live, graveyard);
  live = std::move(staged);
}

enum class LoaderResultAction
{
  Ignore,
  Retire,
  RetireAndReload,
  Stage
};

inline LoaderResultAction DecideLoaderResult(bool hasModel, bool superseded, bool alreadyNeedsLoad, bool rateMismatch,
                                             bool hasError)
{
  if (superseded || alreadyNeedsLoad)
    return LoaderResultAction::Retire;
  if (hasError || !hasModel)
    return LoaderResultAction::Ignore;
  if (rateMismatch)
    return LoaderResultAction::RetireAndReload;
  return LoaderResultAction::Stage;
}

struct RtPublishedPath
{
  char text[kRtPathCapacity]{};
  std::atomic<bool> dirty{false};
};

inline void CopyPathNoAlloc(char* dest, size_t destCap, const char* src)
{
  if (!dest || destCap == 0)
    return;
  if (!src)
    src = "";
  const size_t n = std::min(destCap - 1, std::strlen(src));
  std::memcpy(dest, src, n);
  dest[n] = '\0';
}

inline void PublishPathNoAlloc(RtPublishedPath& slot, const char* src)
{
  CopyPathNoAlloc(slot.text, kRtPathCapacity, src);
  slot.dirty.store(true, std::memory_order_release);
}

#ifndef VOLUM_DSP_STAGING_SKIP_WDL

struct WdlStagedPathPair
{
  WDL_String live;
  WDL_String staged;
};

inline void StagePathOnSuccess(WdlStagedPathPair& paths, const WDL_String& candidatePath)
{
  paths.staged = candidatePath;
}

inline void StagePathOnSuccess(WdlStagedPathPair& paths, const char* candidatePath)
{
  paths.staged.Set(candidatePath);
}

inline void CommitStagedPathOnApply(WdlStagedPathPair& paths)
{
  if (paths.staged.GetLength())
    paths.live = paths.staged;
  paths.staged.Set("");
}

inline void ClearStagedPath(WdlStagedPathPair& paths)
{
  paths.staged.Set("");
}

inline void ClearLiveAndStagedPath(WdlStagedPathPair& paths)
{
  paths.live.Set("");
  paths.staged.Set("");
}

#endif // VOLUM_DSP_STAGING_SKIP_WDL

} // namespace volum::dsp_staging
