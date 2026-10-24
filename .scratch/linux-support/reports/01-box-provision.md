# 01 Box provision

PASS. Branch `feature/linux-01-box`. The coordinator merges; this ticket is not marked resolved.

## Box

- `lum@192.168.178.91`, WSL2, 32 threads, 23 GiB RAM.
- The running system is Ubuntu 26.04.1 LTS (resolute). `spec.md` called it 24.04. `provision-linux.sh` accepts 22.04, 24.04 and 26.04.
- About 393 GiB free after install (was about 395 GiB).
- localai-platform was not stopped or reconfigured. Same pids before and after: `proxy.py` 140, `dockerd` 322, `llama-server` 2946.

## What landed

- `NeuralAmpModeler/scripts/linux/provision-linux.sh`
- `NeuralAmpModeler/scripts/linux/localai-guard.sh`
- `NeuralAmpModeler/scripts/linux/box.ps1`
- Layout `~/volum-linux/{VoLum,wt,locks,out,logs,tools}`. `VoLum` is a plain https clone. Worktrees are `wt/<branch-with-slashes-replaced-by-hyphens>`.
- Root `.gitattributes` already has `*.sh text eol=lf`. `git check-attr eol` confirms the new scripts. No new rule.

## Installed versions

| Tool | Version |
| --- | --- |
| gcc / g++ | 15.2.0 (Ubuntu 15.2.0-16ubuntu1) |
| clang / lld | 21.1.8 |
| cmake | 4.2.3 |
| ninja | 1.13.2 |
| pkg-config | 2.5.1 |
| python3 / pip3 | 3.14.4 / 25.1.1 |
| jackd | jackdmp 1.9.22 |
| xvfb | 2:21.1.22-1ubuntu1.2 |
| xdotool | 3.20160805.1 (1:3.20160805.1-5.1build1) |
| x11-apps (xdpyinfo) | 7.7+11build4 |
| pluginval | 1.0.4 at `~/volum-linux/tools/bin/pluginval` |
| REAPER | 7.82 at `~/volum-linux/tools/bin/reaper` |
| Carla | Ubuntu `carla` 2.5.10-0ubuntu4, `/usr/bin/carla-single` |

Pins in `provision-linux.sh` (sha256):

- pluginval 1.0.4 `c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352`
- REAPER 7.82 `a4aa961534f22ae5ae910e4ec0c200dcb4476c9f7556806fe09a9743cd582563`
- Carla 22.04 `2.4.2-0ubuntu2` `9d69f9bfad29a6a69392bee43df1dd313b0bdb93301aacac35beb3e3ae233eed`
- Carla 24.04 `2.5.8-0ubuntu5` `68cf6eae0af54a8953f46f6fa01fc2c946eaf141ca36397b933f6c38628e532d`
- Carla 26.04 `2.5.10-0ubuntu4` `e0cb6b11558412e11f774e00f6416ce08d7c4688122322a0cb60042fd56e5582`

Carla's GitHub releases have no Linux tarball. The Ubuntu package is the unattended install and it ships `carla-single`. `--ci` installs the apt toolchain and test packages (including xvfb, xdotool, jackd2) and skips pluginval, REAPER and Carla.

## Acceptance

`box.ps1 -Branch feature/linux-support -Sha 17eb3c83097b8c999457dd1453e90aea2720610e -Command "gcc --version && xvfb-run -a xdpyinfo | head -3"`

pwsh exit 0, and the same command from `powershell.exe` exit 0. Worktree `~/volum-linux/wt/feature-linux-support` at `17eb3c83097b8c999457dd1453e90aea2720610e` (iPlug2 submodule `b98aacb44`). Output:

```
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
name of display:    :99
version number:    11.0
vendor string:    The X.Org Foundation
```

`localai-guard.sh` while `bash -c 'exec -a speed_matrix_fake sleep 600'` was running:

```
BOX_BUSY_LOCALAI process matches busy pattern: speed_matrix_fake 600  sleep
```

Exit 3. After that pid was killed, exit 0 (llama-server stayed under the 10% line: 0 ticks over 5 s, CLK_TCK 100). `box.ps1` from PowerShell 5 also exited 3 on that fake process and did not run the command. The fake pid was the only process killed.

Second `provision-linux.sh`: log said apt, clone, pluginval, REAPER and Carla were already present. `find` mtimes under `~/volum-linux` and `dpkg-query -W` were identical to the pre-run snapshot (both diffs empty).

## Other checks

- Both build locks held by short `flock sleep` processes: `box.ps1` logged `both build locks held; waiting for build1`, then `LOCK_ACQUIRED`, exit 0.
- `box.ps1 -Command "exit 7"` returned 7. Immediately after, `flock -n` acquired `build1.lock` and `build2.lock`.
- Unreachable ssh: a `ssh.cmd` shim that exits 255 made `box.ps1` print `ssh unreachable: lum@192.168.178.91 (exit 255)` and exit 2. The command did not run.
- `--ci` on the already provisioned box exited 0 and skipped the three tool downloads.

## Traps

- Pipe remote scripts to `ssh … 'bash -s'` with CRs stripped and a trailing `#` line. A `$` in `-Command` is base64'd so it is never inlined in a double-quoted ssh argument. Exit codes 7 and 9 survived that pipe before the real runs.
- Do not feed `localai-guard.sh` to `bash -c`. The script body contains the busy patterns, so the shell's own command line would look busy. `bash -s` or `bash localai-guard.sh` is safe.
- Lock files are opened append-only. Truncating them with `>` is unnecessary and races a holder.
- pwsh can turn a native non-zero status into a terminating error. `box.ps1` turns that preference off so exit 3 stays an exit code.
- `carla-single -h` tries to open a JACK client and fails when jackd is not running (`Failed to init temporary jack client`). The package is installed; nothing was left running.
