# 13 UI acceptance on Linux

Status: **review pending**. Scenes are captured. The cold visual review has not been run (the implementer is Grok; an Opus reviewer has to do it). Linux reference PNGs are not committed yet: they wait until that review accepts the pictures.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Superproject | `feature/linux-13-ui` | `8c3545e6` |
| iPlug2 | `volum/linux-13` | `43813bc38` |

Sabotage switches are gone from the plug-in. `git diff fdc226e5` on `VoLumLayoutBuild.inc.cpp` is the Josefin weight line, and `VoLumCustomOverlay.h` is unchanged. Red proofs edit those files in place, rebuild, expect failure, and restore them on every exit.

iPlug2 changes are Linux-only except `GetScaleForHWND`: inert unless `VOLUM_SCREEN_SCALE` is set. That hook is how the Windows shots at scale 1 and scale 2 were taken on a laptop that is not at 200% DPI.

## Scenes

Both sides are the GL framebuffer (`VOLUM_SELF_CAPTURE_DIR`), 900x600 at scale 1 and 1800x1200 at scale 2. Linux scale 2 is `GDK_SCALE=2`. The seed library is `docs/screenshot-seed`. Audio on Linux is a private `jackd -d dummy`.

| Scene | Scale 1 SSIM | Scale 2 SSIM |
| --- | ---: | ---: |
| main (THC Sunset) | 0.999854 | 0.999879 |
| Settings SIGNAL | 0.999985 | 0.999986 |
| Settings SYSTEM | 0.994605 | 0.995635 |
| custom amp builder | 0.999978 | 0.999981 |
| pack export | 0.998539 | 0.998731 |
| tuner | 0.999984 | 0.999984 |

SSIM here is `1 - d`, where `d` is the normalized term ImageMagick 7 prints in parentheses for `compare -metric SSIM` (`0` is identical). Two Linux captures of main, one second apart, score **1.000000**. Shifting the sidebar rect 18 px in the source and rebuilding scores **0.968077**.

**Threshold: 0.99.** The worst real scene is SYSTEM at 0.994605. The shifted control is 0.968077. 0.99 sits between them. The SYSTEM gap is the model-information line: Windows shows plugin latency 1.8 ms and buffer 512, Linux shows round trip 16.0 ms and buffer 256, because the Linux capture is on jackd's 256-frame dummy cycle. The rest of that card matches. Shortcut text also differs by font rasterization; that is inside the 0.005 of the SYSTEM score.

Images (Windows on the left of each pair):

- Contact sheet: `.scratch/linux-support/reports/13-ui/pairs/contact.png`
- Full pairs: `.scratch/linux-support/reports/13-ui/pairs/*-pair.png`
- Linux frames: `.scratch/linux-support/reports/13-ui/linux/`
- Windows frames: `.scratch/linux-support/reports/13-ui/windows/`

## Carried items

**Unhandled key reaches the host: PASS.** An `q` sent to the editor is logged by the editor as forwarded (keysym 113) and by the standalone window as `down 81` (`VOLUM_HOST_KEY_LOG`). The send uses event mask 0 so it reaches the client that created the parent. GDK selects XInput2, and a KeyPressMask send was delivered to nobody (that failure was observed before the fix: forward log set, host log empty).

**Popup menu in REAPER: PASS, on the decoded project from this run.** Default in the preset menu resets the open editor from Sunset Crunch / channel 3 to No Preset / channel 1 / `V30-THC-1.nam`. The plug-in state lives in the REAPER project chunk, not `volum-settings.json` (that file is the standalone's). Decoding the base64 VST chunk:

- positive project: contains `V30-THC-1`
- Default handler removed in the source, then rebuilt: the chunk does not contain `V30-THC-1`

**Text entry in REAPER: PASS.** The builder's name field opens the name dialog. Letters are delivered with `XSendEvent` to the deepest editor-sized window (the wrapper of the same size does not receive them: propagation goes to ancestors). `LinuxProofAmp` is in `volum-content.json`. The same sequence with `StartTextEntry` returning immediately does not write that name. The source is restored afterwards.

## Not the acceptance CI

An intermediate `ci.yml` run on `4264732c` is [40149262641](https://github.com/guitarlum/VoLum/actions/runs/40149262641). That is not this head. The acceptance run waits until the review's defects are fixed.
