// VoLum ProcessBlock helpers.
//
// Tail-included from NeuralAmpModeler.cpp for file-size hygiene (not a separate TU).

namespace
{
dsp::noise_gate::TriggerParams VolumMakeNoiseGateTriggerParams(double threshold)
{
  const double time = 0.01;
  const double ratio = 0.1;
  const double openTime = 0.005;
  const double holdTime = 0.01;
  const double closeTime = 0.05;
  return {time, threshold, ratio, openTime, holdTime, closeTime};
}
} // namespace

iplug::sample** NeuralAmpModeler::_VolumProcessPreChain(iplug::sample** preAmpPointers,
                                                        const volum::ProcessingPlan& processingPlan,
                                                        const size_t numChannelsInternal, const int nFrames,
                                                        const double sampleRate)
{
  if (processingPlan.runPrePitch)
  {
    // Reconfigure (block-size/quality change) happens off the audio thread in
    // OnIdle. Here we only try-lock; if a reconfigure is mid-flight we pass the
    // dry signal through for this block rather than allocate or block.
    std::unique_lock<std::mutex> lock(mPrePitchMutex, std::try_to_lock);
    if (lock.owns_lock() && mPitch.Configured())
    {
      const int mode = std::clamp(GetParam(kPrePitchMode)->Int(), 0, volum::kVoLumPitchModeCount - 1);
      const int voicing = GetParam(kPrePitchVoicing)->Int();
      const int transChar = std::clamp(GetParam(kPrePitchTransChar)->Int(), 0, volum::kVoLumPitchCharacterCount - 1);
      mPitch.SetParams(static_cast<dsp::effect::VoLumPitch::Mode>(mode), GetParam(kPrePitchSemitones)->Value(),
                       GetParam(kPrePitchMix)->Value(), GetParam(kPrePitchOctDown)->Value(),
                       GetParam(kPrePitchOctUp)->Value(), GetParam(kPrePitchDry)->Value(),
                       static_cast<dsp::effect::VoLumPitch::Voicing>(voicing), GetParam(kPrePitchLevel)->Value(),
                       static_cast<dsp::effect::VoLumPitch::Character>(transChar));
      preAmpPointers = mPitch.Process(preAmpPointers, numChannelsInternal, nFrames);
    }
  }

  if (processingPlan.runPreComp)
  {
    mPreCompressor.SetParams(GetParam(kPreCompAmount)->Value(), GetParam(kPreCompRatio)->Value(),
                             GetParam(kPreCompAttack)->Value(), GetParam(kPreCompRelease)->Value(), 1.0,
                             GetParam(kPreCompLevel)->Value(), sampleRate);
    preAmpPointers = mPreCompressor.Process(preAmpPointers, numChannelsInternal, nFrames);
  }

  auto processPreSlot = [&](int slot, int gainParam, int bassParam, int midParam, int midFreqParam, int trebleParam,
                            int levelParam) {
    if (!processingPlan.runPreNam[slot])
      return;

    const double inGain = std::pow(10.0, GetParam(gainParam)->Value() / 20.0);
    mPreInputGain[slot].SetParams(recursive_linear_filter::LevelParams(inGain));
    preAmpPointers = mPreInputGain[slot].Process(preAmpPointers, numChannelsInternal, nFrames);

    mPreModel[slot]->process(preAmpPointers[0], mOutputPointers[0], nFrames);
    preAmpPointers = mOutputPointers;

    mPreEq[slot].SetParams(GetParam(bassParam)->Value(), GetParam(midParam)->Value(), GetParam(midFreqParam)->Value(),
                           GetParam(trebleParam)->Value());
    preAmpPointers = mPreEq[slot].Process(preAmpPointers, numChannelsInternal, nFrames);

    const double outGain = volum::DbToAmpWithMuteFloor(GetParam(levelParam)->Value(), GetParam(levelParam)->GetMin());
    mPreOutputGain[slot].SetParams(recursive_linear_filter::LevelParams(outGain));
    preAmpPointers = mPreOutputGain[slot].Process(preAmpPointers, numChannelsInternal, nFrames);
  };

  processPreSlot(0, kPreNam1Gain, kPreNam1Bass, kPreNam1Mid, kPreNam1MidFreq, kPreNam1Treble, kPreNam1Level);
  processPreSlot(1, kPreNam2Gain, kPreNam2Bass, kPreNam2Mid, kPreNam2MidFreq, kPreNam2Treble, kPreNam2Level);
  return preAmpPointers;
}

