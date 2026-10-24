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
