#pragma once

#include <array>
#include <cstring>

// VoLum: canonical list of parameters whose DSP effect is CACHED in a plugin
// member (gain multipliers, tone-stack filter coefficients) and is therefore
// only recomputed inside NeuralAmpModeler::OnParamChange.
//
// Programmatic restores (preset recall F5, amp switch, standalone session
// restore, DAW chunk restore) push parameter values through
// SendParameterValueFromDelegate, which updates the IParam store + UI but does
// NOT invoke OnParamChange. Without an explicit re-apply the cached values stay
// stale -- the classic symptom is OUTPUT recalled from -inf showing 0 dB on the
// knob while audio stays silent until the knob is nudged.
//
// _VolumApplyDspCaches() must re-apply exactly this set. This header keeps the
// set in one auditable place so a unit test can pin it: adding a new cached
// param is then a conscious act (extend OnParamChange, _VolumApplyDspCaches, and
// this list together).
namespace volum
{
namespace dsp_cache
{

// Stable GetName() strings of the cached params re-applied on every restore.
// Order is documentation only; the test asserts set membership + count.
inline constexpr std::array<const char*, 9> kRestoreReappliedCaches = {
  "Input",      // kInputLevel       -> mInputGain via _SetInputGain
  "Output",     // kOutputLevel      -> mOutputGain via _SetOutputGain
  "SupportOutput", // kSupportOutputLevel -> mSupportOutputGain via _SetSupportOutputGain
  "Bass",       // kToneBass         -> mToneStack "bass"
  "Middle",     // kToneMid          -> mToneStack "middle"
  "Treble",     // kToneTreble       -> mToneStack "treble"
  "SupportBass",   // kSupportToneBass   -> mSupportToneStack "bass"
  "SupportMiddle", // kSupportToneMid    -> mSupportToneStack "middle"
  "SupportTreble", // kSupportToneTreble -> mSupportToneStack "treble"
};

inline bool IsRestoreReappliedCache(const char* paramName)
{
  if (paramName == nullptr)
    return false;
  for (const char* n : kRestoreReappliedCaches)
    if (std::strcmp(n, paramName) == 0)
      return true;
  return false;
}

} // namespace dsp_cache
} // namespace volum
