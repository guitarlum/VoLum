// VoLum ProcessBlock helpers.
//
// Tail-included from NeuralAmpModeler.cpp for file-size hygiene (not a separate TU).

sample** NeuralAmpModeler::_VolumProcessPreChain(sample** preAmpPointers, const volum::ProcessingPlan& processingPlan,
                                                 const size_t numChannelsInternal, const int nFrames,
                                                 const double sampleRate)
{
  if (processingPlan.runPreComp)
  {
    mPreCompressor.SetParams(GetParam(kPreCompAmount)->Value(), GetParam(kPreCompRatio)->Value(),
                             GetParam(kPreCompAttack)->Value(), GetParam(kPreCompRelease)->Value(),
                             1.0, GetParam(kPreCompLevel)->Value(), sampleRate);
    preAmpPointers = mPreCompressor.Process(preAmpPointers, numChannelsInternal, nFrames);
  }

  auto processPreSlot = [&](int slot, int activeParam, int gainParam, int bassParam, int midParam, int midFreqParam,
                            int trebleParam, int levelParam) {
    (void)activeParam;
    if (!processingPlan.runPreNam[slot])
      return;

    const double inGain = std::pow(10.0, GetParam(gainParam)->Value() / 20.0);
    mPreInputGain[slot].SetParams(recursive_linear_filter::LevelParams(inGain));
    preAmpPointers = mPreInputGain[slot].Process(preAmpPointers, numChannelsInternal, nFrames);

    mPreModel[slot]->process(preAmpPointers[0], mOutputPointers[0], nFrames);
    preAmpPointers = mOutputPointers;

    mPreEq[slot].SetParams(GetParam(bassParam)->Value(), GetParam(midParam)->Value(),
                           GetParam(midFreqParam)->Value(), GetParam(trebleParam)->Value());
    preAmpPointers = mPreEq[slot].Process(preAmpPointers, numChannelsInternal, nFrames);

    const double outGain = volum::DbToAmpWithMuteFloor(GetParam(levelParam)->Value(), GetParam(levelParam)->GetMin());
    mPreOutputGain[slot].SetParams(recursive_linear_filter::LevelParams(outGain));
    preAmpPointers = mPreOutputGain[slot].Process(preAmpPointers, numChannelsInternal, nFrames);
  };

  processPreSlot(0, kPreNam1Active, kPreNam1Gain, kPreNam1Bass, kPreNam1Mid, kPreNam1MidFreq, kPreNam1Treble,
                 kPreNam1Level);
  processPreSlot(1, kPreNam2Active, kPreNam2Gain, kPreNam2Bass, kPreNam2Mid, kPreNam2MidFreq, kPreNam2Treble,
                 kPreNam2Level);
  return preAmpPointers;
}

void NeuralAmpModeler::_VolumProcessPostChain(iplug::sample** outputs, const volum::ProcessingPlan& processingPlan,
                                              const size_t numChannelsExternalOut, const int nFrames,
                                              const double sampleRate)
{
  iplug::sample** postPointers = outputs;

  // POST bypass-edge clear: when the user bypasses Delay or Reverb (active -> inactive
  // for any reason: explicit toggle, preset switch, missing model), the effect's
  // internal lines still hold the previous tail. Without this, re-enabling the effect
  // later replays a "ghost" of whatever was playing before bypass. Mirrors how
  // _FallbackDSP already clears POST when the main model goes missing.
  if (mPostDelayWasActive && !processingPlan.runDelay)
    mDelay.Reset();
  if (mPostReverbWasActive && !processingPlan.runReverb)
    mReverb.Reset();
  mPostDelayWasActive = processingPlan.runDelay;
  mPostReverbWasActive = processingPlan.runReverb;

  if (processingPlan.runDelay)
  {
    mDelay.SetParams(GetParam(kDelayTime)->Value(), GetParam(kDelayFeedback)->Value(),
                     GetParam(kDelayMix)->Value(), GetParam(kDelayMode)->Int(), sampleRate,
                     GetParam(kDelayTone)->Value(), GetParam(kDelayAge)->Value(),
                     GetParam(kDelayPingPong)->Bool());
    postPointers = mDelay.Process(postPointers, numChannelsExternalOut, nFrames);
  }

  if (processingPlan.runReverb)
  {
    mReverb.SetParams(GetParam(kReverbMix)->Value(), GetParam(kReverbDecay)->Value(),
                      GetParam(kReverbTone)->Value(), GetParam(kReverbPreDelay)->Value(),
                      GetParam(kReverbShimmer)->Value(), GetParam(kReverbMode)->Int(), sampleRate,
                      GetParam(kReverbSubMode)->Int());
    postPointers = mReverb.Process(postPointers, numChannelsExternalOut, nFrames);
  }

  if (postPointers != outputs)
  {
    for (size_t c = 0; c < numChannelsExternalOut; c++)
      std::memcpy(outputs[c], postPointers[c], nFrames * sizeof(iplug::sample));
  }
}
