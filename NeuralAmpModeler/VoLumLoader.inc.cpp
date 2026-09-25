// VoLum async-loader thread implementation.
//
// This file is a tail-include (compiled as part of NeuralAmpModeler.cpp);
// it is NOT a separate translation unit. The intent is purely file-size
// hygiene - the loader queue, worker thread, and PRE/SUPPORT request
// dispatchers used to live at the bottom of NeuralAmpModeler.cpp.
//
// Owned class members (mVolum*Queue, mVolumLoaderThread, atomic flags) are
// declared in NeuralAmpModeler.h and accessed normally.

void NeuralAmpModeler::_VolumStartLoader()
{
  if (mVolumLoaderThread.joinable())
    return;

  mVolumLoaderStop.store(false);
  mVolumLoaderThread = std::thread([this]() { _VolumLoaderThreadMain(); });
}

void NeuralAmpModeler::_VolumStopLoader()
{
  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    mVolumLoaderStop.store(true);
    mVolumLoadRequests.clear();
    mVolumLoadingMainPath.clear();
    mVolumLoadingSupportPath.clear();
    mVolumLoadingPrePath[0].clear();
    mVolumLoadingPrePath[1].clear();
  }
  mVolumLoaderCv.notify_one();

  if (mVolumLoaderThread.joinable())
    mVolumLoaderThread.join();
}

