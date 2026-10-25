Status: resolved
Blocked by: 09, 10

# 13 UI acceptance on Linux

## Scope

May write:
- `NeuralAmpModeler/scripts/linux/ui-scenes.sh`
- `NeuralAmpModeler/tests/linux-ui-refs/**` (new)
- `docs/screenshot-recipes.md`, adding a Linux section

## Work

- **Scenes:** main window default rig, Settings SIGNAL and SYSTEM, custom amp
  overlay, pack dialog, tuner, at scale 1 and scale 2.
- **Windows references:** capture from the Windows GL2 standalone with the
  existing `win-screenshot.ps1` recipes on the laptop, using the same sandbox
  library.
- **Comparison:** an SSIM or perceptual diff per scene, with a threshold that
  allows for AA and font differences.
- **Review:** a cold visual reviewer from a different model family than the
  implementer compares them side by side and lists defects: clipping, missing
  glyphs, wrong weights, misplaced controls, scale bugs.
- Fix the defects, then commit the Linux reference set and make `ui-scenes.sh`
  a regression gate against it.

## Carried from ticket 07 (coordinator, 2026-10-24)

- **Unhandled keys reach the host:** 07 forwards keys VoLum ignores to the
  parent window with `XSendEvent`, but headless REAPER never starts playback,
  so the forwarded space bar was never observed. Prove it in a host that
  reacts to a forwarded key under Xvfb (Carla, or the standalone with a test
  hook), and show it fails with forwarding removed.

## Carried from ticket 09 (coordinator, 2026-10-25)

- **Text entry and popup menus in a host:** on Linux these are still the
  in-graphics IGraphics controls (`IGraphicsLinux.cpp` `CreatePlatformTextEntry`
  / `CreatePlatformPopupMenu`), and no host run has typed into a field or
  picked a menu item. In REAPER under Xvfb, type a name into a text field (for
  example a custom amp name) and commit it, and pick a VoLum popup menu item
  that changes state; check the result in the saved settings. Each check fails
  when its feature is broken on purpose.

## Acceptance

- Every scene is under the threshold, and the cold reviewer's verdict is
  `accepted`.
- The gate fails on a deliberately shifted control.
- It is wired into a CI job or `verify-linux.sh`.
