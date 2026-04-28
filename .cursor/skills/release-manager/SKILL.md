---
name: release-manager
description: Prepare VoLum PRs and GitHub releases. Use when creating release PRs, resolving release merge conflicts, editing GitHub release notes, checking artifacts, or discussing main/dev branch release flow.
---

# Release Manager

## Workflow

1. Check branch and dirt first:
   - `git status --short --branch`
   - Keep unrelated local dirt, especially `iPlug2`, out of commits.
   - Branching: `main` = released, `dev` = integration. After any `main` release, merge `main` -> `dev` and push. New work branches off `dev` (`feature/<topic>`); merge feature -> `dev` when verified; promote `dev` -> `main` only as part of a release.
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
5. Before finish:
   - Report PR/release URL.
   - Report CI state if available.
   - Mention any skipped local tests.

## Useful Commands

- `gh pr view <n> --json mergeable,mergeStateStatus,statusCheckRollup,url`
- `gh release view <tag> --json name,body,assets,isDraft,url`
- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
