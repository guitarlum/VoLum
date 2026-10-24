Status: ready-for-agent
Blocked by: 02

# 04 Linux doctest CI job

## Scope

May write:
- `.github/workflows/ci.yml`: new job `linux-tests`
- `NeuralAmpModeler/scripts/merge-verified-branch.ps1`
- `NeuralAmpModeler/scripts/ci-watch.ps1`, only if job names need it

## Work

- **`linux-tests` job** on `ubuntu-22.04`:
  - checkout with submodules, `download-vst3-sdk.sh`, `provision-linux.sh --ci`;
  - `run-tests-linux.sh`, then `run-tests-linux.sh --clang --sanitize`.
- **`merge-verified-branch.ps1`** also requires every job whose name starts with
  `Linux` to be green, unless `-AllowWindowsOnly` is passed.

## Acceptance

- `ci-watch.ps1 -Ref <branch> -Dispatch` is green, including `Linux tests`, with
  the run id in the report.
- `merge-verified-branch.ps1 -DryRun` refuses a run whose Linux job failed. Show
  this with a run id or a mocked run object.

## Milestone

When 02, 03 and 04 are resolved, the coordinator merges `feature/linux-support`
into `dev` (M1).
