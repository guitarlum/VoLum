# 13 UI acceptance on Linux

Status: **review pending**. Scenes are captured. The cold visual review has not been run (the implementer is Grok; an Opus reviewer has to do it). Linux reference PNGs are not committed yet: they wait until that review accepts the pictures.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Superproject scenes | `feature/linux-13-ui` | `c18583a2bd86fe9c88455fbf2d4a1c36dd37288e` |
| iPlug2 | `volum/linux-13` | `5e102632fd298ef57d7321f5a1d0bd624495c422` |

The commit that adds this report also teaches `ui-scenes.sh` to read the preset result out of the REAPER project's base64 VST chunk.

iPlug2 changes are Linux-only except `GetScaleForHWND`: if `VOLUM_SCREEN_SCALE` is set it returns that scale, otherwise the monitor DPI path is unchanged. That hook is how the Windows shots at scale 1 and scale 2 were taken on a laptop that is not at 200% DPI. Nothing else in the Windows or macOS build changes.

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

SSIM here is `1 - d`, where `d` is the normalized term ImageMagick 7 prints in parentheses for `compare -metric SSIM` (`0` is identical). Two Linux captures of main, one second apart, score **1.000000**. An 18 px sidebar shift (`VOLUM_UI_SHIFT_PROOF=1`) scores **0.953762**.

**Threshold: 0.99.** The worst real scene is SYSTEM at 0.994605. The shifted control is 0.953762. 0.99 sits between them. The SYSTEM gap is the model-information line: Windows shows plugin latency 1.8 ms and buffer 512, Linux shows round trip 16.0 ms and buffer 256, because the Linux capture is on jackd's 256-frame dummy cycle. The rest of that card matches. Shortcut text also differs by font rasterization; that is inside the 0.005 of the SYSTEM score.

Images (Windows on the left of each pair):

- Contact sheet: `.scratch/linux-support/reports/13-ui/pairs/contact.png`
- Full pairs: `.scratch/linux-support/reports/13-ui/pairs/*-pair.png`
- Linux frames: `.scratch/linux-support/reports/13-ui/linux/`
- Windows frames: `.scratch/linux-support/reports/13-ui/windows/`

## Carried items

**Unhandled key reaches the host: PASS.** An `q` sent to the editor is logged by the editor as forwarded (keysym 113) and by the standalone window as `down 81` (`VOLUM_HOST_KEY_LOG`). The send uses event mask 0 so it reaches the client that created the parent. GDK selects XInput2, and a KeyPressMask send was delivered to nobody (that failure was observed before the fix: forward log set, host log empty).

**Popup menu in REAPER: PASS, on the decoded project from this run.** Default in the preset menu resets the open editor from Sunset Crunch / channel 3 to No Preset / channel 1 / `V30-THC-1.nam`. The plug-in state lives in the REAPER project chunk, not `volum-settings.json` (that file is the standalone's). Decoding the base64 VST chunk:

- positive project: contains `V30-THC-1`
- `VOLUM_BREAK_POPUP=1`: contains `V30-THC-3` and not `V30-THC-1`

The script now decodes that chunk. The run that produced the files grepped the raw `.rpp` and reported fail; the bytes above are from that same save.

**Text entry in REAPER: not accepted.** The shot after the name sequence is still the PLAY screen, and `LinuxProofAmp` is not in the content store. `VOLUM_BREAK_TEXT_ENTRY=1` is in the script; it is not evidence until the positive check passes.

## Not the acceptance CI

An intermediate `ci.yml` run on `4264732c` is [40149262641](https://github.com/guitarlum/VoLum/actions/runs/40149262641). That is not this head. The acceptance run waits until the review's defects are fixed.
