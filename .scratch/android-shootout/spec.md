# android-shootout — spec

## Destination

By the morning of 2026-10-20 the owner can pick one of three Android phone-landscape
UI/UX prototypes. Each covers the desktop app's full feature set against a shared
fake engine and carries a cold Opus approval pinned to its final commit. An engine
spike report says whether the current-dev DSP builds and matches on Android.

The owner picks. Nothing is merged to `dev`, and no candidate is declared the winner.

## Locked decisions (do not reopen)

- Kotlin + Jetpack Compose. Phone landscape only. minSdk 29, target/compile 35.
- Start fresh. The July 2026 Android PoC was deleted on purpose; do not resurrect
  or consult it (not in git history on any live branch, not on disk).
- Three candidates with different layout and interaction directions and **one
  fixed VoLum identity** (below).
- Builds and emulators run on the box (`lum@192.168.178.91`, Ubuntu 26.04 under
  WSL2, KVM, 32 threads, ~15 GB RAM shared with Ollama). Cursor, workers and
  reviewers run on the laptop.
- The UI runs against a deterministic fake engine. Wiring the real engine is not
  tonight.
- Out of scope tonight: real-phone latency/CPU numbers, real touch feel, an
  Android CI job, changelog and user-guide entries, merging to `dev`.

## Feature coverage

"Exact same feature coverage as the desktop app" means every user-facing control,
mode, dialog, list, setting, state and recovery path in `docs/user-guide.en.md`
and the desktop code gets a stable feature id in
`.scratch/android-shootout/features.md` (ticket 01), with its Android mapping.

Known Android mappings:

- ASIO/CoreAudio device + sample rate + buffer → Android audio input/output
  device (USB audio interface first), sample rate, buffer/latency mode.
- MIDI foot controllers → Android MIDI (USB and BLE), same learn/assign model.
- File import (custom amps, IRs, pedals, packs) → Storage Access Framework
  pickers; the fake engine returns fixture files.
- Microphone permission (needed for input) including denial and retry.
- No USB interface connected / interface unplugged mid-session.
- Keyboard shortcuts and DAW plugin mode → N/A (listed as such, with reason).

## VoLum identity (fixed; overrides `/frontend-design`'s "pick a new aesthetic")

Every UI worker and both reviewers read
`c:\Users\SteffenDangmann\.agents\skills\frontend-design\SKILL.md` first. Its
ambition goes into composition, motion, touch feel and finish **inside** the
VoLum look. Never a new brand, never generic Material.

- Fonts from `NeuralAmpModeler/resources/fonts/`: Poiret One (thin display:
  logo, sound names in PLAY, amp title), Josefin Sans Regular/SemiBold (labels,
  values), Michroma where the desktop uses it. Never Roboto, never the system font.
- Colours from `NeuralAmpModeler/VoLumColorHelpers.h` and `Colors.h`: raisin-black
  panels, amber/gold accents, brass, teal list ink, cyan sunset grid.
- Selection language: the `AmberPicker`, `Brass` and `ListTeal` styles of
  `DrawVoLumSelection` / `SelectionInkColor`, ported once as a Compose component.
  Every mutually exclusive control uses it.
- Surfaces: gold corner brackets, PRE diagonal crosshatch, AMP circuit-board
  grid, pedal-card art and motifs (`VoLumTriptychMotifs.h`, `VoLumPedalCardControl.h`).
- Moving art: the sunset hero, per-amp procedural art and the animator
  (`NeuralAmpModeler/art/VoLumArt*.h`, `VoLumArtAnimator.h`, `VoLumFractalArt.h`,
  `VoLumHero.h`, `VoLumStageArtCache.h`) are ported once to Compose Canvas in
  `:design`, with desktop timing and easing. Candidates use it; they do not
  reinvent it.
- References: `docs/user-guide-*.png` (desktop screenshots) and the desktop
  motion contact sheets in `.scratch/android-shootout/reference/` (ticket 01).

## Candidate directions

Same identity, different layout and interaction model:

- **A, Signal Flow** (`feature/android-ui-a`, Opus): desktop-faithful BUILD
  chain strip (PRE / AMP / POST) with a focus editor, PLAY as a second surface.
- **B, Stage Floor** (`feature/android-ui-b`, GPT-5.6): performance first. PLAY
  sounds as big tiles, BUILD as a pedalboard. Designed for a phone on a mic stand,
  hit from arm's length.
- **C, Studio Instrument** (`feature/android-ui-c`, Grok 4.7): the hero art
  dominates the screen; blocks are overlays on it, editing happens in contextual
  bottom sheets, metering is rich.

## Project shape (ticket 01 builds it; others extend only their own paths)

`NeuralAmpModeler/projects/android/`:

- `:core` — pure Kotlin. Domain model of the full feature set, reducer, the
  deterministic fake engine (meters, tuner, metronome, MIDI events, file imports,
  packs, failures), the `FeatureId` registry generated from `features.md`, and
  the screen-id + fixture registry. Unit tests per feature id.
- `:design` — VoLum identity (ticket 02 owns it).
- `:harness` — `Modifier.featureId(...)` semantics key, capture activity that
  renders any screen id in any fixture via an intent, instrumented coverage and
  touch-target tests shared by all candidates.
- `:ui-a`, `:ui-b`, `:ui-c` — one per candidate (tickets 03–05 own them).
- `:app` — one product flavor per candidate (`a`, `b`, `c`), distinct
  `applicationId` suffixes so all three install side by side.
- `engine/` — the native engine spike (ticket 06 owns it), not wired to `:app`.

## Box workflow

- Laptop worktrees come from `NeuralAmpModeler/scripts/worktree-win.ps1 -Add
  <branch> -Base feature/android-shootout -NoSubmodules`.
- Code reaches the box by `git push` to the bare repo
  `ssh://lum@192.168.178.91/home/lum/volum-android/VoLum.git`, with
  `GIT_SSH_COMMAND="ssh -i C:/Users/SteffenDangmann/.ssh/volum_a2_pc -o BatchMode=yes"`
  set in the process environment. Never change git config.
- Box layout and env: `~/volum-android/{VoLum.git,wt,locks,out,logs,env.sh}`,
  SDK `~/android-sdk`, AVD `volum_phone`. Provisioned by
  `NeuralAmpModeler/scripts/android-box/provision.sh` (idempotent).
- Locks: `flock ~/volum-android/locks/build{1,2}.lock` (at most two Gradle builds)
  and `locks/emu.lock` (one emulator user at a time). Never touch
  `~/localai-platform`, never stop Ollama.
- Smoke-tested 00:46: KVM boot in 12 s, `adb exec-out screencap` works, the
  launcher stays portrait. The app forces landscape in its manifest, and
  `Capture` fails any frame that is not 2400×1080.
- Upload box scripts with `scp` of an LF-only file
  (`[IO.File]::WriteAllText` with CRs removed). Piping a file through ssh adds a
  CR to its last line, and a remote `sed` inside a single-quoted PowerShell
  argument loses its quotes (it once deleted the first "r" of every line).
- PowerShell → ssh traps: pipe scripts to `bash -s` with CRs stripped and a
  sacrificial trailing `#` line; never inline `$` in a double-quoted ssh argument.
