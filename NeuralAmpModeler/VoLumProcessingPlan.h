#pragma once

namespace volum
{

struct ProcessingPlan
{
  bool runPreComp = false;
  bool runPreNam[2]{false, false};
  bool runNoiseGate = false;
  bool runMainModel = false;
  bool runFallback = true;
  bool runToneStack = false;
  bool runIR = false;
  bool runDelay = false;
  bool runReverb = false;
  bool mixMetronome = true;
  bool silenceForTuner = false;
};

inline ProcessingPlan MakeProcessingPlan(bool haveMainModel, bool noiseGateActive, bool toneStackActive, bool irActive,
                                         bool haveIR, bool preCompActive, const bool preNamActive[2],
                                         const bool havePreNam[2], bool delayActive, bool reverbActive,
                                         bool tunerActive)
{
  ProcessingPlan plan;
  plan.runPreComp = preCompActive;
  plan.runPreNam[0] = preNamActive[0] && havePreNam[0];
  plan.runPreNam[1] = preNamActive[1] && havePreNam[1];
  plan.runNoiseGate = noiseGateActive;
  plan.runMainModel = haveMainModel;
  plan.runFallback = !haveMainModel;
  plan.runToneStack = haveMainModel && toneStackActive;
  plan.runIR = haveMainModel && irActive && haveIR;
  plan.runDelay = haveMainModel && delayActive;
  plan.runReverb = haveMainModel && reverbActive;
  plan.silenceForTuner = tunerActive;
  return plan;
}

} // namespace volum
