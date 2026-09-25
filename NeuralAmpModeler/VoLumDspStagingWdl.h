#pragma once

#include "VoLumDspStaging.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// WDL_String is provided by iPlug headers before this include in NeuralAmpModeler.h.
// Tests define VOLUM_DSP_STAGING_SKIP_WDL so they can exercise the RT helpers
// without pulling iPlug.

namespace volum::dsp_staging
{

// Same cap as VoLumPitch::kRealtimeBlockReserve. Hosts grow the callback without
// another OnReset; ProcessBlock must not allocate or throw past this. Plain
// scratch vectors only: they cost nothing until touched.
constexpr int kRealtimeBlockReserve = 8192;
constexpr size_t kDspGraveyardCapacity = 16;
constexpr size_t kRtPathCapacity = 32768;

inline int ReservedAudioBlockSize(int hostBlockSize)
{
  return std::max({hostBlockSize, 64, kRealtimeBlockReserve});
}

// A NAM is Reset at the host block, never at kRealtimeBlockReserve. Its conv
// ring buffers are 2 * lookback + maxBlock long and the write head walks all of
// it, so an 8192 reserve made every block stream megabytes through the cache:
// a PRE NAM + amp at 64 frames went from ~14% to ~80-90% of the deadline (p99
// over 100%) and crackled in 1.3.0. A larger host block is chunked by
// ProcessNamInChunks instead.
inline int NamResetBlockSize(int hostBlockSize)
{
  return std::max(hostBlockSize, 64);
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

// Mono NAM block. A block past the NAM's Reset size runs as consecutive chunks
// of at most maxBlock, which is sample-identical to the host having delivered
// them that way - no allocation, no throw, no dry gap. maxBlock <= 0 (never
// Reset) copies dry.
template <typename Sample, typename ProcessFn>
void ProcessNamInChunks(int numFrames, int maxBlock, Sample* input, Sample* output, ProcessFn&& process)
{
  if (numFrames <= 0 || !input || !output)
    return;
  if (maxBlock <= 0)
  {
    if (input != output)
      std::memcpy(output, input, static_cast<size_t>(numFrames) * sizeof(Sample));
    return;
  }
  for (int offset = 0; offset < numFrames; offset += maxBlock)
  {
    Sample* in = input + offset;
    Sample* out = output + offset;
    process(&in, &out, std::min(maxBlock, numFrames - offset));
  }
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

// Removal of a lane's asset (NAM or IR): both the live object and one still
// waiting to be applied go to the graveyard.
template <typename T>
void RetireLiveAndStaged(std::unique_ptr<T>& live, std::unique_ptr<T>& staged,
                         std::vector<std::unique_ptr<T>>& graveyard)
{
  RetireToGraveyard(live, graveyard);
  RetireToGraveyard(staged, graveyard);
}

// Off the audio thread, under the staging lock the audio thread blocks on. The
// predecessor is handed back so the caller destroys it after unlocking.
template <typename T>
[[nodiscard]] std::unique_ptr<T> ReplaceStaged(std::unique_ptr<T>& staged, std::unique_ptr<T> incoming)
{
  std::unique_ptr<T> previous = std::move(staged);
  staged = std::move(incoming);
  return previous;
}

template <typename T>
[[nodiscard]] std::unique_ptr<T> ReplaceStaged(std::unique_ptr<T>& staged, std::nullptr_t)
{
  return ReplaceStaged(staged, std::unique_ptr<T>{});
}

// Loader results the audio thread has drained. Their strings and the container
// itself are freed in OnIdle, so a processed batch is swapped into an empty slot.
constexpr size_t kSpentLoaderBatchSlots = 4;

// Audio thread, under the lock OnIdle reaps the slots under. False means every
// slot still waits for OnIdle: the caller keeps the batch and retries.
template <typename Batch, size_t N>
bool HandOffSpentBatch(Batch& batch, std::array<Batch, N>& slots)
{
  if (batch.empty())
    return true;
  for (auto& slot : slots)
  {
    if (slot.empty())
    {
      slot.swap(batch);
      return true;
    }
  }
  return false;
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

// The live path of an asset, handed from the audio thread (which applies the
// asset) to OnIdle (which owns the WDL_String). Both sides touch it under the
// staging lock.
struct RtPublishedPath
{
  char text[kRtPathCapacity]{};
  // The asset was removed: OnIdle clears the live path instead of committing text.
  bool clear = false;
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

// An empty path means "keep the live path" (an IR re-staged for a new sample
// rate may carry none), so it must not overwrite a publish OnIdle has not taken.
inline void PublishPathNoAlloc(RtPublishedPath& slot, const char* src)
{
  if (!src || !*src)
    return;
  CopyPathNoAlloc(slot.text, kRtPathCapacity, src);
  slot.clear = false;
  slot.dirty.store(true, std::memory_order_release);
}

inline void PublishPathClearNoAlloc(RtPublishedPath& slot)
{
  slot.text[0] = '\0';
  slot.clear = true;
  slot.dirty.store(true, std::memory_order_release);
}

enum class PublishedPathAction
{
  None,
  Commit,
  Clear
};

inline PublishedPathAction TakePublishedPath(RtPublishedPath& slot)
{
  if (!slot.dirty.exchange(false, std::memory_order_acq_rel))
    return PublishedPathAction::None;
  const bool clear = slot.clear;
  slot.clear = false;
  return clear ? PublishedPathAction::Clear : PublishedPathAction::Commit;
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

// OnIdle, under the staging lock. `out` must already hold kRtPathCapacity so the
// copy cannot allocate while the audio thread may be waiting on the lock; an
// unreserved `out` leaves the publish for the next idle tick.
inline PublishedPathAction TakePublishedPath(RtPublishedPath& slot, std::string& out)
{
  if (out.capacity() < kRtPathCapacity)
    return PublishedPathAction::None;
  const PublishedPathAction action = TakePublishedPath(slot);
  if (action == PublishedPathAction::Commit)
    out.assign(slot.text);
  return action;
}

// OnIdle, after the staging lock is released: the WDL_String work. Found by ADL,
// so the plugin's WdlStagedPathPair and a test's pair type both work.
template <typename PathPair>
void ApplyPublishedPath(PublishedPathAction action, const std::string& path, PathPair& paths)
{
  switch (action)
  {
    case PublishedPathAction::Commit:
      StagePathOnSuccess(paths, path.c_str());
      CommitStagedPathOnApply(paths);
      break;
    case PublishedPathAction::Clear: ClearLiveAndStagedPath(paths); break;
    case PublishedPathAction::None: break;
  }
}

} // namespace volum::dsp_staging
