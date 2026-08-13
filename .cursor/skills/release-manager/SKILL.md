---
name: release-manager
description: Prepare VoLum PRs and GitHub releases. Use when creating release PRs, resolving release merge conflicts, editing GitHub release notes, checking artifacts, or discussing main/dev branch release flow.
---

# Release Manager

## Workflow

1. Check branch and dirt first:
   - `git status --short --branch`
   - Keep unrelated local dirt, especially `iPlug2`, out of commits.
   - Follow the branch flow in `vo-lum-workflow.mdc` "Branching"; promote `dev` -> `main` only as part of a release.
2. Identify workflow type:
   - PR to `main`: inspect full `origin/main...HEAD` diff, not only latest commit.
   - Draft release: inspect actual release assets with `gh release view`.
   - Native release: remember `Release Native` checks out `main`, creates a tag, then builds from that tag.
3. Version sanity:
   - Confirm `config.h`, `installer/VoLum.iss`, and `resources/*.plist` agree.
   - If mismatched, use existing version scripts instead of hand editing when possible.
4. User-facing text:
   - Preserve README images/badges unless user explicitly asks to change them.
   - Release notes should mention concrete asset names and install path choices.
   - **Keep release notes short, and scale them to the release.** A single
     bugfix gets a few sentences: what is fixed, who is affected, what did not
     change. No engineering narrative, no test-strategy or process sections, no
     recap of how the bug was found - that belongs in the PR and changelog.
5. CI on a release branch:
   - Pushing `release/*` triggers nothing; CI auto-runs only on `dev`/`main`.
   - `pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -Ref release/x.y.z -Dispatch`
     starts a run and follows it to green, naming the failing step if it is not.
   - Artifacts for manual testing: `VoLum-win` (installer + portable zip + pdbs)
     and `VoLum-mac`; fetch with `gh run download <id> -n VoLum-win -D <dir>`.
6. Before finish:
   - Report PR/release URL.
   - Report CI state if available.
   - Mention any skipped local tests.

## Useful Commands

- `gh pr view <n> --json mergeable,mergeStateStatus,statusCheckRollup,url`
- `gh release view <tag> --json name,body,assets,isDraft,url`
- Tests/build/package commands: `AGENTS.md` "Fast Commands".
