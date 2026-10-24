# Linux support (1.4.0 beta)

Owner plan: `C:\Users\SteffenDangmann\.cursor\plans\volum_linux_support_loop_da771e44.plan.md`
(grilled 2026-10-24). Every decision below is locked. A worker that needs a
product call not covered here writes it to `reports/escalations.md` and stops
that ticket.

## Destination

Native Linux x86_64 VoLum:
- a VST3 with its editor;
- a standalone app on RtAudio JACK + ALSA + PulseAudio, with MIDI through RtMidi ALSA;
- X11 only (Wayland desktops via XWayland);
- packaged as a tarball.

Merged into `dev`, with the `release-native.yml` Linux leg green on a dry run
and 1.4.0-beta notes drafted. The owner triggers the release.

Windows and macOS 1.3.1 behaviour and sound stay as they are. The Windows and
macOS golden hash sets must not change.

## Locked decisions

- **Scope:** VST3 with editor and standalone. No CLAP, LV2 or aarch64.
  Build on Ubuntu 22.04 (glibc 2.35); test on 24.04 too.
- **iPlug2 fork:** branch `volum/linux` on `guitarlum/iPlug2`, off `b98aacb44`.
  Keep the Dec-2023 base; do not rebase onto upstream master.
  - Every Linux change goes in new files or `OS_LINUX` sections, so Windows and
    macOS compile exactly as before.
  - Align file and function names with upstream PR iPlug2/iPlug2#1336.
  - No upstream PR in this effort.
- **Graphics:** `IGraphicsLinux` on NanoVG GL2, the same path as Windows,
  including the heavier Josefin weight in `VoLumLayoutBuild.inc.cpp`.
- **Threading:** all X11/GL work runs on the host UI thread.
  - VST3: the `IRunLoop` from `IPlugFrame` (X connection fd handler plus a ~16 ms
    timer). Timers with the editor closed come from the `IRunLoop` obtained
    through `IPluginFactory3::setHostContext`.
  - Standalone: the app drives the same loop itself.
  - No render thread, no `XInitThreads`-based design (PR #1336's flaw).
  - Each instance makes its GL context current before every draw.
- **Arena (ticket 07):** two candidates.
  - (a) our own Xlib+GLX core;
  - (b) vendored pugl X11 backend (ISC) plus an adapter.
  - Both reuse leaf code from #1336: keysym table, GLX setup, clipboard,
    fontconfig, XDG/`dladdr` paths, the SWELL-GDK app.
- **Dialogs:**
  - File pickers run out of process: xdg-desktop-portal via `gdbus`, then
    `zenity`, then `kdialog`.
  - Menus and text entry are drawn by IGraphics.
  - The `VOLUM_*` env hooks in `VoLumHarnessEnv.h` replace the pickers in tests.
- **Paths:**
  - Library, settings, dual-amp settings, update state and log live under
    `$XDG_DATA_HOME/VoLum` (default `~/.local/share/VoLum`), same layout as
    Windows.
  - Rigs search order: next to the binary, then `<bundle>.vst3/Contents/Resources/VoLumRigs`,
    then `~/.local/share/VoLum/VoLumRigs`, then `/usr/share/VoLum/VoLumRigs`.
- **Update check:** libcurl loaded with `dlopen`. If it is missing, the check is
  disabled and nothing crashes.
- **Build:**
  - New VoLum-owned CMake for the Linux app and VST3. It reuses the source lists
    of `NeuralAmpModeler/tests/CMakeLists.txt` where it can.
  - `.sln`, `.vcxproj` and the Xcode projects are untouched.
- **Portability:**
  - Static libstdc++ and libgcc.
  - JACK, Pulse and curl are loaded at runtime (`dlopen` or weak link); ALSA,
    X11 and GL are linked.
  - The VST3 bundle layout is `VoLum.vst3/Contents/x86_64-linux/VoLum.so` plus
    `Contents/Resources`.
- **Golden DSP:** a Linux-specific expected hash set is pre-authorized. It is
  gated by a null test against Windows renders (residual at or below -100 dBFS
  peak, unless an escalation sets otherwise).
  - `test_golden_dsp.cpp` `PlatformExpected` gains a Linux branch.
  - Never edit the Windows or macOS sets.
- **Package:** `VoLum-<version>-linux-x86_64.tar.gz` containing `VoLum.vst3/`,
  `VoLum` (standalone), `VoLumRigs/`, `install.sh`, `LICENSE`,
  `THIRD_PARTY_LICENSES.md`.
  - `install.sh` installs to `~/.vst3`, `~/.local/bin`, `~/.local/share/VoLum`
    and a `.desktop` file.
- **Gates (all headless under `xvfb-run`):**
  - Steinberg `validator` (built from the vendored SDK).
  - pluginval at strictness 10.
  - REAPER for Linux, portable tarball plus a startup Lua script: load, open
    editor, render and null against a reference, two instances, close.
  - `carla-single` as a second host.
  - Standalone audio on `jackd -d dummy -r 48000 -p 256`.
  - Real device, box only: standalone on RtAudio Pulse, then WSLg, then Windows,
    then the RME Babyface Pro FS (Windows default device, TotalMix loopback on
    the main outs). Record it back, null-test it, and count xruns.
  - Real-Linux-kernel latency is left to beta users.
- **UI acceptance:**
  - Xvfb screenshots of fixed scenes, compared with Windows GL2 references using
    a perceptual-diff threshold (ImageMagick `compare -metric SSIM` or
    equivalent; threshold set in the ticket).
  - A model family different from the implementer's does a cold visual review.
  - Then commit the Linux reference set. HiDPI at scale 1 and 2.
- **E2E:** `NeuralAmpModeler/scripts/e2e-standalone-linux.sh` (Xvfb + xdotool +
  an ALSA virtual MIDI port via `snd-virmidi` or `aconnect`, or a JACK MIDI
  fallback). Scenarios:
  - launch with a fresh library;
  - amp and preset switch;
  - custom NAM import and pack via the env hooks;
  - MIDI recall;
  - Preferences dialog;
  - clean shutdown.
- **CI:** use GitHub Actions as much as possible.
  - New `ci.yml` jobs on `ubuntu-22.04`: doctest, plugin gates, e2e, package.
  - `merge-verified-branch.ps1` must also require the Linux jobs.
- **Merge:** milestone merges of `feature/linux-support` into `dev` via
  `merge-verified-branch.ps1` (all Windows, macOS and Linux jobs green). Never `main`.

## Machines

- **Box:** `lum@192.168.178.91`, the owner's desktop running WSL2 Ubuntu 26.04.1 (GCC 15.2,
  Clang 21, CMake 4.2; release builds still come from `ubuntu-22.04` CI).
  32 threads, ~23 GB, passwordless sudo, WSLg PulseAudio bridge, Babyface
  attached on the Windows side.
  - SSH: `ssh -i C:/Users/SteffenDangmann/.ssh/volum_a2_pc -o BatchMode=yes`.
  - Layout: `~/volum-linux/{VoLum,wt,locks,out,logs}`. `VoLum` is a GitHub
    clone; `wt/<branch>` are its worktrees.
  - Code reaches the box only by `git push` to GitHub, then fetch on the box.
  - At most 2 concurrent builds: `flock ~/volum-linux/locks/build{1,2}.lock`.
    One display/audio user at a time: `locks/gui.lock`.
  - **localai-platform guard:** before any box job, `localai-guard.sh` must pass
    (no `speed_matrix`, `bench/run`, `fetch_models`, `build_llama` or pytest
    processes, and `llama-server` under 10% of a core over 5 s).
    - If it fails, the job exits 3 (`BOX_BUSY_LOCALAI`). The ticket defers box
      work and continues with cloud and CI.
    - Never stop, kill or reconfigure anything of `~/localai-platform`,
      `llama-server`, Ollama or docker.
  - PowerShell to ssh traps: pipe scripts to `bash -s` with CRs stripped and a
    sacrificial trailing `#` line. Never inline `$` in a double-quoted ssh
    argument. Upload files with `scp` of LF-only content.
