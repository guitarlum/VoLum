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
  request.blockSize = GetBlockSize();

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
  request.blockSize = GetBlockSize();

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
  request.blockSize = GetBlockSize();

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

void NeuralAmpModeler::_VolumDrainLoaderResults()
{
  std::deque<VoLumLoadResult> results;
  {
    std::unique_lock<std::mutex> lock(mVolumLoaderMutex, std::try_to_lock);
    if (!lock.owns_lock())
      return;
    results.swap(mVolumLoadResults);
  }

  for (auto& result : results)
  {
    if (result.model != nullptr && (result.sampleRate != GetSampleRate() || result.blockSize != GetBlockSize()))
    {
      result.model->Reset(GetSampleRate(), GetBlockSize());
      result.sampleRate = GetSampleRate();
      result.blockSize = GetBlockSize();
    }

    if (result.kind == VoLumLoadKind::Main)
    {
      bool superseded = false;
      {
        std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
        if (mVolumLoadingMainPath == result.path)
          mVolumLoadingMainPath.clear();
        else if (!mVolumLoadingMainPath.empty())
          superseded = true;
      }
      if (superseded)
        continue;
      mVolumIsLoading.store(false);
      if (mVolumNeedsLoad.load())
        continue;

      if (!result.error.empty())
      {
        // Keep the last known-good model for uninterrupted audio, but tell the
        // main/UI thread to make the fallback explicit in the footer.
        mVolumMainLoadFailed.store(true);
        VOLUM_LOG("model", "MAIN load FAILED " + result.path + " : " + result.error);
        continue;
      }

      if (result.model != nullptr)
      {
        std::lock_guard<std::mutex> lock(mStagingMutex);
        mStagedModel = std::move(result.model);
        volum::dsp_staging::StagePathOnSuccess(mNAMPaths, result.path.c_str());
      }
      VOLUM_LOG("model", "MAIN loaded " + result.path);
      continue;
    }

    if (result.kind == VoLumLoadKind::Support)
    {
      {
        std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
        if (mVolumLoadingSupportPath == result.path)
          mVolumLoadingSupportPath.clear();
      }
      mVolumSupportIsLoading.store(false);
      if (mVolumSupportNeedsLoad.load())
        continue;

      if (!result.error.empty())
      {
        mShouldRemoveSupportModel.store(true);
        VOLUM_LOG("model", "SUPPORT load FAILED " + result.path + " : " + result.error);
        continue;
      }

      if (result.model != nullptr)
      {
        std::lock_guard<std::mutex> lock(mStagingMutex);
        mStagedSupportModel = std::move(result.model);
      }
      VOLUM_LOG("model", "SUPPORT loaded " + result.path);
      continue;
    }

    const int slot = result.slot;
    if (slot < 0 || slot >= 2)
      continue;

    mVolumPreIsLoading[slot].store(false);
    {
      std::lock_guard<std::mutex> lock(mVolumLoaderMutex);
      if (mVolumLoadingPrePath[slot] == result.path)
        mVolumLoadingPrePath[slot].clear();
    }
    if (mVolumPreNeedsLoad[slot].load())
      continue;

    if (!result.error.empty())
    {
      mShouldRemovePreModel[slot].store(true);
      VOLUM_LOG("model", "PRE slot " + std::to_string(slot) + " load FAILED " + result.path + " : " + result.error);
      continue;
    }

    if (result.model != nullptr)
    {
      std::lock_guard<std::mutex> lock(mStagingMutex);
      mStagedPreModel[slot] = std::move(result.model);
    }
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
      rel.empty()
        ? std::string()
        : volum::content::PathToUtf8(volum::content::GlobalContentStore().ResolveStored(rel));
    if (fileToLoad.empty())
    {
      mShouldRemoveSupportModel.store(true);
      mVolumSupportIsLoading.store(false);
      mVolumLastLoadedSupportFile.clear();
      return;
    }
    mVolumSupportIsLoading.store(true);
    mVolumLastLoadedSupportFile =
      volum::content::PathToUtf8(volum::content::PathFromUtf8(fileToLoad).filename());
    _VolumQueueSupportModelLoad(fileToLoad, -1); // -1 = custom: skip factory prefetch
    return;
  }

  const int supportAmpIdx = GetParam(kSupportAmpIdx)->Int();
  if (!dualActive || supportAmpIdx < 0 || supportAmpIdx >= volum::kAmpCount || mVolumRigsRoot.empty())
  {
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  namespace fs = std::filesystem;
  const int speakerIdx = std::clamp(GetParam(kSupportSpeakerIdx)->Int(), 0, 3);
  auto channels = volum::DiscoverChannels(
    volum::content::PathFromUtf8(mVolumRigsRoot),
    volum::kAmps[supportAmpIdx].folderName,
    volum::kSpeakerPrefixes[speakerIdx]);
  if (channels.empty())
  {
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
    mShouldRemoveSupportModel.store(true);
    mVolumSupportIsLoading.store(false);
    mVolumLastLoadedSupportFile.clear();
    return;
  }

  mVolumSupportIsLoading.store(true);
  mVolumLastLoadedSupportFile =
    volum::content::PathToUtf8(volum::content::PathFromUtf8(fileToLoad).filename());
  _VolumQueueSupportModelLoad(fileToLoad, supportAmpIdx);
}