#pragma once

// Overlay attach / dismiss / keyboard-swallow SSOT.
// Attach order is z-order in iPlug. Full-window overlays must sit above BUILD
// chrome (preset bar, preset menu, IR menu). One list drives every caller.

#include <initializer_list>

namespace volum
{
namespace ui
{

// Human names in attach-order (lowest z first). Tests lock these strings
// against VoLumLayoutBuild.inc.cpp so a new overlay cannot sneak under the
// preset dropdown again.
inline constexpr const char* kOverlayAttachNeedles[] = {
  "AttachControl(settingsPage, kCtrlTagSettingsBox)",
  "AttachControl(pack, kCtrlTagVoLumPackOverlay)",
  "AttachControl(overlay, kCtrlTagVoLumCustomOverlay)",
  "AttachControl(new VoLumConfirmDialogControl(b), kCtrlTagVoLumConfirm)",
  "AttachControl(nameDlg, kCtrlTagVoLumNameDialog)",
  "AttachControl(tunerCtrl, kCtrlTagVoLumTuner)",
  "AttachControl(metCtrl, kCtrlTagVoLumMetronome)",
};

// BUILD chrome that must attach *before* the overlays above.
inline constexpr const char* kChromeUnderOverlayNeedles[] = {
  "kCtrlTagVoLumPresetBar)",
  "AttachControl(presetMenu, kCtrlTagVoLumPresetMenu)",
  "AttachControl(irMenu, kCtrlTagVoLumIrMenu)",
};

// Settings, the tuner and the metronome close on a mouse-down outside their
// panel. The overlay consumes that click either way, so it never also lands on
// the control underneath. Same edges as IRECT::Contains.
inline bool OutsidePanelClickCloses(float panelL, float panelT, float panelR, float panelB, float x, float y)
{
  return !(x >= panelL && x < panelR && y >= panelT && y < panelB);
}

template <typename Tags, typename IsOpen>
bool AnyTagOpen(const Tags& tags, IsOpen&& isOpen)
{
  for (int tag : tags)
  {
    if (isOpen(tag))
      return true;
  }
  return false;
}

template <typename IsOpen>
bool AnyOverlayOpen(std::initializer_list<int> tags, IsOpen&& isOpen)
{
  return AnyTagOpen(tags, isOpen);
}

} // namespace ui
} // namespace volum
