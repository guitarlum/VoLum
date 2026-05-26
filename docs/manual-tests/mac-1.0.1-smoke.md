# macOS 1.0.1 Release Smoke Test

End-to-end manual smoke pass before tagging the 1.0.1 release. Owner: tester
on a Mac. Estimated runtime: 30-45 minutes.

This list focuses on changes that landed between `v1.0.0` and the 1.0.1
branch, with extra attention to mac-only fixes (mic permission, VST3 signing,
installer relocation, trackpad knob direction, perf review) and the new
PRE/POST lock feature including its B-Lock-1 bug fix.

## Pre-flight

- [ ] You have a clean Mac (or a fresh user account) to ensure permissions /
      preferences are not carried over from older installs.
- [ ] Optional: copy `~/Library/Application Support/VoLum/volum-settings.json`
      somewhere safe if you want to A/B downgrade test later.
- [ ] Note your macOS version and which audio interface you'll use.

## Install + first launch

1. [ ] Install via the `.pkg` produced by `scripts/makedist-mac.sh full all`
       (or grab the `release-native.yml` artifact). Confirm the installer
       runs without an "unidentified developer" gatekeeper prompt.
2. [ ] First launch the standalone. macOS prompts for microphone access ->
       grant.
3. [ ] Quit and relaunch. Confirm the mic permission is still granted (no
       second prompt) and audio works on launch.
4. [ ] Open *System Settings -> Privacy & Security -> Microphone* and revoke
       VoLum. Relaunch VoLum -> it must not crash, prompt again, and recover
       once granted.

## Audio configuration

5. [ ] Open Preferences. Sweep sample rate (44.1k, 48k, 96k) and buffer size
       (64, 128, 256, 512). Each apply must keep audio alive without a
       crash.
6. [ ] Switch input/output devices to a connected external interface; back to
       built-in. Confirm clean transitions.
7. [ ] Plug/unplug a USB interface while VoLum is running. Confirm VoLum
       neither crashes nor drops into a permanently-silent state.

## UI sanity (perf review fallout)

8. [ ] Click between PRE, AMP, POST expansion. Hero fractal art renders
       cleanly with no flicker or stale-cache artifacts (commit `255f107`
       cached hero fractal layers).
9. [ ] Resize the window (if supported) or reopen the UI in a host. No
       cached art bleeds at the wrong scale.
10. [ ] Trackpad: scroll up on a knob -> value goes up (matching system
        natural-scrolling direction). Test on AMP gain, PRE NAM gain,
        DELAY mix knobs. Reverse-scroll if you change the system setting.
11. [ ] Keyboard: arrow keys give 0.5 dB steps on amp output / dual-amp pan /
        PRE NAM levels; fine (Shift) gives 0.1 dB. Same as mouse wheel.

## PRE/POST lock feature (the headline)

12. [ ] Open AMP A, tweak PRE (e.g. enable PRE NAM 1, change Gain). Click
        the PRE lock icon -> it goes gold.
13. [ ] Switch to AMP B. PRE knob values should NOT change; the dirty arrow
        appears next to the PRE lock (because B's stored PRE differs from
        the locked live PRE).
14. [ ] Click *Store to AMP* on PRE -> arrow clears, PRE values are now
        AMP B's stored PRE.
15. [ ] Click PRE lock again to unlock -> live PRE restores to whatever AMP
        B's stored PRE is.
16. [ ] Repeat steps 12-15 for POST (DELAY/REVERB).

### B-Lock-1 regression - the bug this release fixes

17. [ ] Open AMP A, tweak PRE settings the way you can recognize them by ear
        (e.g. unique PRE NAM gain).
18. [ ] Click PRE lock. Switch to AMP B (now in dirty-locked state - arrow
        visible next to PRE lock).
19. [ ] Quit the app cleanly (Cmd+Q).
20. [ ] Relaunch. Expectations:
        - [ ] AMP B is still selected.
        - [ ] PRE lock is still on (gold).
        - [ ] PRE knob values are EXACTLY what they were before quitting
              (not zeros, not AMP B's stored PRE).
        - [ ] Click the AMP A tile - its stored PRE values are unchanged
              (verify by unlocking once on AMP A: knobs should restore
              AMP A's original stored PRE).
21. [ ] Repeat steps 17-20 for POST lock.

### DAW preset round-trip (chunk path, same bug)

22. [ ] Open a DAW (Logic / Reaper / Live). Insert VoLum on a track.
23. [ ] Tweak PRE, click PRE lock, switch amp to a different one (locked
        live PRE carries across).
24. [ ] Save the project. Close the project. Reopen the project.
25. [ ] Expectation: PRE lock still on, exact live PRE values restored, no
        amp slot's stored PRE was modified.
26. [ ] Repeat for POST.

## Settings backwards-compat (B-Compat-1)

27. [ ] After running 1.0.1 enough to have written
        `~/Library/Application Support/VoLum/volum-settings.json`, quit.
28. [ ] Open the JSON in a text editor. Confirm `"version": 6` (NOT 7) and
        that `preLocked` / `postLocked` / `liveLockedPre` / `liveLockedPost`
        keys are present when applicable.
29. [ ] Install the v1.0.0 build over 1.0.1 (or use a sibling install).
        Launch 1.0.0. Expectation: 1.0.0 reads the file without resetting
        your delay/reverb layout (the v6 reader ignores the new lock-related
        keys and keeps your tweaks).
30. [ ] Reinstall 1.0.1 over 1.0.0. Launch. Expectation: state intact, lock
        keys still where they were.

## Plugin formats

31. [ ] VST3 in Reaper: loads, processes audio, GUI renders, preset save +
        reload works. Resize-friendly.
32. [ ] AU in Logic Pro: loads with no validation errors, processes audio.
33. [ ] AUv3 in GarageBand or Logic (mobile): loads, processes audio.
34. [ ] All three pass `auval -v aufx ...` for AU (Logic auval) and
        Steinberg `validator` for VST3 if you have it handy.

## Golden parity

35. [ ] Open a preset saved with 1.0.0 (if you kept any). Compare by ear
        and meter: the sound should be indistinguishable from 1.0.0
        (golden DSP tests guarantee bit-equivalence on synthetic inputs).
36. [ ] Spot-check 2-3 amps with PRE/POST both engaged. No clicks, no NaN
        muting, no surprise level shift.

## Failure handling

If any check fails:

- Note the exact step number, build version (`VoLum -> About`), macOS
  version, and a 1-line repro.
- File against the 1.0.1 release branch or post in the release tracker.
