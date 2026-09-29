#pragma once

namespace volum
{

struct ProcessingPlan
{
  bool runPrePitch = false;
  bool runPreComp = false;
  bool runPreNam[2]{false, false};
  bool runNoiseGate = false;
  bool runMainModel = false;
  bool runSupportModel = false;
  bool runDualAmp = false;
  bool runFallback = true;
  bool runToneStack = false;
  bool runSupportToneStack = false;
  bool runIR = false;
  bool runSupportIR = false;
  bool runChorus = false;
  bool runDelay = false;
  bool runReverb = false;
  bool runTremolo = false;
  bool silenceForTuner = false;
};

inline ProcessingPlan MakeProcessingPlan(bool haveMainModel, bool noiseGateActive, bool toneStackActive, bool irActive,
                                         bool haveIR, bool preCompActive, const bool preNamActive[2],
                                         const bool havePreNam[2], bool delayActive, bool reverbActive,
                                         bool tunerActive, bool dualAmpActive = false,
                                         bool haveSupportModel = false, bool supportToneStackActive = false,
                                         bool supportIrActive = false, bool haveSupportIR = false,
                                         bool prePitchActive = false, bool tremoloActive = false,
                                         bool chorusActive = false)
{
  ProcessingPlan plan;
  plan.runPrePitch = prePitchActive;
  plan.runPreComp = preCompActive;
  plan.runPreNam[0] = preNamActive[0] && havePreNam[0];
  plan.runPreNam[1] = preNamActive[1] && havePreNam[1];
  plan.runNoiseGate = noiseGateActive;
  plan.runMainModel = haveMainModel;
  plan.runSupportModel = dualAmpActive && haveSupportModel;
  plan.runDualAmp = haveMainModel && plan.runSupportModel;
  plan.runFallback = !haveMainModel;
  plan.runToneStack = haveMainModel && toneStackActive;
  plan.runSupportToneStack = plan.runSupportModel && supportToneStackActive;
  plan.runIR = haveMainModel && irActive && haveIR;
  plan.runSupportIR = plan.runSupportModel && supportIrActive && haveSupportIR;
  plan.runChorus = (haveMainModel || plan.runSupportModel) && chorusActive;
  plan.runDelay = (haveMainModel || plan.runSupportModel) && delayActive;
  plan.runReverb = (haveMainModel || plan.runSupportModel) && reverbActive;
  plan.runTremolo = (haveMainModel || plan.runSupportModel) && tremoloActive;
  plan.silenceForTuner = tunerActive;
  return plan;
}

} // namespace volum