iplug::sample** NeuralAmpModeler::_VolumProcessMainAmpChain(iplug::sample** preAmpPointers,
                                                            const volum::ProcessingPlan& processingPlan,
                                                            const size_t numChannelsInternal, const int nFrames,
                                                            const double sampleRate)
{
  sample** triggerOutput = preAmpPointers;
  if (processingPlan.runNoiseGate)
  {
    const auto triggerParams = VolumMakeNoiseGateTriggerParams(GetParam(kNoiseGateThreshold)->Value());
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(preAmpPointers, numChannelsInternal, nFrames);
  }

  if (processingPlan.runMainModel)
  {
    mModel->process(triggerOutput[0], mOutputPointers[0], nFrames);
    if (volum::ScrubNonFiniteInPlace(mOutputPointers[0], static_cast<std::size_t>(nFrames)))
    {
      mDelay.Reset();
      mReverb.Reset();
      mTremolo.Reset();
    }
  }
  else
  {
    _FallbackDSP(triggerOutput, mOutputPointers, numChannelsInternal, nFrames);
    if (!mPostEffectsClearedForMissingModel)
    {
      mDelay.Reset();
      mReverb.Reset();
      mTremolo.Reset();
      mPostEffectsClearedForMissingModel = true;
    }
  }
  if (processingPlan.runMainModel)
    mPostEffectsClearedForMissingModel = false;

  sample** hpfPointers = mOutputPointers;
  if (processingPlan.runMainModel)
  {
    sample** gateGainOutput = processingPlan.runNoiseGate
                                ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, nFrames)
                                : mOutputPointers;

    sample** toneStackOutPointers = (processingPlan.runToneStack && mToneStack != nullptr)
                                      ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                      : gateGainOutput;

    sample** irPointers = toneStackOutPointers;
    if (processingPlan.runIR)
      irPointers = mIR->Process(toneStackOutPointers, numChannelsInternal, nFrames);

    hpfPointers = mHighPass.Process(irPointers, numChannelsInternal, nFrames);
  }
  return hpfPointers;
}

