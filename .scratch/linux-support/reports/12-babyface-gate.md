# 12 Real-device gate

The owner route is Windows playback `Analog (3+4) (RME Babyface Pro)`, looped at 0 dB into ADAT 3/4, recorded as `ADAT (3+4) (RME Babyface Pro)`. WSLg still names them `RDPSink` / `RDPSource` (1 ch, 44100 Hz). Nothing on the Windows side was changed. The played tone is a −24 dBFS 440 Hz sine. The mono source folds it, so the fitted gain is 2.

Code head is unchanged: `6a3cd4dd7eebb50cff6b16cb019a8d0fabb95ea4`. iPlug2 `volum/linux-12` is `2a3ba520c3cf08efb6ec14e642a18add9c61b928`. CI on that code head is [40208730717](https://github.com/guitarlum/VoLum/actions/runs/40208730717), green, including Formatting and macOS. No code change in this round, so no new CI.

`NULL_PEAK_DB=-60`, from the 20 s calibration residual peak of −77.82 dBFS (about 18 dB of margin). A dropped buffer still lands near −24 dBFS. Not refit per run, and not loosened.

## The failed pass `bf2-green-1` (no new audio)

The recording is still at `~/volum-linux/out/12/bf2-green-1/rec.wav` (303.6 s, 44100 Hz, 2 ch). The earlier “0–30 s is bad” was one fit over a window that included a short opening. Sample by sample against a 440 Hz sine locked on 40–50 s:

- 0–0.45 s is silence. That is the loopback delay. Local level −90 to −144 dBFS.
- The tone is present at full level (−18.1 to −18.3 dBFS) from 0.45 s.
- Its phase, relative to the steady-state sine, slips smoothly: −35° at 0.4 s, then about −30° to −40° per 100 ms, and is 0° at 1.4 s. That is about 200° in one second, roughly 0.9 Hz of drift, not a step.
- From 1.4 s to 300 s the phase error is 0° and the residual peak is −78 dBFS.
- Median rising-zero interval in 0.55–1.25 s is 100 samples (441 Hz). A few intervals are 2 samples, which is why a raw zero-crossing count reads ~480 Hz. A stationary spectrum of that same span is 440 Hz, and the next bin (400 Hz) is about 30 dB down. There is no second tone.
- 44 samples between 0.45 s and 1.36 s step harder than the sine itself. They are spread across the second, not one hole. A dropped 256-frame buffer would be one ~6 ms gap and a phase jump of more than two cycles. No `gate-xrun` was logged.

**Verdict:** not foreign audio. The 440 Hz sine is intact in level and in its median period, and nothing tonal is added on top. The phase is not continuous with the steady delay until 1.4 s. It is a smooth coast of our own tone, and the part after the aligner's 0.5 s skip is what fails the −60 dBFS peak (live result: gain 1.9973, lag 19669 samples / 446 ms, residual peak −12.03 dBFS, rms −46.76 dBFS, 11607 dropout samples, 0 xruns).

The calibration and `bf-green-1` do not do this. On `bf-green-1` the phase error is already 0° by 0.6 s, which is inside the 0.5 s the aligner leaves out, so that file nulls at −77 dBFS. The coast is intermittent in length. It is not a reason to skip the opening or to move −60.

## This round

Before each 300 s pass: 4 s of digital silence, then 6 s of the sine. Silence was judged quiet unless a stretch longer than ~45 ms exceeded −50 dBFS or the file rms was above −60 dBFS. A 2 ms click does not fail that. The short sine is recorded even when its own null fails; the opening coast can dominate a 6 s file and still leave a clean 300 s file.

| Attempt | What | Peak | Gain | Lag | Residual peak | Residual rms | Dropouts | Xruns | Result |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Probe mute 1 | 4 s silence | −64.29 | 0 | — | — | −93.55 (file) | 0 | 0 | quiet |
| Probe tone 1 | 6 s sine | −16.42 | 1.8951 | 39555 (897 ms) | −12.46 | −30.90 | 12955 | 0 | opening coast; 300 s still run |
| Green 1 | 300 s | −17.97 | 2.0000 | 31419 (712 ms) | −77.38 | −90.12 | 0 | 0 | pass |
| Probe mute 2 | 4 s silence | −37.40 | 0 | — | — | −77.96 (file) | — | 0 | one 1.9 ms broadband click at 0.827 s, 83 samples, peak −37.4; the rest of the file is below −60. Not a sustained signal. Pass 2 was not started on this probe. |
| Probe tone 2 | 6 s sine, same output directory as a previous log, so ignore the first mute retry | −16.81 | 1.3241 | 37331 | −12.90 | −23.50 | 124382 | 0 | phase still moving until 3.2 s, then locked at −18 dBFS. Not a 300 s attempt. |
| Probe mute 2b | 4 s silence | −37.84 | 0 | — | — | −78.11 (file) | — | 0 | same kind of click, 81 samples above −50 dBFS. Quiet otherwise. |
| Probe tone 2b | 6 s sine | −16.44 | 1.7130 | 39908 (905 ms) | −11.81 | −26.74 | 29397 | 0 | opening coast |
| Green 2 | 300 s | −17.90 | 2.0000 | 39152 (888 ms) | −77.38 | −90.12 | 0 | 0 | pass |
| Probe mute 3 | 4 s silence | −52.25 | 0 | — | — | −91.39 (file) | 0 | 0 | quiet |
| Probe tone 3 | 6 s sine | −17.97 | 2.0000 | 40396 (916 ms) | −77.57 | −90.06 | 0 | 0 | pass |
| Green 3 | 300 s | −17.95 | 1.9998 | 38562 (875 ms) | −14.47 | −57.77 | 989 | 0 | fail |

Green 3's bad samples are the same coast, not a new event. Against a sine locked on 60–70 s, the residual exceeds half the tone from 0 to 1.403 s (61870 samples) and again in the last 0.2 s after the tone has stopped. The aligner starts 0.5 s after the lag (0.875 + 0.5 = 1.37 s), so only the last ~30 ms of the coast is inside the null: 989 samples, peak −14.47 dBFS. The rms of −57.77 dBFS is what those samples do to an otherwise −90 dBFS residual. Phase at 0.85 s is −134° while the level is already −18.6 dBFS. No xrun. This is the path coast again, so there is no replacement pass.

## Red proofs on the real route

Both must fail, and both did.

| | Peak | Gain | Lag | Residual peak | Residual rms | Dropouts | Xruns | Exit |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `--mute` 6 s | −62.01 | 0 | — | — | — | 0 | 0 | 1, recording silent |
| `--drop` 8 s | −17.97 | 1.9988 | 38264 (868 ms) | −17.98 | −53.22 | 159 | 0 | 1, residual above −60 |

The dropped buffer is 159 samples in the recording (the self-test hole is 167). The rest of that take is the sine at gain 2.0.

## Root cause of the coast

The callback writes an exact 440 Hz phase at the opened 48000 Hz rate from `Init`'s plan. When a take fails, the recording still has that sine at full level; what moves is the phase against the delay measured later in the same file. It takes up to about 1.4 s from the start of the file (about half a second after the tone arrives) to settle, and some opens settle before the aligner's 0.5 s skip (green 1, green 2, the calibration, `bf-green-1`). There is no logged gap over 50 ms and no single missing buffer. The coast is in the WSLg resample / loopback delay, after the callback. It is not another application's audio: a notification would not hold −18 dBFS of 440 Hz with a 100-sample period and no competing bin.

The skip stays 0.5 s and the tolerance stays −60 dBFS. Lengthening either one would hide this.
