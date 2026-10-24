Status: ready-for-agent
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

## Acceptance

- Every scene is under the threshold, and the cold reviewer's verdict is
  `accepted`.
- The gate fails on a deliberately shifted control.
- It is wired into a CI job or `verify-linux.sh`.