iplug::sample* NeuralAmpModeler::_VolumProcessDualAmpSupportLane(const volum::ProcessingPlan& processingPlan,
                                                                 const size_t numChannelsInternal, const int nFrames,
                                                                 const double sampleRate)
{
  if (!processingPlan.runDualAmp)
    return nullptr;

  assert(mDualSupportLaneBuffer.capacity() >= static_cast<size_t>(nFrames)
         && "Dual-amp support scratch not pre-reserved");
  mDualSupportLaneBuffer.resize(nFrames);

  const double supportInputGain = DBToAmp(GetParam(kSupportInputLevel)->Value());
  for (size_t i = 0; i < static_cast<size_t>(nFrames); ++i)
    mDualMainLaneBuffer[i] = static_cast<sample>(static_cast<double>(mDualMainLaneBuffer[i]) * supportInputGain);

  sample* supportInputPtr = mDualMainLaneBuffer.data();
  sample* supportOutputPtr = mDualSupportLaneBuffer.data();
  sample* supportInputPointers[1] = {supportInputPtr};
  sample** supportTriggerOutput = supportInputPointers;

  if (GetParam(kSupportNoiseGateActive)->Bool())
  {
    const auto triggerParams = VolumMakeNoiseGateTriggerParams(GetParam(kSupportNoiseGateThreshold)->Value());
    mSupportNoiseGateTrigger.SetParams(triggerParams);
    mSupportNoiseGateTrigger.SetSampleRate(sampleRate);
    supportTriggerOutput = mSupportNoiseGateTrigger.Process(supportInputPointers, numChannelsInternal, nFrames);
  }

  mSupportModel->process(supportTriggerOutput[0], supportOutputPtr, nFrames);
  if (volum::ScrubNonFiniteInPlace(supportOutputPtr, static_cast<std::size_t>(nFrames)))
  {
    mDelay.Reset();
    mReverb.Reset();
    mTremolo.Reset();
  }

  sample* supportModelPointers[1] = {supportOutputPtr};
  sample** supportPostPointers = supportModelPointers;
  if (GetParam(kSupportNoiseGateActive)->Bool())
    supportPostPointers = mSupportNoiseGateGain.Process(supportPostPointers, numChannelsInternal, nFrames);

  if (processingPlan.runSupportToneStack && mSupportToneStack != nullptr)
    supportPostPointers = mSupportToneStack->Process(supportPostPointers, numChannelsInternal, nFrames);

  // Per-lane custom IR: convolve the SUPPORT amp's own cab (DIRECT capture) here,
  // mirroring the MAIN lane (tone stack -> IR -> high-pass). Independent of the
  // MAIN convolver so a custom IR is local to one lane (spec 3.2).
  if (processingPlan.runSupportIR)
    supportPostPointers = mSupportIR->Process(supportPostPointers, numChannelsInternal, nFrames);

  supportPostPointers = mSupportHighPass.Process(supportPostPointers, numChannelsInternal, nFrames);
  return supportPostPointers[0];
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
  if (mPostTremoloWasActive && !processingPlan.runTremolo)
    mTremolo.Reset();
  mPostDelayWasActive = processingPlan.runDelay;
  mPostReverbWasActive = processingPlan.runReverb;
  mPostTremoloWasActive = processingPlan.runTremolo;

  if (processingPlan.runDelay)
  {
    mDelay.SetParams(GetParam(kDelayTime)->Value(), GetParam(kDelayFeedback)->Value(), GetParam(kDelayMix)->Value(),
                     GetParam(kDelayMode)->Int(), sampleRate, GetParam(kDelayTone)->Value(),
                     GetParam(kDelayAge)->Value(), GetParam(kDelayPingPong)->Bool());
    postPointers = mDelay.Process(postPointers, numChannelsExternalOut, nFrames);
  }

  if (processingPlan.runReverb)
  {
    mReverb.SetParams(GetParam(kReverbMix)->Value(), GetParam(kReverbDecay)->Value(), GetParam(kReverbTone)->Value(),
                      GetParam(kReverbPreDelay)->Value(), GetParam(kReverbShimmer)->Value(),
                      GetParam(kReverbMode)->Int(), sampleRate, GetParam(kReverbSubMode)->Int());
    postPointers = mReverb.Process(postPointers, numChannelsExternalOut, nFrames);
  }

  // Tremolo runs LAST (after Reverb): the reverberated signal pulses too, matching
  // a vintage amp where the trem stage sits after the reverb tank. Processes in
  // place on the current POST bus, so no pointer-chain swap is needed.
  if (processingPlan.runTremolo)
  {
    // Tempo source: host transport for plugins, the app metronome for standalone
    // (where no DAW transport exists). Sync converts BPM + division to an LFO Hz.
    double tremoloBpm;
#ifdef APP_API
    tremoloBpm = static_cast<double>(mMetronomeDSP.GetBPM());
#else
    tremoloBpm = GetTempo();
#endif
    if (!(tremoloBpm > 0.0))
      tremoloBpm = 120.0;
    const bool tremoloSync = GetParam(kTremoloSync)->Bool();
    const double tremoloRateHz = tremoloSync
                                   ? volum::VoLumTremoloSyncRateHz(tremoloBpm, GetParam(kTremoloDivision)->Int())
                                   : GetParam(kTremoloRate)->Value();
    mTremolo.SetParams(tremoloRateHz, GetParam(kTremoloDepth)->Value(), GetParam(kTremoloShape)->Value(),
                       GetParam(kTremoloMix)->Value(), GetParam(kTremoloCrossover)->Value(),
                       GetParam(kTremoloMode)->Int(), sampleRate);
    mTremolo.Process(postPointers, numChannelsExternalOut, nFrames);
  }

  if (postPointers != outputs)
  {
    for (size_t c = 0; c < numChannelsExternalOut; c++)
      std::memcpy(outputs[c], postPointers[c], nFrames * sizeof(iplug::sample));
  }
}
