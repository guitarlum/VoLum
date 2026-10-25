# Escalations (owner)

## E1 2026-10-24 05:20: cloud agents cannot start

`Task environment=cloud` failed with: "The Cursor app needs to be installed on
your repository."

- **Needed from the owner:** install the Cursor GitHub app on `guitarlum/VoLum`
  and `guitarlum/iPlug2`, plus the other submodule forks `guitarlum/NeuralAmpModelerCore`
  and `guitarlum/AudioDSPTools`.
- **Until then:** every worker runs locally. Builds and gates go through the box
  (`scripts/linux/box.ps1`, 2 build locks) and GitHub Actions.
  - The ticket 07 arena runs both candidates on the box, one per build lock.
- Nothing is blocked; the loop is only slower.
- **Closed 2026-10-24 10:35:** the owner declined. Workers stay local (the laptop,
  plus builds on the box and in GitHub Actions) for the whole effort. Do not
  dispatch cloud agents or ask again.

## E2 2026-10-25 14:10: macOS pitch-burst timing check flaked once (pre-existing, not Linux)

`test_volum_pitch_burst.cpp:535` (`CHECK(ratio <= 0.35)`, live burst median over
reference median) failed once on the hosted macOS runner in CI 40187833567
(ratio 0.381); the rerun of the failed jobs on the same run passed. The test and
its threshold predate this effort and were not touched.

- **Needed from the owner:** nothing now. If it recurs on `dev`, decide whether
  the macOS runner should print the ratio instead of failing, the way the arm64
  arm of the 0.5 check already does.
- The loop does not widen the threshold (standing order).