# Does a short upstream NAM-player sweep land before 1.3.0 feature work?

Type: grilling
Status: resolved

## Question

`backlog/R3-upstream-nam-player-sync-review.md` is a recurring cherry-pick review of NeuralAmpModelerPlugin + NAMCore, not a feature. Last recorded sync: 2026-06-22. The `upstream-sync` skill is the procedure. This map’s headlines (MIDI, PLAY, Pack, Chorus, update notifier) all land in VoLum-only files; a sweep mainly reduces future cherry-pick pain and might catch free DSP/core fixes.

Decide:

1. **Before feature work** — a timeboxed review (and any adopt-bucket cherry-picks) on `dev` before MIDI / PLAY / Pack / Chorus coding starts.
2. **After this minor** — park R3 until 1.3.0 ships; conductor does not wait on it.
3. **Parallel, non-blocking** — a conductor may spawn a sweep sub-agent, but headline specs do not wait.

Recommend: **after this minor** (option 2). R3 is never “done”; blocking a large book on a cherry-pick review is the wrong gate. If the owner later wants a look, graduate a research ticket — do not do that research unless this ticket says the sweep is in.

Do not cherry-pick on this ticket.

## Answer

**Yes, a short adopt lands before headline coding — not a full R3.** NAM Player (sdatkinson/NeuralAmpModelerPlugin) since the 2026-06-22 baseline (`96337e9`) is five commits. This session does not cherry-pick.

**Adopt (conductor, first, on `dev`).** Cherry-pick AudioDSPTools [Fix loading WAV files from Unicode paths](https://github.com/sdatkinson/AudioDSPTools/commit/e19ef4b5b3bf2171c431847563acde29eedf85c0) (`#25`, `v0.1.2`) onto `guitarlum/AudioDSPTools` `effect-staging`. Bump the VoLum submodule pointer. In `_StageIR`, pass the UTF-8 path into `ImpulseResponse` (drop `u8path().string()`, which is the old Windows bug). Gate: AudioDSPTools WAV tests plus VoLum IR load. Then MIDI / PLAY / Pack / Chorus.

**Skip (NAM Player plugin).** Version 0.8.0 branding. File-browser `(FAILED)` UI and release-mouse-capture-before-OpenURL — they patch `NAMFileBrowserControl` / `NAMGetButtonControl`, which VoLum never attaches. Their iPlug2 SHA (`de5a4fb`, standalone device-pair validation) is not our ASIO fork; do not take their pointer.

**Park until after 1.3.0.** NAMCore `origin/main` is 35 commits ahead (LSTM RT safety, A2 FiLM/gating, convolution/A2-fast, FFT linear conv, Eigen 5.0.1, Sequential). Keep our two unique commits (`f4a6cc0` A2 load-prewarm cut, `27027cc` head_scale detector test) when that bump happens. Recurring R3 stays in `backlog/R3-upstream-nam-player-sync-review.md`; this book does not wait on another sweep.

**Not this ticket.** The 08/27 POLY/DROP WSOLA cheapening is already on `dev` and already in 1.3.0; no extra sound-impact review.
