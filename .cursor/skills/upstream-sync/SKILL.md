---
name: upstream-sync
description: Sync VoLum with upstream NAM sources. Use when bumping NeuralAmpModelerCore, cherry-picking from NeuralAmpModelerPlugin, comparing upstream-equivalent files, or discussing upstream sync strategy.
---

# Upstream Sync

VoLum is independent, but stays close enough to two upstream sources that fixes and features can be cherry-picked instead of reinvented.

## NeuralAmpModelerCore

- `NeuralAmpModelerCore/` is the NAM model runtime submodule (wavenet / lstm / convnet / ResamplingContainer).
- Sync with `git submodule update --remote NeuralAmpModelerCore`.
- Then run the Windows tests (`AGENTS.md` "Fast Commands"); make sure `test_nam_rigs.cpp` loads every bundled `.nam` and runs one finite, bounded `process()` block.
- Bundled main amps live in `rigs/`; PRE captures live in `rigs/PrePedals/`. Both must keep loading across submodule upgrades.
- If any rig fails to load or emits non-finite samples, pin the submodule back and open an upstream issue.

## NeuralAmpModelerPlugin

- The upstream plugin shell remote is `upstream` at `https://github.com/sdatkinson/NeuralAmpModelerPlugin.git` (push disabled).
- Inspect with `git fetch upstream main`, then `git log --oneline VoLum-last-upstream-merge..upstream/main -- NeuralAmpModeler`.
- Cherry-pick or merge selectively into a `chore/upstream-sync-YYYY-MM-DD` branch. Never blanket-merge upstream.
- Tag the merge point in `NeuralAmpModeler/installer/changelog.txt` so the next sync knows where to start.

## File Boundaries

Upstream-equivalent files still track `NeuralAmpModelerPlugin` shape:

- `NeuralAmpModeler.cpp`
- `NeuralAmpModeler.h`
- `NeuralAmpModelerControls.h`
- `ToneStack.cpp`, `ToneStack.h`
- `config.h`
- `Colors.h`
- `choc_DisableAllWarnings.h`, `choc_ReenableAllWarnings.h`
- `Unserialization.cpp`
- `projects/*.vcxproj*`
- `projects/NeuralAmpModeler-macOS.xcodeproj/`
- `projects/NeuralAmpModeler-iOS.xcodeproj/`

VoLum-only files almost never need upstream input:

- Every `VoLum*.h`, `VoLum*.cpp`, and `VoLum*.inc.cpp` in `NeuralAmpModeler/`.
- `rigs/`, `docs/`, `format.bash`, `.cursor/`, `AGENTS.md`, `THIRD_PARTY_LICENSES.md`, `installer/changelog.txt`, `installer/license.rtf`.
- `AudioDSPTools/` lives on the `guitarlum/AudioDSPTools` fork (`effect-staging`); upstream is `sdatkinson/AudioDSPTools` and follows the same selective-cherry-pick rule.

## Refactor Rules

- Do not move `NeuralAmpModeler.cpp`, `NeuralAmpModeler.h`, `Unserialization.cpp`, `ToneStack.{h,cpp}`, `NeuralAmpModelerControls.h`, or `config.h` out of `NeuralAmpModeler/`. Their paths must match upstream so cherry-picks apply without manual fixup.
- New VoLum source files always start with the `VoLum` prefix. The `// VoLum:`
  comment fence for upstream-equivalent files is in `neural-amp-modeler-native.mdc`,
  which auto-attaches on the files this skill edits.
