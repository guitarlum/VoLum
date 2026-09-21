#pragma once

// One 46 px plate, one 26 px ink band centered in it. Preset name stays in
// the middle. PLAY/BUILD sits with tuner / metronome / gear on the right rail.

namespace volum
{

inline constexpr float kHeaderPlateH = 46.f;
inline constexpr float kHeaderInkH = 26.f;
// Derived, never typed by hand: an uneven pad is what made the toggle and tools
// read as sitting on the hairline while the wordmark looked centered.
inline constexpr float kHeaderInkPad = (kHeaderPlateH - kHeaderInkH) * 0.5f;
inline constexpr float kHeaderRail = 18.f;
inline constexpr float kHeaderToggleW = 44.f;
inline constexpr float kHeaderTool = 26.f;
inline constexpr float kHeaderToolGap = 13.f;
inline constexpr float kHeaderPresetW = 240.f;
inline constexpr float kHeaderCabGap = 10.f;

struct HeaderChrome
{
  float plateL = 0.f;
  float plateR = 0.f;
  float plateT = 0.f;
  float plateB = 0.f;
  float inkT = 0.f;
  float inkB = 0.f;
  float toggleL = 0.f;
  float toggleR = 0.f;
  float presetL = 0.f;
  float presetR = 0.f;
  float tunerL = 0.f;
  float tunerR = 0.f;
  float metroL = 0.f;
  float metroR = 0.f;
  float gearL = 0.f;
  float gearR = 0.f;
  float cabBandT = 0.f;
};

inline HeaderChrome LayoutHeaderChrome(float mainL, float mainR, float windowT)
{
  HeaderChrome h;
  h.plateL = mainL;
  h.plateR = mainR;
  h.plateT = windowT;
  h.plateB = windowT + kHeaderPlateH;
  h.inkT = windowT + kHeaderInkPad;
  h.inkB = h.inkT + kHeaderInkH;

  const float railR = mainR - kHeaderRail;

  const float mid = (mainL + mainR) * 0.5f;
  h.presetL = mid - kHeaderPresetW * 0.5f;
  h.presetR = mid + kHeaderPresetW * 0.5f;

  h.gearR = railR;
  h.gearL = h.gearR - kHeaderTool;
  h.metroR = h.gearL - kHeaderToolGap;
  h.metroL = h.metroR - kHeaderTool;
  h.tunerR = h.metroL - kHeaderToolGap;
  h.tunerL = h.tunerR - kHeaderTool;
  h.toggleR = h.tunerL - kHeaderToolGap;
  h.toggleL = h.toggleR - kHeaderToggleW;

  h.cabBandT = h.plateB + kHeaderCabGap;
  return h;
}

// Disabled controls may show a tooltip; they must not take clicks. The layout
// used to grant mouse-events-when-disabled to every control, which is how the
// greyed Input calibration switch still toggled on factory rigs.
struct DisabledPointerPolicy
{
  static constexpr bool kMouseOverWhenDisabled = true;
  static constexpr bool kMouseEventsWhenDisabled = false;
};

// Tuner mute and metronome click are owned by editor chrome. Closing the
// editor without stopping both left a VST3 instance clicking in the dark with
// a rebuilt toolbar that drew the button off. One helper, both DSP members.
template <typename Tuner, typename Metro>
inline void HaltEditorOwnedOverlayDsp(Tuner& tuner, Metro& metro)
{
  tuner.SetActive(false);
  metro.SetActive(false);
}

} // namespace volum
