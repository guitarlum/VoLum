# 12 Real-device gate (resume)

The owner route is in place: Windows playback `Analog (3+4) (RME Babyface Pro)` looped at 0 dB into ADAT 3/4, recording `ADAT (3+4) (RME Babyface Pro)`. WSLg still calls them `RDPSink` / `RDPSource` (source 1 ch, 44100 Hz). Nothing on the Windows side was changed.

The three consecutive 300 s passes are **not** done. One 300 s recording is a clean null. The next 300 s recording is clean after the first 30 s and not before. Audio was stopped there, as instructed, and not started again.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Code | `feature/linux-12-babyface` | `6a3cd4dd7eebb50cff6b16cb019a8d0fabb95ea4` |
| Report | same branch, the commit that adds this file | later than the code head |
| iPlug2 | `volum/linux-12` | `2a3ba520c3cf08efb6ec14e642a18add9c61b928` |

`2a3ba520c` is `volum/linux` `98519e71f` merged with the gate-tone commits. The superproject merge is `origin/feature/linux-support` at `69375bb5`, which contains `2c6a96d5`.

CI on the code head: [40208730717](https://github.com/guitarlum/VoLum/actions/runs/40208730717), green, including Formatting and macOS. Earlier dispatches on older heads are not this head: `40205249560` (`78a43f28`) and `40206199911` (`718f8140`, failed the executable-bit guard before the suite).

## Gate tone

`VOLUM_LINUX_GATE_TONE` is read in `IPlugAPPHost::Init`, before `TryToChangeAudio` starts the stream. The callback only plays that plan. It has no `getenv` and no function-local statics. Phase and the dropout counter live on the host and are written only by the callback. `tests/test_iplug_app_linux_policy.cpp` checks the parser and that the callback source does not call `getenv`. That test is in the green CI run.

## Calibration (Babyface, 20 s)

`~/volum-linux/out/12/bf-cal`, app from `92c5d678` (same tone code as the code head). Re-measured with the final aligner:

| | |
| --- | --- |
| peak | -17.48 dBFS |
| fitted gain | 2.0000 |
| lag | the later re-measure of this file used the same fit; the live run's lag was 20330 samples (461 ms) |
| overlap | 21.17 s |
| residual peak | **-77.82 dBFS** |
| residual rms | -90.12 dBFS |
| dropout samples | 0 |
| xruns, open through quit | 0 |

The gain of 2 is the mono source folding the stereo sine. Peak -17.48 matches the coordinator's -17.7. The played tone is -24 dBFS.

**`NULL_PEAK_DB=-60`.** That is about 18 dB looser than the -77.82 peak. A dropped buffer still lands near -24 dBFS, so it fails. The number is not refit per run.

## Five-minute recordings

**Saved pass, re-measured after the aligner fix** (`bf-green-1`, no second playback). The first live pass had failed only because the aligner locked the sine a fraction of a period off the onset. The file itself is a 440 Hz tone for all 300 s.

| | |
| --- | --- |
| peak | -17.45 dBFS |
| gain | 2.0000 |
| lag | 19960 samples (453 ms) |
| overlap | 301.97 s |
| residual peak | -77.00 dBFS |
| residual rms | -90.13 dBFS |
| dropout samples | 0 |
| xruns, open through quit | 0 |

**Next live pass** (`bf2-green-1`), fixed aligner, then stopped:

| | |
| --- | --- |
| peak | -16.72 dBFS |
| gain | 1.9973 |
| lag | 19669 samples (446 ms) |
| overlap | 301.97 s |
| residual peak | **-12.03 dBFS** |
| residual rms | -46.76 dBFS |
| dropout samples | 11607 |
| xruns, open through quit | 0 |

Per 30 s chunk, that file is two different things. 0–30 s: gain 1.902, residual peak -11.66 dBFS, 41034 samples over half the fitted tone. Every chunk from 30 s to 300 s: gain 2.000, residual peak about -77 dBFS, rms about -90 dBFS, 0 bad samples. One-second peak and rms stay near -18 / -21 for the whole file, so it did not go quiet and it did not pick up a second signal. The opening half-minute is not a clean sine. No callback gap over 50 ms was logged, so it is not a stall the gate's xrun counter sees.

Runs 2 and 3 were not started. `--mute` and `--drop` were not repeated on this route after that.

## What changed in the aligner

A pure sine shifted in time is a phase shift. The fit is now sine plus cosine, and the first and last 0.5 s of the tone are left out so the resampler opening is not the null peak. The self-test still passes: a clean synthetic nulls, silence is rejected, one zeroed buffer (167 samples, residual peak -24.97 dBFS) is rejected.

## Xruns

Neither Babyface recording logged a `gate-xrun`. On this route the open gap seen earlier on the private null sink did not appear. Nothing outside device open/close showed up to root-cause.

## Uncertain

- Whether the noisy first 30 s of `bf2-green-1` is the stream settling (a warmup the gate should skip) or a one-off on the loopback. A 20 s calibration and one full file were clean through that same opening. I did not play again to find out.
- The real-route `--mute` and `--drop` runs are still outstanding. The synthetic self-test covers those failures; the headphones have not heard them on this route in this resume.
