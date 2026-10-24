Status: ready-for-agent
Blocked by: (none)

# 01 Box and runner provisioning

## Scope

May write: `NeuralAmpModeler/scripts/linux/**`, the box `~/volum-linux/**`.

## Work

- **`provision-linux.sh`** (idempotent, apt, Ubuntu 22.04 and 24.04):
  - toolchain: gcc, g++, clang, lld, cmake, ninja, pkg-config;
  - libraries: X11/Xcursor/Xrandr/Xext/Xi dev, libgl-dev, mesa-utils, libasound2-dev,
    libjack-jackd2-dev, jackd2, libpulse-dev, libfontconfig-dev, libfreetype-dev,
    libgtk-3-dev (SWELL-GDK), libcurl4-openssl-dev;
  - tools: xvfb, xdotool, x11-apps, imagemagick, alsa-utils, zenity, python3-pip.
  - It also fetches pluginval (Linux zip), REAPER for Linux (portable tarball) and
    Carla into `~/volum-linux/tools`. Pin versions and checksums.
  - The same script is used by CI jobs and cloud agents (`--ci` skips the tools
    that need no display).
- **`localai-guard.sh`:** exit 0 when the box is free, exit 3 with `BOX_BUSY_LOCALAI <reason>`
  otherwise (rules in spec.md, Machines).
- **`box.ps1`** (laptop side), which:
  - runs `localai-guard.sh`;
  - fetches `<branch>` from GitHub into `~/volum-linux/VoLum`;
  - creates or updates `~/volum-linux/wt/<branch>` at `<sha>` with submodules;
  - takes a build lock and runs `<command>` there;
  - streams its output and returns its exit code.
  - It follows the ssh/PowerShell traps in spec.md.
- Run provisioning on the box once and record installed versions in
  `reports/01-box-provision.md`.

## Acceptance

- `box.ps1 -Branch feature/linux-support -Sha <head> -Command "gcc --version && xvfb-run -a xdpyinfo | head -3"`
  exits 0 from the laptop.
- `localai-guard.sh` returns 3 while a fake `sleep 600 # speed_matrix` runs, and 0 after.
- A second `provision-linux.sh` run changes nothing.