void NeuralAmpModeler::_VolumQueueMainModelLoad(std::string fileToLoad, int ampIdx, std::string rigsRoot)
{
  if (fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Main;
  request.ampIdx = ampIdx;
  request.fileToLoad = fileToLoad;
  request.rigsRoot = std::move(rigsRoot);
  request.sampleRate = GetSampleRate();
  request.blockSize = volum::dsp_staging::NamResetBlockSize(GetBlockSize());

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumLoadingMainPath == fileToLoad)
      return;

    mVolumLoadingMainPath = fileToLoad;
    _VolumDropQueuedLoadRequests([](const VoLumLoadRequest& queued) {
      return queued.kind == VoLumLoadKind::Main || queued.kind == VoLumLoadKind::MainPrefetch;
    });
    mVolumLoadRequests.push_front(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumQueueMainPrefetch(std::string fileToLoad)
{
  if (fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::MainPrefetch;
  request.fileToLoad = fileToLoad;

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumDspCache.find(fileToLoad) != mVolumDspCache.end())
      return;
    const auto alreadyQueued =
      std::any_of(mVolumLoadRequests.begin(), mVolumLoadRequests.end(), [&](const VoLumLoadRequest& queued) {
        return queued.kind == VoLumLoadKind::MainPrefetch && queued.fileToLoad == fileToLoad;
      });
    if (alreadyQueued)
      return;
    mVolumLoadRequests.push_back(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumQueueSupportModelLoad(std::string fileToLoad, int ampIdx)
{
  if (fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Support;
  request.ampIdx = ampIdx;
  request.fileToLoad = fileToLoad;
  request.sampleRate = GetSampleRate();
  request.blockSize = volum::dsp_staging::NamResetBlockSize(GetBlockSize());

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumLoadingSupportPath == fileToLoad)
      return;
    mVolumLoadingSupportPath = fileToLoad;
    _VolumDropQueuedLoadRequests([](const VoLumLoadRequest& queued) { return queued.kind == VoLumLoadKind::Support; });
    mVolumLoadRequests.push_back(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

void NeuralAmpModeler::_VolumQueuePreNamLoad(int slot, std::string fileToLoad)
{
  if (slot < 0 || slot >= 2 || fileToLoad.empty())
    return;

  VoLumLoadRequest request;
  request.kind = VoLumLoadKind::Pre;
  request.slot = slot;
  request.fileToLoad = fileToLoad;
  request.sampleRate = GetSampleRate();
  request.blockSize = volum::dsp_staging::NamResetBlockSize(GetBlockSize());

  {
    std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
    if (mVolumLoadingPrePath[slot] == fileToLoad)
      return;
    mVolumLoadingPrePath[slot] = fileToLoad;
    _VolumDropQueuedLoadRequests(
      [&](const VoLumLoadRequest& queued) { return queued.kind == VoLumLoadKind::Pre && queued.slot == slot; });
    mVolumLoadRequests.push_back(std::move(request));
  }
  mVolumLoaderCv.notify_one();
}

// Runs on the audio thread, from _ApplyDSPStaging inside ProcessBlock. Nothing
// here may do file I/O, Reset/prewarm a NAM, WDL_String::Set, or destroy a
// ResamplingNAM - load outcomes are logged by _VolumLoaderThreadMain, stale
// rate/block results re-queue via mVolumNeedsLoad, and outgoing models go to
// the OnIdle graveyard. The drained batch itself (its heap strings and deque
// blocks) is handed to OnIdle too, via mVolumSpentLoadResults.
void NeuralAmpModeler::_VolumDrainLoaderResults()
{
  // A batch still parked from an earlier drain: every spent slot was waiting for
  // OnIdle. New results stay queued on the loader side until it can go.
  if (!mVolumDrainBatch.empty())
  {
    std::lock_guard<std::mutex> lock(mStagingMutex);
    if (!volum::dsp_staging::HandOffSpentBatch(mVolumDrainBatch, mVolumSpentLoadResults))
      return;
  }

  auto& results = mVolumDrainBatch;
  {
    std::unique_lock<std::mutex> lock(mVolumLoaderMutex, std::try_to_lock);
    if (!lock.owns_lock())
      return;
    results.swap(mVolumLoadResults);
    // Path bookkeeping stays inside this one try_lock. A later blocking lock
    // used to stall the audio thread behind the loader thread.
    for (auto& result : results)
    {
      if (result.kind == VoLumLoadKind::Main)
      {
        if (mVolumLoadingMainPath == result.path)
          mVolumLoadingMainPath.clear();
        else if (!mVolumLoadingMainPath.empty())
          result.superseded = true;
      }
      else if (result.kind == VoLumLoadKind::Support)
      {
        if (mVolumLoadingSupportPath == result.path)
          mVolumLoadingSupportPath.clear();
      }
      else if (result.slot >= 0 && result.slot < 2)
      {
        if (mVolumLoadingPrePath[result.slot] == result.path)
          mVolumLoadingPrePath[result.slot].clear();
      }
    }
  }
  if (results.empty())
    return;

  const double liveRate = GetSampleRate();
  const int liveBlock = volum::dsp_staging::NamResetBlockSize(GetBlockSize());

  for (auto& result : results)
  {
    const bool rateMismatch =
      result.model != nullptr && (result.sampleRate != liveRate || result.blockSize != liveBlock);

    if (result.kind == VoLumLoadKind::Main)
    {
      bool superseded = result.superseded;
      if (!superseded)
        mVolumIsLoading.store(false);

      const auto action = volum::dsp_staging::DecideLoaderResult(
        result.model != nullptr, superseded, mVolumNeedsLoad.load(), rateMismatch, !result.error.empty());
      if (action == volum::dsp_staging::LoaderResultAction::RetireAndReload)
        mVolumNeedsLoad.store(true);
      else if (action == volum::dsp_staging::LoaderResultAction::Ignore && !result.error.empty())
      {
        // Keep the last known-good model for uninterrupted audio, but tell the
        // main/UI thread to make the fallback explicit in the footer.
        mVolumMainLoadFailed.store(true);
      }
      else if (action == volum::dsp_staging::LoaderResultAction::Stage)
      {
        std::lock_guard<std::mutex> lock(mStagingMutex);
        volum::dsp_staging::StageIncomingModel(mStagedModel, result.model, mDspGraveyard);
        volum::dsp_staging::CopyPathNoAlloc(mPendingNamPath, volum::dsp_staging::kRtPathCapacity, result.path.c_str());
      }
      continue;
    }

    if (result.kind == VoLumLoadKind::Support)
    {
      mVolumSupportIsLoading.store(false);

      const auto action = volum::dsp_staging::DecideLoaderResult(
        result.model != nullptr, false, mVolumSupportNeedsLoad.load(), rateMismatch, !result.error.empty());
      if (action == volum::dsp_staging::LoaderResultAction::RetireAndReload)
        mVolumSupportNeedsLoad.store(true);
      else if (action == volum::dsp_staging::LoaderResultAction::Ignore && !result.error.empty())
        mShouldRemoveSupportModel.store(true);
      else if (action == volum::dsp_staging::LoaderResultAction::Stage)
      {
        std::lock_guard<std::mutex> lock(mStagingMutex);
        volum::dsp_staging::StageIncomingModel(mStagedSupportModel, result.model, mDspGraveyard);
      }
      continue;
    }

    const int slot = result.slot;
    if (slot < 0 || slot >= 2)
      continue;

    mVolumPreIsLoading[slot].store(false);

    const auto action = volum::dsp_staging::DecideLoaderResult(
      result.model != nullptr, false, mVolumPreNeedsLoad[slot].load(), rateMismatch, !result.error.empty());
    if (action == volum::dsp_staging::LoaderResultAction::RetireAndReload)
      mVolumPreNeedsLoad[slot].store(true);
    else if (action == volum::dsp_staging::LoaderResultAction::Ignore && !result.error.empty())
      mShouldRemovePreModel[slot].store(true);
    else if (action == volum::dsp_staging::LoaderResultAction::Stage)
    {
      std::lock_guard<std::mutex> lock(mStagingMutex);
      volum::dsp_staging::StageIncomingModel(mStagedPreModel[slot], result.model, mDspGraveyard);
    }
  }

  {
    std::lock_guard<std::mutex> lock(mStagingMutex);
    for (auto& result : results)
      volum::dsp_staging::RetireToGraveyard(result.model, mDspGraveyard);
    // False parks the batch; the next drain retries before taking new results.
    volum::dsp_staging::HandOffSpentBatch(results, mVolumSpentLoadResults);
  }
}

void NeuralAmpModeler::_VolumLoaderThreadMain()
{
  namespace fs = std::filesystem;

  auto touchCache = [&](const std::string& key) {
    mVolumDspCacheOrder.erase(
      std::remove(mVolumDspCacheOrder.begin(), mVolumDspCacheOrder.end(), key), mVolumDspCacheOrder.end());
    mVolumDspCacheOrder.push_front(key);
  };

  auto storeCache = [&](const std::string& key, nam::dspData&& config) {
    mVolumDspCache[key] = std::move(config);
    touchCache(key);
    while (mVolumDspCacheOrder.size() > kVolumDspCacheMaxEntries)
    {
      mVolumDspCache.erase(mVolumDspCacheOrder.back());
      mVolumDspCacheOrder.pop_back();
    }
  };

  auto makeModel = [&](const std::string& path) {
    auto cacheIt = mVolumDspCache.find(path);
    if (cacheIt != mVolumDspCache.end())
    {
      touchCache(path);
      // Core may consume/move fields during construction, so keep the cached copy immutable.
      nam::dspData cachedConfig = cacheIt->second;
      return nam::get_dsp(cachedConfig);
    }

    nam::dspData conf;
    auto model = nam::get_dsp(fs::u8path(path), conf);
    storeCache(path, std::move(conf));
    return model;
  };

  for (;;)
  {
    VoLumLoadRequest request;
    {
      std::unique_lock<std::mutex> lock(mVolumLoaderMutex);
      mVolumLoaderCv.wait(lock, [&]() { return mVolumLoaderStop.load() || !mVolumLoadRequests.empty(); });

      if (mVolumLoaderStop.load() && mVolumLoadRequests.empty())
        break;

      request = std::move(mVolumLoadRequests.front());
      mVolumLoadRequests.pop_front();
    }

    VoLumLoadResult result;
    result.kind = request.kind;
    result.slot = request.slot;
    result.path = request.fileToLoad;
    result.sampleRate = request.sampleRate;
    result.blockSize = request.blockSize;

    try
    {
      if (request.kind == VoLumLoadKind::Main)
      {
        auto model = makeModel(request.fileToLoad);
        result.model = std::make_unique<ResamplingNAM>(std::move(model), request.sampleRate);
        // VoLum: select the A2 Lite/Full slice (no-op on non-slimmable models)
        // before Reset so only the chosen slice is prewarmed.
        result.model->SetSlimmableSize(mVolumLiteMode.load() ? 0.0 : 1.0);
        result.model->Reset(request.sampleRate, request.blockSize);

        // ampIdx < 0 marks a custom-amp load (files live in the content library,
        // not the factory rig tree), so skip the factory sibling-prefetch scan.
        if (!mVolumNeedsLoad.load() && request.ampIdx >= 0 && !request.rigsRoot.empty())
        {
          const fs::path ampDir = fs::path(request.rigsRoot) / volum::kAmps[request.ampIdx].folderName;
          std::error_code ec;
          if (fs::is_directory(ampDir, ec))
          {
            for (const auto& entry : fs::directory_iterator(ampDir, ec))
            {
              if (mVolumNeedsLoad.load() || mVolumLoaderStop.load())
                break;
              if (!entry.is_regular_file(ec))
                continue;

              if (entry.path().extension() != ".nam")
                continue;

              std::error_code pathEc;
              const std::string prefetchPath = fs::weakly_canonical(entry.path(), pathEc).string();
              if (pathEc || prefetchPath.empty() || prefetchPath == request.fileToLoad)
                continue;
              if (mVolumDspCache.find(prefetchPath) == mVolumDspCache.end())
              {
                _VolumQueueMainPrefetch(prefetchPath);
              }
            }
          }
        }
      }
      else if (request.kind == VoLumLoadKind::MainPrefetch)
      {
        if (!mVolumNeedsLoad.load() && !mVolumLoaderStop.load()
            && mVolumDspCache.find(request.fileToLoad) == mVolumDspCache.end())
        {
          nam::dspData conf;
          nam::get_dsp(fs::u8path(request.fileToLoad), conf);
          storeCache(request.fileToLoad, std::move(conf));
        }
      }
      else
      {
        auto model = makeModel(request.fileToLoad);
        result.model = std::make_unique<ResamplingNAM>(std::move(model), request.sampleRate);
        // VoLum: select the A2 Lite/Full slice (no-op on non-slimmable models)
        // before Reset so only the chosen slice is prewarmed. Covers the SUPPORT
        // amp and both PRE NAM pedal slots.
        result.model->SetSlimmableSize(mVolumLiteMode.load() ? 0.0 : 1.0);
        result.model->Reset(request.sampleRate, request.blockSize);
      }
    }
    catch (const std::runtime_error& e)
    {
      result.error = e.what();
      if (request.kind == VoLumLoadKind::Main)
        std::cerr << "VoLum load failed: " << result.error << std::endl;
      else if (request.kind == VoLumLoadKind::Support)
        std::cerr << "VoLum support load failed: " << result.error << std::endl;
      else if (request.kind == VoLumLoadKind::MainPrefetch)
        std::cerr << "VoLum prefetch failed: " << result.error << std::endl;
      else
        std::cerr << "VoLum PRE load failed: " << result.error << std::endl;
    }

    if (request.kind == VoLumLoadKind::MainPrefetch)
      continue;

    // Load outcomes are logged here, on the loader thread, and never from
    // _VolumDrainLoaderResults: the drain runs inside ProcessBlock, and every log
    // entry stats a file, may rename it, and opens an ofstream under a mutex.
    // Doing that from the audio thread meant every model load - so every amp,
    // channel or cab switch - did blocking file I/O in the realtime callback.
    // This also reports loads the drain later discards as superseded, which is
    // the more honest record of what was actually read from disk.
    {
      std::string kindLabel;
      switch (request.kind)
      {
        case VoLumLoadKind::Main: kindLabel = "MAIN"; break;
        case VoLumLoadKind::Support: kindLabel = "SUPPORT"; break;
        default: kindLabel = "PRE slot " + std::to_string(request.slot); break;
      }
      if (!result.error.empty())
        VOLUM_LOG("model", kindLabel + " load FAILED " + result.path + " : " + result.error);
      else
        // "read", not "loaded": this runs on the worker as soon as the file is
        // parsed. Whether the model reaches the audio graph is decided later, when
        // the audio thread drains the queue and may discard it as superseded. The
        // logging moved here to keep file I/O off that thread, and a line claiming
        // a model was in use when it never was made the log misleading.
        VOLUM_LOG("model", kindLabel + " read " + result.path);
    }

    {
      std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
      mVolumLoadResults.push_back(std::move(result));
    }
  }
}

void NeuralAmpModeler::_VolumRequestPreNamLoad(int slot)
{
  if (slot < 0 || slot >= 2)
    return;

  const int activeParam = slot == 0 ? kPreNam1Active : kPreNam2Active;
  const int captureParam = slot == 0 ? kPreNam1Capture : kPreNam2Capture;
  if (!volum::ShouldLoadPrePedalCapture(GetParam(activeParam)->Bool(), GetParam(captureParam)->Int()))
  {
    mShouldRemovePreModel[slot].store(true);
    mVolumPreIsLoading[slot].store(false);
    return;
  }

  // Resolve to an absolute path: factory captures under rigs/PrePedals, custom
  // imported pedals (index >= kCustomPedalIndexBase) from the content library.
  const std::string fileToLoad = _VolumGetPreCaptureLoadPath(GetParam(captureParam)->Int());
  if (fileToLoad.empty())
  {
    mShouldRemovePreModel[slot].store(true);
    mVolumPreIsLoading[slot].store(false);
    return;
  }

  mVolumPreIsLoading[slot].store(true);
  _VolumQueuePreNamLoad(slot, fileToLoad);
}

void NeuralAmpModeler::_VolumRequestSupportModelLoad()
{
  const bool dualActive = GetParam(kDualAmpActive)->Bool();

  // Custom SUPPORT partner (F6 dual amp): resolve the .nam from the custom amp's
  // manifest (content library) for the focused (slot, channel) rather than the
  // factory rig tree. supportAmpIdx is -1 while a custom partner is active.
  if (mVolumCustomSupportIdx >= 0)
  {
    if (!dualActive)
    {
      mVolumSupportSelected.store(false);
      mShouldRemoveSupportModel.store(true);
      mVolumSupportIsLoading.store(false);
      mVolumLastLoadedSupportFile.clear();
      return;
    }
    const auto amp = volum::custom::CustomAmpAt(mVolumCustomSupportIdx);
    std::string rel = volum::content::CaptureFileFor(amp, mVolumCustomSupportSlot, mVolumCustomSupportChannel);
    if (rel.empty())
    {
      int s = volum::custom::kDirectSlot, c = 1;
      if (volum::content::DefaultCaptureSelection(amp, s, c))
        rel = volum::content::CaptureFileFor(amp, s, c);
    }
    const std::string fileToLoad =
      rel.empty() ? std::string() : volum::content::PathToUtf8(volum::content::GlobalContentStore().ResolveStored(rel));
    if (fileToLoad.empty())
    {
      mVolumSupportSelected.store(false);
      mShouldRemoveSupportModel.store(true);
      mVolumSupportIsLoading.store(false);
      mVolumLastLoadedSupportFile.clear();
      return;
    }
    mVolumSupportSelected.store(true);
    mVolumSupportIsLoading.store(true);
    mVolumLastLoadedSupportFile = volum::content::PathToUtf8(volum::content::PathFromUtf8(fileToLoad).filename());
    _VolumQueueSupportModelLoad(fileToLoad, -1); // -1 = custom: skip factory prefetch
    return;
  }

  const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
  if (!dualActive || supportAmpIdx < 0 || supportAmpIdx >= volum::kAmpCount || mVolumRigsRoot.empty())
  {
    mVolumSupportSelected.store(false);
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  namespace fs = std::filesystem;
  const int speakerIdx = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3);
  auto channels = volum::DiscoverChannels(volum::content::PathFromUtf8(mVolumRigsRoot),
                                          volum::kAmps[supportAmpIdx].folderName, volum::kSpeakerPrefixes[speakerIdx]);
  if (channels.empty())
  {
    mVolumSupportSelected.store(false);
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  int channelIdx = std::clamp(GetParam(kSupportChannelIdx)->Int(), 0, static_cast<int>(channels.size()) - 1);
  if (channelIdx != GetParam(kSupportChannelIdx)->Int())
  {
    GetParam(kSupportChannelIdx)->Set(channelIdx);
    SendParameterValueFromDelegate(kSupportChannelIdx, GetParam(kSupportChannelIdx)->GetNormalized(), true);
  }

  const auto rigPath = volum::content::PathFromUtf8(mVolumRigsRoot) / volum::kAmps[supportAmpIdx].folderName
                       / channels[channelIdx].filename;
  std::error_code ec;
  const std::string fileToLoad = volum::content::PathToUtf8(fs::weakly_canonical(rigPath, ec));
  if (fileToLoad.empty())
  {
    mVolumSupportSelected.store(false);
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  mVolumSupportSelected.store(true);
  mVolumSupportIsLoading.store(true);
  mVolumLastLoadedSupportFile = volum::content::PathToUtf8(volum::content::PathFromUtf8(fileToLoad).filename());
  _VolumQueueSupportModelLoad(fileToLoad, supportAmpIdx);
}