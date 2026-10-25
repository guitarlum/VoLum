# 13 UI acceptance on Linux

Status: **accepted**. The cold visual review passed. The scene gate is in CI and green on the code head below. This report commit does not change that head.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Code head | `feature/linux-13-ui` | `0c0daf87ba30072afa66a71e0a6a292dad1c4141` |
| iPlug2 | `volum/linux-13` | `98519e71f28f813a316f8d7d03d7da5b3d56a2b8` |
| CI | [40176527088](https://github.com/guitarlum/VoLum/actions/runs/40176527088) | same code head, success, every job including macOS and Formatting |

`feature/linux-support` at `03eccfbe` (tickets 11 and 08) is merged. iPlug2 `volum/linux-13` includes `volum/linux` at `3d894cd8e`. Windows `run-tests-win.ps1` on the merge: 1479 passed, 0 failed. The commit after that only indents three preprocessor lines in `VoLumSelfCapture.h`.

The Linux reference set is `NeuralAmpModeler/tests/linux-ui-refs/` (twelve PNGs and `manifest.txt`). Threshold is **0.99**. `verify-linux.sh` calls `ui-scenes.sh`, and the Linux standalone CI job runs `--gate`.

## SSIM, Windows at 48 kHz against the Linux references

Masked scores. ImageMagick 7, `1 - d` from the normalized term in parentheses.

| Scene | Scale 1 | Scale 2 |
| --- | ---: | ---: |
| main (THC Sunset) | 0.999909 | 0.999953 |
| Settings SIGNAL | 0.999994 | 0.999999 |
| Settings SYSTEM | 0.999991 | 1.000000 |
| custom amp builder | 0.999984 | 0.999986 |
| pack export | 0.999998 | 1.000000 |
| tuner | 0.999994 | 0.999999 |

Windows was recaptured from the sandbox only. `settings.ini` in that sandbox asks for `sr=48000` on the same DirectSound default devices the machine already uses. The log says `stream open: 48000 Hz, buffer 64, requested 48000 Hz`. The Windows default device and the Windows sound settings were not changed. At 48 kHz the 48 kHz model adds no resampler latency, so the line is `Plugin latency: 0.0 ms (0 samples)`.

## Masks

Logical 900×600 rectangles. Scale 2 multiplies them with the framebuffer. They are painted the same colour on both shots before SSIM. A control that moves leaves the rectangle, so the sidebar shift still fails.

| Mask | Scenes | Rectangle | Why |
| --- | --- | --- | --- |
| Sebago sidebar art | every scene | x=16, y=469, 22×22 | The icon is the Clifford attractor in `DrawSidebarMiniFractal` (2200 sin/cos steps). libm is not bit-equal across platforms, so those dots cannot match. The rectangle is the 22 px icon, not the row. |
| Latency line | system, pack | x=68, y=338, 360×36 | Both sides are at 48 kHz. DirectSound reports no driver latency, so Windows says `Plugin latency: 0.0 ms (0 samples)` and `buffer 64; driver reports none`. JACK reports a round trip and buffer 256. The two text rows are masked. The model-rate row above them is not. |

## OUT meter

The scene shots are idle. Linux is a private jackd dummy, so the meter is dark. The Windows default input still leaves a one-pixel gold floor on the OUT meter; that is not masked, and main stays at 0.999909.

The gate then feeds a 440 Hz tone into the VoLum JACK input and reads the BUILD OUT well at logical x=789, y=430, 10×40.

- Green: well mean **0.9**. The meter lights.
- Red: `NAMMeterControl::OnMsgFromDelegate` is edited to return immediately, the app is rebuilt, and the same tone scores **0.05**. The source is restored and the app rebuilt again. No product switch.

## Other red proofs

Each one edits the source, rebuilds, expects failure, and restores on every exit. The tree was clean afterwards.

- Sidebar rect moved 18 px (`b.L + 6` to `b.L + 24`): SSIM **0.976326**, under 0.99, with the Sebago mask applied.
- Preset Default handler removed: the REAPER project chunk does not contain `V30-THC-1`.
- `StartTextEntry` returns immediately: `LinuxProofAmp` is not written to the content store.

## Red proofs, rerun

`--red` on `cd0335d8` (the commit before the preprocessor indent) scored the committed `tests/linux-ui-refs/main-s1.png` and then checked `git diff --quiet` in the repo and in `iPlug2`. Six checks, none failed. The source was clean afterwards.

| Check | Result |
| --- | --- |
| Sidebar shifted 18 px | SSIM **0.976326**, under 0.99 |
| OUT meter update removed | well mean **0.05**, stays dark |
| Preset Default removed | chunk has no `V30-THC-1` |
| `StartTextEntry` returns immediately | `LinuxProofAmp` is not saved |
| `XSendEvent` forward removed | `VOLUM_HOST_KEY_LOG` has no `down` line |

## Not changed

The Roboto fallback in SYSTEM, the tuner frame offset, hyphens versus dashes, and the "x" remove glyph are Windows UI. Windows and macOS golden hashes and the existing screenshot references were left alone.