- **Cloud agents** (Cursor, Linux VMs): build-only work, arena candidates, and
  CI-like Xvfb gates. They clone `guitarlum/VoLum` with submodules; the fork
  branch `volum/linux` is on GitHub.
- **Local WSL:** optional and no sudo. Doctest only if its deps are already there.
- **Laptop:** Windows tests (`run-tests-win.ps1`), `ci-watch.ps1`, merges,
  coordinator.

## How the loop runs (pstack drives, conductor standing orders apply)

- The coordinator (`/loop` wake) runs `conductor-status.ps1 -Effort linux-support`
  first, then picks frontier tickets: `ready-for-agent`, every `Blocked by`
  resolved, lowest number first.
- **Disjoint tickets** run as a swarm: parallel Task workers in one message, each
  on its own branch `feature/linux-<NN>-<slug>` off `feature/linux-support` and
  its own path set. They merge back into `feature/linux-support` when verified.
- **Design forks** (ticket 07) run as an arena:
  1. Two runners (Claude Opus 5.5 and Grok 4.7) get the same brief, one per
     candidate.
  2. Cross-family judges score a 3 to 6 criterion rubric written before spawning.
  3. Pick a base, graft the best parts of the other, verify.
- **Cold verifier per ticket:**
  - Read-only reviewers on the ticket's diff plus the gate output: one Opus, one
    Grok.
  - Findings are bucketed: act on, consider, noted, dismissed.
  - Only "act on" items block. The writer never reviews its own work.
- **Workers own CI:** push, `ci-watch.ps1 -Ref <branch> -Dispatch`, report a green
  run id.
- **Escalation never blocks the loop:** write to `reports/escalations.md`, mark
  the ticket `Status: escalated`, continue the other tickets.
  - Stop the whole loop only for irreversible git (force-push, hard reset of a
    shared branch), box disk under 50 GB, or a fix that would change Windows or
    macOS sound or behaviour.
- **Models:** from `~/.cursor/rules/pstack-models.mdc` (budget medium). Swarm
  workers use Grok 4.7; the hardest tasks (07, 10) use Opus 5.5. Reviewers are
  Opus 5.5 and Grok 4.7.
