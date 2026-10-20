// This translation unit is the Android portability fence for the desktop audio
// chain. An include added here has to remain free of iPlug2 and platform UI APIs.

#include "Delay.h"
#include "ImpulseResponse.h"
#include "NoiseGate.h"
#include "RecursiveLinearFilter.h"
#include "Reverb.h"
#include "dsp.h"
#include "wav.h"

#include "ToneStack.h"
#include "VoLumChorus.h"
#include "VoLumDspCacheParams.h"
#include "VoLumDspStaging.h"
#include "VoLumDualAmpPlan.h"
#include "VoLumHostRestoreGate.h"
#include "VoLumIrShapingDsp.h"
#include "VoLumLatencyReport.h"
#include "VoLumLatencyRequests.h"
#include "VoLumLatencySnapshot.h"
#include "VoLumLevelMute.h"
#include "VoLumMasterSafety.h"
#include "VoLumMetronomeDSP.h"
#include "VoLumNanGuard.h"
#include "VoLumOutputMode.h"
#include "VoLumPreEffects.h"
#include "VoLumProcessIO.h"
#include "VoLumProcessingPlan.h"
#include "VoLumPitchShifter.h"
#include "VoLumResetExclusion.h"
#include "VoLumTremolo.h"
#include "VoLumTunerDSP.h"

#include "activations.h"
#include "dsp.h"
#include "get_dsp.h"
#include "slimmable.h"
#include "wavenet/a2_fast.h"

namespace volum::android::engine
{

bool CompileSurfaceProbe()
{
  const bool preNam[2] = {false, false};
  const bool havePreNam[2] = {false, false};
  const auto plan = MakeProcessingPlan(
    true, false, false, false, false, false, preNam, havePreNam, false, false, false);
  return plan.runMainModel && !plan.runFallback && SoftSafetyClip(0.0) == 0.0;
}

} // namespace volum::android::engine
